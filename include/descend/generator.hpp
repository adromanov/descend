#pragma once

#include <utility>

namespace descend {
namespace detail {

template <class T, class F>
struct generator
{
    F f;

    using output_type = T;
};

} // namespace detail

template <class T, class F>
constexpr auto generator(F&& f)
{
    return detail::generator<T, F>{(F&&) f};
}

template <class T>
constexpr auto iota(T begin)
{
    return generator<T>([begin] (auto output) mutable {
        std::move(output)(begin);
        ++begin;
        return true;
    });
}

template <class T, class E>
constexpr auto iota(T begin, E end)
{
    return generator<T>([begin, end] (auto output) mutable {
        if (begin != end) {
            std::move(output)(begin);
            ++begin;
            return true;
        }
        return false;
    });
}

} // namespace descend
