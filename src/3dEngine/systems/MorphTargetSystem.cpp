#include "3dEngine/systems/MorphTargetSystem.hpp"

#include <iostream>

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
    for (auto& [id, obj] : frameInfo.gameObjects)
    {
      if (obj.model && obj.model->hasMorphTargets())
      {
        // Initialize morph targets for newly added models at runtime
        if (!manager_->isModelInitialized(obj.model.get()))
        {
          try
          {
            manager_->initializeModel(obj.model);
          }
          catch (const std::exception& e)
          {
            std::cerr << "[MorphTargetSystem] ERROR initializing object " << id << ": " << e.what() << std::endl;
            continue; // Skip this object
          }
        }

        // Dispatch compute shader: baseVertices + morphDeltas * weights → blendedVertices
        manager_->updateAndBlend(frameInfo.commandBuffer, obj.model);
      }
    }
  }

} // namespace engine
