#include "Engine/Application/UseCases/SyncEnvironmentLightingUseCase.hpp"

namespace engine {

SyncEnvironmentLightingUseCase::SyncEnvironmentLightingUseCase(IEnvironmentLightingPort& environmentLighting)
    : environmentLighting_(environmentLighting) {}

void SyncEnvironmentLightingUseCase::execute(bool showSkyboxEnabled) const {
  environmentLighting_.syncEnvironmentLighting(showSkyboxEnabled);
}

}  // namespace engine