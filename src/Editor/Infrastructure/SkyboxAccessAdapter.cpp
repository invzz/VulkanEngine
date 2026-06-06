#include "Editor/Infrastructure/SkyboxAccessAdapter.hpp"

#include <iostream>
#include <string>

#include "Engine/EngineState.hpp"
#include "Engine/Scene/Skybox.hpp"

namespace engine {

    SkyboxAccessAdapter::SkyboxAccessAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    void SkyboxAccessAdapter::setSkybox(Device& device, const char* folderPath, const char* extension) {
        std::cout << "[SkyboxAccessAdapter] Loading skybox from: " << folderPath << '\n';
        engineState_.skyboxRef() = Skybox::loadFromFolder(device, std::string(folderPath), extension);
    }

    void SkyboxAccessAdapter::resetSkybox() {
        std::cout << "[SkyboxAccessAdapter] Resetting skybox.\n";
        engineState_.skyboxRef().reset();
    }

    bool SkyboxAccessAdapter::hasSkybox() const {
        return engineState_.sceneRuntimeService().view().skybox != nullptr;
    }

}  // namespace engine
