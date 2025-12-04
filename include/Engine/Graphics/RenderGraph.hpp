#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {

  class RenderPass
  {
  public:
    RenderPass(const std::string& name) : name(name) {}
    virtual ~RenderPass() = default;

    virtual void execute(FrameInfo& frameInfo) = 0;

    const std::string& getName() const { return name; }

  protected:
    std::string name;
  };

  class RenderGraph
  {
  public:
    void addPass(std::unique_ptr<RenderPass> pass);
    void execute(FrameInfo& frameInfo);
    void reset();

  private:
    std::vector<std::unique_ptr<RenderPass>> passes;
  };

  // A generic pass that executes a lambda
  class LambdaRenderPass : public RenderPass
  {
  public:
    using Callback = std::function<void(FrameInfo&)>;

    LambdaRenderPass(const std::string& name, Callback callback) : RenderPass(name), callback(callback) {}

    void execute(FrameInfo& frameInfo) override
    {
      if (callback) callback(frameInfo);
    }

  private:
    Callback callback;
  };

} // namespace engine
