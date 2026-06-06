#pragma once

#include "Engine/Application/Ports/ISceneEntityPort.hpp"

namespace engine {

    // Use case for deleting entities from a scene.
    class DeleteEntityUseCase {
       public:
        explicit DeleteEntityUseCase(ISceneEntityPort& sceneEntity);

        // Delete an entity. Returns true if the entity was valid and deleted.
        bool execute(entt::entity entity);

       private:
        ISceneEntityPort& sceneEntity_;
    };

}  // namespace engine
