#pragma once

#include <cstdint>

#include "entt/entity/fwd.hpp"

namespace engine {

    struct SceneRuntimeState {
        bool&         physicsSimulationRunning;
        entt::entity& selectedEntity;
        entt::entity& cameraEntity;
        uint32_t&     selectedObjectId;
        bool&         pendingUpdateCameraAfterSceneLoad;
        bool          solidGroundEnabled = true;
    };

}  // namespace engine
