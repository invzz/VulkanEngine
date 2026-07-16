#pragma once

#include <string>

namespace engine::ui {

    /// Lowercase a copy of `value`.
    std::string ToLower(std::string value);

    /// True if `filter` is empty, or `name` contains it (case-insensitive).
    bool MatchesFilter(const std::string& name, const char* filter);

}  // namespace engine::ui
