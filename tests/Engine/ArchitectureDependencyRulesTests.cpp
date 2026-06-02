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
