#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <unordered_map>

#include "Model.hpp"
#include "PBRMaterial.hpp"

namespace engine {

  class AnimationController; // Forward declaration

  struct TransformComponent
  {
    glm::vec3 translation{};               // position offset
    glm::vec3 scale{1.0f, 1.0f, 1.0f};     // scaling factors
    glm::vec3 rotation{};                  // rotation angles in radians
    glm::vec3 baseScale{1.0f, 1.0f, 1.0f}; // Base scale (for animated objects, multiplied with animation scale)

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

  struct DirectionalLightComponent
  {
    float intensity{1.0f};
  };

  struct SpotLightComponent
  {
    float intensity{1.0f};
    float innerCutoffAngle{12.5f}; // Inner cone angle in degrees
    float outerCutoffAngle{17.5f}; // Outer cone angle in degrees
    float constantAttenuation{1.0f};
    float linearAttenuation{0.09f};
    float quadraticAttenuation{0.032f};
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
    std::shared_ptr<Model>                     model{};
    std::unique_ptr<PointLightComponent>       pointLight          = nullptr;
    std::unique_ptr<DirectionalLightComponent> directionalLight    = nullptr;
    std::unique_ptr<SpotLightComponent>        spotLight           = nullptr;
    std::unique_ptr<PBRMaterial>               pbrMaterial         = nullptr;
    std::unique_ptr<AnimationController>       animationController = nullptr;

    static GameObject create(std::string name = "GameObject")
    {
      static id_t currentId = 0;
      return GameObject{currentId++, std::move(name)};
    }

    // delete copy operations
    GameObject(const GameObject&)            = delete;
    GameObject& operator=(const GameObject&) = delete;

    // default move operations
    GameObject(GameObject&&) noexcept            = default;
    GameObject& operator=(GameObject&&) noexcept = default;

    id_t getId() const { return id; }

    // Get bounding sphere for frustum culling (center in world space, radius)
    void getBoundingSphere(glm::vec3& center, float& radius) const
    {
      center = transform.translation;
      // Simple approximation: use maximum scale component as radius
      radius = glm::max(glm::max(transform.scale.x, transform.scale.y), transform.scale.z) * 5.0f; // Conservative multiplier
    }

    static GameObject makePointLightObject(float intensity = 10.f, const glm::vec3& color = {1.f, 1.f, 1.f}, float radius = 0.1f)
    {
      GameObject obj            = GameObject::create();
      obj.color                 = color;
      obj.transform.scale.x     = radius;
      obj.pointLight            = std::make_unique<PointLightComponent>();
      obj.pointLight->intensity = intensity;
      return obj;
    }

    static GameObject makeDirectionalLightObject(float intensity = 1.0f, const glm::vec3& color = {1.f, 1.f, 1.f})
    {
      GameObject obj                  = GameObject::create();
      obj.color                       = color;
      obj.directionalLight            = std::make_unique<DirectionalLightComponent>();
      obj.directionalLight->intensity = intensity;
      return obj;
    }

    static GameObject makeSpotLightObject(float intensity = 10.f, const glm::vec3& color = {1.f, 1.f, 1.f}, float innerAngle = 12.5f, float outerAngle = 17.5f)
    {
      GameObject obj                  = GameObject::create();
      obj.color                       = color;
      obj.transform.scale.x           = 0.1f; // Visual size
      obj.spotLight                   = std::make_unique<SpotLightComponent>();
      obj.spotLight->intensity        = intensity;
      obj.spotLight->innerCutoffAngle = innerAngle;
      obj.spotLight->outerCutoffAngle = outerAngle;
      return obj;
    }

    static GameObject
    makePBRObject(std::shared_ptr<Model> model, const glm::vec3& albedo = {1.0f, 1.0f, 1.0f}, float metallic = 0.0f, float roughness = 0.5f, float ao = 1.0f)
    {
      GameObject obj             = GameObject::create();
      obj.model                  = model;
      obj.pbrMaterial            = std::make_unique<PBRMaterial>();
      obj.pbrMaterial->albedo    = albedo;
      obj.pbrMaterial->metallic  = metallic;
      obj.pbrMaterial->roughness = roughness;
      obj.pbrMaterial->ao        = ao;
      return obj;
    }

  private:
    explicit GameObject(id_t objId, std::string objName) : id{objId}, name(std::move(objName)) {}
    std::string name;
  };

} // namespace engine
