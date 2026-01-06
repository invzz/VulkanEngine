#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SHADOWSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SHADOWSYSTEM_HPP

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Engine/Graphics/CubeShadowMap.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/ShadowMap.hpp"

namespace engine {

  /**
   * @brief System for rendering shadow maps from light perspectives
   *
   * Manages shadow map rendering for directional, spot, and point lights.
   * Uses 2D shadow maps for directional/spot lights and cube maps for point
   * lights.
   */
  class ShadowSystem
  {
  public:
    static constexpr int DIRECTIONAL_CASCADE_COUNT = 4;
    static constexpr int MAX_SPOT_SHADOW_MAPS      = 4;
    static constexpr int MAX_SHADOW_MAPS           = DIRECTIONAL_CASCADE_COUNT + MAX_SPOT_SHADOW_MAPS; // Directional cascades + spotlights
    static constexpr int MAX_CUBE_SHADOW_MAPS      = 4;                                                // For point lights (cube maps)

    ShadowSystem(Device& device, uint32_t shadowMapSize = 2048);
    ~ShadowSystem();

    ShadowSystem(const ShadowSystem&)            = delete;
    ShadowSystem& operator=(const ShadowSystem&) = delete;

    /**
     * @brief Render all shadow maps for the frame
     * @param frameInfo Current frame information
     * @param shadowDistance World-space distance from the camera to cover with the
     *        directional light shadow. Larger values reduce quality.
     */
    void renderShadowMaps(FrameInfo& frameInfo, float shadowDistance = 20.0f);

    [[nodiscard]] int       getDirectionalCascadeCount() const { return directionalCascadeCount_; }
    [[nodiscard]] int       getDirectionalCascadeBaseIndex() const { return directionalCascadeBaseIndex_; }
    [[nodiscard]] glm::vec4 getDirectionalCascadeSplits() const
    {
      return glm::vec4(directionalCascadeSplits_[0], directionalCascadeSplits_[1], directionalCascadeSplits_[2], directionalCascadeSplits_[3]);
    }

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
    [[nodiscard]] const glm::mat4& getLightSpaceMatrix(int index = 0) const { return lightSpaceMatrices_[index]; }

    /**
     * @brief Get number of active shadow-casting directional/spot lights
     */
    [[nodiscard]] int getShadowLightCount() const { return shadowLightCount_; }

    /**
     * @brief Get number of active shadow-casting point lights
     */
    [[nodiscard]] int getCubeShadowLightCount() const { return cubeShadowLightCount_; }

    /**
     * @brief Get point light position for shadow calculation in shader
     */
    [[nodiscard]] const glm::vec3& getPointLightPosition(int index = 0) const { return pointLightPositions_[index]; }

    /**
     * @brief Get point light range (far plane) for shadow calculation
     */
    [[nodiscard]] float getPointLightRange(int index = 0) const { return pointLightRanges_[index]; }

    /**
     * @brief Get descriptor info for shadow map sampling
     */
    [[nodiscard]] VkDescriptorImageInfo getShadowMapDescriptorInfo(int index = 0) const { return shadowMaps_[index]->getDescriptorInfo(); }

    /**
     * @brief Get descriptor info for cube shadow map sampling
     */
    [[nodiscard]] VkDescriptorImageInfo getCubeShadowMapDescriptorInfo(int index = 0) const { return cubeShadowMaps_[index]->getDescriptorInfo(); }

  private:
    void createPipelineLayout();
    void createPipeline();
    void createCubeShadowPipelineLayout();
    void createCubeShadowPipeline();

    /**
     * @brief Calculate orthographic projection matrix for directional light
     */
    glm::mat4 calculateDirectionalCascadeMatrix(const glm::vec3& lightDirection, const Camera& camera, float cascadeNear, float cascadeFar) const;

    /**
     * @brief Calculate perspective projection matrix for spotlight
     */
    static glm::mat4 calculateSpotLightMatrix(const glm::vec3& position, const glm::vec3& direction, float outerCutoffDegrees, float range);

    /**
     * @brief Calculate perspective projection matrix for one face of a point
     * light cube map
     */
    static glm::mat4 calculatePointLightMatrix(const glm::vec3& position, int face, float range);

    /**
     * @brief Render scene to a 2D shadow map with given light space matrix
     */
    void renderToShadowMap(FrameInfo& frameInfo, ShadowMap& shadowMap, const glm::mat4& lightSpaceMatrix);

    /**
     * @brief Render point light shadow maps (all 6 faces for each point light)
     */
    void renderPointLightShadowMaps(FrameInfo& frameInfo);

    /**
     * @brief Render all 6 faces of a cube shadow map for a single point light
     */
    void renderToCubeShadowMap(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, const glm::vec3& position, float range);

    /**
     * @brief Render scene to a single face of a cube shadow map
     */
    void renderToCubeFace(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, int face, const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPos, float farPlane);

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

    int   directionalCascadeCount_     = 0;
    int   directionalCascadeBaseIndex_ = 0; // Cascades are stored starting at 0
    float directionalCascadeSplits_[DIRECTIONAL_CASCADE_COUNT]{0.0f, 0.0f, 0.0f, 0.0f};

    glm::vec3 pointLightPositions_[MAX_CUBE_SHADOW_MAPS];
    float     pointLightRanges_[MAX_CUBE_SHADOW_MAPS];
    int       cubeShadowLightCount_ = 0;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SHADOWSYSTEM_HPP
