#include "Editor/Infrastructure/RuntimeStateAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

    RuntimeStateAdapter::RuntimeStateAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    bool& RuntimeStateAdapter::showSkyboxRef() {
        return engineState_.showSkyboxRef();
    }

    bool& RuntimeStateAdapter::showGridRef() {
        return engineState_.showGridRef();
    }

    bool& RuntimeStateAdapter::showDebugObjectsRef() {
        return engineState_.showDebugObjectsRef();
    }

    bool& RuntimeStateAdapter::showColliderWireframesRef() {
        return engineState_.showColliderWireframesRef();
    }

    bool& RuntimeStateAdapter::physicsSimulationRunningRef() {
        return engineState_.physicsSimulationRunningRef();
    }

    bool& RuntimeStateAdapter::solidGroundEnabledRef() {
        return engineState_.solidGroundEnabledRef();
    }

    SkyboxSettings& RuntimeStateAdapter::skySettingsRef() {
        return engineState_.skySettingsRef();
    }

    ShadowSettings& RuntimeStateAdapter::shadowSettingsRef() {
        return engineState_.shadowSettingsRef();
    }

    PostProcessPushConstants& RuntimeStateAdapter::postProcessPushRef() {
        return engineState_.postProcessPushRef();
    }

}  // namespace engine
