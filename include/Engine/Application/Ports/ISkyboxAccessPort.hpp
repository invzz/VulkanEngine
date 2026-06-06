#pragma once

#include <vulkan/vulkan_core.h>

#include <memory>

namespace engine {

    class Device;
    class Skybox;

    // Port for skybox access without knowing EngineState internals.
    // Allows setting, getting, and resetting the scene's skybox resource.
    class ISkyboxAccessPort {
       public:
        virtual ~ISkyboxAccessPort() = default;

        // Set the skybox from a folder path (returns unique_ptr for caller to manage)
        virtual void setSkybox(Device& device, const char* folderPath, const char* extension) = 0;

        // Reset the skybox to null/empty state
        virtual void resetSkybox() = 0;

        // Check if a skybox is currently loaded
        [[nodiscard]] virtual bool hasSkybox() const = 0;
    };

}  // namespace engine
