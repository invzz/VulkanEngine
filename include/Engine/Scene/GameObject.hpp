#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Component.hpp"
#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "components/DirectionalLightComponent.hpp"
#include "components/LODComponent.hpp"
#include "components/PointLightComponent.hpp"
#include "components/SpotLightComponent.hpp"
#include "components/TransformComponent.hpp"

namespace engine {

  class AnimationController; // Forward declaration

  struct PointLightParams
  {
    std::string name      = {"PointLight"};
    float       intensity = 10.0f;
    glm::vec3   color     = {1.0f, 1.0f, 1.0f};
    float       radius    = 0.1f;
  };

  struct DirectionalLightParams
  {
    std::string name      = {"DirectionalLight"};
    float       intensity = 1.0f;
    glm::vec3   color     = {1.0f, 1.0f, 1.0f};
  };

  struct SpotLightParams
  {
    std::string name       = {"SpotLight"};
    float       intensity  = 10.0f;
    glm::vec3   color      = {1.0f, 1.0f, 1.0f};
    float       innerAngle = 12.5f;
    float       outerAngle = 17.5f;
  };

  struct PBRObjectParams
  {
    std::string            name = {"PBRObject"};
    std::shared_ptr<Model> model;
    glm::vec3              albedo    = {1.0f, 1.0f, 1.0f};
    float                  metallic  = 0.0f;
    float                  roughness = 0.5f;
    float                  ao        = 1.0f;
  };

  class GameObject
  {
  public:
    using id_t = unsigned int;
    using Map  = std::unordered_map<id_t, GameObject>;

    TransformComponent transform{};
    glm::vec3          color{};
    id_t               id{0};

    // Optional pointer components (Legacy - prefer using addComponent<T>)
    std::shared_ptr<Model> model{};

    // Generic Component System
    std::vector<std::unique_ptr<Component>> components;

    template <typename T, typename... Args> T& addComponent(Args&&... args)
    {
      auto component = std::make_unique<T>(std::forward<Args>(args)...);
      component->setOwner(this);
      T& ref = *component;
      components.push_back(std::move(component));
      return ref;
    }

    template <typename T> T* getComponent() const
    {
      for (const auto& component : components)
      {
        if (auto casted = dynamic_cast<T*>(component.get()))
        {
          return casted;
        }
      }
      return nullptr;
    }

    template <typename T> bool hasComponent() const { return getComponent<T>() != nullptr; }

    static GameObject create(std::string name = "GameObject")
    {
      static id_t currentId = 0;
      return GameObject{currentId++, std::move(name)};
    }

    // delete copy operations
    GameObject(const GameObject&)            = delete;
    GameObject& operator=(const GameObject&) = delete;

    // Custom move operations to update component owners
    GameObject(GameObject&& other) noexcept;
    GameObject& operator=(GameObject&& other) noexcept;

    id_t getId() const { return id; }

    // Get bounding sphere for frustum culling (center in world space, radius)
    void getBoundingSphere(glm::vec3& center, float& radius) const;

    static GameObject  makePointLightObject(const PointLightParams& params);
    static GameObject  makeDirectionalLightObject(const DirectionalLightParams& params);
    static GameObject  makeSpotLightObject(const SpotLightParams& params);
    static GameObject  makePBRObject(const PBRObjectParams& params);
    void               setName(const std::string& newName) { name = newName; }
    const std::string& getName() const { return name; }

  private:
    explicit GameObject(id_t objId, std::string objName) : id{objId}, name(std::move(objName)) {}
    std::string name;
  };

} // namespace engine
