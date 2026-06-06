#include "Editor/Infrastructure/AnimationAccessAdapter.hpp"

namespace engine {

    AnimationAccessAdapter::AnimationAccessAdapter(AnimationSystem* animationSystem)
        : animationSystem_(animationSystem) {}

    AnimationSystem* AnimationAccessAdapter::getAnimationSystem() {
        return animationSystem_;
    }

}  // namespace engine
