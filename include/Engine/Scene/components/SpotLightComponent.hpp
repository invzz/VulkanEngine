#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_SPOTLIGHTCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_SPOTLIGHTCOMPONENT_HPP
#include <glm/glm.hpp>

#include "../Component.hpp"
#include "LightCommon.hpp"
namespace engine {
    struct SpotLightComponent {
        float         intensity{1.0f};
        glm::vec3     color{1.0f, 1.0f, 1.0f};
        float         innerCutoffAngle{12.5f};
        float         outerCutoffAngle{17.5f};
        float         constantAttenuation{1.0f};
        float         linearAttenuation{0.09f};
        float         quadraticAttenuation{0.032f};
        bool          useTargetPoint{false};
        glm::vec3     targetPoint{0.0f, 0.0f, 0.0f};
        bool          bake{false};
        LightMobility lightType{LightMobility::Static};
    };
}  // namespace engine
#endif
