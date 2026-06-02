
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
  if (engineState_->getObjectSelectionSystem() != nullptr) engineState_->getObjectSelectionSystem()->update(frameInfo);
  if (engineState_->getInputSystem() != nullptr) engineState_->getInputSystem()->update(frameInfo);
  LODSystem::update(frameInfo);
  CameraSystem::update(frameInfo, renderer.getAspectRatio());

    // --- Physics (requires explicit Play from UI) ---
    if (engineState_->physicsSimulationRunningRef()) {
      if (engineState_->getJoltPhysicsSystem() != nullptr) {
        engineState_->getJoltPhysicsSystem()->syncToEntities(frameInfo.scene);
        engineState_->getJoltPhysicsSystem()->update(frameInfo.frameTime, 8, 1.0f / 120.0f);
        engineState_->getJoltPhysicsSystem()->syncToEntities(frameInfo.scene);
      } else {
        // legacy PhysicsSystem::update (kept for compatibility)
        PhysicsSystem::update(frameInfo);
      }
  }
}

}  // namespace engine