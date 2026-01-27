#include "descend/compose.hpp"
#include "descend/debug.hpp"
#include "descend/descend.hpp"
#include "descend/stages.hpp"
#include "descend/error_or.hpp"

#include <charconv>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dd = descend;


template <class T>
std::ostream& operator << (std::ostream& s, const std::optional<T>& o)
{
    return o ? (s << '<' << *o << '>') : (s << "<empty>");
}
template <class T>
std::ostream& operator << (std::ostream& s, const std::vector<T>& v)
{
    s << '[';
    const char* sep = "";
    for (const auto& x : v) {
        s << std::exchange(sep, ", ") << x;
    }
    return s << ']';
}

template <class T>
std::ostream& operator << (std::ostream& strm, const dd::error_or<T>& x)
{
    return (x.has_value())
        ? (strm << "value=" << x.value())
        : (strm << "error=" << x.error());
}



auto pythagorean_triples()
{
    auto zip_flat = [] (auto f) {
        return dd::compose(dd::zip_result(std::move(f)), dd::flatten());
    };

    return dd::compose(
            dd::iota(1),
            zip_flat([](int c) { return dd::iota(1, c + 1); }),
            zip_flat([](int c, int a) { return dd::iota(a, c + 1); }),
            dd::swizzle<1, 2, 0>(),
            dd::filter([](int a, int b, int c) { return a*a + b*b == c*c; })
    );
}

void pythagorean_triples_example()
{
    dd::apply(
            pythagorean_triples(),
            dd::take_n(10),
            dd::for_each([] (int a, int b, int c) {
                std::cout << a << ' ' << b << ' ' << c << '\n';
            })
    );
}


// non-copyable non-movable string to check no copying\moving is performed inside chain
struct String : private std::string
{
    String(const long i) : std::string(std::to_string(i))
    {}

    String(const String& other) = delete;
    String(String&& other) = delete;

    const std::string& as_string() const
    { return static_cast<const std::string&>(*this); }

    friend std::ostream& operator << (std::ostream& strm, const String& s)
    { return strm << s.as_string(); }
};


// Example from talk, with std::string replaced with non-copyable non-movable class
// (and int replaced with long, since int overflows)
void cube_vs_hash_example()
{
    using LongAndString = std::pair<long, String>;
    auto make_long_and_string = [] (const long i) -> LongAndString {
        return {i*i*i, i};
    };
    dd::apply(
            dd::iota(1),
            dd::transform(make_long_and_string),
            dd::filter([] (const auto& p) {
                return std::size_t(p.first) >= std::hash<std::string>{}(p.second.as_string());
            }),
            dd::transform(&LongAndString::second),
            dd::take_n(4),
            dd::for_each([] (auto&& s) { std::cout << s << '\n'; })
    );
}


std::optional<int> parse_int(const std::string_view str) noexcept
{
    int result = 0;
    const char* begin = str.data(),
              * end   = str.size() + begin;
    const auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ptr == end && ec == std::errc{}) {
        return std::optional<int>{result};
    }
    else {
        return std::optional<int>{};
    }
}
int squared(const int x) noexcept
{
    return x*x;
}


void monadic_unwrap_optional_example() // would short-circuit and return nullopt for the whole chain
{
    {
        auto stages = dd::compose(
                dd::unwrap_optional(),
                dd::transform(parse_int),
                dd::unwrap_optional(),
                dd::accumulate()
        );

        std::vector<std::optional<std::string>> values1 = {"1", "2", "3"};
        std::vector<std::optional<std::string>> values2 = {"1", "2", "abc"};
        std::vector<std::optional<std::string>> values3 = {"1", std::nullopt, "3"};

        std::cout << dd::apply_debug(values1, stages) << '\n';
        std::cout << dd::apply(values2, stages) << '\n';
        std::cout << dd::apply(values3, stages) << '\n';
    }
    {
        /*
        The video shows the sequence
            unwrap_optional_complete()
            transform_complete(parse_int)
            transform_complete(squared)

        But transform_complete(parse_int) outputs optional<int>, it can't be directly passed into next stage.
        Unless some hacks are implemented (or my understanding is wrong) that would not work and another
        unwrap_optional_complete() is needed to pass result of parse_int() into squared()

        Since unwrap_optional_complete() wraps result into an optional, then unwrap + squared would return std::optional.
        That meants that the result of next computaion at first unwrap_optional_complete() would be std::optional.
        If unwrap_optional_complete() just wraps result of the rest of computation into std::optional, the final
        result should be std::optional<std::optional<int>>

        Since it is not the case in the video, I assume unwrap_optional_complete() checks the type of the rest of the
        computation, and if it std::optional already - it won't wrap it again.

        I decided to implement it this way. Same for incremental unwrap_optional()
        */

        std::optional<std::string> value = "7";
        auto result = dd::apply_debug(
                value,
                dd::unwrap_optional_complete(),
                dd::transform_complete(parse_int),
                dd::unwrap_optional_complete(),
                dd::transform_complete(squared)
        );
        std::cout << result << '\n';
    }
}


dd::error_or<int> parse_int_or_error(const std::string_view str) noexcept
{
    auto x = parse_int(str);
    if (x) {
        return dd::error_or<int>{dd::in_place_value, *x};
    }
    else {
        return dd::error_or<int>{dd::in_place_error, std::make_error_code(std::errc::permission_denied)};
    }
}

void monadic_unwrap_error_or_example() // would short-circuit and error for the whole chain
{
    using namespace std::string_literals;
    using string_vector = std::vector<std::string>;

    auto stages = dd::compose(  // input as error_or<vector<string>>
            dd::unwrap_error_or_complete(), // vector<string>
            dd::transform(&parse_int_or_error), // error_or<int>
            dd::unwrap_error_or(), // int
            dd::transform(&squared), // int
            dd::to<std::vector>() // vector<int>
    );

    dd::error_or<string_vector> values1 = dd::make_error_or(std::vector{"5"s, "6"s, "7"s}); // all good
    dd::error_or<string_vector> values2 = dd::make_error_or(std::vector{"5"s, "ABC"s, "7"s}); // one of parse would return error
    dd::error_or<string_vector> values3 = dd::make_error_or<string_vector>(std::make_error_code(std::errc::already_connected)); // top-level error

    std::cout << "\n\n";
    std::cout << dd::apply_debug(values1, stages) << '\n';
    std::cout << dd::apply(values2, stages) << '\n';
    std::cout << dd::apply(values3, stages) << '\n';
}

struct Employee
{
    int id;
    bool is_fulltime;
    std::string org;
};

void map_group_by_example()
{
    auto employees = std::vector<Employee>
    {
        {1, true , "A"},
        {2, true , "B"},
        {3, false, "A"},
        {4, true , "B"}
    };

    dd::apply_debug(
            employees,
            dd::map_group_by<std::unordered_map>(
                &Employee::org,
                dd::filter(&Employee::is_fulltime),
                dd::count()
            ),
            dd::for_each([] (auto&& org, auto&& count) {
                std::cout << org << ' ' << count << '\n';
            })
    );

    dd::apply_debug(
            employees,
            dd::map_group_by<std::unordered_map>(
                &Employee::org,
                dd::filter(&Employee::is_fulltime),
                dd::tee(
                    dd::compose(dd::transform(&Employee::id), dd::max()),
                    dd::count()
                ),
                dd::expand_complete()
            ),
            dd::for_each([] (auto&& org, auto&& max_id, auto&& count) {
                std::cout << org << ' ' << max_id << ' ' << count << '\n';
            })
    );
}


int main()
{
    // Simple pipeline
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    dd::apply(
            numbers,
            dd::filter([](int x) { return x % 2 == 0; }),
            dd::transform([](int x) { return x * 2; }),
            dd::for_each([](int x) { std::cout << x << '\n'; })
    );
    // Output: 4, 8

    // Generator-based pipeline with enumerate
    auto result = dd::apply(
            dd::iota(1),  // infinite sequence
            dd::filter([](int x) { return x % 3 == 0; }),
            dd::take_n(5),
            dd::enumerate(),  // Add index to each element
            dd::transform([]([[maybe_unused]] int idx, int val) { return val * 2; }),
            dd::to<std::vector>()
    );
    // result: {6, 12, 18, 24, 30}
    std::cout << result << '\n';

    // Group and aggregate
    dd::apply(
            std::vector{1, 2, 3, 4, 5, 6, 7, 8},
            dd::map_group_by<std::map>(
                [](int x) { return x % 3; },  // Group by remainder
                dd::count()
            ),
            dd::for_each([](int key, size_t count) {
                std::cout << "Remainder " << key << ": " << count << " items\n";
            })
    );

    // Enumerate
    dd::apply(
            {100, 200, 300},
            dd::zip_result([] (int x) { return x * 2; }),
            dd::enumerate(),
            dd::for_each([] (auto&& index, auto&& orig, auto&& zipped) {
                static_assert(std::is_same_v<decltype(index), const int&>); // because enumeration index is const&
                static_assert(std::is_same_v<decltype(orig), int&&>); // because input range was &&
                static_assert(std::is_same_v<decltype(zipped), int&&>); // because zip_result() returns temporary
                std::cout << '#' << index << ": " << orig << ' ' << zipped << '\n';
            })
    );


    // In-place sort
    std::vector<int> ints = { 1, 2, 3 };
    auto res = dd::apply(
            std::ref(ints),
            dd::sort()
    );
    static_assert(std::is_same_v<decltype(res), std::reference_wrapper<std::vector<int>>>);

    // Input range may be composition itself
    dd::apply(
            dd::compose(dd::iota(1), dd::filter([] (auto x) { return x % 3 == 0; })),
            dd::take_n(10),
            dd::transform([] (auto x) { return x * 2; }),
            dd::for_each([sep = ""] (auto x) mutable {
                std::cout << std::exchange(sep, ", ") << x;
            })
    );
    std::cout << '\n';

    // Multi-arguments
    dd::apply(
            dd::compose(dd::iota(1), dd::filter([] (auto x) { return x % 3 == 0; })),
            dd::zip_result([] (const int x) { return std::to_string(x); }),
            dd::zip_result([] (const int x, const std::string&) { return 2*x; }),
            dd::take_n(10),
            dd::for_each([] (auto&& x, auto&& y, auto&& z) mutable {
                std::cout << x << ',' << std::quoted(y) << ',' << z << "  ";
            })
    );
    std::cout << '\n';

    // zip_result() with flatten()
    dd::apply(
            std::vector<int>{1, 2, 3}, // zip_result() would receive const int&, despite range being &&,
                                       // because next stages would receive items from range as &&
            dd::zip_result([] (const int& x) { return std::vector<int>{x * 1, x * 11, x * 111}; }),
            dd::flatten(), // for_each() would receive 'x' as const int&, because it will be
                           // combined with several elements from zip_result()
            dd::for_each([] (const int& x, int&& y) mutable { // 'y' is int&& because it is from temporary container
                std::cout << "X=" << x << " Y=" << y << '\n';
            })
            );
    std::cout << '\n';

    // flatten can also work with generators
    dd::apply(
            std::vector<int>{1, 2, 3},
            dd::zip_result([] (int x) { return dd::iota(x + 10, x + 15); }),
            dd::flatten(),
            dd::for_each([] (auto&& x, auto&& y) mutable {
                std::cout << "X=" << x << " Y=" << y << '\n';
            })
            );
    std::cout << '\n';

    return 0;
}
