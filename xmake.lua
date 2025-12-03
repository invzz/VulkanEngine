--- ===========================================================================
-- XMAKE BUILD SCRIPT
-- ===========================================================================
add_rules("mode.debug", "mode.release")
local project_dir = os.projectdir()
local shader_path = path.join(project_dir, "assets/shaders/compiled//")
local texture_path = path.join(project_dir, "assets/textures//")
local model_path = path.join(project_dir, "assets/models//")

add_defines("GLFW_USE_WAYLAND=1")
add_defines("GLFW_INCLUDE_VULKAN")
add_requires("glfw")
add_requires("glm")
add_requires("vulkan")
add_requires("tinyobjloader")
add_requires("tinygltf")
add_requires("stb")
add_requires("nlohmann_json")
add_requires("imgui v1.92.1-docking", {configs = {glfw = true, vulkan = true}})

target("ThreeBodiesSimulation")
    set_kind("binary")
    add_files("src/demos/ThreeBodiesSimulation/**.cpp")
    add_includedirs("src/demos/ThreeBodiesSimulation")
    add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
    add_packages("glfw", "glm", "vulkan")
    add_deps("2dEngine")

target("Cube")
    set_kind("binary")
    add_files("src/demos/Cube/**.cpp")
    add_includedirs("src/demos/Cube")
    add_includedirs("src/demos/Cube/ui")
    add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
    add_defines("MODEL_PATH=\"" .. model_path .. "\"")
    add_defines("TEXTURE_PATH=\"" .. texture_path .. "\"")
    add_packages("glfw", "glm", "vulkan", "imgui")
    add_deps("3dEngine")
    
-- static libraries
target("2dEngine")
    set_kind("static")
    add_files("src/2dEngine/**.cpp")
    add_includedirs("include", {public = true})
    add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
    
    add_packages("glfw", "glm", "vulkan")

target("3dEngine")
    set_kind("static")
    add_files("src/3dEngine/**.cpp")
    add_includedirs("include", {public = true})
    add_packages("glfw", "glm", "vulkan")
    add_packages("tinyobjloader")
    add_packages("tinygltf")
    add_packages("stb")
    add_packages("nlohmann_json")
    add_packages("imgui")
    add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
    add_defines("MODEL_PATH=\"" .. model_path .. "\"")
    add_defines("TEXTURE_PATH=\"" .. texture_path .. "\"")

before_build(function (target)
    os.exec("bash " .. os.projectdir() .. "/format_code.sh")
    os.exec("bash " .. os.projectdir() .. "/compile_shaders.sh")

end)


