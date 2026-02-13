#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class EngineState;
class Renderer;

class HZBPass : public IRenderPass {
 public:
  HZBPass(EngineState* engineState, Renderer& renderer) : engineState_(engineState), renderer_(renderer) {}

  void execute(FrameInfo& frameInfo) override;
  [[nodiscard]] const std::string& getName() const override {
    static std::string name = "HZB";
    return name;
  }

 private:
  EngineState* engineState_ = nullptr;
  Renderer& renderer_;
};

}  // namespace engine
