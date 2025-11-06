#pragma once

#include <memory>

#include "2dEngine/Model.hpp"

namespace engine {
  struct RigidBody2dComponent
  {
    glm::vec2 velocity;
    float     mass{1.0f};
  };

  struct Transform2DComponent
  {
    glm::vec2 translation{0.0f, 0.0f}; // position offset
    glm::vec2 scale{1.0f, 1.0f};       // scaling factors
    float     rotation{0.0f};          // rotation angle in radians

    glm::mat2 mat2() const
    {
      float     s           = std::sin(rotation);
      float     c           = std::cos(rotation);
      glm::mat2 rotationMat = {{c, s}, {-s, c}};
      glm::mat2 scaleMat    = {{
                                    scale.x,
                                    0.0f,
                            },
                               {
                                    0.0f,
                                    scale.y,
                            }};

      return rotationMat * scaleMat;
    };
  };

  using id_t = unsigned int;

  class GameObject
  {
  public:
    std::shared_ptr<Model> model{};
    glm::vec3              color{};
    Transform2DComponent   transform2d{};
    RigidBody2dComponent   rigidBody2d{};
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
