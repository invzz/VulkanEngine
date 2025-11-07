
/**
 * @class engine::SwapChain
 * @brief Manages Vulkan swapchain, image views, framebuffers, render pass,
 * depth resources, and synchronization objects.
 *
 * The SwapChain class encapsulates all logic for creating and managing the
 * Vulkan swapchain and its associated resources. It handles:
 *   - Swapchain creation and destruction
 *   - Image view creation for swapchain images
 *   - Render pass setup
 *   - Depth buffer resources
 *   - Framebuffer creation
 *   - Synchronization primitives (semaphores, fences)
 *   - Image acquisition and presentation
 *   - Frame synchronization for multiple frames in flight
 *
 * Usage:
 *   - Construct with a valid Device and window extent
 *   - Call acquireNextImage() before rendering each frame
 *   - Call submitCommandBuffers() to submit rendering and present the image
 *
 * @note This class is designed for onboarding and learning Vulkan best
 * practices. All resource management is automatic.
 */

#include "3dEngine/SwapChain.hpp"

// std
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

#include "3dEngine/Exceptions.hpp"

namespace engine {

  SwapChain::SwapChain(Device& deviceRef, VkExtent2D extent) : device{deviceRef}, windowExtent{extent}
  {
    presentIdState.enabled = deviceRef.supportsPresentId();
    Init();
  }
  SwapChain::SwapChain(Device& deviceRef, VkExtent2D extent, std::shared_ptr<SwapChain> previous)
      : device{deviceRef}, windowExtent{extent}, oldSwapChain{previous}
  {
    presentIdState.enabled = deviceRef.supportsPresentId();
    Init();

    if (oldSwapChain != nullptr)
    {
      oldSwapChain = nullptr;
    }
  }

  SwapChain::~SwapChain()
  {
    for (auto imageView : swapChainImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }
    swapChainImageViews.clear();

    if (swapChain != nullptr)
    {
      vkDestroySwapchainKHR(device.device(), swapChain, nullptr);
      swapChain = nullptr;
    }

    for (int i = 0; i < depthImages.size(); i++)
    {
      vkDestroyImageView(device.device(), depthImageViews[i], nullptr);
      vkDestroyImage(device.device(), depthImages[i], nullptr);
      vkFreeMemory(device.device(), depthImageMemorys[i], nullptr);
    }

    for (auto framebuffer : swapChainFramebuffers)
    {
      vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
    }

    vkDestroyRenderPass(device.device(), renderPass, nullptr);

    // cleanup synchronization objects
    for (auto semaphore : renderFinishedSemaphores)
    {
      if (semaphore != VK_NULL_HANDLE)
      {
        vkDestroySemaphore(device.device(), semaphore, nullptr);
      }
    }
    for (auto semaphore : imageAvailableSemaphores)
    {
      if (semaphore != VK_NULL_HANDLE)
      {
        vkDestroySemaphore(device.device(), semaphore, nullptr);
      }
    }
    for (auto fence : inFlightFences)
    {
      if (fence != VK_NULL_HANDLE)
      {
        vkDestroyFence(device.device(), fence, nullptr);
      }
    }
  }

  VkResult SwapChain::acquireNextImage(uint32_t* imageIndex)
  {
    vkWaitForFences(device.device(), 1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

    // Use currentFrame for semaphore
    VkResult result = vkAcquireNextImageKHR(device.device(),
                                            swapChain,
                                            std::numeric_limits<uint64_t>::max(),
                                            imageAvailableSemaphores[currentFrame],
                                            VK_NULL_HANDLE,
                                            imageIndex);

    if ((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) && imagesInFlight[*imageIndex] != VK_NULL_HANDLE)
    {
      vkWaitForFences(device.device(), 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
    }

    return result;
  }

  VkResult SwapChain::submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex)
  {
    imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

    VkSubmitInfo submitInfo = {};
    submitInfo.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore          waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount         = 1;
    submitInfo.pWaitSemaphores            = waitSemaphores;
    submitInfo.pWaitDstStageMask          = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = buffers;

    VkSemaphore signalSemaphores[]  = {renderFinishedSemaphores[*imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    vkResetFences(device.device(), 1, &inFlightFences[currentFrame]);
    if (vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
    {
      throw CommandBufferSubmissionException("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount  = 1;
    presentInfo.pSwapchains     = swapChains;

    presentInfo.pImageIndices = imageIndex;

    VkPresentIdKHR presentIdInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR, .pNext = nullptr, .swapchainCount = 0, .pPresentIds = nullptr};
    uint64_t       presentIdValue = 0;

    if (presentIdState.enabled)
    {
      // Tag each present so validation can correlate semaphore ownership.
      presentIdValue               = presentIdState.next++;
      presentIdInfo.swapchainCount = 1;
      presentIdInfo.pPresentIds    = &presentIdValue;
      presentInfo.pNext            = &presentIdInfo;
    }

    auto result = vkQueuePresentKHR(device.presentQueue(), &presentInfo);

    currentFrame = (currentFrame + 1) % static_cast<size_t>(maxFramesInFlight());

    return result;
  }

  void SwapChain::Init()
  {
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDepthResources();
    createFramebuffers();
    createSyncObjects();
  }

  void SwapChain::createSwapChain()
  {
    SwapChainSupportDetails swapChainSupport = device.getSwapChainSupport();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D         extent        = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface                  = device.surface();

    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices              = device.findPhysicalQueueFamilies();
    uint32_t           queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

    if (indices.graphicsFamily != indices.presentFamily)
    {
      createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }
    else
    {
      createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 0;       // Optional
      createInfo.pQueueFamilyIndices   = nullptr; // Optional
    }

    // Ensure we use a supported transform; fall back to identity if needed.
    const VkSurfaceTransformFlagsKHR supportedTransforms = swapChainSupport.capabilities.supportedTransforms;
    VkSurfaceTransformFlagBitsKHR    preTransform        = swapChainSupport.capabilities.currentTransform;
    if (!(supportedTransforms & preTransform))
    {
      preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    createInfo.preTransform = preTransform;

    // Pick a composite alpha the surface supports, prefer opaque.
    VkCompositeAlphaFlagsKHR    supportedAlpha = swapChainSupport.capabilities.supportedCompositeAlpha;
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(supportedAlpha & compositeAlpha))
    {
      const VkCompositeAlphaFlagBitsKHR candidates[] = {VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                                        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                                                        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
      for (auto candidate : candidates)
      {
        if (supportedAlpha & candidate)
        {
          compositeAlpha = candidate;
          break;
        }
      }
    }
    createInfo.compositeAlpha = compositeAlpha;

    createInfo.presentMode  = presentMode;
    createInfo.clipped      = VK_TRUE;
    createInfo.oldSwapchain = oldSwapChain == nullptr ? VK_NULL_HANDLE : oldSwapChain->swapChain;

    std::cerr << "Swapchain create request:\n"
              << "  minImageCount: " << createInfo.minImageCount << '\n'
              << "  imageFormat: " << createInfo.imageFormat << '\n'
              << "  imageColorSpace: " << createInfo.imageColorSpace << '\n'
              << "  extent: " << createInfo.imageExtent.width << "x" << createInfo.imageExtent.height << '\n'
              << "  preTransform: " << createInfo.preTransform << '\n'
              << "  supportedTransforms: " << supportedTransforms << '\n'
              << "  compositeAlpha: " << createInfo.compositeAlpha << '\n'
              << "  presentMode: " << createInfo.presentMode << '\n'
              << "  imageUsage: " << createInfo.imageUsage << std::endl;

    if (VkResult createResult = vkCreateSwapchainKHR(device.device(), &createInfo, nullptr, &swapChain); createResult != VK_SUCCESS)
    {
      std::cerr << "vkCreateSwapchainKHR failed with VkResult " << static_cast<int32_t>(createResult) << std::endl;
      throw SwapChainCreationException("failed to create swap chain!");
    }

    // we only specified a minimum number of images in the swap chain, so
    // the implementation is allowed to create a swap chain with more.
    // That's why we'll first query the final number of images with
    // vkGetSwapchainImagesKHR, then resize the container and finally call
    // it again to retrieve the handles.
    uint32_t actualImageCount = 0;
    if (VkResult getImagesResult = vkGetSwapchainImagesKHR(device.device(), swapChain, &actualImageCount, nullptr); getImagesResult != VK_SUCCESS)
    {
      std::cerr << "First vkGetSwapchainImagesKHR failed with VkResult " << static_cast<int32_t>(getImagesResult) << std::endl;
      throw SwapChainCreationException("failed to query swap chain images!");
    }

    swapChainImages.resize(actualImageCount);
    if (VkResult getImagesResult = vkGetSwapchainImagesKHR(device.device(), swapChain, &actualImageCount, swapChainImages.data());
        getImagesResult != VK_SUCCESS)
    {
      std::cerr << "Second vkGetSwapchainImagesKHR failed with VkResult " << static_cast<int32_t>(getImagesResult) << std::endl;
      throw SwapChainCreationException("failed to retrieve swap chain images!");
    }

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent      = extent;
  }

  void SwapChain::createImageViews()
  {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = swapChainImages[i];
      viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format                          = swapChainImageFormat;
      viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel   = 0;
      viewInfo.subresourceRange.levelCount     = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &viewInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
      {
        throw ImageViewCreationException("failed to create texture image view!");
      }
    }
  }

  void SwapChain::createRenderPass()
  {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = findDepthFormat();
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format                  = getSwapChainImageFormat();
    colorAttachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout             = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment            = 0;
    colorAttachmentRef.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass    = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
    dependency.srcAccessMask       = 0;
    dependency.srcStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstSubpass          = 0;
    dependency.dstStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments    = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo                 renderPassInfo = {};
    renderPassInfo.sType                                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount                        = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments                           = attachments.data();
    renderPassInfo.subpassCount                           = 1;
    renderPassInfo.pSubpasses                             = &subpass;
    renderPassInfo.dependencyCount                        = 1;
    renderPassInfo.pDependencies                          = &dependency;

    if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
      throw RenderPassCreationException("failed to create render pass!");
    }
  }

  void SwapChain::createFramebuffers()
  {
    swapChainFramebuffers.resize(imageCount());
    for (size_t i = 0; i < imageCount(); i++)
    {
      std::array<VkImageView, 2> attachments = {swapChainImageViews[i], depthImageViews[i]};

      VkExtent2D              swce            = getSwapChainExtent();
      VkFramebufferCreateInfo framebufferInfo = {};
      framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass              = renderPass;
      framebufferInfo.attachmentCount         = static_cast<uint32_t>(attachments.size());
      framebufferInfo.pAttachments            = attachments.data();
      framebufferInfo.width                   = swce.width;
      framebufferInfo.height                  = swce.height;
      framebufferInfo.layers                  = 1;

      if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
      {
        throw FramebufferCreationException("failed to create framebuffer!");
      }
    }
  }

  void SwapChain::createDepthResources()
  {
    VkFormat depthFormat = findDepthFormat();

    swapChainDepthFormat = depthFormat;

    VkExtent2D swce = getSwapChainExtent();

    depthImages.resize(imageCount());
    depthImageMemorys.resize(imageCount());
    depthImageViews.resize(imageCount());

    for (int i = 0; i < depthImages.size(); i++)
    {
      VkImageCreateInfo imageInfo{};
      imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageInfo.imageType     = VK_IMAGE_TYPE_2D;
      imageInfo.extent.width  = swce.width;
      imageInfo.extent.height = swce.height;
      imageInfo.extent.depth  = 1;
      imageInfo.mipLevels     = 1;
      imageInfo.arrayLayers   = 1;
      imageInfo.format        = depthFormat;
      imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
      imageInfo.flags         = 0;

      device.memory().createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImages[i], depthImageMemorys[i]);

      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = depthImages[i];
      viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format                          = depthFormat;
      viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      viewInfo.subresourceRange.baseMipLevel   = 0;
      viewInfo.subresourceRange.levelCount     = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS)
      {
        throw ImageViewCreationException("failed to create texture image view!");
      }
    }
  }

  void SwapChain::createSyncObjects()
  {
    const auto frameCount = static_cast<size_t>(maxFramesInFlight());
    imageAvailableSemaphores.assign(frameCount, VK_NULL_HANDLE);
    inFlightFences.assign(frameCount, VK_NULL_HANDLE);
    renderFinishedSemaphores.assign(imageCount(), VK_NULL_HANDLE);
    imagesInFlight.assign(imageCount(), VK_NULL_HANDLE);
    if (presentIdState.enabled)
    {
      presentIdState.next = 1;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& semaphore : imageAvailableSemaphores)
    {
      if (vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
      {
        throw SemaphoreCreationException("failed to create image-available semaphore!");
      }
    }

    for (auto& semaphore : renderFinishedSemaphores)
    {
      if (vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
      {
        throw SemaphoreCreationException("failed to create render-finished semaphore!");
      }
    }

    for (auto& fence : inFlightFences)
    {
      if (vkCreateFence(device.device(), &fenceInfo, nullptr, &fence) != VK_SUCCESS)
      {
        throw InFlightFenceException("failed to create in-flight fence!");
      }
    }
  }

  VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const
  {
    for (const auto& availableFormat : availableFormats)
    {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      {
        return availableFormat;
      }
    }

    return availableFormats[0];
  }

  VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const
  {
    for (const auto& availablePresentMode : availablePresentModes)
    {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
      {
        std::cout << "Present mode: FIFO Relaxed" << std::endl;
        return availablePresentMode;
      }
    }

    std::cout << "Present mode: V-Sync" << std::endl;
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
  {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
      return capabilities.currentExtent;
    }
    else
    {
      VkExtent2D actualExtent = windowExtent;
      actualExtent.width      = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
      actualExtent.height     = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
      return actualExtent;
    }
  }

  VkFormat SwapChain::findDepthFormat()
  {
    return device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                                      VK_IMAGE_TILING_OPTIMAL,
                                      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  }

} // namespace engine
