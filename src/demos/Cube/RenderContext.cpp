#include "RenderContext.hpp"

#include "3dEngine/SwapChain.hpp"

namespace engine {

  RenderContext::RenderContext(Device& device)
      : device_{device}, uboBuffers_(SwapChain::maxFramesInFlight()), globalDescriptorSets_(SwapChain::maxFramesInFlight())
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
                          .build();
  }

  void RenderContext::createGlobalSetLayout()
  {
    globalSetLayout_ = DescriptorSetLayout::Builder(device_).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).build();
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
      DescriptorWriter(*globalSetLayout_, *globalPool_).writeBuffer(0, &bufferInfo).build(globalDescriptorSets_[i]);
    }
  }

  void RenderContext::updateUBO(int frameIndex, const GlobalUbo& ubo)
  {
    uboBuffers_[frameIndex]->writeToBuffer(&ubo);
    uboBuffers_[frameIndex]->flush();
  }

  // Shadow descriptors removed - to be reimplemented later

} // namespace engine
