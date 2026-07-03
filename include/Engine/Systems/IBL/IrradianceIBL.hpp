#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBL_IRRADIANCEIBL_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBL_IRRADIANCEIBL_HPP

#include <vulkan/vulkan.h>

#include "Engine/Systems/IBL/IBLSettings.hpp"

namespace engine {

    class Device;
    class Skybox;

    namespace ibl {

        class IrradianceIBL {
           public:
            explicit IrradianceIBL(Device& device);
            ~IrradianceIBL();

            IrradianceIBL(const IrradianceIBL&)            = delete;
            IrradianceIBL& operator=(const IrradianceIBL&) = delete;

            void createFallback();
            void resetToUninitialized();

            void createForSettings(const Settings& settings);
            void ensurePipelineResources();
            void generateFromSkybox(Skybox& skybox, const Settings& settings);

            [[nodiscard]] VkDescriptorImageInfo getDescriptorInfo() const;

            [[nodiscard]] VkImage image() const {
                return image_;
            }
            [[nodiscard]] VkImageView imageView() const {
                return imageView_;
            }
            [[nodiscard]] VkSampler sampler() const {
                return sampler_;
            }

            void adoptLoaded(VkImage image, VkDeviceMemory memory, VkImageView imageView, VkSampler sampler);

            void deferDestroyImageResources();

            void destroyImmediate();

           private:
            Device& device_;

            VkImage        image_     = VK_NULL_HANDLE;
            VkDeviceMemory memory_    = VK_NULL_HANDLE;
            VkImageView    imageView_ = VK_NULL_HANDLE;
            VkSampler      sampler_   = VK_NULL_HANDLE;

            VkRenderPass          renderPass_     = VK_NULL_HANDLE;
            VkPipelineLayout      pipelineLayout_ = VK_NULL_HANDLE;
            VkPipeline            pipeline_       = VK_NULL_HANDLE;
            VkDescriptorSetLayout descSetLayout_  = VK_NULL_HANDLE;
            VkDescriptorPool      descPool_       = VK_NULL_HANDLE;
            VkDescriptorSet       descSet_        = VK_NULL_HANDLE;
        };

    }  // namespace ibl

}  // namespace engine

#endif
