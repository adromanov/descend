#pragma once

#include "descend/compose.hpp"
#include "descend/finalize.hpp"
#include "descend/generator.hpp"
#include "descend/helpers.hpp"
#include "descend/iterate.hpp"
#include "descend/stage_styles.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace descend {

namespace detail {

struct void_sink
{
    void process_incremental(auto&&, auto&&) {}
    void process_complete(auto&&, auto&&) {}
    void end(auto&&) {}
};


template <std::size_t I>
struct index : std::integral_constant<std::size_t, I>{};


template <class T, class Tuple>
struct prepend_tuple_helper;
template <class T, class... Ts>
struct prepend_tuple_helper<T, std::tuple<Ts...>>
{
    using type = std::tuple<T, Ts...>;
};
template <class T, class Tuple>
using prepend_tuple_t = typename prepend_tuple_helper<T, Tuple>::type;



// Wraps Stage
template <class Chain, std::size_t I, class StageImpl>
struct stage_chain_component
{
    using stage_impl_type = StageImpl;
    using stage_type = typename StageImpl::stage_type;
    using input_type = typename StageImpl::input_type;
    using output_type = typename StageImpl::output_type;

    constexpr decltype(auto) get(index<I>)
    { return *this; }
    constexpr decltype(auto) get(index<I>) const
    { return *this; }

    constexpr auto& get_chain()
    { return static_cast<Chain&>(*this); }

    constexpr auto& get_chain() const
    { return static_cast<const Chain&>(*this); }

    constexpr decltype(auto) next()
    { return get_chain().get(index<I + 1>{}); }

    constexpr decltype(auto) next() const
    { return get_chain().get(index<I + 1>{}); }

    constexpr decltype(auto) end()
    {
        if constexpr (requires { m_stage_impl.end(next()); } ) {
            return m_stage_impl.end(next());
        }
        else {
            return next().end();
        }
    }

    constexpr bool done() const
    {
        if constexpr (requires { m_stage_impl.done(); }) {
            return m_stage_impl.done();
        }
        else {
            // In the video they have 'return false' here
            // I don't think it is right: if current stage has no 'done' method
            // it should delegate this desicion to next stages.
            // Next stage could be take(5) and it is already got all elements it needs
            return next().done();
        }
    }

    // incremental -> incremental
    constexpr void process_incremental(input_type&& input)
        requires (has_incremental_input<stage_type>())
    {
        m_stage_impl.process_incremental((input_type&&) input, next());
    }

    // complete -> complete
    constexpr decltype(auto) process_complete(input_type&& input)
        requires (has_complete_input<stage_type>())
    {
        return m_stage_impl.process_complete((input_type&&) input, next());
    }

    // complete -> incremental
    template <class Input>
    constexpr decltype(auto) process_complete(Input&& input)
        requires (has_incremental_input<stage_type>())
    {
        detail::iterate(
                (Input&&) input,
                [this] () { return done(); },
                [this] (input_type&& elem) {
                    m_stage_impl.process_incremental((input_type&&) elem, next());
                });
        return end();
    }

    StageImpl m_stage_impl;
};


template <class IndexSequence, class... StageImpls>
struct chain;

template <std::size_t... Is, class... StageImpls>
struct chain<std::index_sequence<Is...>, StageImpls...>
    : stage_chain_component<chain<std::index_sequence<Is...>, StageImpls...>, Is, StageImpls>...
{
    using stage_impls = std::tuple<StageImpls...>;

    using stage_chain_component<chain, Is, StageImpls>::get...;

    template <class Output>
    constexpr decltype(auto) process_complete(Output&& output)
    {
        return finalize((Output&&) output);
    }

    constexpr auto& get(index<sizeof...(Is)>)
    { return *this; }
    constexpr auto& get(index<sizeof...(Is)>) const
    { return *this; }

    constexpr auto& get_chain()
    { return *this; }
    constexpr auto& get_chain() const
    { return *this; }

    constexpr bool done() const
    { return false; }

    template <class T = int>
    constexpr void end()
    {
        static_assert(detail::always_false_v<T>,
                "Last stage must be complete stage, not incremental");
    }
};


template <class PrevStage, class PrevStageImpl, class Stage>
struct stage_impl_helper
{
    using prev_output_type = typename PrevStageImpl::output_type;
    // here we need to check stage processing styles to check whether we can connect

    static_assert(!(has_incremental_output<PrevStage>() && has_complete_input<Stage>()),
            "Can't connect incremental output to complete input");

    // complete to incremental should be possible for ranges and generators only
    static_assert(!(has_complete_output<PrevStage>() && has_incremental_input<Stage>() && !can_iterate_over_with_unwrap_v<prev_output_type>),
            "Can't connect complete output with incremental input: can't iterate over output");

    // TODO: do we need it? is it possible we want to process generator as complete?
    static_assert(!(   is_specialization_of_v<generator, std::remove_cvref_t<typename PrevStageImpl::output_type>>
                    && has_complete_input<Stage>()),
            "Can't connect generator to complete stage");

    static constexpr auto stage_input_type_helper() noexcept
    {
        if constexpr (has_complete_input<Stage>() || has_incremental_output<PrevStage>()) {
            return std::type_identity<prev_output_type>{};
        }
        else if constexpr (has_incremental_input<Stage>()) {
            return std::type_identity<iterate_output_with_unwrap_t<prev_output_type>>{};
        }
    }
    using stage_input_type = typename decltype(stage_input_type_helper())::type;

    using stage_impl = decltype(std::declval<Stage>().template make_impl<stage_input_type>());
};


template <class Range, class RangeImpl, class... Stages>
struct stage_impl_combiner;

template <class PrevStage, class PrevStageImpl, class Stage, class... RestStages>
struct stage_impl_combiner<PrevStage, PrevStageImpl, Stage, RestStages...>
{
    using helper = stage_impl_helper<PrevStage, PrevStageImpl, Stage>;
    using impl = typename helper::stage_impl;

    using next_combiner = stage_impl_combiner<Stage, impl, RestStages...>;

    using inputs = prepend_tuple_t<typename helper::stage_input_type, typename next_combiner::inputs>;
    using impls   = prepend_tuple_t<impl, typename next_combiner::impls>;
};

template <class LastStage, class LastStageImpl>
struct stage_impl_combiner<LastStage, LastStageImpl>
{
    static_assert(has_complete_output<LastStage>(),
            "Last stage should output complete result");
    using inputs = std::tuple<>;
    using impls = std::tuple<>;
};



template <class Inputs, class Impls>
struct chain_maker;

template <class... Inputs, class... Impls>
struct chain_maker<std::tuple<Inputs...>, std::tuple<Impls...>>
{
    template <class... Stages>
    static constexpr auto make(Stages&&... stages)
    {
        using chain = chain<std::index_sequence_for<Stages...>, Impls...>;
        return chain{ {((Stages&&) stages).template make_impl<Inputs>()}... };
    }
};

template <class Range>
struct range_as_stage_wrapper
{
    using output_type = Range;
    static constexpr auto style = stage_styles::complete_to_complete;
};

template <class Range, class... Stages>
constexpr auto make_chain([[maybe_unused]] Range&& range, Stages&&... stages)
{
    static_assert(!is_specialization_of_v<composition, std::remove_cvref_t<Range>>,
            "At this stage Range should not contain compositions, there might be a bug");
    static_assert(!(is_specialization_of_v<composition, std::remove_cvref_t<Stages>> || ...),
            "at this stage Stages should not contain compositions, there might be a bug");

    using range_wrapper = range_as_stage_wrapper<Range&&>;
    using combiner = stage_impl_combiner<range_wrapper, range_wrapper, Stages&&...>;
    using maker = chain_maker<typename combiner::inputs, typename combiner::impls>;

    return maker::make((Stages&&) stages...);
}

// Remove all compostitions from Args and creates a chain from inner parts,
// which is suitable to iteratively accept (via process_incremental()) values with type Input
template <class Input, class... Args>
constexpr auto make_subchain_for_input(Args&&... args)
{
    auto dummy_gen = descend::generator<Input>([] (auto) { return false; });

    return apply_to_decomposed([] <class Range, class... Stages> (Range&& range, Stages&&... stages) {
        return make_chain((Range&&) range, (Stages&&) stages...);
    }, dummy_gen, (Args&&) args...);
}

template <class Subchain>
using subchain_end_t = decltype(std::declval<Subchain>().get(detail::index<0>{}).end());

} // namespace detail
} // namespace descend
