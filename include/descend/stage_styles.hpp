#pragma once

#include <ostream>
#include <string_view>
#include <type_traits>

namespace descend::detail {

namespace stage_styles {

enum class processing_style { incremental, complete };

constexpr bool is_incremental(const processing_style style)
{ return style == processing_style::incremental; }

constexpr bool is_complete(const processing_style style)
{ return style == processing_style::complete; }

struct stage_style
{
    processing_style input, output;
};

inline constexpr auto complete_to_complete = stage_style{processing_style::complete, processing_style::complete};
inline constexpr auto incremental_to_complete = stage_style{processing_style::incremental, processing_style::complete};
inline constexpr auto incremental_to_incremental = stage_style{processing_style::incremental, processing_style::incremental};

constexpr std::string_view to_string_view(const processing_style style) noexcept
{
    switch (style) {
    case processing_style::incremental: return "incremental";
    case processing_style::complete: return "complete";
    }
    return "";
}
inline std::ostream& operator << (std::ostream& strm, const processing_style style)
{ return strm << to_string_view(style); }

inline std::ostream& operator << (std::ostream& strm, const stage_style style)
{ return strm << style.input << "->" << style.output; }

} // namespace stage_styles

template <class T>
concept ExactlyStage = requires
{
    is_complete(T::style.input);
    is_complete(T::style.output);
};

template <class T>
concept Stage = ExactlyStage<std::remove_cvref_t<T>>;


template <class Stage>
constexpr bool has_incremental_input()
{ return stage_styles::is_incremental(std::remove_cvref_t<Stage>::style.input); }

template <class Stage>
constexpr bool has_incremental_output()
{ return stage_styles::is_incremental(std::remove_cvref_t<Stage>::style.output); }

template <class Stage>
constexpr bool has_complete_input()
{ return stage_styles::is_complete(std::remove_cvref_t<Stage>::style.input); }

template <class Stage>
constexpr bool has_complete_output()
{ return stage_styles::is_complete(std::remove_cvref_t<Stage>::style.output); }

} // namespace descend::detail
