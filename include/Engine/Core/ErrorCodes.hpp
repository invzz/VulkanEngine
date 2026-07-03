#ifndef VULKANENGINE_INCLUDE_ENGINE_CORE_ERRORCODES_HPP
#define VULKANENGINE_INCLUDE_ENGINE_CORE_ERRORCODES_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

    enum class ErrorBoundary : uint8_t {
        Recoverable = 0,
        Fatal       = 1,
    };

    enum class ErrorCode : uint16_t {
        Unspecified = 0,

        BufferAllocationFallback      = 1001,
        BufferAllocationFailure       = 1002,
        DescriptorPoolOverflowUsed    = 1101,
        DescriptorPoolAllocationError = 1102,
        DescriptorPoolCreationError   = 1103,
    };

    struct ErrorEvent {
        ErrorCode     code{ErrorCode::Unspecified};
        ErrorBoundary boundary{ErrorBoundary::Recoverable};
        std::string   message;
        uint64_t      timestampNs{0};
        uint64_t      count{0};
    };

    class ErrorState {
       public:
        static void                                  report(ErrorCode code, ErrorBoundary boundary, const std::string& message);
        static void                                  clear();
        [[nodiscard]] static std::vector<ErrorEvent> recentEvents(size_t maxEvents = 32);
        [[nodiscard]] static uint64_t                countByBoundary(ErrorBoundary boundary);
    };

}  // namespace engine

#endif
