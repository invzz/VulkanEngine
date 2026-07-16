#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_DIRECTIONALLIGHTCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_DIRECTIONALLIGHTCOMPONENT_HPP
#include <glm/glm.hpp>

#include "../Component.hpp"
#include "LightCommon.hpp"
namespace engine {
    struct DirectionalLightComponent {
        float         intensity{1.0f};
        glm::vec3     color{1.0f, 1.0f, 1.0f};
        bool          useTargetPoint{false};
        glm::vec3     targetPoint{0.0f, 0.0f, 0.0f};
        bool          bake{false};
        LightMobility lightType{LightMobility::Static};
        // When true, the light's direction/colour/intensity are driven by the
        // SkyboxSettings sun model (timeOfDay). Toggled in the Lights panel.
        bool isSun{false};
    };
}  // namespace engine
#endif
