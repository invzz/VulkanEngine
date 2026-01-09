--- ===========================================================================
-- XMAKE BUILD SCRIPT - Cross-platform Vulkan Engine
-- ===========================================================================
set_languages("cxx20")
add_rules("mode.debug", "mode.release")
local project_dir = os.projectdir()

-- Enable extra diagnostics for unused/dead code.
-- Notes:
-- - Compilers can warn about unused locals/params and unreferenced static functions.
-- - The linker can eliminate unreferenced functions/data and (optionally) print what was removed.
option("deadcode")
    set_showmenu(true)
    set_default(false)
    set_description("Enable unused/dead code warnings and link-time dead stripping reporting")
option_end()

-- Convert backslashes to forward slashes for C++ string compatibility
local shader_path = path.join(project_dir, "assets/shaders/compiled//"):gsub("\\", "/") .."/"
local texture_path = path.join(project_dir, "assets/textures//"):gsub("\\", "/") .."/"
local model_path = path.join(project_dir, "assets/models//"):gsub("\\", "/") .."/"
local profile_path = path.join(project_dir, "profile/"):gsub("\\", "/") .."/"
local tool_path = path.join(project_dir, "tools/"):gsub("\\", "/") .."/"
if is_plat("windows") then
    local exr2vtex_path = path.join(project_dir, "tools/EXR2VTEX")
    add_defines("EXR2VTEX_PATH= \"" .. exr2vtex_path .. "\"")
else
    local exr2vtex_path = path.join(project_dir, "tools/EXR2VTEX")
    add_defines("EXR2VTEX_PATH= \"" .. exr2vtex_path .. "\"")
end
-- Asset paths (make available to all targets)
add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
add_defines("MODEL_PATH=\"" .. model_path .. "\"")
add_defines("TEXTURE_PATH=\"" .. texture_path .. "\"")
add_defines("TOOL_PATH=\"" .. tool_path .. "\"")
            
-- Platform-specific defines
if is_plat("linux") then
    add_defines("GLFW_USE_WAYLAND=1")
end
add_defines("GLFW_INCLUDE_VULKAN")

-- GLM configuration (keep consistent across all translation units)
add_defines("GLM_FORCE_RADIANS")
add_defines("GLM_FORCE_DEPTH_ZERO_TO_ONE")
add_defines("GLM_ENABLE_EXPERIMENTAL")

-- Use Vulkan SDK loader on Windows (do not define VK_NO_PROTOTYPES)
-- Package dependencies - use Vulkan SDK on Windows
if is_plat("windows") then
    add_requires("gtest")
    add_requires("glfw")
    add_requires("glm")
    add_requires("vulkan-headers")
    add_requires("tinyobjloader")
    add_requires("tinygltf")
    add_requires("stb")
    add_requires("tinyexr")
    add_requires("nlohmann_json")
    add_requires("meshoptimizer")
    add_requires("entt")
    add_requires("imgui v1.92.1-docking", {configs = {glfw = true, vulkan = true}})

    -- Vulkan SDK paths for Windows (require VULKAN_SDK env var)
    local vulkan_sdk = os.getenv("VULKAN_SDK")
    if not vulkan_sdk then
        raise("VULKAN_SDK is not set. Install the Vulkan SDK and set VULKAN_SDK (or run setup_windows.ps1).")
    end

    local vulkan_include = path.join(vulkan_sdk, "Include")
    local vulkan_lib = path.join(vulkan_sdk, "Lib")

    if not os.isdir(vulkan_include) then
        raise("VULKAN_SDK Include directory not found: " .. vulkan_include)
    end
    if not os.isdir(vulkan_lib) then
        raise("VULKAN_SDK Lib directory not found: " .. vulkan_lib)
    end

    -- Treat SDK headers as system headers to reduce warnings/tidy noise.
    add_sysincludedirs(vulkan_include, {public = true})
    add_linkdirs(vulkan_lib, {public = true})
else
    add_requires("gtest")
    add_requires("glfw")
    add_requires("glm")
    add_requires("vulkan")
    add_requires("tinyobjloader")
    add_requires("tinygltf")
    add_requires("stb")
    add_requires("tinyexr")
    add_requires("nlohmann_json")
    add_requires("meshoptimizer")
    add_requires("entt")
    add_requires("imgui v1.92.1-docking", {configs = {glfw = true, vulkan = true}})
end

target("Cube")
    set_kind("binary")
    add_files("src/demos/Cube/**.cpp")
    set_default(true)
    add_includedirs("include", {public = true})
    add_includedirs("src/demos/Cube")
    -- Cube includes UI headers via "CubeUI/ui/..." from include/.
    if is_mode("debug") then
        add_defines("ENABLE_PROFILING")
        add_defines("PROFILE_OUTPUT_DIR=\"" .. profile_path .. "\"")
    end
    if is_plat("windows") then
        add_packages("glfw", "glm", "vulkan-headers", "imgui", "entt", "nlohmann_json", "tinygltf", "tinyexr")
        -- Link the system Vulkan loader on Windows for the final executable
        add_syslinks("vulkan-1")
    else
        add_packages("glfw", "glm", "vulkan", "imgui", "entt", "nlohmann_json", "tinygltf")
    end
    -- Ensure libraries that depend on importers are listed first so linker ordering resolves symbols
    add_deps("CubeUI")
    add_deps("Engine")
    add_deps("EngineImporters")

    -- xatlas third-party static target (submodule at third_party/xatlas)
    target("xatlas")
        set_kind("static")
        add_files("third_party/xatlas/source/xatlas/xatlas.cpp")
        add_includedirs("third_party/xatlas/source/xatlas", {public = true})

    -- UV unwrap library (API only - no image allocation or baking here)
    target("UVUnwrap")
        set_kind("static")
        add_files("src/tools/UVUnwrap/**.cpp")
        add_includedirs("include", {public = true})
        add_includedirs("include/Tools/UVUnwrap", {public = true})
        add_deps("xatlas")

    add_deps("EngineSceneIO")

    if has_config("deadcode") then
        if is_plat("windows") then
            -- MSVC/clang-cl flags (via link.exe)
            add_cxflags("/W4", "/w44061", "/w44100", "/w44189", "/w44505", "/Gy", "/Gw", {force = true})
            add_ldflags("/OPT:REF", "/OPT:ICF", "/VERBOSE:REF", {force = true})
        else
            add_cxflags("-Wall", "-Wextra", "-Wunused-parameter", "-Wunused-variable", "-Wunused-function", "-Wunreachable-code", "-ffunction-sections", "-fdata-sections", {force = true})
            add_ldflags("-Wl,--gc-sections", "-Wl,--print-gc-sections", {force = true})
        end
    end
    target("IBLBaker")
        set_kind("binary")
        add_files("src/tools/IBLBaker/**.cpp")
        add_includedirs("include", {public = true})
        if is_plat("windows") then
            add_packages("glfw", "glm", "vulkan-headers", "entt", "nlohmann_json", "tinygltf")
            add_syslinks("vulkan-1")
        else
            add_packages("glfw", "glm", "vulkan", "entt", "nlohmann_json", "tinygltf")
        end
        -- Linker group flags allow circular static archive resolution (Engine <-> EngineSceneIO)
        add_ldflags("-Wl,--start-group", "-Wl,--end-group")
        add_deps("Engine")

        -- After build: copy tool binary to tools/ for easy discovery
        on_build(function (target)
            local bin = target:targetfile()
            if bin and os.isfile(bin) then
                local outDir = path.join(project_dir, "tools")
                os.mkdir(outDir)
                local dest = path.join(outDir, path.filename(bin))
                os.cp(bin, dest)
                print("Copied tool: " .. dest)
            end
        end)

    target("EXR2VTEX")
        set_kind("binary")
        add_files("src/tools/EXR2VTEX/**.cpp")
        add_includedirs("include", {public = true})
        if is_plat("windows") then
            add_packages("glfw", "glm", "vulkan-headers", "tinyexr")
            add_syslinks("vulkan-1")
        else
            add_packages("glfw", "glm", "vulkan", "tinyexr")
        end
        add_deps("Engine")

        -- After build: copy tool binary to tools/ for easy discovery
        on_build(function (target)
            local bin = target:targetfile()
            if bin and os.isfile(bin) then
                local outDir = path.join(project_dir, "tools")
                os.mkdir(outDir)
                local dest = path.join(outDir, path.filename(bin))
                os.cp(bin, dest)
                print("Copied tool: " .. dest)
            end
        end)

    -- ModelLightBaker: offline baker for per-model directional lightmaps
    target("ModelLightBaker")
        set_kind("binary")
        add_files("src/tools/ModelLightBaker/**.cpp")
        add_includedirs("include", {public = true})
        if is_plat("windows") then
            add_packages("glfw", "glm", "vulkan-headers", "entt", "nlohmann_json", "tinygltf", "stb", "tinyexr")
            add_syslinks("vulkan-1")
        else
            add_packages("glfw", "glm", "vulkan", "entt", "nlohmann_json", "tinygltf", "stb", "tinyexr")
        end
        add_deps("EngineImporters", "Engine")

        -- After build: copy tool binary to tools/ for easy discovery
        on_build(function (target)
            local bin = target:targetfile()
            if bin and os.isfile(bin) then
                local outDir = path.join(project_dir, "tools")
                os.mkdir(outDir)
                local dest = path.join(outDir, path.filename(bin))
                os.cp(bin, dest)
                print("Copied tool: " .. dest)
            end
        end)
    
target("CubeUI")
    set_kind("static")
    add_files("src/demos/CubeUI/ui/**.cpp")
    add_includedirs("include", {public = true})
    if is_plat("windows") then
        add_packages("glfw", "glm", "vulkan-headers", "imgui", "entt", "nlohmann_json")
    else
        add_packages("glfw", "glm", "vulkan", "imgui", "entt", "nlohmann_json")
    end
    add_deps("Engine")

target("Engine")
    set_kind("static")
    add_files("src/Engine/**.cpp")
    -- Keep runtime/core in Engine. Optional modules are split into separate targets.
    add_includedirs("include", {public = true})
    if is_mode("debug") then
        add_defines("ENABLE_PROFILING")
        add_defines("PROFILE_OUTPUT_DIR=\"" .. profile_path .. "\"")
    end
    if is_plat("windows") then
        add_packages("glfw", "glm", "vulkan-headers", "tinyexr")
        -- Link the system Vulkan loader on Windows
        add_syslinks("vulkan-1")
    else
        add_packages("glfw", "glm", "vulkan-headers", "tinyexr")
    end
    add_packages("tinyobjloader")
    add_packages("tinygltf")
    add_packages("stb")
    add_packages("nlohmann_json")
    add_packages("meshoptimizer")
    add_packages("imgui")
    add_packages("entt")

    -- Ensure Engine links against EngineSceneIO so symbols from Scene IO (e.g. parseSceneLightmapManifest) are available when linking executables
    add_deps("EngineSceneIO")

    if has_config("deadcode") then
        if is_plat("windows") then
            add_cxflags("/W4", "/w44061", "/w44100", "/w44189", "/w44505", "/Gy", "/Gw", {force = true})
        else
            add_cxflags("-Wall", "-Wextra", "-Wunused-parameter", "-Wunused-variable", "-Wunused-function", "-Wunreachable-code", "-ffunction-sections", "-fdata-sections", {force = true})
        end
    end

-- Asset importers (glTF/OBJ). Split from Engine for cleaner boundaries.
target("EngineImporters")
    set_kind("static")
    add_files("src/EngineImporters/Resources/importers/**.cpp")
    add_includedirs("include", {public = true})
    -- Importers include core engine headers (e.g. Window.hpp) that require these packages.
    if is_plat("windows") then
        add_packages("glfw", "glm", "vulkan-headers")
    else
        add_packages("glfw", "glm", "vulkan-headers")
    end
    add_packages("tinyobjloader")
    add_packages("tinygltf")
    add_packages("stb")
    add_packages("nlohmann_json")
    add_packages("meshoptimizer")
    -- Note: EngineImporters are implementation detail of Engine; Engine should depend on them so the final link order places EngineImporters after Engine.

target("Engine")
    set_kind("static")
    add_files("src/Engine/**.cpp")
    -- Keep runtime/core in Engine. Optional modules are split into separate targets.
    add_includedirs("include", {public = true})
    if is_mode("debug") then
        add_defines("ENABLE_PROFILING")
        add_defines("PROFILE_OUTPUT_DIR=\"" .. profile_path .. "\"")
    end
    if is_plat("windows") then
        add_packages("glfw", "glm", "vulkan-headers")
        -- Link the system Vulkan loader on Windows
        add_syslinks("vulkan-1")
    else
        add_packages("glfw", "glm", "vulkan-headers")
    end
    add_packages("tinyobjloader")
    add_packages("tinygltf")
    add_packages("stb")
    add_packages("nlohmann_json")
    add_packages("meshoptimizer")
    add_packages("imgui")
    add_packages("entt")

    -- Ensure Engine links against EngineImporters so the importers are available at link time
    add_deps("EngineImporters")
    -- SceneSerializer is part of the runtime Engine so compile it into Engine to avoid circular static dependencies
    add_files("src/EngineSceneIO/Scene/SceneSerializer.cpp")

-- Scene serialization/deserialization (json). Split from Engine for cleaner boundaries.
target("EngineSceneIO")
    set_kind("static")
    add_files("src/EngineSceneIO/Scene/LightmapManifest.cpp")
    add_includedirs("include", {public = true})
    -- Scene IO includes some core Engine headers that pull in Window.hpp.
    add_packages("glfw", "glm", "vulkan-headers")
    add_packages("nlohmann_json")
    add_packages("entt")

-- Unit tests for scene lightmap manifest parsing
target("LightmapTests")
    set_kind("binary")
    add_files("tests/*.cpp")
    add_packages("gtest", "nlohmann_json", "entt", "glm", "glfw", "vulkan", "tinyexr")
    add_links("gtest_main")
    add_deps("EngineSceneIO", "Engine")
    -- Depend on xatlas so xatlas-based tests/linking succeed
    add_deps("xatlas")
    -- Depend on the UVUnwrap library (stub) so tests can link against the API
    add_deps("UVUnwrap")

-- Utility target to (re)compile shaders on demand.
target("Shaders")
    set_kind("phony")
    set_group("utility")
    on_build(function (target)
        -- Compile shaders (platform-independent)
        if is_host("windows") then
            os.exec("powershell -ExecutionPolicy Bypass -File " .. os.projectdir() .. "/compile_shaders.ps1")
        else
            os.exec("bash " .. os.projectdir() .. "/compile_shaders.sh")
        end
    end)


