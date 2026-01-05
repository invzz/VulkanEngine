#include "Engine/Graphics/FrameBuffer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "Engine/Graphics/Device.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

  FrameBuffer::FrameBuffer(Device& device, VkExtent2D extent, uint32_t frameCount, bool useMipmaps) : device{device}, extent{extent}, frameCount{frameCount}, useMipmaps{useMipmaps}
  {
    createRenderPass();
    createImages();
    createFramebuffers();
  }

  FrameBuffer::~FrameBuffer()
  {
    cleanup();
    vkDestroyRenderPass(device.device(), renderPass, nullptr);
    vkDestroyRenderPass(device.device(), depthPrepassRenderPass, nullptr);
    vkDestroyRenderPass(device.device(), renderPassLoadDepth, nullptr);
    vkDestroyRenderPass(device.device(), renderPassLoadColorDepth, nullptr);
    vkDestroyRenderPass(device.device(), gbufferRenderPass, nullptr);
    vkDestroyRenderPass(device.device(), deferredLightingRenderPass, nullptr);
  }

  void FrameBuffer::cleanup()
  {
    for (auto framebuffer : framebuffers)
    {
      vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
    }

    for (auto framebuffer : depthPrepassFramebuffers)
    {
      vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
    }

    for (auto framebuffer : loadDepthFramebuffers)
    {
      vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
    }

    for (auto framebuffer : loadColorDepthFramebuffers)
    {
      vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
    }

    for (auto framebuffer : gbufferFramebuffers)
    {
      vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
    }

    for (auto framebuffer : deferredLightingFramebuffers)
    {
      vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
    }

    for (auto imageView : colorImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }

    for (auto imageView : colorAttachmentImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }

    for (auto image : colorImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }

    for (auto memory : colorImageMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    for (auto imageView : sceneColorImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }

    for (auto image : sceneColorImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }

    for (auto memory : sceneColorImageMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    for (auto imageView : gbufferNormalImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }
    for (auto image : gbufferNormalImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }
    for (auto memory : gbufferNormalMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    for (auto imageView : gbufferAlbedoImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }
    for (auto image : gbufferAlbedoImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }
    for (auto memory : gbufferAlbedoMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    for (auto imageView : gbufferMaterialImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }
    for (auto image : gbufferMaterialImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }
    for (auto memory : gbufferMaterialMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    for (auto imageView : depthImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }

    for (auto image : depthImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }

    for (auto memory : depthImageMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    for (auto& mipViews : depthMipImageViews)
    {
      for (auto imageView : mipViews)
      {
        vkDestroyImageView(device.device(), imageView, nullptr);
      }
    }
    depthMipImageViews.clear();

    for (auto imageView : hzbImageViews)
    {
      vkDestroyImageView(device.device(), imageView, nullptr);
    }

    for (auto image : hzbImages)
    {
      vkDestroyImage(device.device(), image, nullptr);
    }

    for (auto memory : hzbImageMemorys)
    {
      vkFreeMemory(device.device(), memory, nullptr);
    }

    for (auto& mipViews : hzbMipImageViews)
    {
      for (auto imageView : mipViews)
      {
        vkDestroyImageView(device.device(), imageView, nullptr);
      }
    }
    hzbMipImageViews.clear();

    if (sampler != VK_NULL_HANDLE)
    {
      vkDestroySampler(device.device(), sampler, nullptr);
      sampler = VK_NULL_HANDLE;
    }

    if (depthSampler != VK_NULL_HANDLE)
    {
      vkDestroySampler(device.device(), depthSampler, nullptr);
      depthSampler = VK_NULL_HANDLE;
    }

    if (hzbSampler != VK_NULL_HANDLE)
    {
      vkDestroySampler(device.device(), hzbSampler, nullptr);
      hzbSampler = VK_NULL_HANDLE;
    }
  }

  void FrameBuffer::resize(VkExtent2D newExtent)
  {
    extent = newExtent;
    cleanup();
    createImages();
    createFramebuffers();
  }

  void FrameBuffer::createRenderPass()
  {
    VkFormat const depthFormat =
            device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    // 1) Default offscreen render pass (clears color + depth)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

    if (useMipmaps)
    {
      colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
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

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies   = dependencies.data();

    if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create frame buffer render pass!");
    }

    // 2) Depth prepass render pass (depth clear, no color attachments used)
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
    prepassDependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    prepassDependencies[0].dstSubpass      = 0;
    prepassDependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    prepassDependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    prepassDependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    prepassDependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    prepassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    prepassDependencies[1].srcSubpass      = 0;
    prepassDependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    prepassDependencies[1].srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    prepassDependencies[1].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    prepassDependencies[1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    prepassDependencies[1].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    prepassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> prepassAttachments = {prepassColorAttachment, prepassDepthAttachment};
    VkRenderPassCreateInfo                 prepassRenderPassInfo{};
    prepassRenderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    prepassRenderPassInfo.attachmentCount = static_cast<uint32_t>(prepassAttachments.size());
    prepassRenderPassInfo.pAttachments    = prepassAttachments.data();
    prepassRenderPassInfo.subpassCount    = 1;
    prepassRenderPassInfo.pSubpasses      = &prepassSubpass;
    prepassRenderPassInfo.dependencyCount = static_cast<uint32_t>(prepassDependencies.size());
    prepassRenderPassInfo.pDependencies   = prepassDependencies.data();

    if (vkCreateRenderPass(device.device(), &prepassRenderPassInfo, nullptr, &depthPrepassRenderPass) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create depth prepass render pass!");
    }

    // 3) Main pass variant that LOADs depth (used after depth prepass)
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
    loadDependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    loadDependencies[0].dstSubpass      = 0;
    loadDependencies[0].srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    loadDependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    loadDependencies[0].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    loadDependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    loadDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    loadDependencies[1].srcSubpass      = 0;
    loadDependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    loadDependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    loadDependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    loadDependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    loadDependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    loadDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> loadAttachments = {loadColorAttachment, loadDepthAttachment};
    VkRenderPassCreateInfo                 loadRenderPassInfo{};
    loadRenderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    loadRenderPassInfo.attachmentCount = static_cast<uint32_t>(loadAttachments.size());
    loadRenderPassInfo.pAttachments    = loadAttachments.data();
    loadRenderPassInfo.subpassCount    = 1;
    loadRenderPassInfo.pSubpasses      = &loadSubpass;
    loadRenderPassInfo.dependencyCount = static_cast<uint32_t>(loadDependencies.size());
    loadRenderPassInfo.pDependencies   = loadDependencies.data();

    if (vkCreateRenderPass(device.device(), &loadRenderPassInfo, nullptr, &renderPassLoadDepth) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create load-depth render pass!");
    }

    // 4) Main pass variant that LOADs BOTH color + depth
    VkAttachmentDescription loadColorDepthColorAttachment = colorAttachment;
    loadColorDepthColorAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;
    loadColorDepthColorAttachment.initialLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (useMipmaps)
    {
      loadColorDepthColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
      loadColorDepthColorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkAttachmentDescription loadColorDepthDepthAttachment = loadDepthAttachment;
    // After the first main pass we end in SHADER_READ_ONLY_OPTIMAL; allow the render pass to transition from there.
    loadColorDepthDepthAttachment.loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
    loadColorDepthDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    loadColorDepthDepthAttachment.finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
    loadColorDepthDependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    loadColorDepthDependencies[0].dstSubpass      = 0;
    loadColorDepthDependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    loadColorDepthDependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    loadColorDepthDependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    loadColorDepthDependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    loadColorDepthDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    loadColorDepthDependencies[1].srcSubpass      = 0;
    loadColorDepthDependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    loadColorDepthDependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    loadColorDepthDependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    loadColorDepthDependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    loadColorDepthDependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    loadColorDepthDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> loadColorDepthAttachments = {loadColorDepthColorAttachment, loadColorDepthDepthAttachment};
    VkRenderPassCreateInfo                 loadColorDepthRpInfo{};
    loadColorDepthRpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    loadColorDepthRpInfo.attachmentCount = static_cast<uint32_t>(loadColorDepthAttachments.size());
    loadColorDepthRpInfo.pAttachments    = loadColorDepthAttachments.data();
    loadColorDepthRpInfo.subpassCount    = 1;
    loadColorDepthRpInfo.pSubpasses      = &loadColorDepthSubpass;
    loadColorDepthRpInfo.dependencyCount = static_cast<uint32_t>(loadColorDepthDependencies.size());
    loadColorDepthRpInfo.pDependencies   = loadColorDepthDependencies.data();

    if (vkCreateRenderPass(device.device(), &loadColorDepthRpInfo, nullptr, &renderPassLoadColorDepth) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create load-color-depth render pass!");
    }

    // 5) G-buffer render pass: write normal/albedo/material + emissive(HDR color) + depth
    VkAttachmentDescription gN{};
    gN.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    gN.samples        = VK_SAMPLE_COUNT_1_BIT;
    gN.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gN.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    gN.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gN.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gN.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    gN.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription gA = gN;
    VkAttachmentDescription gM = gN;

    // Emissive is written directly into the HDR color buffer during the opaque/G-buffer pass.
    // This ensures emissive contributes to post/bloom and is visible in the scene color copy.
    VkAttachmentDescription gHdr = colorAttachment;
    gHdr.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gHdr.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
    gHdr.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    gHdr.finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription gDepth{};
    gDepth.format  = depthFormat;
    gDepth.samples = VK_SAMPLE_COUNT_1_BIT;
    // Load depth produced by the depth prepass (HZB restores depth to ATTACHMENT_OPTIMAL).
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

    VkAttachmentReference gbufferDepthRef{4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription gbufferSubpass{};
    gbufferSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    gbufferSubpass.colorAttachmentCount    = static_cast<uint32_t>(gbufferColorRefs.size());
    gbufferSubpass.pColorAttachments       = gbufferColorRefs.data();
    gbufferSubpass.pDepthStencilAttachment = &gbufferDepthRef;

    std::array<VkSubpassDependency, 2> gbufferDeps{};
    gbufferDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    gbufferDeps[0].dstSubpass = 0;
    // Depth is produced by the depth prepass (and optionally touched by HZB compute barriers).
    // Ensure depth writes are visible before the G-buffer subpass loads/tests depth.
    gbufferDeps[0].srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    gbufferDeps[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    gbufferDeps[0].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    gbufferDeps[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    gbufferDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    gbufferDeps[1].srcSubpass      = 0;
    gbufferDeps[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    gbufferDeps[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    gbufferDeps[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    gbufferDeps[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    gbufferDeps[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    gbufferDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 5> gbufferAttachments = {gN, gA, gM, gHdr, gDepth};

    VkRenderPassCreateInfo gbufferInfo{};
    gbufferInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    gbufferInfo.attachmentCount = static_cast<uint32_t>(gbufferAttachments.size());
    gbufferInfo.pAttachments    = gbufferAttachments.data();
    gbufferInfo.subpassCount    = 1;
    gbufferInfo.pSubpasses      = &gbufferSubpass;
    gbufferInfo.dependencyCount = static_cast<uint32_t>(gbufferDeps.size());
    gbufferInfo.pDependencies   = gbufferDeps.data();

    if (vkCreateRenderPass(device.device(), &gbufferInfo, nullptr, &gbufferRenderPass) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create gbuffer render pass!");
    }

    // 6) Deferred lighting pass: fullscreen lighting into HDR color.
    // NOTE: Emissive was already written into the HDR color during the G-buffer pass, so we LOAD here.
    // NOTE: We sample depth as a texture in the shader, so we must NOT also bind the depth image as an attachment here.
    VkAttachmentDescription litColor = colorAttachment;
    litColor.loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;
    // Depth prepass leaves the (unused) HDR color attachment in COLOR_ATTACHMENT_OPTIMAL.
    // We clear it here anyway, so starting from COLOR_ATTACHMENT_OPTIMAL avoids layout mismatches.
    litColor.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    litColor.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference litColorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription litSubpass{};
    litSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    litSubpass.colorAttachmentCount    = 1;
    litSubpass.pColorAttachments       = &litColorRef;
    litSubpass.pDepthStencilAttachment = nullptr;

    std::array<VkSubpassDependency, 2> litDeps{};
    litDeps[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    litDeps[0].dstSubpass      = 0;
    litDeps[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    litDeps[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    litDeps[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    litDeps[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    litDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    litDeps[1].srcSubpass      = 0;
    litDeps[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    litDeps[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    litDeps[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    litDeps[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    litDeps[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    litDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 1> litAttachments = {litColor};

    VkRenderPassCreateInfo litInfo{};
    litInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    litInfo.attachmentCount = static_cast<uint32_t>(litAttachments.size());
    litInfo.pAttachments    = litAttachments.data();
    litInfo.subpassCount    = 1;
    litInfo.pSubpasses      = &litSubpass;
    litInfo.dependencyCount = static_cast<uint32_t>(litDeps.size());
    litInfo.pDependencies   = litDeps.data();

    if (vkCreateRenderPass(device.device(), &litInfo, nullptr, &deferredLightingRenderPass) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create deferred lighting render pass!");
    }
  }

  void FrameBuffer::createImages()
  {
    if (useMipmaps)
    {
      mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
    }
    else
    {
      mipLevels = 1;
    }

    colorImages.resize(frameCount);
    colorImageMemorys.resize(frameCount);
    colorImageViews.resize(frameCount);
    colorAttachmentImageViews.resize(frameCount);
    sceneColorImages.resize(frameCount);
    sceneColorImageMemorys.resize(frameCount);
    sceneColorImageViews.resize(frameCount);
    gbufferNormalImages.resize(frameCount);
    gbufferNormalMemorys.resize(frameCount);
    gbufferNormalImageViews.resize(frameCount);
    gbufferAlbedoImages.resize(frameCount);
    gbufferAlbedoMemorys.resize(frameCount);
    gbufferAlbedoImageViews.resize(frameCount);
    gbufferMaterialImages.resize(frameCount);
    gbufferMaterialMemorys.resize(frameCount);
    gbufferMaterialImageViews.resize(frameCount);
    depthImages.resize(frameCount);
    depthImageMemorys.resize(frameCount);
    depthImageViews.resize(frameCount);

    // HZB Vectors
    hzbImages.resize(frameCount);
    hzbImageMemorys.resize(frameCount);
    hzbImageViews.resize(frameCount);
    hzbMipImageViews.resize(frameCount);

    VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depthFormat = device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                                                      VK_IMAGE_TILING_OPTIMAL,
                                                      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

    for (uint32_t i = 0; i < frameCount; i++)
    {
      // Create Color Image
      VkImageCreateInfo imageInfo{};
      imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageInfo.imageType     = VK_IMAGE_TYPE_2D;
      imageInfo.extent.width  = extent.width;
      imageInfo.extent.height = extent.height;
      imageInfo.extent.depth  = 1;
      imageInfo.mipLevels     = mipLevels;
      imageInfo.arrayLayers   = 1;
      imageInfo.format        = colorFormat;
      imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

      if (useMipmaps)
      {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      }

      imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

      device.getMemory().createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImages[i], colorImageMemorys[i]);

      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = colorImages[i];
      viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format                          = colorFormat;
      viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel   = 0;
      viewInfo.subresourceRange.levelCount     = mipLevels;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &viewInfo, nullptr, &colorImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create color image view!");
      }

      // Create Color Attachment Image View (Mip Level 0 only)
      VkImageViewCreateInfo attachmentViewInfo       = viewInfo;
      attachmentViewInfo.subresourceRange.levelCount = 1;
      if (vkCreateImageView(device.device(), &attachmentViewInfo, nullptr, &colorAttachmentImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create color attachment image view!");
      }

      // Create Depth Image
      VkImageCreateInfo depthImageInfo{};
      depthImageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      depthImageInfo.imageType     = VK_IMAGE_TYPE_2D;
      depthImageInfo.extent.width  = extent.width;
      depthImageInfo.extent.height = extent.height;
      depthImageInfo.extent.depth  = 1;
      depthImageInfo.mipLevels     = mipLevels;
      depthImageInfo.arrayLayers   = 1;
      depthImageInfo.format        = depthFormat;
      depthImageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      depthImageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
      if (useMipmaps)
      {
        depthImageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      }
      depthImageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
      depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

      device.getMemory().createImageWithInfo(depthImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImages[i], depthImageMemorys[i]);

      VkImageViewCreateInfo depthViewInfo{};
      depthViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      depthViewInfo.image                           = depthImages[i];
      depthViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      depthViewInfo.format                          = depthFormat;
      depthViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      depthViewInfo.subresourceRange.baseMipLevel   = 0;
      depthViewInfo.subresourceRange.levelCount     = 1; // Framebuffer needs single mip level
      depthViewInfo.subresourceRange.baseArrayLayer = 0;
      depthViewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &depthViewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create depth image view!");
      }

      // Create per-mip views for HZB
      depthMipImageViews.resize(frameCount);
      depthMipImageViews[i].resize(mipLevels);
      for (uint32_t mip = 0; mip < mipLevels; mip++)
      {
        VkImageViewCreateInfo mipViewInfo           = depthViewInfo;
        mipViewInfo.subresourceRange.baseMipLevel   = mip;
        mipViewInfo.subresourceRange.levelCount     = 1;
        mipViewInfo.subresourceRange.baseArrayLayer = 0;
        mipViewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device.device(), &mipViewInfo, nullptr, &depthMipImageViews[i][mip]) != VK_SUCCESS)
        {
          throw std::runtime_error("failed to create depth mip image view!");
        }
        // std::cout << "Created Depth Mip View: Frame " << i << ", Mip " << mip << ", Handle " << depthMipImageViews[i][mip] <<'\n';
      }

      // Create HZB Images (R32_SFLOAT)
      VkImageCreateInfo hzbImageInfo{};
      hzbImageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      hzbImageInfo.imageType     = VK_IMAGE_TYPE_2D;
      hzbImageInfo.extent.width  = extent.width;
      hzbImageInfo.extent.height = extent.height;
      hzbImageInfo.extent.depth  = 1;
      hzbImageInfo.mipLevels     = mipLevels;
      hzbImageInfo.arrayLayers   = 1;
      hzbImageInfo.format        = VK_FORMAT_R32_SFLOAT;
      hzbImageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      hzbImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      hzbImageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      hzbImageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
      hzbImageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

      device.getMemory().createImageWithInfo(hzbImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, hzbImages[i], hzbImageMemorys[i]);

      // Full View
      VkImageViewCreateInfo hzbViewInfo{};
      hzbViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      hzbViewInfo.image                           = hzbImages[i];
      hzbViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      hzbViewInfo.format                          = VK_FORMAT_R32_SFLOAT;
      hzbViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      hzbViewInfo.subresourceRange.baseMipLevel   = 0;
      hzbViewInfo.subresourceRange.levelCount     = mipLevels;
      hzbViewInfo.subresourceRange.baseArrayLayer = 0;
      hzbViewInfo.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &hzbViewInfo, nullptr, &hzbImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create HZB image view!");
      }

      // Per-Mip Views
      hzbMipImageViews[i].resize(mipLevels);
      for (uint32_t mip = 0; mip < mipLevels; mip++)
      {
        VkImageViewCreateInfo mipViewInfo         = hzbViewInfo;
        mipViewInfo.subresourceRange.baseMipLevel = mip;
        mipViewInfo.subresourceRange.levelCount   = 1;

        if (vkCreateImageView(device.device(), &mipViewInfo, nullptr, &hzbMipImageViews[i][mip]) != VK_SUCCESS)
        {
          throw std::runtime_error("failed to create HZB mip image view!");
        }
      }
    }

    // Create Scene Color Copy images (mip chain, used for rough transmission blur)
    VkFormat sceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    for (uint32_t i = 0; i < frameCount; i++)
    {
      VkImageCreateInfo sceneInfo{};
      sceneInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      sceneInfo.imageType     = VK_IMAGE_TYPE_2D;
      sceneInfo.extent.width  = extent.width;
      sceneInfo.extent.height = extent.height;
      sceneInfo.extent.depth  = 1;
      sceneInfo.mipLevels     = mipLevels;
      sceneInfo.arrayLayers   = 1;
      sceneInfo.format        = sceneColorFormat;
      sceneInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      sceneInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      sceneInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      sceneInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
      sceneInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

      device.getMemory().createImageWithInfo(sceneInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sceneColorImages[i], sceneColorImageMemorys[i]);

      VkImageViewCreateInfo sceneView{};
      sceneView.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      sceneView.image                           = sceneColorImages[i];
      sceneView.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      sceneView.format                          = sceneColorFormat;
      sceneView.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      sceneView.subresourceRange.baseMipLevel   = 0;
      sceneView.subresourceRange.levelCount     = mipLevels;
      sceneView.subresourceRange.baseArrayLayer = 0;
      sceneView.subresourceRange.layerCount     = 1;

      if (vkCreateImageView(device.device(), &sceneView, nullptr, &sceneColorImageViews[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create scene color image view!");
      }
    }

    // Create G-buffer images (single mip, half-float)
    VkFormat gbufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    for (uint32_t i = 0; i < frameCount; i++)
    {
      auto createGbufferImage = [&](VkImage& image, VkDeviceMemory& mem, VkImageView& view) {
        VkImageCreateInfo info{};
        info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType     = VK_IMAGE_TYPE_2D;
        info.extent.width  = extent.width;
        info.extent.height = extent.height;
        info.extent.depth  = 1;
        info.mipLevels     = 1;
        info.arrayLayers   = 1;
        info.format        = gbufferFormat;
        info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.samples       = VK_SAMPLE_COUNT_1_BIT;
        info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        device.getMemory().createImageWithInfo(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, mem);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = image;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = gbufferFormat;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &view) != VK_SUCCESS)
        {
          throw std::runtime_error("failed to create gbuffer image view!");
        }
      };

      createGbufferImage(gbufferNormalImages[i], gbufferNormalMemorys[i], gbufferNormalImageViews[i]);
      createGbufferImage(gbufferAlbedoImages[i], gbufferAlbedoMemorys[i], gbufferAlbedoImageViews[i]);
      createGbufferImage(gbufferMaterialImages[i], gbufferMaterialMemorys[i], gbufferMaterialImageViews[i]);
    }

    // Create Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter     = VK_FILTER_LINEAR;
    samplerInfo.minFilter     = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias    = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod        = 0.0f;
    samplerInfo.maxLod        = static_cast<float>(mipLevels);
    samplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create texture sampler!");
    }

    // Create Depth Sampler (Reduction Mode Min/Max if supported, otherwise Linear)
    // For HZB we ideally want a reduction sampler, but for now we'll use a separate sampler for depth
    VkSamplerCreateInfo depthSamplerInfo = samplerInfo;
    // Use NEAREST for depth to avoid interpolation issues during HZB generation if we do it manually
    // But for sampling in shader, we might want LINEAR if we do PCF or similar.
    // For Occlusion Culling, we want conservative depth.
    depthSamplerInfo.magFilter  = VK_FILTER_NEAREST;
    depthSamplerInfo.minFilter  = VK_FILTER_NEAREST;
    depthSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(device.device(), &depthSamplerInfo, nullptr, &depthSampler) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create depth sampler!");
    }

    // Create HZB Sampler
    VkSamplerCreateInfo hzbSamplerInfo = samplerInfo;
    hzbSamplerInfo.magFilter           = VK_FILTER_NEAREST;
    hzbSamplerInfo.minFilter           = VK_FILTER_NEAREST;
    hzbSamplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(device.device(), &hzbSamplerInfo, nullptr, &hzbSampler) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create HZB sampler!");
    }
  }

  void FrameBuffer::createFramebuffers()
  {
    framebuffers.resize(frameCount);
    depthPrepassFramebuffers.resize(frameCount);
    loadDepthFramebuffers.resize(frameCount);
    loadColorDepthFramebuffers.resize(frameCount);
    gbufferFramebuffers.resize(frameCount);
    deferredLightingFramebuffers.resize(frameCount);

    for (size_t i = 0; i < frameCount; i++)
    {
      std::array<VkImageView, 2> attachments = {colorAttachmentImageViews[i], depthImageViews[i]};

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass      = renderPass;
      framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebufferInfo.pAttachments    = attachments.data();
      framebufferInfo.width           = extent.width;
      framebufferInfo.height          = extent.height;
      framebufferInfo.layers          = 1;

      if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create framebuffer!");
      }

      VkFramebufferCreateInfo prepassFramebufferInfo = framebufferInfo;
      prepassFramebufferInfo.renderPass              = depthPrepassRenderPass;
      if (vkCreateFramebuffer(device.device(), &prepassFramebufferInfo, nullptr, &depthPrepassFramebuffers[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create depth prepass framebuffer!");
      }

      VkFramebufferCreateInfo loadDepthFramebufferInfo = framebufferInfo;
      loadDepthFramebufferInfo.renderPass              = renderPassLoadDepth;
      if (vkCreateFramebuffer(device.device(), &loadDepthFramebufferInfo, nullptr, &loadDepthFramebuffers[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create load-depth framebuffer!");
      }

      VkFramebufferCreateInfo loadColorDepthFramebufferInfo = framebufferInfo;
      loadColorDepthFramebufferInfo.renderPass              = renderPassLoadColorDepth;
      if (vkCreateFramebuffer(device.device(), &loadColorDepthFramebufferInfo, nullptr, &loadColorDepthFramebuffers[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create load-color-depth framebuffer!");
      }

      // G-buffer framebuffer: N, Albedo, Material, HDR (emissive), Depth
      std::array<VkImageView, 5> gbufferAttachments = {gbufferNormalImageViews[i], gbufferAlbedoImageViews[i], gbufferMaterialImageViews[i], colorAttachmentImageViews[i], depthImageViews[i]};
      VkFramebufferCreateInfo    gbufferFbInfo{};
      gbufferFbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      gbufferFbInfo.renderPass      = gbufferRenderPass;
      gbufferFbInfo.attachmentCount = static_cast<uint32_t>(gbufferAttachments.size());
      gbufferFbInfo.pAttachments    = gbufferAttachments.data();
      gbufferFbInfo.width           = extent.width;
      gbufferFbInfo.height          = extent.height;
      gbufferFbInfo.layers          = 1;

      if (vkCreateFramebuffer(device.device(), &gbufferFbInfo, nullptr, &gbufferFramebuffers[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create gbuffer framebuffer!");
      }

      // Deferred lighting framebuffer (HDR color + depth)
      std::array<VkImageView, 1> litAttachments = {colorAttachmentImageViews[i]};
      VkFramebufferCreateInfo    litFbInfo{};
      litFbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      litFbInfo.renderPass      = deferredLightingRenderPass;
      litFbInfo.attachmentCount = static_cast<uint32_t>(litAttachments.size());
      litFbInfo.pAttachments    = litAttachments.data();
      litFbInfo.width           = extent.width;
      litFbInfo.height          = extent.height;
      litFbInfo.layers          = 1;

      if (vkCreateFramebuffer(device.device(), &litFbInfo, nullptr, &deferredLightingFramebuffers[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create deferred lighting framebuffer!");
      }
    }
  }

  VkDescriptorImageInfo FrameBuffer::getDescriptorImageInfo(int index) const
  {
    return VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = colorImageViews[index],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  VkDescriptorImageInfo FrameBuffer::getSceneColorDescriptorImageInfo(int index) const
  {
    return VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = sceneColorImageViews[index],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  VkDescriptorImageInfo FrameBuffer::getGbufferNormalImageInfo(int index) const
  {
    return VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = gbufferNormalImageViews[index],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  VkDescriptorImageInfo FrameBuffer::getGbufferAlbedoImageInfo(int index) const
  {
    return VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = gbufferAlbedoImageViews[index],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  VkDescriptorImageInfo FrameBuffer::getGbufferMaterialImageInfo(int index) const
  {
    return VkDescriptorImageInfo{
            .sampler     = sampler,
            .imageView   = gbufferMaterialImageViews[index],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  void FrameBuffer::beginRenderPass(VkCommandBuffer commandBuffer, int frameIndex)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPass;
    renderPassInfo.framebuffer       = framebuffers[frameIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void FrameBuffer::beginDepthPrepassRenderPass(VkCommandBuffer commandBuffer, int frameIndex)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = depthPrepassRenderPass;
    renderPassInfo.framebuffer       = depthPrepassFramebuffers[frameIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void FrameBuffer::beginRenderPassLoadDepth(VkCommandBuffer commandBuffer, int frameIndex)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPassLoadDepth;
    renderPassInfo.framebuffer       = loadDepthFramebuffers[frameIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void FrameBuffer::beginRenderPassLoadColorDepth(VkCommandBuffer commandBuffer, int frameIndex)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPassLoadColorDepth;
    renderPassInfo.framebuffer       = loadColorDepthFramebuffers[frameIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void FrameBuffer::beginGbufferRenderPass(VkCommandBuffer commandBuffer, int frameIndex)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = gbufferRenderPass;
    renderPassInfo.framebuffer       = gbufferFramebuffers[frameIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    std::array<VkClearValue, 5> clearValues{};
    clearValues[0].color        = {{0.0f, 0.0f, 1.0f, 0.0f}}; // default normal
    clearValues[1].color        = {{0.0f, 0.0f, 0.0f, 1.0f}}; // albedo
    clearValues[2].color        = {{0.0f, 1.0f, 1.0f, 1.5f}}; // roughness/metallic/ao/ior defaults (rough=0, metal=1?) updated in shader anyway
    clearValues[3].color        = {{0.0f, 0.0f, 0.0f, 1.0f}}; // HDR emissive base
    clearValues[4].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void FrameBuffer::beginDeferredLightingRenderPass(VkCommandBuffer commandBuffer, int frameIndex)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = deferredLightingRenderPass;
    renderPassInfo.framebuffer       = deferredLightingFramebuffers[frameIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;

    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues    = nullptr;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void FrameBuffer::endRenderPass(VkCommandBuffer commandBuffer) const
  {
    vkCmdEndRenderPass(commandBuffer);
  }

  void FrameBuffer::generateMipmaps(VkCommandBuffer commandBuffer, int frameIndex)
  {
    if (!useMipmaps) return;

    VkImage image     = colorImages[frameIndex];
    int32_t mipWidth  = extent.width;
    int32_t mipHeight = extent.height;

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = image;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    // Transition Mip 0 to TRANSFER_SRC
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    for (uint32_t i = 1; i < mipLevels; i++)
    {
      // Transition Mip i to TRANSFER_DST
      barrier.subresourceRange.baseMipLevel = i;
      barrier.oldLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.srcAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
      barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

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

      // Transition Mip i-1 to SHADER_READ_ONLY
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

      // Transition Mip i to TRANSFER_SRC (for next loop)
      if (i < mipLevels - 1)
      {
        barrier.subresourceRange.baseMipLevel = i;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
      }

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition Last Mip to SHADER_READ_ONLY
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  }

  void FrameBuffer::generateSceneColorMipmaps(VkCommandBuffer commandBuffer, int frameIndex)
  {
    if (!useMipmaps) return;

    VkImage image     = sceneColorImages[frameIndex];
    int32_t mipWidth  = extent.width;
    int32_t mipHeight = extent.height;

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = image;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    // Transition Mip 0 (just copied) to TRANSFER_SRC
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    for (uint32_t i = 1; i < mipLevels; i++)
    {
      // Transition Mip i to TRANSFER_DST
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

      // Transition Mip i-1 to SHADER_READ_ONLY
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

      // Transition Mip i to TRANSFER_SRC for next loop
      if (i < mipLevels - 1)
      {
        barrier.subresourceRange.baseMipLevel = i;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
      }

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition Last Mip to SHADER_READ_ONLY
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  }

} // namespace engine
