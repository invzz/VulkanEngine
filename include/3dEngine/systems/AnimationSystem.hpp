#pragma once

#include <memory>
#include <unordered_set>

#include "3dEngine/Device.hpp"
#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/MorphTargetManager.hpp"

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
     * @brief Register a GameObject that needs animation updates
     *
     * Call this when:
     * - Loading a model with animations at startup
     * - Spawning an animated object at runtime
     *
     * @param objectId The unique ID of the GameObject
     */
    void registerAnimatedObject(GameObject::id_t objectId);

    /**
     * @brief Unregister a GameObject (e.g., when destroying it)
     */
    void unregisterAnimatedObject(GameObject::id_t objectId);

    /**
     * @brief Update all registered animations
     *
     * This performs two steps:
     * 1. CPU: Update AnimationControllers (interpolate weights/transforms)
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
    Device&                              device_;
    std::unique_ptr<MorphTargetManager>  morphManager_;
    std::unordered_set<GameObject::id_t> animatedObjects_; // Only objects with animations

    void updateAnimationControllers(FrameInfo& frameInfo);
    void updateMorphTargets(FrameInfo& frameInfo);
  };

} // namespace engine
