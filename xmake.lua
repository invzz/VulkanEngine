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
    "TOOL_PATH=\""    .. normpath(path.join(project_dir, "tools")) .. "/\"",

    "EXR2VTEX_PATH=\""     .. tool_path("EXR2VTEX")     .. "\"",
    "UVUNWRAP_CLI_PATH=\"" .. tool_path("UVUnwrapCLI")  .. "\""
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

-- ============================================================================
-- Core Libraries
-- ============================================================================

target("EngineSceneIO")
    set_kind("static")
    add_files("src/EngineSceneIO/Scene/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "nlohmann_json", "entt")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")

target("EngineImporters")
    set_kind("static")
    add_files("src/EngineImporters/**.cpp")
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "glfw", "tinyobjloader", "tinygltf",
        "stb", "nlohmann_json", "meshoptimizer"
    )
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")

target("Engine")
    set_kind("static")
    add_files("src/Engine/**.cpp")
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

    if is_plat("windows") then
        add_syslinks("vulkan-1")
    end

-- ============================================================================
-- UV Unwrap
-- ============================================================================

target("UVUnwrap")
    set_kind("static")
    add_files("src/tools/UVUnwrap/**.cpp")
    add_includedirs("include", "include/Tools/UVUnwrap", {public = true})
    add_deps("xatlas")

target("UVUnwrapCLI")
    set_kind("binary")
    add_files("src/tools/UVUnwrapCLI/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("nlohmann_json")
    add_deps("UVUnwrap")
    copy_to_tools(target)

-- ============================================================================
-- Offline Tools
-- ============================================================================

target("IBLBaker")
    set_kind("binary")
    add_files("src/tools/IBLBaker/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "nlohmann_json", "entt", "tinygltf")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("Engine", "EngineSceneIO", "EngineImporters")
    copy_to_tools(target)

target("EXR2VTEX")
    set_kind("binary")
    add_files("src/tools/EXR2VTEX/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "tinyexr")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("Engine", "EngineSceneIO", "EngineImporters")
    copy_to_tools(target)

target("ModelLightBaker")
    set_kind("binary")
    add_files("src/tools/ModelLightBaker/**.cpp")
    add_includedirs("include", {public = true})
    add_packages(
        "glm", "glfw", "entt", "nlohmann_json",
        "tinygltf", "stb", "tinyexr"
    )
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("Engine", "EngineSceneIO", "EngineImporters")
    copy_to_tools(target)

-- ============================================================================
-- Demo / UI
-- ============================================================================

target("CubeUI")
    set_kind("static")
    add_files("src/demos/CubeUI/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glm", "glfw", "imgui", "entt", "nlohmann_json")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("Engine")

target("Cube")
    set_kind("binary")
    set_default(true)
    add_files("src/demos/Cube/**.cpp")
    add_includedirs("include", "src/demos/Cube", {public = true})
    add_packages("glm", "glfw", "imgui", "entt", "nlohmann_json", "tinygltf", "tinyexr")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_deps("CubeUI", "Engine", "EngineSceneIO", "EngineImporters")

    if is_plat("windows") then
        add_syslinks("vulkan-1")
    end

-- ============================================================================
-- Tests
-- ============================================================================

target("LightmapTests")
    set_kind("binary")
    add_files("tests/**.cpp")
    add_packages("gtest", "glm", "nlohmann_json", "entt", "tinyexr")
    add_packages(is_plat("windows") and "vulkan-headers" or "vulkan")
    add_links("gtest_main")
    add_deps("Engine", "EngineSceneIO", "EngineImporters", "UVUnwrap", "xatlas")

-- ============================================================================
-- Utilities
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
