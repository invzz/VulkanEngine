#include "Editor/ui/StaticColliderRules.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace engine {

    namespace {

        const std::vector<std::string>& autoTokens() {
            static const std::vector<std::string> tokens = {
                "col_", "ucx_", "collision", "collider", "wall", "floor",
                "ground", "world", "level", "static"};
            return tokens;
        }

        std::string toLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

    }  // namespace

    bool StaticColliderRules::ShouldCreate(
        const std::string&                              path,
        const std::string&                              name,
        ModelInsertionOptions::StaticColliderImportMode mode) {
        switch (mode) {
            case ModelInsertionOptions::StaticColliderImportMode::ForceOn:
                return true;
            case ModelInsertionOptions::StaticColliderImportMode::ForceOff:
                return false;
            case ModelInsertionOptions::StaticColliderImportMode::AutoDetect:
            default:
                return AutoDetect(path, name);
        }
    }

    std::string StaticColliderRules::Tooltip() {
        std::string text   = "Auto tokens: ";
        const auto& tokens = autoTokens();
        for (size_t i = 0; i < tokens.size(); ++i) {
            text += tokens[i];
            if (i + 1 < tokens.size()) {
                text += ", ";
            }
        }
        return text;
    }

    bool StaticColliderRules::AutoDetect(const std::string& path, const std::string& name) {
        const std::string combined = toLower(path + " " + name);
        for (const auto& token : autoTokens()) {
            if (combined.find(token) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

}  // namespace engine
