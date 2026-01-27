#pragma once

#include "descend/generator.hpp"
#include "descend/helpers.hpp"

#include <unordered_map>
#include <unordered_set>
#include <type_traits>

namespace descend::detail {

/*
To be able to return inputs by reference we need to pass std::reference_wrapper to further stages,
so after the last one we can return it as-is by value.
By default, all inputs except generators are considered const

So, unwrapping input would do:
x            -> range should be const X&
std::ref(x)  -> range should be X&
std::move(x) -> range should be X&&
*/
template <class Input, class DecayedInput = std::remove_cvref_t<Input>>
constexpr decltype(auto) unwrap_input(Input&& input) noexcept
{
    if constexpr (is_specialization_of_v<std::reference_wrapper, DecayedInput>) {
        // unwrap reference wrappers
        return input.get();
    }
    else if constexpr (is_specialization_of_v<generator, DecayedInput> || std::is_rvalue_reference_v<Input&&>) {
        // forward generators and rvalue references as is
        return (Input&&) input;
    }
    else {
        // by default add const
        return std::as_const(input);
    }
}
template <class Input>
using unwrapped_input_t = decltype(unwrap_input(std::declval<Input>()));



template <class T>
struct iterate_output
{ };

template <class T>
    requires is_specialization_of_v<generator, std::remove_cvref_t<T>>
struct iterate_output<T>
{
    using type = typename std::remove_cvref_t<T>::output_type;
};

template <class T>
consteval bool has_begin_end() noexcept
{
    using std::begin;
    using std::end;
    return requires (T t) { begin(t) != end(t); };
}
template <class T>
    requires (has_begin_end<T>())
struct iterate_output<T>
{
    consteval static auto value_type_helper() noexcept
    {
        using range = std::remove_cvref_t<T>;
        if constexpr (requires { typename range::value_type; }) {

            constexpr bool is_rvalue_ref =    std::is_rvalue_reference_v<T>
                                           && !std::is_const_v<std::remove_reference_t<T>>;

            constexpr bool is_unordered_map =    is_specialization_of_v<std::unordered_map, range>
                                              || is_specialization_of_v<std::unordered_multimap, range>;
            constexpr bool is_unordered_set =    is_specialization_of_v<std::unordered_set, range>
                                              || is_specialization_of_v<std::unordered_multiset, range>;

            // Special treatment for rvalue ref to std::unordered_* containers
            // since we are going to extract nodes out of it and make key\value non-const
            if constexpr (is_rvalue_ref && is_unordered_map) {
                return
                    std::type_identity<
                        std::pair<
                            typename range::key_type&&,
                            typename range::mapped_type&&
                        >&&
                    >{};
            }
            else if constexpr (is_rvalue_ref && is_unordered_set) {
                return std::type_identity<typename range::value_type&&>{};
            }
            else {
                using value_type = typename range::value_type;
                return std::type_identity<decltype(forward_like<T>(std::declval<value_type>()))>{};
            }
        }
        else if constexpr (std::is_array_v<range>) {
            return std::type_identity<decltype(forward_like<T>(std::declval<range>()[0]))>{};
        }
    }

    using type = typename decltype(value_type_helper())::type;
};

template <class T>
using iterate_output_t = typename iterate_output<T>::type;

template <class T>
inline constexpr bool can_iterate_over_v = requires { typename iterate_output_t<T>; };

template <class T>
inline constexpr bool can_iterate_over_with_unwrap_v = can_iterate_over_v<unwrapped_input_t<T>>;

template <class T>
using iterate_output_with_unwrap_t = iterate_output_t<unwrapped_input_t<T>>;



template <class K, class V, class Done, class Callback>
constexpr void iterate_like(std::unordered_map<K, V>&& range, Done&& done, Callback&& callback)
{
    while (!done() && !range.empty()) {
        auto handle = range.extract(range.begin());
        callback(std::pair<K &&, V&&>{std::move(handle.key()), std::move(handle.mapped())});
    }
}

template <class K, class V, class Done, class Callback>
constexpr void iterate_like(std::unordered_set<K, V>&& range, Done&& done, Callback&& callback)
{
    while (!done() && !range.empty()) {
        auto handle = range.extract(range.begin());
        callback(std::move(handle.value()));
    }
}

template <class Range, class Done, class Callback>
constexpr void iterate_like(Range&& range, Done&& done, Callback&& callback)
{
    if (!done()) {
        using std::begin;
        using std::end;
        auto b = begin(range);
        auto e = end(range);
        for (; b != e; ++b) {
            callback(forward_like<Range>(*b));
            if (done()) {
                break;
            }
        }
    }
}

template <class Gen, class Done, class Callback>
constexpr void iterate_generator(Gen&& gen, Done&& done, Callback&& callback)
{
    using output_type = typename std::remove_cvref_t<Gen>::output_type;
    // stage should support incremental
    while (!done()) {
        const bool should_continue =
            gen.f([&callback] (output_type x) {
                callback(static_cast<output_type>(x));
            });
        if (!should_continue) {
            break;
        }
    }
}


template <class Input, class Done, class Callback>
constexpr void iterate(Input&& input, Done&& done, Callback&& callback)
{
    static_assert(can_iterate_over_with_unwrap_v<Input&&>,
            "Can't iterate over input");
    auto&& range = unwrap_input((Input&&) input);
    using range_type = decltype(range);

    if constexpr (is_specialization_of_v<generator, std::remove_cvref_t<range_type>>) {
        iterate_generator((range_type&&) range, (Done&&) done, (Callback&&) callback);
    }
    else {
        iterate_like((range_type&&) range, (Done&&) done, (Callback&&) callback);
    }
}

} // namespace descend::detail
