#include "Engine/Systems/PostProcessingSystem.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <array>
#include <glm/glm.hpp>
#include <stdexcept>

namespace engine {

  PostProcessingSystem::PostProcessingSystem(Device& device, VkRenderPass renderPass, std::vector<VkDescriptorSetLayout> setLayouts) : device{device}
  {
    createPipelineLayout(setLayouts);
    try
    {
      createPipeline(renderPass);
    }
    catch (...)
    {
      vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
      throw;
    }
  }

  PostProcessingSystem::~PostProcessingSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
  }

  void PostProcessingSystem::createPipelineLayout(std::vector<VkDescriptorSetLayout> setLayouts)
  {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(PostProcessPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts            = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create pipeline layout!");
    }
  }

  void PostProcessingSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    // Disable depth test/write for post process quad
    pipelineConfig.depthStencilInfo.depthTestEnable  = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

    // Rasterization: Cull mode none
    pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

    // Empty vertex input state (generating vertices in shader)
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.attributeDescriptions.clear();

    pipeline = std::make_unique<Pipeline>(device, SHADER_PATH "/post_process.vert.spv", SHADER_PATH "/post_process.frag.spv", pipelineConfig);
  }

  void PostProcessingSystem::render(FrameInfo& frameInfo, VkDescriptorSet descriptorSet, const PostProcessPushConstants& push)
  {
    pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostProcessPushConstants), &push);

    vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
  }

} // namespace engine
