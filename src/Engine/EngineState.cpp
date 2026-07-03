#include "Engine/EngineState.hpp"

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"

#include "EngineSceneIO/Scene/SceneSerializer.hpp"
#include "ModelLib/Resources/TextureManager.hpp"

namespace {
    auto const _vtex = &engine::ibl_detail::vtex::loadImage;
}

namespace engine {

    EngineState::~EngineState() = default;

    void EngineState::initialize(Device& device, Renderer& renderer, ResourceManager& rm,
        IRenderContextPort* renderContext, Window* window, bool mtRecording, uint32_t mtThreads) {
        if (renderContext == nullptr) {
            throw RuntimeException("EngineState: null IRenderContextPort");
}
        resourceManager_   = &rm;
        renderContextPort_ = renderContext;
        device_            = &device;
        createInputDevices(window);

        initRegistry_.clear();
        std::string e;
        initRegistry_.registerSystem("core", {}, [&](auto&) { initCoreSystems(device, renderer, mtRecording, mtThreads); return true; }, &e);
        initRegistry_.registerSystem("desc", {"core"}, [&](auto&) { initDescriptorResources(device, renderer); return true; }, &e);
        initRegistry_.registerSystem("pf", {"desc"}, [&](auto&) { allocatePerFrameDescriptorSets(renderer); return true; }, &e);
        initRegistry_.registerSystem("pipe", {"core"}, [&](auto&) {
        models_->createDepthPrepassPipeline(renderer.getOffscreenDepthPrepassRenderPass());
        models_->setShadowSystem(shadow_.get()); models_->setIBLSystem(ibl_.get()); return true; }, &e);
        initRegistry_.registerSystem("pp", {"pf", "pipe"}, [&](auto&) { initPostProcessing(device, renderer); return true; }, &e);
        initRegistry_.registerSystem("inp", {"core"}, [&](auto&) { initInputRelatedSystems(window); return true; }, &e);
        initRegistry_.registerSystem("phys", {"core"}, [&](auto&) {
        phys_ = std::make_unique<PhysicsSystem>(device); jolt_ = std::make_unique<JoltPhysicsSystem>();
        registerSystem(phys_); registerSystem(jolt_); return true; }, &e);
        std::string ie;
        if (!initRegistry_.initializeAll(&ie)) {
            throw RuntimeException(ie);
}
    }

    void EngineState::createInputDevices(Window* w) {
        if (w != nullptr) {
            kbd_   = std::make_unique<Keyboard>(*w);
            mouse_ = std::make_unique<Mouse>(*w);
        }
    }

    void EngineState::initCoreSystems(Device& d, Renderer& r, bool mt, uint32_t th) {
        if (renderContextPort_ == nullptr) {
            throw RuntimeException("initCore: null");
}
        auto rp      = r.getOffscreenRenderPassLoadColorDepth();
        auto sl      = renderContextPort_->getGlobalSetLayout();
        cameraSys_   = std::make_unique<CameraSystem>(d, rp, sl);
        colliderDbg_ = std::make_unique<ColliderDebugRenderSystem>(d, rp, sl);
        selOutline_  = std::make_unique<SelectionOutlineSystem>(d, rp, sl);
        registerSystem(cameraSys_);
        registerSystem(colliderDbg_);
        registerSystem(selOutline_);
        anim_ = std::make_unique<AnimationSystem>(d);
        lod_  = std::make_unique<LODSystem>();
        registerSystem(anim_);
        registerSystem(lod_);
        shadow_ = std::make_unique<ShadowSystem>(d, 4096);
        ibl_    = std::make_unique<IBLSystem>(d);
        morph_  = std::make_unique<MorphTargetManager>(d);
        registerSystem(shadow_);
        registerSystem(ibl_);
        registerSystem(morph_);
        skyboxR_ = std::make_unique<SkyboxRenderSystem>(d, rp);
        grid_    = std::make_unique<GridRenderSystem>(d, rp, sl);
        models_  = std::make_unique<ModelRenderSystem>(d, rp, sl, resourceManager_->getTextureManager().getDescriptorSetLayout());
        registerSystem(skyboxR_);
        registerSystem(grid_);
        registerSystem(models_);
        models_->enableMultiThreadedRecording(mt, th);
        light_ = std::make_unique<LightSystem>(d, rp, sl);
        registerSystem(light_);
        spatial_ = std::make_unique<SpatialSystem>();
    }

    void EngineState::initDescriptorResources(Device& d, Renderer& r) {
        descriptors_ = std::make_unique<DescriptorManager>();
        descriptors_->createDescriptorResources(d, r);
        models_->createGbufferPipeline(r.getGbufferRenderPass());
        deferred_ = std::make_unique<DeferredLightingSystem>(d, r.getDeferredLightingRenderPass(),
            std::vector<VkDescriptorSetLayout>{renderContextPort_->getGlobalSetLayout(),
                descriptors_->gbufferSetLayout().getDescriptorSetLayout(),
                descriptors_->deferredShadowSetLayout().getDescriptorSetLayout(),
                descriptors_->deferredIblSetLayout().getDescriptorSetLayout()});
        registerSystem(deferred_);
    }

    void EngineState::allocatePerFrameDescriptorSets(Renderer& r) {
        descriptors_->allocatePerFrameDescriptors(r);
        deferredIblDescriptorSetsRef().resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < (int) deferredIblDescriptorSetsRef().size(); ++i) {
            auto irr  = ibl_->getIrradianceDescriptorInfo();
            auto pre  = ibl_->getPrefilteredDescriptorInfo();
            auto brdf = ibl_->getBRDFLUTDescriptorInfo();
            DescriptorWriter(descriptors_->deferredIblSetLayout(), descriptors_->deferredIblPool())
                .writeImage(0, &irr)
                .writeImage(1, &pre)
                .writeImage(2, &brdf)
                .buildOrThrow(deferredIblDescriptorSetsRef()[i]);
        }
    }

    void EngineState::initPostProcessing(Device& d, Renderer& r) {
        descriptors_->recreatePostProcessDescriptorSets(d, r, VK_NULL_HANDLE);
        postProc_ = std::make_unique<PostProcessingSystem>(d, r.getPostFxRenderPass(),
            std::vector<VkDescriptorSetLayout>{descriptors_->postProcessSetLayout().getDescriptorSetLayout()});
        registerSystem(postProc_);
    }

    void EngineState::initInputRelatedSystems(Window* w) {
        if (kbd_ && mouse_ && (w != nullptr)) {
            objSel_ = std::make_unique<ObjectSelectionSystem>(*kbd_);
            input_  = std::make_unique<InputSystem>(*kbd_, *mouse_, *w);
            registerSystem(objSel_);
            registerSystem(input_);
        }
    }

    entt::entity EngineState::createEntity() {
        return scene_.createEntity();
    }
    void EngineState::destroyEntity(entt::entity e) {
        scene_.destroyEntity(e);
    }
    bool EngineState::isValidEntity(entt::entity e) const {
        return scene_.getRegistry().valid(e);
    }

    entt::entity EngineState::addCamera(const std::string& n) {
        auto  e = createEntity();
        auto& r = scene_.getRegistry();
        r.emplace<TransformComponent>(e);
        r.emplace<CameraComponent>(e);
        r.emplace<NameComponent>(e, n.empty() ? "Camera" : n);
        return e;
    }
    entt::entity EngineState::addDirectionalLight(const std::string& n) {
        auto  e = createEntity();
        auto& r = scene_.getRegistry();
        r.emplace<TransformComponent>(e);
        r.emplace<DirectionalLightComponent>(e);
        r.emplace<NameComponent>(e, n.empty() ? "DirLight" : n);
        return e;
    }
    entt::entity EngineState::addPointLight(const std::string& n) {
        auto  e = createEntity();
        auto& r = scene_.getRegistry();
        r.emplace<TransformComponent>(e);
        r.emplace<PointLightComponent>(e);
        r.emplace<NameComponent>(e, n.empty() ? "PtLight" : n);
        return e;
    }
    entt::entity EngineState::addSpotLight(const std::string& n) {
        auto  e = createEntity();
        auto& r = scene_.getRegistry();
        r.emplace<TransformComponent>(e);
        r.emplace<SpotLightComponent>(e);
        r.emplace<NameComponent>(e, n.empty() ? "SpotLight" : n);
        return e;
    }
    entt::entity EngineState::addModel(const std::string& n, const std::string& /*unused*/) {
        auto  e = createEntity();
        auto& r = scene_.getRegistry();
        r.emplace<TransformComponent>(e);
        r.emplace<NameComponent>(e, n.empty() ? "Model" : n);
        return e;
    }

    void EngineState::saveScene(const std::string& p) {
        if (serializer_ != nullptr) {
            serializer_->serialize(p);
        }
    }
    bool EngineState::loadScene(const std::string& p) {
        if (serializer_ == nullptr) {
            return false;
        }
        clearSceneBodies();
        setGroundEnabled(editor_.solidGround);
        if (!serializer_->deserialize(p)) {
            return false;
        }
        editor_.physicsRunning = false;
        editor_.selectedEntity = cameraEntity_ = entt::null;
        pendingCamAfterLoad_                   = true;
        ensureCameraExists();
        return true;
    }
    void EngineState::reconcileSceneLoad() {
        if (!pendingCamAfterLoad_) {
            return;
        }
        pendingCamAfterLoad_ = false;
        cameraEntity_        = entt::null;
        for (auto e : scene_.getRegistry().view<CameraComponent>()) {
            cameraEntity_ = e;
            break;
        }
    }
    void EngineState::ensureCameraExists() {
        for (auto e : scene_.getRegistry().view<CameraComponent>()) {
            cameraEntity_ = e;
            return;
        }
        addCamera("Camera");
    }

    void EngineState::syncEnvironmentLighting(bool show) {
        if (show && !skybox_) {
            skybox_      = Skybox::loadFromFolder(*device_, std::string(TEXTURE_PATH) + "/skybox/Yokohama", "jpg");
            auto discard = ibl_->loadFromDisk(std::string(TEXTURE_PATH) + "/ibl/Yokohama");
        }
        if (!show && skybox_) {
            skybox_.reset();
            ibl_->resetToFallback();
        }
        ibl_->update();
        if (ibl_->getGenerationCounter() != iblGeneration_) {
            writeIBLDescriptorsToSets();
            iblGeneration_ = ibl_->getGenerationCounter();
        }
    }
    bool EngineState::loadIBL(const char* p) {
        return ibl_->loadFromDisk(std::string(p));
    }
    void EngineState::resetIBLToFallback() {
        ibl_->resetToFallback();
    }
    void EngineState::writeIBLDescriptorsToSets() {
        auto ir = ibl_->getIrradianceDescriptorInfo();
        auto pr = ibl_->getPrefilteredDescriptorInfo();
        auto br = ibl_->getBRDFLUTDescriptorInfo();
        for (auto& s : deferredIblDescriptorSetsRef()) {
            DescriptorWriter(deferredIblSetLayout(), deferredIblPool()).writeImage(0, &ir).writeImage(1, &pr).writeImage(2, &br).overwrite(s);
        }
    }

    glm::vec3 EngineState::getTranslation(entt::entity e) const {
        auto& r = scene_.getRegistry();
        return r.all_of<TransformComponent>(e) ? r.get<TransformComponent>(e).translation : glm::vec3{};
    }
    void EngineState::setTranslation(entt::entity e, const glm::vec3& v) {
        scene_.getRegistry().get<TransformComponent>(e).translation = v;
    }
    glm::vec3 EngineState::getRotation(entt::entity e) const {
        auto& r = scene_.getRegistry();
        return r.all_of<TransformComponent>(e) ? r.get<TransformComponent>(e).rotation : glm::vec3{};
    }
    void EngineState::setRotation(entt::entity e, const glm::vec3& v) {
        scene_.getRegistry().get<TransformComponent>(e).rotation = v;
    }
    glm::vec3 EngineState::getScale(entt::entity e) const {
        auto& r = scene_.getRegistry();
        return r.all_of<TransformComponent>(e) ? r.get<TransformComponent>(e).scale : glm::vec3{1};
    }
    void EngineState::setScale(entt::entity e, const glm::vec3& v) {
        scene_.getRegistry().get<TransformComponent>(e).scale = v;
    }

    void EngineState::resetShadowSettings() {
        shadowSettings_ = ShadowSettings{};
    }
    void EngineState::changeShadowSettings(bool c, float pl, float sl) {
        shadowSettings_.enableShadowCulling    = c;
        shadowSettings_.pointLightDefaultRange = pl;
        shadowSettings_.spotLightDefaultRange  = sl;
    }

    void EngineState::clearSceneBodies() {
        if (jolt_) {
            jolt_->clear();
        }
    }
    void EngineState::setGroundEnabled(bool e) {
        if (jolt_) {
            jolt_->setGroundEnabled(e);
        }
    }

    VkDescriptorSet EngineState::gbufferDescriptorSet(int frameIndex) const {
        return descriptors_->gbufferDescriptorSet(frameIndex);
    }
    VkDescriptorSet& EngineState::gbufferDescriptorSetRef(int frameIndex) {
        return descriptors_->gbufferDescriptorSetRef(frameIndex);
    }
    VkDescriptorSet EngineState::deferredShadowDescriptorSet(int frameIndex) const {
        return descriptors_->deferredShadowDescriptorSet(frameIndex);
    }
    VkDescriptorSet& EngineState::deferredShadowDescriptorSetRef(int frameIndex) {
        return descriptors_->deferredShadowDescriptorSetRef(frameIndex);
    }
    VkDescriptorSet EngineState::deferredIblDescriptorSet(int frameIndex) const {
        return descriptors_->deferredIblDescriptorSet(frameIndex);
    }
    VkDescriptorSet EngineState::postProcessDescriptorSet(int frameIndex) const {
        return descriptors_->postProcessDescriptorSet(frameIndex);
    }
    VkDescriptorSet& EngineState::postProcessDescriptorSetRef(int frameIndex) {
        return descriptors_->postProcessDescriptorSetRef(frameIndex);
    }
    std::vector<VkDescriptorSet>& EngineState::deferredIblDescriptorSetsRef() {
        return descriptors_->deferredIblDescriptorSets();
    }
    DescriptorSetLayout& EngineState::gbufferSetLayout() {
        return descriptors_->gbufferSetLayout();
    }
    DescriptorPool& EngineState::gbufferPool() {
        return descriptors_->gbufferPool();
    }
    DescriptorSetLayout& EngineState::postProcessSetLayout() {
        return descriptors_->postProcessSetLayout();
    }
    DescriptorPool& EngineState::postProcessPool() {
        return descriptors_->postProcessPool();
    }
    DescriptorSetLayout& EngineState::deferredIblSetLayout() {
        return descriptors_->deferredIblSetLayout();
    }
    DescriptorPool& EngineState::deferredIblPool() {
        return descriptors_->deferredIblPool();
    }
    DescriptorSetLayout& EngineState::deferredShadowSetLayout() {
        return descriptors_->deferredShadowSetLayout();
    }
    DescriptorPool& EngineState::deferredShadowPool() {
        return descriptors_->deferredShadowPool();
    }

    void EngineState::recreatePostProcessingSystem(Device& d, VkRenderPass rp) {
        postProc_ = std::make_unique<PostProcessingSystem>(d, rp, std::vector<VkDescriptorSetLayout>{postProcessSetLayout().getDescriptorSetLayout()});
        registerSystem(postProc_);
    }

    void EngineState::updatePostProcessDescriptors(int frameIndex, Renderer& renderer) {
        descriptors_->updatePostProcessDescriptors(frameIndex, renderer);
    }

}  // namespace engine
