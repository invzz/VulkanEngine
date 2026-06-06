#include "Engine/Systems/LODSystem.hpp"

#include <limits>
#include <memory>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/LODComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"

namespace engine {

    void LODSystem::update(FrameInfo& frameInfo) {
        glm::vec3 const cameraPos = frameInfo.camera.getPosition();

        auto view = frameInfo.scene->getRegistry().view<LODComponent, TransformComponent, ModelComponent>();
        for (auto entity : view) {
            auto [lod, transform, modelComp] = view.get<LODComponent, TransformComponent, ModelComponent>(entity);
            if (lod.levels.empty())
                continue;

            float const            distance      = glm::length(transform.translation - cameraPos);
            std::shared_ptr<Model> selectedModel = nullptr;
            float                  maxDistFound  = -1.0f;

            for (const auto& level : lod.levels) {
                if (distance >= level.distance) {
                    if (level.distance > maxDistFound) {
                        maxDistFound  = level.distance;
                        selectedModel = level.model;
                    }
                }
            }

            if (!selectedModel) {
                float minDist = std::numeric_limits<float>::max();
                for (const auto& level : lod.levels) {
                    if (level.distance < minDist) {
                        minDist       = level.distance;
                        selectedModel = level.model;
                    }
                }
            }

            if (selectedModel && modelComp.model != selectedModel) {
                modelComp.model = selectedModel;
            }
        }
    }

}  // namespace engine
