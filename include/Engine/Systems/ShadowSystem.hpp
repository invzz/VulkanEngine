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

#include "ModelLib/Resources/Model.hpp"

namespace engine {

    /**
 * @brief Settings for shadow map rendering
 */
    struct ShadowSettings {
        // Enable conservative CPU culling (off by default).
        // When true, the system will skip rendering shadow maps for spot
        // projections or point cubemaps that have no potential shadow casters.
        // This avoids
        // unnecessary render-pass and pipeline work on CPU/GPU.
        bool enableShadowCulling = false;

        // Additional tunables to avoid magic numbers and make testing easier
        float pointLightDefaultRange = 25.0f;
        float spotLightDefaultRange  = 50.0f;
    };

    /**
 * @brief System for rendering shadow maps from light perspectives
 *
 * Uses mesh shaders with built-in frustum culling (Level 3 GPU-driven).
 * Manages shadow map rendering for spot and point lights.
 * Uses 2D shadow maps for spot lights and cube maps for point
 * lights.
 */
    class ShadowSystem {
       public:
        static constexpr int MAX_SPOT_SHADOW_MAPS = 4;
        static constexpr int MAX_SHADOW_MAPS      = MAX_SPOT_SHADOW_MAPS;
        static constexpr int MAX_CUBE_SHADOW_MAPS = 4;

        ShadowSystem(Device& device, uint32_t shadowMapSize = 2048);
        ~ShadowSystem();

        ShadowSystem(const ShadowSystem&)            = delete;
        ShadowSystem& operator=(const ShadowSystem&) = delete;

        /**
   * @brief Render all shadow maps for the frame
   * @param frameInfo Current frame information
   * @param settings Shadow settings
   */
        void renderShadowMaps(FrameInfo& frameInfo, const ShadowSettings& settings);

        ShadowMap& getShadowMap(int index = 0) {
            return *shadowMaps_[index];
        }
        CubeShadowMap& getCubeShadowMap(int index = 0) {
            return *cubeShadowMaps_[index];
        }

        [[nodiscard]] const glm::mat4& getLightSpaceMatrix(int index = 0) const {
            return lightSpaceMatrices_[index];
        }
        [[nodiscard]] int getShadowLightCount() const {
            return shadowLightCount_;
        }
        [[nodiscard]] int getCubeShadowLightCount() const {
            return cubeShadowLightCount_;
        }
        [[nodiscard]] const glm::vec3& getPointLightPosition(int index = 0) const {
            return pointLightPositions_[index];
        }
        [[nodiscard]] float getPointLightRange(int index = 0) const {
            return pointLightRanges_[index];
        }

        [[nodiscard]] VkDescriptorImageInfo getShadowMapDescriptorInfo(int index = 0) const {
            return shadowMaps_[index]->getDescriptorInfo();
        }
        [[nodiscard]] VkDescriptorImageInfo getCubeShadowMapDescriptorInfo(int index = 0) const {
            return cubeShadowMaps_[index]->getDescriptorInfo();
        }

       private:
        void createMeshPipelineLayout();
        void createMeshPipeline();
        void createCubeMeshPipelineLayout();
        void createCubeMeshPipeline();

        static glm::mat4 calculateSpotLightMatrix(const glm::vec3& position, const glm::vec3& direction, float outerCutoffDegrees, float range);
        static glm::mat4 calculatePointLightMatrix(const glm::vec3& position, int face, float range);

        // Conservative CPU test: does the model's world bounding sphere intersect
        // the given light projection (lightSpaceMatrix = proj * view)?
        bool modelIntersectsLightFrustum(const std::shared_ptr<engine::Model>& model, const glm::mat4& modelMatrix, const glm::mat4& lightSpaceMatrix) const;

        // Unified CPU culling helper used by spot/point flows.
        bool shouldRenderModel(const std::shared_ptr<engine::Model>& model,
            const glm::mat4&                                         modelMatrix,
            const glm::mat4&                                         lightSpaceMatrix,
            float                                                    lightRange = 0.0f,
            const glm::vec3&                                         lightPos   = glm::vec3(0.0f)) const;

        /**
   * @brief Render scene to a 2D shadow map using mesh shaders (GPU culling)
   */
        void renderToShadowMapMesh(FrameInfo& frameInfo, ShadowMap& shadowMap, const glm::mat4& lightSpaceMatrix);

        // Modular per-light-type renderers (extracted from renderShadowMaps)
        void renderSpotShadows(FrameInfo& frameInfo, const ShadowSettings& settings);
        void renderPointShadows(FrameInfo& frameInfo, const ShadowSettings& settings);

        void renderPointLightShadowMaps(FrameInfo& frameInfo, const ShadowSettings& settings);
        void renderToCubeShadowMap(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, const glm::vec3& position, float range);
        void renderToCubeFaceMesh(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, int face, const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPos, float farPlane);

        Device&  device_;
        uint32_t shadowMapSize_;

        // 2D shadow maps for spot lights (mesh shader pipeline)
        std::vector<std::unique_ptr<ShadowMap>> shadowMaps_;
        std::unique_ptr<Pipeline>               meshPipeline_;
        VkPipelineLayout                        meshPipelineLayout_ = VK_NULL_HANDLE;

        // Cube shadow maps for point lights (mesh shader pipeline)
        std::vector<std::unique_ptr<CubeShadowMap>> cubeShadowMaps_;
        std::unique_ptr<Pipeline>                   cubeMeshPipeline_;
        VkPipelineLayout                            cubeMeshPipelineLayout_ = VK_NULL_HANDLE;

        glm::mat4 lightSpaceMatrices_[MAX_SHADOW_MAPS];
        int       shadowLightCount_ = 0;

        glm::vec3 pointLightPositions_[MAX_CUBE_SHADOW_MAPS];
        float     pointLightRanges_[MAX_CUBE_SHADOW_MAPS];
        int       cubeShadowLightCount_ = 0;
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SHADOWSYSTEM_HPP
