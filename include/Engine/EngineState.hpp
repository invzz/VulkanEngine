#pragma once
#include <glm/glm.hpp>

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Engine/EditorState.hpp"
#include "Engine/Graphics/DescriptorManager.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/IRenderContextPort.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Systems/EnvironmentLightingService.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Scene/TransformService.hpp"
#include "Engine/Scene/SpatialSystem.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/ColliderDebugRenderSystem.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/SelectionCompositeSystem.hpp"
#include "Engine/Systems/SelectionMaskSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "ModelLib/Resources/MorphTargetManager.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
namespace engine {
    class Device;
    class Renderer;
    class Keyboard;
    class Mouse;
    class Window;
    /**
     * EngineState — single source of truth: scene, settings, skybox/IBL,
     * and type-safe DI system registry. Replaces the old port/adapter/use-case
     * architecture with direct method calls.
     */
    class EngineState {
       public:
        ~EngineState();
        void initialize(Device& device, Renderer& renderer, ResourceManager& rm,
            IRenderContextPort* renderContext, Window* window,
            bool mtRecording, uint32_t mtThreads);
        template <typename T>
        void registerSystem(std::unique_ptr<T>& sys) {
            systems_[typeid(T)] = sys.get();
        }
        template <typename T>
        T& system() {
            return *static_cast<T*>(systems_.at(typeid(T)));
        }
        template <typename T>
        T* systemPtr() {
            auto it = systems_.find(typeid(T));
            return it != systems_.end() ? static_cast<T*>(it->second) : nullptr;
        }
        Scene& scene() {
            return scene_;
        }
        entt::entity createEntity();
        void         destroyEntity(entt::entity e);
        bool         isValidEntity(entt::entity e) const;
        entt::entity addCamera(const std::string& name = "");
        entt::entity addDirectionalLight(const std::string& name = "");
        entt::entity addPointLight(const std::string& name = "");
        entt::entity addSpotLight(const std::string& name = "");
        entt::entity addModel(const std::string& name = "", const std::string& path = "");
        entt::entity selectedEntity() const {
            return editor_.selectedEntity;
        }
        void setSelectedEntity(entt::entity e) {
            editor_.selectedEntity = e;
        }
        entt::entity cameraEntity() const {
            return cameraEntity_;
        }
        void setCameraEntity(entt::entity e) {
            cameraEntity_ = e;
        }
        void setSerializer(class SceneSerializer* s) {
            serializer_ = s;
        }
        void saveScene(const std::string& path);
        bool loadScene(const std::string& path);
        void reconcileSceneLoad();
        void syncEnvironmentLighting(bool show) { envLighting_->syncEnvironmentLighting(show); }
        // Drive a directional light flagged as the sun (isSun) so its
        // direction, colour and intensity track SkyboxSettings::timeOfDay.
        // Creates a sun light on first use if none is flagged.
        void      updateSunLight() { envLighting_->updateSunLight(); }
        bool      loadIBL(const char* path) { return envLighting_->loadIBL(path); }
        void      resetIBLToFallback() { envLighting_->resetIBLToFallback(); }
        glm::vec3 getTranslation(entt::entity e) const { return transformSvc_->getTranslation(e); }
        void      setTranslation(entt::entity e, const glm::vec3& v) { transformSvc_->setTranslation(e, v); }
        glm::vec3 getRotation(entt::entity e) const { return transformSvc_->getRotation(e); }
        void      setRotation(entt::entity e, const glm::vec3& v) { transformSvc_->setRotation(e, v); }
        glm::vec3 getScale(entt::entity e) const { return transformSvc_->getScale(e); }
        void      setScale(entt::entity e, const glm::vec3& v) { transformSvc_->setScale(e, v); }
        bool&     showSkybox() {
            return editor_.showSkybox;
        }
        bool& showGrid() {
            return editor_.showGrid;
        }
        bool& showDebugObjects() {
            return editor_.showDebugObjects;
        }
        bool& showColliderWireframes() {
            return editor_.showColliderWireframes;
        }
        bool& debugMode() {
            return editor_.debugMode;
        }
        bool& physicsRunning() {
            return editor_.physicsRunning;
        }
        bool& solidGround() {
            return editor_.solidGround;
        }
        SkyboxSettings& skySettings() {
            return envLighting_->skySettings();
        }
        std::unique_ptr<Skybox>& skybox() {
            return envLighting_->skybox();
        }
        ShadowSettings& shadowSettings() {
            return shadowSettings_;
        }
        PostProcessPushConstants& postProcess() {
            return postProcess_;
        }
        void                          resetShadowSettings();
        void                          changeShadowSettings(bool cull, float plr, float slr);
        void                          clearSceneBodies();
        void                          setGroundEnabled(bool enabled);
        void                          recreatePostProcessingSystem(Device& device, VkRenderPass rp);
        /**
         * @brief Update post-processing descriptors to point to the current
         * offscreen color/depth images. Must be called after offscreen framebuffer
         * resize so descriptors don't reference destroyed image views.
         */
        void           updatePostProcessDescriptors(int frameIndex, Renderer& renderer);
        /**
         * @brief Direct access to the descriptor manager (pools, layouts, and
         * per-frame descriptor sets for G-buffer / deferred IBL / deferred
         * shadow / post-processing). Exposed so render passes reach descriptor
         * state without EngineState forwarding every per-pass accessor.
         */
        DescriptorManager& descriptors() {
            return *descriptors_;
        }
        SpatialSystem& spatialSystem() {
            return *spatial_;
        }
        EditorState& editor() {
            return editor_;
        }
        const EditorState& editor() const {
            return editor_;
        }
        IRenderContextPort& renderContext() {
            return *renderContextPort_;
        }
        ResourceManager& resourceManager() {
            return *resourceManager_;
        }
        Mouse* getMouse() const {
            return mouse_.get();
        }

       private:
        void                                        createInputDevices(Window* w);
        void                                        initCoreSystems(Device& d, Renderer& r, bool mt, uint32_t th);
        void                                        initDescriptorResources(Device& d, Renderer& r);
        void                                        allocatePerFrameDescriptorSets(Renderer& r);
        void                                        initPostProcessing(Device& d, Renderer& r);
        void                                        initInputRelatedSystems(Window* w);
        void                                        ensureCameraExists();
        std::unordered_map<std::type_index, void*>  systems_;
        std::unique_ptr<DescriptorManager>          descriptors_;
        std::unique_ptr<ObjectSelectionSystem>      objSel_;
        std::unique_ptr<InputSystem>                input_;
        std::unique_ptr<CameraSystem>               cameraSys_;
        std::unique_ptr<ColliderDebugRenderSystem>  colliderDbg_;
        std::unique_ptr<SelectionMaskSystem>        selMask_;
        std::unique_ptr<SelectionCompositeSystem>   selComposite_;
        std::unique_ptr<AnimationSystem>            anim_;
        std::unique_ptr<LODSystem>                  lod_;
        std::unique_ptr<ModelRenderSystem>          models_;
        std::unique_ptr<ShadowSystem>               shadow_;
        std::unique_ptr<LightSystem>                light_;
        std::unique_ptr<SkyboxRenderSystem>         skyboxR_;
        std::unique_ptr<GridRenderSystem>           grid_;
        std::unique_ptr<DeferredLightingSystem>     deferred_;
        std::unique_ptr<PostProcessingSystem>       postProc_;
        std::unique_ptr<IBLSystem>                  ibl_;
        std::unique_ptr<PhysicsSystem>              phys_;
        std::unique_ptr<JoltPhysicsSystem>          jolt_;
        std::unique_ptr<MorphTargetManager>         morph_;
        std::unique_ptr<SpatialSystem>              spatial_;
        std::unique_ptr<Keyboard>                   kbd_;
        std::unique_ptr<Mouse>                      mouse_;
        IRenderContextPort*                         renderContextPort_ = nullptr;
        ResourceManager*                            resourceManager_   = nullptr;
        Device*                                     device_            = nullptr;
        Scene                                       scene_;
        entt::entity                                cameraEntity_        = entt::null;
        bool                                        pendingCamAfterLoad_ = false;
        ShadowSettings                              shadowSettings_{};
        PostProcessPushConstants                    postProcess_{};
        EditorState                                 editor_{};
        SceneSerializer*                            serializer_    = nullptr;
        std::unique_ptr<TransformService>           transformSvc_;
        std::unique_ptr<EnvironmentLightingService> envLighting_;
    };
}  // namespace engine
