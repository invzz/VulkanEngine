#include "CollisionSystem.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <optional>
#include <random>
#include <utility>

#include "SimulationConfig.hpp"
#include "ThreeBodySimulationPhysics.hpp"

namespace engine {

  float computeRadius(const GameObject& obj)
  {
    const glm::vec2 absScale = glm::abs(obj.transform2d.scale);
    return glm::max(absScale.x, absScale.y);
  }

  glm::vec3 randomColor()
  {
    static std::mt19937                  rng{std::random_device{}()};
    static std::uniform_int_distribution dist{0, static_cast<int>(SimulationConfig::InitialBodies::colors.size()) - 1};
    return SimulationConfig::InitialBodies::colors[static_cast<std::size_t>(dist(rng))];
  }

  void mergeBodies(std::size_t              keepIndex,
                   std::size_t              removeIndex,
                   std::vector<GameObject>& physicsObjects,
                   std::vector<glm::vec3>&  baseColors,
                   std::vector<float>&      trailSpawnAccumulator)
  {
    auto&             objKeep   = physicsObjects[keepIndex];
    const GameObject& objRemove = physicsObjects[removeIndex];

    const float massKeep   = objKeep.rigidBody2d.mass;
    const float massRemove = objRemove.rigidBody2d.mass;
    const float totalMass  = massKeep + massRemove;
    if (totalMass <= std::numeric_limits<float>::epsilon()) return;

    const glm::vec2 newPos = (massKeep * objKeep.transform2d.translation + massRemove * objRemove.transform2d.translation) / totalMass;
    const glm::vec2 newVel = (massKeep * objKeep.rigidBody2d.velocity + massRemove * objRemove.rigidBody2d.velocity) / totalMass;

    const glm::vec3 parentColor = physicsObjects[removeIndex].color;

    objKeep.transform2d.translation  = newPos;
    objKeep.transform2d.scale        = glm::vec2{uniformScaleForMass(totalMass)};
    objKeep.rigidBody2d.velocity     = newVel;
    objKeep.rigidBody2d.mass         = totalMass;
    objKeep.color                    = parentColor;
    baseColors[keepIndex]            = parentColor;
    trailSpawnAccumulator[keepIndex] = glm::min(trailSpawnAccumulator[keepIndex], trailSpawnAccumulator[removeIndex]);

    physicsObjects.erase(physicsObjects.begin() + removeIndex);
    baseColors.erase(baseColors.begin() + removeIndex);
    trailSpawnAccumulator.erase(trailSpawnAccumulator.begin() + removeIndex);
  }

  bool trySplitBody(std::size_t                   index,
                    std::vector<GameObject>&      physicsObjects,
                    std::vector<glm::vec3>&       baseColors,
                    std::vector<float>&           trailSpawnAccumulator,
                    const std::shared_ptr<Model>& circleModel)
  {
    if (index >= physicsObjects.size()) return false;

    auto&       body = physicsObjects[index];
    const float mass = body.rigidBody2d.mass;
    if (mass < 2.0f * minimumSplitMass()) return false;

    const float     childMass            = mass * 0.5f;
    const float     childScale           = uniformScaleForMass(childMass);
    const glm::vec3 parentColorPrimary   = randomColor();
    const glm::vec3 parentColorSecondary = randomColor();

    const glm::vec2 centre   = body.transform2d.translation;
    const glm::vec2 velocity = body.rigidBody2d.velocity;

    glm::vec2 axis = velocity;
    if (glm::dot(axis, axis) < 1e-6f) axis = {1.0f, 0.0f};
    axis = glm::normalize(axis);
    const glm::vec2 tangent{-axis.y, axis.x};

    const float offset     = SimulationConfig::Collision::splitOffsetFactor;
    const float impulseMag = SimulationConfig::Collision::splitImpulse;

    body.transform2d.translation = centre + tangent * (childScale * offset);
    body.transform2d.scale       = glm::vec2{childScale};
    body.rigidBody2d.mass        = childMass;
    body.rigidBody2d.velocity    = velocity + axis * impulseMag;
    body.color                   = parentColorPrimary;
    baseColors[index]            = parentColorPrimary;
    trailSpawnAccumulator[index] = 0.0f;

    GameObject fragment              = GameObject::create();
    fragment.model                   = circleModel;
    fragment.transform2d.scale       = glm::vec2{childScale};
    fragment.transform2d.translation = centre - tangent * (childScale * offset);
    fragment.rigidBody2d.mass        = childMass;
    fragment.rigidBody2d.velocity    = velocity - axis * impulseMag;
    fragment.color                   = parentColorSecondary;

    physicsObjects.push_back(std::move(fragment));
    baseColors.push_back(parentColorSecondary);
    trailSpawnAccumulator.push_back(0.0f);

    return true;
  }

  std::optional<std::pair<std::size_t, std::size_t>> findCollisionPair(const std::vector<GameObject>& physicsObjects)
  {
    const std::size_t count = physicsObjects.size();
    for (std::size_t i = 0; i < count; ++i)
    {
      for (std::size_t j = i + 1; j < count; ++j)
      {
        const GameObject& objA      = physicsObjects[i];
        const GameObject& objB      = physicsObjects[j];
        const float       radiusSum = computeRadius(objA) + computeRadius(objB);
        const float       distance  = glm::length(objA.transform2d.translation - objB.transform2d.translation);
        if (distance < radiusSum)
        {
          return std::pair<std::size_t, std::size_t>{i, j};
        }
      }
    }
    return std::nullopt;
  }

  void handleCollisions(std::vector<GameObject>&      physicsObjects,
                        std::vector<glm::vec3>&       baseColors,
                        std::vector<float>&           trailSpawnAccumulator,
                        const std::shared_ptr<Model>& circleModel)
  {
    const float splitSpeedThreshold = SimulationConfig::Collision::splitSpeedThreshold;
    if (physicsObjects.size() < 2) return;

    while (true)
    {
      const auto collisionPair = findCollisionPair(physicsObjects);
      if (!collisionPair) break;

      const std::size_t idxA = collisionPair->first;
      const std::size_t idxB = collisionPair->second;
      const auto&       objA = physicsObjects[idxA];
      const auto&       objB = physicsObjects[idxB];

      if (const float relativeSpeed = glm::length(objA.rigidBody2d.velocity - objB.rigidBody2d.velocity); relativeSpeed > splitSpeedThreshold)
      {
        const std::size_t higher = glm::max(idxA, idxB);
        const std::size_t lower  = glm::min(idxA, idxB);
        if (trySplitBody(higher, physicsObjects, baseColors, trailSpawnAccumulator, circleModel))
        {
          continue;
        }
        if (trySplitBody(lower, physicsObjects, baseColors, trailSpawnAccumulator, circleModel))
        {
          continue;
        }
      }

      const std::size_t keep   = glm::min(idxA, idxB);
      const std::size_t remove = glm::max(idxA, idxB);
      mergeBodies(keep, remove, physicsObjects, baseColors, trailSpawnAccumulator);
    }
  }

} // namespace engine
