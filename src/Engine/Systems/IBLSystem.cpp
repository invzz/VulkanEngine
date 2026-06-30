#include "Engine/Systems/IBLSystem.hpp"

#include "Engine/Systems/IBL/BRDFLUT.hpp"
#include "Engine/Systems/IBL/IrradianceIBL.hpp"
#include "Engine/Systems/IBL/PrefilteredEnvIBL.hpp"

namespace engine {

    IBLSystem::IBLSystem(Device& device) : device_{device} {
        irradiance_  = std::make_unique<ibl::IrradianceIBL>(device_);
        prefiltered_ = std::make_unique<ibl::PrefilteredEnvIBL>(device_);
        brdfLUT_     = std::make_unique<ibl::BRDFLUT>(device_);

        // Ensure descriptor bindings are always valid even before any environment skybox is loaded.
        // This creates tiny black fallback textures (irradiance/prefilter cubemaps + BRDF LUT).
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
            // Update settings
            settings_ = nextSettings_;

            // Regenerate
            generateFromSkybox(*nextSkybox_);

            // Reset flag
            regenerationRequested_ = false;
            nextSkybox_            = nullptr;
        }
    }

    void IBLSystem::generateFromSkybox(Skybox& skybox) {
        // Industry-standard runtime behavior:
        // - BRDF LUT is global/static (generate once per device/settings)
        // - Irradiance/prefilter depend on the environment

        brdfLUT_->ensureForSettings(settings_);

        // Drop only the environment-dependent image resources.
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
