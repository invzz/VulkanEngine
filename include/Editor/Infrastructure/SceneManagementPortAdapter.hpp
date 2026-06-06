#pragma once

#include "Engine/Application/Ports/ISceneManagementPort.hpp"

namespace engine {

    class EngineState;
    class Scene;
    class ResourceManager;

    // Adapter that bridges EngineState to the scene management port.
    class SceneManagementPortAdapter final : public ISceneManagementPort {
       public:
        explicit SceneManagementPortAdapter(EngineState& engineState);

        // Entity management
        entt::entity createEntity() override;
        void         destroyEntity(entt::entity entity) override;

        // Model management
        void addModel(entt::entity entity, const std::string& modelPath, const glm::mat4& transform) override;

        // Selection
        void                       setSelectedEntity(entt::entity entity) override;
        [[nodiscard]] entt::entity getSelectedEntity() const override;

        // Camera
        void                       setCameraEntity(entt::entity entity) override;
        [[nodiscard]] entt::entity getCameraEntity() const override;

        // Scene access
        [[nodiscard]] Scene*           scene() override;
        [[nodiscard]] entt::registry&  registry() override;
        [[nodiscard]] ResourceManager* resourceManager() override;

       private:
        EngineState& engineState_;
    };

}  // namespace engine
