#ifndef EDITOR_UI_SCENE_HPP
#define EDITOR_UI_SCENE_HPP

#include <imgui.h>

#include <entt/entity/fwd.hpp>
#include <functional>
#include <string>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/SceneUtils.hpp"

#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine {

    class Device;
    class EngineState;

    namespace ui::SceneComponents {

        /**
 * @brief Tracks a pending model load operation.
 */
        struct PendingModelLoad {
            AsyncLoadId                                             id{0};
            std::string                                             path;
            std::string                                             name;
            engine::ModelInsertionOptions                           options;
            engine::ModelInsertionOptions::StaticColliderImportMode colliderMode{engine::ModelInsertionOptions::StaticColliderImportMode::AutoDetect};
            bool                                                    cancelled = false;
        };

        /**
 * @brief Collected entities by component type.
 */
        struct EntityCollection {
            std::vector<entt::entity> cameras;
            std::vector<entt::entity> dirLights;
            std::vector<entt::entity> pointLights;
            std::vector<entt::entity> spotLights;
            std::vector<entt::entity> models;
        };

        /**
 * @brief Gather entities from the scene registry by component type.
 */
        EntityCollection collectEntities(const engine::Scene& scene);

        /**
 * @brief Enforce policy: keep at most one directional light entity.
 * @param dirLights Input/output - modified in place.
 * @param toDelete Output - excess directional light entities pushed here.
 */
        void enforceSingleDirectionalLight(std::vector<entt::entity>& dirLights,
            std::vector<entt::entity>&                                toDelete);

        /**
 * @brief Render a selectable entity row with icon, label, and actions.
 * @param entity Entity to render.
 * @param icon Icon string (e.g., "[CAM]").
 * @param color Icon color.
 * @param frameInfo Current frame info (for selection state).
 * @param registry Entity registry.
 * @param toDelete Vector to push delete targets to.
 */
        void drawEntityRow(entt::entity entity,
            const char*                 icon,
            ImVec4                      color,
            FrameInfo&                  frameInfo,
            const entt::registry&       registry,
            std::vector<entt::entity>&  toDelete);

        /**
 * @brief Render the Cameras section with + button and entity list.
 * @param cameras Collected camera entities.
 * @param filter Current search filter string.
 * @param frameInfo Current frame info.
 * @param scene Current scene.
 * @param registry Entity registry.
 * @param toDelete Vector to push delete targets to.
 */
        void drawCameraSection(const std::vector<entt::entity>& cameras,
            const char*                                         filter,
            FrameInfo&                                          frameInfo,
            engine::Scene&                                      scene,
            entt::registry&                                     registry,
            std::vector<entt::entity>&                          toDelete);

        /**
 * @brief Render the Lights section with + button and sub-sections.
 * @param dirLights Collected directional light entities.
 * @param pointLights Collected point light entities.
 * @param spotLights Collected spot light entities.
 * @param filter Current search filter string.
 * @param frameInfo Current frame info.
 * @param scene Current scene.
 * @param registry Entity registry.
 * @param toDelete Vector to push delete targets to.
 */
        void drawLightSection(const std::vector<entt::entity>& dirLights,
            const std::vector<entt::entity>&                   pointLights,
            const std::vector<entt::entity>&                   spotLights,
            const char*                                        filter,
            FrameInfo&                                         frameInfo,
            engine::Scene&                                     scene,
            entt::registry&                                    registry,
            std::vector<entt::entity>&                         toDelete);

        /**
 * @brief Render the Models section with + button and model index popup.
 * @param models Collected model entities.
 * @param filter Current search filter string.
 * @param frameInfo Current frame info.
 * @param scene Current scene.
 * @param registry Entity registry.
 * @param toDelete Vector to push delete targets to.
 * @param colliderMode Input/output - current collider import mode.
 * @param enqueueModelLoad Callback to enqueue a model load operation.
 */
        void drawModelSection(const std::vector<entt::entity>& models,
            const char*                                        filter,
            FrameInfo&                                         frameInfo,
            engine::Scene&                                     scene,
            entt::registry&                                    registry,
            std::vector<entt::entity>&                         toDelete,
            ModelInsertionOptions::StaticColliderImportMode&   colliderMode,
            std::function<void(const std::string&,
                const std::string&,
                const ModelInsertionOptions&,
                ModelInsertionOptions::StaticColliderImportMode)>
                enqueueModelLoad);

        /**
 * @brief Render pending model load progress display.
 * @param pendingLoads Vector of pending model loads.
 * @param resourceManager Resource service for snapshot queries.
 */
        void drawPendingLoadsSection(std::vector<PendingModelLoad>& pendingLoads,
            ResourceManager*                                        resourceManager);

        /**
 * @brief Determine if a model should have a static collider.
 * @param path Model file path.
 * @param name Model name.
 * @param mode Collider import mode.
 * @return true if static collider should be created.
 */
        bool shouldCreateStaticCollider(const std::string&  path,
            const std::string&                              name,
            ModelInsertionOptions::StaticColliderImportMode mode);

    }  // namespace ui::SceneComponents

}  // namespace engine

#endif  // EDITOR_UI_SCENE_HPP
