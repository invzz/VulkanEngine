#include "3dEngine/GameObject.hpp"

#include "3dEngine/AnimationController.hpp"

namespace engine {

  void GameObject::getBoundingSphere(glm::vec3& center, float& radius) const
  {
    center = transform.translation;
    // Simple approximation: use maximum scale component as radius
    radius = glm::max(glm::max(transform.scale.x, transform.scale.y), transform.scale.z) * 5.0f; // Conservative multiplier
  }

  GameObject GameObject::makePointLightObject(const PointLightParams& params)
  {
    GameObject obj            = GameObject::create(params.name);
    obj.color                 = params.color;
    obj.transform.scale.x     = params.radius;
    obj.pointLight            = std::make_unique<PointLightComponent>();
    obj.pointLight->intensity = params.intensity;
    return obj;
  }

  GameObject GameObject::makeDirectionalLightObject(const DirectionalLightParams& params)
  {
    GameObject obj                  = GameObject::create(params.name);
    obj.color                       = params.color;
    obj.directionalLight            = std::make_unique<DirectionalLightComponent>();
    obj.directionalLight->intensity = params.intensity;
    return obj;
  }

  GameObject GameObject::makeSpotLightObject(const SpotLightParams& params)
  {
    GameObject obj                  = GameObject::create(params.name);
    obj.color                       = params.color;
    obj.transform.scale.x           = 0.1f; // Visual size
    obj.spotLight                   = std::make_unique<SpotLightComponent>();
    obj.spotLight->intensity        = params.intensity;
    obj.spotLight->innerCutoffAngle = params.innerAngle;
    obj.spotLight->outerCutoffAngle = params.outerAngle;
    return obj;
  }

  GameObject GameObject::makePBRObject(const PBRObjectParams& params)
  {
    GameObject obj             = GameObject::create(params.name);
    obj.model                  = params.model;
    obj.pbrMaterial            = std::make_unique<PBRMaterial>();
    obj.pbrMaterial->albedo    = params.albedo;
    obj.pbrMaterial->metallic  = params.metallic;
    obj.pbrMaterial->roughness = params.roughness;
    obj.pbrMaterial->ao        = params.ao;
    return obj;
  }

} // namespace engine
