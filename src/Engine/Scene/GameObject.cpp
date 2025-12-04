#include "Engine/Scene/GameObject.hpp"

#include "Engine/Scene/AnimationController.hpp"

namespace engine {

  void GameObject::getBoundingSphere(glm::vec3& center, float& radius) const
  {
    center = transform.translation;
    // Simple approximation: use maximum scale component as radius
    radius = glm::max(glm::max(transform.scale.x, transform.scale.y), transform.scale.z) * 5.0f; // Conservative multiplier
  }

  GameObject GameObject::makePointLightObject(const PointLightParams& params)
  {
    GameObject obj        = GameObject::create(params.name);
    obj.color             = params.color;
    obj.transform.scale.x = params.radius;
    auto& pointLight      = obj.addComponent<PointLightComponent>();
    pointLight.intensity  = params.intensity;
    return obj;
  }

  GameObject GameObject::makeDirectionalLightObject(const DirectionalLightParams& params)
  {
    GameObject obj     = GameObject::create(params.name);
    obj.color          = params.color;
    auto& dirLight     = obj.addComponent<DirectionalLightComponent>();
    dirLight.intensity = params.intensity;
    return obj;
  }

  GameObject GameObject::makeSpotLightObject(const SpotLightParams& params)
  {
    GameObject obj             = GameObject::create(params.name);
    obj.color                  = params.color;
    obj.transform.scale.x      = 0.1f; // Visual size
    auto& spotLight            = obj.addComponent<SpotLightComponent>();
    spotLight.intensity        = params.intensity;
    spotLight.innerCutoffAngle = params.innerAngle;
    spotLight.outerCutoffAngle = params.outerAngle;
    return obj;
  }

  GameObject GameObject::makePBRObject(const PBRObjectParams& params)
  {
    GameObject obj        = GameObject::create(params.name);
    obj.model             = params.model;
    auto& pbrMaterial     = obj.addComponent<PBRMaterial>();
    pbrMaterial.albedo    = params.albedo;
    pbrMaterial.metallic  = params.metallic;
    pbrMaterial.roughness = params.roughness;
    pbrMaterial.ao        = params.ao;
    return obj;
  }

  GameObject::GameObject(GameObject&& other) noexcept
      : transform(std::move(other.transform)), color(std::move(other.color)), id(other.id), model(std::move(other.model)),
        components(std::move(other.components)), name(std::move(other.name))
  {
    for (auto& component : components)
    {
      if (component) component->setOwner(this);
    }
  }

  GameObject& GameObject::operator=(GameObject&& other) noexcept
  {
    if (this != &other)
    {
      transform  = std::move(other.transform);
      color      = std::move(other.color);
      id         = other.id;
      model      = std::move(other.model);
      components = std::move(other.components);
      name       = std::move(other.name);

      for (auto& component : components)
      {
        if (component) component->setOwner(this);
      }
    }
    return *this;
  }

} // namespace engine
