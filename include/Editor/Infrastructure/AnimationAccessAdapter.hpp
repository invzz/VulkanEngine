#pragma once

#include "Engine/Application/Ports/IAnimationAccessPort.hpp"

namespace engine {

class AnimationSystem;

// Adapter that bridges animation runtime service to the animation access port.
class AnimationAccessAdapter final : public IAnimationAccessPort {
 public:
  explicit AnimationAccessAdapter(AnimationSystem* animationSystem);

  [[nodiscard]] AnimationSystem* getAnimationSystem() override;

 private:
  AnimationSystem* animationSystem_ = nullptr;
};

}  // namespace engine
