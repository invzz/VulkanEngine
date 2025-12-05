#pragma once

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

    void                  updateUBO(int frameIndex, const GlobalUbo& ubo);
    void                  updateHZBDescriptor(int frameIndex, VkDescriptorImageInfo hzbImageInfo);
    VkDescriptorSet       getGlobalDescriptorSet(int frameIndex) const { return globalDescriptorSets_[frameIndex]; }
    VkDescriptorSetLayout getGlobalSetLayout() const { return globalSetLayout_->getDescriptorSetLayout(); }

  private:
    Device&                              device_;
    MeshManager&                         meshManager_;
    std::unique_ptr<DescriptorPool>      globalPool_;
    std::unique_ptr<DescriptorSetLayout> globalSetLayout_;
    std::vector<std::unique_ptr<Buffer>> uboBuffers_;
    std::vector<VkDescriptorSet>         globalDescriptorSets_;

    void createDescriptorPool();
    void createGlobalSetLayout();
    void createUBOBuffers();
    void createGlobalDescriptorSets();
  };

} // namespace engine
