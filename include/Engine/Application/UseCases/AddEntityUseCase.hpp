#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Engine/Application/Ports/ISceneEntityPort.hpp"

#include "entt/entity/fwd.hpp"

namespace engine {

    class Scene;

    // Use case for adding entities to a scene with specific components.
    class AddEntityUseCase {
       public:
        enum class EntityType {
            Camera,
            DirectionalLight,
            PointLight,
            SpotLight,
            Model,
        };

        explicit AddEntityUseCase(ISceneEntityPort& sceneEntity);

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

       private:
        ISceneEntityPort& sceneEntity_;
    };

}  // namespace engine
