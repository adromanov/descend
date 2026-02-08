#include <doctest.h>

#include "descend/descend.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace {

namespace dd = descend;

TEST_CASE("Transform stage doubles values")
{
    auto result = dd::apply(
        std::vector{1, 2, 3, 4, 5},
        dd::transform([](int x) { return x * 2; }),
        dd::to<std::vector>()
    );

    CHECK(result == std::vector{2, 4, 6, 8, 10});
}

TEST_CASE("Filter stage removes odd numbers")
{
    auto result = dd::apply(
        std::vector{1, 2, 3, 4, 5, 6},
        dd::filter([](int x) { return x % 2 == 0; }),
        dd::to<std::vector>()
    );

    CHECK(result == std::vector{2, 4, 6});
}

TEST_CASE("Transform and filter")
{
    auto result = dd::apply(
        std::vector{1, 2, 3, 4, 5},
        dd::transform([](int x) { return x * 2; }),
        dd::filter([](int x) { return x > 5; }),
        dd::to<std::vector>()
    );

    CHECK(result == std::vector{6, 8, 10});
}

TEST_CASE("Take stage limits output")
{
    auto result = dd::apply(
        std::vector{1, 2, 3, 4, 5},
        dd::take_n(3),
        dd::to<std::vector>()
    );

    CHECK(result == std::vector{1, 2, 3});
}

TEST_CASE("Max stage finds maximum value")
{
    auto result = dd::apply(
        std::vector{3, 7, 2, 9, 4},
        dd::max()
    );

    CHECK(result == 9);
}

TEST_CASE("Generator with iota produces sequence")
{
    auto result = dd::apply(
        dd::iota(1, 6),
        dd::to<std::vector>()
    );

    CHECK(result == std::vector{1, 2, 3, 4, 5});
}

TEST_CASE("Generator with iota and transform")
{
    auto result = dd::apply(
        dd::iota(1, 4),
        dd::transform([](int x) { return x * x; }),
        dd::to<std::vector>()
    );

    CHECK(result == std::vector{1, 4, 9});
}

TEST_CASE("Empty vector produces empty result")
{
    auto result = dd::apply(
        std::vector<int>{},
        dd::transform([](int x) { return x * 2; }),
        dd::to<std::vector>()
    );

    CHECK(result.empty());
}

TEST_CASE("For each stage accumulates sum")
{
    int sum = 0;
    dd::apply(
        std::vector{1, 2, 3, 4, 5},
        dd::for_each([&sum](int x) { sum += x; })
    );

    CHECK(sum == 15);
}

TEST_CASE("Type conversion works with transform")
{
    auto result = dd::apply(
        std::vector{1, 2, 3},
        dd::transform([](int x) { return static_cast<double>(x) * 1.5; }),
        dd::to<std::vector>()
    );

    CHECK(result.size() == 3);
    CHECK(result[0] == doctest::Approx(1.5));
    CHECK(result[1] == doctest::Approx(3.0));
    CHECK(result[2] == doctest::Approx(4.5));
}

TEST_CASE("String transformation")
{
    auto result = dd::apply(
        std::vector<std::string>{"hello", "world"},
        dd::transform([](const std::string& s) {
            std::string upper = s;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            return upper;
        }),
        dd::to<std::vector>()
    );

    CHECK(result == std::vector<std::string>{"HELLO", "WORLD"});
}

TEST_CASE("name2id => id2name")
{
    {
        std::map<std::string, int> name2id = {{"Alice", 1}, {"Bob", 2}};

        auto id2name = dd::apply(
            std::move(name2id),
            dd::expand(),
            dd::swizzle<1, 0>(),
            dd::to<std::map>()
        );
        CHECK(id2name == std::map<int, std::string>{
                {1, "Alice"}, {2, "Bob"}});
    }

    {
        std::map<std::string, int> name2id = {{"Alice", 1}, {"Bob", 2}, {"Carol", 1}};

        auto id2name = dd::apply(
            std::move(name2id),
            dd::expand(),
            dd::swizzle<1, 0>(),
            dd::to<std::multimap>()
        );
        // 2 multimaps can be different in terms of order of elements, let's convert to vec and sort

        auto id2name_vec = dd::apply(
                id2name,
                dd::to<std::vector>(),
                dd::sort()
                );

        CHECK(id2name_vec == std::vector<std::pair<int, std::string>>{
                {1, "Alice"}, {1, "Carol"}, {2, "Bob"}});
    }
}

TEST_CASE("name2person => id2name")
{
    struct Person { int age; int id; };

    {
        std::map<std::string, Person> name2person = {{"Bilbo", {111, 1}}, {"Frodo", {50, 2}}};

        auto id2name = dd::apply(
            std::move(name2person),
            dd::expand(),
            dd::transform_arg<1>(&Person::id),
            dd::swizzle<1, 0>(),
            dd::to<std::map>()
        );
        CHECK(id2name == std::map<int, std::string>{
                {1, "Bilbo"}, {2, "Frodo"}});
    }
}

TEST_CASE("count")
{
    const auto count1 = dd::apply(
            {1, 1, 1, 1},
            dd::count());
    CHECK(count1 == 4);

    const auto count2 = dd::apply(
            {1, 1, 1},
            dd::count(),
            dd::transform_complete([] (auto x) { return x * 3; }));
    CHECK(count2 == 9);
}

TEST_CASE("tee simple")
{
    const auto [count, max] =
        dd::apply(
            {5, 6, 8, 7},
            dd::tee(
                dd::count(),
                dd::max()));

    CHECK(count == 4);
    CHECK(max.has_value());
    CHECK(max == 8);
}

TEST_CASE("tee with compositions")
{
    const auto [count, max] =
        dd::apply(
            {5, 6, 8, 7},
            dd::tee(
                dd::compose(
                    dd::count(),
                    dd::transform_complete([] (auto&& x) { return x * 3; })
                ),
                dd::compose(
                    dd::max(),
                    dd::unwrap_optional_complete(),
                    dd::transform_complete([] (auto&& x) { return std::to_string(x); })
                )
            )
        );

    CHECK(count == 12);
    CHECK(max.has_value());
    CHECK(max == "8");
}

// Helper to convert args<A, B> to std::pair<A, B>
// Will be replaced by a proper dd::make_pair() stage later
constexpr auto to_pair()
{
    return dd::transform([](auto&& a, auto&& b) {
        return std::make_pair(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b));
    });
}

TEST_CASE("group_by consecutive runs")
{
    // Input: [1, 1, 2, 2, 2, 1, 3, 3]
    // Expected groups: [1,1], [2,2,2], [1], [3,3]
    std::vector<std::pair<int, std::vector<int>>> result;

    dd::apply(
        std::vector{1, 1, 2, 2, 2, 1, 3, 3},
        dd::group_by(
            std::identity(),
            dd::to<std::vector>()
        ),
        to_pair(),
        dd::for_each([&result](std::pair<int, std::vector<int>> p) {
            result.push_back(std::move(p));
        })
    );

    REQUIRE(result.size() == 4);
    CHECK(result[0] == std::pair{1, std::vector{1, 1}});
    CHECK(result[1] == std::pair{2, std::vector{2, 2, 2}});
    CHECK(result[2] == std::pair{1, std::vector{1}});  // 1 appears again!
    CHECK(result[3] == std::pair{3, std::vector{3, 3}});
}

TEST_CASE("group_by with count - run length encoding")
{
    // Input: "aaabbc"
    // Expected: [(a,3), (b,2), (c,1)]
    auto result = dd::apply(
        std::string{"aaabbc"},
        dd::group_by(
            std::identity(),
            dd::count()
        ),
        to_pair(),
        dd::to<std::vector>()
    );

    REQUIRE(result.size() == 3);
    CHECK(result[0] == std::pair{'a', std::size_t{3}});
    CHECK(result[1] == std::pair{'b', std::size_t{2}});
    CHECK(result[2] == std::pair{'c', std::size_t{1}});
}

TEST_CASE("group_by single element groups")
{
    auto result = dd::apply(
        std::vector{1, 2, 3, 4, 5},
        dd::group_by(
            std::identity(),
            dd::count()
        ),
        to_pair(),
        dd::to<std::vector>()
    );

    REQUIRE(result.size() == 5);
    for (const auto& [key, count] : result) {
        CHECK(count == 1);
    }
}

TEST_CASE("group_by single group")
{
    auto result = dd::apply(
        std::vector{1, 1, 1, 1, 1},
        dd::group_by(
            std::identity(),
            dd::to<std::vector>()
        ),
        to_pair(),
        dd::to<std::vector>()
    );

    REQUIRE(result.size() == 1);
    CHECK(result[0] == std::pair{1, std::vector{1, 1, 1, 1, 1}});
}

TEST_CASE("group_by empty input")
{
    auto result = dd::apply(
        std::vector<int>{},
        dd::group_by(
            std::identity(),
            dd::count()
        ),
        to_pair(),
        dd::to<std::vector>()
    );

    CHECK(result.empty());
}

TEST_CASE("group_by with transform in subchain")
{
    auto result = dd::apply(
        std::vector{1, 1, 2, 2, 3},
        dd::group_by(
            std::identity(),
            dd::transform([](int x) { return x * 10; }),
            dd::to<std::vector>()
        ),
        to_pair(),
        dd::to<std::vector>()
    );

    REQUIRE(result.size() == 3);
    CHECK(result[0] == std::pair{1, std::vector{10, 10}});
    CHECK(result[1] == std::pair{2, std::vector{20, 20}});
    CHECK(result[2] == std::pair{3, std::vector{30}});
}

TEST_CASE("group_by with key extractor")
{
    struct Item { int category; int value; };

    auto result = dd::apply(
        std::vector<Item>{{1, 10}, {1, 20}, {2, 30}, {2, 40}, {1, 50}},
        dd::group_by(
            &Item::category,
            dd::transform([](const Item& item) { return item.value; }),
            dd::to<std::vector>()
        ),
        to_pair(),
        dd::to<std::vector>()
    );

    REQUIRE(result.size() == 3);
    CHECK(result[0] == std::pair{1, std::vector{10, 20}});
    CHECK(result[1] == std::pair{2, std::vector{30, 40}});
    CHECK(result[2] == std::pair{1, std::vector{50}});
}

} // namespace anonymous
