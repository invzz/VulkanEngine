#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace engine {

    class Scene;
    struct GlobalUbo;
    struct GlobalUboCold;

    // Port for RenderContext operations without depending on the Editor layer.
    class IRenderContextPort {
       public:
        virtual ~IRenderContextPort() = default;

        struct LightCounts {
            int point       = 0;
            int directional = 0;
            int spot        = 0;
        };

        virtual LightCounts                         updateLightBuffers(int frameIndex, Scene& scene)                              = 0;
        virtual void                                updateUBO(int frameIndex, const GlobalUbo& ubo, const GlobalUboCold& uboCold) = 0;
        [[nodiscard]] virtual VkDescriptorSet       getGlobalDescriptorSet(int frameIndex)                                        = 0;
        [[nodiscard]] virtual VkDescriptorSetLayout getGlobalSetLayout()                                                          = 0;
    };

}  // namespace engine
