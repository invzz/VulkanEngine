#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "entt/entity/fwd.hpp"

namespace engine {

    class Scene;

    // Port for scene entity management without knowing EngineState internals.
    class ISceneEntityPort {
       public:
        virtual ~ISceneEntityPort() = default;

        // Create a new entity in the scene and return its handle.
        [[nodiscard]] virtual entt::entity createEntity() = 0;

        // Delete an entity from the scene.
        virtual void deleteEntity(entt::entity entity) = 0;

        // Check if an entity is valid in the scene.
        [[nodiscard]] virtual bool isValid(entt::entity entity) const = 0;

        // Get the scene this port operates on.
        [[nodiscard]] virtual Scene* scene() = 0;
    };

}  // namespace engine
