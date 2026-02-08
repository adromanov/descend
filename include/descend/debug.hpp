#pragma once

#include "descend/higher_order.hpp"

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>

namespace descend::detail {

template <class T>
constexpr std::string_view type_name_impl() {
#if defined(__clang__)
    std::string_view p = __PRETTY_FUNCTION__;
    auto start = p.find("T = ") + 4;
    auto end   = p.find_last_of(']');
    return p.substr(start, end - start);
#elif defined(__GNUC__)  // not clang
    std::string_view p = __PRETTY_FUNCTION__;
    auto start = p.find("with T = ") + 9;
    auto end   = p.find(';', start);
    return p.substr(start, end - start);
#elif defined(_MSC_VER)
    std::string_view p = __FUNCSIG__;
    auto start = p.find("type_name<") + 10;
    auto end   = p.find(">(void)", start);
    return p.substr(start, end - start);
#else
    return "unsupported compiler";
#endif
}

inline void replace_all(
        std::string& str,
        const std::string& from,
        const std::string& to)
{
    std::size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length(); // move past the replacement
    }
}

template <class T>
std::string type_name()
{
    auto s = std::string{type_name_impl<T>()};
    replace_all(s,
            "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >",
            "std::string");
    replace_all(s, "descend::detail::stages::", "");
    replace_all(s, "descend::detail::", "");
    return s;
}


struct indent
{
    std::size_t depth = 0;
    friend std::ostream& operator << (std::ostream& strm, const indent ind)
    {
        for (std::size_t i = 0; i < ind.depth; ++i) {
            strm << "  ";
        }
        return strm;
    }
};

// Usually StageImpl provides stage_type alias to corresponding stage, which has style.
// Some stages (currently the ones, using base_accumulate_stage) have 2 aliases:
// * stage_type to get processig style from
// * display_stage_type to output display name
template <class StageImpl>
struct display_stage_type_for_impl
{
    using type = typename StageImpl::stage_type;
};
template <class StageImpl>
    requires requires { typename StageImpl::display_stage_type; }
struct display_stage_type_for_impl<StageImpl>
{
    using type = typename StageImpl::display_stage_type;
};

// Default debug print for any stage - prints basic info
template <class StageImpl>
void debug_print_stage_default(std::ostream& strm, const std::size_t depth, const std::size_t index)
{
    using display_stage_type = typename display_stage_type_for_impl<StageImpl>::type;
    constexpr auto style = StageImpl::stage_type::style;
    strm << indent(depth) << '#' << index << " stage: " << std::quoted(type_name<display_stage_type>()) << ' ' << style << '\n';
    strm << indent(depth) << "input: " << std::quoted(type_name<typename StageImpl::input_type>()) << '\n';
    strm << indent(depth) << "output: " << std::quoted(type_name<typename StageImpl::output_type>()) << '\n';
}

// Custom debug printer - use class template for partial specialization
template <class StageType>
struct custom_debug_printer
{
    template <class StageImpl>
    static void print(std::ostream&, std::size_t)
    {
        // Default: no custom output
    }
};

// Custom debug print - dispatches to custom_debug_printer
template <class StageImpl>
void debug_print_stage_custom(std::ostream& strm, std::size_t depth)
{
    using stage_type = typename StageImpl::stage_type;
    custom_debug_printer<stage_type>::template print<StageImpl>(strm, depth);
}

template <class StagesTuple>
struct stages_printer;

template <class... StageImpls>
struct stages_printer<std::tuple<StageImpls...>>
{
    static void print(std::ostream& strm, const std::size_t depth = 0)
    {
        const auto print_stage = [&] <class StageImpl> (std::ostream& strm, const std::size_t index) {
            // Generic debug info
            debug_print_stage_default<StageImpl>(strm, depth, index);

            // Custom stage-specific debug info
            debug_print_stage_custom<StageImpl>(strm, depth);
        };

        std::size_t index = 0;
        ((print_stage.template operator() <StageImpls> (strm, index++)), ...);
    }
};

template <class Chain>
void debug_print_chain(std::ostream& strm, std::size_t depth = 0)
{
    strm << indent(depth) << "Stages:\n";
    stages_printer<typename Chain::stage_impls>::print(strm, depth);
    strm.flush();
}

template <class Chain>
void debug_print_chain(std::ostream& strm, const Chain&, std::size_t depth = 0)
{
    debug_print_chain<Chain>(strm, depth);
}

// Specialization for tee_stage
template <class... Subchains>
struct custom_debug_printer<stages::tee_stage<Subchains...>>
{
    template <class StageImpl>
    static void print(std::ostream& strm, std::size_t depth)
    {
        using chains_tuple = typename StageImpl::chains_tuple;

        strm << indent(depth) << "  Subchains (" << std::tuple_size_v<chains_tuple> << "):\n";

        auto print_subchain = [&] <std::size_t I> () {
            using chain_type = std::tuple_element_t<I, chains_tuple>;
            strm << indent(depth) << "    [" << I << "]:\n";
            debug_print_chain<chain_type>(strm, depth + 3);
        };

        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            (print_subchain.template operator() <Is> (), ...);
        } (std::make_index_sequence<std::tuple_size_v<chains_tuple>>{});
    }
};

// Specialization for map_group_by_stage
template <template <class...> class Map, class KeyGetter, class ComposedChain>
struct custom_debug_printer<stages::map_group_by_stage<Map, KeyGetter, ComposedChain>>
{
    template <class StageImpl>
    static void print(std::ostream& strm, std::size_t depth)
    {
        using chain_type = typename StageImpl::chain_type;
        using key_type = typename StageImpl::key_type;

        strm << indent(depth) << "  Key type: " << std::quoted(type_name<key_type>()) << '\n';
        strm << indent(depth) << "  Per-group chain:\n";
        debug_print_chain<chain_type>(strm, depth + 2);
    }
};

// Specialization for group_by_stage
template <class KeyGetter, class ComposedChain>
struct custom_debug_printer<stages::group_by_stage<KeyGetter, ComposedChain>>
{
    template <class StageImpl>
    static void print(std::ostream& strm, std::size_t depth)
    {
        using chain_type = typename StageImpl::chain_type;
        using key_type = typename StageImpl::key_type;

        strm << indent(depth) << "  Key type: " << std::quoted(type_name<key_type>()) << '\n';
        strm << indent(depth) << "  Per-group chain:\n";
        debug_print_chain<chain_type>(strm, depth + 2);
    }
};

} // namespace descend::detail

namespace descend {

// apply_debug - executes chain with debug output
// FIXME: that is a copy-pase from apply(), would be better to have shared code,
// but let's keep it as is for now.
namespace detail {
template <class Range, class... Stages>
[[nodiscard]]
constexpr auto apply_no_compositions_debug(std::ostream& strm, Range&& range, Stages&&... stages)
{
    auto chain = detail::make_chain((Range&&) range, (Stages&&)stages...);
    debug_print_chain(strm, chain);
    return chain.get(detail::index<0>{}).process_complete((Range&&) range);
}
} // namespace detail

template <class Range, class... Stages>
[[nodiscard]]
constexpr auto apply_debug(std::ostream& strm, Range&& range, Stages&&... stages)
{
    return detail::apply_to_decomposed([&strm] <class... Args> (Args&&... args) {
        return detail::apply_no_compositions_debug(strm, (Args&&) args...);
    }, (Range&&) range, (Stages&&) stages...);
}
template <class Range, class... Stages>
[[nodiscard]]
constexpr auto apply_debug(Range&& range, Stages&&... stages)
{
    return apply_debug(std::cout, (Range&&) range, (Stages&&) stages...);
}

// Array overloads
template <class T, std::size_t N, class... Stages>
[[nodiscard]]
constexpr auto apply_debug(std::ostream& strm, T (&&array)[N], Stages&&... stages)
{
    return detail::apply_to_decomposed([&strm] <class... Args> (Args&&... args) {
        return detail::apply_no_compositions_debug(strm, (Args&&) args...);
    }, std::move(array), (Stages&&) stages...);
}
template <class T, std::size_t N, class... Stages>
[[nodiscard]]
constexpr auto apply_debug(T (&&array)[N], Stages&&... stages)
{
    return apply_debug(std::cout, std::move(array), (Stages&&) stages...);
}

} // namespace descend
