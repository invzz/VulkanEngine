#pragma once

#include <memory>

#include "2dEngine/Model.hpp"

namespace engine {
  class SimpleModels
  {
  public:
    static std::shared_ptr<engine::Model> createCircleModel(Device& device, unsigned int numSides);

    static std::shared_ptr<Model> createSquareModel(Device& device, glm::vec2 offset);
  };
} // namespace engine