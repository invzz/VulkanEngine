#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Engine/Application/Ports/ISceneEntityPort.hpp"

#include "entt/entity/fwd.hpp"

namespace engine {

    // Use case for managing scene entities (add, delete, load models).
    // Aggregates AddEntityUseCase and DeleteEntityUseCase operations.
    class SceneEntityManagementUseCase {
       public:
        enum class EntityType {
            Camera,
            DirectionalLight,
            PointLight,
            SpotLight,
            Model,
        };

        explicit SceneEntityManagementUseCase(ISceneEntityPort& sceneEntity);

        // Add a camera entity with name.
        entt::entity addCamera(const std::string& name = "Camera");

        // Add a directional light entity with name.
        entt::entity addDirectionalLight(const std::string& name = "Directional Light");

        // Add a point light entity with name.
        entt::entity addPointLight(const std::string& name = "Point Light");

        // Add a spot light entity with name.
        entt::entity addSpotLight(const std::string& name = "Spot Light");

        // Add a model entity with name and model path.
        entt::entity addModel(const std::string& name, const std::string& modelPath);

        // Delete an entity. Returns true if the entity was valid and deleted.
        bool deleteEntity(entt::entity entity);

        // Check if an entity is valid.
        [[nodiscard]] bool isValid(entt::entity entity) const;

        // Get the scene this port operates on.
        [[nodiscard]] Scene* scene() const;

       private:
        ISceneEntityPort& sceneEntity_;
    };

}  // namespace engine
