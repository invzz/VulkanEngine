#ifndef VULKANENGINE_INCLUDE_ENGINE_CORE_LOGGER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_CORE_LOGGER_HPP
#include <atomic>
#include <cstdint>
#include <sstream>
#include <string_view>
#include <utility>
namespace engine {
    enum class LogLevel : uint8_t {
        Error = 0,
        Warn  = 1,
        Info  = 2,
        Debug = 3,
    };
    enum class LogChannel : uint32_t {
        None     = 0u,
        General  = 1u << 0,
        Render   = 1u << 1,
        Sync     = 1u << 2,
        Scene    = 1u << 3,
        Resource = 1u << 4,
        All      = 0xFFFFFFFFu,
    };



    class Logger {
       public:
        static void     setMinimumLevel(LogLevel level);
        static LogLevel minimumLevel();
        static void     setChannelMask(uint32_t mask);
        static void     enableChannel(LogChannel channel, bool enabled);
        static bool     isChannelEnabled(LogChannel channel);
        template <typename... Args>
        static void error(LogChannel channel, Args&&... args) {
            logStream(LogLevel::Error, channel, std::forward<Args>(args)...);
        }
        template <typename... Args>
        static void warn(LogChannel channel, Args&&... args) {
            logStream(LogLevel::Warn, channel, std::forward<Args>(args)...);
        }
        template <typename... Args>
        static void info(LogChannel channel, Args&&... args) {
            logStream(LogLevel::Info, channel, std::forward<Args>(args)...);
        }
        template <typename... Args>
        static void debug(LogChannel channel, Args&&... args) {
            logStream(LogLevel::Debug, channel, std::forward<Args>(args)...);
        }

       private:
        template <typename... Args>
        static void logStream(LogLevel level, LogChannel channel, Args&&... args) {
            std::ostringstream stream;
            (stream << ... << std::forward<Args>(args));
            log(level, channel, stream.str());
        }
        static void                  log(LogLevel level, LogChannel channel, std::string_view message);
        static std::atomic<uint8_t>  minLevel_;
        static std::atomic<uint32_t> channelMask_;
    };
    constexpr uint32_t toMask(LogChannel channel) {
        return static_cast<uint32_t>(channel);
    }
}  // namespace engine
#endif
