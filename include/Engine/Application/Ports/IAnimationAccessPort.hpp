#pragma once

namespace engine {

class AnimationSystem;

// Port for delivery to access the animation system without depending on EngineState.
class IAnimationAccessPort {
 public:
  virtual ~IAnimationAccessPort() = default;

  [[nodiscard]] virtual AnimationSystem* getAnimationSystem() = 0;
};

}  // namespace engine
