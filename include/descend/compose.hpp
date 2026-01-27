#pragma once

#include "helpers.hpp"

#include <functional>
#include <tuple>
#include <type_traits>

namespace descend {

namespace detail {

// Class to hold composed stages
template <class... Ts>
struct composition : std::tuple<Ts...>
{
    using tuple_type = std::tuple<Ts...>;

    using std::tuple<Ts...>::tuple;

    template <class Tuple>
        requires std::is_constructible_v<tuple_type, Tuple>
    constexpr composition(Tuple&& tuple) noexcept(std::is_nothrow_constructible_v<tuple_type, Tuple>)
        : tuple_type((Tuple&&) tuple)
    { }

    constexpr decltype(auto) as_tuple() & noexcept
    { return static_cast<tuple_type&>(*this); }
    constexpr decltype(auto) as_tuple() const & noexcept
    { return static_cast<const tuple_type&>(*this); }

    constexpr decltype(auto) as_tuple() && noexcept
    { return static_cast<tuple_type&&>(*this); }
    constexpr decltype(auto) as_tuple() const && noexcept
    { return static_cast<const tuple_type&&>(*this); }
};

template <class Tuple>
struct composition_for_tuple;
template <class... Ts>
struct composition_for_tuple<std::tuple<Ts...>>
{
    using type = composition<Ts...>;
};
template <class T>
using composition_for_tuple_t = typename composition_for_tuple<T>::type;

template <class Tuple>
constexpr auto to_composition(Tuple&& tuple)
    -> composition_for_tuple_t<std::remove_cvref_t<Tuple>>
{
    return {(Tuple&&) tuple};
}


template <class Part>
constexpr decltype(auto) as_tuple(Part&& part)
{
    if constexpr (is_specialization_of_v<composition, std::remove_cvref_t<Part>>) {
        return ((Part&&) part).as_tuple();
    }
    else {
        return std::make_tuple((Part&&) part);
    }
}

template <class Part>
constexpr decltype(auto) as_forwarding_tuple(Part&& part) noexcept
{
    if constexpr (is_specialization_of_v<composition, std::remove_cvref_t<Part>>) {
        return std::apply([] <class... Args> (Args&&... args) {
            return std::forward_as_tuple((Args&&) args...);
        }, ((Part&&) part).as_tuple());
    }
    else {
        return std::forward_as_tuple((Part&&) part);
    }
}

template <class... Parts>
constexpr auto forward_compose(Parts&&... parts) noexcept
    -> decltype(to_composition(std::tuple_cat(as_forwarding_tuple((Parts&&) parts)...)))
{
    return to_composition(std::tuple_cat(as_forwarding_tuple((Parts&&) parts)...));
}

} // namespace detail

template <class... Parts>
constexpr auto compose(Parts&&... parts)
    -> decltype(detail::to_composition(std::tuple_cat(detail::as_tuple((Parts&&) parts)...)))
{
    return detail::to_composition(std::tuple_cat(detail::as_tuple((Parts&&) parts)...));
}

namespace detail {

// removes all compositions from Parts and calls Func with all stages inside all composed parts
// for:
//      composition<A, B>, C, composition<C, D, F>
// will call Func with
//      A, B, C, D, E, F
template <class Func, class... Parts>
constexpr decltype(auto) apply_to_decomposed(Func&& func, Parts&&... parts)
{
    return std::apply([&func] <class... Args> (Args&&... args) -> decltype(auto) {
        return std::invoke((Func&&) func, (Args&&) args...);
    }, detail::forward_compose((Parts&&) parts...).as_tuple());
}
} // namespace detail

} // namespace descend
