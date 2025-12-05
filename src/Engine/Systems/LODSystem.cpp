#include "Engine/Systems/LODSystem.hpp"

#include <algorithm>
#include <glm/glm.hpp>

#include "Engine/Scene/components/LODComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  void LODSystem::update(FrameInfo& frameInfo)
  {
    glm::vec3 cameraPos = frameInfo.camera.getPosition();

    auto view = frameInfo.scene->getRegistry().view<LODComponent, TransformComponent, ModelComponent>();
    for (auto entity : view)
    {
      auto [lod, transform, modelComp] = view.get<LODComponent, TransformComponent, ModelComponent>(entity);
      if (lod.levels.empty()) continue;

      float distance = glm::length(transform.translation - cameraPos);

      // Find the appropriate LOD level
      // Levels should be sorted by distance (ascending or descending?)
      // Let's assume ascending: 0, 10, 50
      // If distance < 10, use LOD0 (dist 0)
      // If distance < 50, use LOD1 (dist 10)
      // Else use LOD2 (dist 50)

      // Actually, usually LOD levels are defined as "start using this LOD at this distance".
      // So:
      // LOD0: 0
      // LOD1: 10
      // LOD2: 50

      // If dist is 5: 5 >= 0 (LOD0). Next is 10. 5 < 10. So keep LOD0.
      // If dist is 20: 20 >= 10 (LOD1). Next is 50. 20 < 50. So keep LOD1.
      // If dist is 100: 100 >= 50 (LOD2). No next. So keep LOD2.

      // So we want the level with the highest distance that is <= current distance.

      std::shared_ptr<Model> selectedModel = nullptr;

      // Iterate in reverse to find the first one that satisfies distance >= level.distance
      // Assuming levels are sorted by distance ascending.
      // If not sorted, we should sort them or just search.
      // Let's assume they might not be sorted, so we search for the best fit.

      float maxDistFound = -1.0f;

      for (const auto& level : lod.levels)
      {
        if (distance >= level.distance)
        {
          if (level.distance > maxDistFound)
          {
            maxDistFound  = level.distance;
            selectedModel = level.model;
          }
        }
      }

      // If we found a suitable LOD, use it.
      // If distance is smaller than all LODs (e.g. closest LOD starts at 10, but we are at 5),
      // we should probably use the one with smallest distance (LOD0).

      if (!selectedModel)
      {
        // Find the level with minimum distance
        float minDist = std::numeric_limits<float>::max();
        for (const auto& level : lod.levels)
        {
          if (level.distance < minDist)
          {
            minDist       = level.distance;
            selectedModel = level.model;
          }
        }
      }

      if (selectedModel && modelComp.model != selectedModel)
      {
        modelComp.model = selectedModel;
      }
    }
  }

} // namespace engine
