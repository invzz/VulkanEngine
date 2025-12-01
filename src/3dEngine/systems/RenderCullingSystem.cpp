#include "3dEngine/systems/RenderCullingSystem.hpp"

#include <algorithm>

#include "3dEngine/GameObjectManager.hpp"

namespace engine {

  void RenderCullingSystem::cullAndSort(FrameInfo& frameInfo, std::vector<RenderableObject>& outVisible, bool sortByMaterial)
  {
    outVisible.clear();
    outVisible.reserve(frameInfo.objectManager->getAllObjects().size());

    // Frustum culling pass
    for (const auto& [id, gameObject] : frameInfo.objectManager->getAllObjects())
    {
      // Skip objects without models or materials
      if (!gameObject.model) continue;

      bool isMultiMaterial = gameObject.model->hasMultipleMaterials();
      bool hasPBRMaterial  = gameObject.pbrMaterial != nullptr;

      if (!isMultiMaterial && !hasPBRMaterial) continue;

      // Get bounding sphere
      glm::vec3 center;
      float     radius;
      gameObject.getBoundingSphere(center, radius);

      // Frustum culling
      if (!isVisible(frameInfo.camera, center, radius)) continue;

      // Calculate distance to camera
      float distanceToCamera = glm::length(frameInfo.camera.getPosition() - center);

      // Add to visible list
      outVisible.push_back({
              .id               = id,
              .obj              = &gameObject,
              .distanceToCamera = distanceToCamera,
              .sortKey          = hasPBRMaterial ? static_cast<const void*>(gameObject.pbrMaterial.get()) : nullptr,
      });
    }

    // Sort if requested
    if (sortByMaterial)
    {
      std::stable_sort(outVisible.begin(), outVisible.end(), [](const RenderableObject& a, const RenderableObject& b) {
        // Sort by material pointer to minimize state changes
        // Objects with sortKey (single material) come first, sorted by material
        // Objects without sortKey (multi-material) come last
        if (a.sortKey && b.sortKey)
        {
          return a.sortKey < b.sortKey;
        }
        return a.sortKey > b.sortKey;
      });
    }
  }

  bool RenderCullingSystem::isVisible(const Camera& camera, const glm::vec3& center, float radius)
  {
    return camera.isInFrustum(center, radius);
  }

} // namespace engine
