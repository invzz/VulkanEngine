#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace engine {
    class ThemeLoader {
       public:
        static std::vector<nlohmann::json> loadAll(const std::string& theme_dir);
        static nlohmann::json              loadByName(const std::string& theme_dir, const std::string& name);
        static std::string                 loadCurrentTheme(const std::string& config_path);
        static void                        saveCurrentTheme(const std::string& config_path, const std::string& theme_name);
    };
}  // namespace engine
