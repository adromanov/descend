#pragma once

#include "helpers.hpp"

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace descend {
namespace detail {

// args is used for multi-argument streams, returning args from a stage will allow to
// have several args at next stage.
//
// Since data flow is push - earlier stages calls later stages direcly and pass parameters there
// args would almost always be received as rvalue reference.
//
// We have special args_invoke which would decompose args and call a callable with several arguments passed
// Since each argument inside args can be value, lvalue reference or rvalue reference we need to have
// explicit semantics of how each arg would be treated by args_invoke.
//
// Let's disable calling args_invoke() with non-const reference to args - we probably don't need it
//
// For const args<int, const long, bool&, const double&, float&&, const char&&>& the invocable would be called with:
// (const int&, const long&, const bool&, const double&, const float&, const char&)
//
// For args<int, bool&, const double&, float&&, const char&&>&& the invocable would be called with
// (int &&, const long&&, bool &, const double&, float&&, const char&&)
//   ^- note that value-type argument became r-value reference

template <class... Ts>
struct args : std::tuple<Ts...>
{
    using tuple_type = std::tuple<Ts...>;
    using std::tuple<Ts...>::tuple;

    template <class Tuple>
        requires std::is_constructible_v<tuple_type, Tuple>
    constexpr args(Tuple&& tuple) noexcept(std::is_nothrow_constructible_v<tuple_type, Tuple>)
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
using make_index_sequence_for_tuple = std::make_index_sequence<std::tuple_size_v<Tuple>>;

template <class Args>
using make_index_sequence_for_args = make_index_sequence_for_tuple<typename std::remove_cvref_t<Args>::tuple_type>;



template <std::size_t I, class... Ts>
constexpr decltype(auto) get(args<Ts...>&& args) noexcept
{
    return std::get<I>(std::move(args).as_tuple());
}

// Everything becomes const &
template <std::size_t I, class... Ts>
constexpr decltype(auto) get(const args<Ts...>& args) noexcept
{
    auto&& res = std::get<I>(args.as_tuple());
    return std::as_const(res);
}

// Non-const lvalue references are not supported (no usage for now, semantics is unclear)
template <std::size_t I, class... Ts>
constexpr void get(args<Ts...>& args) noexcept = delete;


template <class F, class Arg>
constexpr decltype(auto) args_invoke(F&& f, Arg&& arg)
{
    if constexpr (is_specialization_of_v<args, std::remove_cvref_t<Arg>>) {
        return [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            return std::invoke((F&&) f, get<Is>((Arg&&) arg)...);
        } (make_index_sequence_for_args<Arg>{});
    }
    else {
        return std::invoke((F&&) f, (Arg&&) arg);
    }
}

template <class F, class Arg>
constexpr bool is_args_invocable_helper()
{
    if constexpr (is_specialization_of_v<args, std::remove_cvref_t<Arg>>) {
        return [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            return std::is_invocable_v<F, decltype(get<Is>(std::declval<Arg>()))...>;
        } (make_index_sequence_for_args<Arg>{});
    }
    else {
        return std::is_invocable_v<F, Arg>;
    }
}
template <class F, class Arg>
inline constexpr bool is_args_invocable_v = is_args_invocable_helper<F, Arg>();


template <class F, class Arg>
using args_invoke_result_t = decltype(args_invoke(std::declval<F>(), std::declval<Arg>()));


template <class Tuple>
struct args_for_tuple;
template <class... Ts>
struct args_for_tuple<std::tuple<Ts...>>
{
    using type = args<Ts...>;
};
template <class F, class S>
struct args_for_tuple<std::pair<F, S>>
{
    using type = args<F, S>;
};

template <class T>
using args_for_tuple_t = typename args_for_tuple<T>::type;

template <class Tuple>
constexpr auto to_args(Tuple&& tuple)
    -> args_for_tuple_t<std::remove_cvref_t<Tuple>>
{
    return {(Tuple&&) tuple};
}



template <class Maker, std::size_t... Is>
constexpr auto make_args_elementwise(Maker&& maker, std::index_sequence<Is...>)
{
    using args_type = args<decltype(maker.template operator()<Is>())...>;
    return args_type{ maker.template operator()<Is>()... };
}

template <std::size_t Index, class Args, class Func>
constexpr auto transform_one_arg(Args&& args, Func&& func)
{
    auto get_element_or_transform = [&] <std::size_t I> () -> decltype(auto)
    {
        if constexpr (I == Index) {
            return std::invoke(func, get<I>((Args&&) args));
        }
        else {
            return get<I>((Args&&) args);
        }
    };

    return make_args_elementwise(get_element_or_transform, make_index_sequence_for_args<Args>{});
}

} // namespace detail
} // namespace descend
