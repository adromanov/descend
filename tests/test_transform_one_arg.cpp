#include <doctest.h>

#include "descend/args.hpp"
#include "descend/helpers.hpp"

namespace {

namespace dd = descend;

using dd::detail::args;
using dd::detail::unit;
using dd::detail::transform_one_arg;

#define CHECK_TYPE(X, ...) static_assert(std::is_same_v<decltype(X), __VA_ARGS__>)

TEST_CASE("transform_one_arg")
{
    unit u = {};

    const auto to_value = [] (auto&&) { return unit{}; };
    const auto to_lvalue_ref = [&u] (auto&&) -> decltype(auto) { return (u); };
    const auto to_const_lvalue_ref = [&u] (auto&&) -> decltype(auto) { return std::as_const(u); };
    auto to_rvalue_ref = [x = unit()] (auto&&) mutable -> decltype(auto) { return std::move(x); };

    bool b = false;
    double d = 3.14;
    float f = 1.0;
    const char c = 'c';

    auto make_test_args = [&] ()
    {
        return args<int, const long, bool&, const double&, float&&, const char&&>
                   {  0,          1,     b,             d, std::move(f), std::move(c) }; // NOLINT(performance-move-const-arg)
    };

    SUBCASE("const args&") {
        // For const args&, all elements become const& when accessed
        auto test_args = make_test_args();

        // Transform index 0 - returns value
        {
            auto res = transform_one_arg<0>(std::as_const(test_args), to_value);
            CHECK_TYPE(res, args<unit, const long&, const bool&, const double&, const float&, const char&>);
        }

        // Transform index 0 - returns lvalue ref
        {
            auto res = transform_one_arg<0>(std::as_const(test_args), to_lvalue_ref);
            CHECK_TYPE(res, args<unit &, const long&, const bool&, const double&, const float&, const char&>);
        }

        // Transform index 0 - returns const lvalue ref
        {
            auto res = transform_one_arg<0>(std::as_const(test_args), to_const_lvalue_ref);
            CHECK_TYPE(res, args<const unit&, const long&, const bool&, const double&, const float&, const char&>);
        }

        // Transform index 0 - returns rvalue ref
        {
            auto res = transform_one_arg<0>(std::as_const(test_args), to_rvalue_ref);
            CHECK_TYPE(res, args<unit&&, const long&, const bool&, const double&, const float&, const char&>);
        }

        // Transform index 1 - returns value
        {
            auto res = transform_one_arg<1>(std::as_const(test_args), to_value);
            CHECK_TYPE(res, args<const int&, unit, const bool&, const double&, const float&, const char&>);
        }

        // Transform index 1 - returns lvalue ref
        {
            auto res = transform_one_arg<1>(std::as_const(test_args), to_lvalue_ref);
            CHECK_TYPE(res, args<const int&, unit&, const bool&, const double&, const float&, const char&>);
        }

        // Transform index 1 - returns const lvalue ref
        {
            auto res = transform_one_arg<1>(std::as_const(test_args), to_const_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const unit&, const bool&, const double&, const float&, const char&>);
        }

        // Transform index 1 - returns rvalue ref
        {
            auto res = transform_one_arg<1>(std::as_const(test_args), to_rvalue_ref);
            CHECK_TYPE(res, args<const int&, unit&&, const bool&, const double&, const float&, const char&>);
        }

        // Transform index 2 - returns value
        {
            auto res = transform_one_arg<2>(std::as_const(test_args), to_value);
            CHECK_TYPE(res, args<const int&, const long&, unit, const double&, const float&, const char&>);
        }

        // Transform index 2 - returns lvalue ref
        {
            auto res = transform_one_arg<2>(std::as_const(test_args), to_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, unit&, const double&, const float&, const char&>);
        }

        // Transform index 2 - returns const lvalue ref
        {
            auto res = transform_one_arg<2>(std::as_const(test_args), to_const_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const unit&, const double&, const float&, const char&>);
        }

        // Transform index 2 - returns rvalue ref
        {
            auto res = transform_one_arg<2>(std::as_const(test_args), to_rvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, unit&&, const double&, const float&, const char&>);
        }

        // Transform index 3 - returns value
        {
            auto res = transform_one_arg<3>(std::as_const(test_args), to_value);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, unit, const float&, const char&>);
        }

        // Transform index 3 - returns lvalue ref
        {
            auto res = transform_one_arg<3>(std::as_const(test_args), to_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, unit&, const float&, const char&>);
        }

        // Transform index 3 - returns const lvalue ref
        {
            auto res = transform_one_arg<3>(std::as_const(test_args), to_const_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const unit&, const float&, const char&>);
        }

        // Transform index 3 - returns rvalue ref
        {
            auto res = transform_one_arg<3>(std::as_const(test_args), to_rvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, unit&&, const float&, const char&>);
        }

        // Transform index 4 - returns value
        {
            auto res = transform_one_arg<4>(std::as_const(test_args), to_value);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const double&, unit, const char&>);
        }

        // Transform index 4 - returns lvalue ref
        {
            auto res = transform_one_arg<4>(std::as_const(test_args), to_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const double&, unit&, const char&>);
        }

        // Transform index 4 - returns const lvalue ref
        {
            auto res = transform_one_arg<4>(std::as_const(test_args), to_const_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const double&, const unit&, const char&>);
        }

        // Transform index 4 - returns rvalue ref
        {
            auto res = transform_one_arg<4>(std::as_const(test_args), to_rvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const double&, unit&&, const char&>);
        }

        // Transform index 5 - returns value
        {
            auto res = transform_one_arg<5>(std::as_const(test_args), to_value);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const double&, const float&, unit>);
        }

        // Transform index 5 - returns lvalue ref
        {
            auto res = transform_one_arg<5>(std::as_const(test_args), to_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const double&, const float&, unit&>);
        }

        // Transform index 5 - returns const lvalue ref
        {
            auto res = transform_one_arg<5>(std::as_const(test_args), to_const_lvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const double&, const float&, const unit&>);
        }

        // Transform index 5 - returns rvalue ref
        {
            auto res = transform_one_arg<5>(std::as_const(test_args), to_rvalue_ref);
            CHECK_TYPE(res, args<const int&, const long&, const bool&, const double&, const float&, unit&&>);
        }
    }

    SUBCASE("args&&") {
        // For args&&, elements are forwarded as-is (preserving their reference types)

        // Transform index 0 (int value type) - returns value
        {
            auto res = transform_one_arg<0>(make_test_args(), to_value);
            CHECK_TYPE(res, args<unit, const long&&, bool&, const double&, float&&, const char&&>);
        }

        // Transform index 0 (int value type) - returns lvalue ref
        {
            auto res = transform_one_arg<0>(make_test_args(), to_lvalue_ref);
            CHECK_TYPE(res, args<unit&, const long&&, bool&, const double&, float&&, const char&&>);
        }

        // Transform index 0 (int value type) - returns const lvalue ref
        {
            auto res = transform_one_arg<0>(make_test_args(), to_const_lvalue_ref);
            CHECK_TYPE(res, args<const unit&, const long&&, bool&, const double&, float&&, const char&&>);
        }

        // Transform index 0 (int value type) - returns rvalue ref
        {
            auto res = transform_one_arg<0>(make_test_args(), to_rvalue_ref);
            CHECK_TYPE(res, args<unit&&, const long&&, bool&, const double&, float&&, const char&&>);
        }

        // Transform index 1 (const long value type) - returns value
        {
            auto res = transform_one_arg<1>(make_test_args(), to_value);
            CHECK_TYPE(res, args<int&&, unit, bool&, const double&, float&&, const char&&>);
        }

        // Transform index 1 (const long value type) - returns lvalue ref
        {
            auto res = transform_one_arg<1>(make_test_args(), to_lvalue_ref);
            CHECK_TYPE(res, args<int&&, unit&, bool&, const double&, float&&, const char&&>);
        }

        // Transform index 1 (const long value type) - returns const lvalue ref
        {
            auto res = transform_one_arg<1>(make_test_args(), to_const_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const unit&, bool&, const double&, float&&, const char&&>);
        }

        // Transform index 1 (const long value type) - returns rvalue ref
        {
            auto res = transform_one_arg<1>(make_test_args(), to_rvalue_ref);
            CHECK_TYPE(res, args<int&&, unit&&, bool&, const double&, float&&, const char&&>);
        }

        // Transform index 2 (bool&) - returns value
        {
            auto res = transform_one_arg<2>(make_test_args(), to_value);
            CHECK_TYPE(res, args<int&&, const long&&, unit, const double&, float&&, const char&&>);
        }

        // Transform index 2 (bool&) - returns lvalue ref
        {
            auto res = transform_one_arg<2>(make_test_args(), to_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, unit&, const double&, float&&, const char&&>);
        }

        // Transform index 2 (bool&) - returns const lvalue ref
        {
            auto res = transform_one_arg<2>(make_test_args(), to_const_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, const unit&, const double&, float&&, const char&&>);
        }

        // Transform index 2 (bool&) - returns rvalue ref
        {
            auto res = transform_one_arg<2>(make_test_args(), to_rvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, unit&&, const double&, float&&, const char&&>);
        }

        // Transform index 3 (const double&) - returns value
        {
            auto res = transform_one_arg<3>(make_test_args(), to_value);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, unit, float&&, const char&&>);
        }

        // Transform index 3 (const double&) - returns lvalue ref
        {
            auto res = transform_one_arg<3>(make_test_args(), to_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, unit&, float&&, const char&&>);
        }

        // Transform index 3 (const double&) - returns const lvalue ref
        {
            auto res = transform_one_arg<3>(make_test_args(), to_const_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const unit&, float&&, const char&&>);
        }

        // Transform index 3 (const double&) - returns rvalue ref
        {
            auto res = transform_one_arg<3>(make_test_args(), to_rvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, unit&&, float&&, const char&&>);
        }

        // Transform index 4 (float&&) - returns value
        {
            auto res = transform_one_arg<4>(make_test_args(), to_value);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const double&, unit, const char&&>);
        }

        // Transform index 4 (float&&) - returns lvalue ref
        {
            auto res = transform_one_arg<4>(make_test_args(), to_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const double&, unit&, const char&&>);
        }

        // Transform index 4 (float&&) - returns const lvalue ref
        {
            auto res = transform_one_arg<4>(make_test_args(), to_const_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const double&, const unit&, const char&&>);
        }

        // Transform index 4 (float&&) - returns rvalue ref
        {
            auto res = transform_one_arg<4>(make_test_args(), to_rvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const double&, unit&&, const char&&>);
        }

        // Transform index 5 (const char&&) - returns value
        {
            auto res = transform_one_arg<5>(make_test_args(), to_value);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const double&, float&&, unit>);
        }

        // Transform index 5 (const char&&) - returns lvalue ref
        {
            auto res = transform_one_arg<5>(make_test_args(), to_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const double&, float&&, unit&>);
        }

        // Transform index 5 (const char&&) - returns const lvalue ref
        {
            auto res = transform_one_arg<5>(make_test_args(), to_const_lvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const double&, float&&, const unit&>);
        }

        // Transform index 5 (const char&&) - returns rvalue ref
        {
            auto res = transform_one_arg<5>(make_test_args(), to_rvalue_ref);
            CHECK_TYPE(res, args<int&&, const long&&, bool&, const double&, float&&, unit&&>);
        }
    }
}

} // namespace anonymous
