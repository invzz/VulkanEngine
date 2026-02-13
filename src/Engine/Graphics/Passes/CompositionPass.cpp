#include "Engine/Graphics/Passes/CompositionPass.hpp"

#include "Editor/ui/UIManager.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

namespace {
struct SunInfo {
  glm::vec3 directionToSun{0.0f, 1.0f, 0.0f};
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float intensity{0.0f};
  bool valid{false};
};

SunInfo queryPrimaryDirectionalLightSunInfo(engine::Scene const& scene) {
  SunInfo info{};

  auto const& registry = scene.getRegistry();
  auto view = registry.view<engine::TransformComponent, engine::DirectionalLightComponent>();
  for (auto entity : view) {
    auto const& transform = view.get<engine::TransformComponent>(entity);
    auto const& light = view.get<engine::DirectionalLightComponent>(entity);

    glm::vec3 const lightRayDir = glm::normalize(transform.getForwardDir());
    info.directionToSun = -lightRayDir;
    info.color = light.color;
    info.intensity = light.intensity;
    info.valid = true;
    break;
  }

  return info;
}
}  // namespace

namespace engine {

void CompositionPass::execute(FrameInfo& frameInfo) {
  // Update post-process descriptors
  auto imageInfo = renderer_.getOffscreenImageInfo(frameInfo.frameIndex);
  auto depthInfo = renderer_.getDepthImageInfo(frameInfo.frameIndex);

  // Refresh the post-process descriptor set each frame (image/depth views may change on resize)
  DescriptorWriter(*engineState_->postProcessSetLayout, *engineState_->postProcessPool).writeImage(0, &imageInfo).writeImage(1, &depthInfo).overwrite(engineState_->postProcessDescriptorSets[frameInfo.frameIndex]);

  engineState_->postProcessPush.inverseProjection = glm::inverse(camera_.getProjection());
  engineState_->postProcessPush.projection = camera_.getProjection();

  engineState_->postProcessPush.debugMode = frameInfo.debugMode;

  // if (fogSettings_.enableGodRays)
  // {
  //   SunInfo const   sunInfo = queryPrimaryDirectionalLightSunInfo(scene_);
  //   glm::vec3 const sunDir  = sunInfo.directionToSun;

  //   glm::vec3 const sunWorldPos = camera_.getPosition() + sunDir * 1000.0f;
  //   glm::vec4 const clipPos     = camera_.getProjection() * camera_.getView() * glm::vec4(sunWorldPos, 1.0f);

  //   if (sunInfo.valid && (sunInfo.intensity > 0.0f) && (clipPos.w > 0.0f))
  //   {
  //     glm::vec3 const ndc           = glm::vec3(clipPos) / clipPos.w;
  //     glm::vec2 const screenPos     = glm::vec2(ndc.x, ndc.y) * 0.5f + 0.5f;
  //     postProcessPush_.sunScreenPos = glm::vec4(screenPos, 1.0f, 0.0f);
  //   }
  //   else
  //   {
  //     postProcessPush_.sunScreenPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
  //   }

  //   float const sunHeight           = sunInfo.directionToSun.y;
  //   float       intensityMultiplier = 1.0f;
  //   float       decayModifier       = 0.0f;

  //   if (sunHeight > -0.1f && sunHeight < 0.5f)
  //   {
  //     float const dist  = glm::abs(sunHeight - 0.1f);
  //     float const boost = glm::max(0.0f, 1.0f - (dist / 0.4f));

  //     intensityMultiplier = 1.0f + (boost * 2.0f);
  //     decayModifier       = boost * 0.015f;
  //   }

  //   postProcessPush_.godRayDensity  = fogSettings_.godRayDensity;
  //   postProcessPush_.godRayWeight   = fogSettings_.godRayWeight * intensityMultiplier;
  //   postProcessPush_.godRayDecay    = glm::clamp(fogSettings_.godRayDecay + decayModifier, 0.0f, 0.995f);
  //   postProcessPush_.godRayExposure = fogSettings_.godRayExposure * intensityMultiplier;
  // }
  // else
  // {
  //   postProcessPush_.sunScreenPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
  // }

  engineState_->postProcessingSystem->render(frameInfo, engineState_->postProcessDescriptorSets[frameInfo.frameIndex], engineState_->postProcessPush);
  engineState_->uiManager->render(frameInfo, frameInfo.commandBuffer, true);
}

}  // namespace engine
