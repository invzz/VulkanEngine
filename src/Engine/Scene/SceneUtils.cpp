#include "Engine/Scene/SceneUtils.hpp"

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

    glm::vec4 getCameraPosition(const Scene& scene, entt::entity cameraEntity) {
        if (scene.getRegistry().valid(cameraEntity) &&
            scene.getRegistry().all_of<TransformComponent>(cameraEntity)) {
            return glm::vec4(
                scene.getRegistry().get<TransformComponent>(cameraEntity).translation, 1.0f);
        }
        return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

}  // namespace engine
