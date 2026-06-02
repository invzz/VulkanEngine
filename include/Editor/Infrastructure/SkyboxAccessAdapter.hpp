#pragma once

#include "Engine/Application/Ports/ISkyboxAccessPort.hpp"

namespace engine {

class Device;
class EngineState;

// Adapter that bridges EngineState to the skybox access port.
class SkyboxAccessAdapter final : public ISkyboxAccessPort {
 public:
  explicit SkyboxAccessAdapter(EngineState& engineState);

  void setSkybox(Device& device, const char* folderPath, const char* extension) override;
  void resetSkybox() override;
  [[nodiscard]] bool hasSkybox() const override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
