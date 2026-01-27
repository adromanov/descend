#include <doctest.h>

#include "descend/stages.hpp"

namespace {

namespace dd = descend;

constexpr auto expand = [] <class Arg> (Arg&& arg) -> decltype(auto) {
    return dd::detail::stages::expand_stage::expand_to_args((Arg&&) arg);
};
using dd::detail::args;

TEST_CASE("expand non-expandable non-args")
{
    int i = 1;

    SUBCASE("prvalue as T&&") {
        auto&& res = expand(1); // dangling reference, but we would use it inside full expression containing temporary
        static_assert(std::is_same_v<decltype(res), int&&>);
    }

    SUBCASE("lvalue ref as T&") {
        auto&& res = expand(i);
        static_assert(std::is_same_v<decltype(res), int&>);
    }

    SUBCASE("const lvalue ref as const T&") {
        auto&& res = expand(std::as_const(i));
        static_assert(std::is_same_v<decltype(res), const int&>);
    }

    SUBCASE("rvalue ref as T&&") {
        auto&& res = expand(std::move(i));
        static_assert(std::is_same_v<decltype(res), int&&>);
    }
}

TEST_CASE("expand std::tuple")
{
    SUBCASE("const std::tuple&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto t = std::tuple<int, const long, bool&, const double&, float&&, const char&&>
                           {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        auto res = expand(std::as_const(t));
        static_assert(std::is_same_v<decltype(res), args<const int&, const long&, bool&, const double&, float&, const char&>>); // TODO is that what right design??
    }

    SUBCASE("std::tuple&&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto t = std::tuple<int, const long, bool&, const double&, float&&, const char&&>
                           {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        auto res = expand(std::move(t));
        static_assert(std::is_same_v<decltype(res), args<int&&, const long&&, bool&, const double&, float&&, const char&&>>);
    }
}

TEST_CASE("expand std::pair")
{
    SUBCASE("const std::pair&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto p1 = std::pair<int, const long> { 0, 7 };
        auto p2 = std::pair<bool &, const double&> { b, d };
        auto p3 = std::pair<float&&, const char&&> { std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        auto res1 = expand(std::as_const(p1));
        auto res2 = expand(std::as_const(p2));
        auto res3 = expand(std::as_const(p3));
        static_assert(std::is_same_v<decltype(res1), args<const int&, const long&>>);
        static_assert(std::is_same_v<decltype(res2), args<bool&, const double&>>);
        static_assert(std::is_same_v<decltype(res3), args<float&, const char&>>);
    }

    SUBCASE("std::pair&&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto p1 = std::pair<int, const long> { 0, 7 };
        auto p2 = std::pair<bool &, const double&> { b, d };
        auto p3 = std::pair<float&&, const char&&> { std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        auto res1 = expand(std::move(p1));
        auto res2 = expand(std::move(p2));
        auto res3 = expand(std::move(p3));
        static_assert(std::is_same_v<decltype(res1), args<int&&, const long&&>>);
        static_assert(std::is_same_v<decltype(res2), args<bool&, const double&>>);
        static_assert(std::is_same_v<decltype(res3), args<float&&, const char&&>>);
    }
}

TEST_CASE("expand std::array")
{
    auto array = std::array<int, 2>{ 1, 2 };

    SUBCASE("const std::array&") {
        auto res = expand(std::as_const(array));
        static_assert(std::is_same_v<decltype(res), args<const int&, const int&>>);
    }

    SUBCASE("std::array&&") {
        auto res = expand(std::move(array));
        static_assert(std::is_same_v<decltype(res), args<int&&, int&&>>);
    }
}

TEST_CASE("expand args without tuples")
{
    SUBCASE("const args&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto t = args<int, const long, bool&, const double&, float&&, const char&&>
                     {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        auto res = expand(std::as_const(t));
        static_assert(std::is_same_v<decltype(res), args<const int&, const long&, const bool&, const double&, const float&, const char&>>); // TODO is that what right design??
    }

    SUBCASE("args&&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto t = args<int, const long, bool&, const double&, float&&, const char&&>
                     {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        auto res = expand(std::move(t));
        static_assert(std::is_same_v<decltype(res), args<int&&, const long&&, bool&, const double&, float&&, const char&&>>);
    }
}

TEST_CASE("expand args with tuples")
{
    /*
    using Tuple = std::tuple<int, const long, bool&, const double&, float&&, const char&&>;

    SUBCASE("tuple by value") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto t = Tuple{ 0, 1, b, d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        auto res = args<Tuple>{std::move(t)};

        auto 
        static_assert(std::is_same_v<decltype(res), args<const int&, const long&, const bool&, const double&, const float&, const char&>>); // TODO is that what right design??
    }

    SUBCASE("args&&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto t = args<int, const long, bool&, const double&, float&&, const char&&>
                     {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        auto res = expand(std::move(t));
        static_assert(std::is_same_v<decltype(res), args<int&&, const long&&, bool&, const double&, float&&, const char&&>>);
    }
    */
}

} // namespace anonymous
