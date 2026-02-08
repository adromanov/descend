#pragma once

#include "descend/helpers.hpp"
#include "descend/stage_styles.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace descend {
namespace detail::stages {

// Provide base implementation for different accumulate stages,
// reducing the amount of boilerplate code needed for incremental->complete stages
//
// MakeInit should provide template <class Input> operator () for creating initial value
//
// UpdateOp should provide operator() (Init& init, Input&& input)
// to update init with input
//
// DisplayStage is used for debug printing stage, otherwise all stages would be base_accumulate_stage,
// which is not very informative
template <class DisplayStage, class MakeInit, class UpdateOp>
struct base_accumulate_stage
{
    static constexpr auto style = stage_styles::incremental_to_complete;

    [[no_unique_address]]
    MakeInit make_init;
    [[no_unique_address]]
    UpdateOp update_op;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(std::declval<MakeInit>().template operator () <Input> ());
        using stage_type = base_accumulate_stage; // for processing style
        using display_stage_type = DisplayStage; // for output in debug

        [[no_unique_address]]
        output_type output;

        [[no_unique_address]]
        UpdateOp update_op;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&&)
        {
            update_op(output, (Input&&) input);
        }
        template <class Next>
        constexpr auto end(Next&& next)
        {
            return next.process_complete(std::forward<output_type>(output));
        }
    };

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{ make_init.template operator () <Input> (), update_op };
    }

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{ ((MakeInit &&) make_init).template operator () <Input> (), (UpdateOp&&) update_op };
    }
};

template <class DisplayStage, class MakeInit, class UpdateOp>
constexpr auto make_base_accumulate_stage(MakeInit&& make_init, UpdateOp&& update_op)
{
    return base_accumulate_stage<DisplayStage, MakeInit, UpdateOp>{
            (MakeInit&&) make_init, (UpdateOp&&) update_op};
}

} // namespace detail::stages


inline namespace stages {

template <class Init = detail::unit, class Op = std::plus<>>
constexpr auto accumulate(Init init = {}, Op&& op = {})
{
    auto make_init = [init = std::move(init)] <class Input> ()
    {
        (void) init;
        if constexpr (std::is_same_v<Init, detail::unit>) {
            return std::remove_cvref_t<Input>{}; // if no init is passed - use default constructed decayed Input as init
        }
        else {
            // Always copy, but if references are needed - std::reference_wrapper<> can be used
            return init;
        }
    };

    auto update_op = [op = (Op&&) op] <class Accum, class Input> (Accum& accum, Input&& input)
    {
        accum = op(std::move(accum), (Input&&) input);
    };

    return detail::stages::make_base_accumulate_stage<struct accumulate_stage>(
            std::move(make_init), std::move(update_op));
}

// outputs minimum of all processed elements asd std::optional<std::remove_cvref_t<Input>>
// uses Compare for comparison
template <class Compare = std::less<>>
constexpr auto min(Compare&& compare = {})
{
    auto make_init = [] <class Input> ()
    {
        return std::optional<std::remove_cvref_t<Input>>{};
    };

    auto update_op = [compare = (Compare&&) compare] <class MaybeMin, class Input> (MaybeMin& min, Input&& input)
    {
        if (!min.has_value() || compare(std::as_const(input), std::as_const(*min))) {
            min = (Input&&) input;
        }
    };

    return detail::stages::make_base_accumulate_stage<struct min_stage>(
            std::move(make_init), std::move(update_op));
}

// outputs maximum of all processed elements asd std::optional<std::remove_cvref_t<Input>>
// uses Compare for comparison
template <class Compare = std::less<>>
constexpr auto max(Compare&& compare = {})
{
    auto make_init = [] <class Input> ()
    {
        return std::optional<std::remove_cvref_t<Input>>{};
    };
    auto update_op = [compare = (Compare&&) compare] <class MaybeMax, class Input> (MaybeMax& max, Input&& input)
    {
        if (!max.has_value() || compare(std::as_const(*max), std::as_const(input))) {
            max = (Input&&) input;
        }
    };

    return detail::stages::make_base_accumulate_stage<struct max_stage>(
            std::move(make_init), std::move(update_op));
}

// Outputs std::optional of struct with 2 members: min and max, both being std::remove_cvref<Input>
// uses Compare for comparison
template <class Compare = std::less<>>
constexpr auto min_max(Compare&& compare = {})
{
    auto make_init = [] <class Input> ()
    {
        using decayed_input = std::remove_cvref_t<Input>;
        struct min_max { decayed_input min, max; };
        return std::optional<min_max>{};
    };

    auto update_op = [compare = (Compare&&) compare] <class MaybeMinMax, class Input> (MaybeMinMax& mm, Input&& input)
    {
        if (!mm.has_value()) {
            // since (Input&&) is only a cast, we don't care about argument evaluation order
            mm.emplace(input, (Input&&) input);
        }
        else {
            const bool should_update_min = compare(std::as_const(input), std::as_const(mm->min));
            const bool should_update_max = compare(std::as_const(mm->max), std::as_const(input));

            // in this case there would be no benefits of using forwarding
            constexpr bool should_use_copy =    std::is_trivially_copy_constructible_v<std::remove_cvref_t<Input>>
                                             || std::is_lvalue_reference_v<Input>;
            if constexpr (should_use_copy) {
                if (should_update_min) {
                    mm->min = input;
                }
                if (should_update_max) {
                    mm->max = input;
                }
            }
            else { // let's use forward to move construct if we can
                // LessThanComparable requires that if (a < b) => !(b < a), but who knows what was passed as comparator
                if (should_update_min && should_update_max) [[unlikely]] {
                    mm->min = input;
                    mm->max = (Input&&) input;
                }
                else if (should_update_min) {
                    mm->min = (Input&&) input;
                }
                else if (should_update_max) {
                    mm->max = (Input&&) input;
                }
            }
        }
    };

    return detail::stages::make_base_accumulate_stage<struct min_max_stage>(
            std::move(make_init), std::move(update_op));
}

// outputs the number of elements received as std::size_t
constexpr auto count()
{
    return detail::stages::make_base_accumulate_stage<struct count_stage>(
            [] <class> () { return std::size_t{0}; },
            [] (auto& count, auto&&) { ++count; });
}

} // namespace stages
} // namespace descend
