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
  class MaterialRenderBindings
  {
  public:
    explicit MaterialRenderBindings(Device& device);
    ~MaterialRenderBindings();

    MaterialRenderBindings(const MaterialRenderBindings&)            = delete;
    MaterialRenderBindings& operator=(const MaterialRenderBindings&) = delete;

    void createResources();

    void beginFrame(int frameIndex);

    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }

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

    VkDescriptorSetLayout                   descriptorSetLayout_{VK_NULL_HANDLE};
    std::unique_ptr<engine::DescriptorPool> descriptorPool_;
    std::vector<VkDescriptorSet>            descriptorSets_;

    std::vector<std::unique_ptr<Buffer>> buffers_;
    std::vector<uint32_t>                dynamicOffsetIndexByFrame_;

    VkDeviceSize atomSize_{0};

    static constexpr uint32_t kMaterialSetIndex           = 4;
    static constexpr uint32_t kMaxMaterialRecordsPerFrame = 10000;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MATERIALRENDERBINDINGS_HPP
