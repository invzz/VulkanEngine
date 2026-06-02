#include "Editor/Infrastructure/EnvironmentLightingAdapter.hpp"

#include <iostream>
#include <string>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Skybox.hpp"

namespace engine {

EnvironmentLightingAdapter::EnvironmentLightingAdapter(Device& device, EngineState& engineState)
    : device_(device), engineState_(engineState) {}

void EnvironmentLightingAdapter::syncEnvironmentLighting(bool showSkyboxEnabled) {
  auto rendering  = engineState_.renderingService().view();
  auto sceneState = engineState_.sceneRuntimeService().view();

  if ((rendering.showSkybox != nullptr) && showSkyboxEnabled && (sceneState.skybox == nullptr)) {
    std::cout << "[EnvironmentLightingAdapter] Loading skybox..." << '\n';
    engineState_.skyboxRef() = Skybox::loadFromFolder(device_, std::string(TEXTURE_PATH) + "/skybox/Yokohama", "jpg");

    if (!rendering.iblSystem->loadFromDisk(std::string(TEXTURE_PATH) + "/ibl/Yokohama")) {
      std::cout << "[EnvironmentLightingAdapter] No prebaked IBL found for Yokohama (assets/textures/ibl/Yokohama). Using fallback until you regenerate/bake." << '\n';
    }
  }

  if ((rendering.showSkybox != nullptr) && !showSkyboxEnabled && (sceneState.skybox != nullptr)) {
    std::cout << "[EnvironmentLightingAdapter] Skybox disabled. Resetting IBL to fallback." << '\n';
    engineState_.skyboxRef().reset();
    rendering.iblSystem->resetToFallback();
  }

  rendering.iblSystem->update();

  uint64_t const newGeneration = rendering.iblSystem->getGenerationCounter();
  if (newGeneration == iblGenerationCounter_) {
    return;
  }

  auto irradianceInfo = rendering.iblSystem->getIrradianceDescriptorInfo();
  auto prefilterInfo  = rendering.iblSystem->getPrefilteredDescriptorInfo();
  auto brdfInfo       = rendering.iblSystem->getBRDFLUTDescriptorInfo();

  auto& deferredIblSets = engineState_.deferredIblDescriptorSetsRef();
  for (auto& deferredIblDescriptorSet : deferredIblSets) {
    DescriptorWriter(engineState_.deferredIblSetLayoutRef(), engineState_.deferredIblPoolRef())
        .writeImage(0, &irradianceInfo)
        .writeImage(1, &prefilterInfo)
        .writeImage(2, &brdfInfo)
        .overwrite(deferredIblDescriptorSet);
  }

  iblGenerationCounter_ = newGeneration;
}

}  // namespace engine