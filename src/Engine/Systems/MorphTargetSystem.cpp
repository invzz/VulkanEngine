#include "Engine/Systems/MorphTargetSystem.hpp"

#include <exception>
#include <iostream>
#include <memory>

#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"

#include "ModelLib/Resources/MorphTargetManager.hpp"
namespace engine {
    MorphTargetSystem::MorphTargetSystem(Device& device) {
        try {
            manager_ = std::make_unique<MorphTargetManager>(device);
            engine::Logger::info(engine::LogChannel::Render, "[MorphTargetSystem] Initialized successfully");
        } catch (const std::exception& e) {
            engine::Logger::error(engine::LogChannel::Render, "[MorphTargetSystem] ERROR: ", e.what());
            throw;
        }
    }
    MorphTargetSystem::~MorphTargetSystem() = default;
    void MorphTargetSystem::update(FrameInfo& frameInfo) {
        if (!manager_) {
            return;
        }
        auto view = frameInfo.scene->getRegistry().view<ModelComponent>();
        for (auto entity : view) {
            auto& modelComp = view.get<ModelComponent>(entity);
            if (modelComp.model && modelComp.model->hasMorphTargets()) {
                if (!manager_->isModelInitialized(modelComp.model.get())) {
                    try {
                        manager_->initializeModel(modelComp.model);
                    } catch (const std::exception& e) {
                        engine::Logger::error(engine::LogChannel::Render, "[MorphTargetSystem] ERROR initializing object ", (uint32_t) entity, ": ", e.what());
                        continue;
                    }
                }
                manager_->updateAndBlend(frameInfo.commandBuffer, modelComp.model);
            }
        }
    }
}  // namespace engine
