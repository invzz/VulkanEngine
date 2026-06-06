#pragma once

#include "Engine/Application/Ports/ISceneEntityPort.hpp"

namespace engine {

    class EngineState;

    // Adapter that bridges EngineState to the scene entity port.
    class SceneEntityAdapter final : public ISceneEntityPort {
       public:
        explicit SceneEntityAdapter(EngineState& engineState);

        [[nodiscard]] entt::entity createEntity() override;
        void                       deleteEntity(entt::entity entity) override;
        [[nodiscard]] bool         isValid(entt::entity entity) const override;
        [[nodiscard]] Scene*       scene() override;

       private:
        EngineState& engineState_;
    };

}  // namespace engine
