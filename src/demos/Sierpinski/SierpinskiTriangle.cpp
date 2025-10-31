#include "SierpinskiTriangle.hpp"

#include <glm/glm.hpp>
#include <vector>

#include "Model.hpp"

namespace engine {
    std::vector<Model::Vertex>
    SierpinskiTriangle::create(uint32_t order, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3)
    {
        std::vector<Model::Vertex> vertices;

        auto red   = glm::vec3(1.0f, 0.0f, 0.0f);
        auto green = glm::vec3(0.0f, 1.0f, 0.0f);
        auto blue  = glm::vec3(0.0f, 0.0f, 1.0f);

        if (order == 0)
        {
            vertices.push_back({p1, red});
            vertices.push_back({p2, green});
            vertices.push_back({p3, blue});
        }
        else
        {
            glm::vec2 mid12 = (p1 + p2) / 2.0f;
            glm::vec2 mid23 = (p2 + p3) / 2.0f;
            glm::vec2 mid31 = (p3 + p1) / 2.0f;

            auto top   = create(order - 1, p1, mid12, mid31);
            auto left  = create(order - 1, mid12, p2, mid23);
            auto right = create(order - 1, mid31, mid23, p3);

            vertices.insert(vertices.end(), top.begin(), top.end());
            vertices.insert(vertices.end(), left.begin(), left.end());
            vertices.insert(vertices.end(), right.begin(), right.end());
        }

        return vertices;
    };
} // namespace engine