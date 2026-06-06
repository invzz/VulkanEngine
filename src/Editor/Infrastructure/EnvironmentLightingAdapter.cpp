#include "Editor/Infrastructure/EnvironmentLightingAdapter.hpp"

#include <iostream>
#include <string>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Descriptors.hpp"
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

    void EnvironmentLightingAdapter::loadSkybox(Device& device, const char* path) {
        engineState_.skyboxRef() = Skybox::loadFromFolder(device, std::string(path), "jpg");
    }

    void EnvironmentLightingAdapter::resetSkybox() {
        engineState_.skyboxRef().reset();
    }

    bool EnvironmentLightingAdapter::hasSkybox() const {
        return engineState_.sceneRuntimeService().view().skybox != nullptr;
    }

    bool EnvironmentLightingAdapter::loadIBLFromDisk(const char* path) {
        return engineState_.renderingService().view().iblSystem->loadFromDisk(std::string(path));
    }

    void EnvironmentLightingAdapter::resetIBLToFallback() {
        engineState_.renderingService().view().iblSystem->resetToFallback();
    }

    void EnvironmentLightingAdapter::updateIBL() {
        engineState_.renderingService().view().iblSystem->update();
    }

    uint64_t EnvironmentLightingAdapter::getIBLGenerationCounter() const {
        return engineState_.renderingService().view().iblSystem->getGenerationCounter();
    }

    void EnvironmentLightingAdapter::writeIBLDescriptors(
        const VkDescriptorImageInfo&  irradianceInfo,
        const VkDescriptorImageInfo&  prefilterInfo,
        const VkDescriptorImageInfo&  brdfInfo,
        std::vector<VkDescriptorSet>& descriptorSets,
        DescriptorSetLayout const&    layout,
        DescriptorPool const&         pool) {
        for (auto& descriptorSet : descriptorSets) {
            DescriptorWriter(const_cast<DescriptorSetLayout&>(layout), const_cast<DescriptorPool&>(pool))
                .writeImage(0, const_cast<VkDescriptorImageInfo*>(&irradianceInfo))
                .writeImage(1, const_cast<VkDescriptorImageInfo*>(&prefilterInfo))
                .writeImage(2, const_cast<VkDescriptorImageInfo*>(&brdfInfo))
                .overwrite(descriptorSet);
        }
    }

    bool* EnvironmentLightingAdapter::showSkyboxRef() {
        return engineState_.renderingService().view().showSkybox;
    }

    Skybox* EnvironmentLightingAdapter::getSkybox() {
        return engineState_.sceneRuntimeService().view().skybox;
    }

    SkyboxSettings* EnvironmentLightingAdapter::getSkySettings() {
        return engineState_.sceneRuntimeService().view().skySettings;
    }

    ShadowSettings* EnvironmentLightingAdapter::getShadowSettings() {
        return engineState_.sceneRuntimeService().view().shadowSettings;
    }

}  // namespace engine
