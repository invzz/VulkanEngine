#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBL_BRDFLUT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBL_BRDFLUT_HPP

#include <vulkan/vulkan.h>

#include "Engine/Systems/IBL/IBLSettings.hpp"

namespace engine {

  class Device;

  namespace ibl {

    class BRDFLUT
    {
    public:
      explicit BRDFLUT(Device& device);
      ~BRDFLUT();

      BRDFLUT(const BRDFLUT&)            = delete;
      BRDFLUT& operator=(const BRDFLUT&) = delete;

      void createFallback();
      void resetToUninitialized();

      void ensureForSettings(const Settings& settings);

      [[nodiscard]] VkDescriptorImageInfo getDescriptorInfo() const;

      // For VTEX I/O
      [[nodiscard]] VkImage     image() const { return image_; }
      [[nodiscard]] VkImageView imageView() const { return imageView_; }
      [[nodiscard]] VkSampler   sampler() const { return sampler_; }
      [[nodiscard]] int         currentSize() const { return currentSize_; }

      void adoptLoaded(VkImage image, VkDeviceMemory memory, VkImageView imageView, VkSampler sampler, int size);

      void deferDestroyImageResources();
      void destroyImmediate();

    private:
      void createForSettings(const Settings& settings);
      void ensurePipelineResources();
      void generate(const Settings& settings);

      Device& device_;

      VkImage        image_     = VK_NULL_HANDLE;
      VkDeviceMemory memory_    = VK_NULL_HANDLE;
      VkImageView    imageView_ = VK_NULL_HANDLE;
      VkSampler      sampler_   = VK_NULL_HANDLE;

      int currentSize_ = 0;

      VkPipelineLayout      pipelineLayout_ = VK_NULL_HANDLE;
      VkPipeline            pipeline_       = VK_NULL_HANDLE;
      VkDescriptorSetLayout descSetLayout_  = VK_NULL_HANDLE;
      VkDescriptorPool      descPool_       = VK_NULL_HANDLE;
      VkDescriptorSet       descSet_        = VK_NULL_HANDLE;
    };

  } // namespace ibl

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBL_BRDFLUT_HPP
