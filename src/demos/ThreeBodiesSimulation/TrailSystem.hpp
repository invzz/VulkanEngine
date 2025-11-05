#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "2dEngine/GameObject.hpp"
#include "2dEngine/Model.hpp"
#include "SimulationConfig.hpp"

namespace engine {

  class TrailSystem
  {
  public:
    explicit TrailSystem(std::shared_ptr<Model> model);

    void spawn(const glm::vec2& position, const glm::vec2& velocity, const glm::vec3& color);
    void update(float dt);

    const std::vector<GameObject>& renderObjects() const;

  private:
    void removeAt(std::size_t index);

    std::shared_ptr<Model>  trailModel;
    std::vector<GameObject> particles{};
    std::vector<float>      ages{};
    std::vector<float>      lifetimes{};
    std::vector<glm::vec3>  baseColors{};

    static constexpr float minWidth            = SimulationConfig::Trails::minWidth;
    static constexpr float maxWidth            = SimulationConfig::Trails::maxWidth;
    static constexpr float minLength           = SimulationConfig::Trails::minLength;
    static constexpr float maxLength           = SimulationConfig::Trails::maxLength;
    static constexpr float minLifetime         = SimulationConfig::Trails::minLifetime;
    static constexpr float maxLifetime         = SimulationConfig::Trails::maxLifetime;
    static constexpr float speedForMaxScale    = SimulationConfig::Trails::speedForMaxScale;
    static constexpr float speedForMaxLifetime = SimulationConfig::Trails::speedForMaxLifetime;
    static constexpr float trailShrinkFactor   = SimulationConfig::Trails::shrinkFactor;
  };

  void updateColorsAndTrails(std::vector<GameObject>&      physicsObjects,
                             const std::vector<glm::vec3>& baseColors,
                             std::vector<float>&           trailSpawnAccumulator,
                             TrailSystem&                  trailSystem,
                             float                         frameDt);

} // namespace engine
