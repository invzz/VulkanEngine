#pragma once

#include "Engine/State/StateViews.hpp"

namespace engine {

class EngineState;

class RenderingStateService {
 public:
  explicit RenderingStateService(EngineState& engineState);
  [[nodiscard]] RenderingStateView view() const;

 private:
  EngineState& engineState_;
};

class SceneRuntimeService {
 public:
  explicit SceneRuntimeService(EngineState& engineState);
  [[nodiscard]] SceneRuntimeStateView view() const;

 private:
  EngineState& engineState_;
};

class InputStateService {
 public:
  explicit InputStateService(EngineState& engineState);
  [[nodiscard]] InputStateView view() const;

 private:
  EngineState& engineState_;
};

class ResourceStateService {
 public:
  explicit ResourceStateService(EngineState& engineState);
  [[nodiscard]] ResourceStateView view() const;

 private:
  EngineState& engineState_;
};

class AnimationRuntimeService {
 public:
  explicit AnimationRuntimeService(EngineState& engineState);
  [[nodiscard]] AnimationSystem* animation() const;

 private:
  EngineState& engineState_;
};

class PhysicsRuntimeService {
 public:
  explicit PhysicsRuntimeService(EngineState& engineState);
  [[nodiscard]] JoltPhysicsSystem* joltPhysics() const;
  [[nodiscard]] PhysicsSystem* physics() const;

 private:
  EngineState& engineState_;
};

}  // namespace engine