#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "3dEngine/Device.hpp"
#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/Pipeline.hpp"
#include "3dEngine/ShadowMap.hpp"

namespace engine {

  /**
   * @brief System for rendering shadow maps from light perspectives
   *
   * Manages shadow map rendering for directional and spot lights.
   * Renders scene depth from light's POV before main render pass.
   */
  class ShadowSystem
  {
  public:
    static constexpr int MAX_SHADOW_MAPS = 4; // 1 directional + up to 3 spotlights

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
     * @brief Get the light space matrix at specified index
     */
    const glm::mat4& getLightSpaceMatrix(int index = 0) const { return lightSpaceMatrices_[index]; }

    /**
     * @brief Get number of active shadow-casting lights
     */
    int getShadowLightCount() const { return shadowLightCount_; }

    /**
     * @brief Get descriptor info for shadow map sampling (for directional light - index 0)
     */
    VkDescriptorImageInfo getShadowMapDescriptorInfo(int index = 0) const { return shadowMaps_[index]->getDescriptorInfo(); }

  private:
    void createPipelineLayout();
    void createPipeline();

    /**
     * @brief Calculate orthographic projection matrix for directional light
     */
    glm::mat4 calculateDirectionalLightMatrix(const glm::vec3& lightDirection, const glm::vec3& sceneCenter, float sceneRadius);

    /**
     * @brief Calculate perspective projection matrix for spotlight
     * @param outerCutoffDegrees Outer cone angle in degrees
     */
    glm::mat4 calculateSpotLightMatrix(const glm::vec3& position, const glm::vec3& direction, float outerCutoffDegrees, float range);

    /**
     * @brief Render scene to a shadow map with given light space matrix
     */
    void renderToShadowMap(FrameInfo& frameInfo, ShadowMap& shadowMap, const glm::mat4& lightSpaceMatrix);

    Device& device_;

    std::vector<std::unique_ptr<ShadowMap>> shadowMaps_;
    std::unique_ptr<Pipeline>               pipeline_;
    VkPipelineLayout                        pipelineLayout_ = VK_NULL_HANDLE;

    glm::mat4 lightSpaceMatrices_[MAX_SHADOW_MAPS];
    int       shadowLightCount_ = 0;
  };

} // namespace engine
