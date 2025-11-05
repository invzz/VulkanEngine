#include "SimpleModels.hpp"

#include <glm/gtc/constants.hpp>

namespace engine {
    std::shared_ptr<Model> SimpleModels::createSquareModel(Device& device, glm::vec2 offset)
    {
        std::vector<Model::Vertex> vertices = {
                {{-0.5f, -0.5f}},
                {{0.5f, 0.5f}},
                {{-0.5f, 0.5f}},
                {{-0.5f, -0.5f}},
                {{0.5f, -0.5f}},
                {{0.5f, 0.5f}},
        };
        for (auto& v : vertices)
        {
            v.position += offset;
        }
        return std::make_unique<Model>(device, vertices);
    }

    std::shared_ptr<Model> SimpleModels::createCircleModel(Device& device, unsigned int numSides)
    {
        std::vector<Model::Vertex> uniqueVertices{};
        const auto                 sideCount = static_cast<int>(numSides);
        for (int i = 0; i < sideCount; i++)
        {
            float angle = static_cast<float>(i) * glm::two_pi<float>() / static_cast<float>(sideCount);
            uniqueVertices.push_back({{glm::cos(angle), glm::sin(angle)}});
        }
        uniqueVertices.push_back({}); // adds center vertex at 0, 0

        std::vector<Model::Vertex> vertices{};
        for (int i = 0; i < sideCount; i++)
        {
            vertices.push_back(uniqueVertices[i]);
            vertices.push_back(uniqueVertices[(i + 1) % sideCount]);
            vertices.push_back(uniqueVertices[sideCount]);
        }
        return std::make_unique<Model>(device, vertices);
    }
} // namespace engine
