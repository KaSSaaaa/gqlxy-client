#pragma once

#include <algorithm>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace gqlxy::internal {

template <std::ranges::input_range R>
auto to_vector(R&& r) {
    using T = std::ranges::range_value_t<R>;
    return std::vector<T>(std::ranges::begin(r), std::ranges::end(r));
}

template <std::ranges::input_range R>
auto to_string(R&& r) {
    return std::string(std::ranges::begin(r), std::ranges::end(r));
}

template <std::ranges::input_range R>
auto trim(R&& s) {
    static auto is_space = [](auto c) { return std::isspace(static_cast<unsigned char>(c)); };
    auto r = s
        | std::views::drop_while(is_space)
        | std::views::reverse
        | std::views::drop_while(is_space)
        | std::views::reverse;
    return std::string(r.begin(), r.end());
}

inline auto split(std::string_view s, char delim) -> std::vector<std::string> {
    return to_vector(
        s | std::views::split(delim)
          | std::views::transform([](auto part) { return to_string(part); })
    );
}

template <typename F>
auto make_optional_if(bool cond, F&& f) -> std::optional<std::invoke_result_t<F>> {
    if (!cond) return std::nullopt;
    return f();
}

template <std::ranges::range R>
auto to_optional(R&& r, std::ranges::iterator_t<R> it)
    -> std::optional<std::ranges::range_value_t<R>>
{
    if (it == std::ranges::end(r)) return std::nullopt;
    return *it;
}

template <std::ranges::range R, typename Pred>
auto find_optional(R&& r, Pred&& pred)
    -> std::optional<std::ranges::range_value_t<R>>
{
    return to_optional(r, std::ranges::find_if(r, std::forward<Pred>(pred)));
}

template <std::ranges::input_range R, typename F>
    requires std::ranges::input_range<std::decay_t<std::invoke_result_t<F, std::ranges::range_reference_t<R>>>>
auto flat_map(R&& r, F&& f) {
    using Inner = std::decay_t<std::invoke_result_t<F, std::ranges::range_reference_t<R>>>;
    using T     = std::ranges::range_value_t<Inner>;
    std::vector<T> result;
    for (auto&& x : r) {
        auto sub = f(x);
        result.insert(result.end(), std::ranges::begin(sub), std::ranges::end(sub));
    }
    return result;
}

inline auto chunk_by_blank(const std::vector<std::string>& lines)
    -> std::vector<std::vector<std::string>>
{
    std::vector<std::vector<std::string>> blocks;
    std::vector<std::string>             current;
    for (const auto& line : lines) {
        if (line.empty()) {
            if (!current.empty()) { blocks.push_back(std::move(current)); current = {}; }
        } else {
            current.push_back(line);
        }
    }
    if (!current.empty()) blocks.push_back(std::move(current));
    return blocks;
}

}
