#include "Engine/Systems/DeferredLightingSystem.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

DeferredLightingSystem::DeferredLightingSystem(Device& device, VkRenderPass renderPass, std::vector<VkDescriptorSetLayout> setLayouts) : device{device} {
  createPipelineLayout(std::move(setLayouts));
  createPipeline(renderPass);
}

DeferredLightingSystem::~DeferredLightingSystem() {
  if (pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    pipelineLayout = VK_NULL_HANDLE;
  }
}

void DeferredLightingSystem::createPipelineLayout(std::vector<VkDescriptorSetLayout> setLayouts) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();

  if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create deferred lighting pipeline layout!");
  }
}

void DeferredLightingSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  Pipeline::defaultPipelineConfigInfo(pipelineConfig);

  pipelineConfig.renderPass = renderPass;
  pipelineConfig.pipelineLayout = pipelineLayout;

  // Additive lighting over pre-filled HDR (emissive was written earlier).
  pipelineConfig.colorBlendAttachment.blendEnable = VK_TRUE;
  pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  pipelineConfig.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  // Keep alpha deterministic (overwrite).
  pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  pipelineConfig.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  // Fullscreen triangle: no depth test/write.
  pipelineConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
  pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

  // Rasterization: Cull mode none
  pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

  // Empty vertex input state (generating vertices in shader)
  pipelineConfig.bindingDescriptions.clear();
  pipelineConfig.attributeDescriptions.clear();

  pipeline = std::make_unique<Pipeline>(device, std::string(SHADER_PATH) + R"(post_process.vert.spv)", std::string(SHADER_PATH) + R"(deferred_lighting.frag.spv)", pipelineConfig);
}

void DeferredLightingSystem::render(FrameInfo& frameInfo, VkDescriptorSet globalSet, VkDescriptorSet gbufferSet, VkDescriptorSet shadowSet, VkDescriptorSet iblSet) {
  pipeline->bind(frameInfo.commandBuffer);

  // Bind descriptor sets in pipeline layout order: global (0), gbuffer (1), shadow (2), ibl (3)
  std::array<VkDescriptorSet, 4> sets{globalSet, gbufferSet, shadowSet, iblSet};
  vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

  vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
}

}  // namespace engine
