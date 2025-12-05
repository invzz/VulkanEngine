#pragma once

#include <memory>

#include "Engine/Resources/Model.hpp"

namespace engine {

  struct ModelComponent
  {
    std::shared_ptr<Model> model;
  };

} // namespace engine
