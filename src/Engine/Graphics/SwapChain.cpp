
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

#include "Engine/Graphics/SwapChain.hpp"

// std
#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"

#include "vulkan/vulkan_core.h"

namespace engine {

    SwapChain::SwapChain(Device& deviceRef, VkExtent2D extent) : device{deviceRef}, windowExtent{extent} {
        presentIdState.enabled = deviceRef.supportsPresentId();
        Init();
    }
    SwapChain::SwapChain(Device& deviceRef, VkExtent2D extent, std::shared_ptr<SwapChain> previous) : device{deviceRef}, windowExtent{extent}, oldSwapChain{std::move(previous)} {
        presentIdState.enabled = deviceRef.supportsPresentId();
        Init();

        // Keep `oldSwapChain` alive until the new swap chain safely takes over.
        // Do NOT null it here; higher-level code will wait on old fences and release it.
    }

    SwapChain::~SwapChain() {
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device.device(), imageView, nullptr);
        }
        swapChainImageViews.clear();

        if (swapChain != nullptr) {
            vkDestroySwapchainKHR(device.device(), swapChain, nullptr);
            swapChain = nullptr;
        }

        for (size_t i = 0; i < depthImages.size(); i++) {
            vkDestroyImageView(device.device(), depthImageViews[i], nullptr);
            vkDestroyImage(device.device(), depthImages[i], nullptr);
            vkFreeMemory(device.device(), depthImageMemorys[i], nullptr);
        }

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }

        vkDestroyRenderPass(device.device(), renderPass, nullptr);

        // Cleanup synchronization objects
        for (size_t i = 0; i < inFlightFences.size(); ++i) {
            vkDestroySemaphore(device.device(), imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device.device(), renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device.device(), inFlightFences[i], nullptr);
        }
    }

    VkResult SwapChain::acquireNextImage(uint32_t* imageIndex) {
        device.setCurrentFrameIndex(static_cast<uint32_t>(currentFrame));

        // Wait for this frame's fence before reusing its resources
        vkWaitForFences(device.device(), 1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

        // Safe point: the in-flight fence for this frame index has been waited.
        // Destroy any resources deferred for this frame index.
        device.flushDeferred(static_cast<uint32_t>(currentFrame));

        return vkAcquireNextImageKHR(device.device(), swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, imageIndex);
    }

    VkResult SwapChain::submitCommandBuffers(const VkCommandBuffer* buffers, const uint32_t* imageIndex) {
        VkSemaphore const    waitSemaphores[]   = {imageAvailableSemaphores[currentFrame]};
        VkSemaphore const    signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[]       = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo submitInfo{
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = waitSemaphores,
            .pWaitDstStageMask    = waitStages,
            .commandBufferCount   = 1,
            .pCommandBuffers      = buffers,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = signalSemaphores,
        };

        vkResetFences(device.device(), 1, &inFlightFences[currentFrame]);

        VkResult const submitResult = device.submitGraphics(&submitInfo, inFlightFences[currentFrame]);
        if (submitResult != VK_SUCCESS) {
            return submitResult;
        }

        VkPresentInfoKHR presentInfo{
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = signalSemaphores,
            .swapchainCount     = 1,
            .pSwapchains        = &swapChain,
            .pImageIndices      = imageIndex,
        };

        VkPresentIdKHR presentIdInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR};
        uint64_t       presentIdValue = 0;

        if (presentIdState.enabled) {
            presentIdValue               = presentIdState.next++;
            presentIdInfo.swapchainCount = 1;
            presentIdInfo.pPresentIds    = &presentIdValue;
            presentInfo.pNext            = &presentIdInfo;
        }

        auto result = device.present(&presentInfo);

        currentFrame = (currentFrame + 1) % static_cast<size_t>(maxFramesInFlight());

        return result;
    }

    void SwapChain::Init() {
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDepthResources();
        createFramebuffers();
        createSyncObjects();
    }

    bool SwapChain::waitForInFlightFences(uint64_t timeoutNs) const {
        if (inFlightFences.empty())
            return true;

        for (auto fence : inFlightFences) {
            if (fence == VK_NULL_HANDLE)
                continue;
            VkResult const r = vkWaitForFences(device.device(), 1, &fence, VK_TRUE, timeoutNs);
            if (r != VK_SUCCESS) {
                Logger::warn(LogChannel::Sync, "waitForInFlightFences: fence wait failed or timed out (result=", r, ")");
                return false;
            }
        }
        return true;
    }

    void SwapChain::createSwapChain() {
        SwapChainSupportDetails const swapChainSupport = device.getSwapChainSupport();

        VkSurfaceFormatKHR const surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR const   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D const         extent        = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
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

        QueueFamilyIndices const indices              = device.findPhysicalQueueFamilies();
        uint32_t                 queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;        // Optional
            createInfo.pQueueFamilyIndices   = nullptr;  // Optional
        }

        // Ensure we use a supported transform; fall back to identity if needed.
        const VkSurfaceTransformFlagsKHR supportedTransforms = swapChainSupport.capabilities.supportedTransforms;
        VkSurfaceTransformFlagBitsKHR    preTransform        = swapChainSupport.capabilities.currentTransform;
        if ((supportedTransforms & preTransform) == 0u) {
            preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        }
        createInfo.preTransform = preTransform;

        // Pick a composite alpha the surface supports, prefer opaque.
        VkCompositeAlphaFlagsKHR const supportedAlpha = swapChainSupport.capabilities.supportedCompositeAlpha;
        VkCompositeAlphaFlagBitsKHR    compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if ((supportedAlpha & compositeAlpha) == 0u) {
            const VkCompositeAlphaFlagBitsKHR candidates[] = {VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
            for (auto candidate : candidates) {
                if ((supportedAlpha & candidate) != 0u) {
                    compositeAlpha = candidate;
                    break;
                }
            }
        }
        createInfo.compositeAlpha = compositeAlpha;

        createInfo.presentMode  = presentMode;
        createInfo.clipped      = VK_TRUE;
        createInfo.oldSwapchain = oldSwapChain == nullptr ? VK_NULL_HANDLE : oldSwapChain->swapChain;
#ifdef SWAPCHAIN_DEBUG
        Logger::debug(LogChannel::Render, "Swapchain create request:");
        Logger::debug(LogChannel::Render, "  minImageCount: ", createInfo.minImageCount);
        Logger::debug(LogChannel::Render, "  imageFormat: ", createInfo.imageFormat);
        Logger::debug(LogChannel::Render, "  imageColorSpace: ", createInfo.imageColorSpace);
        Logger::debug(LogChannel::Render, "  extent: ", createInfo.imageExtent.width, "x", createInfo.imageExtent.height);
        Logger::debug(LogChannel::Render, "  preTransform: ", createInfo.preTransform);
        Logger::debug(LogChannel::Render, "  supportedTransforms: ", supportedTransforms);
        Logger::debug(LogChannel::Render, "  compositeAlpha: ", createInfo.compositeAlpha);
        Logger::debug(LogChannel::Render, "  presentMode: ", createInfo.presentMode);
        Logger::debug(LogChannel::Render, "  imageUsage: ", createInfo.imageUsage);
#endif
        if (VkResult const createResult = vkCreateSwapchainKHR(device.device(), &createInfo, nullptr, &swapChain); createResult != VK_SUCCESS) {
            Logger::error(LogChannel::Render, "vkCreateSwapchainKHR failed with VkResult ", static_cast<int32_t>(createResult));
            throw SwapChainCreationException("failed to create swap chain!");
        }

        // we only specified a minimum number of images in the swap chain, so
        // the implementation is allowed to create a swap chain with more.
        // That's why we'll first query the final number of images with
        // vkGetSwapchainImagesKHR, then resize the container and finally call
        // it again to retrieve the handles.
        uint32_t actualImageCount = 0;
        if (VkResult const getImagesResult = vkGetSwapchainImagesKHR(device.device(), swapChain, &actualImageCount, nullptr); getImagesResult != VK_SUCCESS) {
            Logger::error(LogChannel::Render, "First vkGetSwapchainImagesKHR failed with VkResult ", static_cast<int32_t>(getImagesResult));
            throw SwapChainCreationException("failed to query swap chain images!");
        }

        swapChainImages.resize(actualImageCount);
        if (VkResult const getImagesResult = vkGetSwapchainImagesKHR(device.device(), swapChain, &actualImageCount, swapChainImages.data()); getImagesResult != VK_SUCCESS) {
            Logger::error(LogChannel::Render, "Second vkGetSwapchainImagesKHR failed with VkResult ", static_cast<int32_t>(getImagesResult));
            throw SwapChainCreationException("failed to retrieve swap chain images!");
        }

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent      = extent;
    }

    void SwapChain::createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
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

            if (vkCreateImageView(device.device(), &viewInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw ImageViewCreationException("failed to create texture image view!");
            }
        }
    }

    void SwapChain::createRenderPass() {
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
        colorAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;  // Preserve viewport content
        colorAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.initialLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Must match present layout when using LOAD
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

        if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw RenderPassCreationException("failed to create render pass!");
        }
    }

    void SwapChain::createFramebuffers() {
        swapChainFramebuffers.resize(imageCount());
        for (size_t i = 0; i < imageCount(); i++) {
            std::array<VkImageView, 2> attachments = {swapChainImageViews[i], depthImageViews[i]};

            VkExtent2D const        swce            = getSwapChainExtent();
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass              = renderPass;
            framebufferInfo.attachmentCount         = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments            = attachments.data();
            framebufferInfo.width                   = swce.width;
            framebufferInfo.height                  = swce.height;
            framebufferInfo.layers                  = 1;

            if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw FramebufferCreationException("failed to create framebuffer!");
            }
        }
    }

    void SwapChain::createDepthResources() {
        VkFormat const depthFormat = findDepthFormat();

        swapChainDepthFormat = depthFormat;

        VkExtent2D const swce = getSwapChainExtent();

        depthImages.resize(imageCount());
        depthImageMemorys.resize(imageCount());
        depthImageViews.resize(imageCount());

        for (int i = 0; i < depthImages.size(); i++) {
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

            if (vkCreateImageView(device.device(), &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS) {
                throw ImageViewCreationException("failed to create texture image view!");
            }
        }
    }

    void SwapChain::createSyncObjects() {
        const auto frameCount = static_cast<size_t>(maxFramesInFlight());
        imageAvailableSemaphores.resize(frameCount);
        renderFinishedSemaphores.resize(frameCount);
        inFlightFences.resize(frameCount);

        VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo     fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

        for (size_t i = 0; i < frameCount; ++i) {
            if (vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device.device(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw SemaphoreCreationException("failed to create synchronization objects for frame!");
            }
        }

        if (presentIdState.enabled) {
            presentIdState.next = 1;
        }
    }

    VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        const std::vector<VkPresentModeKHR> preferredModes = {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR};

        for (auto preferred : preferredModes) {
            if (std::ranges::find(availablePresentModes, preferred) != availablePresentModes.end()) {
                Logger::info(LogChannel::Render, "Present mode selected: ", preferred);
                return preferred;
            }
        }

        Logger::info(LogChannel::Render, "Present mode: FIFO (V-Sync)");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        VkExtent2D actualExtent = windowExtent;
        actualExtent.width      = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height     = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }

    VkFormat SwapChain::findDepthFormat() {
        return device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

}  // namespace engine
