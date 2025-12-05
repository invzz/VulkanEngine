#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Engine/Resources/Model.hpp"

namespace engine {

  struct AnimationComponent
  {
    std::shared_ptr<Model> model;
    std::vector<glm::mat4> nodeTransforms; // Global transforms for each node

    int   currentAnimationIndex = -1;
    float currentTime           = 0.0f;
    float playbackSpeed         = 1.0f;
    bool  isPlaying             = false;
    bool  loop                  = true;

    AnimationComponent(std::shared_ptr<Model> m = nullptr) : model(m)
    {
      if (model)
      {
        nodeTransforms.resize(model->getNodes().size(), glm::mat4(1.0f));
      }
    }

    // Helper methods for UI/Scripts
    void play(int animationIndex = 0, bool shouldLoop = true)
    {
      if (!model || animationIndex < 0 || animationIndex >= (int)model->getAnimations().size()) return;
      currentAnimationIndex = animationIndex;
      currentTime           = 0.0f;
      isPlaying             = true;
      loop                  = shouldLoop;
    }

    void stop()
    {
      isPlaying   = false;
      currentTime = 0.0f;
    }
  };

} // namespace engine
