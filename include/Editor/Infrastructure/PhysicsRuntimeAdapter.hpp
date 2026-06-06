#pragma once

#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"

namespace engine {

    class JoltPhysicsSystem;
    class EngineState;

    class PhysicsRuntimeAdapter final : public IPhysicsRuntimePort {
       public:
        explicit PhysicsRuntimeAdapter(EngineState& engineState);

        [[nodiscard]] bool&              physicsSimulationRunningRef() override;
        void                             clearSceneBodies() override;
        void                             setGroundEnabled(bool enabled) override;
        [[nodiscard]] JoltPhysicsSystem* joltPhysicsSystem() const override;

       private:
        EngineState& engineState_;
    };

}  // namespace engine
