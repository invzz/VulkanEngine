#pragma once
#include <exception>
#include <stdexcept>
#include <string>
namespace engine {

  /**
   * @class RuntimeException
   * @brief Generic runtime error used across the engine instead of
   * std::runtime_error
   */
  class RuntimeException : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  class SwapChainCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class ImageViewCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class RenderPassCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class FramebufferCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class InFlightFenceException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class SemaphoreCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class CommandBufferSubmissionException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class GraphicsPipelineCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class ReadFileException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class ShaderModuleCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class WindowSurfaceCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class CommandBufferRecordingException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class WindowInitializationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

  class WindowCreationException : public RuntimeException
  {
  public:
    using RuntimeException::RuntimeException;
  };

} // namespace engine
