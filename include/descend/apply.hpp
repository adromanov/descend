#pragma once

#include "descend/chain.hpp"
#include "descend/compose.hpp"
#ifdef DESCEND_ENABLE_TYPE_DEBUG
#include "descend/debug.hpp"

#include <iostream>
#endif
#include <cstddef>

namespace descend {

namespace detail {
struct test_failed_exception : std::exception
{ };
} // namespace detail

namespace detail {

template <class Range, class... Stages>
[[nodiscard]]
constexpr auto apply_no_compositions(Range&& range, Stages&&... stages)
{
    auto chain = detail::make_chain((Range&&) range, (Stages&&)stages...);
#ifdef DESCEND_ENABLE_TYPE_DEBUG
    try {
        return chain.get(detail::index<0>{}).process_complete((Range&&) range);
    }
    catch (test_failed_exception& e) {
        detail::debug_print_chain(std::cerr, chain);
        throw;
    }
#else
    return chain.get(detail::index<0>{}).process_complete((Range&&) range);
#endif
}
} // namespace detail


template <class Range, class... Stages>
[[nodiscard]]
constexpr auto apply(Range&& range, Stages&&... stages)
{
    // Remove all compositions from Range and Stages
    return detail::apply_to_decomposed([] <class... Args> (Args&&... args) {
        return detail::apply_no_compositions((Args&&) args...);
    }, (Range&&) range, (Stages&&) stages...);
}

template <class T, std::size_t N, class... Stages>
[[nodiscard]]
constexpr auto apply(T (&&array)[N], Stages&&... stages)
{
    // Remove all compositions from Stages
    return detail::apply_to_decomposed([] <class... Args> (Args&&... args) {
        return detail::apply_no_compositions((Args&&) args...);
    }, std::move(array), (Stages&&) stages...);
}

} // namespace descend
