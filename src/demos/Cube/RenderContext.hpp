#pragma once

#include <memory>
#include <vector>

#include "3dEngine/Buffer.hpp"
#include "3dEngine/Descriptors.hpp"
#include "3dEngine/Device.hpp"
#include "3dEngine/FrameInfo.hpp"

namespace engine {

  // Manages descriptor sets and uniform buffers for rendering
  class RenderContext
  {
  public:
    explicit RenderContext(Device& device);

    void                  updateUBO(int frameIndex, const GlobalUbo& ubo);
    VkDescriptorSet       getGlobalDescriptorSet(int frameIndex) const { return globalDescriptorSets_[frameIndex]; }
    VkDescriptorSetLayout getGlobalSetLayout() const { return globalSetLayout_->getDescriptorSetLayout(); }

  private:
    Device&                              device_;
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
