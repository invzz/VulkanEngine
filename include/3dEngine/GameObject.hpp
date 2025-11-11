#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <unordered_map>

#include "Model.hpp"

namespace engine {

  struct TransformComponent
  {
    glm::vec3 translation{};           // position offset
    glm::vec3 scale{1.0f, 1.0f, 1.0f}; // scaling factors
    glm::vec3 rotation{};              // rotation angles in radians

    // Matrix corresponding to translate * rotate * scale
    // * optimized version : using precomputed sines and cosines
    // Note: rotation order is Y (yaw), X (pitch), Z (roll)
    // Reference:
    // https://en.wikipedia.org/wiki/Rotation_matrix#In_three_dimensions
    glm::mat4 modelTransform() const;
    glm::vec3 getForwardDir() const;
    glm::vec3 getRightDir() const;
    glm::mat3 normalMatrix() const;
  };

  struct PointLightComponent
  {
    float intensity{1.0f};
  };

  class GameObject
  {
  public:
    using id_t = unsigned int;
    using Map  = std::unordered_map<id_t, GameObject>;
    TransformComponent transform{};
    glm::vec3          color{};
    id_t               id{0};

    // Optional pointer components
    std::shared_ptr<Model>               model{};
    std::unique_ptr<PointLightComponent> pointLight = nullptr;

    static GameObject create()
    {
      static id_t currentId = 0;
      return GameObject{currentId++};
    }

    // delete copy operations
    GameObject(const GameObject&)            = delete;
    GameObject& operator=(const GameObject&) = delete;

    // default move operations
    GameObject(GameObject&&) noexcept            = default;
    GameObject& operator=(GameObject&&) noexcept = default;

    id_t getId() const { return id; }

    static GameObject makePointLightObject(float intensity = 10.f, const glm::vec3& color = {1.f, 1.f, 1.f}, float radius = 0.1f)
    {
      GameObject obj            = GameObject::create();
      obj.color                 = color;
      obj.transform.scale.x     = radius;
      obj.pointLight            = std::make_unique<PointLightComponent>();
      obj.pointLight->intensity = intensity;
      return obj;
    }

  private:
    explicit GameObject(id_t objId) : id{objId} {}
  };

} // namespace engine
