#pragma once

#include <memory>
#include <vector>

#include "3dEngine/Model.hpp"

namespace engine {

  struct LODLevel
  {
    std::shared_ptr<Model> model;
    float                  distance; // Distance at which this LOD becomes active
  };

  struct LODComponent
  {
    std::vector<LODLevel> levels; // Should be sorted by distance
  };

} // namespace engine
