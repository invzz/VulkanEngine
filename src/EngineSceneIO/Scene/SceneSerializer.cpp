#include "EngineSceneIO/Scene/SceneSerializer.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "Engine/Core/Logger.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/LODComponent.hpp"
#include "Engine/Scene/components/LightCommon.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PhysicsComponents.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"

#include "ModelLib/Resources/PBRMaterial.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/fwd.hpp"
#include "nlohmann/json_fwd.hpp"
namespace {
    void glmVec3ToJson(nlohmann::json& j, const glm::vec3& v) {
        j = nlohmann::json{v.x, v.y, v.z};
    }
    void glmVec3FromJson(const nlohmann::json& j, glm::vec3& v) {
        v.x = j[0];
        v.y = j[1];
        v.z = j[2];
    }
    void glmVec4ToJson(nlohmann::json& j, const glm::vec4& v) {
        j = nlohmann::json{v.x, v.y, v.z, v.w};
    }
    void glmVec4FromJson(const nlohmann::json& j, glm::vec4& v) {
        v.x = j[0];
        v.y = j[1];
        v.z = j[2];
        v.w = j[3];
    }
}  // namespace
namespace nlohmann {
    template <>
    struct adl_serializer<glm::vec3> {
        static void to_json(json& j, const glm::vec3& v) {
            glmVec3ToJson(j, v);
        }
        static void from_json(const json& j, glm::vec3& v) {
            glmVec3FromJson(j, v);
        }
    };
    template <>
    struct adl_serializer<glm::vec4> {
        static void to_json(json& j, const glm::vec4& v) {
            glmVec4ToJson(j, v);
        }
        static void from_json(const json& j, glm::vec4& v) {
            glmVec4FromJson(j, v);
        }
    };
}  // namespace nlohmann
namespace engine {
    namespace {
        std::string physicsModeToString(RigidBodyComponent::PhysicsMode mode) {
            switch (mode) {
                case RigidBodyComponent::PhysicsMode::Dynamic:
                    return "dynamic";
                case RigidBodyComponent::PhysicsMode::Kinematic:
                    return "kinematic";
                case RigidBodyComponent::PhysicsMode::Static:
                    return "static";
                default:
                    return "dynamic";
            }
        }
        RigidBodyComponent::PhysicsMode physicsModeFromJson(const nlohmann::json& value) {
            if (value.is_string()) {
                const std::string mode = value.get<std::string>();
                if (mode == "static") {
                    return RigidBodyComponent::PhysicsMode::Static;
                }
                if (mode == "kinematic") {
                    return RigidBodyComponent::PhysicsMode::Kinematic;
                }
                return RigidBodyComponent::PhysicsMode::Dynamic;
            }
            if (value.is_number_integer()) {
                const int raw = value.get<int>();
                if (raw == static_cast<int>(RigidBodyComponent::PhysicsMode::Static)) {
                    return RigidBodyComponent::PhysicsMode::Static;
                }
                if (raw == static_cast<int>(RigidBodyComponent::PhysicsMode::Kinematic)) {
                    return RigidBodyComponent::PhysicsMode::Kinematic;
                }
                return RigidBodyComponent::PhysicsMode::Dynamic;
            }
            return RigidBodyComponent::PhysicsMode::Dynamic;
        }
        std::string colliderShapeToString(ColliderComponent::ShapeType shape) {
            switch (shape) {
                case ColliderComponent::ShapeType::Sphere:
                    return "sphere";
                case ColliderComponent::ShapeType::Box:
                    return "box";
                case ColliderComponent::ShapeType::Capsule:
                    return "capsule";
                case ColliderComponent::ShapeType::Mesh:
                    return "mesh";
                default:
                    return "sphere";
            }
        }
        ColliderComponent::ShapeType colliderShapeFromJson(const nlohmann::json& value) {
            if (value.is_string()) {
                const std::string shape = value.get<std::string>();
                if (shape == "box") {
                    return ColliderComponent::ShapeType::Box;
                }
                if (shape == "capsule") {
                    return ColliderComponent::ShapeType::Capsule;
                }
                if (shape == "mesh") {
                    return ColliderComponent::ShapeType::Mesh;
                }
                return ColliderComponent::ShapeType::Sphere;
            }
            if (value.is_number_integer()) {
                const int raw = value.get<int>();
                if (raw == static_cast<int>(ColliderComponent::ShapeType::Box)) {
                    return ColliderComponent::ShapeType::Box;
                }
                if (raw == static_cast<int>(ColliderComponent::ShapeType::Capsule)) {
                    return ColliderComponent::ShapeType::Capsule;
                }
                if (raw == static_cast<int>(ColliderComponent::ShapeType::Mesh)) {
                    return ColliderComponent::ShapeType::Mesh;
                }
                return ColliderComponent::ShapeType::Sphere;
            }
            return ColliderComponent::ShapeType::Sphere;
        }
        std::string variantPolicyToString(ModelRenderSystem::VariantPolicy policy) {
            switch (policy) {
                case ModelRenderSystem::VariantPolicy::Auto:
                    return "auto";
                case ModelRenderSystem::VariantPolicy::ForceStandard:
                    return "force_standard";
                case ModelRenderSystem::VariantPolicy::ForceFull:
                    return "force_full";
                default:
                    return "auto";
            }
        }
        ModelRenderSystem::VariantPolicy variantPolicyFromJson(const nlohmann::json& value) {
            if (value.is_string()) {
                const std::string mode = value.get<std::string>();
                if (mode == "force_standard") {
                    return ModelRenderSystem::VariantPolicy::ForceStandard;
                }
                if (mode == "force_full") {
                    return ModelRenderSystem::VariantPolicy::ForceFull;
                }
                return ModelRenderSystem::VariantPolicy::Auto;
            }
            if (value.is_number_integer()) {
                const int raw = value.get<int>();
                if (raw == static_cast<int>(ModelRenderSystem::VariantPolicy::ForceStandard)) {
                    return ModelRenderSystem::VariantPolicy::ForceStandard;
                }
                if (raw == static_cast<int>(ModelRenderSystem::VariantPolicy::ForceFull)) {
                    return ModelRenderSystem::VariantPolicy::ForceFull;
                }
            }
            return ModelRenderSystem::VariantPolicy::Auto;
        }
        std::string logLevelToString(LogLevel level) {
            switch (level) {
                case LogLevel::Error:
                    return "error";
                case LogLevel::Warn:
                    return "warn";
                case LogLevel::Info:
                    return "info";
                case LogLevel::Debug:
                    return "debug";
                default:
                    return "info";
            }
        }
        LogLevel logLevelFromJson(const nlohmann::json& value) {
            if (value.is_string()) {
                const std::string level = value.get<std::string>();
                if (level == "error") {
                    return LogLevel::Error;
                }
                if (level == "warn") {
                    return LogLevel::Warn;
                }
                if (level == "debug") {
                    return LogLevel::Debug;
                }
                return LogLevel::Info;
            }
            if (value.is_number_integer()) {
                const int raw = value.get<int>();
                if (raw <= static_cast<int>(LogLevel::Error)) {
                    return LogLevel::Error;
                }
                if (raw == static_cast<int>(LogLevel::Warn)) {
                    return LogLevel::Warn;
                }
                if (raw >= static_cast<int>(LogLevel::Debug)) {
                    return LogLevel::Debug;
                }
            }
            return LogLevel::Info;
        }
        void writePostProcessSettings(nlohmann::json& settingsJson, const PostProcessPushConstants& push) {
            settingsJson["postProcess"] = {
                {"exposure", push.exposure},
                {"contrast", push.contrast},
                {"saturation", push.saturation},
                {"vignette", push.vignette},
                {"toneMappingMode", push.toneMappingMode},
                {"enableBloom", push.enableBloom},
                {"bloomIntensity", push.bloomIntensity},
                {"bloomThreshold", push.bloomThreshold},
                {"enableFXAA", push.enableFXAA},
                {"fxaaSpanMax", push.fxaaSpanMax},
                {"fxaaReduceMul", push.fxaaReduceMul},
                {"fxaaReduceMin", push.fxaaReduceMin},
                {"enableSSAO", push.enableSSAO},
                {"ssaoRadius", push.ssaoRadius},
                {"ssaoBias", push.ssaoBias}};
        }
        void applyPostProcessSettings(const nlohmann::json& settingsJson, PostProcessPushConstants& push) {
            push.exposure        = settingsJson.value("exposure", push.exposure);
            push.contrast        = settingsJson.value("contrast", push.contrast);
            push.saturation      = settingsJson.value("saturation", push.saturation);
            push.vignette        = settingsJson.value("vignette", push.vignette);
            push.toneMappingMode = settingsJson.value("toneMappingMode", push.toneMappingMode);
            push.enableBloom     = settingsJson.value("enableBloom", push.enableBloom);
            push.bloomIntensity  = settingsJson.value("bloomIntensity", push.bloomIntensity);
            push.bloomThreshold  = settingsJson.value("bloomThreshold", push.bloomThreshold);
            push.enableFXAA      = settingsJson.value("enableFXAA", push.enableFXAA);
            push.fxaaSpanMax     = settingsJson.value("fxaaSpanMax", push.fxaaSpanMax);
            push.fxaaReduceMul   = settingsJson.value("fxaaReduceMul", push.fxaaReduceMul);
            push.fxaaReduceMin   = settingsJson.value("fxaaReduceMin", push.fxaaReduceMin);
            push.enableSSAO      = settingsJson.value("enableSSAO", push.enableSSAO);
            push.ssaoRadius      = settingsJson.value("ssaoRadius", push.ssaoRadius);
            push.ssaoBias        = settingsJson.value("ssaoBias", push.ssaoBias);
        }
        void ensureDefaultDirectionalLight(Scene& scene) {
            auto&      registry       = scene.getRegistry();
            const bool hasDirectional = !registry.view<DirectionalLightComponent>().empty();
            const bool hasPoint       = !registry.view<PointLightComponent>().empty();
            const bool hasSpot        = !registry.view<SpotLightComponent>().empty();
            if (hasDirectional || hasPoint || hasSpot) {
                return;
            }
            const bool hasModel = !registry.view<ModelComponent>().empty();
            if (!hasModel) {
                return;
            }
            auto  lightEntity     = scene.createEntity();
            auto& transform       = registry.emplace<TransformComponent>(lightEntity);
            transform.translation = {6.0f, 8.0f, -6.0f};
            transform.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
            auto& light     = registry.emplace<DirectionalLightComponent>(lightEntity);
            light.intensity = 4.0f;
            light.color     = glm::vec3(1.0f, 0.95f, 0.9f);
            light.bake      = false;
            light.lightType = LightMobility::Static;
            registry.emplace<NameComponent>(lightEntity, "DefaultDirectionalLight");
            std::cout << "SceneSerializer: created default directional light\n";
        }
    }  // namespace
    SceneSerializer::SceneSerializer(Scene& scene, ResourceManager& resourceManager) : scene(scene), resourceManager(resourceManager) {}
    void SceneSerializer::setRuntimeSettingsBindings(const RuntimeSettingsBindings& bindings) {
        settingsBindings_ = bindings;
    }
    void SceneSerializer::serialize(const std::string& filepath) {
        nlohmann::json sceneJson;
        sceneJson["objects"] = nlohmann::json::array();
        if (settingsBindings_.showSkybox != nullptr) {
            nlohmann::json settingsJson;
            settingsJson["showSkybox"] = *settingsBindings_.showSkybox;
            if (settingsBindings_.showGrid != nullptr) {
                settingsJson["showGrid"] = *settingsBindings_.showGrid;
            }
            if (settingsBindings_.showDebugObjects != nullptr) {
                settingsJson["showDebugObjects"] = *settingsBindings_.showDebugObjects;
            }
            if (settingsBindings_.physicsSimulationRunning != nullptr) {
                settingsJson["physicsSimulationRunning"] = *settingsBindings_.physicsSimulationRunning;
            }
            if (settingsBindings_.skySettings != nullptr) {
                settingsJson["sky"] = nlohmann::json{{"debugCubemapFaces", settingsBindings_.skySettings->debugCubemapFaces},
                                                     {"proceduralSky", settingsBindings_.skySettings->proceduralSky},
                                                     {"useSkyLUT", settingsBindings_.skySettings->useSkyLUT},
                                                     {"timeOfDay", settingsBindings_.skySettings->timeOfDay},
                                                     {"skyIntensity", settingsBindings_.skySettings->skyIntensity},
                                                     {"skyMode", (int)settingsBindings_.skySettings->skyMode}};
            }
            if (settingsBindings_.debugMode != nullptr) {
                settingsJson["debugMode"] = *settingsBindings_.debugMode;
            }
            if (settingsBindings_.viewGizmoOrbitSelected != nullptr) {
                settingsJson["viewGizmoOrbitSelected"] = *settingsBindings_.viewGizmoOrbitSelected;
            }
            if (settingsBindings_.multithreadedRecordingEnabled != nullptr && settingsBindings_.multithreadedRecordingThreads != nullptr) {
                settingsJson["performance"] = {
                    {"multithreadedRecordingEnabled", *settingsBindings_.multithreadedRecordingEnabled},
                    {"multithreadedRecordingThreads", *settingsBindings_.multithreadedRecordingThreads}};
            }
            if (settingsBindings_.postProcessPush != nullptr) {
                writePostProcessSettings(settingsJson, *settingsBindings_.postProcessPush);
            }
            if (settingsBindings_.iblSystem != nullptr) {
                const auto& iblSettings = settingsBindings_.iblSystem->getSettings();
                settingsJson["ibl"]     = {
                    {"irradianceSize", iblSettings.irradianceSize},
                    {"prefilterSize", iblSettings.prefilterSize},
                    {"prefilterMipLevels", iblSettings.prefilterMipLevels},
                    {"brdfLUTSize", iblSettings.brdfLUTSize},
                    {"prefilterSampleCount", iblSettings.prefilterSampleCount},
                    {"irradianceSampleDelta", iblSettings.irradianceSampleDelta}};
            }
            if (settingsBindings_.modelRenderSystem != nullptr) {
                settingsJson["shaderVariants"] = {
                    {"variantPolicy", variantPolicyToString(settingsBindings_.modelRenderSystem->variantPolicy())},
                    {"shaderHotReloadEnabled", settingsBindings_.modelRenderSystem->shaderHotReloadEnabled()}};
            }
            if (settingsBindings_.getGpuProfilerEnabled) {
                settingsJson["gpuProfiler"] = {{"enabled", settingsBindings_.getGpuProfilerEnabled()}};
            }
            settingsJson["logging"] = {
                {"minimumLevel", logLevelToString(Logger::minimumLevel())},
                {"channels",
                    {
                        {"general", Logger::isChannelEnabled(LogChannel::General)},
                        {"render", Logger::isChannelEnabled(LogChannel::Render)},
                        {"sync", Logger::isChannelEnabled(LogChannel::Sync)},
                        {"scene", Logger::isChannelEnabled(LogChannel::Scene)},
                        {"resource", Logger::isChannelEnabled(LogChannel::Resource)},
                    }}};
            sceneJson["settings"] = settingsJson;
        }
        auto view = scene.getRegistry().view<entt::entity>();
        for (auto entity : view) {
            nlohmann::json objJson;
            objJson["id"] = std::to_string((uint32_t) entity);
            if (scene.getRegistry().all_of<NameComponent>(entity)) {
                objJson["name"] = scene.getRegistry().get<NameComponent>(entity).name;
            } else {
                objJson["name"] = "GameObject";
            }
            if (scene.getRegistry().all_of<TransformComponent>(entity)) {
                auto& t              = scene.getRegistry().get<TransformComponent>(entity);
                objJson["transform"] = {{"translation", t.translation}, {"rotation", t.rotation}, {"scale", t.scale}};
            }
            if (scene.getRegistry().all_of<CameraComponent>(entity)) {
                const auto& camera = scene.getRegistry().get<CameraComponent>(entity);
                objJson["camera"]  = {
                    {"fovY", camera.fovY},
                    {"nearZ", camera.nearZ},
                    {"farZ", camera.farZ},
                    {"orthoSize", camera.orthoSize},
                    {"isOrthographic", camera.isOrthographic},
                    {"isPrimary", camera.isPrimary}};
            }
            if (scene.getRegistry().all_of<ModelComponent>(entity)) {
                auto& modelComp = scene.getRegistry().get<ModelComponent>(entity);
                if (modelComp.model) {
                    objJson["modelPath"] = modelComp.model->getFilePath();
                    if (scene.getRegistry().all_of<PBRMaterial>(entity)) {
                        auto&          mat = scene.getRegistry().get<PBRMaterial>(entity);
                        nlohmann::json matJson;
                        matJson["albedo"]    = mat.albedo;
                        matJson["metallic"]  = mat.metallic;
                        matJson["roughness"] = mat.roughness;
                        matJson["ao"]        = mat.ao;
                        objJson["material"]  = matJson;
                    }
                }
            }
            if (scene.getRegistry().all_of<PointLightComponent>(entity)) {
                auto& pl              = scene.getRegistry().get<PointLightComponent>(entity);
                objJson["pointLight"] = {{"intensity", pl.intensity}, {"color", pl.color}, {"radius", pl.radius}, {"bake", pl.bake}, {"lightType", to_string(pl.lightType)}};
            }
            if (scene.getRegistry().all_of<DirectionalLightComponent>(entity)) {
                auto& dl                    = scene.getRegistry().get<DirectionalLightComponent>(entity);
                objJson["directionalLight"] = {{"intensity", dl.intensity}, {"color", dl.color}, {"bake", dl.bake}, {"lightType", to_string(dl.lightType)}};
            }
            if (scene.getRegistry().all_of<SpotLightComponent>(entity)) {
                auto& sl = scene.getRegistry().get<SpotLightComponent>(entity);
                objJson["spotLight"] =
                    {{"intensity", sl.intensity}, {"color", sl.color}, {"innerAngle", sl.innerCutoffAngle}, {"outerAngle", sl.outerCutoffAngle}, {"bake", sl.bake}, {"lightType", to_string(sl.lightType)}};
            }
            if (scene.getRegistry().all_of<LODComponent>(entity)) {
                auto&          lod     = scene.getRegistry().get<LODComponent>(entity);
                nlohmann::json lodJson = nlohmann::json::array();
                for (const auto& level : lod.levels) {
                    if (level.model) {
                        lodJson.push_back({{"distance", level.distance}, {"modelPath", level.model->getFilePath()}});
                    }
                }
                objJson["lodComponent"] = lodJson;
            }
            if (scene.getRegistry().all_of<RigidBodyComponent>(entity)) {
                const auto& rb       = scene.getRegistry().get<RigidBodyComponent>(entity);
                objJson["rigidBody"] = {
                    {"mass", rb.mass},
                    {"velocity", rb.velocity},
                    {"acceleration", rb.acceleration},
                    {"angularVelocity", rb.angularVelocity},
                    {"isStatic", rb.isStatic},
                    {"useGravity", rb.useGravity},
                    {"friction", rb.friction},
                    {"restitution", rb.restitution},
                    {"mode", physicsModeToString(rb.mode)}};
            }
            if (scene.getRegistry().all_of<ColliderComponent>(entity)) {
                const auto& collider = scene.getRegistry().get<ColliderComponent>(entity);
                objJson["collider"]  = {
                    {"shape", colliderShapeToString(collider.shape)},
                    {"size", collider.size},
                    {"radius", collider.radius},
                    {"centerOffset", collider.centerOffset},
                    {"isTrigger", collider.isTrigger},
                    {"collisionGroup", collider.collisionGroup},
                    {"collisionMask", collider.collisionMask}};
            }
            if (scene.getRegistry().all_of<PhysicsMaterialComponent>(entity)) {
                const auto& pm             = scene.getRegistry().get<PhysicsMaterialComponent>(entity);
                objJson["physicsMaterial"] = {
                    {"friction", pm.friction},
                    {"restitution", pm.restitution},
                    {"density", pm.density},
                    {"dynamicFriction", pm.dynamicFriction},
                    {"staticFriction", pm.staticFriction},
                    {"damping", pm.damping},
                    {"angularDamping", pm.angularDamping}};
            }
            sceneJson["objects"].push_back(objJson);
        }
        std::ofstream out(filepath);
        out << sceneJson.dump(4);
        out.close();
    }
    bool SceneSerializer::deserialize(const std::string& filepath) {
        std::cout << "SceneSerializer: attempting to open scene file: " << filepath << " (abs=" << std::filesystem::absolute(filepath) << ")" << std::endl;
        std::ifstream in(filepath);
        if (!in.is_open()) {
            std::cerr << "Failed to open scene file: " << filepath << '\n';
            return false;
        }
        nlohmann::json sceneJson;
        try {
            in >> sceneJson;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse scene file: " << e.what() << '\n';
            return false;
        }
        scene.getRegistry().clear();
        bool foundAny = false;
        if (sceneJson.contains("objects")) {
            foundAny = true;
            for (const auto& objJson : sceneJson["objects"]) {
                std::string const name   = objJson.value("name", "GameObject");
                std::string const id     = objJson.value("id", name);
                auto              entity = scene.createEntity();
                scene.getRegistry().emplace<TransformComponent>(entity);
                scene.getRegistry().emplace<NameComponent>(entity, id);
                if (objJson.contains("transform")) {
                    auto& t               = objJson["transform"];
                    auto& transform       = scene.getRegistry().get<TransformComponent>(entity);
                    transform.translation = t.value("translation", glm::vec3(0.0f));
                    transform.rotation    = t.value("rotation", glm::vec3(0.0f));
                    transform.scale       = t.value("scale", glm::vec3(1.0f));
                }
                if (objJson.contains("camera")) {
                    const auto& cameraJson = objJson["camera"];
                    auto&       camera     = scene.getRegistry().emplace<CameraComponent>(entity);
                    camera.fovY            = cameraJson.value("fovY", camera.fovY);
                    camera.nearZ           = cameraJson.value("nearZ", camera.nearZ);
                    camera.farZ            = cameraJson.value("farZ", camera.farZ);
                    camera.orthoSize       = cameraJson.value("orthoSize", camera.orthoSize);
                    camera.isOrthographic  = cameraJson.value("isOrthographic", camera.isOrthographic);
                    camera.isPrimary       = cameraJson.value("isPrimary", camera.isPrimary);
                }
                if (objJson.contains("modelPath")) {
                    std::string const modelPath = objJson["modelPath"];
                    auto              model     = resourceManager.loadModel(modelPath, true, true, true);
                    scene.getRegistry().emplace<ModelComponent>(entity, model);
                    if (objJson.contains("material")) {
                        auto& matJson         = objJson["material"];
                        auto& pbrMaterial     = scene.getRegistry().emplace<PBRMaterial>(entity);
                        pbrMaterial.albedo    = glm::vec4(matJson.value("albedo", glm::vec3(1.0f)), 1.0f);
                        pbrMaterial.metallic  = matJson.value("metallic", 0.0f);
                        pbrMaterial.roughness = matJson.value("roughness", 0.5f);
                        pbrMaterial.ao        = matJson.value("ao", 1.0f);
                    }
                }
                if (objJson.contains("pointLight")) {
                    auto& pl             = objJson["pointLight"];
                    auto& pointLight     = scene.getRegistry().emplace<PointLightComponent>(entity);
                    pointLight.intensity = pl.value("intensity", 1.0f);
                    pointLight.color     = pl.value("color", glm::vec3(1.0f));
                    pointLight.radius    = pl.value("radius", 15.0f);
                    pointLight.bake      = pl.value("bake", false);
                    pointLight.lightType = mobility_from_string(pl.value("lightType", std::string("static")));
                }
                if (objJson.contains("directionalLight")) {
                    auto& dl           = objJson["directionalLight"];
                    auto& dirLight     = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
                    dirLight.intensity = dl.value("intensity", 1.0f);
                    dirLight.color     = dl.value("color", glm::vec3(1.0f));
                    dirLight.bake      = dl.value("bake", false);
                    dirLight.lightType = mobility_from_string(dl.value("lightType", std::string("static")));
                }
                if (objJson.contains("spotLight")) {
                    auto& sl                   = objJson["spotLight"];
                    auto& spotLight            = scene.getRegistry().emplace<SpotLightComponent>(entity);
                    spotLight.intensity        = sl.value("intensity", 1.0f);
                    spotLight.color            = sl.value("color", glm::vec3(1.0f));
                    spotLight.innerCutoffAngle = sl.value("innerAngle", 12.5f);
                    spotLight.outerCutoffAngle = sl.value("outerAngle", 17.5f);
                    spotLight.bake             = sl.value("bake", false);
                    spotLight.lightType        = mobility_from_string(sl.value("lightType", std::string("static")));
                }
                if (objJson.contains("lodComponent")) {
                    auto& lodComponent = scene.getRegistry().emplace<LODComponent>(entity);
                    for (const auto& levelJson : objJson["lodComponent"]) {
                        float const       distance  = levelJson.value("distance", 0.0f);
                        std::string const modelPath = levelJson.value("modelPath", "");
                        if (!modelPath.empty()) {
                            auto model = resourceManager.loadModel(modelPath, true, true, true);
                            lodComponent.levels.push_back({model, distance});
                        }
                    }
                }
                if (objJson.contains("rigidBody")) {
                    const auto& rbJson = objJson["rigidBody"];
                    auto&       rb     = scene.getRegistry().emplace<RigidBodyComponent>(entity);
                    rb.mass            = rbJson.value("mass", 1.0f);
                    rb.velocity        = rbJson.value("velocity", glm::vec3(0.0f));
                    rb.acceleration    = rbJson.value("acceleration", glm::vec3(0.0f));
                    rb.angularVelocity = rbJson.value("angularVelocity", glm::vec3(0.0f));
                    rb.isStatic        = rbJson.value("isStatic", false);
                    rb.useGravity      = rbJson.value("useGravity", true);
                    rb.friction        = rbJson.value("friction", 0.5f);
                    rb.restitution     = rbJson.value("restitution", 0.3f);
                    if (rbJson.contains("mode")) {
                        rb.mode = physicsModeFromJson(rbJson["mode"]);
                    } else {
                        rb.mode = rb.isStatic ? RigidBodyComponent::PhysicsMode::Static : RigidBodyComponent::PhysicsMode::Dynamic;
                    }
                    rb.pendingBodyStateOverride = true;
                }
                if (objJson.contains("collider")) {
                    const auto& colliderJson = objJson["collider"];
                    auto&       collider     = scene.getRegistry().emplace<ColliderComponent>(entity);
                    if (colliderJson.contains("shape")) {
                        collider.shape = colliderShapeFromJson(colliderJson["shape"]);
                    }
                    collider.size                = colliderJson.value("size", glm::vec3(1.0f));
                    collider.radius              = colliderJson.value("radius", 0.5f);
                    collider.centerOffset        = colliderJson.value("centerOffset", glm::vec3(0.0f));
                    collider.isTrigger           = colliderJson.value("isTrigger", false);
                    collider.collisionGroup      = colliderJson.value("collisionGroup", 0u);
                    collider.collisionMask       = colliderJson.value("collisionMask", 0xFFFFFFFFu);
                    collider.pendingShapeRebuild = true;
                }
                if (objJson.contains("physicsMaterial")) {
                    const auto& pmJson = objJson["physicsMaterial"];
                    auto&       pm     = scene.getRegistry().emplace<PhysicsMaterialComponent>(entity);
                    pm.friction        = pmJson.value("friction", 0.5f);
                    pm.restitution     = pmJson.value("restitution", 0.3f);
                    pm.density         = pmJson.value("density", 1.0f);
                    pm.dynamicFriction = pmJson.value("dynamicFriction", 0.4f);
                    pm.staticFriction  = pmJson.value("staticFriction", 0.6f);
                    pm.damping         = pmJson.value("damping", 0.0f);
                    pm.angularDamping  = pmJson.value("angularDamping", 0.0f);
                }
            }
        }
        if (sceneJson.contains("lights")) {
            foundAny              = true;
            auto const& lightsArr = sceneJson["lights"];
            std::cout << "SceneSerializer: found top-level lights array, count=" << lightsArr.size() << '\n';
            for (const auto& lightJson : lightsArr) {
                std::string const id = lightJson.value("id", std::string("light"));
                std::cout << "SceneSerializer: parsing light id=" << id << '\n';
                auto entity = scene.createEntity();
                scene.getRegistry().emplace<NameComponent>(entity, id);
                std::string const type = lightJson.value("type", std::string("point"));
                if (type == "point") {
                    auto& pointLight     = scene.getRegistry().emplace<PointLightComponent>(entity);
                    pointLight.intensity = lightJson.value("intensity", 1.0f);
                    pointLight.color     = lightJson.value("color", glm::vec3(1.0f));
                    pointLight.radius    = lightJson.value("radius", 15.0f);
                    pointLight.bake      = lightJson.value("bake", false);
                    pointLight.lightType = mobility_from_string(lightJson.value("lightType", std::string("static")));
                    std::cout << "SceneSerializer: added PointLightComponent id=" << id << " bake=" << pointLight.bake << "\n";
                } else if (type == "directional") {
                    auto& dirLight     = scene.getRegistry().emplace<DirectionalLightComponent>(entity);
                    dirLight.intensity = lightJson.value("intensity", 1.0f);
                    dirLight.color     = lightJson.value("color", glm::vec3(1.0f));
                    dirLight.bake      = lightJson.value("bake", false);
                    dirLight.lightType = mobility_from_string(lightJson.value("lightType", std::string("static")));
                    std::cout << "SceneSerializer: added DirectionalLightComponent id=" << id << " bake=" << dirLight.bake << "\n";
                } else if (type == "spot") {
                    auto& spotLight            = scene.getRegistry().emplace<SpotLightComponent>(entity);
                    spotLight.intensity        = lightJson.value("intensity", 1.0f);
                    spotLight.color            = lightJson.value("color", glm::vec3(1.0f));
                    spotLight.innerCutoffAngle = lightJson.value("innerAngle", 12.5f);
                    spotLight.outerCutoffAngle = lightJson.value("outerAngle", 17.5f);
                    spotLight.bake             = lightJson.value("bake", false);
                    spotLight.lightType        = mobility_from_string(lightJson.value("lightType", std::string("static")));
                    std::cout << "SceneSerializer: added SpotLightComponent id=" << id << " bake=" << spotLight.bake << "\n";
                }
            }
        }
        if (sceneJson.contains("settings") && settingsBindings_.showSkybox != nullptr) {
            const auto& settingsJson      = sceneJson["settings"];
            *settingsBindings_.showSkybox = settingsJson.value("showSkybox", *settingsBindings_.showSkybox);
            if (settingsBindings_.showGrid != nullptr) {
                *settingsBindings_.showGrid = settingsJson.value("showGrid", *settingsBindings_.showGrid);
            }
            if (settingsBindings_.showDebugObjects != nullptr) {
                *settingsBindings_.showDebugObjects = settingsJson.value("showDebugObjects", *settingsBindings_.showDebugObjects);
            }
            if (settingsBindings_.physicsSimulationRunning != nullptr) {
                *settingsBindings_.physicsSimulationRunning = settingsJson.value("physicsSimulationRunning", *settingsBindings_.physicsSimulationRunning);
            }
            if (settingsJson.contains("sky") && settingsBindings_.skySettings != nullptr) {
                const auto& skyJson                              = settingsJson["sky"];
                settingsBindings_.skySettings->debugCubemapFaces = skyJson.value("debugCubemapFaces", settingsBindings_.skySettings->debugCubemapFaces);
                settingsBindings_.skySettings->proceduralSky     = skyJson.value("proceduralSky", settingsBindings_.skySettings->proceduralSky);
                settingsBindings_.skySettings->useSkyLUT         = skyJson.value("useSkyLUT", settingsBindings_.skySettings->useSkyLUT);
                settingsBindings_.skySettings->timeOfDay         = skyJson.value("timeOfDay", settingsBindings_.skySettings->timeOfDay);
                settingsBindings_.skySettings->skyIntensity      = skyJson.value("skyIntensity", settingsBindings_.skySettings->skyIntensity);
                if (skyJson.contains("skyMode")) {
                    settingsBindings_.skySettings->skyMode = (SkyMode)skyJson.value("skyMode", (int)settingsBindings_.skySettings->skyMode);
                }
            }
            if (settingsBindings_.debugMode != nullptr) {
                *settingsBindings_.debugMode = settingsJson.value("debugMode", *settingsBindings_.debugMode);
            }
            if (settingsBindings_.viewGizmoOrbitSelected != nullptr) {
                *settingsBindings_.viewGizmoOrbitSelected = settingsJson.value(
                    "viewGizmoOrbitSelected",
                    *settingsBindings_.viewGizmoOrbitSelected);
            }
            if (settingsJson.contains("performance") && settingsBindings_.multithreadedRecordingEnabled != nullptr && settingsBindings_.multithreadedRecordingThreads != nullptr) {
                const auto& perfJson                             = settingsJson["performance"];
                *settingsBindings_.multithreadedRecordingEnabled = perfJson.value("multithreadedRecordingEnabled", *settingsBindings_.multithreadedRecordingEnabled);
                *settingsBindings_.multithreadedRecordingThreads = perfJson.value("multithreadedRecordingThreads", *settingsBindings_.multithreadedRecordingThreads);
                if (settingsBindings_.modelRenderSystem != nullptr) {
                    settingsBindings_.modelRenderSystem->enableMultiThreadedRecording(
                        *settingsBindings_.multithreadedRecordingEnabled,
                        *settingsBindings_.multithreadedRecordingThreads);
                }
            }
            if (settingsJson.contains("postProcess") && settingsBindings_.postProcessPush != nullptr) {
                applyPostProcessSettings(settingsJson["postProcess"], *settingsBindings_.postProcessPush);
            }
            if (settingsJson.contains("ibl") && settingsBindings_.iblSystem != nullptr) {
                auto        iblSettings           = settingsBindings_.iblSystem->getSettings();
                const auto& iblJson               = settingsJson["ibl"];
                iblSettings.irradianceSize        = iblJson.value("irradianceSize", iblSettings.irradianceSize);
                iblSettings.prefilterSize         = iblJson.value("prefilterSize", iblSettings.prefilterSize);
                iblSettings.prefilterMipLevels    = iblJson.value("prefilterMipLevels", iblSettings.prefilterMipLevels);
                iblSettings.brdfLUTSize           = iblJson.value("brdfLUTSize", iblSettings.brdfLUTSize);
                iblSettings.prefilterSampleCount  = iblJson.value("prefilterSampleCount", iblSettings.prefilterSampleCount);
                iblSettings.irradianceSampleDelta = iblJson.value("irradianceSampleDelta", iblSettings.irradianceSampleDelta);
                settingsBindings_.iblSystem->updateSettings(iblSettings);
            }
            if (settingsJson.contains("shaderVariants") && settingsBindings_.modelRenderSystem != nullptr) {
                const auto& variantsJson = settingsJson["shaderVariants"];
                if (variantsJson.contains("variantPolicy")) {
                    settingsBindings_.modelRenderSystem->setVariantPolicy(variantPolicyFromJson(variantsJson["variantPolicy"]));
                }
                settingsBindings_.modelRenderSystem->setShaderHotReloadEnabled(
                    variantsJson.value("shaderHotReloadEnabled", settingsBindings_.modelRenderSystem->shaderHotReloadEnabled()));
            }
            if (settingsJson.contains("gpuProfiler")) {
                const auto& profilerJson = settingsJson["gpuProfiler"];
                if (settingsBindings_.setGpuProfilerEnabled) {
                    bool const current = settingsBindings_.getGpuProfilerEnabled ? settingsBindings_.getGpuProfilerEnabled() : false;
                    settingsBindings_.setGpuProfilerEnabled(profilerJson.value("enabled", current));
                }
            }
            if (settingsJson.contains("logging")) {
                const auto& loggingJson = settingsJson["logging"];
                if (loggingJson.contains("minimumLevel")) {
                    Logger::setMinimumLevel(logLevelFromJson(loggingJson["minimumLevel"]));
                }
                if (loggingJson.contains("channels")) {
                    const auto& channelsJson = loggingJson["channels"];
                    Logger::enableChannel(LogChannel::General, channelsJson.value("general", Logger::isChannelEnabled(LogChannel::General)));
                    Logger::enableChannel(LogChannel::Render, channelsJson.value("render", Logger::isChannelEnabled(LogChannel::Render)));
                    Logger::enableChannel(LogChannel::Sync, channelsJson.value("sync", Logger::isChannelEnabled(LogChannel::Sync)));
                    Logger::enableChannel(LogChannel::Scene, channelsJson.value("scene", Logger::isChannelEnabled(LogChannel::Scene)));
                    Logger::enableChannel(LogChannel::Resource, channelsJson.value("resource", Logger::isChannelEnabled(LogChannel::Resource)));
                }
            }
        }
        {
            auto camViewCT              = scene.getRegistry().view<engine::CameraComponent, engine::TransformComponent>();
            bool hasCameraWithTransform = (camViewCT.begin() != camViewCT.end());
            if (!hasCameraWithTransform) {
                auto camView = scene.getRegistry().view<engine::CameraComponent>();
                auto it      = camView.begin();
                if (it != camView.end()) {
                    scene.getRegistry().emplace<engine::TransformComponent>(*it);
                    std::cout << "SceneSerializer: added TransformComponent to existing Camera entity\n";
                } else {
                    auto e = scene.createEntity();
                    scene.getRegistry().emplace<engine::TransformComponent>(e);
                    scene.getRegistry().emplace<engine::CameraComponent>(e);
                    scene.getRegistry().emplace<engine::NameComponent>(e, "Camera");
                    std::cout << "SceneSerializer: created default Camera entity\n";
                }
            }
        }
        ensureDefaultDirectionalLight(scene);
        return foundAny;
    }
}  // namespace engine
