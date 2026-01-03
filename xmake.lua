--- ===========================================================================
-- XMAKE BUILD SCRIPT - Cross-platform Vulkan Engine
-- ===========================================================================
set_languages("cxx20")
add_rules("mode.debug", "mode.release")
local project_dir = os.projectdir()

-- Convert backslashes to forward slashes for C++ string compatibility
local shader_path = path.join(project_dir, "assets/shaders/compiled//"):gsub("\\", "/") .."/"
local texture_path = path.join(project_dir, "assets/textures//"):gsub("\\", "/") .."/"
local model_path = path.join(project_dir, "assets/models//"):gsub("\\", "/") .."/"
local profile_path = path.join(project_dir, "profile/"):gsub("\\", "/") .."/"

-- Platform-specific defines
if is_plat("linux") then
    add_defines("GLFW_USE_WAYLAND=1")
end
add_defines("GLFW_INCLUDE_VULKAN")

-- Use Vulkan SDK loader on Windows (do not define VK_NO_PROTOTYPES)

task("tidy_changed")
    set_menu {
        usage = "xmake tidy_changed [options]",
        description = "Run clang-tidy on files changed vs a base ref",
        options = {
            {"b", "base", "kv", "HEAD~1", "Base git ref to diff against (default: HEAD~1; use main for PRs)"},
            {"j", "jobs", "kv", "1", "Parallel jobs (PowerShell 7+ on Windows)"}
        }
    }
    on_run(function ()
        local option = import("core.base.option")
        local base = option.get("base") or "HEAD~1"
        local jobs = option.get("jobs") or "1"

        if is_host("windows") then
            os.execv("powershell", {
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-File", path.join(os.projectdir(), "scripts", "run_clang_tidy_changed.ps1"),
                "-Base", base,
                "-Jobs", jobs
            })
        else
            os.execv("bash", {path.join(os.projectdir(), "scripts", "run_clang_tidy_changed.sh"), base})
        end
    end)

task_end()

-- Package dependencies - use Vulkan SDK on Windows
if is_plat("windows") then
    add_requires("glfw")
    add_requires("glm")
    add_requires("vulkan-headers")
    add_requires("tinyobjloader")
    add_requires("tinygltf")
    add_requires("stb")
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
    add_requires("glfw")
    add_requires("glm")
    add_requires("vulkan")
    add_requires("tinyobjloader")
    add_requires("tinygltf")
    add_requires("stb")
    add_requires("nlohmann_json")
    add_requires("meshoptimizer")
    add_requires("entt")
    add_requires("imgui v1.92.1-docking", {configs = {glfw = true, vulkan = true}})
end

target("Cube")
    set_kind("binary")
    add_files("src/demos/Cube/**.cpp")
    add_includedirs("include", {public = true})
    add_includedirs("src/demos/Cube")
    add_includedirs("src/demos/Cube/ui")
    add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
    add_defines("MODEL_PATH=\"" .. model_path .. "\"")
    add_defines("TEXTURE_PATH=\"" .. texture_path .. "\"")
    if is_mode("debug") then
        add_defines("ENABLE_PROFILING")
        add_defines("PROFILE_OUTPUT_DIR=\"" .. profile_path .. "\"")
    end
    if is_plat("windows") then
        add_packages("glfw", "glm", "vulkan-headers", "imgui", "entt", "nlohmann_json", "tinygltf")
        -- Link the system Vulkan loader on Windows for the final executable
        add_syslinks("vulkan-1")
    else
        add_packages("glfw", "glm", "vulkan", "imgui", "entt", "nlohmann_json", "tinygltf")
    end
    add_deps("Engine")
    
target("Engine")
    set_kind("static")
    add_files("src/Engine/**.cpp")
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
    add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
    add_defines("MODEL_PATH=\"" .. model_path .. "\"")
    add_defines("TEXTURE_PATH=\"" .. texture_path .. "\"")

before_build(function (target)
    -- Format code (platform-independent)
    -- if is_host("windows") then
    --     os.exec("powershell -ExecutionPolicy Bypass -File " .. os.projectdir() .. "/format_code.ps1")
    -- else
    --     os.exec("bash " .. os.projectdir() .. "/format_code.sh")
    -- end
    
    -- Compile shaders (platform-independent)
    if is_host("windows") then
        os.exec("powershell -ExecutionPolicy Bypass -File " .. os.projectdir() .. "/compile_shaders.ps1")
    else
        os.exec("bash " .. os.projectdir() .. "/compile_shaders.sh")
    end

end)


