#include "Engine/Core/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace engine {

    std::atomic<uint8_t>  Logger::minLevel_{static_cast<uint8_t>(LogLevel::Info)};
    std::atomic<uint32_t> Logger::channelMask_{toMask(LogChannel::All)};

    namespace {

        std::mutex gLogMutex;

        const char* levelName(LogLevel level) {
            switch (level) {
                case LogLevel::Error:
                    return "ERROR";
                case LogLevel::Warn:
                    return "WARN";
                case LogLevel::Info:
                    return "INFO";
                case LogLevel::Debug:
                    return "DEBUG";
                default:
                    return "UNKNOWN";
            }
        }

        const char* channelName(LogChannel channel) {
            switch (channel) {
                case LogChannel::General:
                    return "General";
                case LogChannel::Render:
                    return "Render";
                case LogChannel::Sync:
                    return "Sync";
                case LogChannel::Scene:
                    return "Scene";
                case LogChannel::Resource:
                    return "Resource";
                default:
                    return "Other";
            }
        }

    }  // namespace

    void Logger::setMinimumLevel(LogLevel level) {
        minLevel_.store(static_cast<uint8_t>(level), std::memory_order_relaxed);
    }

    LogLevel Logger::minimumLevel() {
        return static_cast<LogLevel>(minLevel_.load(std::memory_order_relaxed));
    }

    void Logger::setChannelMask(uint32_t mask) {
        channelMask_.store(mask, std::memory_order_relaxed);
    }

    void Logger::enableChannel(LogChannel channel, bool enabled) {
        uint32_t const bit  = toMask(channel);
        uint32_t       mask = channelMask_.load(std::memory_order_relaxed);
        if (enabled) {
            mask |= bit;
        } else {
            mask &= ~bit;
        }
        channelMask_.store(mask, std::memory_order_relaxed);
    }

    bool Logger::isChannelEnabled(LogChannel channel) {
        uint32_t const mask = channelMask_.load(std::memory_order_relaxed);
        return (mask & toMask(channel)) != 0u;
    }

    void Logger::log(LogLevel level, LogChannel channel, std::string_view message) {
        if (static_cast<uint8_t>(level) > minLevel_.load(std::memory_order_relaxed)) {
            return;
        }
        if ((channelMask_.load(std::memory_order_relaxed) & toMask(channel)) == 0u) {
            return;
        }

        auto const  now = std::chrono::system_clock::now();
        std::time_t t   = std::chrono::system_clock::to_time_t(now);

        std::tm tmBuffer{};
#if defined(_WIN32)
        localtime_s(&tmBuffer, &t);
#else
        localtime_r(&t, &tmBuffer);
#endif

        std::lock_guard<std::mutex> lock(gLogMutex);
        std::ostream&               out = (level == LogLevel::Error || level == LogLevel::Warn) ? std::cerr : std::cout;
        out << "[" << std::put_time(&tmBuffer, "%H:%M:%S") << "]"
            << "[" << levelName(level) << "]"
            << "[" << channelName(channel) << "] "
            << message << '\n';
    }

}  // namespace engine
