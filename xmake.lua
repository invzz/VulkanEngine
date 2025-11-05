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
    add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
    add_packages("glfw", "glm", "vulkan")
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
    add_defines("SHADER_PATH=\"" .. shader_path .. "\"")
    add_packages("glfw", "glm", "vulkan")

before_build(function (target)
    os.exec("bash " .. os.projectdir() .. "/format_code.sh")
    os.exec("bash " .. os.projectdir() .. "/compile_shaders.sh")

end)

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

