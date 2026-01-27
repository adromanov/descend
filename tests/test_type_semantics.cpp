#include "descend/descend.hpp"

#include <doctest.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define DD_CHECK_TYPE_OF(X, ...) do {                   \
    const bool same = std::is_same_v<decltype(X), __VA_ARGS__>;\
    CHECK_MESSAGE(same,                                 \
            "types mismatch: ",                         \
            #X,                                         \
            " is '",                                    \
            dd::detail::type_name<decltype(X)>(),       \
            "' expecting '",                            \
            dd::detail::type_name<__VA_ARGS__>(),       \
            "'"                                         \
            );                                          \
    if (!same) {                                        \
        throw dd::detail::test_failed_exception{};      \
    }                                                   \
} while (false)

namespace {

namespace dd = descend;

TEST_CASE("iterate through vector")
{
    std::vector<int> ints{1, 2, 3};

    SUBCASE("by default yields const T&") {
        dd::apply(
            ints,
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const int&);
            })
        );
    }

    SUBCASE("std::as_const yields const T&") {
        dd::apply(
            std::as_const(ints),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const int&);
            })
        );
    }

    SUBCASE("std::cref yields const T&") {
        dd::apply(
            std::cref(ints),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const int&);
            })
        );
    }

    SUBCASE("std::ref yields T&") {
        dd::apply(
            std::ref(ints),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&);
            })
        );
    }

    SUBCASE("std::move yields T&&") {
        dd::apply(
            std::move(ints),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&&);
            })
        );
    }

    SUBCASE("prvalue yields T&&") {
        dd::apply(
            [] () { return std::vector<int>{1, 2, 3}; } (),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&&);
            })
        );
    }
}


TEST_CASE("iterate through generator")
{
    auto gen = dd::iota(1, 101);

    SUBCASE("by default yields T&&") {
        dd::apply(
            gen,
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&&);
            })
        );
    }

    SUBCASE("std::ref yields T&&") {
        dd::apply(
            std::ref(gen),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&&);
            })
        );
    }

    SUBCASE("std::move yields T&&") {
        dd::apply(
            std::move(gen),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&&);
            })
        );
    }

    // No std::cref or std::as_const since calling () on generator is non-const
    // TODO: add static_assert for nicer error messages
}

TEST_CASE("iterate through unordered_set")
{
    std::unordered_set<int> int_set = {1, 2, 3};

    SUBCASE("by default yields const T&") {
        dd::apply(
            int_set,
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const int&);
            })
        );
    }

    // std::ref is not supported
    // TODO: add static_assert for nicer error message

    SUBCASE("std::move yields T&& (extracting keys)") {
        dd::apply(
            std::move(int_set),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&&);
            })
        );
    }
}

TEST_CASE("iterate through unordered_map")
{
    std::unordered_map<int, std::string> int_map = {{1, "11"}, {2, "22"}};

    SUBCASE("by default yields const std::pair<const Key, Mapped>&") {
        dd::apply(
            int_map,
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const std::pair<const int, std::string>&);
            })
        );
    }

    SUBCASE("std::ref yields std::pair<const Key, Mapped>&") {
        dd::apply(
            std::ref(int_map),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, std::pair<const int, std::string>&);
            })
        );
    }

    SUBCASE("std::move yields std::pair<Key&&, Mapped&&>&&") {
        dd::apply(
            std::move(int_map),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, std::pair<int&&, std::string&&>&&);
            })
        );
    }
}

TEST_CASE("iterate through unordered_map with expand")
{
    std::unordered_map<int, std::string> int_map = {{1, "11"}, {2, "22"}};

    SUBCASE("by default yields (const Key&, const Mapped&)") {
        dd::apply(
            int_map,
            dd::expand(),
            dd::for_each([](auto&& k, auto&& v) {
                DD_CHECK_TYPE_OF(k, const int&);
                DD_CHECK_TYPE_OF(v, const std::string&);
            })
        );
    }

    SUBCASE("std::ref yields (const Key&, Mapped&)") {
        dd::apply(
            std::ref(int_map),
            dd::expand(),
            dd::for_each([](auto&& k, auto&& v) {
                DD_CHECK_TYPE_OF(k, const int&);
                DD_CHECK_TYPE_OF(v, std::string&);
            })
        );
    }

    SUBCASE("std::move yields (Key&&, Mapped&&)") {
        dd::apply(
            std::move(int_map),
            dd::expand(),
            dd::for_each([](auto&& k, auto&& v) {
                DD_CHECK_TYPE_OF(k, int&&);
                DD_CHECK_TYPE_OF(v, std::string&&);
            })
        );
    }
}

TEST_CASE("iterate through map")
{
    std::map<int, std::string> int_map = {{1, "11"}, {2, "22"}};

    SUBCASE("by default yields const std::pair<const Key, Mapped>&") {
        dd::apply(
            int_map,
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const std::pair<const int, std::string>&);
            })
        );
    }

    SUBCASE("std::ref yields std::pair<const Key, Mapped>&") {
        dd::apply(
            std::ref(int_map),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, std::pair<const int, std::string>&);
            })
        );
    }

    // no extract happens since it is O(log(N)) due to rebalancing after each extract
    SUBCASE("std::move yields std::pair<const Key, Mapped>&&") {
        dd::apply(
            std::move(int_map),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, std::pair<const int, std::string>&&);
            })
        );
    }
}

TEST_CASE("iterate through map with expand")
{
    std::map<int, std::string> int_map = {{1, "11"}, {2, "22"}};

    SUBCASE("by default yields (const Key&, const Mapped&)") {
        dd::apply(
            int_map,
            dd::expand(),
            dd::for_each([](auto&& k, auto&& v) {
                DD_CHECK_TYPE_OF(k, const int&);
                DD_CHECK_TYPE_OF(v, const std::string&);
            })
        );
    }

    SUBCASE("std::ref yields (const Key&, Mapped&)") {
        dd::apply(
            std::ref(int_map),
            dd::expand(),
            dd::for_each([](auto&& k, auto&& v) {
                DD_CHECK_TYPE_OF(k, const int&);
                DD_CHECK_TYPE_OF(v, std::string&);
            })
        );
    }

    SUBCASE("std::move yields (const Key&&, Mapped&&)") {
        dd::apply(
            std::move(int_map),
            dd::expand(),
            dd::for_each([](auto&& k, auto&& v) {
                DD_CHECK_TYPE_OF(k, const int&&);
                DD_CHECK_TYPE_OF(v, std::string&&);
            })
        );
    }
}

TEST_CASE("transform")
{
    std::vector<std::string> strs = {"a", "b", "c"};

    SUBCASE("identify by copy yields T&&)") {
        dd::apply(
            strs,
            dd::transform([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const std::string&);
                return x;
            }),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, std::string&&);
            })
        );
    }

    SUBCASE("decltype(auto) by default yields const T&)") {
        dd::apply(
            strs,
            dd::transform([](auto&& x) -> decltype(auto) {
                DD_CHECK_TYPE_OF(x, const std::string&);
                return x;
            }),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const std::string&);
            })
        );
    }

    SUBCASE("decltype(auto) after std::ref yields T&&)") {
        dd::apply(
            std::ref(strs),
            dd::transform([](auto&& x) -> decltype(auto) {
                DD_CHECK_TYPE_OF(x, std::string&);
                return x;
            }),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, std::string&);
            })
        );
    }
}


TEST_CASE("Map expand transform preserves references")
{
    std::map<int, std::string> m = {{1, "abc"}, {2, "def"}};
    dd::apply(
        std::ref(m),
        dd::expand(),
        dd::transform([](auto&& k, auto&& v) {
            DD_CHECK_TYPE_OF(k, const int&);
            DD_CHECK_TYPE_OF(v, std::string&);
            return static_cast<std::size_t>(k) + v.size();
        }),
        dd::for_each([](std::size_t) {})
    );
}

// Tests for zip_result stage

TEST_CASE("zip unique ptrs")
{
    std::vector<std::unique_ptr<int>> ints;
    ints.push_back(std::make_unique<int>(1));
    ints.push_back(std::make_unique<int>(2));
    ints.push_back(std::make_unique<int>(3));
    dd::apply(
        ints,
        dd::zip_result([] (auto&& ptr) {
            DD_CHECK_TYPE_OF(ptr, const std::unique_ptr<int>&);
            return std::make_unique<int>(*ptr);
        }),
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, const std::unique_ptr<int>&);
            DD_CHECK_TYPE_OF(y, std::unique_ptr<int>&&);
        })
    );
}

TEST_CASE("Double zip preserves rvalue ref")
{
    dd::apply(
        std::vector<int>{1, 2, 3},  // rvalue -> produces int&&
        dd::zip_result([](auto&& x) {
            DD_CHECK_TYPE_OF(x, const int&);
            return std::vector<int>{22, 33, 44};
        }),
        dd::zip_result([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, const int&);
            DD_CHECK_TYPE_OF(y, const std::vector<int>&);
            return 42;
        }),
        dd::for_each([](auto&& x, auto&& y, auto&& z) {
            DD_CHECK_TYPE_OF(x, int&&);
            DD_CHECK_TYPE_OF(y, std::vector<int>&&);
            DD_CHECK_TYPE_OF(z, int&&);
        })
    );
}

// Tests for flatten stage with zip_result

TEST_CASE("Flatten converts rvalue ref to const ref")
{
    // Safe default: flatten() converts T&& -> const T& for prefix arguments
    dd::apply(
        std::vector<int>{1, 2, 3},  // rvalue -> produces int&&
        dd::zip_result([](int) { return std::vector<int>{22, 33, 44}; }),
        dd::flatten(),  // SAFE: converts int&& -> const int&
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, const int&);  // Converted for safety!
            DD_CHECK_TYPE_OF(y, int&&);
        })
    );
}

TEST_CASE("Flatten_forward preserves rvalue ref")
{
    // Opt-in risky behavior: flatten_forward() preserves T&& for prefix arguments
    dd::apply(
        std::vector<int>{1, 2, 3},  // rvalue -> produces int&&
        dd::zip_result([](int) { return std::vector<int>{22, 33, 44}; }),
        dd::flatten_forward(),  // RISKY: preserves int&&
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, int&&);  // Preserved (risky!)
            DD_CHECK_TYPE_OF(y, int&&);
        })
    );
}

TEST_CASE("Flatten preserves lvalue refs")
{
    // Both flatten() and flatten_forward() preserve lvalue references
    std::vector<int> vec{1, 2, 3};
    dd::apply(
        std::ref(vec),  // mutable lvalue ref -> produces int&
        dd::zip_result([](const int&) { return std::vector<int>{22, 33, 44}; }),
        dd::flatten(),  // Preserves int& (safe)
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, int&);  // Preserved
            DD_CHECK_TYPE_OF(y, int&&);
        })
    );
}

TEST_CASE("Flatten with generator yields mixed refs")
{
    dd::apply(
        std::vector<int>{1, 2, 3},
        dd::zip_result([](int) { return dd::iota(11, 15); }),
        dd::flatten(),
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, const int&);  // Converted
            DD_CHECK_TYPE_OF(y, int&&);  // Generator produces rvalue
        })
    );
}

TEST_CASE("flatten with std::ref")
{
    std::vector<char> chars = { 'a', 'b' };

    dd::apply(
        std::vector<int>{1, 2, 3},
        dd::zip_result([&](auto&& x) {
            DD_CHECK_TYPE_OF(x, const int&);
            return std::ref(chars);
        }),
        dd::flatten(),
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, const int&); // because converted
            DD_CHECK_TYPE_OF(y, char &);
        })
    );
}

TEST_CASE("flatten with std::cref")
{
    std::vector<char> chars = { 'a', 'b' };

    dd::apply(
        std::vector<int>{1, 2, 3},
        dd::zip_result([&](auto&& x) {
            DD_CHECK_TYPE_OF(x, const int&);
            return std::cref(chars);
        }),
        dd::flatten(),
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, const int&); // because converted
            DD_CHECK_TYPE_OF(y, const char &);
        })
    );
}

TEST_CASE("flatten with T&")
{
    std::vector<char> chars = { 'a', 'b' };

    dd::apply(
        std::vector<int>{1, 2, 3},
        dd::zip_result([&](auto&& x) -> decltype(auto) {
            DD_CHECK_TYPE_OF(x, const int&);
            DD_CHECK_TYPE_OF((chars), std::vector<char>&);
            return (chars); // returnning just chars without braces would create a copy due to decltype(auto) rules
        }),
        dd::flatten(),
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, const int&); // because converted
            DD_CHECK_TYPE_OF(y, const char &);
        })
    );
}

TEST_CASE("flatten with std::unordered_map&&")
{
    dd::apply(
        std::vector<int>{1, 2, 3},
        dd::zip_result([](auto&& x) {
            DD_CHECK_TYPE_OF(x, const int&);
            return std::unordered_map<std::string, bool>{{"true", true}, {"false", false}};
        }),
        dd::flatten(),
        dd::for_each([](auto&& x, auto&& y) {
            DD_CHECK_TYPE_OF(x, const int&); // because converted
            DD_CHECK_TYPE_OF(y, std::pair<std::string&&, bool&&>&&);
        })
    );
}

TEST_CASE("flatten + expand with std::unordered_map")
{
    dd::apply(
        std::vector<int>{1, 2, 3},
        dd::zip_result([](auto&& x) {
            DD_CHECK_TYPE_OF(x, const int&);
            return std::unordered_map<std::string, bool>{{"true", true}, {"false", false}};
        }),
        dd::flatten(),
        dd::expand(),
        dd::for_each([](auto&& x, auto&& y, auto&& z) {
            DD_CHECK_TYPE_OF(x, const int&);
            DD_CHECK_TYPE_OF(y, std::string&&);
            DD_CHECK_TYPE_OF(z, bool&&);
        })
    );
}

TEST_CASE("unwrap_optional")
{
    std::vector<std::optional<int>> opt_ints = { 1, std::nullopt, 2 };

    SUBCASE("const T&") {
        dd::apply(
            opt_ints,
            dd::unwrap_optional(),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, const int&);
            })
        );
    }

    SUBCASE("T&") {
        dd::apply(
            std::ref(opt_ints),
            dd::unwrap_optional(),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&);
            })
        );
    }

    SUBCASE("T&&") {
        dd::apply(
            std::move(opt_ints),
            dd::unwrap_optional(),
            dd::for_each([](auto&& x) {
                DD_CHECK_TYPE_OF(x, int&&);
            })
        );
    }
}

} // namespace anonymous
