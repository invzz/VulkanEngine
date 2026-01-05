#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SHADOWMAP_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SHADOWMAP_HPP

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <memory>

#include "Engine/Graphics/Device.hpp"

namespace engine {

  /**
   * @brief Shadow map for depth-only rendering from light's perspective
   *
   * Creates a depth-only framebuffer and render pass for shadow mapping.
   * Supports directional, point, and spot lights.
   */
  class ShadowMap
  {
  public:
    ShadowMap(Device& device, uint32_t width = 2048, uint32_t height = 2048);
    ~ShadowMap();

    ShadowMap(const ShadowMap&)            = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    [[nodiscard]] VkRenderPass  getRenderPass() const { return renderPass_; }
    [[nodiscard]] VkFramebuffer getFramebuffer() const { return framebuffer_; }
    [[nodiscard]] VkImageView   getImageView() const { return depthImageView_; }
    [[nodiscard]] VkSampler     getSampler() const { return sampler_; }

    [[nodiscard]] uint32_t getWidth() const { return width_; }
    [[nodiscard]] uint32_t getHeight() const { return height_; }

    [[nodiscard]] VkDescriptorImageInfo getDescriptorInfo() const
    {
      return VkDescriptorImageInfo{
              .sampler     = sampler_,
              .imageView   = depthImageView_,
              .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
      };
    }

    /**
     * @brief Begin shadow map render pass
     */
    void beginRenderPass(VkCommandBuffer commandBuffer);

    /**
     * @brief End shadow map render pass
     */
    void endRenderPass(VkCommandBuffer commandBuffer);

  private:
    void createDepthResources();
    void createRenderPass();
    void createFramebuffer();
    void createSampler();

    Device& device_;

    uint32_t width_;
    uint32_t height_;

    VkImage        depthImage_       = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView    depthImageView_   = VK_NULL_HANDLE;
    VkSampler      sampler_          = VK_NULL_HANDLE;
    VkRenderPass   renderPass_       = VK_NULL_HANDLE;
    VkFramebuffer  framebuffer_      = VK_NULL_HANDLE;
    VkFormat       depthFormat_      = VK_FORMAT_D32_SFLOAT;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SHADOWMAP_HPP
