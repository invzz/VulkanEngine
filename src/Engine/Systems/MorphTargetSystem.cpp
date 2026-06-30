#include "Engine/Systems/MorphTargetSystem.hpp"

#include <exception>
#include <iostream>
#include <memory>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"

#include "ModelLib/Resources/MorphTargetManager.hpp"

namespace engine {

    MorphTargetSystem::MorphTargetSystem(Device& device) {
        try {
            manager_ = std::make_unique<MorphTargetManager>(device);
            std::cout << "[MorphTargetSystem] Initialized successfully" << '\n';
        } catch (const std::exception& e) {
            std::cerr << "[MorphTargetSystem] ERROR: " << e.what() << '\n';
            throw;
        }
    }

    MorphTargetSystem::~MorphTargetSystem() = default;

    void MorphTargetSystem::update(FrameInfo& frameInfo) {
        if (!manager_) {
            return;
        }

        // Update morph targets for all models that have them
        auto view = frameInfo.scene->getRegistry().view<ModelComponent>();
        for (auto entity : view) {
            auto& modelComp = view.get<ModelComponent>(entity);
            if (modelComp.model && modelComp.model->hasMorphTargets()) {
                // Initialize morph targets for newly added models at runtime
                if (!manager_->isModelInitialized(modelComp.model.get())) {
                    try {
                        manager_->initializeModel(modelComp.model);
                    } catch (const std::exception& e) {
                        std::cerr << "[MorphTargetSystem] ERROR initializing object " << (uint32_t) entity << ": " << e.what() << '\n';
                        continue;  // Skip this object
                    }
                }

                // Dispatch compute shader: baseVertices + morphDeltas * weights →
                // blendedVertices
                manager_->updateAndBlend(frameInfo.commandBuffer, modelComp.model);
            }
        }
    }

}  // namespace engine
