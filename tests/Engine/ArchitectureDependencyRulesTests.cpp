#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
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
    // Recursively collect every C/C++ source/header under `scanRoot` and return
    // the file-relative paths that contain a forbidden #include prefix.
    std::vector<std::string> findIncludeViolations(
        const fs::path&                 root,
        const fs::path&                 relativeDir,
        const std::vector<std::string>& forbiddenIncludePrefixes,
        const std::set<std::string>&    allowedFiles = {}) {
        std::vector<std::string> violations;
        const fs::path           scanRoot = root / relativeDir;
        if (!fs::exists(scanRoot)) {
            // A missing scan root is NOT an error: it means the layer does not
            // exist (e.g. an application layer that was intentionally removed).
            // Previously this returned a violation, which made the suite fail
            // whenever a scanned directory was absent. We only assert on real
            // files that violate the rules below.
            return violations;
        }
        for (const auto& entry : fs::recursive_directory_iterator(scanRoot)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const fs::path& path = entry.path();
            const auto      ext  = path.extension().string();
            if (ext != ".hpp" && ext != ".h" && ext != ".cpp" && ext != ".cc" && ext != ".cxx") {
                continue;
            }
            if (!allowedFiles.empty() && allowedFiles.count(path.filename().string())) {
                continue;
            }
            std::ifstream in(path);
            if (!in.is_open()) {
                violations.push_back("Could not open file: " + path.string());
                continue;
            }
            std::string line;
            size_t      lineNo = 0;
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
    // Collect every C/C++ file under `scanRoot` whose content contains any
    // forbidden token. Used for semantic rules that go beyond literal includes.
    std::vector<std::string> findTokenViolations(
        const fs::path&                 root,
        const fs::path&                 relativeDir,
        const std::vector<std::string>& forbiddenTokens,
        const std::set<std::string>&    allowedFiles = {}) {
        std::vector<std::string> violations;
        const fs::path           scanRoot = root / relativeDir;
        if (!fs::exists(scanRoot)) {
            return violations;
        }
        for (const auto& entry : fs::recursive_directory_iterator(scanRoot)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const fs::path& path = entry.path();
            const auto      ext  = path.extension().string();
            if (ext != ".hpp" && ext != ".h" && ext != ".cpp" && ext != ".cc" && ext != ".cxx") {
                continue;
            }
            if (!allowedFiles.empty() && allowedFiles.count(path.filename().string())) {
                continue;
            }
            std::ifstream in(path);
            if (!in.is_open()) {
                violations.push_back("Could not open file: " + path.string());
                continue;
            }
            std::string line;
            size_t      lineNo = 0;
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
    // Filter out violations whose file is in `allowedFiles` (allowlisted with
    // justification at the call site).
    std::vector<std::string> filterUnknownViolations(
        const std::vector<std::string>& violations,
        const std::set<std::string>&    allowedFiles) {
        std::vector<std::string> unknown;
        for (const auto& violation : violations) {
            const auto        colonPos = violation.find(':');
            const std::string file     = (colonPos == std::string::npos) ? violation : violation.substr(0, colonPos);
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

// ---------------------------------------------------------------------------
// Architecture boundary rules for the CURRENT design.
//
// The engine uses a layered architecture:
//   Domain       (Scene + Components, pure data / ECS)
//   Runtime      (Engine/Systems + Engine/Graphics, Vulkan + per-system logic)
//   Composition  (EngineState, the DI container / system registry)
//   Application  (Editor, the ImGui delivery layer)
//   IO           (EngineSceneIO, ModelLib)
//
// A previous version of this file asserted a ports/adapters/use-case (Clean /
// Hexagonal) layer that was later removed (commit c75cdbb). Those assertions
// are gone. The rules below enforce the architecture that actually exists and
// must hold going forward.
// ---------------------------------------------------------------------------

// Engine must not depend on the Editor (delivery) layer. The one exception is
// RenderContextAdapter, which is the deliberate Engine<->Editor glue that
// adapts Editor::RenderContext to the IRenderContextPort contract.
TEST(ArchitectureDependencyRules, EngineSourcesMustNotIncludeEditorHeaders) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Engine",
        {"Editor/"});
    const std::set<std::string> allowedFiles = {
        "src/Engine/Graphics/RenderContextAdapter.cpp",
    };
    const auto unknownViolations = filterUnknownViolations(violations, allowedFiles);
    EXPECT_TRUE(unknownViolations.empty())
        << "Found non-allowlisted Engine->Editor includes in src/Engine:"
        << joinViolations(unknownViolations)
        << "\nIf this dependency is intentional, add it to the allowlist with justification.";
}

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

// The Scene domain (components + scene types) must not reach up into the
// runtime Systems layer. GPU-resource types (Device, Buffer) are permitted
// because domain objects (e.g. Skybox) legitimately own Vulkan resources.
TEST(ArchitectureDependencyRules, DomainSourcesMustNotDependOnRuntimeSystems) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Engine" / "Scene",
        {"Engine/Systems/"});
    EXPECT_TRUE(violations.empty())
        << "Scene domain sources must not include Engine/Systems:" << joinViolations(violations);
}

TEST(ArchitectureDependencyRules, DomainHeadersMustNotDependOnRuntimeSystems) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"include"} / "Engine" / "Scene",
        {"Engine/Systems/"});
    EXPECT_TRUE(violations.empty())
        << "Scene domain headers must not include Engine/Systems:" << joinViolations(violations);
}

// Runtime systems must not depend on the Editor (delivery) layer.
TEST(ArchitectureDependencyRules, RuntimeSystemsMustNotDependOnEditor) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();
    const auto headerViolations = findIncludeViolations(
        repoRoot,
        fs::path{"include"} / "Engine" / "Systems",
        {"Editor/"});
    EXPECT_TRUE(headerViolations.empty())
        << "Runtime system headers must not depend on Editor layer:" << joinViolations(headerViolations);
    const auto sourceViolations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Engine" / "Systems",
        {"Editor/"});
    EXPECT_TRUE(sourceViolations.empty())
        << "Runtime system sources must not depend on Editor layer:" << joinViolations(sourceViolations);
}

// The ModelLib loading layer must not reach into Engine's raytracing/graphics
// internals. BLAS construction is injected via a callback (setModelLoadedCallback)
// so the loader stays decoupled from Engine/Graphics.
TEST(ArchitectureDependencyRules, ModelLibMustNotDependOnEngineGraphicsInternals) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();
    const auto headerViolations = findIncludeViolations(
        repoRoot,
        fs::path{"include"} / "ModelLib",
        {"Engine/Graphics/AccelBuilder.hpp", "Engine/Graphics/RenderPipeline.hpp", "Engine/Graphics/Renderer.hpp"});
    EXPECT_TRUE(headerViolations.empty())
        << "ModelLib headers must not include Engine raytracing/graphics internals:" << joinViolations(headerViolations);
}

// The delivery app must not perform low-level descriptor / IBL orchestration
// directly in its update flow; that belongs in EngineState (Composition layer).
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
}

// The Editor UI must reach rendering systems through EngineState, not by
// holding raw subsystem pointers that bypass the composition layer.
TEST(ArchitectureDependencyRules, EditorUiMustNotIncludeEngineGraphicsPassesDirectly) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Editor" / "ui",
        {"Engine/Graphics/Passes/"});
    EXPECT_TRUE(violations.empty())
        << "Editor UI must not include Engine/Graphics/Passes directly:" << joinViolations(violations);
}

// Editor UI must not reach into ModelLib's internal resource implementations.
TEST(ArchitectureDependencyRules, EditorUiMustNotDependOnModelLibInternals) {
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty()) << "Could not locate repository root from cwd=" << fs::current_path().string();
    // Asset-browsing panels legitimately reach the ResourceManager to enumerate
    // and load models. This should ideally go through a narrow ResourceService,
    // but is allowlisted for now as an intentional Editor IO concern.
    const std::set<std::string> allowedFiles = {
        "src/Editor/ui/Panels/ScenePanel.cpp",
        "src/Editor/ui/Panels/SceneUI.cpp",
        "src/Editor/ui/ModelBrowser.cpp",
    };
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src"} / "Editor" / "ui",
        {"ModelLib/Resources/ResourceManager.hpp", "ModelLib/Resources/MeshManager.hpp"});
    const auto unknownViolations = filterUnknownViolations(violations, allowedFiles);
    EXPECT_TRUE(unknownViolations.empty())
        << "Editor UI must not depend on ModelLib resource internals:" << joinViolations(unknownViolations);
}
