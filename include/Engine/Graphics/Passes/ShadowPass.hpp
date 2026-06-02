#pragma once

#include "Engine/Application/StateViews/RenderingStateView.hpp"
#include "Engine/Application/StateViews/SceneRuntimeStateView.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class RenderContext;

class ShadowPass : public IRenderPass {
 public:
  ShadowPass(RenderingStateView rendering, SceneRuntimeStateView sceneRuntime, RenderContext* renderContext)
      : rendering_(rendering), sceneRuntime_(sceneRuntime), renderContext_(renderContext) {}

  void execute(FrameInfo& frameInfo) override;
  [[nodiscard]] const std::string& getName() const override {
    static std::string name = "Shadow";
    return name;
  }

 private:
  RenderingStateView rendering_;
  SceneRuntimeStateView sceneRuntime_;
  RenderContext* renderContext_ = nullptr;
};

}  // namespace engine
