#include "Engine/Systems/GridRenderSystem.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace engine {

  GridRenderSystem::GridRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device_{device}
  {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
  }

  GridRenderSystem::~GridRenderSystem()
  {
    vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
  }

  void GridRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
  {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

    VkPipelineLayoutCreateInfo const pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges    = nullptr,
    };

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create grid pipeline layout!");
    }
  }

  void GridRenderSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout_ != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.renderPass                 = renderPass;
    pipelineConfig.pipelineLayout             = pipelineLayout_;
    pipelineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    pipeline_ = std::make_unique<Pipeline>(device_, std::string(SHADER_PATH) + "grid.vert.spv", std::string(SHADER_PATH) + "grid.frag.spv", pipelineConfig);
  }

  void GridRenderSystem::render(FrameInfo& frameInfo) const
  {
    pipeline_->bind(frameInfo.commandBuffer);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    constexpr int gridExtent   = 20;
    constexpr int linesPerAxis = gridExtent * 2 + 1;
    constexpr int totalLines   = linesPerAxis * 2;
    constexpr int totalVerts   = totalLines * 2;

    vkCmdDraw(frameInfo.commandBuffer, totalVerts, 1, 0, 0);
  }

} // namespace engine
