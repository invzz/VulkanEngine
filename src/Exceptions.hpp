#pragma once
#include <exception>
namespace engine {
    class SwapChainCreationException : public std::exception
    {
      public:
        explicit SwapChainCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class ImageViewCreationException : public std::exception
    {
      public:
        explicit ImageViewCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class RenderPassCreationException : public std::exception
    {
      public:
        explicit RenderPassCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class FramebufferCreationException : public std::exception
    {
      public:
        explicit FramebufferCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class InFlightFenceException : public std::exception
    {
      public:
        explicit InFlightFenceException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class SemaphoreCreationException : public std::exception
    {
      public:
        explicit SemaphoreCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class CommandBufferSubmissionException : public std::exception
    {
      public:
        explicit CommandBufferSubmissionException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class GraphicsPipelineCreationException : public std::exception
    {
      public:
        explicit GraphicsPipelineCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class ReadFileException : public std::exception
    {
      public:
        explicit ReadFileException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class ShaderModuleCreationException : public std::exception
    {
      public:
        explicit ShaderModuleCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class WindowSurfaceCreationException : public std::exception
    {
      public:
        explicit WindowSurfaceCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class WindowInitializationException : public std::exception
    {
      public:
        explicit WindowInitializationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };

    class WindowCreationException : public std::exception
    {
      public:
        explicit WindowCreationException(const char* message) : msg(message) {}
        const char* what() const noexcept override { return msg; }

      private:
        const char* msg;
    };
} // namespace engine