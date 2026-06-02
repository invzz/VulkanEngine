#pragma once

#include "Engine/Application/Ports/IPostProcessingAccessPort.hpp"

namespace engine {

class EngineState;
class Device;

// Adapter that bridges EngineState to the post-processing access port.
class PostProcessingAccessAdapter final : public IPostProcessingAccessPort {
 public:
  explicit PostProcessingAccessAdapter(EngineState& engineState);

  void recreatePostProcessingSystem(Device& device,
                                    VkRenderPass renderPass,
                                    std::vector<VkDescriptorSetLayout> setLayouts) override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
