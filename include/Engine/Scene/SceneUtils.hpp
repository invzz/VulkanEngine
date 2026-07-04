#pragma once
#include <glm/glm.hpp>

#include <entt/entt.hpp>
namespace engine {
    class Scene;
    glm::vec4 getCameraPosition(const Scene& scene, entt::entity cameraEntity);
}  // namespace engine
