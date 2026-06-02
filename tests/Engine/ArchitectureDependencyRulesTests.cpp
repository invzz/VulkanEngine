#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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

std::vector<std::string> findIncludeViolations(
    const fs::path& root,
    const fs::path& relativeDir,
    const std::vector<std::string>& forbiddenIncludePrefixes) {
    std::vector<std::string> violations;
    const fs::path scanRoot = root / relativeDir;

    if (!fs::exists(scanRoot)) {
        violations.push_back("Missing expected directory: " + scanRoot.string());
        return violations;
    }

    for (const auto& entry : fs::recursive_directory_iterator(scanRoot)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path& path = entry.path();
        const auto ext = path.extension().string();
        if (ext != ".hpp" && ext != ".h" && ext != ".cpp" && ext != ".cc" && ext != ".cxx") {
            continue;
        }

        std::ifstream in(path);
        if (!in.is_open()) {
            violations.push_back("Could not open file: " + path.string());
            continue;
        }

        std::string line;
        size_t lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            for (const auto& prefix : forbiddenIncludePrefixes) {
                if (line.find("#include \"" + prefix) != std::string::npos) {
                    violations.push_back(path.lexically_relative(root).string() + ":" + std::to_string(lineNo) + " -> " + line);
                    break;
                }
            }
        }
    }

    return violations;
}

std::vector<std::string> findTokenViolations(
    const fs::path& root,
    const fs::path& relativeDir,
    const std::vector<std::string>& forbiddenTokens) {
    std::vector<std::string> violations;
    const fs::path scanRoot = root / relativeDir;

    if (!fs::exists(scanRoot)) {
        violations.push_back("Missing expected directory: " + scanRoot.string());
        return violations;
    }

    for (const auto& entry : fs::recursive_directory_iterator(scanRoot)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path& path = entry.path();
        const auto ext = path.extension().string();
        if (ext != ".hpp" && ext != ".h" && ext != ".cpp" && ext != ".cc" && ext != ".cxx") {
            continue;
        }

        std::ifstream in(path);
        if (!in.is_open()) {
            violations.push_back("Could not open file: " + path.string());
            continue;
        }

        std::string line;
        size_t lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            for (const auto& token : forbiddenTokens) {
                if (line.find(token) != std::string::npos) {
                    violations.push_back(path.lexically_relative(root).string() + ":" + std::to_string(lineNo) + " -> " + line);
                    break;
                }
            }
        }
    }

    return violations;
}

std::string joinViolations(const std::vector<std::string>& violations) {
    std::string out;
    for (const auto& violation : violations) {
        out += "\n" + violation;
    }
    return out;
}

std::vector<std::string> filterUnknownViolations(
    const std::vector<std::string>& violations,
    const std::set<std::string>& allowedFiles) {
    std::vector<std::string> unknown;
    for (const auto& violation : violations) {
        const auto colonPos = violation.find(':');
        const std::string file = (colonPos == std::string::npos) ? violation : violation.substr(0, colonPos);
        if (!allowedFiles.contains(file)) {
            unknown.push_back(violation);
        }
    }
    return unknown;
}

std::string readWholeFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

TEST(ArchitectureDependencyRules, EngineHeadersMustNotIncludeEditorHeaders) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"include"} / "Engine",
        {"Editor/"});
    EXPECT_TRUE(violations.empty())
        << "Found forbidden Engine->Editor includes in include/Engine:" << joinViolations(violations);
}

TEST(ArchitectureDependencyRules, EngineSourcesMustNotIncludeEditorHeaders) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Engine",
        {"Editor/"});

    // Transitional allowlist: keep this list small and burn it down over time.
    const std::set<std::string> allowedFiles = {
        "src/Engine/EngineState.cpp",
        "src/Engine/Graphics/Passes/CompositionPass.cpp",
        "src/Engine/Graphics/Passes/OffscreenPass.cpp",
        "src/Engine/Graphics/Passes/ShadowPass.cpp",
    };

    const auto unknownViolations = filterUnknownViolations(violations, allowedFiles);
    EXPECT_TRUE(unknownViolations.empty())
        << "Found non-allowlisted Engine->Editor includes in src/Engine:" << joinViolations(unknownViolations)
        << "\nIf this dependency is intentional, add it to the temporary allowlist with justification.";
}

TEST(ArchitectureDependencyRules, ApplicationHeadersMustNotDependOnInfrastructureOrDelivery) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"include"} / "Engine" / "Application",
        {
            "Editor/",
            "Engine/Graphics/",
            "Engine/Systems/",
            "EngineSceneIO/",
            "ModelLib/",
        });
    EXPECT_TRUE(violations.empty())
        << "Found forbidden Application header dependencies (must only depend on Domain/Ports/contracts):"
        << joinViolations(violations);
}

TEST(ArchitectureDependencyRules, ApplicationSourcesMustNotDependOnEditorDeliveryLayer) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Engine" / "Application",
        {"Editor/"});
    EXPECT_TRUE(violations.empty())
        << "Found forbidden Application source dependencies on delivery/editor layer:" << joinViolations(violations);
}

TEST(ArchitectureDependencyRules, DomainHeadersMustNotDependOnApplicationOrEditor) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"include"} / "Engine" / "Scene",
        {
            "Engine/Application/",
            "Editor/",
        });
    EXPECT_TRUE(violations.empty())
        << "Found forbidden Domain header dependencies on Application/Editor layers:" << joinViolations(violations);
}

TEST(ArchitectureDependencyRules, DomainSourcesMustNotDependOnApplicationOrEditor) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Engine" / "Scene",
        {
            "Engine/Application/",
            "Editor/",
        });
    EXPECT_TRUE(violations.empty())
        << "Found forbidden Domain source dependencies on Application/Editor layers:" << joinViolations(violations);
}

TEST(ArchitectureDependencyRules, InfrastructureAdaptersMustOnlyDependOnPorts) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"include"} / "Editor" / "Infrastructure",
        {
            "Editor/ui/",
            "Engine/Graphics/",
            "Engine/Systems/",
            "EngineSceneIO/",
            "ModelLib/",
        });
    EXPECT_TRUE(violations.empty())
        << "Infrastructure adapter headers must only depend on Application Ports (not Editor internals or Engine internals):"
        << joinViolations(violations);
}

TEST(ArchitectureDependencyRules, InfrastructureAdaptersSourcesMustOnlyDependOnPortsAndRuntime) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Editor" / "Infrastructure",
        {
            "Editor/ui/",
            "Engine/Application/UseCases/",
            "Engine/Application/SceneRuntimeState.hpp",
        });
    EXPECT_TRUE(violations.empty())
        << "Infrastructure adapter sources must not depend on Application use cases or runtime state:"
        << joinViolations(violations);
}

TEST(ArchitectureDependencyRules, RuntimeSystemsMustNotDependOnApplicationLayer) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto headerViolations = findIncludeViolations(
        repoRoot,
        fs::path{"include"} / "Engine" / "Systems",
        {"Engine/Application/"});
    EXPECT_TRUE(headerViolations.empty())
        << "Runtime system headers must not depend on Application layer:" << joinViolations(headerViolations);

    const auto sourceViolations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Engine" / "Systems",
        {"Engine/Application/"});
    EXPECT_TRUE(sourceViolations.empty())
        << "Runtime system sources must not depend on Application layer:" << joinViolations(sourceViolations);
}

TEST(ArchitectureDependencyRules, DeliveryAppMustNotPerformPostLoadCameraReconciliation) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string appSource = readWholeFile(repoRoot / "src/Editor/app.cpp");
    ASSERT_FALSE(appSource.empty()) << "Failed to read src/Editor/app.cpp";

    EXPECT_EQ(appSource.find("if (pendingUpdateCameraAfterSceneLoad)"), std::string::npos)
        << "Delivery should not own post-load camera reconciliation branching";
    EXPECT_EQ(appSource.find("registry.view<engine::CameraComponent>()"), std::string::npos)
        << "Delivery should not scan CameraComponent views for post-load reconciliation";
    EXPECT_NE(appSource.find("reconcileSceneLoadUseCase->execute(runtimeState)"), std::string::npos)
        << "Delivery should delegate post-load camera reconciliation through Application use case";
}

TEST(ArchitectureDependencyRules, DeliveryAppMustNotPerformEnvironmentLightingOrDescriptorOrchestration) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string appSource = readWholeFile(repoRoot / "src/Editor/app.cpp");
    ASSERT_FALSE(appSource.empty()) << "Failed to read src/Editor/app.cpp";

    EXPECT_EQ(appSource.find("Skybox::loadFromFolder"), std::string::npos)
        << "Delivery should not load skybox resources directly in update flow";
    EXPECT_EQ(appSource.find("DescriptorWriter("), std::string::npos)
        << "Delivery should not rewrite deferred IBL descriptor sets directly in update flow";
    EXPECT_EQ(appSource.find("deferredIblDescriptorSetsRef()"), std::string::npos)
        << "Delivery should not manipulate deferred IBL descriptor set state directly";
    EXPECT_NE(appSource.find("syncEnvironmentLightingUseCase->execute(showSkyboxEnabled)"), std::string::npos)
        << "Delivery should delegate environment lighting sync through Application use case";
}

TEST(ArchitectureDependencyRules, GroupedStateAccessorsShouldBeReplacedByNarrowStateServices) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findTokenViolations(
        repoRoot,
        fs::path{"src"},
        {
            "->renderingState(",
            "->sceneState(",
            "->inputState(",
            "->resourceState(",
            "->systemServices(",
        });

    const std::set<std::string> allowedFiles = {
        "src/Engine/State/StateServices.cpp",
        "src/Engine/Graphics/Passes/ComputePass.cpp",
        "src/Engine/Graphics/Passes/OffscreenPass.cpp",
        "src/Engine/Graphics/Passes/UpdatePass.cpp",
    };

    const auto unknownViolations = filterUnknownViolations(violations, allowedFiles);
    EXPECT_TRUE(unknownViolations.empty())
        << "Found direct grouped-state accessor usages outside allowed migration shims:" << joinViolations(unknownViolations);
}

TEST(ArchitectureDependencyRules, EditorUiShouldUseNarrowServicesInsteadOfLegacyEngineStateGetters) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findTokenViolations(
        repoRoot,
        fs::path{"src"} / "Editor" / "ui",
        {
            "getScene(",
            "getRenderContext(",
            "getResourceManager(",
            "getIBLSystem(",
            "getModelRenderSystem(",
            "getAnimationSystem(",
            "getObjectSelectionSystem(",
            "getInputSystem(",
            "getCameraSystem(",
            "getColliderDebugRenderSystem(",
            "getShadowSystem(",
            "getLightSystem(",
            "getGridRenderSystem(",
            "getDeferredLightingSystem(",
            "getJoltPhysicsSystem(",
            "getPostProcessingSystem(",
            "renderingState(",
            "sceneState(",
            "inputState(",
            "resourceState(",
            "systemServices(",
        });

    // Transitional allowlist: keep empty unless a migration exception is justified.
    const std::set<std::string> allowedFiles = {};

    const auto unknownViolations = filterUnknownViolations(violations, allowedFiles);
    EXPECT_TRUE(unknownViolations.empty())
        << "Editor UI must use narrow EngineState services/views instead of legacy getters:" << joinViolations(unknownViolations);
}

TEST(ArchitectureDependencyRules, RenderPassesShouldUseStateServicesInsteadOfLegacyEngineStateGetters) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const auto violations = findTokenViolations(
        repoRoot,
        fs::path{"src"} / "Engine" / "Graphics" / "Passes",
        {
            "getScene(",
            "getRenderContext(",
            "getResourceManager(",
            "getIBLSystem(",
            "getModelRenderSystem(",
            "getAnimationSystem(",
            "getObjectSelectionSystem(",
            "getInputSystem(",
            "getCameraSystem(",
            "getColliderDebugRenderSystem(",
            "getShadowSystem(",
            "getLightSystem(",
            "getGridRenderSystem(",
            "getDeferredLightingSystem(",
            "getJoltPhysicsSystem(",
            "getPostProcessingSystem(",
        });

    const std::set<std::string> allowedFiles = {};

    const auto unknownViolations = filterUnknownViolations(violations, allowedFiles);
    EXPECT_TRUE(unknownViolations.empty())
        << "Render passes must use EngineState services/views instead of legacy getters:" << joinViolations(unknownViolations);
}

TEST(ArchitectureDependencyRules, MigratedPanelsShouldUseStateServicesForRuntimeQueries) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string settingsPanel = readWholeFile(repoRoot / "src/Editor/ui/SettingsPanel.cpp");
    const std::string iblPanel = readWholeFile(repoRoot / "src/Editor/ui/IBLPanel.cpp");

    ASSERT_FALSE(settingsPanel.empty()) << "Failed to read src/Editor/ui/SettingsPanel.cpp";
    ASSERT_FALSE(iblPanel.empty()) << "Failed to read src/Editor/ui/IBLPanel.cpp";

    EXPECT_EQ(settingsPanel.find("getModelRenderSystem("), std::string::npos)
        << "SettingsPanel should use renderingService().view().modelRenderSystem instead of getModelRenderSystem()";
    EXPECT_EQ(settingsPanel.find("cameraEntityValue("), std::string::npos)
        << "SettingsPanel should use sceneRuntimeService().view().cameraEntity instead of cameraEntityValue()";
    EXPECT_EQ(settingsPanel.find("getScene("), std::string::npos)
        << "SettingsPanel should use sceneRuntimeService().view().scene instead of getScene()";
    EXPECT_EQ(settingsPanel.find("getResourceManager("), std::string::npos)
        << "SettingsPanel should use resourceService().view().resourceManager instead of getResourceManager()";

    EXPECT_EQ(iblPanel.find("getIBLSystem("), std::string::npos)
        << "IBLPanel should use renderingService().view().iblSystem instead of getIBLSystem()";
    EXPECT_EQ(iblPanel.find("getSkybox("), std::string::npos)
        << "IBLPanel should use sceneRuntimeService().view().skybox instead of getSkybox()";
}

TEST(ArchitectureDependencyRules, ScenePanelShouldUseSceneRuntimeServiceForSceneAccess) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();

    const std::string scenePanel = readWholeFile(repoRoot / "src/Editor/ui/ScenePanel.cpp");
    ASSERT_FALSE(scenePanel.empty()) << "Failed to read src/Editor/ui/ScenePanel.cpp";

    EXPECT_EQ(scenePanel.find("getScene("), std::string::npos)
        << "ScenePanel should query scene through sceneRuntimeService().view().scene instead of getScene()";
    EXPECT_EQ(scenePanel.find("getResourceManager("), std::string::npos)
        << "ScenePanel should use resourceService().view().resourceManager instead of getResourceManager()";
    EXPECT_NE(scenePanel.find("sceneRuntimeService().view()"), std::string::npos)
        << "ScenePanel should use sceneRuntimeService().view() for runtime scene access";
}
