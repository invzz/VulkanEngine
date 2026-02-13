#include "Engine/Systems/IBL/BRDFLUT.hpp"
#include "Engine/Systems/IBL/IrradianceIBL.hpp"
#include "Engine/Systems/IBL/PrefilteredEnvIBL.hpp"
#include "Engine/Systems/IBLSystem.hpp"

namespace engine {

void IBLSystem::resetToFallback() {
  // Destroy current IBL resources (environment + BRDF LUT), then recreate the tiny black fallbacks.
  // We intentionally reset everything so descriptor infos always point at valid views/samplers.
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

  // Cleanup is a hard tear-down and may run while frames are still queued.
  vkDeviceWaitIdle(dev);

  // Ensure any previously deferred destroys are executed before we start tearing down.
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
