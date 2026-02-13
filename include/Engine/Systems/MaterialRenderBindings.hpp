#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MATERIALRENDERBINDINGS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MATERIALRENDERBINDINGS_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

struct FrameInfo;
struct PBRMaterial;

// Owns the per-frame material dynamic UBO + descriptor sets used by ModelRenderSystem.
class MaterialRenderBindings {
 public:
  explicit MaterialRenderBindings(Device& device);
  ~MaterialRenderBindings();

  MaterialRenderBindings(const MaterialRenderBindings&) = delete;
  MaterialRenderBindings& operator=(const MaterialRenderBindings&) = delete;

  void createResources();

  void beginFrame(int frameIndex);

  [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const {
    return descriptorSetLayout_;
  }

  // Quick validation: returns true if the per-frame material descriptor set for `frameIndex` is valid.
  [[nodiscard]] bool frameDescriptorSetValid(int frameIndex) const;

  // Test helpers: enable capture of descriptor handles observed during bind, and query captured values.
  void enableBindCapture(bool enable);
  [[nodiscard]] std::vector<VkDescriptorSet> getCapturedBinds() const;

  // Query the raw per-frame descriptor set handle (test-only).
  [[nodiscard]] VkDescriptorSet getFrameDescriptorSet(int frameIndex) const;

  // Writes a material record into the current frame's material buffer and binds the descriptor set
  // (including the dynamic offset) at the expected set index.
  void bindMaterial(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const PBRMaterial* material, float isSelected);

  [[nodiscard]] static bool needsFullVariant(FrameInfo const& frameInfo, const PBRMaterial* material);

 private:
  [[nodiscard]] VkDeviceSize materialAtomSize() const;

  void createDescriptorSetLayout();
  void createBuffers();
  void createPoolAndSets();
  void updateDescriptorSets();
  void writeAndBind(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const void* data, VkDeviceSize dataSize);

  Device& device_;

  VkDescriptorSetLayout descriptorSetLayout_{VK_NULL_HANDLE};
  std::unique_ptr<engine::DescriptorPool> descriptorPool_;
  std::vector<VkDescriptorSet> descriptorSets_;

  std::vector<std::unique_ptr<Buffer>> buffers_;
  std::vector<uint32_t> dynamicOffsetIndexByFrame_;

  VkDeviceSize atomSize_{0};

  // --- Test / diagnostic helpers ---
  // Protects access to the capture buffer used by unit tests.
  mutable std::mutex captureMutex_;
  bool captureEnabled_ = false;
  std::vector<VkDescriptorSet> capturedBinds_;

  // Protects allocation/write of dynamic offsets into the per-frame mapped
  // buffer when bindMaterial is invoked concurrently (defensive).
  mutable std::mutex allocMutex_;

  static constexpr uint32_t kMaterialSetIndex = 4;
  static constexpr uint32_t kMaxMaterialRecordsPerFrame = 10000;
};

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MATERIALRENDERBINDINGS_HPP
