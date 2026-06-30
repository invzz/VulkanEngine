#pragma once

#include <glm/glm.hpp>

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
#include "Engine/Scene/Skybox.hpp"
#include "Engine/SystemRegistry.hpp"
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
#include "Engine/Systems/SelectionOutlineSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "ModelLib/Resources/MorphTargetManager.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"

#include <entt/entt.hpp>

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

        // ---- Lifecycle ----
        void initialize(Device& device, Renderer& renderer, ResourceManager& rm,
            IRenderContextPort* renderContext, Window* window,
            bool mtRecording, uint32_t mtThreads);

        // ---- DI container ----
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

        // ---- Scene ----
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

        // ---- Selection & Camera ----
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

        // ---- Persistence ----
        void setSerializer(class SceneSerializer* s) {
            serializer_ = s;
        }
        void saveScene(const std::string& path);
        bool loadScene(const std::string& path);
        void reconcileSceneLoad();

        // ---- Skybox / IBL ----
        void syncEnvironmentLighting(bool show);
        bool loadIBL(const char* path);
        void resetIBLToFallback();

        // ---- Transform ----
        glm::vec3 getTranslation(entt::entity e) const;
        void      setTranslation(entt::entity e, const glm::vec3& v);
        glm::vec3 getRotation(entt::entity e) const;
        void      setRotation(entt::entity e, const glm::vec3& v);
        glm::vec3 getScale(entt::entity e) const;
        void      setScale(entt::entity e, const glm::vec3& v);

        // ---- Settings ----
        bool& showSkybox() {
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
            return skySettings_;
        }
        ShadowSettings& shadowSettings() {
            return shadowSettings_;
        }
        PostProcessPushConstants& postProcess() {
            return postProcess_;
        }
        void resetShadowSettings();
        void changeShadowSettings(bool cull, float plr, float slr);

        // ---- Physics ----
        void clearSceneBodies();
        void setGroundEnabled(bool enabled);

        // ---- Descriptors ----
        VkDescriptorSet               gbufferDescriptorSet(int frameIndex) const;
        VkDescriptorSet&              gbufferDescriptorSetRef(int frameIndex);
        VkDescriptorSet               deferredShadowDescriptorSet(int frameIndex) const;
        VkDescriptorSet&              deferredShadowDescriptorSetRef(int frameIndex);
        VkDescriptorSet               deferredIblDescriptorSet(int frameIndex) const;
        VkDescriptorSet               postProcessDescriptorSet(int frameIndex) const;
        VkDescriptorSet&              postProcessDescriptorSetRef(int frameIndex);
        std::vector<VkDescriptorSet>& deferredIblDescriptorSetsRef();
        DescriptorSetLayout&          gbufferSetLayout();
        DescriptorPool&               gbufferPool();
        DescriptorSetLayout&          postProcessSetLayout();
        DescriptorPool&               postProcessPool();
        DescriptorSetLayout&          deferredIblSetLayout();
        DescriptorPool&               deferredIblPool();
        DescriptorSetLayout&          deferredShadowSetLayout();
        DescriptorPool&               deferredShadowPool();

        // ---- Post-processing ----
        void recreatePostProcessingSystem(Device& device, VkRenderPass rp);

        // ---- Non-owned deps ----
        const EditorState& editor() const {
            return editor_;
        }
        IRenderContextPort& renderContext() {
            return *renderContextPort_;
        }
        ResourceManager& resourceManager() {
            return *resourceManager_;
        }

       private:
        void createInputDevices(Window* w);
        void initCoreSystems(Device& d, Renderer& r, bool mt, uint32_t th);
        void initDescriptorResources(Device& d, Renderer& r);
        void allocatePerFrameDescriptorSets(Renderer& r);
        void initPostProcessing(Device& d, Renderer& r);
        void initInputRelatedSystems(Window* w);
        void ensureCameraExists();
        void writeIBLDescriptorsToSets();

        // -- Storage --
        std::unordered_map<std::type_index, void*> systems_;
        ::engine::SystemRegistry                   initRegistry_;
        std::unique_ptr<DescriptorManager>         descriptors_;

        std::unique_ptr<ObjectSelectionSystem>     objSel_;
        std::unique_ptr<InputSystem>               input_;
        std::unique_ptr<CameraSystem>              cameraSys_;
        std::unique_ptr<ColliderDebugRenderSystem> colliderDbg_;
        std::unique_ptr<SelectionOutlineSystem>    selOutline_;
        std::unique_ptr<AnimationSystem>           anim_;
        std::unique_ptr<LODSystem>                 lod_;
        std::unique_ptr<ModelRenderSystem>         models_;
        std::unique_ptr<ShadowSystem>              shadow_;
        std::unique_ptr<LightSystem>               light_;
        std::unique_ptr<SkyboxRenderSystem>        skyboxR_;
        std::unique_ptr<GridRenderSystem>          grid_;
        std::unique_ptr<DeferredLightingSystem>    deferred_;
        std::unique_ptr<PostProcessingSystem>      postProc_;
        std::unique_ptr<IBLSystem>                 ibl_;
        std::unique_ptr<PhysicsSystem>             phys_;
        std::unique_ptr<JoltPhysicsSystem>         jolt_;
        std::unique_ptr<MorphTargetManager>        morph_;
        std::unique_ptr<Keyboard>                  kbd_;
        std::unique_ptr<Mouse>                     mouse_;

        IRenderContextPort* renderContextPort_ = nullptr;
        ResourceManager*    resourceManager_   = nullptr;
        Device*             device_            = nullptr;

        Scene        scene_;
        entt::entity cameraEntity_        = entt::null;
        bool         pendingCamAfterLoad_ = false;

        std::unique_ptr<Skybox>  skybox_;
        SkyboxSettings           skySettings_{};
        ShadowSettings           shadowSettings_{};
        PostProcessPushConstants postProcess_{};

        EditorState editor_{};

        SceneSerializer* serializer_    = nullptr;
        uint64_t         iblGeneration_ = 0;
    };

}  // namespace engine
