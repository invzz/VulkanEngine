#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/MorphTargetManager.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"

namespace engine {

  /**
   * @brief Central system for all animation types
   *
   * Manages skeletal animations, morph target animations, and procedural animations.
   * Only updates objects that have been explicitly registered (no per-frame scanning).
   *
   * Usage:
   *   1. Call registerAnimatedObject() when adding a GameObject with animations
   *   2. Call unregisterAnimatedObject() when removing it
   *   3. update() will:
   *      - Update all AnimationControllers (interpolate morph weights, bones, etc.)
   *      - Dispatch GPU compute shaders for morph target blending
   */
  class AnimationSystem
  {
  public:
    AnimationSystem(Device& device);
    ~AnimationSystem();

    AnimationSystem(const AnimationSystem&)            = delete;
    AnimationSystem& operator=(const AnimationSystem&) = delete;

    /**
     * @brief Update all registered animations
     *
     * This performs two steps:
     * 1. CPU: Update AnimationComponents (interpolate weights/transforms)
     * 2. GPU: Dispatch morph target compute shaders (blend vertices)
     *
     * Should be called BEFORE the render pass begins.
     *
     * @param frameInfo Contains deltaTime, command buffer, and game objects
     */
    void update(FrameInfo& frameInfo);

    /**
     * @brief Get the morph target manager (for render systems that need blended buffers)
     */
    MorphTargetManager* getMorphManager() { return morphManager_.get(); }

  private:
    Device&                             device_;
    std::unique_ptr<MorphTargetManager> morphManager_;

    void updateAnimations(FrameInfo& frameInfo);
    void updateMorphTargets(FrameInfo& frameInfo);

    // Helper functions moved from AnimationController
    void updateNodeTransforms(AnimationComponent& animComp, const Model::Animation& animation);
    void computeGlobalTransforms(AnimationComponent& animComp, int nodeIndex, const glm::mat4& parentTransform);

    // Interpolation helpers
    glm::vec3 interpolateVec3(float                                           time,
                              const std::vector<std::pair<float, glm::vec3>>& keyframes); // Wait, the signature in AnimationController used AnimationSampler
    // I should probably use AnimationSampler in the signature to match the logic easier.

    glm::vec3          interpolateVec3(const Model::AnimationSampler& sampler, float time);
    glm::quat          interpolateQuat(const Model::AnimationSampler& sampler, float time);
    std::vector<float> interpolateMorphWeights(const Model::AnimationSampler& sampler, float time);
  };

} // namespace engine
