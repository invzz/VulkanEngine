#include "3dEngine/systems/AnimationSystem.hpp"

#include <iostream>

namespace engine {

  AnimationSystem::AnimationSystem(Device& device) : device_(device)
  {
    try
    {
      morphManager_ = std::make_unique<MorphTargetManager>(device);
      std::cout << "[AnimationSystem] Initialized successfully" << std::endl;
    }
    catch (const std::exception& e)
    {
      std::cerr << "[AnimationSystem] ERROR: " << e.what() << std::endl;
      throw;
    }
  }

  AnimationSystem::~AnimationSystem() = default;

  void AnimationSystem::registerAnimatedObject(GameObject::id_t objectId)
  {
    animatedObjects_.insert(objectId);
  }

  void AnimationSystem::unregisterAnimatedObject(GameObject::id_t objectId)
  {
    animatedObjects_.erase(objectId);
  }

  void AnimationSystem::update(FrameInfo& frameInfo)
  {
    // Step 1: Update animation controllers (CPU-side: interpolate weights/transforms)
    updateAnimationControllers(frameInfo);

    // Step 2: Dispatch morph target compute shaders (GPU-side: blend vertices)
    updateMorphTargets(frameInfo);
  }

  void AnimationSystem::updateAnimationControllers(FrameInfo& frameInfo)
  {
    // Only iterate registered animated objects (not all gameObjects)
    for (GameObject::id_t objectId : animatedObjects_)
    {
      auto it = frameInfo.gameObjects.find(objectId);
      if (it == frameInfo.gameObjects.end())
      {
        continue; // Object was removed but not unregistered yet
      }

      GameObject& obj = it->second;

      // Update the AnimationController (interpolates morph weights, skeletal transforms, etc.)
      if (obj.animationController)
      {
        obj.animationController->update(frameInfo.frameTime);

        // Apply root node transform to GameObject for translation/rotation animations
        // This makes the object move/rotate in world space based on animation data
        obj.animationController->applyRootNodeToGameObject(obj);
      }
    }
  }

  void AnimationSystem::updateMorphTargets(FrameInfo& frameInfo)
  {
    if (!morphManager_)
    {
      return;
    }

    // Only iterate registered animated objects
    for (GameObject::id_t objectId : animatedObjects_)
    {
      auto it = frameInfo.gameObjects.find(objectId);
      if (it == frameInfo.gameObjects.end())
      {
        continue;
      }

      GameObject& obj = it->second;

      // Only process if object has morph targets
      if (obj.model && obj.model->hasMorphTargets())
      {
        // Initialize GPU buffers for new models
        if (!morphManager_->isModelInitialized(obj.model.get()))
        {
          try
          {
            morphManager_->initializeModel(obj.model);
          }
          catch (const std::exception& e)
          {
            std::cerr << "[AnimationSystem] ERROR initializing morph for object " << objectId << ": " << e.what() << std::endl;
            continue;
          }
        }

        // Dispatch compute shader: baseVertices + morphDeltas * weights → blendedVertices
        morphManager_->updateAndBlend(frameInfo.commandBuffer, obj.model);
      }
    }
  }

} // namespace engine
