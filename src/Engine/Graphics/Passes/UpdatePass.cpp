
#include "Engine/Graphics/Passes/UpdatePass.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"

namespace engine {

void UpdatePass::execute(FrameInfo& frameInfo) {
  auto input = engineState_->inputService().view();

  // forward update calls to systems owned by EngineState
  if (input.objectSelectionSystem != nullptr) input.objectSelectionSystem->update(frameInfo);
  if (input.inputSystem != nullptr) input.inputSystem->update(frameInfo);
  LODSystem::update(frameInfo);
  CameraSystem::update(frameInfo, renderer.getAspectRatio());

    // --- Physics (requires explicit Play from UI) ---
    if (engineState_->physicsSimulationRunningRef()) {
      auto systems = engineState_->systemServices();
      if (systems.joltPhysics != nullptr) {
        systems.joltPhysics->syncToEntities(frameInfo.scene);
        systems.joltPhysics->update(frameInfo.frameTime, 8, 1.0f / 120.0f);
        systems.joltPhysics->syncToEntities(frameInfo.scene);
      } else {
        // legacy PhysicsSystem::update (kept for compatibility)
        PhysicsSystem::update(frameInfo);
      }
  }
}

}  // namespace engine