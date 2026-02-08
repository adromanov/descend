#pragma once

#include "descend/args.hpp"
#include "descend/stage_styles.hpp"

#include <type_traits>
#include <utility>

namespace descend {
namespace detail::stages {

// Provide base implementation for different transform stages,
// reducing the amount of boilerplate code needed for incremental->incremental stages
//
// Automatically unwraps args<> in case multi-argument input.
//
// DisplayStage is used for debug printing stage, otherwise all stages would be base_transform_stage,
// which is not very informative
template <class DisplayStage, class Transform>
struct base_transform_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    [[no_unique_address]]
    Transform transform;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = args_invoke_result_t<Transform &, Input &&>;
        using stage_type = base_transform_stage;
        using display_stage_type = DisplayStage; // for output in debug

        [[no_unique_address]]
        Transform transform;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            next.process_incremental(args_invoke(transform, (Input&&) input));
        }
    };

    template <class Input>
    constexpr auto make_impl() &
    {
        static_assert(is_args_invocable_v<Transform &, Input&&>,
                "transform operation cannot be called with input type. Different number of arguments? Different type? Different reference category?");
        return impl<Input>{ transform };
    }

    template <class Input>
    constexpr auto make_impl() &&
    {
        static_assert(is_args_invocable_v<Transform &, Input&&>,
                "transform operation cannot be called with input type. Different number of arguments? Different type? Different reference category?");
        return impl<Input>{ (Transform&&) transform };
    }
};

template <class DisplayStage, class Transform>
constexpr auto make_base_transform_stage(Transform&& transform)
{
    return base_transform_stage<DisplayStage, Transform>{ (Transform&&) transform };
}

// These will only be used for displaying in case of debug output
template <class Transform>
struct transform_stage {};

template <class T>
struct construct_stage {};

} // namespace detail::stages


inline namespace stages {

template <class Transform>
constexpr auto transform(Transform&& transform)
{
    using display_stage = detail::stages::transform_stage<Transform>; // for displaying stage name during debug print
    return detail::stages::make_base_transform_stage<display_stage>((Transform&&) transform);
}

template <class T>
constexpr auto construct()
{
    using display_stage = detail::stages::construct_stage<T>; // for displaying stage name during debug print
    return detail::stages::make_base_transform_stage<display_stage>([] (auto&&... args) {
        static_assert(std::is_constructible_v<T, decltype(args)&&...>,
                "can't construct T from arguments in construct stage");
        return T(std::forward<decltype(args)>(args)...);
    });
}

constexpr auto make_pair()
{
    return detail::stages::make_base_transform_stage<struct make_pair_stage>([] (auto&& f, auto&& s) {
        return std::make_pair(std::forward<decltype(f)>(f), std::forward<decltype(s)>(s));
    });
}

constexpr auto make_tuple()
{
    return detail::stages::make_base_transform_stage<struct make_tuple_stage>([] (auto&&... args) {
        return std::make_tuple(std::forward<decltype(args)>(args)...);
    });
}

} // namespace stages
} // namespace descend
