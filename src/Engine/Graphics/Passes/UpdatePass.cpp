#include "Engine/Graphics/Passes/UpdatePass.hpp"

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/JoltPhysicsSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PhysicsSystem.hpp"

namespace engine {

    UpdatePass::UpdatePass(ObjectSelectionSystem* os, InputSystem* inp,
        JoltPhysicsSystem* jolt, bool& physRunning,
        Renderer& renderer)
        : objSel_(os), input_(inp), jolt_(jolt), physRunning_(physRunning), renderer_(renderer) {}

    void UpdatePass::execute(FrameInfo& frameInfo) {
        if (objSel_)
            objSel_->update(frameInfo);
        if (input_)
            input_->update(frameInfo);

        LODSystem::update(frameInfo);
        CameraSystem::update(frameInfo, renderer_.getAspectRatio());

        if (physRunning_) {
            if (jolt_) {
                jolt_->syncToEntities(frameInfo.scene);
                jolt_->update(frameInfo.frameTime, 8, 1.0f / 120.0f);
                jolt_->syncToEntities(frameInfo.scene);
            } else {
                PhysicsSystem::update(frameInfo);
            }
        }
    }

    const std::string& UpdatePass::getName() const {
        static std::string n = "Update";
        return n;
    }

}  // namespace engine
