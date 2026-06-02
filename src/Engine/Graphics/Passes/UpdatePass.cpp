
#include "Engine/Graphics/Passes/UpdatePass.hpp"

#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace engine {

void UpdatePass::execute(FrameInfo& frameInfo) {
  // forward update calls to systems owned by EngineState
  if (inputState_.objectSelectionSystem != nullptr) inputState_.objectSelectionSystem->update(frameInfo);
  if (inputState_.inputSystem != nullptr) inputState_.inputSystem->update(frameInfo);
  LODSystem::update(frameInfo);
  CameraSystem::update(frameInfo, renderer.getAspectRatio());

    // --- Physics (requires explicit Play from UI) ---
    if (physicsPort_ != nullptr && physicsPort_->physicsSimulationRunningRef()) {
      auto* joltPhysics = physicsPort_->joltPhysicsSystem();
      if (joltPhysics != nullptr) {
        joltPhysics->syncToEntities(frameInfo.scene);
        joltPhysics->update(frameInfo.frameTime, 8, 1.0f / 120.0f);
        joltPhysics->syncToEntities(frameInfo.scene);
      } else {
        // legacy PhysicsSystem::update (kept for compatibility)
        PhysicsSystem::update(frameInfo);
      }
  }
}

}  // namespace engine