#include "Engine/Systems/AnimationSystem.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

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

  void AnimationSystem::update(FrameInfo& frameInfo)
  {
    // Step 1: Update animation components (CPU-side: interpolate weights/transforms)
    updateAnimations(frameInfo);

    // Step 2: Dispatch morph target compute shaders (GPU-side: blend vertices)
    updateMorphTargets(frameInfo);
  }

  void AnimationSystem::updateAnimations(FrameInfo& frameInfo)
  {
    auto view = frameInfo.scene->getRegistry().view<AnimationComponent, TransformComponent>();
    for (auto entity : view)
    {
      auto [anim, transform] = view.get<AnimationComponent, TransformComponent>(entity);

      if (!anim.isPlaying || !anim.model || anim.currentAnimationIndex < 0)
      {
        continue;
      }

      const auto& animation = anim.model->getAnimations()[anim.currentAnimationIndex];

      // Update time
      anim.currentTime += frameInfo.frameTime * anim.playbackSpeed;

      // Handle looping
      if (anim.currentTime > animation.duration)
      {
        if (anim.loop)
        {
          anim.currentTime = fmod(anim.currentTime, animation.duration);
        }
        else
        {
          anim.currentTime = animation.duration;
          anim.isPlaying   = false;
        }
      }

      // Update node transforms based on animation
      updateNodeTransforms(anim, animation);

      // Apply root node transform to TransformComponent
      // Find the root node (first node without a parent)
      int         rootNodeIndex = -1;
      const auto& nodes         = anim.model->getNodes();

      for (size_t i = 0; i < nodes.size(); i++)
      {
        bool isRoot = true;
        for (size_t j = 0; j < nodes.size(); j++)
        {
          const auto& children = nodes[j].children;
          if (std::find(children.begin(), children.end(), static_cast<int>(i)) != children.end())
          {
            isRoot = false;
            break;
          }
        }

        if (isRoot)
        {
          rootNodeIndex = static_cast<int>(i);
          break;
        }
      }

      if (rootNodeIndex >= 0 && rootNodeIndex < static_cast<int>(nodes.size()))
      {
        const auto& rootNode  = nodes[rootNodeIndex];
        transform.translation = rootNode.translation;
        transform.rotation    = glm::eulerAngles(rootNode.rotation);
        transform.scale       = rootNode.scale * transform.baseScale;
      }
    }
  }

  void AnimationSystem::updateMorphTargets(FrameInfo& frameInfo)
  {
    if (!morphManager_)
    {
      return;
    }

    auto view = frameInfo.scene->getRegistry().view<ModelComponent>();
    for (auto entity : view)
    {
      auto& modelComp = view.get<ModelComponent>(entity);

      if (modelComp.model && modelComp.model->hasMorphTargets())
      {
        // Initialize GPU buffers for new models
        if (!morphManager_->isModelInitialized(modelComp.model.get()))
        {
          try
          {
            morphManager_->initializeModel(modelComp.model);
          }
          catch (const std::exception& e)
          {
            std::cerr << "[AnimationSystem] ERROR initializing morph for object " << (uint32_t)entity << ": " << e.what() << std::endl;
            continue;
          }
        }

        // Dispatch compute shader
        morphManager_->updateAndBlend(frameInfo.commandBuffer, modelComp.model);
      }
    }
  }

  void AnimationSystem::updateNodeTransforms(AnimationComponent& animComp, const Model::Animation& animation)
  {
    // Apply animation to nodes
    auto& nodes = animComp.model->getNodes();

    for (const auto& channel : animation.channels)
    {
      if (channel.targetNode < 0 || channel.targetNode >= static_cast<int>(nodes.size()))
      {
        continue;
      }

      const auto& sampler = animation.samplers[channel.samplerIndex];
      auto&       node    = nodes[channel.targetNode];

      switch (channel.path)
      {
      case Model::AnimationChannel::TRANSLATION:
        node.translation = interpolateVec3(sampler, animComp.currentTime);
        break;
      case Model::AnimationChannel::ROTATION:
        node.rotation = interpolateQuat(sampler, animComp.currentTime);
        break;
      case Model::AnimationChannel::SCALE:
        node.scale = interpolateVec3(sampler, animComp.currentTime);
        break;
      case Model::AnimationChannel::WEIGHTS:
        node.morphWeights = interpolateMorphWeights(sampler, animComp.currentTime);
        break;
      }
    }

    // Recompute global transforms
    for (size_t i = 0; i < nodes.size(); i++)
    {
      bool isRoot = true;
      for (size_t j = 0; j < nodes.size(); j++)
      {
        const auto& children = nodes[j].children;
        if (std::find(children.begin(), children.end(), static_cast<int>(i)) != children.end())
        {
          isRoot = false;
          break;
        }
      }

      if (isRoot)
      {
        computeGlobalTransforms(animComp, static_cast<int>(i), glm::mat4(1.0f));
      }
    }
  }

  void AnimationSystem::computeGlobalTransforms(AnimationComponent& animComp, int nodeIndex, const glm::mat4& parentTransform)
  {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(animComp.model->getNodes().size()))
    {
      return;
    }

    const auto& node           = animComp.model->getNodes()[nodeIndex];
    glm::mat4   localTransform = node.getLocalTransform();

    if (nodeIndex < static_cast<int>(animComp.nodeTransforms.size()))
    {
      animComp.nodeTransforms[nodeIndex] = parentTransform * localTransform;
    }

    for (int childIdx : node.children)
    {
      computeGlobalTransforms(animComp, childIdx, animComp.nodeTransforms[nodeIndex]);
    }
  }

  glm::vec3 AnimationSystem::interpolateVec3(const Model::AnimationSampler& sampler, float time)
  {
    if (sampler.times.empty() || sampler.translations.empty())
    {
      return glm::vec3(0.0f);
    }

    if (time <= sampler.times.front()) return sampler.translations.front();
    if (time >= sampler.times.back()) return sampler.translations.back();

    size_t nextIndex = 0;
    for (size_t i = 0; i < sampler.times.size() - 1; i++)
    {
      if (time >= sampler.times[i] && time < sampler.times[i + 1])
      {
        nextIndex = i + 1;
        break;
      }
    }

    size_t prevIndex = nextIndex - 1;

    if (sampler.interpolation == Model::AnimationSampler::STEP)
    {
      return sampler.translations[prevIndex];
    }

    float t0     = sampler.times[prevIndex];
    float t1     = sampler.times[nextIndex];
    float factor = (time - t0) / (t1 - t0);

    return glm::mix(sampler.translations[prevIndex], sampler.translations[nextIndex], factor);
  }

  glm::quat AnimationSystem::interpolateQuat(const Model::AnimationSampler& sampler, float time)
  {
    if (sampler.times.empty() || sampler.rotations.empty())
    {
      return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    if (time <= sampler.times.front()) return sampler.rotations.front();
    if (time >= sampler.times.back()) return sampler.rotations.back();

    size_t nextIndex = 0;
    for (size_t i = 0; i < sampler.times.size() - 1; i++)
    {
      if (time >= sampler.times[i] && time < sampler.times[i + 1])
      {
        nextIndex = i + 1;
        break;
      }
    }

    size_t prevIndex = nextIndex - 1;

    if (sampler.interpolation == Model::AnimationSampler::STEP)
    {
      return sampler.rotations[prevIndex];
    }

    float t0     = sampler.times[prevIndex];
    float t1     = sampler.times[nextIndex];
    float factor = (time - t0) / (t1 - t0);

    return glm::slerp(sampler.rotations[prevIndex], sampler.rotations[nextIndex], factor);
  }

  std::vector<float> AnimationSystem::interpolateMorphWeights(const Model::AnimationSampler& sampler, float time)
  {
    if (sampler.times.empty() || sampler.morphWeights.empty())
    {
      return {};
    }

    if (time <= sampler.times.front()) return sampler.morphWeights.front();
    if (time >= sampler.times.back()) return sampler.morphWeights.back();

    size_t nextIndex = 0;
    for (size_t i = 0; i < sampler.times.size() - 1; i++)
    {
      if (time >= sampler.times[i] && time < sampler.times[i + 1])
      {
        nextIndex = i + 1;
        break;
      }
    }

    size_t prevIndex = nextIndex - 1;

    if (sampler.interpolation == Model::AnimationSampler::STEP)
    {
      return sampler.morphWeights[prevIndex];
    }

    float t0     = sampler.times[prevIndex];
    float t1     = sampler.times[nextIndex];
    float factor = (time - t0) / (t1 - t0);

    const auto&        prevWeights = sampler.morphWeights[prevIndex];
    const auto&        nextWeights = sampler.morphWeights[nextIndex];
    std::vector<float> result(prevWeights.size());

    for (size_t i = 0; i < prevWeights.size(); i++)
    {
      result[i] = prevWeights[i] * (1.0f - factor) + nextWeights[i] * factor;
    }

    return result;
  }

} // namespace engine
