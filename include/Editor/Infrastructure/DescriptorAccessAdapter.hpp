#pragma once

#include "Engine/Application/Ports/IDescriptorAccessPort.hpp"

namespace engine {

class EngineState;

// Adapter that bridges EngineState to the descriptor access port.
class DescriptorAccessAdapter final : public IDescriptorAccessPort {
 public:
  explicit DescriptorAccessAdapter(EngineState& engineState);

  [[nodiscard]] DescriptorPool& getDescriptorPool() override;
  [[nodiscard]] DescriptorSetLayout& getPostProcessSetLayout() override;
  [[nodiscard]] VkDescriptorSet getGbufferDescriptorSet(uint32_t frameIndex) override;
  [[nodiscard]] VkDescriptorSet getDeferredShadowDescriptorSet(uint32_t frameIndex) override;
  [[nodiscard]] VkDescriptorSet getDeferredIblDescriptorSet(uint32_t frameIndex) override;
  [[nodiscard]] VkDescriptorSet getPostProcessDescriptorSet(uint32_t frameIndex) override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
