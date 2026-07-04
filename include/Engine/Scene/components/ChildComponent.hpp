#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_CHILDCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_CHILDCOMPONENT_HPP
#include <entt/entt.hpp>
namespace engine {
    struct ChildComponent {
        entt::entity parent{entt::null};
    };
}  // namespace engine
#endif