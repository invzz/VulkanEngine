#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_MODELCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_MODELCOMPONENT_HPP

#include <memory>

#include "ModelLib/Resources/Model.hpp"

namespace engine {

    struct ModelComponent {
        std::shared_ptr<Model> model;
    };

}  // namespace engine

#endif
