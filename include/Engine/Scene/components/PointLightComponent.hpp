#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_POINTLIGHTCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_POINTLIGHTCOMPONENT_HPP

#include <glm/glm.hpp>

#include "../Component.hpp"

namespace engine {

  struct PointLightComponent
  {
    float     intensity{1.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float     radius{15.0f};
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_POINTLIGHTCOMPONENT_HPP
