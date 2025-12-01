#pragma once

#include <vector>

#include "../Camera.hpp"
#include "../FrameInfo.hpp"
#include "../GameObject.hpp"

namespace engine {

  /**
   * @brief Handles frustum culling and object sorting for rendering
   *
   * Provides utilities for:
   * - Frustum culling (skipping objects outside camera view)
   * - Distance-based sorting
   * - Material-based sorting to minimize state changes
   */
  class RenderCullingSystem
  {
  public:
    struct RenderableObject
    {
      GameObject::id_t  id;
      const GameObject* obj;
      float             distanceToCamera;

      // For sorting optimization
      const void* sortKey; // Could be material pointer, shader, etc.
    };

    /**
     * @brief Perform frustum culling on objects
     * @param frameInfo Current frame information
     * @param outVisible Vector to fill with visible objects
     * @param sortByMaterial Whether to sort by material to reduce state changes
     */
    static void cullAndSort(FrameInfo& frameInfo, std::vector<RenderableObject>& outVisible, bool sortByMaterial = true);

    /**
     * @brief Check if a sphere is in the camera frustum
     */
    static bool isVisible(const Camera& camera, const glm::vec3& center, float radius);

  private:
    RenderCullingSystem() = delete; // Static class
  };

} // namespace engine
