#pragma once

#include "descend/args.hpp"
#include "descend/finalize.hpp"
#include "descend/helpers.hpp"
#include "descend/iterate.hpp"
#include "descend/stage_styles.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace descend {
namespace detail::stages {

/*
All stages should accept "Input&& input"" to
    * avoid copies
    * deal with reference types

For forwarding of input further:
    (Input&&) input
    std::forward<Input>(input)

Complete-to-complete which do not change input type should call unwrap_input() (see sort)

When stage carries some data, there should be:
1) either 2 versions of make_impl() provided one for & overload and one for &&
 * template <class Input> auto make_impl() &;
 * template <class Input> auto make_impl() &&;
2) or make_impl() should always make copies of data.

That's because stages can be used in higher order stages like map_group_by, which could create
chains from stages several times.
*/

template <class F>
struct transform_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    [[no_unique_address]]
    F f;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = args_invoke_result_t<F, Input>;
        using stage_type = transform_stage;

        [[no_unique_address]]
        F f;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            next.process_incremental(args_invoke(f, (Input&&) input));
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{(F&&)f};
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{f};
    }
};


template <class F>
struct transform_complete_stage
{
    static constexpr auto style = stage_styles::complete_to_complete;

    [[no_unique_address]]
    F f;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = args_invoke_result_t<F, unwrapped_input_t<Input>>;
        using stage_type = transform_complete_stage;

        [[no_unique_address]]
        F f;

        template <class Next>
        constexpr decltype(auto) process_complete(Input&& input, Next&& next)
        {
            return next.process_complete(args_invoke(f, unwrap_input(input)));
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{(F&&)f};
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{f};
    }
};


template <class Pred>
struct filter_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    [[no_unique_address]]
    Pred pred;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = Input;
        using stage_type = filter_stage;

        Pred pred;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            if (args_invoke(pred, std::as_const(input))) {
                next.process_incremental((Input&&) input);
            }
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{(Pred&&) pred};
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{pred};
    }
};


struct take_n_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    std::size_t n;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = Input;
        using stage_type = take_n_stage;

        std::size_t n;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            if (n != 0) {
                next.process_incremental((Input&&) input);
                --n;
            }
        }
        constexpr bool done() const
        {
            return n == 0;
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        return impl<Input>{n};
    }
};


struct max_stage
{
    static constexpr auto style = stage_styles::incremental_to_complete;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = std::optional<std::remove_cvref_t<Input>>;
        using stage_type = max_stage;

        output_type max;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&&)
        {
            if (!max || *max < input) {
                max = (Input&&) input;
            }
        }
        template <class Next>
        constexpr decltype(auto) end(Next&& next)
        {
            return next.process_complete(std::move(max));
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        return impl<Input>{};
    }
};

template <template <class...> class Cont>
struct to_stage
{
    static constexpr auto style = stage_styles::incremental_to_complete;

    template <class Input>
    struct impl
    {
        static constexpr auto make_container(Input&& input)
        {
            return args_invoke([] <class... Args> (Args&&...) {
                using container_type = Cont<finalize_result_t<Args, false>...>; // keep reference wrappers as is
                using value_type = typename container_type::value_type;

                static_assert(std::is_constructible_v<value_type, Args&&...>,
                        "Can't construct value type of a container from input arguments");

                return container_type{};
            }, (Input&&) input);
        }

        using input_type = Input;
        using output_type = decltype(make_container(std::declval<Input>()));
        using stage_type = to_stage;

        output_type out = {};

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&&)
        {
            args_invoke([this] <class... Args> (Args&&... args) {
                using value_type = typename output_type::value_type;
                out.insert(out.end(), value_type{(Args&&) args...});
            }, (Input&&) input);
        }
        template <class Next>
        constexpr decltype(auto) end(Next&& next)
        {
            return next.process_complete(std::move(out));
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        return impl<Input>{};
    }
};


template <class F>
struct for_each_stage
{
    static constexpr auto style = stage_styles::incremental_to_complete;

    [[no_unique_address]]
    F f;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = void;
        using stage_type = for_each_stage;

        [[no_unique_address]]
        F f;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&&)
        {
            args_invoke(f, (Input&&) input);
        }

        template <class Next>
        constexpr void end(Next&&)
        { }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{(F&&)f};
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{f};
    }
};

// Expands tuple-like elements (tuple/pair/array) at the top level.
// If input is args<...>, expands each expandable element inside and flattens the result.
// Examples:
//   tuple<int, double> -> args<int&&, double&&>
//   args<int&, tuple<double, float>&&> -> args<int&, double&&, float&&>
//   args<int, tuple<double, tuple<char, bool>>> -> args<int&&, double&&, tuple<char, bool>&&>
struct expand_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    // Check if type is expandable (has tuple_size, but not args itself)
    template <class T>
    static constexpr bool is_expandable =    !is_specialization_of_v<args, T>
                                          && requires { std::tuple_size<std::remove_cvref_t<T>>::value; };

    // Expand single element to tuple (single element or multiple if tuple-like)
    template <class T>
    static constexpr auto to_tuple(T&& t) noexcept
    {
        if constexpr (is_expandable<T>) {
            // Expandable: tuple/pair/array -> tuple<Args&&...>
            return std::apply([] <class... Args> (Args&&... args) {
                return std::forward_as_tuple((Args&&) args...);
            }, (T&&) t);
        }
        else {
            // Not expandable: wrap in single-element
            return std::forward_as_tuple((T&&) t);
        }
    }

    template <class T>
    static constexpr decltype(auto) expand_to_args(T&& t)
    {
        if constexpr (is_specialization_of_v<args, std::remove_cvref_t<T>>) {
            // Input is args: expand each expandable element inside and flatten
            return args_invoke([] <class... Elems> (Elems&&... elems) {
                return to_args(std::tuple_cat(to_tuple((Elems&&)elems)...));
            }, (T&&) t);
        }
        else if constexpr (is_expandable<T>) {
            // Input is tuple-like (not args): convert to args
            // Can also be done with the code above, but let's avoid extra tuple_cat
            return to_args(to_tuple((T&&) t));
        }
        else {
            // Single non-expandable element: forward as is
            return (T&&) t;
        }
    }

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(expand_to_args(std::declval<Input &&>()));
        using stage_type = expand_stage;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            next.process_incremental(expand_to_args((Input&&) input));
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        return impl<Input>{};
    }
};

struct expand_complete_stage
{
    static constexpr auto style = stage_styles::complete_to_complete;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(expand_stage::expand_to_args(unwrap_input(std::declval<Input &&>())));
        using stage_type = expand_complete_stage;

        template <class Next>
        constexpr decltype(auto) process_complete(Input&& input, Next&& next)
        {
            return next.process_complete(expand_stage::expand_to_args(unwrap_input((Input&&) input)));
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        return impl<Input>{};
    }
};


// zip_result functor should receive all args as const&
template <class F>
struct zip_result_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    [[no_unique_address]]
    F f;

    // Result of calling f with all args will be appended to args.
    // We can't use forward_as_tuple-like type deduction: if f returns value we would have
    // a dangling reference to temporary after return from do_zip_result.
    template <class T>
    static constexpr auto do_zip_result(F& f, T&& t)
    {
        return args_invoke([&f] <class... Args> (Args&&... args) {
            using f_result_type = decltype(std::invoke(f, std::as_const(args)...));
            return detail::args<Args&&..., f_result_type>{(Args&&) args..., std::invoke(f, std::as_const(args)...) };
        }, (T&&) t);
    }

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(do_zip_result(std::declval<F&>(), std::declval<Input>()));
        using stage_type = zip_result_stage;

        [[no_unique_address]]
        F f;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            next.process_incremental(do_zip_result(f, (Input&&) input));
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{(F&&) f};
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{f};
    }
};


template <bool PreserveRvalueRefs>
struct flatten_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    // we have incoming tuple<int, bool&, const char&, std::string&, float>
    // we want to change last type to X
    // so we want to get args<const int&, bool&, const char&, std::string&, X&&> or const X& or X&
    //
    // PreserveRvalueRefs controls safety vs flexibility:
    // false: T&& -> const T& (safe, prevents use after move)
    // true: T&& -> T&& (may be risky, allows moving from multi-iterated args)

    template <std::size_t I, class Tuple, class Sub>
    static constexpr decltype(auto) change_last_arg_impl(Tuple&& tuple, Sub&& sub)
    {
        using decayed_tuple = std::remove_cvref_t<Tuple>;
        using element_type = std::tuple_element_t<I, decayed_tuple>;
        constexpr auto tuple_size = std::tuple_size_v<decayed_tuple>;

        if constexpr (I == tuple_size - 1) {
            // last argument - replace with iterated element
            return (Sub&&) sub;
        }
        else if constexpr (std::is_rvalue_reference_v<element_type>) {
            if constexpr (PreserveRvalueRefs) {
                // flatten_forward(): preserve T&& (allows moving, may be risky with multiple iterations)
                return std::move(std::get<I>(tuple));
            }
            else {
                // flatten(): convert T&& -> const T& (safe, prevents use after move)
                return std::as_const(std::get<I>(tuple));
            }
        }
        else if constexpr (std::is_lvalue_reference_v<element_type>) {
            // preserve lvalue references (both const and non-const are safe)
            return std::get<I>(tuple);
        }
        else {
            // value type -> const reference
            return std::as_const(std::get<I>(tuple));
        }
    }

    template <class Args, class Sub>
    static constexpr auto change_last_arg(Args&& args, Sub&& sub)
    {
        auto&& tuple = ((Args&&)args).as_tuple();

        auto make = [&] <std::size_t I> () -> decltype(auto) {
            return change_last_arg_impl<I>(std::forward<decltype(tuple)>(tuple), (Sub&&) sub);
        };
        return make_args_elementwise(make, make_index_sequence_for_args<Args>{});
    }

    template <class Args>
    static constexpr decltype(auto) get_last_arg_as_range(Args&& args)
    {
        constexpr auto tuple_size = std::tuple_size_v<typename std::remove_cvref_t<Args>::tuple_type>;
        return std::get<tuple_size - 1>((Args&&) args);
    }

    template <class Input>
    struct impl
    {
        using input_type = Input;

        using range_type = decltype(get_last_arg_as_range(std::declval<Input>()));
        using range_value_type = iterate_output_with_unwrap_t<range_type>;

        using output_type = decltype(
                change_last_arg(
                    std::declval<Input &>(), // note Input &, we are going to call change_last_arg in a loop
                    std::declval<range_value_type>()));
        using stage_type = flatten_stage;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            // we are going to "steal" only last element
            // it is okay to forward 'input' and use it after
            auto&& last = get_last_arg_as_range((Input&&) input);

            iterate(
                    std::forward<decltype(last)>(last),
                    [&next] () { return next.done(); },
                    [&input, &next] <class Elem> (Elem&& elem) {
                        // Here we use input, not (Input&&) input
                        // It does not matter much since we are going to check types inside args
                        // But we are calling this in the loop, so let's not forward 'input' several times
                        next.process_incremental(change_last_arg(input, (Elem&&) elem));
                    });
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        return impl<Input>{};
    }
};


template <std::size_t... Is>
struct swizzle_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    static_assert(sizeof...(Is) != 0,
            "At least one index should be provided for swizzle stage");

    static constexpr bool check_distinct()
    {
        std::size_t indices[] = {Is...};
        const auto b = std::begin(indices),
                   e = std::end  (indices);
        std::sort(b, e);
        return std::adjacent_find(b, e) == e;
    }
    static_assert(check_distinct(), "Indices have duplicate elements in swizzle stage");

    template <class Args>
    static constexpr auto swizzle(Args&& args)
    {
        const auto make = [&] <std::size_t I> () -> decltype(auto) {
            return std::get<I>(((Args&&) args).as_tuple());
        };
        return make_args_elementwise(make, std::index_sequence<Is...>{});
    }

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(swizzle(std::declval<Input>()));
        using stage_type = swizzle_stage;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            next.process_incremental(swizzle((Input&&) input));
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        using decayed_input = std::remove_cvref_t<Input>;
        static_assert(is_specialization_of_v<args, decayed_input>,
                "swizzle stage works with args<> only, check previous stage's output");

        constexpr auto args_count = std::tuple_size_v<typename decayed_input::tuple_type>;
        static_assert(((Is < args_count) && ...),
                "All indices in swizzle stage should be less than number of arguments from the previous stage");

        return impl<Input>{};
    }
};


template <std::size_t Index, class F>
struct transform_arg_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    [[no_unique_address]]
    F f;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(transform_one_arg<Index>(std::declval<Input>(), std::declval<F&>()));
        using stage_type = transform_arg_stage;

        [[no_unique_address]]
        F f;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            next.process_incremental(transform_one_arg<Index>((Input&&) input, f));
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        static_assert(is_specialization_of_v<args, std::remove_cvref_t<Input>>,
                "transform_arg stage requires multi-arg input(args<...>)");
        return impl<Input>{(F&&)f};
    }
    template <class Input>
    constexpr auto make_impl() &
    {
        static_assert(is_specialization_of_v<args, std::remove_cvref_t<Input>>,
                "transform_arg stage requires multi-arg input(args<...>)");
        return impl<Input>{f};
    }
};


// unwrap_optional stage would break the computation and return std::nullopt
// if one of the optional processed in process_incremental() was empty.
//
// Alternative is to just filter out empty optionals, but it could be done explicitly with filter()
struct unwrap_optional_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    template <class T, class Callable>
    static constexpr auto wrap_result_with_optional_if_needed(
            const std::optional<T>& carrier,
            Callable&& callable)
    {
        using callable_result_type = std::remove_cvref_t<std::invoke_result_t<Callable&&>>;
        if constexpr (std::is_same_v<callable_result_type, void>) {
            return carrier.has_value()
                ? std::invoke((Callable&&) callable)
                : [] () {} (); // immediately called lambda returning void
        } else if constexpr (is_specialization_of_v<std::optional, callable_result_type>) {
            // it is already optional, no need to wrap
            return carrier.has_value()
                ? std::invoke((Callable&&) callable)
                : callable_result_type{};
        }
        else {
            using optional_type = decltype(std::make_optional(std::invoke((Callable&&) callable)));
            return carrier.has_value()
                ? std::make_optional(std::invoke((Callable&&) callable))
                : optional_type{};
        }
    }


    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(*std::declval<Input>());
        using stage_type = unwrap_optional_stage;

        std::optional<unit> carrier = std::make_optional(unit{});

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            if (input.has_value()) {
                next.process_incremental(*((Input&&) input));
            }
            else {
                carrier = std::nullopt;
            }
        }
        constexpr bool done() const
        {
            return !carrier.has_value();
        }

        template <class Next>
        constexpr auto end(Next&& next)
        {
            return unwrap_optional_stage::wrap_result_with_optional_if_needed(
                    carrier,
                    [&next] () { return next.end(); });
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        static_assert(is_specialization_of_v<std::optional, std::remove_cvref_t<Input>>,
                "unwrap_optional stage requires std::optional as input");
        return impl<Input>{};
    }
};

struct unwrap_optional_complete_stage
{
    static constexpr auto style = stage_styles::complete_to_complete;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(*unwrap_input(std::declval<Input>()));
        using stage_type = unwrap_optional_complete_stage;

        template <class Next>
        constexpr decltype(auto) process_complete(Input&& input, Next&& next)
        {
            auto&& opt = unwrap_input((Input&&) input);
            return unwrap_optional_stage::wrap_result_with_optional_if_needed(
                    opt,
                    [&opt, &next] () { return next.process_complete(*(std::forward<decltype(opt)>(opt))); });
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        static_assert(is_specialization_of_v<std::optional, std::remove_cvref_t<unwrapped_input_t<Input>>>,
                "unwrap_optional_complete stage requires std::optional as input");
        return impl<Input>{};
    }
};


template <class Comp, bool Stable>
struct sort_stage
{
    static constexpr auto style = stage_styles::complete_to_complete;

    [[no_unique_address]]
    Comp comp;

    template <class Input, class Range>
    static constexpr bool can_sort()
    {
        return requires (Range range, Comp cmp) {
            cmp(*std::begin(range), *std::begin(range)); // add convertible to bool?
            std::begin(range)[0] = *std::begin(range);
        };
    }

    template <class Input>
    struct impl
    {
        [[no_unique_address]]
        Comp comp;

        using input_type = Input;
        using output_type = Input;
        using stage_type = sort_stage;

        template <class Next>
        constexpr decltype(auto) process_complete(Input&& input, Next&& next)
        {
            auto&& range = unwrap_input((Input&&) input);
            static_assert(can_sort<Input, decltype(range)>(),
                    "Can't sort input type, ensure passed range is sortable and use std::ref");

            if constexpr (Stable) {
                std::stable_sort(std::begin(range), std::end(range), comp);
            }
            else {
                std::sort(std::begin(range), std::end(range), comp);
            }
            return next.process_complete((Input&&) input);
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{(Comp&&) comp};
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{comp};
    }
};

template <class Init = unit, class Op = std::plus<>>
struct accumulate_stage
{
    static constexpr auto style = stage_styles::incremental_to_complete;

    [[no_unique_address]]
    Init init;
    [[no_unique_address]]
    Op op;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = std::conditional_t<std::is_same_v<Init, unit>, std::remove_cvref_t<Input>, Init>;
        using stage_type = accumulate_stage;

        [[no_unique_address]]
        output_type init;
        [[no_unique_address]]
        Op op;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&&)
        {
            init = op(std::move(init), (Input&&) input);
        }
        template <class Next>
        constexpr auto end(Next&& next)
        {
            return next.process_complete(std::forward<output_type>(init));
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        if constexpr (std::is_same_v<Init, unit>) {
            return impl<Input>{ {}, (Op&&) op };
        }
        else {
            return impl<Input>{ std::move(init), (Op&&) op };
        }
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        if constexpr (std::is_same_v<Init, unit>) {
            return impl<Input>{ {}, op };
        }
        else {
            return impl<Input>{ init, op };
        }
    }
};

struct count_stage
{
    static constexpr auto style = stage_styles::incremental_to_complete;

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = std::size_t;
        using stage_type = count_stage;

        std::size_t count = 0;

        template <class Next>
        constexpr void process_incremental(Input&&, Next&&)
        {
            ++count;
        }
        template <class Next>
        constexpr auto end(Next&& next)
        {
            // next stage accepts std::size_t&&
            return next.process_complete(std::move(count));
        }
    };
    template <class Input>
    constexpr auto make_impl()
    {
        return impl<Input>{};
    }
};


template <class Index>
struct enumerate_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    Index start;

    template <class Input>
    static constexpr auto add_index(const Index& index, Input&& input)
    {
        return args_invoke([&index] <class... Args> (Args&&... args) {
            return detail::args<const Index&, Args&&...>{ index, (Args&&) args...};
        }, (Input&&) input);
    }

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(add_index(std::declval<Index>(), std::declval<Input>()));
        using stage_type = enumerate_stage;

        Index current;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            next.process_incremental(add_index(current, (Input&&) input));
            ++current;
        }
    };

    template <class Input>
    constexpr auto make_impl()
    {
        return impl<Input>{start};
    }
};

} // namespace detail::stages

inline namespace stages {

template <class F>
constexpr auto transform(F&& f)
{
    return detail::stages::transform_stage<F>{(F&&) f};
}
template <class F>
constexpr auto transform_complete(F&& f)
{
    return detail::stages::transform_complete_stage<F>{(F&&) f};
}
template <class Pred>
constexpr auto filter(Pred&& pred)
{
    return detail::stages::filter_stage<Pred>{(Pred&&) pred};
}
constexpr auto take_n(const std::size_t n)
{
    return detail::stages::take_n_stage{n};
}
constexpr auto max()
{
    return detail::stages::max_stage{};
}
template <template <class...> class Cont>
constexpr auto to()
{
    return detail::stages::to_stage<Cont>{};
};
template <class F>
constexpr auto for_each(F&& f)
{
    return detail::stages::for_each_stage<F>{(F&&) f};
}
constexpr auto expand()
{
    return detail::stages::expand_stage{};
}
constexpr auto expand_complete()
{
    return detail::stages::expand_complete_stage{};
}
template <class F>
constexpr auto zip_result(F&& f)
{
    return detail::stages::zip_result_stage<F>{(F&&) f};
}
constexpr auto flatten()
{
    return detail::stages::flatten_stage<false>{};  // Safe: T&& -> const T&
}
constexpr auto flatten_forward()
{
    return detail::stages::flatten_stage<true>{};   // May be risky: preserves T&&
}
template <std::size_t... Is>
constexpr auto swizzle()
{
    return detail::stages::swizzle_stage<Is...>{};
}
template <std::size_t I, class F>
constexpr auto transform_arg(F&& f)
{
    return detail::stages::transform_arg_stage<I, F>{(F&&) f};
}
constexpr auto unwrap_optional()
{
    return detail::stages::unwrap_optional_stage{};
}
constexpr auto unwrap_optional_complete()
{
    return detail::stages::unwrap_optional_complete_stage{};
}
template <class Comp = std::less<>>
constexpr auto sort(Comp&& comp = {})
{
    return detail::stages::sort_stage<Comp, false>{(Comp&&) comp};
}
template <class Comp = std::less<>>
constexpr auto stable_sort(Comp&& comp = {})
{
    return detail::stages::sort_stage<Comp, true>{(Comp&&) comp};
}
template <class Init = detail::unit, class Op = std::plus<>>
constexpr auto accumulate(Init init = {}, Op&& op = {})
{
    return detail::stages::accumulate_stage<Init, Op>{std::move(init), (Op&&) op};
}
constexpr auto count()
{
    return detail::stages::count_stage{};
}
template <class Index = int>
constexpr auto enumerate(Index start = {})
{
    return detail::stages::enumerate_stage<Index>{std::move(start)};
}

} // namespace stages
} // namespace descend
