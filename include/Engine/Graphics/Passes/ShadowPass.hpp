#pragma once

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class ShadowSystem;
class RenderContext;

class ShadowPass : public IRenderPass {
 public:
  // Accept the central EngineState so the pass can access systems/settings.
  explicit ShadowPass(EngineState* engineState) : engineState_(engineState) {}

  void execute(FrameInfo& frameInfo) override;
  [[nodiscard]] const std::string& getName() const override {
    static std::string name = "Shadow";
    return name;
  }

 private:
  EngineState* engineState_ = nullptr;
};

}  // namespace engine
