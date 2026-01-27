#pragma once

#include <type_traits>
#include <utility>
#include <variant>

namespace descend::detail {

using unit = std::monostate;

template<class T, class U>
constexpr auto&& forward_like(U&& x) noexcept
{
    constexpr bool is_adding_const = std::is_const_v<std::remove_reference_t<T>>;
    if constexpr (std::is_lvalue_reference_v<T&&>) {
        if constexpr (is_adding_const) {
            return std::as_const(x);
        }
        else {
            return static_cast<U&>(x);
        }
    }
    else {
        if constexpr (is_adding_const) {
            return std::move(std::as_const(x));
        }
        else {
            return std::move(x); // NOLINT(bugprone-move-forwarding-reference)
        }
    }
}

template <class T>
inline constexpr bool always_false_v = false;

template <template <class...> class Template, class T>
struct is_specialization_of : std::false_type{};

template <template <class...> class Template, class... Ts>
struct is_specialization_of<Template, Template<Ts...>> : std::true_type{};

template <template <class...> class Template, class T>
inline constexpr bool is_specialization_of_v = is_specialization_of<Template, T>::value;

} // namespace descend::details

namespace descend {
// Inspect type or compile-time value via compilation error.
template <class...>
struct TP;
template <auto...>
struct VP;
//
//  TP<decltype(...)> x; // error: implicit instantiation of undefined template 'descend::TP</* type for inspecting */>'
//
} // namespace descend
