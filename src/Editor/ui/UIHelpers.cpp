#include "Editor/ui/UIHelpers.hpp"

#include <algorithm>
#include <cctype>

namespace engine::ui {

    std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool MatchesFilter(const std::string& name, const char* filter) {
        if (filter == nullptr || filter[0] == '\0') {
            return true;
        }
        return ToLower(name).find(ToLower(filter)) != std::string::npos;
    }

}  // namespace engine::ui
