#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class EngineState;
class Renderer;

class DepthPrepass : public IRenderPass {
 public:
  DepthPrepass(EngineState* engineState, Renderer& renderer) : engineState_(engineState), renderer_(renderer) {}

  void execute(FrameInfo& frameInfo) override;
  [[nodiscard]] const std::string& getName() const override {
    static std::string name = "DepthPrepass";
    return name;
  }

 private:
  EngineState* engineState_ = nullptr;
  Renderer& renderer_;
};

}  // namespace engine
