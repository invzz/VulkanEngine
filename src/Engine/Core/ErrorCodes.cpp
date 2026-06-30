#include "Engine/Core/ErrorCodes.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>

namespace engine {

    namespace {

        struct ErrorStateStorage {
            std::mutex             mutex;
            std::deque<ErrorEvent> events;
            uint64_t               recoverableCount{0};
            uint64_t               fatalCount{0};
        };

        ErrorStateStorage& storage() {
            static ErrorStateStorage s;
            return s;
        }

        uint64_t nowNs() {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        constexpr size_t kMaxStoredEvents = 256;

    }  // namespace

    void ErrorState::report(ErrorCode code, ErrorBoundary boundary, const std::string& message) {
        auto&                       s = storage();
        std::lock_guard<std::mutex> lock(s.mutex);

        if (boundary == ErrorBoundary::Recoverable) {
            s.recoverableCount++;
        } else {
            s.fatalCount++;
        }

        if (!s.events.empty()) {
            ErrorEvent& last = s.events.back();
            if (last.code == code && last.boundary == boundary && last.message == message) {
                last.count += 1;
                last.timestampNs = nowNs();
                return;
            }
        }

        ErrorEvent event;
        event.code        = code;
        event.boundary    = boundary;
        event.message     = message;
        event.timestampNs = nowNs();
        event.count       = 1;
        s.events.push_back(std::move(event));

        while (s.events.size() > kMaxStoredEvents) {
            s.events.pop_front();
        }
    }

    void ErrorState::clear() {
        auto&                       s = storage();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.events.clear();
        s.recoverableCount = 0;
        s.fatalCount       = 0;
    }

    std::vector<ErrorEvent> ErrorState::recentEvents(size_t maxEvents) {
        auto&                       s = storage();
        std::lock_guard<std::mutex> lock(s.mutex);

        std::vector<ErrorEvent> out;
        if (s.events.empty()) {
            return out;
        }

        size_t const count = std::min(maxEvents, s.events.size());
        out.reserve(count);
        auto beginIt = s.events.end() - static_cast<std::ptrdiff_t>(count);
        for (auto it = beginIt; it != s.events.end(); ++it) {
            out.push_back(*it);
        }
        return out;
    }

    uint64_t ErrorState::countByBoundary(ErrorBoundary boundary) {
        auto&                       s = storage();
        std::lock_guard<std::mutex> lock(s.mutex);
        return (boundary == ErrorBoundary::Recoverable) ? s.recoverableCount : s.fatalCount;
    }

}  // namespace engine
