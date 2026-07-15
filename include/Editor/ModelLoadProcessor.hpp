#ifndef EDITOR_MODEL_LOAD_PROCESSOR_HPP
#define EDITOR_MODEL_LOAD_PROCESSOR_HPP
#include <functional>
#include <string>

#include "Engine/Scene/Scene.hpp"

#include "Editor/ui/ModelInsertionOptions.hpp"
#include "ModelLib/Resources/Model.hpp"
namespace engine {
    struct ModelInsertionOptions;
    /**
 * @brief Centralized processor for model loading operations.
 * 
 * Consolidates common logic for:
 * - Entity creation with ModelComponent
 * - Light entity creation from model lights
 * - Static collider attachment
 * - Animation/Morph target component setup
 * 
 * Eliminates duplication between SceneLoader and ScenePanel.
 */
    class ModelLoadProcessor {
       public:
        /**
     * @brief Callback type for model load completion
     * @param modelPtr Loaded model (nullptr on failure)
     * @param modelPath Original model path (for logging/debugging)
     * @param entity Created entity (only valid if modelPtr != nullptr)
     */
        using LoadCallback = std::function<void(
            const std::shared_ptr<Model>& modelPtr,
            const std::string&            modelPath,
            entt::entity                  entity)>;
        /**
     * @brief Process a loaded model into the scene
     * 
     * Handles all post-load processing:
     * - Creates entity with ModelComponent
     * - Adds NameComponent
     * - Creates light entities if model has lights
     * - Adds static collider if requested
     * - Adds AnimationComponent if model has animations/morph targets
     * 
     * @param scene Target scene
     * @param modelPtr Loaded model (must be valid)
     * @param modelPath Path to model (for NameComponent, logging, collider detection)
     * @param modelName Display name (for NameComponent)
     * @param colliderMode How to handle static mesh colliders
     */
        static void processLoadedModel(
            Scene&                                          scene,
            const std::shared_ptr<Model>&                   modelPtr,
            const std::string&                              modelPath,
            const std::string&                              modelName,
            ModelInsertionOptions::StaticColliderImportMode colliderMode);
        /**
     * @brief Create a lambda that processes loaded models (for async loading)
     * 
     * Returns a callback suitable for passing to async load completion handlers.
     * Captures scene reference by reference - ensure scene outlives the callback.
     * 
     * @param scene Scene to add entities to
     * @param modelPath Path to model (for logging/collider detection)
     * @param modelName Display name for entity
     * @param colliderMode How to handle static mesh colliders
     * @return Callback function for async load completion
     */
        static LoadCallback createAsyncCallback(
            Scene&                                          scene,
            const std::string&                              modelPath,
            const std::string&                              modelName,
            ModelInsertionOptions::StaticColliderImportMode colliderMode);

       private:
        /**
     * @brief Create light entities from model's embedded lights
     */
        static void createLightEntities(
            Scene&       scene,
            const Model& model,
            entt::entity parentEntity);
        /**
     * @brief Create entity hierarchy from model's glTF node tree
     */
        static void createNodeEntities(
            Scene&       scene,
            const Model& model,
            entt::entity modelEntity);
        /**
     * @brief Create one selection entity per sub-mesh (glTF primitive).
     * These entities carry SubMeshComponent and are regenerated on every load
     * (never serialized), enabling sub-mesh-level outline selection.
     */
        static void createSubMeshEntities(
            Scene&       scene,
            const Model& model,
            entt::entity modelEntity);
        /**
     * @brief Check if static collider should be auto-created
     */
        static bool shouldCreateStaticCollider(
            const std::string&                              modelPath,
            const std::string&                              name,
            ModelInsertionOptions::StaticColliderImportMode mode);
    };
}  // namespace engine
#endif  // EDITOR_MODEL_LOAD_PROCESSOR_HPP
