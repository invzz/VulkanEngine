#include "Engine/Systems/CameraSystem.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  struct CameraDebugPush
  {
    glm::mat4 modelMatrix;
    glm::vec4 color;
    float     fovY;
    float     aspectRatio;
    float     nearZ;
    float     farZ;
  };

  CameraSystem::CameraSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device(device)
  {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
  }

  CameraSystem::~CameraSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
  }

  void CameraSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
  {
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(CameraDebugPush),
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
      throw std::runtime_error("failed to create camera debug pipeline layout!");
    }
  }

  void CameraSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.renderPass                 = renderPass;
    pipelineConfig.pipelineLayout             = pipelineLayout;
    pipelineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    pipeline = std::make_unique<Pipeline>(device, SHADER_PATH "/debug_frustum.vert.spv", SHADER_PATH "/debug_frustum.frag.spv", pipelineConfig);
  }

  void CameraSystem::render(FrameInfo& frameInfo) const
  {
    pipeline->bind(frameInfo.commandBuffer);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    auto& registry = frameInfo.scene->getRegistry();

    // Only render if we have a selected entity that is a camera
    if (registry.valid(frameInfo.selectedEntity) && registry.all_of<CameraComponent, TransformComponent>(frameInfo.selectedEntity))
    {
      // Don't render the active camera's frustum (it would be weird/invisible)
      if (frameInfo.selectedEntity == frameInfo.cameraEntity) return;

      auto [cameraComp, transform] = registry.get<CameraComponent, TransformComponent>(frameInfo.selectedEntity);

      glm::mat4 modelMatrix = glm::mat4(1.0f);
      modelMatrix           = glm::translate(modelMatrix, transform.translation);
      modelMatrix           = glm::rotate(modelMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
      modelMatrix           = glm::rotate(modelMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
      modelMatrix           = glm::rotate(modelMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

      CameraDebugPush push{};
      push.modelMatrix = modelMatrix;
      push.color       = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
      push.fovY        = glm::radians(cameraComp.fovY);
      push.aspectRatio = 16.0f / 9.0f; // Approximation, or use actual aspect ratio if stored
      push.nearZ       = cameraComp.nearZ;
      push.farZ        = cameraComp.farZ;

      vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

      vkCmdDraw(frameInfo.commandBuffer, 24, 1, 0, 0);
    }
  }

  void CameraSystem::update(FrameInfo& frameInfo, float aspectRatio) const
  {
    auto& registry = frameInfo.scene->getRegistry();
    if (registry.valid(frameInfo.cameraEntity))
    {
      // Check if the entity has the required components
      if (registry.all_of<CameraComponent, TransformComponent>(frameInfo.cameraEntity))
      {
        auto&       cameraComp = registry.get<CameraComponent>(frameInfo.cameraEntity);
        const auto& transform  = registry.get<TransformComponent>(frameInfo.cameraEntity);

        updateCamera(cameraComp, transform, aspectRatio);

        // Sync the frameInfo camera with the component camera
        // This ensures the renderer uses the updated camera matrices
        frameInfo.camera = cameraComp.camera;
      }
    }
  }

  void CameraSystem::updateCamera(CameraComponent& cameraComp, const TransformComponent& transform, float aspectRatio) const
  {
    // Update projection
    if (!cameraComp.isOrthographic)
    {
      cameraComp.camera.setPerspectiveProjection(glm::radians(cameraComp.fovY), aspectRatio, cameraComp.nearZ, cameraComp.farZ);
    }
    else
    {
      // For orthographic, we need to define the bounds.
      // Assuming orthoSize is the height, width is derived from aspect ratio.
      float orthoHeight = cameraComp.orthoSize;
      float orthoWidth  = aspectRatio * orthoHeight;
      cameraComp.camera.setOrtographicProjection(-orthoWidth, orthoWidth, -orthoHeight, orthoHeight, cameraComp.nearZ, cameraComp.farZ);
    }

    // Update view
    cameraComp.camera.setViewYXZ(transform.translation, transform.rotation);

    // Update frustum
    cameraComp.camera.updateFrustum();
  }
} // namespace engine
