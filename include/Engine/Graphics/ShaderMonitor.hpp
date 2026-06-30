#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace engine {

    /**
 * @brief Status of shader file monitoring
 */
    enum class ShaderWatchStatus : uint8_t {
        NOT_WATCHING,
        WATCHING,
        ERROR
    };

    /**
 * @brief Tracks shader files and detects changes
 */
    class ShaderMonitor {
       public:
        ShaderMonitor();
        ~ShaderMonitor();

        void                            addShader(const std::string& filePath);
        void                            removeShader(const std::string& filePath);
        bool                            hasAnyShaderChanged(std::string* changedShaderPath = nullptr) const;
        void                            refreshTimestamps();
        std::filesystem::file_time_type getShaderTimestamp(const std::string& filePath) const;

       private:
        std::unordered_map<std::string, std::filesystem::file_time_type> shaderTimestamps_;
    };

}  // namespace engine
