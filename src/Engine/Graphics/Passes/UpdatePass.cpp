
#include "Engine/Graphics/Passes/UpdatePass.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"

namespace engine {

void UpdatePass::execute(FrameInfo& frameInfo) {
  // forward update calls to systems owned by EngineState
  if (engineState_->objectSelectionSystem) engineState_->objectSelectionSystem->update(frameInfo);
  if (engineState_->inputSystem) engineState_->inputSystem->update(frameInfo);
  LODSystem::update(frameInfo);
  CameraSystem::update(frameInfo, renderer.getAspectRatio());

    // --- Physics (requires explicit Play from UI) ---
    if (engineState_->physicsSimulationRunning) {
      if (engineState_->joltPhysicsSystem) {
        engineState_->joltPhysicsSystem->syncToEntities(frameInfo.scene);
        engineState_->joltPhysicsSystem->update(frameInfo.frameTime, 8, 1.0f / 120.0f);
        engineState_->joltPhysicsSystem->syncToEntities(frameInfo.scene);
      } else {
        // legacy PhysicsSystem::update (kept for compatibility)
        PhysicsSystem::update(frameInfo);
      }
  }
}

}  // namespace engine