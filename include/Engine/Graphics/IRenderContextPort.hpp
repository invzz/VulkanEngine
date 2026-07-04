#pragma once
#include <vulkan/vulkan_core.h>

#include <cstdint>
namespace engine {
    class Scene;
    struct GlobalUbo;
    struct GlobalUboCold;
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
    class RenderContextAdapter : public IRenderContextPort {
       public:
        explicit RenderContextAdapter(class RenderContext* ctx) : ctx_(ctx) {}
        LightCounts           updateLightBuffers(int frameIndex, Scene& scene) override;
        void                  updateUBO(int frameIndex, const GlobalUbo& ubo, const GlobalUboCold& uboCold) override;
        VkDescriptorSet       getGlobalDescriptorSet(int frameIndex) override;
        VkDescriptorSetLayout getGlobalSetLayout() override;

       private:
        class RenderContext* ctx_;
    };
}  // namespace engine
