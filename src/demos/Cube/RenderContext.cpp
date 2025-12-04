#include "RenderContext.hpp"

#include "Engine/Graphics/SwapChain.hpp"

namespace engine {

  RenderContext::RenderContext(Device& device, MeshManager& meshManager)
      : device_{device}, meshManager_{meshManager}, uboBuffers_(SwapChain::maxFramesInFlight()), globalDescriptorSets_(SwapChain::maxFramesInFlight())
  {
    createDescriptorPool();
    createGlobalSetLayout();
    createUBOBuffers();
    createGlobalDescriptorSets();
  }

  void RenderContext::createDescriptorPool()
  {
    globalPool_ = DescriptorPool::Builder(device_)
                          .setMaxSets(SwapChain::maxFramesInFlight())
                          .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::maxFramesInFlight())
                          .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SwapChain::maxFramesInFlight())
                          .build();
  }

  void RenderContext::createGlobalSetLayout()
  {
    globalSetLayout_ = DescriptorSetLayout::Builder(device_)
                               .addBinding(0,
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                               .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
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

      DescriptorWriter(*globalSetLayout_, *globalPool_).writeBuffer(0, &bufferInfo).writeBuffer(1, &meshInfo).build(globalDescriptorSets_[i]);
    }
  }

  void RenderContext::updateUBO(int frameIndex, const GlobalUbo& ubo)
  {
    uboBuffers_[frameIndex]->writeToBuffer(&ubo);
    uboBuffers_[frameIndex]->flush();
  }

  // Shadow descriptors removed - to be reimplemented later

} // namespace engine
