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
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Scene/SpatialSystem.hpp"
#include "Engine/SystemRegistry.hpp"
#include "Engine/Systems/ProceduralSkyCapture.hpp"
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
#include "Engine/Systems/SelectionMaskSystem.hpp"
#include "Engine/Systems/SelectionCompositeSystem.hpp"
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
        void      saveScene(const std::string& path);
        bool      loadScene(const std::string& path);
        void      reconcileSceneLoad();
        void      syncEnvironmentLighting(bool show);
        // Drive a directional light flagged as the sun (isSun) so its
        // direction, colour and intensity track SkyboxSettings::timeOfDay.
        // Creates a sun light on first use if none is flagged.
        void      updateSunLight();
        bool      loadIBL(const char* path);
        void      resetIBLToFallback();
        glm::vec3 getTranslation(entt::entity e) const;
        void      setTranslation(entt::entity e, const glm::vec3& v);
        glm::vec3 getRotation(entt::entity e) const;
        void      setRotation(entt::entity e, const glm::vec3& v);
        glm::vec3 getScale(entt::entity e) const;
        void      setScale(entt::entity e, const glm::vec3& v);
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
            return skySettings_;
        }
        std::unique_ptr<Skybox>& skybox() {
            return skybox_;
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
        void                          recreatePostProcessingSystem(Device& device, VkRenderPass rp);
        /**
         * @brief Update post-processing descriptors to point to the current
         * offscreen color/depth images. Must be called after offscreen framebuffer
         * resize so descriptors don't reference destroyed image views.
         */
        void           updatePostProcessDescriptors(int frameIndex, Renderer& renderer);
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
        void                                       createInputDevices(Window* w);
        void                                       initCoreSystems(Device& d, Renderer& r, bool mt, uint32_t th);
        void                                       initDescriptorResources(Device& d, Renderer& r);
        void                                       allocatePerFrameDescriptorSets(Renderer& r);
        void                                       initPostProcessing(Device& d, Renderer& r);
        void                                       initInputRelatedSystems(Window* w);
        void                                       ensureCameraExists();
        void                                       writeIBLDescriptorsToSets();
        void                                       captureProceduralSkyToIBL();
        std::unordered_map<std::type_index, void*> systems_;
        ::engine::SystemRegistry                   initRegistry_;
        std::unique_ptr<DescriptorManager>         descriptors_;
        std::unique_ptr<ObjectSelectionSystem>     objSel_;
        std::unique_ptr<InputSystem>               input_;
        std::unique_ptr<CameraSystem>              cameraSys_;
        std::unique_ptr<ColliderDebugRenderSystem> colliderDbg_;
        std::unique_ptr<SelectionMaskSystem>       selMask_;
        std::unique_ptr<SelectionCompositeSystem>  selComposite_;
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
        std::unique_ptr<SpatialSystem>             spatial_;
        std::unique_ptr<Keyboard>                  kbd_;
        std::unique_ptr<Mouse>                     mouse_;
        IRenderContextPort*                        renderContextPort_ = nullptr;
        ResourceManager*                           resourceManager_   = nullptr;
        Device*                                    device_            = nullptr;
        Scene                                      scene_;
        entt::entity                               cameraEntity_        = entt::null;
        bool                                       pendingCamAfterLoad_ = false;
        std::unique_ptr<Skybox>                    skybox_;
        std::unique_ptr<class ProceduralSkyCapture> procSkyCapture_;
        SkyboxSettings                             skySettings_{};
        ShadowSettings                             shadowSettings_{};
        PostProcessPushConstants                   postProcess_{};
        EditorState                                editor_{};
        SceneSerializer*                           serializer_    = nullptr;
        uint64_t                                   iblGeneration_ = 0;
        // Procedural-sky IBL bake gating: hysteresis for continuous drivers
        // (timeOfDay) plus explicit dead-bands / dirty flags for the rest.
        float                                      procIblLat_        = 1e9f;    // last baked latitude (deg)
        int                                        procIblDay_        = -1;        // last baked day-of-year
        float                                      procIblPendingTime_ = 0.0f;    // accumulated |dtimeOfDay| since last bake
        float                                      procIblSampledTime_ = -1e9f;    // anchor for per-frame time accumulation
        double                                     procIblAtmoR_      = 0.0;       // atmosphereRadius
        double                                     procIblRayH_       = 0.0;       // rayleighScaleHeight
        double                                     procIblMieH_       = 0.0;       // mieScaleHeight
        glm::dvec3                                 procIblBetaR_      = {};        // betaRayleigh
        glm::dvec3                                 procIblBetaM_      = {};        // betaMie
        float                                      procIblMieG_       = -1.0f;     // mieG
        float                                      procIblSkyInt_     = -1.0f;     // skyIntensity
        int                                        bakeDeferredFrames_ = 30;       // skip first N frames (loader burst)
    };
}  // namespace engine
