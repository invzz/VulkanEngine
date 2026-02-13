#pragma once

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class Renderer;
class PostProcessingSystem;
class UIManager;
class Camera;
class Scene;

class CompositionPass : public IRenderPass {
 public:
  // Renderer + camera stay external; other state comes from EngineState.
  CompositionPass(Renderer& renderer, EngineState* engineState, Camera& camera, Window& window)
      : renderer_(renderer), engineState_(engineState), camera_(camera), window_(window) {}

  void execute(FrameInfo& frameInfo) override;
  [[nodiscard]] const std::string& getName() const override {
    static std::string name = "Composition";
    return name;
  }

 private:
  Renderer& renderer_;
  EngineState* engineState_ = nullptr;
  Camera& camera_;
  Window& window_;
};

}  // namespace engine
