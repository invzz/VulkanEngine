#include "3dEngine/AnimationController.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "3dEngine/GameObject.hpp"
#include "3dEngine/ansi_colors.hpp"

namespace engine {

  AnimationController::AnimationController(std::shared_ptr<Model> model) : model_(model)
  {
    if (!model_)
    {
      return;
    }

    // Initialize node transforms with identity
    nodeTransforms_.resize(model_->getNodes().size(), glm::mat4(1.0f));

    // Compute initial transforms from node hierarchy
    const auto& nodes = model_->getNodes();
    if (!nodes.empty())
    {
      // Find root nodes (those without parents)
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
          computeGlobalTransforms(static_cast<int>(i), glm::mat4(1.0f));
        }
      }
    }
  }

  void AnimationController::play(int animationIndex, bool loop)
  {
    if (!model_ || animationIndex < 0 || animationIndex >= static_cast<int>(model_->getAnimations().size()))
    {
      std::cerr << RED << "[AnimationController] Invalid animation index: " << animationIndex << RESET << std::endl;
      return;
    }

    currentAnimationIndex_ = animationIndex;
    currentTime_           = 0.0f;
    isPlaying_             = true;
    loop_                  = loop;

    const auto& animation = model_->getAnimations()[currentAnimationIndex_];
    std::cout << GREEN << "[AnimationController] Playing animation: " << BLUE << animation.name << RESET << std::endl;
  }

  void AnimationController::stop()
  {
    isPlaying_   = false;
    currentTime_ = 0.0f;
  }

  void AnimationController::update(float deltaTime)
  {
    if (!isPlaying_ || !model_ || currentAnimationIndex_ < 0)
    {
      return;
    }

    const auto& animation = model_->getAnimations()[currentAnimationIndex_];

    // Update time
    currentTime_ += deltaTime * playbackSpeed_;

    // Handle looping
    if (currentTime_ > animation.duration)
    {
      if (loop_)
      {
        currentTime_ = fmod(currentTime_, animation.duration);
      }
      else
      {
        currentTime_ = animation.duration;
        isPlaying_   = false;
      }
    }

    // Update node transforms based on animation
    updateNodeTransforms(animation);
  }

  void AnimationController::updateNodeTransforms(const Model::Animation& animation)
  {
    // Apply animation to nodes
    auto& nodes = model_->getNodes();

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
        node.translation = interpolateVec3(sampler, currentTime_);
        break;
      case Model::AnimationChannel::ROTATION:
        node.rotation = interpolateQuat(sampler, currentTime_);
        break;
      case Model::AnimationChannel::SCALE:
        node.scale = interpolateVec3(sampler, currentTime_);
        break;
      case Model::AnimationChannel::WEIGHTS:
        node.morphWeights = interpolateMorphWeights(sampler, currentTime_);
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
        computeGlobalTransforms(static_cast<int>(i), glm::mat4(1.0f));
      }
    }
  }

  void AnimationController::computeGlobalTransforms(int nodeIndex, const glm::mat4& parentTransform)
  {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model_->getNodes().size()))
    {
      return;
    }

    const auto& node           = model_->getNodes()[nodeIndex];
    glm::mat4   localTransform = node.getLocalTransform();
    nodeTransforms_[nodeIndex] = parentTransform * localTransform;

    // Process children
    for (int childIdx : node.children)
    {
      computeGlobalTransforms(childIdx, nodeTransforms_[nodeIndex]);
    }
  }

  glm::vec3 AnimationController::interpolateVec3(const Model::AnimationSampler& sampler, float time) const
  {
    if (sampler.times.empty() || sampler.translations.empty())
    {
      return glm::vec3(0.0f);
    }

    // Find the keyframes to interpolate between
    if (time <= sampler.times.front())
    {
      return sampler.translations.front();
    }

    if (time >= sampler.times.back())
    {
      return sampler.translations.back();
    }

    // Find the two keyframes
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

    // Linear interpolation
    float t0     = sampler.times[prevIndex];
    float t1     = sampler.times[nextIndex];
    float factor = (time - t0) / (t1 - t0);

    return glm::mix(sampler.translations[prevIndex], sampler.translations[nextIndex], factor);
  }

  glm::quat AnimationController::interpolateQuat(const Model::AnimationSampler& sampler, float time) const
  {
    if (sampler.times.empty() || sampler.rotations.empty())
    {
      return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    // Find the keyframes to interpolate between
    if (time <= sampler.times.front())
    {
      return sampler.rotations.front();
    }

    if (time >= sampler.times.back())
    {
      return sampler.rotations.back();
    }

    // Find the two keyframes
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

    // Spherical linear interpolation (slerp)
    float t0     = sampler.times[prevIndex];
    float t1     = sampler.times[nextIndex];
    float factor = (time - t0) / (t1 - t0);

    return glm::slerp(sampler.rotations[prevIndex], sampler.rotations[nextIndex], factor);
  }

  std::vector<float> AnimationController::interpolateMorphWeights(const Model::AnimationSampler& sampler, float time) const
  {
    if (sampler.times.empty() || sampler.morphWeights.empty())
    {
      return {};
    }

    // Find the keyframes to interpolate between
    if (time <= sampler.times.front())
    {
      return sampler.morphWeights.front();
    }

    if (time >= sampler.times.back())
    {
      return sampler.morphWeights.back();
    }

    // Find the two keyframes
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

    // Linear interpolation for all weights
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

  void AnimationController::applyRootNodeToGameObject(GameObject& gameObject) const
  {
    if (!model_ || model_->getNodes().empty())
    {
      return;
    }

    // Find the root node (first node without a parent)
    int         rootNodeIndex = -1;
    const auto& nodes         = model_->getNodes();

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

    if (rootNodeIndex < 0 || rootNodeIndex >= static_cast<int>(nodes.size()))
    {
      return;
    }

    const auto& rootNode = nodes[rootNodeIndex];

    // Apply the root node's transform to the GameObject
    // This makes translation/rotation animations visible
    // For scale, multiply animation scale with base scale to preserve user modifications
    gameObject.transform.translation = rootNode.translation;
    gameObject.transform.rotation    = glm::eulerAngles(rootNode.rotation);
    gameObject.transform.scale       = rootNode.scale * gameObject.transform.baseScale;
  }

} // namespace engine
