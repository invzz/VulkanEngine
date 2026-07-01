#ifndef VULKANENGINE_INCLUDE_ENGINE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_HPP

// Engine.hpp — Public API surface
// Consumers that use the engine (Editor, tools) should include this
// single header to get the core engine interface types.

#pragma once

// Core engine state
#include "Engine/EditorState.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/SystemRegistry.hpp"
#include "Engine/graphics/GraphicsState.hpp"

// Rendering
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/RenderPipeline.hpp"
#include "Engine/Graphics/RenderTarget.hpp"
#include "Engine/Graphics/Renderer.hpp"

// Scene
#include "Engine/Scene/Camera.hpp"
#include "Engine/Scene/Scene.hpp"

// Core
#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"

#endif  // VULKANENGINE_INCLUDE_ENGINE_HPP
