#include "RenderContext.hpp"

#include "Engine/Graphics/SwapChain.hpp"

namespace engine {

  RenderContext::RenderContext(Device& device, MeshManager& meshManager, VkDescriptorImageInfo hzbImageInfo)
      : device_{device}, meshManager_{meshManager}, uboBuffers_(SwapChain::maxFramesInFlight()), globalDescriptorSets_(SwapChain::maxFramesInFlight())
  {
    createDescriptorPool();
    createGlobalSetLayout();
    createUBOBuffers();
    createGlobalDescriptorSets();

    // Initialize with dummy or provided HZB info
    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      updateHZBDescriptor(i, hzbImageInfo);
    }
  }

  void RenderContext::createDescriptorPool()
  {
    globalPool_ = DescriptorPool::Builder(device_)
                          .setMaxSets(SwapChain::maxFramesInFlight())
                          .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::maxFramesInFlight())
                          .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SwapChain::maxFramesInFlight())
                          .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight())
                          .build();
  }

  void RenderContext::createGlobalSetLayout()
  {
    globalSetLayout_ = DescriptorSetLayout::Builder(device_)
                               .addBinding(0,
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                               .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
                               .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TASK_BIT_EXT)
                               .build();
  }

  void RenderContext::createUBOBuffers()
  {
    for (auto& buffer : uboBuffers_)
    {
      buffer = std::make_unique<Buffer>(device_,
                                        sizeof(GlobalUbo),
                                        1,
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                        device_.getProperties().limits.minUniformBufferOffsetAlignment);
      buffer->map();
    }
  }

  void RenderContext::createGlobalDescriptorSets()
  {
    for (size_t i = 0; i < globalDescriptorSets_.size(); i++)
    {
      auto bufferInfo = uboBuffers_[i]->descriptorInfo();
      auto meshInfo   = meshManager_.getDescriptorInfo();

      // Binding 2 (HZB) will be updated later, but we need to write something or use updateHZBDescriptor
      // DescriptorWriter requires all bindings? No, it builds what is added.
      // But if we don't write binding 2, validation might complain if we use it.
      // We will update it immediately in constructor.

      DescriptorWriter(*globalSetLayout_, *globalPool_)
              .writeBuffer(0, &bufferInfo)
              .writeBuffer(1, &meshInfo)
              //.writeImage(2, ...) // We don't have image info here yet
              .build(globalDescriptorSets_[i]);
    }
  }

  void RenderContext::updateHZBDescriptor(int frameIndex, VkDescriptorImageInfo hzbImageInfo)
  {
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = globalDescriptorSets_[frameIndex];
    write.dstBinding      = 2;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &hzbImageInfo;

    vkUpdateDescriptorSets(device_.device(), 1, &write, 0, nullptr);
  }

  void RenderContext::updateUBO(int frameIndex, const GlobalUbo& ubo)
  {
    uboBuffers_[frameIndex]->writeToBuffer(&ubo);
    uboBuffers_[frameIndex]->flush();
  }

  // Shadow descriptors removed - to be reimplemented later

} // namespace engine
