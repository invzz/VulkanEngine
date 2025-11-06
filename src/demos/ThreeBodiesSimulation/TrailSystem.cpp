#include "TrailSystem.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <limits>
#include <utility>

#include "SimulationConfig.hpp"
#include "ThreeBodySimulationPhysics.hpp"

namespace engine {

  namespace detail {
    float edgeExposure(const glm::vec2& position)
    {
      const float maxAbs = glm::max(glm::abs(position.x), glm::abs(position.y));
      if (maxAbs <= BorderSettings::repulsionStart) return 0.0f;
      const float range = BorderSettings::clamp - BorderSettings::repulsionStart;
      if (range <= std::numeric_limits<float>::epsilon()) return 1.0f;
      return glm::clamp((maxAbs - BorderSettings::repulsionStart) / range, 0.0f, 1.0f);
    }
  } // namespace detail

  TrailSystem::TrailSystem(std::shared_ptr<Model> model) : trailModel(std::move(model)) {}

  void TrailSystem::spawn(const glm::vec2& position, const glm::vec2& velocity, const glm::vec3& color)
  {
    const float speed = glm::length(velocity);
    if (speed < std::numeric_limits<float>::epsilon()) return;

    GameObject particle              = GameObject::create();
    particle.model                   = trailModel;
    particle.transform2d.translation = position;
    particle.transform2d.rotation    = glm::atan(velocity.y, velocity.x);

    const float speedRatio     = glm::clamp(speed / speedForMaxScale, 0.f, 1.f);
    const float length         = glm::mix(minLength, maxLength, speedRatio);
    const float width          = glm::mix(minWidth, maxWidth, speedRatio);
    particle.transform2d.scale = {width, length};
    particle.color             = color;

    particles.push_back(std::move(particle));
    ages.push_back(0.f);
    const float lifetimeRatio = glm::clamp(speed / speedForMaxLifetime, 0.f, 1.f);
    lifetimes.push_back(glm::mix(minLifetime, maxLifetime, lifetimeRatio));
    baseColors.push_back(color);
  }

  void TrailSystem::update(float dt)
  {
    for (std::size_t i = 0; i < particles.size(); ++i)
    {
      ages[i] += dt;
      const float     normalizedAge = glm::clamp(1.f - ages[i] / lifetimes[i], 0.f, 1.f);
      const glm::vec3 fadedColor    = normalizedAge * baseColors[i];
      particles[i].color            = fadedColor;
      particles[i].transform2d.scale *= trailShrinkFactor;
    }

    for (std::size_t i = particles.size(); i-- > 0;)
    {
      if (ages[i] >= lifetimes[i])
      {
        removeAt(i);
      }
    }
  }

  const std::vector<GameObject>& TrailSystem::renderObjects() const
  {
    return particles;
  }

  void TrailSystem::removeAt(std::size_t index)
  {
    particles[index] = std::move(particles.back());
    particles.pop_back();

    ages[index] = ages.back();
    ages.pop_back();

    lifetimes[index] = lifetimes.back();
    lifetimes.pop_back();

    baseColors[index] = baseColors.back();
    baseColors.pop_back();
  }

  void updateColorsAndTrails(std::vector<GameObject>&      physicsObjects,
                             const std::vector<glm::vec3>& baseColors,
                             std::vector<float>&           trailSpawnAccumulator,
                             TrailSystem&                  trailSystem,
                             float                         frameDt)
  {
    const float     minIntensity      = SimulationConfig::Trails::minIntensity;
    const float     intensityMaxSpeed = SimulationConfig::Trails::intensityMaxSpeed;
    const float     spawnInterval     = SimulationConfig::Trails::spawnInterval;
    const float     minTrailSpeed     = SimulationConfig::Trails::minTrailSpeed;
    constexpr float epsilon           = std::numeric_limits<float>::epsilon();

    for (std::size_t i = 0; i < physicsObjects.size(); ++i)
    {
      auto&       obj   = physicsObjects[i];
      const float speed = glm::length(obj.rigidBody2d.velocity);

      const float t     = glm::clamp(speed / intensityMaxSpeed, 0.f, 1.f);
      const float scale = glm::mix(minIntensity, 1.0f, t);

      const glm::vec3 pastel     = glm::mix(glm::vec3(0.9f), baseColors[i], SimulationConfig::Trails::pastelMix);
      const glm::vec3 speedTint  = glm::mix(pastel, baseColors[i], t);
      const float     edgeHeat   = glm::pow(detail::edgeExposure(obj.transform2d.translation), SimulationConfig::Trails::edgeGlowExponent);
      const glm::vec3 glowTarget = glm::mix(speedTint, glm::vec3(0.1f), edgeHeat);
      obj.color                  = glm::clamp(scale * glowTarget, glm::vec3(0.0f), glm::vec3(1.0f));

      trailSpawnAccumulator[i] += frameDt;
      if (speed <= minTrailSpeed + epsilon)
      {
        trailSpawnAccumulator[i] = glm::min(trailSpawnAccumulator[i], spawnInterval);
        continue;
      }

      const auto spawnCount = static_cast<int>(trailSpawnAccumulator[i] / spawnInterval);
      if (spawnCount <= 0) continue;

      trailSpawnAccumulator[i] -= spawnInterval * static_cast<float>(spawnCount);
      for (int emit = 0; emit < spawnCount; ++emit)
      {
        trailSystem.spawn(obj.transform2d.translation, obj.rigidBody2d.velocity, obj.color);
      }
    }
  }

} // namespace engine
