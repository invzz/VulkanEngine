#ifndef VULKANENGINE_SRC_DEMOS_CUBE_RENDERCONTEXT_HPP
#define VULKANENGINE_SRC_DEMOS_CUBE_RENDERCONTEXT_HPP

#include <memory>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Resources/MeshManager.hpp"

namespace engine {

  // Manages descriptor sets and uniform buffers for rendering
  class RenderContext
  {
  public:
    explicit RenderContext(Device& device, MeshManager& meshManager, VkDescriptorImageInfo hzbImageInfo);

    void updateUBO(int frameIndex, const GlobalUbo& ubo);
    // Upload dynamic light arrays (SSBO) for this frame and return counts.
    // Note: counts must be copied into GlobalUbo by the caller.
    struct LightCounts
    {
      int point       = 0;
      int directional = 0;
      int spot        = 0;
    };
    LightCounts updateLightBuffers(int frameIndex, Scene& scene);
    // Two global descriptor sets per frame:
    // - "prev" is used in early passes (shadow/depth-prepass) to avoid sampling the same-frame HZB before it exists.
    // - "current" is used in late passes (main scene) after HZB is generated.
    void                                updateHZBDescriptorPrev(int frameIndex, VkDescriptorImageInfo hzbImageInfo);
    void                                updateHZBDescriptorCurrent(int frameIndex, VkDescriptorImageInfo hzbImageInfo);
    [[nodiscard]] VkDescriptorSet       getGlobalDescriptorSet(int frameIndex) const { return globalDescriptorSets_[frameIndex]; }
    [[nodiscard]] VkDescriptorSet       getGlobalDescriptorSetCurrentHzb(int frameIndex) const { return globalDescriptorSetsCurrentHzb_[frameIndex]; }
    [[nodiscard]] VkDescriptorSetLayout getGlobalSetLayout() const { return globalSetLayout_->getDescriptorSetLayout(); }

  private:
    Device&                              device_;
    MeshManager&                         meshManager_;
    std::unique_ptr<DescriptorPool>      globalPool_;
    std::unique_ptr<DescriptorSetLayout> globalSetLayout_;
    std::vector<std::unique_ptr<Buffer>> uboBuffers_;

    // Dynamic light SSBOs (per frame)
    std::vector<std::unique_ptr<Buffer>> pointLightBuffers_;
    std::vector<std::unique_ptr<Buffer>> directionalLightBuffers_;
    std::vector<std::unique_ptr<Buffer>> spotLightBuffers_;
    size_t                               pointLightCapacity_       = 0;
    size_t                               directionalLightCapacity_ = 0;
    size_t                               spotLightCapacity_        = 0;

    std::vector<VkDescriptorSet> globalDescriptorSets_;
    std::vector<VkDescriptorSet> globalDescriptorSetsCurrentHzb_;

    void createDescriptorPool();
    void createGlobalSetLayout();
    void createUBOBuffers();
    void createLightBuffers(size_t pointCapacity, size_t directionalCapacity, size_t spotCapacity);
    void createGlobalDescriptorSets();
    void updateLightDescriptorSets(int frameIndex);
  };

} // namespace engine

#endif // VULKANENGINE_SRC_DEMOS_CUBE_RENDERCONTEXT_HPP
