#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LIGHTSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LIGHTSYSTEM_HPP

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

class LightSystem {
 public:
  LightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
  ~LightSystem();

  // delete copy operations
  LightSystem(const LightSystem&) = delete;
  LightSystem& operator=(const LightSystem&) = delete;

  void render(FrameInfo& frameInfo);
  void update(FrameInfo& frameInfo, GlobalUbo& ubo) const;

  // Update target-locked light rotation (call when light position or target changes)
  static void updateTargetLockedLight(entt::entity entity, Scene* scene);

  // Update rotations of all target-locked lights in the scene.
  // Intended to be called once per frame before gathering light data for rendering.
  static void updateAllTargetLockedLights(Scene& scene);

 private:
  void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createPipeline(VkRenderPass renderPass);
  void createDirectionalLightPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createDirectionalLightPipeline(VkRenderPass renderPass);
  void createSpotLightPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createSpotLightPipeline(VkRenderPass renderPass);

  Device& device;

  // Point light rendering
  std::unique_ptr<Pipeline> pipeline;
  VkPipelineLayout pipelineLayout;

  // Directional light rendering
  std::unique_ptr<Pipeline> directionalPipeline;
  VkPipelineLayout directionalPipelineLayout;

  // Spot light rendering
  std::unique_ptr<Pipeline> spotPipeline;
  VkPipelineLayout spotPipelineLayout;
};
}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LIGHTSYSTEM_HPP
