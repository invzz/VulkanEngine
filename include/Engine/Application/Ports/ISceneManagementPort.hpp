#pragma once

#include <glm/glm.hpp>

#include <entt/entt.hpp>
#include <memory>
#include <string>

namespace engine {

    class Scene;
    class ResourceManager;

    class ISceneManagementPort {
       public:
        virtual ~ISceneManagementPort() = default;

        // Entity management
        virtual entt::entity createEntity()                     = 0;
        virtual void         destroyEntity(entt::entity entity) = 0;

        // Model management
        virtual void addModel(entt::entity entity, const std::string& modelPath, const glm::mat4& transform) = 0;

        // Selection
        virtual void         setSelectedEntity(entt::entity entity) = 0;
        virtual entt::entity getSelectedEntity() const              = 0;

        // Camera
        virtual void         setCameraEntity(entt::entity entity) = 0;
        virtual entt::entity getCameraEntity() const              = 0;

        // Scene access (for UI panels that need direct scene/registry access)
        [[nodiscard]] virtual Scene*           scene()           = 0;
        [[nodiscard]] virtual entt::registry&  registry()        = 0;
        [[nodiscard]] virtual ResourceManager* resourceManager() = 0;
    };

}  // namespace engine
