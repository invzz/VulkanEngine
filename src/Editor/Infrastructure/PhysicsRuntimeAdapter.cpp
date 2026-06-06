#include "Editor/Infrastructure/PhysicsRuntimeAdapter.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"

namespace engine {

    PhysicsRuntimeAdapter::PhysicsRuntimeAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    bool& PhysicsRuntimeAdapter::physicsSimulationRunningRef() {
        return engineState_.physicsSimulationRunningRef();
    }

    void PhysicsRuntimeAdapter::clearSceneBodies() {
        auto* jolt = engineState_.getJoltPhysicsSystem();
        if (jolt != nullptr) {
            jolt->clear();
        }
    }

    void PhysicsRuntimeAdapter::setGroundEnabled(bool enabled) {
        auto* jolt = engineState_.getJoltPhysicsSystem();
        if (jolt != nullptr) {
            jolt->setGroundEnabled(enabled);
        }
    }

    JoltPhysicsSystem* PhysicsRuntimeAdapter::joltPhysicsSystem() const {
        return engineState_.getJoltPhysicsSystem();
    }

}  // namespace engine
