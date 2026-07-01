#ifndef VULKANENGINE_INCLUDE_PCH_HPP
#define VULKANENGINE_INCLUDE_PCH_HPP

// Precompiled header for frequently used heavy dependencies.
// Reduces compile time when included in many translation units.
// Add to xmake.lua: add_pchhdr("pch.hpp") and add_pchsource("src/pch.cpp")

#pragma once

// C/C++ standard library
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Vulkan
#include <vulkan/vulkan.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ImGui + ImGuizmo
#include <imgui.h>

#include <ImGuizmo.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

// entt
#include <entt/entt.hpp>

// Jolt Physics
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

// tinygltf
#include <tiny_gltf.h>

// nlohmann JSON
#include <nlohmann/json.hpp>

// meshoptimizer
#include <meshoptimizer/meshopt.h>

// stb
#include <stb_image.h>

#endif  // VULKANENGINE_INCLUDE_PCH_HPP
