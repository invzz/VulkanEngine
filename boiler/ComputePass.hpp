#ifndef VULKAN_ENGINE_COMPUTE_PASS_HPP
#define VULKAN_ENGINE_COMPUTE_PASS_HPP

#include <string>

#include "Engine/Graphics/RenderGraph.hpp"
#include "SystemRegistry.hpp"

namespace engine {
class ComputePass : public IRenderPass {
 public:
  ComputePass(SystemRegistry& systems) : systems(systems) {}

  [[nodiscard]] const std::string& getName() const override {
    static const std::string name = "Compute";
    return name;
  }

  void execute(FrameInfo& frameInfo) override {
    systems.animation().update(frameInfo);
  }

 private:
  SystemRegistry& systems;
};
}  // namespace engine

#endif  // VULKAN_ENGINE_COMPUTE_PASS_HPP