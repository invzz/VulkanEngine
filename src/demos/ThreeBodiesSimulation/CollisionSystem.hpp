#pragma once

#include <memory>
#include <vector>

#include "GameObject.hpp"
#include "Model.hpp"

namespace engine {

    void handleCollisions(std::vector<GameObject>&      physicsObjects,
                          std::vector<glm::vec3>&       baseColors,
                          std::vector<float>&           trailSpawnAccumulator,
                          const std::shared_ptr<Model>& circleModel);

} // namespace engine
