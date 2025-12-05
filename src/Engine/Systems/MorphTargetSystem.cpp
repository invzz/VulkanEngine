#include "Engine/Systems/MorphTargetSystem.hpp"

#include <iostream>

#include "Engine/Scene/components/ModelComponent.hpp"

namespace engine {

  MorphTargetSystem::MorphTargetSystem(Device& device)
  {
    try
    {
      manager_ = std::make_unique<MorphTargetManager>(device);
      std::cout << "[MorphTargetSystem] Initialized successfully" << std::endl;
    }
    catch (const std::exception& e)
    {
      std::cerr << "[MorphTargetSystem] ERROR: " << e.what() << std::endl;
      throw;
    }
  }

  MorphTargetSystem::~MorphTargetSystem() = default;

  void MorphTargetSystem::update(FrameInfo& frameInfo)
  {
    if (!manager_)
    {
      return;
    }

    // Update morph targets for all models that have them
    auto view = frameInfo.scene->getRegistry().view<ModelComponent>();
    for (auto entity : view)
    {
      auto& modelComp = view.get<ModelComponent>(entity);
      if (modelComp.model && modelComp.model->hasMorphTargets())
      {
        // Initialize morph targets for newly added models at runtime
        if (!manager_->isModelInitialized(modelComp.model.get()))
        {
          try
          {
            manager_->initializeModel(modelComp.model);
          }
          catch (const std::exception& e)
          {
            std::cerr << "[MorphTargetSystem] ERROR initializing object " << (uint32_t)entity << ": " << e.what() << std::endl;
            continue; // Skip this object
          }
        }

        // Dispatch compute shader: baseVertices + morphDeltas * weights → blendedVertices
        manager_->updateAndBlend(frameInfo.commandBuffer, modelComp.model);
      }
    }
  }

} // namespace engine
