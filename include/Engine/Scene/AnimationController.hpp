#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

#include "Component.hpp"
#include "Engine/Resources/Model.hpp"

namespace engine {

  class AnimationController : public Component
  {
  public:
    AnimationController(std::shared_ptr<Model> model);

    void update(float deltaTime);
    void play(int animationIndex = 0, bool loop = true);
    void stop();
    void setPlaybackSpeed(float speed) { playbackSpeed_ = speed; }

    bool  isPlaying() const { return isPlaying_; }
    float getCurrentTime() const { return currentTime_; }
    float getPlaybackSpeed() const { return playbackSpeed_; }
    int   getCurrentAnimation() const { return currentAnimationIndex_; }

    // Get the updated node transforms (for debugging or rendering)
    const std::vector<glm::mat4>& getNodeTransforms() const { return nodeTransforms_; }

    // Apply root node transform to a GameObject (for simple translation/rotation animations)
    void applyRootNodeToGameObject(class GameObject& gameObject) const;

  private:
    std::shared_ptr<Model> model_;
    std::vector<glm::mat4> nodeTransforms_; // Global transforms for each node

    int   currentAnimationIndex_ = -1;
    float currentTime_           = 0.0f;
    float playbackSpeed_         = 1.0f;
    bool  isPlaying_             = false;
    bool  loop_                  = true;

    void updateNodeTransforms(const Model::Animation& animation);
    void computeGlobalTransforms(int nodeIndex, const glm::mat4& parentTransform);

    // Interpolation helpers
    glm::vec3          interpolateVec3(const Model::AnimationSampler& sampler, float time) const;
    glm::quat          interpolateQuat(const Model::AnimationSampler& sampler, float time) const;
    std::vector<float> interpolateMorphWeights(const Model::AnimationSampler& sampler, float time) const;
  };

} // namespace engine
