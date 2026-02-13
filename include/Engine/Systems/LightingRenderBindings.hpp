#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LIGHTINGRENDERBINDINGS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LIGHTINGRENDERBINDINGS_HPP

#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

struct FrameInfo;
class ShadowSystem;
class IBLSystem;

// Owns the per-frame descriptor sets for lighting-related resources used by ModelRenderSystem
// (shadow maps + IBL textures).
class LightingRenderBindings {
 public:
  explicit LightingRenderBindings(Device& device);
  ~LightingRenderBindings();

  LightingRenderBindings(const LightingRenderBindings&) = delete;
  LightingRenderBindings& operator=(const LightingRenderBindings&) = delete;

  void createResources();

  void setShadowSystem(ShadowSystem* shadowSystem);
  void setIBLSystem(IBLSystem* iblSystem);

  [[nodiscard]] VkDescriptorSetLayout getShadowDescriptorSetLayout() const {
    return shadowDescriptorSetLayout_;
  }
  [[nodiscard]] VkDescriptorSetLayout getIBLDescriptorSetLayout() const {
    return iblDescriptorSetLayout_;
  }

  void bindShadow(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout);
  void bindIBL(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout);

 private:
  void createShadowDescriptorResources();
  void createIBLDescriptorResources();

  Device& device_;

  ShadowSystem* shadowSystem_{nullptr};
  IBLSystem* iblSystem_{nullptr};

  VkDescriptorSetLayout shadowDescriptorSetLayout_{VK_NULL_HANDLE};
  VkDescriptorPool shadowDescriptorPool_{VK_NULL_HANDLE};
  std::vector<VkDescriptorSet> shadowDescriptorSets_;

  VkDescriptorSetLayout iblDescriptorSetLayout_{VK_NULL_HANDLE};
  VkDescriptorPool iblDescriptorPool_{VK_NULL_HANDLE};
  std::vector<VkDescriptorSet> iblDescriptorSets_;

  static constexpr uint32_t kShadowSetIndex = 2;
  static constexpr uint32_t kIBLSetIndex = 3;
};

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LIGHTINGRENDERBINDINGS_HPP
