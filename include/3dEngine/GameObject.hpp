#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <memory>

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

  using id_t = unsigned int;

  class GameObject
  {
  public:
    std::shared_ptr<Model> model{};
    glm::vec3              color{};
    TransformComponent     transform{};
    id_t                   id{0};

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

  private:
    explicit GameObject(id_t objId) : id{objId} {}
  };

} // namespace engine
