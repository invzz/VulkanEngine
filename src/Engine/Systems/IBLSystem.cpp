#include "Engine/Systems/IBLSystem.hpp"

#include "Engine/Systems/IBL/BRDFLUT.hpp"
#include "Engine/Systems/IBL/IrradianceIBL.hpp"
#include "Engine/Systems/IBL/PrefilteredEnvIBL.hpp"
namespace engine {
    IBLSystem::IBLSystem(Device& device) : device_{device} {
        irradiance_  = std::make_unique<ibl::IrradianceIBL>(device_);
        prefiltered_ = std::make_unique<ibl::PrefilteredEnvIBL>(device_);
        brdfLUT_     = std::make_unique<ibl::BRDFLUT>(device_);
        createFallbackResources();
    }
    IBLSystem::~IBLSystem() {
        cleanup();
    }
    VkDescriptorImageInfo IBLSystem::getIrradianceDescriptorInfo() const {
        return irradiance_->getDescriptorInfo();
    }
    VkDescriptorImageInfo IBLSystem::getPrefilteredDescriptorInfo() const {
        return prefiltered_->getDescriptorInfo();
    }
    VkDescriptorImageInfo IBLSystem::getBRDFLUTDescriptorInfo() const {
        return brdfLUT_->getDescriptorInfo();
    }
    void IBLSystem::updateSettings(const Settings& settings) {
        settings_ = settings;
    }
    void IBLSystem::requestRegeneration(const Settings& newSettings, Skybox& skybox) {
        nextSettings_          = newSettings;
        nextSkybox_            = &skybox;
        regenerationRequested_ = true;
    }
    void IBLSystem::update() {
        if (regenerationRequested_ && (nextSkybox_ != nullptr)) {
            settings_ = nextSettings_;
            generateFromSkybox(*nextSkybox_);
            regenerationRequested_ = false;
            nextSkybox_            = nullptr;
        }
    }
    void IBLSystem::generateFromSkybox(Skybox& skybox) {
        brdfLUT_->ensureForSettings(settings_);
        irradiance_->deferDestroyImageResources();
        prefiltered_->deferDestroyImageResources();
        irradiance_->createForSettings(settings_);
        prefiltered_->createForSettings(settings_);
        irradiance_->ensurePipelineResources();
        irradiance_->generateFromSkybox(skybox, settings_);
        prefiltered_->ensurePipelineResources();
        prefiltered_->generateFromSkybox(skybox, settings_);
        generated_ = true;
        generationCounter_++;
    }
}  // namespace engine
