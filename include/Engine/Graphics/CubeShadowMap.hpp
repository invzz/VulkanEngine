#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <memory>

#include "Engine/Graphics/Device.hpp"

namespace engine {

  /**
   * @brief Cube shadow map for omnidirectional point light shadows
   *
   * Creates a depth cube map (6 faces) for point light shadow mapping.
   * Each face captures depth from the light's position in one direction.
   */
  class CubeShadowMap
  {
  public:
    CubeShadowMap(Device& device, uint32_t size = 1024);
    ~CubeShadowMap();

    CubeShadowMap(const CubeShadowMap&)            = delete;
    CubeShadowMap& operator=(const CubeShadowMap&) = delete;

    VkRenderPass getRenderPass() const { return renderPass_; }
    VkImageView  getCubeImageView() const { return cubeImageView_; }
    VkSampler    getSampler() const { return sampler_; }

    uint32_t getSize() const { return size_; }

    /**
     * @brief Get framebuffer for a specific cube face
     * @param face Face index (0-5: +X, -X, +Y, -Y, +Z, -Z)
     */
    VkFramebuffer getFramebuffer(int face) const { return framebuffers_[face]; }

    /**
     * @brief Get view matrix for a specific cube face
     * @param lightPos Position of the point light
     * @param face Face index (0-5)
     */
    static glm::mat4 getFaceViewMatrix(const glm::vec3& lightPos, int face);

    /**
     * @brief Get projection matrix for cube shadow map
     * @param nearPlane Near plane distance
     * @param farPlane Far plane distance (light range)
     */
    static glm::mat4 getProjectionMatrix(float nearPlane, float farPlane);

    VkDescriptorImageInfo getDescriptorInfo() const
    {
      return VkDescriptorImageInfo{
              .sampler     = sampler_,
              .imageView   = cubeImageView_,
              .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
      };
    }

    /**
     * @brief Transition all faces to attachment optimal before rendering
     * Call this ONCE before rendering all 6 faces
     */
    void transitionToAttachmentLayout(VkCommandBuffer commandBuffer);

    /**
     * @brief Transition all faces to shader read layout after rendering
     * Call this ONCE after rendering all 6 faces
     */
    void transitionToShaderReadLayout(VkCommandBuffer commandBuffer);

    /**
     * @brief Begin render pass for a specific cube face
     */
    void beginRenderPass(VkCommandBuffer commandBuffer, int face);

    /**
     * @brief End render pass
     */
    void endRenderPass(VkCommandBuffer commandBuffer);

  private:
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();
    void createSampler();

    Device& device_;

    uint32_t size_;

    VkImage        depthImage_        = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_  = VK_NULL_HANDLE;
    VkImageView    cubeImageView_     = VK_NULL_HANDLE;   // View for the entire cube
    VkImageView    faceImageViews_[6] = {VK_NULL_HANDLE}; // Views for each face
    VkSampler      sampler_           = VK_NULL_HANDLE;
    VkRenderPass   renderPass_        = VK_NULL_HANDLE;
    VkFramebuffer  framebuffers_[6]   = {VK_NULL_HANDLE};
    VkFormat       depthFormat_       = VK_FORMAT_D32_SFLOAT;

    VkImage getImage() const { return depthImage_; }
  };

} // namespace engine
