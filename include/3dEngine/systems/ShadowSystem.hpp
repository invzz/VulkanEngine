#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "3dEngine/CubeShadowMap.hpp"
#include "3dEngine/Device.hpp"
#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/Pipeline.hpp"
#include "3dEngine/ShadowMap.hpp"

namespace engine {

  /**
   * @brief System for rendering shadow maps from light perspectives
   *
   * Manages shadow map rendering for directional, spot, and point lights.
   * Uses 2D shadow maps for directional/spot lights and cube maps for point lights.
   */
  class ShadowSystem
  {
  public:
    static constexpr int MAX_SHADOW_MAPS      = 4; // For directional + spotlights
    static constexpr int MAX_CUBE_SHADOW_MAPS = 4; // For point lights (cube maps)

    ShadowSystem(Device& device, uint32_t shadowMapSize = 2048);
    ~ShadowSystem();

    ShadowSystem(const ShadowSystem&)            = delete;
    ShadowSystem& operator=(const ShadowSystem&) = delete;

    /**
     * @brief Render all shadow maps for the frame
     * @param frameInfo Current frame information
     * @param sceneRadius Approximate scene bounds for light frustum calculation
     */
    void renderShadowMaps(FrameInfo& frameInfo, float sceneRadius = 20.0f);

    /**
     * @brief Get the shadow map at specified index for sampling
     */
    ShadowMap& getShadowMap(int index = 0) { return *shadowMaps_[index]; }

    /**
     * @brief Get the cube shadow map at specified index for point lights
     */
    CubeShadowMap& getCubeShadowMap(int index = 0) { return *cubeShadowMaps_[index]; }

    /**
     * @brief Get the light space matrix at specified index
     */
    const glm::mat4& getLightSpaceMatrix(int index = 0) const { return lightSpaceMatrices_[index]; }

    /**
     * @brief Get number of active shadow-casting directional/spot lights
     */
    int getShadowLightCount() const { return shadowLightCount_; }

    /**
     * @brief Get number of active shadow-casting point lights
     */
    int getCubeShadowLightCount() const { return cubeShadowLightCount_; }

    /**
     * @brief Get point light position for shadow calculation in shader
     */
    const glm::vec3& getPointLightPosition(int index = 0) const { return pointLightPositions_[index]; }

    /**
     * @brief Get point light range (far plane) for shadow calculation
     */
    float getPointLightRange(int index = 0) const { return pointLightRanges_[index]; }

    /**
     * @brief Get descriptor info for shadow map sampling
     */
    VkDescriptorImageInfo getShadowMapDescriptorInfo(int index = 0) const { return shadowMaps_[index]->getDescriptorInfo(); }

    /**
     * @brief Get descriptor info for cube shadow map sampling
     */
    VkDescriptorImageInfo getCubeShadowMapDescriptorInfo(int index = 0) const { return cubeShadowMaps_[index]->getDescriptorInfo(); }

  private:
    void createPipelineLayout();
    void createPipeline();
    void createCubeShadowPipelineLayout();
    void createCubeShadowPipeline();

    /**
     * @brief Calculate orthographic projection matrix for directional light
     */
    glm::mat4 calculateDirectionalLightMatrix(const glm::vec3& lightDirection, const glm::vec3& sceneCenter, float sceneRadius);

    /**
     * @brief Calculate perspective projection matrix for spotlight
     */
    glm::mat4 calculateSpotLightMatrix(const glm::vec3& position, const glm::vec3& direction, float outerCutoffDegrees, float range);

    /**
     * @brief Calculate perspective projection matrix for one face of a point light cube map
     */
    glm::mat4 calculatePointLightMatrix(const glm::vec3& position, int face, float range);

    /**
     * @brief Render scene to a 2D shadow map with given light space matrix
     */
    void renderToShadowMap(FrameInfo& frameInfo, ShadowMap& shadowMap, const glm::mat4& lightSpaceMatrix);

    /**
     * @brief Render point light shadow maps (all 6 faces for each point light)
     */
    void renderPointLightShadowMaps(FrameInfo& frameInfo);

    /**
     * @brief Render scene to a single face of a cube shadow map
     */
    void renderToCubeFace(FrameInfo&       frameInfo,
                          CubeShadowMap&   cubeShadowMap,
                          int              face,
                          const glm::mat4& lightSpaceMatrix,
                          const glm::vec3& lightPos,
                          float            farPlane);

    Device&  device_;
    uint32_t shadowMapSize_;

    // 2D shadow maps for directional/spot lights
    std::vector<std::unique_ptr<ShadowMap>> shadowMaps_;
    std::unique_ptr<Pipeline>               pipeline_;
    VkPipelineLayout                        pipelineLayout_ = VK_NULL_HANDLE;

    // Cube shadow maps for point lights
    std::vector<std::unique_ptr<CubeShadowMap>> cubeShadowMaps_;
    std::unique_ptr<Pipeline>                   cubePipeline_;
    VkPipelineLayout                            cubePipelineLayout_ = VK_NULL_HANDLE;

    glm::mat4 lightSpaceMatrices_[MAX_SHADOW_MAPS];
    int       shadowLightCount_ = 0;

    glm::vec3 pointLightPositions_[MAX_CUBE_SHADOW_MAPS];
    float     pointLightRanges_[MAX_CUBE_SHADOW_MAPS];
    int       cubeShadowLightCount_ = 0;
  };

} // namespace engine
