#include "ThreeBodySimulationPhysics.hpp"

#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "2dEngine/GameObject.hpp"

namespace engine {

    float massFromScale(const Transform2DComponent& transform)
    {
        const glm::vec2 absScale = glm::abs(transform.scale);
        const float     area     = absScale.x * absScale.y;
        return glm::max(area * SimulationConfig::Physics::massDensity, 1e-4f);
    }

    float uniformScaleForMass(float mass)
    {
        const float area =
                glm::max(mass / SimulationConfig::Physics::massDensity, SimulationConfig::Physics::minScale * SimulationConfig::Physics::minScale);
        const float scale = glm::sqrt(area);
        return glm::clamp(scale, SimulationConfig::Physics::minScale, SimulationConfig::Physics::maxScale);
    }

    float minimumSplitMass()
    {
        return SimulationConfig::Physics::massDensity * SimulationConfig::Physics::minSplitScale * SimulationConfig::Physics::minSplitScale;
    }

} // namespace engine
