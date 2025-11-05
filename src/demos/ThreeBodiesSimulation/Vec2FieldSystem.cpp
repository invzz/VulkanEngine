#include "Vec2FieldSystem.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <limits>

#include "SimulationConfig.hpp"

namespace engine {

    void Vec2FieldSystem::update(const GravityPhysicsSystem& physicsSystem,
                                 std::vector<GameObject>&    physicsObjs,
                                 std::vector<GameObject>&    vectorField) const
    {
        for (auto& vf : vectorField)
        {
            glm::vec2 direction{};
            glm::vec3 blendedColor{};
            float     totalWeight = 0.0f;
            for (auto& obj : physicsObjs)
            {
                const auto force = physicsSystem.computeForce(obj, vf);
                direction += force;

                const float weight = glm::length(force);
                blendedColor += weight * obj.color;
                totalWeight += weight;
            }

            const float strength    = glm::clamp(glm::log(glm::length(direction) + 1) / SimulationConfig::VectorField::logScaleNormalizer, 0.f, 1.f);
            vf.transform2d.scale.x  = SimulationConfig::VectorField::baseScale + SimulationConfig::VectorField::dynamicScale * strength;
            vf.transform2d.rotation = glm::atan(direction.y, direction.x);

            if (totalWeight > std::numeric_limits<float>::epsilon())
            {
                vf.color = blendedColor / totalWeight;
            }
        }
    }

} // namespace engine
