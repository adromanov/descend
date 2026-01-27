#include <doctest.h>

#include "descend/args.hpp"

namespace {

namespace dd = descend;

TEST_CASE("args_invoke with args")
{
    using dd::detail::args;

    SUBCASE("const args&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto a = args<int, const long, bool&, const double&, float&&, const char&&>
                     {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        dd::detail::args_invoke([] (auto&& a0, auto&& a1, auto&& a2, auto&& a3, auto&& a4, auto&& a5) {
            static_assert(std::is_same_v<decltype(a0), const int&>);
            static_assert(std::is_same_v<decltype(a1), const long&>);
            static_assert(std::is_same_v<decltype(a2), const bool&>);
            static_assert(std::is_same_v<decltype(a3), const double&>);
            static_assert(std::is_same_v<decltype(a4), const float&>);
            static_assert(std::is_same_v<decltype(a5), const char&>);
        }, std::as_const(a));
    }

    SUBCASE("args&&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto a = args<int, const long, bool&, const double&, float&&, const char&&>
                     {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        dd::detail::args_invoke([] (auto&& a0, auto&& a1, auto&& a2, auto&& a3, auto&& a4, auto&& a5) {
            static_assert(std::is_same_v<decltype(a0), int&&>);
            static_assert(std::is_same_v<decltype(a1), const long&&>);
            static_assert(std::is_same_v<decltype(a2), bool&>);
            static_assert(std::is_same_v<decltype(a3), const double&>);
            static_assert(std::is_same_v<decltype(a4), float&&>);
            static_assert(std::is_same_v<decltype(a5), const char&&>);
        }, std::move(a));
    }
}

TEST_CASE("args_invoke with non-args")
{
    int i = 1;

    SUBCASE("prvalue as T&&") {
        dd::detail::args_invoke([] (auto&& x) {
            static_assert(std::is_same_v<decltype(x), int&&>);
        }, 1);
    }

    SUBCASE("lvalue ref as T&") {
        dd::detail::args_invoke([] (auto&& x) {
            static_assert(std::is_same_v<decltype(x), int&>);
        }, i);
    }

    SUBCASE("const lvalue ref as const T&") {
        dd::detail::args_invoke([] (auto&& x) {
            static_assert(std::is_same_v<decltype(x), const int&>);
        }, std::as_const(i));
    }

    SUBCASE("rvalue ref as T&&") {
        dd::detail::args_invoke([] (auto&& x) {
            static_assert(std::is_same_v<decltype(x), int&&>);
        }, std::move(i));
    }
}

TEST_CASE("get for args")
{
    using dd::detail::args;

    SUBCASE("const args&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto a = args<int, const long, bool&, const double&, float&&, const char&&>
                     {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        static_assert(std::is_same_v<decltype(get<0>(std::as_const(a))), const int&>);
        static_assert(std::is_same_v<decltype(get<1>(std::as_const(a))), const long&>);
        static_assert(std::is_same_v<decltype(get<2>(std::as_const(a))), const bool&>);
        static_assert(std::is_same_v<decltype(get<3>(std::as_const(a))), const double&>);
        static_assert(std::is_same_v<decltype(get<4>(std::as_const(a))), const float&>);
        static_assert(std::is_same_v<decltype(get<5>(std::as_const(a))), const char&>);
    }

    SUBCASE("args&&") {
        bool b = false;
        double d = 3.14;
        float f = 1.0;
        const char c = 'c';

        auto a = args<int, const long, bool&, const double&, float&&, const char&&>
                     {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)

        static_assert(std::is_same_v<decltype(get<0>(std::move(a))), int&&>);
        static_assert(std::is_same_v<decltype(get<1>(std::move(a))), const long&&>);
        static_assert(std::is_same_v<decltype(get<2>(std::move(a))), bool&>);
        static_assert(std::is_same_v<decltype(get<3>(std::move(a))), const double&>);
        static_assert(std::is_same_v<decltype(get<4>(std::move(a))), float&&>);
        static_assert(std::is_same_v<decltype(get<5>(std::move(a))), const char&&>);
    }
}

} // namespace anonymous
