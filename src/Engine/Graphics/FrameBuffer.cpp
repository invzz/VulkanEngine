#include "Engine/Graphics/FrameBuffer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "Engine/Graphics/Device.hpp"

#include "vulkan/vulkan_core.h"
namespace engine {
    namespace {
        constexpr bool     kUseCompactGbufferFormats       = true;
        constexpr VkFormat kGbufferNormalFormatCompact     = VK_FORMAT_R16G16_SNORM;
        constexpr VkFormat kGbufferNormalFormatReference   = VK_FORMAT_R16G16B16A16_SFLOAT;
        constexpr VkFormat kGbufferAlbedoFormatCompact     = VK_FORMAT_R8G8B8A8_UNORM;
        constexpr VkFormat kGbufferAlbedoFormatReference   = VK_FORMAT_R16G16B16A16_SFLOAT;
        constexpr VkFormat kGbufferMaterialFormatCompact   = VK_FORMAT_R8G8B8A8_UNORM;
        constexpr VkFormat kGbufferMaterialFormatReference = VK_FORMAT_R16G16B16A16_SFLOAT;
        constexpr VkFormat kSelectedGbufferNormalFormat =
            kUseCompactGbufferFormats ? kGbufferNormalFormatCompact : kGbufferNormalFormatReference;
        constexpr VkFormat kSelectedGbufferAlbedoFormat =
            kUseCompactGbufferFormats ? kGbufferAlbedoFormatCompact : kGbufferAlbedoFormatReference;
        constexpr VkFormat kSelectedGbufferMaterialFormat =
            kUseCompactGbufferFormats ? kGbufferMaterialFormatCompact : kGbufferMaterialFormatReference;
    }  // namespace
    FrameBuffer::FrameBuffer(Device& device, VkExtent2D extent, uint32_t frameCount, bool useMipmaps) : device{device}, extent{extent}, frameCount{frameCount}, useMipmaps{useMipmaps} {
        createRenderPass();
        createImages();
        createFramebuffers();
    }
    FrameBuffer::~FrameBuffer() {
        cleanup();
        vkDestroyRenderPass(device.device(), renderPass, nullptr);
        vkDestroyRenderPass(device.device(), depthPrepassRenderPass, nullptr);
        vkDestroyRenderPass(device.device(), renderPassLoadDepth, nullptr);
        vkDestroyRenderPass(device.device(), renderPassLoadColorDepth, nullptr);
        vkDestroyRenderPass(device.device(), gbufferRenderPass, nullptr);
        vkDestroyRenderPass(device.device(), deferredLightingRenderPass, nullptr);
        vkDestroyRenderPass(device.device(), postFxRenderPass, nullptr);
        vkDestroyRenderPass(device.device(), selectionMaskRenderPass, nullptr);
        vkDestroyRenderPass(device.device(), selectionOutlineRenderPass, nullptr);
    }
    void FrameBuffer::cleanup() {
        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }
        for (auto framebuffer : depthPrepassFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }
        for (auto framebuffer : loadDepthFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }
        for (auto framebuffer : loadColorDepthFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }
        for (auto framebuffer : gbufferFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }
        for (auto framebuffer : deferredLightingFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }
        for (auto framebuffer : postFxFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }
        for (auto& target : colorTargets) {
            target.destroy(device);
        }
        colorTargets.clear();
        for (auto& target : sceneColorTargets) {
            target.destroy(device);
        }
        sceneColorTargets.clear();
        for (auto& target : gbufferNormalTargets) {
            target.destroy(device);
        }
        gbufferNormalTargets.clear();
        for (auto& target : gbufferAlbedoTargets) {
            target.destroy(device);
        }
        gbufferAlbedoTargets.clear();
        for (auto& target : gbufferMaterialTargets) {
            target.destroy(device);
        }
        gbufferMaterialTargets.clear();
        for (auto& target : depthTargets) {
            target.destroy(device);
        }
        depthTargets.clear();
        for (auto& target : postFxTargets) {
            target.destroy(device);
        }
        postFxTargets.clear();
        for (auto& target : selectionOutlineTargets) {
            target.destroy(device);
        }
        selectionOutlineTargets.clear();
        for (auto& target : selectionMaskTargets) {
            target.destroy(device);
        }
        selectionMaskTargets.clear();
    }
    void FrameBuffer::resize(VkExtent2D newExtent) {
        extent = newExtent;
        cleanup();
        createImages();
        createFramebuffers();
    }
    void FrameBuffer::createRenderPass() {
        VkFormat const depthFormat =
            device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        if (useMipmaps) {
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        } else {
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format         = depthFormat;
        depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        std::array<VkSubpassDependency, 2> dependencies;
        dependencies[0].srcSubpass                         = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass                         = 0;
        dependencies[0].srcStageMask                       = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].dstStageMask                       = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask                      = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask                      = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags                    = VK_DEPENDENCY_BY_REGION_BIT;
        dependencies[1].srcSubpass                         = 0;
        dependencies[1].dstSubpass                         = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask                       = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask                       = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dependencies[1].srcAccessMask                      = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask                      = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags                    = VK_DEPENDENCY_BY_REGION_BIT;
        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo                 renderPassInfo{};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies   = dependencies.data();
        if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create frame buffer render pass!");
        }
        VkAttachmentDescription prepassColorAttachment = colorAttachment;
        prepassColorAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        prepassColorAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        prepassColorAttachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        prepassColorAttachment.finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentDescription prepassDepthAttachment{};
        prepassDepthAttachment.format         = depthFormat;
        prepassDepthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        prepassDepthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        prepassDepthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        prepassDepthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        prepassDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        prepassDepthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        prepassDepthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference prepassDepthAttachmentRef{};
        prepassDepthAttachmentRef.attachment = 1;
        prepassDepthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkSubpassDescription prepassSubpass{};
        prepassSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        prepassSubpass.colorAttachmentCount    = 0;
        prepassSubpass.pColorAttachments       = nullptr;
        prepassSubpass.pDepthStencilAttachment = &prepassDepthAttachmentRef;
        std::array<VkSubpassDependency, 2> prepassDependencies;
        prepassDependencies[0].srcSubpass                         = VK_SUBPASS_EXTERNAL;
        prepassDependencies[0].dstSubpass                         = 0;
        prepassDependencies[0].srcStageMask                       = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        prepassDependencies[0].dstStageMask                       = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        prepassDependencies[0].srcAccessMask                      = VK_ACCESS_SHADER_READ_BIT;
        prepassDependencies[0].dstAccessMask                      = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        prepassDependencies[0].dependencyFlags                    = VK_DEPENDENCY_BY_REGION_BIT;
        prepassDependencies[1].srcSubpass                         = 0;
        prepassDependencies[1].dstSubpass                         = VK_SUBPASS_EXTERNAL;
        prepassDependencies[1].srcStageMask                       = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        prepassDependencies[1].dstStageMask                       = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        prepassDependencies[1].srcAccessMask                      = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        prepassDependencies[1].dstAccessMask                      = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        prepassDependencies[1].dependencyFlags                    = VK_DEPENDENCY_BY_REGION_BIT;
        std::array<VkAttachmentDescription, 2> prepassAttachments = {prepassColorAttachment, prepassDepthAttachment};
        VkRenderPassCreateInfo                 prepassRenderPassInfo{};
        prepassRenderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        prepassRenderPassInfo.attachmentCount = static_cast<uint32_t>(prepassAttachments.size());
        prepassRenderPassInfo.pAttachments    = prepassAttachments.data();
        prepassRenderPassInfo.subpassCount    = 1;
        prepassRenderPassInfo.pSubpasses      = &prepassSubpass;
        prepassRenderPassInfo.dependencyCount = static_cast<uint32_t>(prepassDependencies.size());
        prepassRenderPassInfo.pDependencies   = prepassDependencies.data();
        if (vkCreateRenderPass(device.device(), &prepassRenderPassInfo, nullptr, &depthPrepassRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create depth prepass render pass!");
        }
        VkAttachmentDescription loadColorAttachment = colorAttachment;
        loadColorAttachment.initialLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentDescription loadDepthAttachment{};
        loadDepthAttachment.format         = depthFormat;
        loadDepthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        loadDepthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
        loadDepthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        loadDepthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        loadDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        loadDepthAttachment.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        loadDepthAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference loadColorAttachmentRef{};
        loadColorAttachmentRef.attachment = 0;
        loadColorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference loadDepthAttachmentRef{};
        loadDepthAttachmentRef.attachment = 1;
        loadDepthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkSubpassDescription loadSubpass{};
        loadSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        loadSubpass.colorAttachmentCount    = 1;
        loadSubpass.pColorAttachments       = &loadColorAttachmentRef;
        loadSubpass.pDepthStencilAttachment = &loadDepthAttachmentRef;
        std::array<VkSubpassDependency, 2> loadDependencies;
        loadDependencies[0].srcSubpass                         = VK_SUBPASS_EXTERNAL;
        loadDependencies[0].dstSubpass                         = 0;
        loadDependencies[0].srcStageMask                       = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        loadDependencies[0].dstStageMask                       = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        loadDependencies[0].srcAccessMask                      = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        loadDependencies[0].dstAccessMask                      = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        loadDependencies[0].dependencyFlags                    = VK_DEPENDENCY_BY_REGION_BIT;
        loadDependencies[1].srcSubpass                         = 0;
        loadDependencies[1].dstSubpass                         = VK_SUBPASS_EXTERNAL;
        loadDependencies[1].srcStageMask                       = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        loadDependencies[1].dstStageMask                       = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        loadDependencies[1].srcAccessMask                      = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        loadDependencies[1].dstAccessMask                      = VK_ACCESS_SHADER_READ_BIT;
        loadDependencies[1].dependencyFlags                    = VK_DEPENDENCY_BY_REGION_BIT;
        std::array<VkAttachmentDescription, 2> loadAttachments = {loadColorAttachment, loadDepthAttachment};
        VkRenderPassCreateInfo                 loadRenderPassInfo{};
        loadRenderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        loadRenderPassInfo.attachmentCount = static_cast<uint32_t>(loadAttachments.size());
        loadRenderPassInfo.pAttachments    = loadAttachments.data();
        loadRenderPassInfo.subpassCount    = 1;
        loadRenderPassInfo.pSubpasses      = &loadSubpass;
        loadRenderPassInfo.dependencyCount = static_cast<uint32_t>(loadDependencies.size());
        loadRenderPassInfo.pDependencies   = loadDependencies.data();
        if (vkCreateRenderPass(device.device(), &loadRenderPassInfo, nullptr, &renderPassLoadDepth) != VK_SUCCESS) {
            throw std::runtime_error("failed to create load-depth render pass!");
        }
        VkAttachmentDescription loadColorDepthColorAttachment = colorAttachment;
        loadColorDepthColorAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;
        loadColorDepthColorAttachment.initialLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (useMipmaps) {
            loadColorDepthColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        } else {
            loadColorDepthColorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        VkAttachmentDescription loadColorDepthDepthAttachment = loadDepthAttachment;
        loadColorDepthDepthAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;
        loadColorDepthDepthAttachment.initialLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        loadColorDepthDepthAttachment.finalLayout             = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference loadColorDepthColorRef{};
        loadColorDepthColorRef.attachment = 0;
        loadColorDepthColorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference loadColorDepthDepthRef{};
        loadColorDepthDepthRef.attachment = 1;
        loadColorDepthDepthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkSubpassDescription loadColorDepthSubpass{};
        loadColorDepthSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        loadColorDepthSubpass.colorAttachmentCount    = 1;
        loadColorDepthSubpass.pColorAttachments       = &loadColorDepthColorRef;
        loadColorDepthSubpass.pDepthStencilAttachment = &loadColorDepthDepthRef;
        std::array<VkSubpassDependency, 2> loadColorDepthDependencies;
        loadColorDepthDependencies[0].srcSubpass                         = VK_SUBPASS_EXTERNAL;
        loadColorDepthDependencies[0].dstSubpass                         = 0;
        loadColorDepthDependencies[0].srcStageMask                       = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        loadColorDepthDependencies[0].dstStageMask                       = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        loadColorDepthDependencies[0].srcAccessMask                      = VK_ACCESS_SHADER_READ_BIT;
        loadColorDepthDependencies[0].dstAccessMask                      = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        loadColorDepthDependencies[0].dependencyFlags                    = VK_DEPENDENCY_BY_REGION_BIT;
        loadColorDepthDependencies[1].srcSubpass                         = 0;
        loadColorDepthDependencies[1].dstSubpass                         = VK_SUBPASS_EXTERNAL;
        loadColorDepthDependencies[1].srcStageMask                       = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        loadColorDepthDependencies[1].dstStageMask                       = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        loadColorDepthDependencies[1].srcAccessMask                      = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        loadColorDepthDependencies[1].dstAccessMask                      = VK_ACCESS_SHADER_READ_BIT;
        loadColorDepthDependencies[1].dependencyFlags                    = VK_DEPENDENCY_BY_REGION_BIT;
        std::array<VkAttachmentDescription, 2> loadColorDepthAttachments = {loadColorDepthColorAttachment, loadColorDepthDepthAttachment};
        VkRenderPassCreateInfo                 loadColorDepthRpInfo{};
        loadColorDepthRpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        loadColorDepthRpInfo.attachmentCount = static_cast<uint32_t>(loadColorDepthAttachments.size());
        loadColorDepthRpInfo.pAttachments    = loadColorDepthAttachments.data();
        loadColorDepthRpInfo.subpassCount    = 1;
        loadColorDepthRpInfo.pSubpasses      = &loadColorDepthSubpass;
        loadColorDepthRpInfo.dependencyCount = static_cast<uint32_t>(loadColorDepthDependencies.size());
        loadColorDepthRpInfo.pDependencies   = loadColorDepthDependencies.data();
        if (vkCreateRenderPass(device.device(), &loadColorDepthRpInfo, nullptr, &renderPassLoadColorDepth) != VK_SUCCESS) {
            throw std::runtime_error("failed to create load-color-depth render pass!");
        }
        VkAttachmentDescription gN{};
        gN.format                    = kSelectedGbufferNormalFormat;
        gN.samples                   = VK_SAMPLE_COUNT_1_BIT;
        gN.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
        gN.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
        gN.stencilLoadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        gN.stencilStoreOp            = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        gN.initialLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
        gN.finalLayout               = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentDescription gA   = gN;
        gA.format                    = kSelectedGbufferAlbedoFormat;
        VkAttachmentDescription gM   = gN;
        gM.format                    = kSelectedGbufferMaterialFormat;
        VkAttachmentDescription gHdr = colorAttachment;
        gHdr.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        gHdr.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
        gHdr.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        gHdr.finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentDescription gDepth{};
        gDepth.format         = depthFormat;
        gDepth.samples        = VK_SAMPLE_COUNT_1_BIT;
        gDepth.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
        gDepth.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        gDepth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        gDepth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        gDepth.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        gDepth.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        std::array<VkAttachmentReference, 4> gbufferColorRefs{};
        gbufferColorRefs[0] = VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        gbufferColorRefs[1] = VkAttachmentReference{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        gbufferColorRefs[2] = VkAttachmentReference{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        gbufferColorRefs[3] = VkAttachmentReference{3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference const gbufferDepthRef{4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription        gbufferSubpass{};
        gbufferSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        gbufferSubpass.colorAttachmentCount    = static_cast<uint32_t>(gbufferColorRefs.size());
        gbufferSubpass.pColorAttachments       = gbufferColorRefs.data();
        gbufferSubpass.pDepthStencilAttachment = &gbufferDepthRef;
        std::array<VkSubpassDependency, 2> gbufferDeps{};
        gbufferDeps[0].srcSubpass                                 = VK_SUBPASS_EXTERNAL;
        gbufferDeps[0].dstSubpass                                 = 0;
        gbufferDeps[0].srcStageMask                               = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        gbufferDeps[0].dstStageMask                               = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        gbufferDeps[0].srcAccessMask                              = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        gbufferDeps[0].dstAccessMask                              = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        gbufferDeps[0].dependencyFlags                            = VK_DEPENDENCY_BY_REGION_BIT;
        gbufferDeps[1].srcSubpass                                 = 0;
        gbufferDeps[1].dstSubpass                                 = VK_SUBPASS_EXTERNAL;
        gbufferDeps[1].srcStageMask                               = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        gbufferDeps[1].dstStageMask                               = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        gbufferDeps[1].srcAccessMask                              = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        gbufferDeps[1].dstAccessMask                              = VK_ACCESS_SHADER_READ_BIT;
        gbufferDeps[1].dependencyFlags                            = VK_DEPENDENCY_BY_REGION_BIT;
        std::array<VkAttachmentDescription, 5> gbufferAttachments = {gN, gA, gM, gHdr, gDepth};
        VkRenderPassCreateInfo                 gbufferInfo{};
        gbufferInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        gbufferInfo.attachmentCount = static_cast<uint32_t>(gbufferAttachments.size());
        gbufferInfo.pAttachments    = gbufferAttachments.data();
        gbufferInfo.subpassCount    = 1;
        gbufferInfo.pSubpasses      = &gbufferSubpass;
        gbufferInfo.dependencyCount = static_cast<uint32_t>(gbufferDeps.size());
        gbufferInfo.pDependencies   = gbufferDeps.data();
        if (vkCreateRenderPass(device.device(), &gbufferInfo, nullptr, &gbufferRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create gbuffer render pass!");
        }
        VkAttachmentDescription litColor = colorAttachment;
        litColor.loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;
        litColor.initialLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        litColor.finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference const litColorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription        litSubpass{};
        litSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        litSubpass.colorAttachmentCount    = 1;
        litSubpass.pColorAttachments       = &litColorRef;
        litSubpass.pDepthStencilAttachment = nullptr;
        std::array<VkSubpassDependency, 2> litDeps{};
        litDeps[0].srcSubpass                                 = VK_SUBPASS_EXTERNAL;
        litDeps[0].dstSubpass                                 = 0;
        litDeps[0].srcStageMask                               = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        litDeps[0].dstStageMask                               = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        litDeps[0].srcAccessMask                              = VK_ACCESS_SHADER_READ_BIT;
        litDeps[0].dstAccessMask                              = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        litDeps[0].dependencyFlags                            = VK_DEPENDENCY_BY_REGION_BIT;
        litDeps[1].srcSubpass                                 = 0;
        litDeps[1].dstSubpass                                 = VK_SUBPASS_EXTERNAL;
        litDeps[1].srcStageMask                               = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        litDeps[1].dstStageMask                               = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        litDeps[1].srcAccessMask                              = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        litDeps[1].dstAccessMask                              = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        litDeps[1].dependencyFlags                            = VK_DEPENDENCY_BY_REGION_BIT;
        std::array<VkAttachmentDescription, 1> litAttachments = {litColor};
        VkRenderPassCreateInfo                 litInfo{};
        litInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        litInfo.attachmentCount = static_cast<uint32_t>(litAttachments.size());
        litInfo.pAttachments    = litAttachments.data();
        litInfo.subpassCount    = 1;
        litInfo.pSubpasses      = &litSubpass;
        litInfo.dependencyCount = static_cast<uint32_t>(litDeps.size());
        litInfo.pDependencies   = litDeps.data();
        if (vkCreateRenderPass(device.device(), &litInfo, nullptr, &deferredLightingRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create deferred lighting render pass!");
        }
        // --- Post-Fx render pass ---
        {
            VkAttachmentDescription postFxColor{};
            postFxColor.format         = VK_FORMAT_R8G8B8A8_UNORM;
            postFxColor.samples        = VK_SAMPLE_COUNT_1_BIT;
            postFxColor.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            postFxColor.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            postFxColor.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            postFxColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            postFxColor.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            postFxColor.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkAttachmentReference postFxColorRef{};
            postFxColorRef.attachment = 0;
            postFxColorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkSubpassDescription postFxSubpass{};
            postFxSubpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
            postFxSubpass.colorAttachmentCount = 1;
            postFxSubpass.pColorAttachments    = &postFxColorRef;
            std::array<VkSubpassDependency, 2> postFxDeps{};
            postFxDeps[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
            postFxDeps[0].dstSubpass      = 0;
            postFxDeps[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            postFxDeps[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            postFxDeps[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
            postFxDeps[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            postFxDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            postFxDeps[1].srcSubpass      = 0;
            postFxDeps[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
            postFxDeps[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            postFxDeps[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            postFxDeps[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            postFxDeps[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
            postFxDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            VkRenderPassCreateInfo postFxInfo{};
            postFxInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            postFxInfo.attachmentCount = 1;
            postFxInfo.pAttachments    = &postFxColor;
            postFxInfo.subpassCount    = 1;
            postFxInfo.pSubpasses      = &postFxSubpass;
            postFxInfo.dependencyCount = static_cast<uint32_t>(postFxDeps.size());
            postFxInfo.pDependencies   = postFxDeps.data();
            if (vkCreateRenderPass(device.device(), &postFxInfo, nullptr, &postFxRenderPass) != VK_SUCCESS) {
                throw std::runtime_error("failed to create post-fx render pass!");
            }
        }
        // --- Selection mask render pass (color only, depth disabled) ---
        {
            VkAttachmentDescription maskAttachment{};
            maskAttachment.format         = VK_FORMAT_R8_UNORM;
            maskAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
            maskAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            maskAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            maskAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            maskAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            maskAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            maskAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkAttachmentReference maskRef{};
            maskRef.attachment = 0;
            maskRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkSubpassDescription maskSubpass{};
            maskSubpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
            maskSubpass.colorAttachmentCount = 1;
            maskSubpass.pColorAttachments    = &maskRef;
            std::array<VkSubpassDependency, 2> maskDeps{};
            maskDeps[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
            maskDeps[0].dstSubpass      = 0;
            maskDeps[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            maskDeps[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            maskDeps[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
            maskDeps[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            maskDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            maskDeps[1].srcSubpass      = 0;
            maskDeps[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
            maskDeps[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            maskDeps[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            maskDeps[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            maskDeps[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
            maskDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            VkRenderPassCreateInfo maskInfo{};
            maskInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            maskInfo.attachmentCount = 1;
            maskInfo.pAttachments    = &maskAttachment;
            maskInfo.subpassCount    = 1;
            maskInfo.pSubpasses      = &maskSubpass;
            maskInfo.dependencyCount = static_cast<uint32_t>(maskDeps.size());
            maskInfo.pDependencies   = maskDeps.data();
            if (vkCreateRenderPass(device.device(), &maskInfo, nullptr, &selectionMaskRenderPass) != VK_SUCCESS) {
                throw std::runtime_error("failed to create selection mask render pass!");
            }
        }
        // --- Selection outline composite render pass (color only) ---
        {
            VkAttachmentDescription outlineAttachment{};
            outlineAttachment.format         = VK_FORMAT_R8G8B8A8_UNORM;
            outlineAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
            outlineAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            outlineAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            outlineAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            outlineAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            outlineAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            outlineAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkAttachmentReference outlineRef{};
            outlineRef.attachment = 0;
            outlineRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkSubpassDescription outlineSubpass{};
            outlineSubpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
            outlineSubpass.colorAttachmentCount = 1;
            outlineSubpass.pColorAttachments    = &outlineRef;
            std::array<VkSubpassDependency, 2> outlineDeps{};
            outlineDeps[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
            outlineDeps[0].dstSubpass      = 0;
            outlineDeps[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            outlineDeps[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            outlineDeps[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
            outlineDeps[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            outlineDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            outlineDeps[1].srcSubpass      = 0;
            outlineDeps[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
            outlineDeps[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            outlineDeps[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            outlineDeps[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            outlineDeps[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
            outlineDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            VkRenderPassCreateInfo outlineInfo{};
            outlineInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            outlineInfo.attachmentCount = 1;
            outlineInfo.pAttachments    = &outlineAttachment;
            outlineInfo.subpassCount    = 1;
            outlineInfo.pSubpasses      = &outlineSubpass;
            outlineInfo.dependencyCount = static_cast<uint32_t>(outlineDeps.size());
            outlineInfo.pDependencies   = outlineDeps.data();
            if (vkCreateRenderPass(device.device(), &outlineInfo, nullptr, &selectionOutlineRenderPass) != VK_SUCCESS) {
                throw std::runtime_error("failed to create selection outline render pass!");
            }
        }
    }
    void FrameBuffer::createImages() {
        if (useMipmaps) {
            mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
        } else {
            mipLevels = 1;
        }
        colorTargets.resize(frameCount);
        sceneColorTargets.resize(frameCount);
        gbufferNormalTargets.resize(frameCount);
        gbufferAlbedoTargets.resize(frameCount);
        gbufferMaterialTargets.resize(frameCount);
        depthTargets.resize(frameCount);
        postFxTargets.resize(frameCount);
        selectionOutlineTargets.resize(frameCount);
        selectionMaskTargets.resize(frameCount);
        VkFormat const colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat const depthFormat =
            device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        auto makeTarget = [&](RenderTarget&              target,
                              VkFormat                   format,
                              uint32_t                   levels,
                              VkImageUsageFlags          usage,
                              VkImageAspectFlags         aspectMask,
                              bool                       perMipViews,
                              bool                       attachmentView,
                              bool                       createSampler,
                              const VkSamplerCreateInfo* samplerInfo) {
            RenderTarget::CreateInfo info{};
            info.extent                 = extent;
            info.format                 = format;
            info.mipLevels              = levels;
            info.usage                  = usage;
            info.aspectMask             = aspectMask;
            info.createPerMipViews      = perMipViews;
            info.createAttachmentView   = attachmentView;
            info.attachmentBaseMipLevel = 0;
            info.attachmentLevelCount   = 1;
            info.createSampler          = createSampler;
            if (samplerInfo != nullptr) {
                info.samplerInfo = *samplerInfo;
            }
            target.create(device, info);
        };
        VkSamplerCreateInfo linearSamplerInfo{};
        linearSamplerInfo.sType                = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        linearSamplerInfo.magFilter            = VK_FILTER_LINEAR;
        linearSamplerInfo.minFilter            = VK_FILTER_LINEAR;
        linearSamplerInfo.mipmapMode           = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        linearSamplerInfo.addressModeU         = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        linearSamplerInfo.addressModeV         = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        linearSamplerInfo.addressModeW         = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        linearSamplerInfo.mipLodBias           = 0.0f;
        linearSamplerInfo.maxAnisotropy        = 1.0f;
        linearSamplerInfo.minLod               = 0.0f;
        linearSamplerInfo.maxLod               = static_cast<float>(mipLevels);
        linearSamplerInfo.borderColor          = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VkSamplerCreateInfo nearestSamplerInfo = linearSamplerInfo;
        nearestSamplerInfo.magFilter           = VK_FILTER_NEAREST;
        nearestSamplerInfo.minFilter           = VK_FILTER_NEAREST;
        nearestSamplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        nearestSamplerInfo.maxLod              = 1.0f;
        VkImageUsageFlags colorUsage           = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (useMipmaps) {
            colorUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VkImageUsageFlags depthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (useMipmaps) {
            depthUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        for (uint32_t i = 0; i < frameCount; ++i) {
            makeTarget(colorTargets[i], colorFormat, mipLevels, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, false, true, true, &linearSamplerInfo);
            makeTarget(depthTargets[i], depthFormat, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, false, true, true, &nearestSamplerInfo);
            makeTarget(sceneColorTargets[i], colorFormat, mipLevels,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                false,
                false,
                false,
                nullptr);
            makeTarget(gbufferNormalTargets[i], kSelectedGbufferNormalFormat, 1,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                false,
                false,
                false,
                nullptr);
            makeTarget(gbufferAlbedoTargets[i], kSelectedGbufferAlbedoFormat, 1,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                false,
                false,
                false,
                nullptr);
            makeTarget(gbufferMaterialTargets[i], kSelectedGbufferMaterialFormat, 1,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                false,
                false,
                false,
                nullptr);
            makeTarget(postFxTargets[i], VK_FORMAT_R8G8B8A8_UNORM, 1,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                false,
                true,
                true,
                &linearSamplerInfo);
            makeTarget(selectionOutlineTargets[i], VK_FORMAT_R8G8B8A8_UNORM, 1,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                false,
                true,
                true,
                &linearSamplerInfo);
            makeTarget(selectionMaskTargets[i], VK_FORMAT_R8_UNORM, 1,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                false,
                true,
                true,
                &linearSamplerInfo);
        }
    }
    void FrameBuffer::createFramebuffers() {
        framebuffers.resize(frameCount);
        depthPrepassFramebuffers.resize(frameCount);
        loadDepthFramebuffers.resize(frameCount);
        loadColorDepthFramebuffers.resize(frameCount);
        gbufferFramebuffers.resize(frameCount);
        deferredLightingFramebuffers.resize(frameCount);
        postFxFramebuffers.resize(frameCount);
        selectionMaskFramebuffers.resize(frameCount);
        selectionOutlineFramebuffers.resize(frameCount);
        for (size_t i = 0; i < frameCount; i++) {
            std::array<VkImageView, 2> attachments = {colorTargets[i].getAttachmentView(), depthTargets[i].getAttachmentView()};
            VkFramebufferCreateInfo    framebufferInfo{};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments    = attachments.data();
            framebufferInfo.width           = extent.width;
            framebufferInfo.height          = extent.height;
            framebufferInfo.layers          = 1;
            if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
            VkFramebufferCreateInfo prepassFramebufferInfo = framebufferInfo;
            prepassFramebufferInfo.renderPass              = depthPrepassRenderPass;
            if (vkCreateFramebuffer(device.device(), &prepassFramebufferInfo, nullptr, &depthPrepassFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create depth prepass framebuffer!");
            }
            VkFramebufferCreateInfo loadDepthFramebufferInfo = framebufferInfo;
            loadDepthFramebufferInfo.renderPass              = renderPassLoadDepth;
            if (vkCreateFramebuffer(device.device(), &loadDepthFramebufferInfo, nullptr, &loadDepthFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create load-depth framebuffer!");
            }
            VkFramebufferCreateInfo loadColorDepthFramebufferInfo = framebufferInfo;
            loadColorDepthFramebufferInfo.renderPass              = renderPassLoadColorDepth;
            if (vkCreateFramebuffer(device.device(), &loadColorDepthFramebufferInfo, nullptr, &loadColorDepthFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create load-color-depth framebuffer!");
            }
            std::array<VkImageView, 5> gbufferAttachments =
                {gbufferNormalTargets[i].getView(),
                    gbufferAlbedoTargets[i].getView(),
                    gbufferMaterialTargets[i].getView(),
                    colorTargets[i].getAttachmentView(),
                    depthTargets[i].getAttachmentView()};
            VkFramebufferCreateInfo gbufferFbInfo{};
            gbufferFbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            gbufferFbInfo.renderPass      = gbufferRenderPass;
            gbufferFbInfo.attachmentCount = static_cast<uint32_t>(gbufferAttachments.size());
            gbufferFbInfo.pAttachments    = gbufferAttachments.data();
            gbufferFbInfo.width           = extent.width;
            gbufferFbInfo.height          = extent.height;
            gbufferFbInfo.layers          = 1;
            if (vkCreateFramebuffer(device.device(), &gbufferFbInfo, nullptr, &gbufferFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create gbuffer framebuffer!");
            }
            std::array<VkImageView, 1> litAttachments = {colorTargets[i].getAttachmentView()};
            VkFramebufferCreateInfo    litFbInfo{};
            litFbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            litFbInfo.renderPass      = deferredLightingRenderPass;
            litFbInfo.attachmentCount = static_cast<uint32_t>(litAttachments.size());
            litFbInfo.pAttachments    = litAttachments.data();
            litFbInfo.width           = extent.width;
            litFbInfo.height          = extent.height;
            litFbInfo.layers          = 1;
            if (vkCreateFramebuffer(device.device(), &litFbInfo, nullptr, &deferredLightingFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create deferred lighting framebuffer!");
            }
            std::array<VkImageView, 1> postFxAttachments = {postFxTargets[i].getAttachmentView()};
            VkFramebufferCreateInfo    postFxInfo{};
            postFxInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            postFxInfo.renderPass      = postFxRenderPass;
            postFxInfo.attachmentCount = static_cast<uint32_t>(postFxAttachments.size());
            postFxInfo.pAttachments    = postFxAttachments.data();
            postFxInfo.width           = extent.width;
            postFxInfo.height          = extent.height;
            postFxInfo.layers          = 1;
            if (vkCreateFramebuffer(device.device(), &postFxInfo, nullptr, &postFxFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create post-fx framebuffer!");
            }
            std::array<VkImageView, 1> maskAttachments = {selectionMaskTargets[i].getAttachmentView()};
            VkFramebufferCreateInfo    maskInfo{};
            maskInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            maskInfo.renderPass      = selectionMaskRenderPass;
            maskInfo.attachmentCount = static_cast<uint32_t>(maskAttachments.size());
            maskInfo.pAttachments    = maskAttachments.data();
            maskInfo.width           = extent.width;
            maskInfo.height          = extent.height;
            maskInfo.layers          = 1;
            if (vkCreateFramebuffer(device.device(), &maskInfo, nullptr, &selectionMaskFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create selection mask framebuffer!");
            }
            std::array<VkImageView, 1> outlineAttachments = {selectionOutlineTargets[i].getAttachmentView()};
            VkFramebufferCreateInfo    outlineInfo{};
            outlineInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            outlineInfo.renderPass      = selectionOutlineRenderPass;
            outlineInfo.attachmentCount = static_cast<uint32_t>(outlineAttachments.size());
            outlineInfo.pAttachments    = outlineAttachments.data();
            outlineInfo.width           = extent.width;
            outlineInfo.height          = extent.height;
            outlineInfo.layers          = 1;
            if (vkCreateFramebuffer(device.device(), &outlineInfo, nullptr, &selectionOutlineFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create selection outline framebuffer!");
            }
        }
    }
    VkDescriptorImageInfo FrameBuffer::getDescriptorImageInfo(int index) const {
        return VkDescriptorImageInfo{
            .sampler     = colorTargets[index].getSampler(),
            .imageView   = colorTargets[index].getView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    VkDescriptorImageInfo FrameBuffer::getSceneColorDescriptorImageInfo(int index) const {
        return VkDescriptorImageInfo{
            .sampler     = colorTargets[index].getSampler(),
            .imageView   = sceneColorTargets[index].getView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    VkDescriptorImageInfo FrameBuffer::getGbufferNormalImageInfo(int index) const {
        return VkDescriptorImageInfo{
            .sampler     = colorTargets[index].getSampler(),
            .imageView   = gbufferNormalTargets[index].getView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    VkDescriptorImageInfo FrameBuffer::getGbufferAlbedoImageInfo(int index) const {
        return VkDescriptorImageInfo{
            .sampler     = colorTargets[index].getSampler(),
            .imageView   = gbufferAlbedoTargets[index].getView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    VkDescriptorImageInfo FrameBuffer::getGbufferMaterialImageInfo(int index) const {
        return VkDescriptorImageInfo{
            .sampler     = colorTargets[index].getSampler(),
            .imageView   = gbufferMaterialTargets[index].getView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    VkDescriptorImageInfo FrameBuffer::getPostFxDescriptorImageInfo(int index) const {
        return VkDescriptorImageInfo{
            .sampler     = postFxTargets[index].getSampler(),
            .imageView   = postFxTargets[index].getView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    VkDescriptorImageInfo FrameBuffer::getSelectionOutlineDescriptorImageInfo(int index) const {
        return VkDescriptorImageInfo{
            .sampler     = selectionOutlineTargets[index].getSampler(),
            .imageView   = selectionOutlineTargets[index].getView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    VkDescriptorImageInfo FrameBuffer::getSelectionMaskDescriptorImageInfo(int index) const {
        return VkDescriptorImageInfo{
            .sampler     = selectionMaskTargets[index].getSampler(),
            .imageView   = selectionMaskTargets[index].getView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }
    void FrameBuffer::beginRenderPass(VkCommandBuffer commandBuffer, int frameIndex) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = renderPass;
        renderPassInfo.framebuffer       = framebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color           = {{0.01f, 0.01f, 0.01f, 1.0f}};
        clearValues[1].depthStencil    = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues    = clearValues.data();
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    void FrameBuffer::beginDepthPrepassRenderPass(VkCommandBuffer commandBuffer, int frameIndex) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = depthPrepassRenderPass;
        renderPassInfo.framebuffer       = depthPrepassFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color           = {{0.01f, 0.01f, 0.01f, 1.0f}};
        clearValues[1].depthStencil    = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues    = clearValues.data();
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    void FrameBuffer::beginRenderPassLoadDepth(VkCommandBuffer commandBuffer, int frameIndex) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = renderPassLoadDepth;
        renderPassInfo.framebuffer       = loadDepthFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color           = {{0.01f, 0.01f, 0.01f, 1.0f}};
        clearValues[1].depthStencil    = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues    = clearValues.data();
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    void FrameBuffer::beginRenderPassLoadColorDepth(VkCommandBuffer commandBuffer, int frameIndex) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = renderPassLoadColorDepth;
        renderPassInfo.framebuffer       = loadColorDepthFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color           = {{0.01f, 0.01f, 0.01f, 1.0f}};
        clearValues[1].depthStencil    = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues    = clearValues.data();
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    void FrameBuffer::beginGbufferRenderPass(VkCommandBuffer commandBuffer, int frameIndex, bool allowSecondaryCommandBuffers) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = gbufferRenderPass;
        renderPassInfo.framebuffer       = gbufferFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        std::array<VkClearValue, 5> clearValues{};
        clearValues[0].color           = {{0.0f, 0.0f, 1.0f, 0.0f}};
        clearValues[1].color           = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[2].color           = {{0.0f, 1.0f, 1.0f, 1.5f}};
        clearValues[3].color           = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[4].depthStencil    = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues    = clearValues.data();
        VkSubpassContents contents     = allowSecondaryCommandBuffers ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE;
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, contents);
    }
    void FrameBuffer::beginDeferredLightingRenderPass(VkCommandBuffer commandBuffer, int frameIndex) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = deferredLightingRenderPass;
        renderPassInfo.framebuffer       = deferredLightingFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        renderPassInfo.clearValueCount   = 0;
        renderPassInfo.pClearValues      = nullptr;
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    void FrameBuffer::beginPostFxRenderPass(VkCommandBuffer commandBuffer, int frameIndex) {
        VkClearValue clearValues[] = {
            {.color = {0.0f, 0.0f, 0.0f, 1.0f}},
        };
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = postFxRenderPass;
        renderPassInfo.framebuffer       = postFxFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(std::size(clearValues));
        renderPassInfo.pClearValues      = clearValues;
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    void FrameBuffer::beginSelectionMaskRenderPass(VkCommandBuffer commandBuffer, int frameIndex) {
        VkClearValue clearValues[] = {
            {.color = {0.0f, 0.0f, 0.0f, 0.0f}},
        };
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = selectionMaskRenderPass;
        renderPassInfo.framebuffer       = selectionMaskFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(std::size(clearValues));
        renderPassInfo.pClearValues      = clearValues;
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    void FrameBuffer::beginSelectionOutlineRenderPass(VkCommandBuffer commandBuffer, int frameIndex) {
        VkClearValue clearValues[] = {
            {.color = {0.0f, 0.0f, 0.0f, 1.0f}},
        };
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = selectionOutlineRenderPass;
        renderPassInfo.framebuffer       = selectionOutlineFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(std::size(clearValues));
        renderPassInfo.pClearValues      = clearValues;
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    void FrameBuffer::endRenderPass(VkCommandBuffer commandBuffer) {
        vkCmdEndRenderPass(commandBuffer);
    }
    void FrameBuffer::generateMipmaps(VkCommandBuffer commandBuffer, int frameIndex) {
        if (!useMipmaps) {
            return;
        }
        VkImage              image     = colorTargets[frameIndex].getImage();
        int32_t              mipWidth  = static_cast<int32_t>(extent.width);
        int32_t              mipHeight = static_cast<int32_t>(extent.height);
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image                           = image;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i;
            barrier.oldLayout                     = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask                 = 0;
            barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            VkImageBlit blit{};
            blit.srcOffsets[0]                 = {0, 0, 0};
            blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel       = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount     = 1;
            blit.dstOffsets[0]                 = {0, 0, 0};
            blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel       = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount     = 1;
            vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            if (i < mipLevels - 1) {
                barrier.subresourceRange.baseMipLevel = i;
                barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            }
            if (mipWidth > 1) {
                mipWidth /= 2;
            }
            if (mipHeight > 1) {
                mipHeight /= 2;
            }
        }
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    void FrameBuffer::generateSceneColorMipmaps(VkCommandBuffer commandBuffer, int frameIndex) {
        if (!useMipmaps) {
            return;
        }
        VkImage              image     = sceneColorTargets[frameIndex].getImage();
        int32_t              mipWidth  = static_cast<int32_t>(extent.width);
        int32_t              mipHeight = static_cast<int32_t>(extent.height);
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image                           = image;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i;
            barrier.oldLayout                     = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask                 = 0;
            barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            VkImageBlit blit{};
            blit.srcOffsets[0]                 = {0, 0, 0};
            blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel       = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount     = 1;
            blit.dstOffsets[0]                 = {0, 0, 0};
            blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel       = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount     = 1;
            vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            if (i < mipLevels - 1) {
                barrier.subresourceRange.baseMipLevel = i;
                barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            }
            if (mipWidth > 1) {
                mipWidth /= 2;
            }
            if (mipHeight > 1) {
                mipHeight /= 2;
            }
        }
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    void FrameBuffer::transitionDepthToShaderReadOnly(VkCommandBuffer commandBuffer) {
        if (depthTargets.empty()) {
            return;
        }
        if (depthTargets.front().getMipLevels() <= 1) {
            return;
        }
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = depthTargets.front().getImage();
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel   = 1;
        barrier.subresourceRange.levelCount     = depthTargets.front().getMipLevels() - 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr, 0, nullptr, 1, &barrier);
    }
}  // namespace engine
