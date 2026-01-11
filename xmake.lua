-- ============================================================================
-- XMAKE BUILD SCRIPT - Vulkan Engine + Offline Tools
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

local function tool_exe(name)
    return is_plat("windows") and (name .. ".exe") or name
end

local function tool_path(name)
    return normpath(path.join(project_dir, "tools", tool_exe(name)))
end

local function copy_to_tools(target)
    on_build(function (t)
        local bin = t:targetfile()
        if bin and os.isfile(bin) then
            local outdir = path.join(project_dir, "tools")
            os.mkdir(outdir)
            local dst = path.join(outdir, path.filename(bin))
            os.cp(bin, dst)
            print("Copied tool -> " .. normpath(dst))
        end
    end)
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
    "LIGHTMAP_PATH=\"" .. normpath(path.join(project_dir, "assets/lightmaps")) .. "/\"", 
    "SCENE_PATH=\"" .. normpath(path.join(project_dir, "assets/scenes")) .. "/\"",
    "TOOL_PATH=\""    .. normpath(path.join(project_dir, "tools")) .. "/\"",
    "EXR2VTEX_PATH=\""     .. tool_path("EXR2VTEX")     .. "\"",
    "LIGHT_BAKER_PATH=\"" .. tool_path("LightBaker") .. "\"",
    "COMPRESSONATOR_CLI=\"" .. normpath(path.join(project_dir, "tools", "Compressonator", "compressonatorcli")) .. "\""
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

if is_plat("windows") then
    add_requires("vulkan-headers")
else
    add_requires("vulkan")
end

-- ============================================================================
-- Common Flags
-- ============================================================================
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
-- Third Party
-- ============================================================================
target("xatlas")
    set_kind("static")
    add_files("third_party/xatlas/source/xatlas/xatlas.cpp")
    add_includedirs("third_party/xatlas/source/xatlas", {public = true})

-- Single-target provider for STB implementations. This ensures a single
-- translation unit defines STB symbols so static archive link-order doesn't
-- cause unresolved stbi_* references.
target("stb_provider")
    set_kind("static")
    add_files("src/third_party/stb/stb_provider.cpp")
    add_includedirs("include", {public = true})
    add_packages("stb")

-- ============================================================================
-- Tests
-- ============================================================================
target("Tests")
    set_group("tests")
    
    set_kind("binary")
    add_files("tests/**.cpp")
    add_packages("gtest")
    add_packages( "glm", "nlohmann_json", "entt", "tinyexr")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("stb_provider")
    add_deps("Engine", "EngineImporters", "EngineSceneIO", "UVUnwrap", "xatlas", "LightmapBakerLib" )
     -- link main gtest function:
    add_links("gtest_main")

-- ============================================================================
-- Demo / UI
-- ============================================================================
target("Cube")
    set_group("demos")
    set_kind("binary")
    
    set_default(true)
    add_files("src/demos/Cube/**.cpp")
    add_files("src/EngineSceneIO/Scene/SceneSerializer.cpp")
    add_includedirs("include", "src/demos/Cube", {public = true})
    add_packages("glm", "glfw", "imgui", "entt", "nlohmann_json", "tinygltf", "tinyexr")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("Engine", "CubeUI", "EngineImporters")

    if is_plat("windows") then
        add_syslinks("vulkan-1")
    end
-- ============================================================================
-- Core Libraries
-- ============================================================================

target("UVUnwrap")
    set_kind("static")
    add_files("src/tools/UVUnwrap/**.cpp")
    add_includedirs("include", "include/Tools/UVUnwrap", {public = true})
    add_deps("xatlas")

target("CubeUI")
    set_kind("static")
    add_files("src/demos/CubeUI/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "imgui", "entt", "nlohmann_json")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("Engine")

target("EngineSceneIO")
    set_kind("static")
    add_files("src/EngineSceneIO/Scene/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "nlohmann_json", "entt")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")

target("ModelLib")
    set_kind("static")
    set_group("core")
    add_files("src/ModelLib/**.cpp")
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "meshoptimizer", "nlohmann_json", "tinygltf", "tinyobjloader", "stb", "tinyexr", "entt"
    )
    -- ModelLib depends on EngineSceneIO for scene manifest parsing
    add_deps("EngineSceneIO")
    -- Ensure STB implementation is linked after ModelLib to resolve stbi_* symbols
    add_deps("stb_provider")


target("EngineImporters")
    set_kind("static")
    -- No source files: resource implementations were moved into `ModelLib`.
    -- Keep include paths and packages for any header-only helpers or future code.
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "glfw", "tinyobjloader", "tinygltf",
        "stb", "nlohmann_json", "meshoptimizer"
    )
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("stb_provider", "ModelLib")

target("Engine")
    set_kind("static")
    -- Exclude resource implementations moved to ModelLib
    add_files("src/Engine/**.cpp")
    -- resource implementations moved into ModelLib (see ModelLib target)
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "glfw", "tinyexr", "tinygltf",
        "stb", "nlohmann_json", "meshoptimizer",
        "imgui", "entt"
    )
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")

    if is_mode("debug") then
        add_defines(
            "ENABLE_PROFILING",
            "PROFILE_OUTPUT_DIR=\"" .. normpath(path.join(project_dir, "profile")) .. "/\""
        )
    end
    add_deps("stb_provider", "EngineSceneIO", "EngineImporters")

-- ============================================================================
-- Offline Tools
-- ============================================================================

target("IBLBaker")
    set_default(true )
    set_kind("binary")
    add_files("src/tools/IBLBaker/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "nlohmann_json", "entt", "tinygltf")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("Engine", "EngineImporters", "EngineSceneIO")


target("LightBaker")
    set_default(true)
    set_targetdir("tools")
    set_kind("binary")
    add_files("src/tools/LightBaker/**.cpp")
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "glfw", "entt", "nlohmann_json",
        "tinygltf", "stb", "tinyexr"
    )
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("stb_provider", "EngineImporters", "Engine", "EngineSceneIO", "LightmapBakerLib", "UVUnwrap")

-- ----------------------------------------------------------------------------
-- New: Lightmap baking library (stage 1: scene ingestion)
-- ----------------------------------------------------------------------------
target("LightmapBakerLib")
    set_kind("static")
    add_files("src/tools/LightmapBakerLib/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "nlohmann_json", "entt")
    -- LightmapBakerLib may pack EXR -> VTEX using Vulkan via Engine helpers
    add_deps("EngineSceneIO", "Engine", "UVUnwrap")
    
   
-- ============================================================================
-- Utilities
-- ============================================================================
target("tools")
    set_kind("phony")
    
    add_deps(
        "LightBaker"
    )


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
