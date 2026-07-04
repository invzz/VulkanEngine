#include "Engine/Systems/IBL/BRDFLUT.hpp"
#include "Engine/Systems/IBL/IrradianceIBL.hpp"
#include "Engine/Systems/IBL/PrefilteredEnvIBL.hpp"
#include "Engine/Systems/IBLSystem.hpp"
namespace engine {
    void IBLSystem::resetToFallback() {
        irradiance_->deferDestroyImageResources();
        prefiltered_->deferDestroyImageResources();
        brdfLUT_->deferDestroyImageResources();
        createFallbackResources();
    }
    void IBLSystem::createFallbackResources() {
        irradiance_->createFallback();
        prefiltered_->createFallback();
        brdfLUT_->createFallback();
        generated_ = false;
        generationCounter_++;
    }
    void IBLSystem::cleanup() {
        VkDevice dev = device_.device();
        vkDeviceWaitIdle(dev);
        device_.flushAllDeferred();
        generated_ = false;
        if (irradiance_) {
            irradiance_->destroyImmediate();
        }
        if (prefiltered_) {
            prefiltered_->destroyImmediate();
        }
        if (brdfLUT_) {
            brdfLUT_->destroyImmediate();
        }
    }
}  // namespace engine
