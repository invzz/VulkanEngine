#ifndef VULKANENGINE_INCLUDE_ENGINE_CORE_EXCEPTIONS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_CORE_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

#include "Engine/Core/ErrorCodes.hpp"

namespace engine {

    /**
 * @class RuntimeException
 * @brief Generic runtime error used across the engine instead of
 * std::runtime_error
 */
    class RuntimeException : public std::runtime_error {
       public:
        using std::runtime_error::runtime_error;
    };

    class SwapChainCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class ImageViewCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class RenderPassCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class FramebufferCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class InFlightFenceException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class SemaphoreCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class CommandBufferSubmissionException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class GraphicsPipelineCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class ReadFileException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class ShaderModuleCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class WindowSurfaceCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class CommandBufferRecordingException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class WindowInitializationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class WindowCreationException : public RuntimeException {
       public:
        using RuntimeException::RuntimeException;
    };

    class GraphicsException : public RuntimeException {
       public:
        GraphicsException(ErrorCode code, ErrorBoundary boundary, const std::string& message)
            : RuntimeException(message), code_{code}, boundary_{boundary} {}

        [[nodiscard]] ErrorCode code() const {
            return code_;
        }

        [[nodiscard]] ErrorBoundary boundary() const {
            return boundary_;
        }

       private:
        ErrorCode     code_;
        ErrorBoundary boundary_;
    };

    class RecoverableGraphicsException : public GraphicsException {
       public:
        RecoverableGraphicsException(ErrorCode code, const std::string& message)
            : GraphicsException(code, ErrorBoundary::Recoverable, message) {}
    };

    class FatalGraphicsException : public GraphicsException {
       public:
        FatalGraphicsException(ErrorCode code, const std::string& message)
            : GraphicsException(code, ErrorBoundary::Fatal, message) {}
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_CORE_EXCEPTIONS_HPP
