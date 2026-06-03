#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path findRepoRoot() {
    fs::path current = fs::current_path();
    while (!current.empty()) {
        if (fs::exists(current / "xmake.lua") && fs::exists(current / "include" / "Engine") && fs::exists(current / "src" / "Engine")) {
            return current;
        }
        auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

std::string readWholeFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void expectNoDirectSystemMemberAccess(const std::string& source,
    const std::vector<std::string>& forbiddenTokens,
    const std::string& context,
    const std::set<std::string>& portPrefixes = {}) {
    for (const auto& token : forbiddenTokens) {
        size_t pos = 0;
        bool found = false;
        while ((pos = source.find(token, pos)) != std::string::npos) {
            // Check if this is a port-based call (e.g., physicsPort_->joltPhysicsSystem())
            bool isPortCall = false;
            for (const auto& prefix : portPrefixes) {
                if (pos >= prefix.size() && source.substr(pos - prefix.size(), prefix.size()) == prefix) {
                    isPortCall = true;
                    break;
                }
            }
            if (!isPortCall) {
                found = true;
                break;
            }
            ++pos;
        }
        EXPECT_FALSE(found)
            << context << " should use EngineState accessors/services instead of direct member access: " << token;
    }
}

}  // namespace

TEST(EngineLifecycleContracts, InitializeRequiresExplicitRenderContextParameterAndGuard) {
    const fs::path root = findRepoRoot();
    ASSERT_FALSE(root.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string header = readWholeFile(root / "include/Engine/EngineState.hpp");
    const std::string source = readWholeFile(root / "src/Engine/EngineState.cpp");

    ASSERT_FALSE(header.empty()) << "Failed to read include/Engine/EngineState.hpp";
    ASSERT_FALSE(source.empty()) << "Failed to read src/Engine/EngineState.cpp";

    EXPECT_NE(header.find("IRenderContextPort* renderContextPort"), std::string::npos)
        << "EngineState::initialize must declare explicit IRenderContextPort dependency in the public API";

    EXPECT_NE(source.find("renderContextPort == nullptr"), std::string::npos)
        << "EngineState::initialize must validate non-null IRenderContextPort";

    EXPECT_NE(source.find("EngineState::initialize requires a non-null IRenderContextPort"), std::string::npos)
        << "EngineState::initialize should fail with a clear contract violation message";
}

TEST(EngineLifecycleContracts, AppRecreatesPostProcessingSystemAfterSwapchainRecreation) {
    const fs::path root = findRepoRoot();
    ASSERT_FALSE(root.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string appSource = readWholeFile(root / "src/Editor/app.cpp");
    ASSERT_FALSE(appSource.empty()) << "Failed to read src/Editor/app.cpp";

    EXPECT_NE(appSource.find("renderer.wasSwapChainRecreated()"), std::string::npos)
        << "App::render should react to swapchain recreation events";

    // Post-processing recreation is now done via the IPostProcessingAccessPort adapter,
    // not directly through EngineState.
    EXPECT_NE(appSource.find("postProcessingAccessAdapter->recreatePostProcessingSystemWithExistingLayout("), std::string::npos)
        << "App::render should recreate post-processing system via IPostProcessingAccessPort on swapchain recreation";
}

TEST(EngineLifecycleContracts, AppWiresSceneLoadingThroughApplicationUseCase) {
    const fs::path root = findRepoRoot();
    ASSERT_FALSE(root.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string appSource = readWholeFile(root / "src/Editor/app.cpp");
    ASSERT_FALSE(appSource.empty()) << "Failed to read src/Editor/app.cpp";

    EXPECT_NE(appSource.find("std::make_unique<ScenePersistenceAdapter>(sceneSerializer)"), std::string::npos)
        << "App should construct ScenePersistenceAdapter as Infrastructure implementation of persistence port";
    EXPECT_NE(appSource.find("std::make_unique<PhysicsRuntimeAdapter>(engineState)"), std::string::npos)
        << "App should construct PhysicsRuntimeAdapter as Infrastructure implementation of physics port";
    EXPECT_NE(appSource.find("std::make_unique<EnvironmentLightingAdapter>(device, engineState)"), std::string::npos)
        << "App should construct EnvironmentLightingAdapter as Infrastructure implementation of environment lighting port";
    EXPECT_NE(appSource.find("std::make_unique<LoadSceneUseCase>(*sceneRuntime.scene, *scenePersistencePort, physicsRuntimePort.get())"), std::string::npos)
        << "App should construct LoadSceneUseCase in Application layer";
    EXPECT_NE(appSource.find("std::make_unique<SceneSelectionMaintenanceAdapter>(*uiManager)"), std::string::npos)
        << "App should construct SceneSelectionMaintenanceAdapter as Delivery implementation of selection maintenance port";
    EXPECT_NE(appSource.find("std::make_unique<ReconcileSceneLoadUseCase>(*sceneRuntime.scene)"), std::string::npos)
        << "App should construct ReconcileSceneLoadUseCase in Application layer";
    EXPECT_NE(appSource.find("std::make_unique<ProcessSceneSelectionMaintenanceUseCase>(*sceneSelectionMaintenancePort)"), std::string::npos)
        << "App should construct ProcessSceneSelectionMaintenanceUseCase in Application layer";
    EXPECT_NE(appSource.find("std::make_unique<SaveSceneUseCase>(*scenePersistencePort)"), std::string::npos)
        << "App should construct SaveSceneUseCase in Application layer";
    EXPECT_NE(appSource.find("std::make_unique<SyncEnvironmentLightingUseCase>(*environmentLightingPort)"), std::string::npos)
        << "App should construct SyncEnvironmentLightingUseCase in Application layer";
    EXPECT_NE(appSource.find("SceneRuntimeState App::sceneRuntimeState()"), std::string::npos)
        << "App should expose a shared SceneRuntimeState builder for application use cases";
    EXPECT_NE(appSource.find("auto loadRefs = sceneRuntimeState();"), std::string::npos)
        << "App should build runtime state through SceneRuntimeState helper";
    EXPECT_NE(appSource.find("loadSceneUseCase->execute(\"scene.json\", loadRefs)"), std::string::npos)
        << "App should execute scene loading through LoadSceneUseCase";
    EXPECT_NE(appSource.find("reconcileSceneLoadUseCase->execute(runtimeState)"), std::string::npos)
        << "App should execute post-load reconciliation through Application use case";
    EXPECT_NE(appSource.find("processSceneSelectionMaintenanceUseCase->execute(runtimeState)"), std::string::npos)
        << "App should execute selection maintenance through Application use case";
    EXPECT_NE(appSource.find("saveSceneUseCase->execute(\"scene.json\")"), std::string::npos)
        << "App should execute scene saving through SaveSceneUseCase";
    EXPECT_NE(appSource.find("syncEnvironmentLightingUseCase->execute(showSkyboxEnabled)"), std::string::npos)
        << "App should execute environment-lighting sync through Application use case";
}

TEST(EngineLifecycleContracts, EngineStateNoLongerOwnsEditorUiObjects) {
    const fs::path root = findRepoRoot();
    ASSERT_FALSE(root.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string header = readWholeFile(root / "include/Engine/EngineState.hpp");
    ASSERT_FALSE(header.empty()) << "Failed to read include/Engine/EngineState.hpp";

    EXPECT_EQ(header.find("std::unique_ptr<UIManager>"), std::string::npos)
        << "EngineState should not own editor UI manager";
    EXPECT_EQ(header.find("std::unique_ptr<ImGuiManager>"), std::string::npos)
        << "EngineState should not own editor ImGui manager";
}

TEST(EngineLifecycleContracts, EngineStateProvidesGroupedSystemServicesAccessor) {
    const fs::path root = findRepoRoot();
    ASSERT_FALSE(root.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string header = readWholeFile(root / "include/Engine/EngineState.hpp");
    const std::string stateViewsHeader = readWholeFile(root / "include/Engine/State/StateViews.hpp");
    ASSERT_FALSE(header.empty()) << "Failed to read include/Engine/EngineState.hpp";
    ASSERT_FALSE(stateViewsHeader.empty()) << "Failed to read include/Engine/State/StateViews.hpp";

    EXPECT_NE(stateViewsHeader.find("struct SystemServicesView"), std::string::npos)
        << "Engine state view module should expose grouped system service view";
    EXPECT_NE(header.find("systemServices()"), std::string::npos)
        << "EngineState should still provide systemServices() (now private for internal use only)";

    // Verify systemServices() is NOT public - it was moved to private in Phase 2
    const auto publicSection = header.substr(0, header.find("private:"));
    EXPECT_EQ(publicSection.find("systemServices()"), std::string::npos)
        << "systemServices() should be private, not public";

    EXPECT_NE(header.find("RenderingStateService renderingService()"), std::string::npos)
        << "EngineState should provide renderingService() accessor for narrower rendering state service";
    EXPECT_NE(header.find("SceneRuntimeService sceneRuntimeService()"), std::string::npos)
        << "EngineState should provide sceneRuntimeService() accessor for narrower scene runtime service";
    EXPECT_NE(header.find("InputStateService inputService()"), std::string::npos)
        << "EngineState should provide inputService() accessor for narrower input state service";
    EXPECT_NE(header.find("ResourceStateService resourceService()"), std::string::npos)
        << "EngineState should provide resourceService() accessor for narrower resource state service";
}

TEST(EngineLifecycleContracts, HotPathsUseEngineStateSystemAccessorsNotDirectMembers) {
    const fs::path root = findRepoRoot();
    ASSERT_FALSE(root.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string updatePass = readWholeFile(root / "src/Engine/Graphics/Passes/UpdatePass.cpp");
    const std::string offscreenPass = readWholeFile(root / "src/Engine/Graphics/Passes/OffscreenPass.cpp");
    const std::string shadowPass = readWholeFile(root / "src/Engine/Graphics/Passes/ShadowPass.cpp");
    const std::string settingsPanel = readWholeFile(root / "src/Editor/ui/SettingsPanel.cpp");

    ASSERT_FALSE(updatePass.empty()) << "Failed to read src/Engine/Graphics/Passes/UpdatePass.cpp";
    ASSERT_FALSE(offscreenPass.empty()) << "Failed to read src/Engine/Graphics/Passes/OffscreenPass.cpp";
    ASSERT_FALSE(shadowPass.empty()) << "Failed to read src/Engine/Graphics/Passes/ShadowPass.cpp";
    ASSERT_FALSE(settingsPanel.empty()) << "Failed to read src/Editor/ui/SettingsPanel.cpp";

    expectNoDirectSystemMemberAccess(updatePass,
        {{"->objectSelectionSystem", "->inputSystem", "->joltPhysicsSystem"}},
        "UpdatePass",
        {"physicsPort_"});

    expectNoDirectSystemMemberAccess(offscreenPass,
        {{"->modelRenderSystem", "->deferredLightingSystem", "->gridRenderSystem", "->lightSystem", "->cameraSystem", "->colliderDebugRenderSystem", "->shadowSystem"}},
        "OffscreenPass");

    expectNoDirectSystemMemberAccess(shadowPass,
        {{"->shadowSystem"}},
        "ShadowPass");

    expectNoDirectSystemMemberAccess(settingsPanel,
        {{"->modelRenderSystem"}},
        "SettingsPanel");
}
