#pragma once

#include "descend/helpers.hpp"
#include "descend/iterate.hpp"
#include "descend/stage_styles.hpp"

#include <functional>
#include <system_error>
#include <type_traits>
#include <variant>

namespace descend {

// kind of absl::StatusOr, just for showing the usage
template <class T>
struct error_or : std::variant<T, std::error_code>
{
    using variant_type = std::variant<T, std::error_code>;

    using value_type = T;
    using error_type = std::error_code;

    using std::variant<T, std::error_code>::variant;
    constexpr error_or() = default;
    constexpr error_or(const error_or&) = default;
    constexpr error_or(error_or&&) = default;
    constexpr error_or& operator = (const error_or&) = default;
    constexpr error_or& operator = (error_or&&) = default;

    constexpr bool has_value() const noexcept
    { return this->index() == 0; }

    constexpr bool has_error() const noexcept
    { return this->index() == 1; }

    constexpr decltype(auto) as_variant() &       noexcept { return static_cast<      variant_type& >(*this); };
    constexpr decltype(auto) as_variant() const & noexcept { return static_cast<const variant_type& >(*this); };
    constexpr decltype(auto) as_variant() &&      noexcept { return static_cast<      variant_type&&>(std::move(*this)); };

    constexpr decltype(auto) value() &       { return get_impl<0>(*this); }
    constexpr decltype(auto) value() const & { return get_impl<0>(*this); }
    constexpr decltype(auto) value() &&      { return get_impl<0>(std::move(*this)); }

    constexpr decltype(auto) error() &       { return get_impl<1>(*this); }
    constexpr decltype(auto) error() const & { return get_impl<1>(*this); }
    constexpr decltype(auto) error() &&      { return get_impl<1>(std::move(*this)); }

    template <std::size_t I, class Self>
    static decltype(auto) get_impl(Self&& self)
    {
        return std::get<I>(((Self&&) self).as_variant());
    }
};

inline constexpr auto in_place_value = std::in_place_index<0>;
inline constexpr auto in_place_error = std::in_place_index<1>;

template <class Arg, class T = std::remove_cvref_t<Arg>>
    requires (!std::is_same_v<T, std::error_code>)
auto make_error_or(Arg&& arg)
{
    return error_or<T>{in_place_value, (Arg&&) arg};
}
template <class T>
inline auto make_error_or(const std::error_code ec)
{
    return error_or<T>{in_place_error, ec};
}

namespace detail::stages {

// unwrap_status_or(), to<vector()>
// It was said on the video that unwrap_status_or break early on the first error.
// Alternative behavior is to just filter out errorous values, but it could be done explicitly with filter() stage
struct unwrap_error_or_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    template <class T, class Callable>
    static auto wrap_result_with_error_or_if_needed(
            const error_or<T>& carrier,
            Callable&& callable)
    {
        using callable_result_type = std::remove_cvref_t<std::invoke_result_t<Callable&&>>;
        if constexpr (std::is_same_v<callable_result_type, void>) {
            return carrier.has_value()
                ? std::invoke((Callable&&) callable)
                : [] () {} (); // immediately called lambda which returns void
        } else if constexpr (is_specialization_of_v<error_or, callable_result_type>) {
            // it is already error_or, no need to wrap
            return carrier.has_value()
                ? std::invoke((Callable&&) callable)
                : callable_result_type(in_place_error, carrier.error());
        }
        else {
            return carrier.has_value()
                ? error_or<callable_result_type>{in_place_value, std::invoke((Callable&&) callable)}
                : error_or<callable_result_type>{in_place_error, carrier.error()};
        }
    }

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(std::declval<Input>().value());
        using stage_type = unwrap_error_or_stage;

        error_or<unit> carrier = make_error_or(unit());

        template <class Next>
        void process_incremental(Input&& input, Next&& next)
        {
            switch (input.index()) {
            case 0:
                next.process_incremental(((Input&&) input).value());
                break;
            case 1:
                carrier = make_error_or<unit>(input.error());
                break;
            }
        }

        bool done() const
        {
            return carrier.has_error();
        }

        template <class Next>
        auto end(Next&& next)
        {
            return unwrap_error_or_stage::wrap_result_with_error_or_if_needed(
                    carrier,
                    [&next] () { return next.end(); });
        }
    };

    template <class Input>
    auto make_impl()
    {
        static_assert(is_specialization_of_v<error_or, std::remove_cvref_t<Input>>,
                "unwrap_error_or stage requires error_or as input");
        return impl<Input>{};
    }
};

struct unwrap_error_or_complete_stage
{
    static constexpr auto style = stage_styles::complete_to_complete;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(unwrap_input(std::declval<Input>()).value());
        using stage_type = unwrap_error_or_complete_stage;

        template <class Next>
        auto process_complete(Input&& input, Next&& next)
        {
            auto&& eor = unwrap_input((Input&&) input);
            return unwrap_error_or_stage::wrap_result_with_error_or_if_needed(
                    std::as_const(eor),
                    [&eor, &next] () { return next.process_complete(std::forward<decltype(eor)>(eor).value()); });
        }
    };

    template <class Input>
    auto make_impl()
    {
        static_assert(is_specialization_of_v<error_or, std::remove_cvref_t<unwrapped_input_t<Input>>>,
                "unwrap_error_or_complete stage requires erro_or as input");
        return impl<Input>{};
    }
};

} // namespace detail::stages


inline namespace stages {
constexpr auto unwrap_error_or()
{
    return detail::stages::unwrap_error_or_stage{};
}
constexpr auto unwrap_error_or_complete()
{
    return detail::stages::unwrap_error_or_complete_stage{};
}
} // inline namespace stages

} // namespace descend
