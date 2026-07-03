#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_LODCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_LODCOMPONENT_HPP

#include <memory>
#include <vector>

#include "../Component.hpp"
#include "ModelLib/Resources/Model.hpp"

namespace engine {

    struct LODLevel {
        std::shared_ptr<Model> model;
        float                  distance;
    };

    struct LODComponent {
        std::vector<LODLevel> levels;
    };

}  // namespace engine

#endif
