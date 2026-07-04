#include "Editor/ui/Workspace/ThemeLoader.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Engine/Core/Logger.hpp"
namespace engine {
    static nlohmann::json parseFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + path);
        }
        nlohmann::json j;
        file >> j;
        engine::Logger::info(engine::LogChannel::General, "ThemeLoader: parseFile Loaded theme file ", path);
        return j;
    }
    std::vector<nlohmann::json> ThemeLoader::loadAll(const std::string& theme_dir) {
        std::vector<nlohmann::json> themes;
        if (!std::filesystem::exists(theme_dir))
            return themes;
        for (const auto& entry : std::filesystem::directory_iterator(theme_dir)) {
            if (entry.path().extension() == ".json") {
                try {
                    auto j = parseFile(entry.path().string());
                    themes.push_back(j);
                    engine::Logger::info(engine::LogChannel::General, "ThemeLoader: loadAll Loaded theme ", entry.path().filename());
                } catch (const std::exception& e) {
                    engine::Logger::error(engine::LogChannel::General, "ThemeLoader: loadAll Failed to load theme ", entry.path().filename(), ": ", e.what());
                }
            }
        }
        return themes;
    }
    nlohmann::json ThemeLoader::loadByName(const std::string& theme_dir, const std::string& name) {
        auto themes = loadAll(theme_dir);
        for (const auto& j : themes) {
            auto        n            = j.value("name", std::string(""));
            std::string lower_name   = n;
            std::string lower_lookup = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            std::transform(lower_lookup.begin(), lower_lookup.end(), lower_lookup.begin(), ::tolower);
            if (lower_name == lower_lookup) {
                engine::Logger::info(engine::LogChannel::General, "ThemeLoader: load by name ", name);
                return j;
            }
        }
        throw std::runtime_error("Theme not found: " + name);
    }
    std::string ThemeLoader::loadCurrentTheme(const std::string& config_path) {
        try {
            auto j = parseFile(config_path);
            engine::Logger::info(engine::LogChannel::General, "ThemeLoader: Current theme is ", j.value("current_theme", "dark"));
            return j.value("current_theme", "dark");
        } catch (const std::exception& e) {
            engine::Logger::error(engine::LogChannel::General, "ThemeLoader: Failed to load config: ", e.what());
            return "dark";
        }
    }
    void ThemeLoader::saveCurrentTheme(const std::string& config_path, const std::string& theme_name) {
        nlohmann::json j;
        j["current_theme"] = theme_name;
        std::ofstream file(config_path);
        if (file.is_open()) {
            file << j.dump(2);
            file.close();
        } else {
            engine::Logger::error(engine::LogChannel::General, "ThemeLoader: Failed to save config: ", config_path);
        }
    }
}  // namespace engine
