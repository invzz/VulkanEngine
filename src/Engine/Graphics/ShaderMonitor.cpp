#include "Engine/Graphics/ShaderMonitor.hpp"

#include <algorithm>

#include "Engine/Core/Logger.hpp"
namespace engine {
    ShaderMonitor::ShaderMonitor()  = default;
    ShaderMonitor::~ShaderMonitor() = default;
    void ShaderMonitor::addShader(const std::string& filePath) {
        try {
            auto timestamp              = std::filesystem::last_write_time(filePath);
            shaderTimestamps_[filePath] = timestamp;
        } catch (const std::exception& e) {
            Logger::warn(LogChannel::Render, "Failed to add shader for monitoring: ", filePath, " - ", e.what());
        }
    }
    void ShaderMonitor::removeShader(const std::string& filePath) {
        shaderTimestamps_.erase(filePath);
    }
    bool ShaderMonitor::hasAnyShaderChanged(std::string* changedShaderPath) const {
        auto hasChanged = std::ranges::any_of(shaderTimestamps_, [this, changedShaderPath](const auto& entry) {
            const auto& filePath   = entry.first;
            const auto& cachedTime = entry.second;
            try {
                auto current = std::filesystem::last_write_time(filePath);
                if (current != cachedTime) {
                    if (changedShaderPath != nullptr) {
                        *changedShaderPath = filePath;
                    }
                    return true;
                }
            } catch (const std::exception& e) {
                Logger::warn(LogChannel::Render, "Failed to check shader timestamp: ", filePath, " - ", e.what());
            }
            return false;
        });
        return hasChanged;
    }
    void ShaderMonitor::refreshTimestamps() {
        for (auto& [filePath, timestamp] : shaderTimestamps_) {
            try {
                timestamp = std::filesystem::last_write_time(filePath);
            } catch (const std::exception& e) {
                Logger::warn(LogChannel::Render, "Failed to refresh timestamp for: ", filePath, " - ", e.what());
            }
        }
    }
    std::filesystem::file_time_type ShaderMonitor::getShaderTimestamp(const std::string& filePath) const {
        auto it = shaderTimestamps_.find(filePath);
        if (it != shaderTimestamps_.end()) {
            return it->second;
        }
        return {};
    }
}  // namespace engine