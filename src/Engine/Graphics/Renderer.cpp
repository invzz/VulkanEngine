#include "Engine/Graphics/Renderer.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Pipeline.hpp"

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine {

  Renderer::Renderer(Window& window, Device& device) : window{window}, device{device}
  {
    recreateSwapChain();
    createCommandBuffers();
  }

  Renderer::~Renderer()
  {
    freeCommandBuffers();
    vkDestroyPipeline(device.device(), hzbPipeline, nullptr);
    vkDestroyPipelineLayout(device.device(), hzbPipelineLayout, nullptr);
    vkDestroyDescriptorPool(device.device(), hzbDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device.device(), hzbSetLayout, nullptr);
  }

  void Renderer::createCommandBuffers()
  {
    commandBuffers.resize(SwapChain::maxFramesInFlight());

    if (VkCommandBufferAllocateInfo allocInfo{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = device.getCommandPool(),
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
        };
        vkAllocateCommandBuffers(device.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to allocate command buffers!");
    }
  }

  void Renderer::freeCommandBuffers()
  {
    vkFreeCommandBuffers(device.device(), device.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
  }

  void Renderer::recreateSwapChain()
  {
    VkExtent2D extent = window.getExtent();
    while (extent.width == 0 || extent.height == 0)
    {
      extent = window.getExtent();
      glfwWaitEvents();
    }

    vkDeviceWaitIdle(device.device());

    if (swapChain == nullptr)
    {
      swapChain = std::make_unique<SwapChain>(device, extent);
    }
    else
    {
      std::shared_ptr<SwapChain> oldSwapChain = std::move(swapChain);
      swapChain                               = std::make_unique<SwapChain>(device, extent, oldSwapChain);

      if (!oldSwapChain->compareSwapFormats(*swapChain))
      {
        throw SwapChainCreationException("Swap chain image or depth format has changed!");
      }
    }

    // Recreate offscreen resources to match new swapchain extent
    if (offscreenFrameBuffer)
    {
      offscreenFrameBuffer->resize(swapChain->getSwapChainExtent());
    }
    else
    {
      createOffscreenResources();
    }

    // Recreate HZB descriptors since image views changed
    if (hzbDescriptorPool != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device.device(), hzbDescriptorPool, nullptr);
      hzbDescriptorPool = VK_NULL_HANDLE;
      // Re-create pool and sets
      // We need to call createHZBPipeline logic again for descriptors?
      // Or just update them.
      // For simplicity, let's just destroy and recreate the pipeline/descriptors part that depends on images.
      // But pipeline itself doesn't depend on images.
      // Only descriptor sets do.

      // Actually, createHZBPipeline handles everything.
      // But we should separate descriptor creation.
      // For now, let's just call createHZBPipeline() again if we destroy everything?
      // No, that's wasteful.

      // Let's just clear the sets and re-allocate/write them.
      // But we need to know the mip levels.
      // The mip levels might change if extent changes.

      // So we should re-run the descriptor setup part of createHZBPipeline.
      // I'll refactor createHZBPipeline to handle updates or just call it here if I make it robust.
      // But createHZBPipeline creates layout and pipeline too.

      // Let's just destroy the pool and let the next call to generateDepthPyramid re-create?
      // No, generateDepthPyramid assumes they exist.

      // I'll implement a helper updateHZBDescriptors().
      // For now, I'll just call createHZBPipeline() which will check if things exist.
      // But I need to destroy the pool first.

      // Actually, createHZBPipeline is called in constructor.
      // I should call it here too to refresh descriptors.
      // But I need to destroy old descriptors first.

      // Let's make createHZBPipeline robust.
    }
    createHZBPipeline();

    // Transition all depth images to SHADER_READ_ONLY_OPTIMAL to avoid validation errors on first use
    VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();

    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      VkImageMemoryBarrier barrier{};
      barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                           = offscreenFrameBuffer->getDepthImage(i);
      barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      barrier.subresourceRange.baseMipLevel   = 0;
      barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount     = 1;
      barrier.srcAccessMask                   = 0;
      barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           0,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           1,
                           &barrier);
    }

    device.endSingleTimeCommands(commandBuffer);

    // TODO: recreate other resources dependent on swap chain (e.g.,
    // pipelines) the pipeline may not need to be recreated here if using
    // dynamic viewport/scissor
  }

  VkCommandBuffer Renderer::beginFrame()
  {
    assert(!isFrameStarted && "Can't call beginFrame while already in progress");

    uint32_t imageIndex;
    auto     result = swapChain->acquireNextImage(&imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
      recreateSwapChain();
      return nullptr;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
      throw SwapChainCreationException("failed to acquire swap chain image!");
    }

    currentImageIndex             = imageIndex;
    VkCommandBuffer commandBuffer = commandBuffers[currentFrameIndex];
    if (vkResetCommandBuffer(commandBuffer, /*flags=*/0) != VK_SUCCESS)
    {
      throw CommandBufferRecordingException("failed to reset command buffer!");
    }
    isFrameStarted = true;
    VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    if (auto beginCommandBufferResult = vkBeginCommandBuffer(commandBuffer, &beginInfo); beginCommandBufferResult != VK_SUCCESS)
    {
      throw CommandBufferRecordingException("failed to begin recording command buffer!");
    }
    return commandBuffer;
  }

  void Renderer::endFrame()
  {
    assert(isFrameStarted && "Can't call endFrame while frame not in progress");

    auto commandBuffer = getCurrentCommandBuffer();
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
      throw CommandBufferRecordingException("failed to record command buffer!");
    }

    if (auto result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
        result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasWindowResized())
    {
      window.resetWindowResizedFlag();
      recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
      throw SwapChainCreationException("failed to present swap chain image!");
    }

    isFrameStarted    = false;
    currentFrameIndex = (currentFrameIndex + 1) % SwapChain::maxFramesInFlight();
  }

  void Renderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different "
                                                         "frame");

    VkClearValue clearValues[] = {
            {.color = {0.0f, 0.0f, 0.0f, 1.0f}},
            {.depthStencil = {1.0f, 0}},
    };

    VkRenderPassBeginInfo renderPassInfo{
            .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass  = swapChain->getRenderPass(),
            .framebuffer = swapChain->getFrameBuffer(currentImageIndex),
            .renderArea =
                    {
                            .offset = {0, 0},
                            .extent = swapChain->getSwapChainExtent(),
                    },
            .clearValueCount = static_cast<uint32_t>(std::size(clearValues)),
            .pClearValues    = clearValues,
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(swapChain->getSwapChainExtent().width),
            .height   = static_cast<float>(swapChain->getSwapChainExtent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{
            .offset = {0, 0},
            .extent = swapChain->getSwapChainExtent(),
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) const
  {
    assert(isFrameStarted && "Can't end render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on a command buffer from a different "
                                                         "frame");
    vkCmdEndRenderPass(commandBuffer);
  }

  void Renderer::createOffscreenResources()
  {
    offscreenFrameBuffer = std::make_unique<FrameBuffer>(device, swapChain->getSwapChainExtent(), SwapChain::maxFramesInFlight(), true);
  }

  void Renderer::beginOffscreenRenderPass(VkCommandBuffer commandBuffer)
  {
    assert(isFrameStarted && "Can't begin render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

    offscreenFrameBuffer->beginRenderPass(commandBuffer, currentFrameIndex);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapChain->getSwapChainExtent().width);
    viewport.height   = static_cast<float>(swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  }

  void Renderer::endOffscreenRenderPass(VkCommandBuffer commandBuffer) const
  {
    assert(isFrameStarted && "Can't end render pass when frame not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on a command buffer from a different frame");
    offscreenFrameBuffer->endRenderPass(commandBuffer);
  }

  VkDescriptorImageInfo Renderer::getOffscreenImageInfo(int index) const
  {
    return offscreenFrameBuffer->getDescriptorImageInfo(index);
  }

  VkDescriptorImageInfo Renderer::getDepthImageInfo(int index) const
  {
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = offscreenFrameBuffer->getDepthImageView(index);
    info.sampler     = offscreenFrameBuffer->getDepthSampler();
    return info;
  }

  void Renderer::generateOffscreenMipmaps(VkCommandBuffer commandBuffer)
  {
    offscreenFrameBuffer->generateMipmaps(commandBuffer, currentFrameIndex);
  }

  void Renderer::createHZBPipeline()
  {
    // 1. Create Descriptor Set Layout
    if (hzbSetLayout == VK_NULL_HANDLE)
    {
      VkDescriptorSetLayoutBinding bindings[2] = {};
      // Binding 0: Input Depth (Sampler)
      bindings[0].binding            = 0;
      bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bindings[0].descriptorCount    = 1;
      bindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[0].pImmutableSamplers = nullptr;

      // Binding 1: Output Depth (Storage Image)
      bindings[1].binding            = 1;
      bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      bindings[1].descriptorCount    = 1;
      bindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[1].pImmutableSamplers = nullptr;

      VkDescriptorSetLayoutCreateInfo layoutInfo{};
      layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      layoutInfo.bindingCount = 2;
      layoutInfo.pBindings    = bindings;

      if (vkCreateDescriptorSetLayout(device.device(), &layoutInfo, nullptr, &hzbSetLayout) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create HZB descriptor set layout!");
      }
    }

    // 2. Create Pipeline
    if (hzbPipeline == VK_NULL_HANDLE)
    {
#ifdef SHADER_PATH
      std::string shaderPath = std::string(SHADER_PATH) + "/hiz_generate.comp.spv";
#else
      std::string shaderPath = "assets/shaders/compiled/hiz_generate.comp.spv";
#endif
      auto computeShaderCode = Pipeline::readFile(shaderPath);

      VkShaderModule computeShaderModule;

      VkShaderModuleCreateInfo createInfo{};
      createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      createInfo.codeSize = computeShaderCode.size();
      createInfo.pCode    = reinterpret_cast<const uint32_t*>(computeShaderCode.data());

      if (vkCreateShaderModule(device.device(), &createInfo, nullptr, &computeShaderModule) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create shader module!");
      }

      VkPipelineShaderStageCreateInfo shaderStageInfo{};
      shaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      shaderStageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
      shaderStageInfo.module = computeShaderModule;
      shaderStageInfo.pName  = "main";

      VkPushConstantRange pushConstantRange{};
      pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      pushConstantRange.offset     = 0;
      pushConstantRange.size       = sizeof(uint32_t);

      VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
      pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      pipelineLayoutInfo.setLayoutCount         = 1;
      pipelineLayoutInfo.pSetLayouts            = &hzbSetLayout;
      pipelineLayoutInfo.pushConstantRangeCount = 1;
      pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

      if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &hzbPipelineLayout) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create HZB pipeline layout!");
      }

      VkComputePipelineCreateInfo pipelineInfo{};
      pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
      pipelineInfo.layout = hzbPipelineLayout;
      pipelineInfo.stage  = shaderStageInfo;

      if (vkCreateComputePipelines(device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &hzbPipeline) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create HZB compute pipeline!");
      }

      vkDestroyShaderModule(device.device(), computeShaderModule, nullptr);
    }

    // 3. Create Descriptor Pool and Sets
    // We need sets for each frame and each mip transition.
    // Mip levels can be retrieved from offscreenFrameBuffer (assuming it's used for depth)
    // Wait, we need to know which framebuffer has the depth.
    // Assuming offscreenFrameBuffer is used.
    if (!offscreenFrameBuffer) return;

    // Get mip levels from framebuffer (we need to expose it or calculate it)
    // FrameBuffer calculates mipLevels based on extent.
    VkExtent2D extent    = swapChain->getSwapChainExtent();
    uint32_t   mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;

    // We need sets for mipLevels transitions per frame.
    // Pass 0: Depth -> HZB0 (Copy)
    // Pass 1..N: HZB(k-1) -> HZB(k) (Reduce)
    uint32_t setsPerFrame = mipLevels;
    if (setsPerFrame == 0) return;

    uint32_t totalSets = setsPerFrame * SwapChain::maxFramesInFlight();

    std::cout << "HZB Setup: MipLevels=" << mipLevels << ", SetsPerFrame=" << setsPerFrame << ", TotalSets=" << totalSets << std::endl;

    if (hzbDescriptorPool == VK_NULL_HANDLE)
    {
      VkDescriptorPoolSize poolSizes[2] = {};
      poolSizes[0].type                 = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      poolSizes[0].descriptorCount      = totalSets;
      poolSizes[1].type                 = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      poolSizes[1].descriptorCount      = totalSets;

      VkDescriptorPoolCreateInfo poolInfo{};
      poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      poolInfo.poolSizeCount = 2;
      poolInfo.pPoolSizes    = poolSizes;
      poolInfo.maxSets       = totalSets;

      if (vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &hzbDescriptorPool) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create HZB descriptor pool!");
      }
    }

    // Allocate and Update Sets
    hzbDescriptorSets.resize(SwapChain::maxFramesInFlight());
    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      hzbDescriptorSets[i].resize(setsPerFrame);

      std::vector<VkDescriptorSetLayout> layouts(setsPerFrame, hzbSetLayout);
      VkDescriptorSetAllocateInfo        allocInfo{};
      allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocInfo.descriptorPool     = hzbDescriptorPool;
      allocInfo.descriptorSetCount = setsPerFrame;
      allocInfo.pSetLayouts        = layouts.data();

      VkResult allocResult = vkAllocateDescriptorSets(device.device(), &allocInfo, hzbDescriptorSets[i].data());
      if (allocResult != VK_SUCCESS)
      {
        throw std::runtime_error("failed to allocate HZB descriptor sets! Error: " + std::to_string(allocResult));
      }

      // Update sets for each mip transition
      for (uint32_t m = 0; m < setsPerFrame; m++)
      {
        VkImageView inputView;
        VkImageView outputView;
        VkSampler   inputSampler;

        if (m == 0)
        {
          // Pass 0: Depth -> HZB0
          inputView    = offscreenFrameBuffer->getDepthImageView(i);
          outputView   = offscreenFrameBuffer->getHzbMipImageView(i, 0);
          inputSampler = offscreenFrameBuffer->getDepthSampler();
        }
        else
        {
          // Pass m: HZB(m-1) -> HZB(m)
          inputView    = offscreenFrameBuffer->getHzbMipImageView(i, m - 1);
          outputView   = offscreenFrameBuffer->getHzbMipImageView(i, m);
          inputSampler = offscreenFrameBuffer->getHzbSampler();
        }

        if (inputView == VK_NULL_HANDLE || outputView == VK_NULL_HANDLE)
        {
          std::cout << "ERROR: Null View Handle! Frame " << i << ", Mip " << m << ". Input: " << inputView << ", Output: " << outputView << std::endl;
        }

        // Input
        VkDescriptorImageInfo inputInfo{};
        inputInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        inputInfo.imageView   = inputView;
        inputInfo.sampler     = inputSampler;

        // Output
        VkDescriptorImageInfo outputInfo{};
        outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputInfo.imageView   = outputView;
        outputInfo.sampler     = VK_NULL_HANDLE;

        // std::cout << "Updating Set: Frame " << i << ", Mip " << m << ". InputView: " << inputInfo.imageView << ", OutputView: " << outputInfo.imageView <<
        // std::endl;

        VkWriteDescriptorSet descriptorWrites[2] = {};
        descriptorWrites[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet               = hzbDescriptorSets[i][m];
        descriptorWrites[0].dstBinding           = 0;
        descriptorWrites[0].dstArrayElement      = 0;
        descriptorWrites[0].descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount      = 1;
        descriptorWrites[0].pImageInfo           = &inputInfo;

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = hzbDescriptorSets[i][m];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo      = &outputInfo;

        vkUpdateDescriptorSets(device.device(), 2, descriptorWrites, 0, nullptr);
      }
    }
  }

  void Renderer::generateDepthPyramid(VkCommandBuffer commandBuffer)
  {
    if (!offscreenFrameBuffer) return;

    VkExtent2D extent    = swapChain->getSwapChainExtent();
    uint32_t   mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
    if (mipLevels < 2) return;

    // 1. Transition Depth Mip 0 to SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier depthBarrier{};
    depthBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depthBarrier.image                           = offscreenFrameBuffer->getDepthImage(currentFrameIndex);
    depthBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthBarrier.subresourceRange.baseArrayLayer = 0;
    depthBarrier.subresourceRange.layerCount     = 1;
    depthBarrier.subresourceRange.baseMipLevel   = 0;
    depthBarrier.subresourceRange.levelCount     = 1;
    depthBarrier.oldLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthBarrier.srcAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &depthBarrier);

    // 2. Transition HZB Mips to GENERAL (for writing)
    VkImageMemoryBarrier hzbBarrier{};
    hzbBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    hzbBarrier.image                           = offscreenFrameBuffer->getHzbImage(currentFrameIndex);
    hzbBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    hzbBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    hzbBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    hzbBarrier.subresourceRange.baseArrayLayer = 0;
    hzbBarrier.subresourceRange.layerCount     = 1;
    hzbBarrier.subresourceRange.baseMipLevel   = 0;
    hzbBarrier.subresourceRange.levelCount     = mipLevels;
    hzbBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    hzbBarrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    hzbBarrier.srcAccessMask                   = 0;
    hzbBarrier.dstAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &hzbBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, hzbPipeline);

    int32_t mipWidth  = extent.width;
    int32_t mipHeight = extent.height;

    for (uint32_t i = 0; i < mipLevels; i++)
    {
      // Calculate dispatch size
      int32_t currentWidth  = std::max(1, mipWidth >> i);
      int32_t currentHeight = std::max(1, mipHeight >> i);

      // Push Constant: 0 for copy (Pass 0), 1 for reduce (Pass > 0)
      uint32_t mode = (i == 0) ? 0 : 1;
      vkCmdPushConstants(commandBuffer, hzbPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &mode);

      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, hzbPipelineLayout, 0, 1, &hzbDescriptorSets[currentFrameIndex][i], 0, nullptr);

      vkCmdDispatch(commandBuffer, (currentWidth + 31) / 32, (currentHeight + 31) / 32, 1);

      // Barrier: Wait for HZB Mip i write to finish before it becomes input for next iteration
      // Transition HZB Mip i from GENERAL (Write) to SHADER_READ_ONLY_OPTIMAL (Read)
      VkImageMemoryBarrier mipBarrier{};
      mipBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      mipBarrier.image                           = offscreenFrameBuffer->getHzbImage(currentFrameIndex);
      mipBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      mipBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      mipBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      mipBarrier.subresourceRange.baseArrayLayer = 0;
      mipBarrier.subresourceRange.layerCount     = 1;
      mipBarrier.subresourceRange.baseMipLevel   = i;
      mipBarrier.subresourceRange.levelCount     = 1;
      mipBarrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
      mipBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      mipBarrier.srcAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;
      mipBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           0,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           1,
                           &mipBarrier);
    }
  }

} // namespace engine
