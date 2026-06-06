#include "Engine/State/StateServices.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"

namespace engine {

    RenderingStateService::RenderingStateService(EngineState& engineState)
        : engineState_(engineState) {}

    RenderingStateView RenderingStateService::view() const {
        return engineState_.renderingState();
    }

    SceneRuntimeService::SceneRuntimeService(EngineState& engineState)
        : engineState_(engineState) {}

    SceneRuntimeStateView SceneRuntimeService::view() const {
        return engineState_.sceneState();
    }

    InputStateService::InputStateService(EngineState& engineState)
        : engineState_(engineState) {}

    InputStateView InputStateService::view() const {
        return engineState_.inputState();
    }

    ResourceStateService::ResourceStateService(EngineState& engineState)
        : engineState_(engineState) {}

    ResourceStateView ResourceStateService::view() const {
        return engineState_.resourceState();
    }

    AnimationRuntimeService::AnimationRuntimeService(EngineState& engineState)
        : engineState_(engineState) {}

    AnimationSystem* AnimationRuntimeService::animation() const {
        return engineState_.animationSystem.get();
    }

    PhysicsRuntimeService::PhysicsRuntimeService(EngineState& engineState)
        : engineState_(engineState) {}

    JoltPhysicsSystem* PhysicsRuntimeService::joltPhysics() const {
        return engineState_.joltPhysicsSystem.get();
    }

    PhysicsSystem* PhysicsRuntimeService::physics() const {
        return engineState_.physicsSystem.get();
    }

}  // namespace engine