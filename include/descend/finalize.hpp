#pragma once

#include "descend/args.hpp"
#include "descend/helpers.hpp"

#include <tuple>
#include <type_traits>
#include <utility>

namespace descend::detail {

// All chain results go through finalize() before being returned to the user.
//
// Purpose: Provide value semantics by default (similar to std::make_pair()/std::make_tuple()),
// while allowing opt-in for references via std::reference_wrapper<T>.
//
// Rationale:
// - References in pipeline results are potentially dangerous (may dangle after pipeline ends)
// - Value semantics by default is safer and matches standard library conventions
// - Users who need references can explicitly use std::reference_wrapper<T>
//
// Behavior:
// 1. For tuple-like types (args<>, tuple<>, pair<>):
//    - ALL references are converted to values: T&, const T&, T&& → T
//    - std::reference_wrapper<T> is unwrapped to T& (and contents are NOT converted)
//    - Recursively applied to nested tuple-like types
//
// 2. For non-tuple types:
//    - Converted to value type (return type is 'auto', not 'decltype(auto)')
//    - std::reference_wrapper at top level is preserved as-is
//
// Examples (from tests/test_finalize.cpp):
// - args<int, bool&, const double&, float&&>           → args<int, bool, double, float>
// - tuple<int&, const double&&>                        → tuple<int, double>
// - args<std::reference_wrapper<int>>                  → args<int&>
// - args<std::reference_wrapper<pair<int&, double&&>>> → args<pair<int&, double&&>&>  (inner references preserved!)
// - std::vector<int>&&                                 → std::vector<int>
// - std::reference_wrapper<int>                        → std::reference_wrapper<int>  (top-level preserved)
//
// TODO: add customization points? add tags to customize behavior?

template <class Decayed, bool UnwrapRefWrappers>
struct finalize_result_helper
{
    using type = Decayed;
};

template <class T, bool UnwrapRefWrappers>
using finalize_result_t = typename finalize_result_helper<std::remove_cvref_t<T>, UnwrapRefWrappers>::type;

template <class T>
struct finalize_result_helper<std::reference_wrapper<T>, true>
{
    using type = T &;
};

template <class F, class S, bool UnwrapRefWrappers>
struct finalize_result_helper<std::pair<F, S>, UnwrapRefWrappers>
{
    using type = std::pair<
        finalize_result_t<F, UnwrapRefWrappers>,
        finalize_result_t<S, UnwrapRefWrappers>>;
};

template <class... Ts, bool UnwrapRefWrappers>
struct finalize_result_helper<std::tuple<Ts...>, UnwrapRefWrappers>
{
    using type = std::tuple<finalize_result_t<Ts, UnwrapRefWrappers>...>;
};
template <class... Ts, bool UnwrapRefWrappers>
struct finalize_result_helper<args<Ts...>, UnwrapRefWrappers>
{
    using type = args<finalize_result_t<Ts, UnwrapRefWrappers>...>;
};


template <class Output>
constexpr auto finalize(Output&& output)
{
    using decayed_output_type = std::remove_cvref_t<Output>;

    // Check if result is a tuple-like type that may contain nested rvalue references
    constexpr bool is_args_or_pair_or_tuple =    is_specialization_of_v<args      , decayed_output_type>
                                              || is_specialization_of_v<std::tuple, decayed_output_type>
                                              || is_specialization_of_v<std::pair , decayed_output_type>;

    if constexpr (is_args_or_pair_or_tuple) {
        // Path 1: Tuple-like types
        // - Convert all references (T&, const T&, T&&) to values (T)
        // - Unwrap std::reference_wrapper<T> to T& (without converting inner references)
        // - Recursively applied to nested tuple-like types

        // Helper: Convert args to tuple, or pass tuple/pair through unchanged
        constexpr auto args_to_tuple = [] <class T> (T&& t) -> decltype(auto) {
            if constexpr (is_specialization_of_v<args, std::remove_cvref_t<T>>) {
                return ((T&&) t).as_tuple();
            }
            else {
                return (T&&) t;
            }
        };

        using result_type = finalize_result_t<Output, true>; // unwrap ref wrappers = true

        // Reconstruct the tuple-like type with converted element types
        // Note: We DON'T use args_invoke here because:
        // - args_invoke on const args<>& would force everything to const T&
        // - Converting to const& would prevent moving from internal rvalue references
        return std::apply([] <class... Args> (Args&&... args) {
            return result_type{(Args&&) args...};
        }, args_to_tuple((Output&&) output));
    }
    else {
        // Path 2: Non-tuple types
        // Converts to value-type (because return type is 'auto', not 'decltype(auto)')
        // std::reference_wrapper at top level is preserved as-is
        return (Output&&) output;
    }
}

// See tests/test_finalize.cpp for complete examples of type conversions

} // namespace descend::detail
