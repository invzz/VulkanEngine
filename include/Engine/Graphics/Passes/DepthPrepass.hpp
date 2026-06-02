#pragma once

#include "Engine/Application/StateViews/RenderingStateView.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class Renderer;

class DepthPrepass : public IRenderPass {
 public:
  DepthPrepass(RenderingStateView rendering, Renderer& renderer)
      : rendering_(rendering), renderer_(renderer) {}

  void execute(FrameInfo& frameInfo) override;
  [[nodiscard]] const std::string& getName() const override {
    static std::string name = "DepthPrepass";
    return name;
  }

 private:
  RenderingStateView rendering_;
  Renderer& renderer_;
};

}  // namespace engine
