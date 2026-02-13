#pragma once

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class Renderer;
class ModelRenderSystem;
class DeferredLightingSystem;
class ShadowSystem;
class DustRenderSystem;
class RenderContext;
class Device;
class DescriptorPool;
class DescriptorSetLayout;

class OffscreenPass : public IRenderPass {
 public:
  // Renderer remains external; all other resources are read from EngineState.
  OffscreenPass(Renderer& renderer, EngineState* engineState, Device& device, int& debugMode)
      : renderer_(renderer), engineState_(engineState), device_(device), debugMode_(debugMode) {}

  void execute(FrameInfo& frameInfo) override;
  [[nodiscard]] const std::string& getName() const override {
    static std::string name = "Offscreen";
    return name;
  }

 private:
  Renderer& renderer_;
  EngineState* engineState_ = nullptr;
  Device& device_;
  int& debugMode_;
};

}  // namespace engine
