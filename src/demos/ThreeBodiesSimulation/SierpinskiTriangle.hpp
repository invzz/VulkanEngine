#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "Model.hpp"

namespace engine {

    class SierpinskiTriangle
    {
      public:
        static std::vector<Model::Vertex>
        create(uint32_t order, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3);
    };

} // namespace engine