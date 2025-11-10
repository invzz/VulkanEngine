#include "3dEngine/SimpleRenderSystem.hpp"

#include "3dEngine/Exceptions.hpp"

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>

namespace engine {

  struct SimplePushConstantData
  {
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
  };

  SimpleRenderSystem::SimpleRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device(device)
  {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
  }

  void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
  {
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(SimplePushConstantData),
    };

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
    };
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create pipeline layout!");
    }
  }
  SimpleRenderSystem::~SimpleRenderSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
  }

  void SimpleRenderSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipeline = std::make_unique<Pipeline>(device, SHADER_PATH "/simple_shader_3d.vert.spv", SHADER_PATH "/simple_shader_3d.frag.spv", pipelineConfig);
  }

  void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo, const std::vector<GameObject>& gameObjects)
  {
    pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    for (const auto& gameObject : gameObjects)
    {
      SimplePushConstantData push{};

      push.modelMatrix  = gameObject.transform.modelTransform();
      push.normalMatrix = glm::mat4{gameObject.transform.normalMatrix()};

      vkCmdPushConstants(frameInfo.commandBuffer,
                         pipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,
                         sizeof(SimplePushConstantData),
                         &push);
      gameObject.model->bind(frameInfo.commandBuffer);
      gameObject.model->draw(frameInfo.commandBuffer);
    }
  }
} // namespace engine
