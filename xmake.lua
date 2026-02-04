-- ============================================================================
-- XMAKE BUILD SCRIPT - Vulkan Engine
-- ============================================================================

set_languages("cxx20")
add_rules("mode.debug", "mode.release")

local project_dir = os.projectdir()

-- ============================================================================
-- Helpers
-- ============================================================================

local function normpath(p)
    return p:gsub("\\", "/")
end

-- ============================================================================
-- Options
-- ============================================================================

option("deadcode")
    set_showmenu(true)
    set_default(false)
    set_description("Enable unused/dead code warnings and linker GC reporting")
option_end()

-- ============================================================================
-- Global Defines (paths, config)
-- ============================================================================

add_defines(
    "SHADER_PATH=\""  .. normpath(path.join(project_dir, "assets/shaders/compiled")) .. "/\"",
    "MODEL_PATH=\""   .. normpath(path.join(project_dir, "assets/models")) .. "/\"",
    "TEXTURE_PATH=\"" .. normpath(path.join(project_dir, "assets/textures")) .. "/\"",
    "SCENE_PATH=\""   .. normpath(path.join(project_dir, "assets/scenes")) .. "/\""
)

-- GLFW / GLM config
add_defines(
    "GLFW_INCLUDE_VULKAN",
    "GLM_FORCE_RADIANS",
    "GLM_FORCE_DEPTH_ZERO_TO_ONE",
    "GLM_ENABLE_EXPERIMENTAL"
)

if is_plat("linux") then
    add_defines("GLFW_USE_WAYLAND=1")
end

-- ============================================================================
-- Dependencies
-- ============================================================================

set_policy("package.requires_lock", true)

add_requires(
    "glfw", 
    "glm",
    "tinyobjloader",
    "tinygltf",
    "stb",
    "tinyexr",
    "nlohmann_json",
    "meshoptimizer",
    "entt",
    "gtest",
    "imgui v1.92.1-docking", { configs = { glfw = true, vulkan = true } }
)

if is_plat("linux") then
    add_requires("vulkan")
end

-- ============================================================================
-- Platform Configuration
-- ============================================================================

-- Add Vulkan SDK includes on Windows
if is_plat("windows") then
    local vulkan_sdk = os.getenv("VULKAN_SDK")
    if vulkan_sdk then
        add_includedirs(path.join(vulkan_sdk, "Include"))
        add_linkdirs(path.join(vulkan_sdk, "Lib"))
    else
        print("Warning: VULKAN_SDK environment variable not found!")
    end
end

if has_config("deadcode") then
    if is_plat("windows") then
        add_cxflags("/W4", "/Gy", "/Gw", {force = true})
        add_ldflags("/OPT:REF", "/OPT:ICF", "/VERBOSE:REF", {force = true})
    else
        add_cxflags(
            "-Wall", "-Wextra",
            "-Wunused-parameter",
            "-Wunused-variable",
            "-Wunused-function",
            "-ffunction-sections",
            "-fdata-sections",
            {force = true}
        )
        add_ldflags("-Wl,--gc-sections", "-Wl,--print-gc-sections", {force = true})
    end
end

-- ============================================================================
-- Third Party Libraries
-- ============================================================================

-- Single-target provider for STB implementations. This ensures a single
-- translation unit defines STB symbols so static archive link-order doesn't
-- cause unresolved stbi_* references.
target("stb_provider")
    set_kind("static")
    set_group("third_party")
    add_files("src/third_party/stb/stb_provider.cpp")
    add_includedirs("include", {public = true})
    add_packages("stb")

-- ============================================================================
-- Core Libraries
-- ============================================================================

target("EngineSceneIO")
    set_kind("static")
    set_group("core")
    add_files("src/EngineSceneIO/Scene/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "nlohmann_json", "entt")
    if is_plat("linux") then
        add_packages("vulkan")
    elseif is_plat("windows") then
        add_syslinks("vulkan-1")
    end

target("ModelLib")
    set_kind("static")
    set_group("core")
    add_files("src/ModelLib/**.cpp")
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "glfw", "meshoptimizer", "nlohmann_json", 
        "tinygltf", "tinyobjloader", "stb", "tinyexr", "entt"
    )
    if is_plat("linux") then
        add_packages("vulkan")
    elseif is_plat("windows") then
        add_syslinks("vulkan-1")
    end
    add_deps("EngineSceneIO", "stb_provider")

target("EngineImporters")
    set_kind("static")
    set_group("core")
    -- No source files: resource implementations were moved into ModelLib.
    -- Keep include paths and packages for any header-only helpers or future code.
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "glfw", "tinyobjloader", "tinygltf",
        "stb", "nlohmann_json", "meshoptimizer"
    )
    if is_plat("linux") then
        add_packages("vulkan")
    elseif is_plat("windows") then
        add_syslinks("vulkan-1")
    end
    add_deps("stb_provider", "ModelLib")

target("Engine")
    set_kind("static")
    set_group("core")
    add_files("src/Engine/**.cpp")
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "glfw", "tinyexr", "tinygltf",
        "stb", "nlohmann_json", "meshoptimizer",
        "imgui", "entt"
    )
    if is_plat("linux") then
        add_packages("vulkan")
    elseif is_plat("windows") then
        add_syslinks("vulkan-1")
    end
    add_deps("stb_provider", "EngineSceneIO", "EngineImporters")

-- ============================================================================
-- Editor Application
-- ============================================================================

target("Editor")
    set_kind("binary")
    set_group("editor")
    set_default(true)
    add_files("src/Editor/**.cpp")
    add_includedirs("include", "src/Editor", {public = true})
    add_packages("glm", "glfw", "imgui", "entt", "nlohmann_json", "tinygltf", "tinyexr")
    if is_plat("linux") then
        add_packages("vulkan")
    elseif is_plat("windows") then
        add_syslinks("vulkan-1")
    end
    add_deps("Engine", "EngineImporters", "EngineSceneIO", "Shaders")

-- ============================================================================
-- Tools
-- ============================================================================

target("IBLBaker")
    set_kind("binary")
    set_group("tools")
    set_default(true)
    add_files("src/tools/IBLBaker/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "nlohmann_json", "entt", "tinygltf")
    if is_plat("linux") then
        add_packages("vulkan")
    elseif is_plat("windows") then
        add_syslinks("vulkan-1")
    end
    add_deps("Engine", "EngineImporters", "EngineSceneIO")

-- ============================================================================
-- Utility Targets
-- ============================================================================

target("Shaders")
    set_kind("phony")
    set_group("utility")
    on_build(function ()
        if is_host("windows") then
            os.exec("powershell -ExecutionPolicy Bypass -File " .. project_dir .. "/compile_shaders.ps1")
        else
            os.exec("bash " .. project_dir .. "/compile_shaders.sh")
        end
    end)

target("Coverage")
    set_kind("phony")
    set_group("utility")
    add_deps("Tests")
    on_build(function ()
        if is_host("windows") then
            -- Pass -SkipBuild since we already built Tests via add_deps
            local script = path.join(project_dir, "run_coverage.ps1")
            os.execv("powershell", {"-ExecutionPolicy", "Bypass", "-File", script, "-SkipBuild"})
        else
            print("Coverage is only supported on Windows with OpenCppCoverage")
        end
    end)

-- ============================================================================
-- Tests
-- ============================================================================

target("Tests")
    set_kind("binary")
    set_group("tests")
    add_files("tests/**.cpp")
    add_packages("gtest", "glm", "glfw", "nlohmann_json", "entt", "tinyexr", "tinygltf", "tinyobjloader")
    if is_plat("linux") then
        add_packages("vulkan")
    elseif is_plat("windows") then
        add_syslinks("vulkan-1")
    end
    add_deps("stb_provider", "Engine", "EngineSceneIO", "ModelLib")
    add_syslinks("gtest_main")

    -- Copy test assets to the build output directory
    after_build(function (target)
        local targetdir = target:targetdir()
        local projectdir = os.projectdir()
        
        -- glTF test models
        local gltf_models = {"Triangle", "Box", "Cube", "Duck", "Avocado", "BoxTextured"}
        for _, model in ipairs(gltf_models) do
            local src = path.join(projectdir, "assets/models/glTF", model)
            local dst = path.join(targetdir, "assets/models/glTF", model)
            if os.isdir(src) then
                os.mkdir(path.join(targetdir, "assets/models/glTF"))
                os.cp(src, dst)
            end
        end
    end)
