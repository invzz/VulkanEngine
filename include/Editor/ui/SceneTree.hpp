#pragma once

#include <entt/entt.hpp>
#include <vector>

#include "Editor/ui/SceneTreeHelpers.hpp"

namespace engine {
    struct FrameInfo;
}

namespace engine::ui {

    /// Renders a single model entity as a tree node, expanding its
    /// light + sub-mesh children. Owns no editor state; all selection /
    /// deletion side effects are pushed into `frameInfo` / `deletionQueue`.
    class SceneTree {
       public:
        explicit SceneTree(const entt::registry& registry);

        void DrawEntity(entt::entity   entity,
            const char*                icon,
            ImVec4                     color,
            FrameInfo&                 frameInfo,
            std::vector<entt::entity>& deletionQueue);

       private:
        const entt::registry& registry_;
        ChildrenIndex         children_;
    };

}  // namespace engine::ui
