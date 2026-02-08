#pragma once

#include "descend/args.hpp"
#include "descend/chain.hpp"
#include "descend/iterate.hpp"

#include <optional>
#include <type_traits>
#include <utility>

namespace descend {
namespace detail::stages {

template <class... Subchains>
struct tee_stage
{
    static constexpr auto style = stage_styles::incremental_to_complete;

    std::tuple<Subchains...> subchains;

    template <class Input, class SubchainsTuple>
    static constexpr auto make_chains_tuple(SubchainsTuple&& subchains_tuple)
    {
        using input_cref_type = const std::remove_cvref_t<Input>&;

        return std::apply([] <class... Subs> (Subs&&... subs) {
            return std::make_tuple(make_subchain_for_input<input_cref_type>((Subs&&)subs)...);
        }, (SubchainsTuple&&) subchains_tuple);
    }


    template <class Input>
    struct impl
    {
        using chains_tuple = decltype(make_chains_tuple<Input>(std::declval<std::tuple<Subchains...>>()));
        chains_tuple chains;

        static constexpr auto make_result_tuple(chains_tuple& chains)
        {
            return std::apply([] (auto&&... chain) {
                return std::make_tuple(chain.get(detail::index<0>{}).end()...);
            }, chains);
        }

        using input_type = Input;
        using output_type = decltype(make_result_tuple(std::declval<chains_tuple&>()));
        using stage_type = tee_stage;
        using display_stage_type = struct tee_stage_; // display type would be just 'tee_stage_'
                                                      // instead of 'tee_stage<lots of other types...>'

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&&)
        {
            std::apply([&input] (auto&&... chain) {
                (chain.get(detail::index<0>{}).process_incremental(std::as_const(input)), ...);
            }, chains);
        }

        template <class Next>
        constexpr auto end(Next&& next)
        {
            return next.process_complete(make_result_tuple(chains));
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{make_chains_tuple<Input>(std::move(subchains))};
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{make_chains_tuple<Input>(subchains)};
    }
};

// processes elements one by one, extracting aggregation key with KeyGetter
// creates Map from key type to the processing chain
//
// processing an element results in processing the element by the chain,
// corresponding to it's key
//
// after all elements processed passes key + chain.end() to the next element in the whole computation
template <class Key, class ChainResult>
struct prepend_key_to_args
{
    static_assert(!is_specialization_of_v<args, std::remove_cvref_t<ChainResult>>);
    using type = args<Key, ChainResult &&>;
};
template <class Key, class... Args>
struct prepend_key_to_args<Key, args<Args...>>
{
    using type = args<Key, Args...>;
};
template <class Key, class ChainResult>
using prepend_key_to_args_t = typename prepend_key_to_args<Key, ChainResult>::type;

template <template <class...> class Map, class KeyGetter, class ComposedChain>
struct map_group_by_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    [[no_unique_address]]
    KeyGetter key_getter;
    [[no_unique_address]]
    ComposedChain composed_chain;

    template <class Input>
    struct impl
    {
        [[no_unique_address]]
        KeyGetter key_getter;

        [[no_unique_address]]
        ComposedChain composed_chain;

        // The type of chain we would get after decomposing ComposedChain and
        // chaining all parts together to process InputType
        using chain_type = decltype(make_subchain_for_input<Input &&>(std::declval<ComposedChain&>()));

        // aggregation key: result of invoking KeyGetter with const Input&
        using key_type = std::remove_cvref_t<std::invoke_result_t<KeyGetter&, const std::remove_cvref_t<Input>&>>;

        // result of calling chain.end()
        using chain_result_type = std::remove_cvref_t<subchain_end_t<chain_type>>;

        // key to chain map
        using map_type = Map<key_type, chain_type>;

        // we need to understand whether the key be non-const when we iterate through moved map_type
        using iterate_output_type = iterate_output_t<map_type &&>;
        // in order to provide right args<> as output
        using key_ref_type = typename std::remove_reference_t<iterate_output_type>::first_type &&;

        using input_type = Input;
        using output_type = prepend_key_to_args_t<key_ref_type, chain_result_type>;
        using stage_type = map_group_by_stage;
        using display_stage_type = struct map_group_by_stage_; // display name would be just 'map_group_by_stage_'
                                                               // instead of 'map_group_by_stage<lots of other types...>'

        Map<key_type, chain_type> chains = {};

        struct chain_maker
        {
            ComposedChain& composed_chain;
            operator chain_type () &&
            {
                return make_subchain_for_input<Input &&>(composed_chain);
            }
        };

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&&)
        {
            auto&& key = std::invoke(key_getter, std::as_const(input));

            auto [it, _] = chains.try_emplace(std::forward<decltype(key)>(key), chain_maker{composed_chain});
            it->second.get(detail::index<0>{}).process_incremental((Input&&) input);
        }

        template <class Next>
        constexpr auto end(Next&& next)
        {
            auto process_element = [&next] <class Elem> (Elem&& elem) {
                // elem is from chains map, it is a pair of key and chain.
                // we need to separate key from chain, call chain.end() and pass args<key, result> further
                std::apply([&next] <class Key, class Chain> (Key&& key, Chain&& chain) {
                    // chain.end() can be args<>, we would like to separate it as well
                    args_invoke([&] <class... Args> (Args&&... args) {
                        // pass key and args to next as args<Key&&, Args&&...>
                        next.process_incremental({(Key&&) key, (Args&&) args...});
                    }, chain.get(detail::index<0>{}).end());
                }, (Elem&&) elem);
            };

            detail::iterate(
                    std::move(chains),
                    [&next] () { return next.done(); },
                    std::move(process_element));
            return next.end();
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{ (KeyGetter&&) key_getter, (ComposedChain&&) composed_chain };
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{ key_getter, composed_chain };
    }
};

// Consecutive grouping stage - groups consecutive elements with the same key.
// Unlike map_group_by which collects all elements globally, group_by emits
// groups as they complete (when the key changes).
//
// Example:
//   Input: [1, 1, 2, 2, 2, 1, 3, 3]
//   With key = identity, groups emitted:
//     args<1, result_for_[1,1]>
//     args<2, result_for_[2,2,2]>
//     args<1, result_for_[1]>       <- 1 appears again as new consecutive run
//     args<3, result_for_[3,3]>
template <class KeyGetter, class ComposedChain>
struct group_by_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    [[no_unique_address]]
    KeyGetter key_getter;
    [[no_unique_address]]
    ComposedChain composed_chain;

    template <class Input>
    struct impl
    {
        [[no_unique_address]]
        KeyGetter key_getter;

        [[no_unique_address]]
        ComposedChain composed_chain;

        // The type of chain we would get after decomposing ComposedChain
        using chain_type = decltype(make_subchain_for_input<Input &&>(std::declval<ComposedChain&>()));

        // aggregation key: result of invoking KeyGetter with const Input&
        using key_type = std::remove_cvref_t<std::invoke_result_t<KeyGetter&, const std::remove_cvref_t<Input>&>>;

        // result of calling chain.end()
        using chain_result_type = std::remove_cvref_t<subchain_end_t<chain_type>>;

        using input_type = Input;
        using output_type = prepend_key_to_args_t<key_type&&, chain_result_type>;
        using stage_type = group_by_stage;
        using display_stage_type = struct group_by_stage_; // display name

        // Current group state: key and its processing chain
        // std::nullopt means no group started yet
        std::optional<std::pair<key_type, chain_type>> current_group = {};

        template <class Next>
        constexpr void emit_current_group(Next&& next)
        {
            if (current_group.has_value()) {
                auto& [key, chain] = *current_group;
                // Call chain.end() and prepend key, like map_group_by does
                args_invoke([&] <class... Args> (Args&&... args) {
                    next.process_incremental({std::move(key), (Args&&) args...});
                }, chain.get(detail::index<0>{}).end());
            }
        }

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            auto&& key = std::invoke(key_getter, std::as_const(input));

            // If we have first element or key is different from the last one
            if (!current_group.has_value() || (current_group->first != key)) {
                // Emit group if not empty
                emit_current_group(next);
                // Create new chain for new group
                current_group.emplace(
                    std::forward<decltype(key)>(key),
                    make_subchain_for_input<Input &&>(composed_chain)
                );
            }

            // Process element in current group's chain
            current_group->second.get(detail::index<0>{}).process_incremental((Input&&) input);
        }

        template <class Next>
        constexpr auto end(Next&& next)
        {
            // Emit final group if any
            emit_current_group(next);
            return next.end();
        }
    };

    template <class Input>
    constexpr auto make_impl() &&
    {
        return impl<Input>{ (KeyGetter&&) key_getter, (ComposedChain&&) composed_chain };
    }

    template <class Input>
    constexpr auto make_impl() &
    {
        return impl<Input>{ key_getter, composed_chain };
    }
};

} // namespace detail::stages

inline namespace stages {
template <class... Subchains>
constexpr auto tee(Subchains&&... subchains)
{
    const auto make_stage = [&] <class... Ts> (std::tuple<Ts...>&& tuple) {
        return detail::stages::tee_stage<Ts...>{std::move(tuple)};
    };
    return make_stage(std::make_tuple((Subchains&&) subchains...));
}

template <template <class...> class Map, class KeyGetter, class... Stages>
constexpr auto map_group_by(KeyGetter&& key_getter, Stages&&... stages)
{
    auto composed_chain = descend::compose((Stages&&) stages...);
    using composed_chain_type = decltype(composed_chain);
    return detail::stages::map_group_by_stage<Map, KeyGetter, composed_chain_type>{
        (KeyGetter&&) key_getter, std::move(composed_chain) };
}

template <class KeyGetter, class... Stages>
constexpr auto group_by(KeyGetter&& key_getter, Stages&&... stages)
{
    auto composed_chain = descend::compose((Stages&&) stages...);
    using composed_chain_type = decltype(composed_chain);
    return detail::stages::group_by_stage<KeyGetter, composed_chain_type>{
        (KeyGetter&&) key_getter, std::move(composed_chain) };
}

} // inline namespace stages

} // namespace descend
