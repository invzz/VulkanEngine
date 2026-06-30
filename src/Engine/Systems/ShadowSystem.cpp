#include "Engine/Systems/ShadowSystem.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/CubeShadowMap.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/ShadowMap.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "glm/common.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    struct ShadowMeshPushConstants {
        glm::mat4 modelMatrix;
        glm::mat4 lightSpaceMatrix;

        uint64_t meshletBufferAddress;
        uint64_t meshletVerticesAddress;
        uint64_t meshletTrianglesAddress;
        uint64_t vertexBufferAddress;
        uint32_t meshletOffset;
        uint32_t meshletCount;
    };

    struct CubeShadowMeshPushConstants {
        glm::mat4 modelMatrix;
        glm::mat4 lightSpaceMatrix;
        glm::vec4 lightPosAndFarPlane;  // xyz = light position, w = far plane

        uint64_t meshletBufferAddress;
        uint64_t meshletVerticesAddress;
        uint64_t meshletTrianglesAddress;
        uint64_t vertexBufferAddress;
        uint32_t meshletOffset;
        uint32_t meshletCount;
    };

    // ─────────────────────────────────────────────────────────────────────────────

    ShadowSystem::ShadowSystem(Device& device, uint32_t shadowMapSize) : device_{device}, shadowMapSize_{shadowMapSize} {
        // Create 2D shadow maps for spot lights
        for (auto& lightSpaceMatrix : lightSpaceMatrices_) {
            shadowMaps_.push_back(std::make_unique<ShadowMap>(device, shadowMapSize, shadowMapSize));
            lightSpaceMatrix = glm::mat4(1.0f);
        }

        // Create cube shadow maps for point lights
        for (int i = 0; i < MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowMaps_.push_back(std::make_unique<CubeShadowMap>(device, shadowMapSize));
            pointLightPositions_[i] = glm::vec3(0.0f);
            pointLightRanges_[i]    = 25.0f;
        }

        createMeshPipelineLayout();
        createMeshPipeline();
        createCubeMeshPipelineLayout();
        createCubeMeshPipeline();

        std::cout << "[" << GREEN << "ShadowSystem" << RESET << "] Initialized with " << MAX_SHADOW_MAPS << " 2D shadow maps and " << MAX_CUBE_SHADOW_MAPS << " cube shadow maps (" << shadowMapSize << "x"
                  << shadowMapSize << "), mesh shader culling (Level 3)\n";
    }

    ShadowSystem::~ShadowSystem() {
        if (meshPipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_.device(), meshPipelineLayout_, nullptr);
        }
        if (cubeMeshPipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_.device(), cubeMeshPipelineLayout_, nullptr);
        }
    }

    void ShadowSystem::createMeshPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(ShadowMeshPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 0;
        layoutInfo.pSetLayouts            = nullptr;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushConstantRange;

        if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &meshPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow mesh pipeline layout");
        }
    }

    void ShadowSystem::createMeshPipeline() {
        PipelineConfigInfo configInfo{};
        Pipeline::defaultMeshPipelineConfigInfo(configInfo);

        // No color attachment - depth only
        configInfo.colorBlendInfo.attachmentCount = 0;
        configInfo.colorBlendAttachment           = {};

        // Disable Vulkan depth bias - we use texel-scaled shader bias instead.
        // Stacking hardware bias (clip-space) with shader bias (normalized depth)
        // causes cascade-dependent drift because cascades have different depth ranges,
        // which can flip depth comparisons at boundaries and create seams.
        configInfo.rasterizationInfo.depthBiasEnable         = VK_FALSE;
        configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;
        configInfo.rasterizationInfo.depthBiasSlopeFactor    = 0.0f;

        // No culling for shadows (all geometry matters)
        configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        configInfo.renderPass     = shadowMaps_[0]->getRenderPass();
        configInfo.pipelineLayout = meshPipelineLayout_;

        meshPipeline_ = std::make_unique<Pipeline>(device_,
            std::string(SHADER_PATH) + R"(shadow.task.spv)",
            std::string(SHADER_PATH) + R"(shadow.mesh.spv)",
            std::string(SHADER_PATH) + R"(shadow.frag.spv)",
            configInfo);
    }

    void ShadowSystem::createCubeMeshPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(CubeShadowMeshPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 0;
        layoutInfo.pSetLayouts            = nullptr;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushConstantRange;

        if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &cubeMeshPipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create cube shadow mesh pipeline layout");
        }
    }

    void ShadowSystem::createCubeMeshPipeline() {
        PipelineConfigInfo configInfo{};
        Pipeline::defaultMeshPipelineConfigInfo(configInfo);

        // No color attachment - depth only
        configInfo.colorBlendInfo.attachmentCount = 0;
        configInfo.colorBlendAttachment           = {};

        // Depth bias to prevent shadow acne
        configInfo.rasterizationInfo.depthBiasEnable         = VK_TRUE;
        configInfo.rasterizationInfo.depthBiasConstantFactor = 1.25f;
        configInfo.rasterizationInfo.depthBiasSlopeFactor    = 1.75f;

        // No culling for point light shadows
        configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        configInfo.renderPass     = cubeShadowMaps_[0]->getRenderPass();
        configInfo.pipelineLayout = cubeMeshPipelineLayout_;

        cubeMeshPipeline_ = std::make_unique<Pipeline>(device_,
            std::string(SHADER_PATH) + R"(cube_shadow.task.spv)",
            std::string(SHADER_PATH) + R"(cube_shadow.mesh.spv)",
            std::string(SHADER_PATH) + R"(cube_shadow_mesh.frag.spv)",
            configInfo);
    }

    glm::mat4 ShadowSystem::calculateSpotLightMatrix(const glm::vec3& position, const glm::vec3& direction, float outerCutoffDegrees, float range) {
        glm::vec3 const lightDir = glm::normalize(direction);

        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::abs(glm::dot(lightDir, up)) > 0.99f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        glm::mat4 const lightView = glm::lookAt(position, position + lightDir, up);

        float const fov       = glm::radians((outerCutoffDegrees * 2.0f) + 5.0f);
        float const nearPlane = 0.1f;
        float const farPlane  = range > 0.0f ? range : 100.0f;

        glm::mat4 lightProj = glm::perspective(fov, 1.0f, nearPlane, farPlane);
        lightProj[1][1] *= -1;

        return lightProj * lightView;
    }

    // Unified CPU culling helper used across shadow renderers.
    bool ShadowSystem::shouldRenderModel(const std::shared_ptr<engine::Model>& model, const glm::mat4& modelMatrix, const glm::mat4& lightSpaceMatrix, float lightRange, const glm::vec3& lightPos) const {
        if (!model)
            return false;
        if (model->getMeshletCount() == 0)
            return false;

        // If a finite lightRange is provided, use a cheap sphere-vs-sphere test
        // in world space and use squared distances to avoid sqrt.
        if (lightRange > 0.0f) {
            const auto& localBounds = model->getLocalBounds();
            AABB        worldBounds = transformAABB(localBounds, modelMatrix);
            glm::vec3   center      = worldBounds.center();
            float       radius      = glm::length(worldBounds.extents());
            float       rsum        = radius + lightRange;
            glm::vec3   d           = center - lightPos;
            return glm::dot(d, d) <= (rsum * rsum);
        }

        // Otherwise fall back to the conservative projection-space test already
        // used by the system.
        return modelIntersectsLightFrustum(model, modelMatrix, lightSpaceMatrix);
    }

    void ShadowSystem::renderToShadowMapMesh(FrameInfo& frameInfo, ShadowMap& shadowMap, const glm::mat4& lightSpaceMatrix) {
        shadowMap.beginRenderPass(frameInfo.commandBuffer);
        meshPipeline_->bind(frameInfo.commandBuffer);

        // Render all shadow-casting objects using mesh shaders (GPU culling built-in)
        auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
        for (auto entity : view) {
            auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
            if (!modelComp.model || modelComp.model->getMeshletCount() == 0) {
                continue;
            }

            const auto& model = modelComp.model;

            // Conservative CPU cull: use helper to test whether the model's
            // world-space bounding sphere intersects the light projection. This
            // keeps the per-model logic centralized and allows a cheap test here
            // before issuing GPU work.
            if (!modelIntersectsLightFrustum(model, transform.modelTransform(), lightSpaceMatrix)) {
                continue;
            }

            // Batch contiguous opaque submeshes to reduce per-submesh push-constant calls.
            // Conservative criteria:
            //  - consecutive submeshes
            //  - subMesh.meshletOffset is contiguous (offset == prev.offset + prev.count)
            //  - material is opaque for all submeshes in the batch
            //  - same model-level buffer addresses (guaranteed for submeshes of the same model)
            const auto& materials = model->getMaterials();
            const auto& subMeshes = model->getSubMeshes();
            size_t      i         = 0u;
            while (i < subMeshes.size()) {
                const auto& first = subMeshes[i];
                if (first.meshletCount == 0) {
                    ++i;
                    continue;
                }

                // Check material opacity for the first submesh
                bool firstOpaque = true;
                if (first.materialId >= 0 && first.materialId < static_cast<int>(materials.size())) {
                    firstOpaque = (materials[first.materialId].pbrMaterial.alphaMode == engine::AlphaMode::Opaque);
                }
                if (!firstOpaque) {
                    ++i;  // leave transparent/alpha-tested submeshes untouched
                    continue;
                }

                // Start a batch at i
                uint32_t batchOffset = first.meshletOffset;
                uint32_t batchCount  = first.meshletCount;
                size_t   j           = i + 1u;

                // Merge subsequent submeshes while they are contiguous and opaque
                for (; j < subMeshes.size(); ++j) {
                    const auto& next = subMeshes[j];
                    if (next.meshletCount == 0)
                        break;

                    // must be contiguous in meshlet index space
                    if (next.meshletOffset != batchOffset + batchCount)
                        break;

                    // material must be opaque
                    if (next.materialId >= 0 && next.materialId < static_cast<int>(materials.size())) {
                        if (materials[next.materialId].pbrMaterial.alphaMode != engine::AlphaMode::Opaque)
                            break;
                    }

                    // merge
                    batchCount += next.meshletCount;
                }

                // Emit single push-constant + draw for the whole batch
                ShadowMeshPushConstants push{};
                push.modelMatrix             = transform.modelTransform();
                push.lightSpaceMatrix        = lightSpaceMatrix;
                push.meshletBufferAddress    = model->getMeshletBufferAddress();
                push.meshletVerticesAddress  = model->getMeshletVerticesAddress();
                push.meshletTrianglesAddress = model->getMeshletTrianglesAddress();
                push.vertexBufferAddress     = model->getVertexBufferAddress();
                push.meshletOffset           = batchOffset;
                push.meshletCount            = batchCount;

                vkCmdPushConstants(frameInfo.commandBuffer, meshPipelineLayout_, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0, sizeof(push), &push);

                uint32_t const groupCount = (batchCount + 31) / 32;
                device_.vkCmdDrawMeshTasksEXT(frameInfo.commandBuffer, groupCount, 1, 1);

                // Advance to the next submesh after the batch
                i = j;
            }
        }

        engine::ShadowMap::endRenderPass(frameInfo.commandBuffer);
    }

    // --------------------------------------------------------------------------
    // Modular per-light-type renderers
    // --------------------------------------------------------------------------
    void ShadowSystem::renderSpotShadows(FrameInfo& frameInfo, const ShadowSettings& settings) {
        auto spotView = frameInfo.scene->getRegistry().view<SpotLightComponent, TransformComponent>();
        for (auto entity : spotView) {
            if (shadowLightCount_ >= MAX_SHADOW_MAPS)
                break;
            auto [spotLight, transform] = spotView.get<SpotLightComponent, TransformComponent>(entity);

            glm::vec3 const position           = transform.translation;
            glm::vec3 const direction          = transform.getForwardDir();
            float const     outerCutoffDegrees = spotLight.outerCutoffAngle;
            float const     range              = settings.spotLightDefaultRange;

            lightSpaceMatrices_[shadowLightCount_] = calculateSpotLightMatrix(position, direction, outerCutoffDegrees, range);

            bool shouldRenderSpot = true;
            if (settings.enableShadowCulling) {
                shouldRenderSpot = false;
                auto mview       = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
                for (auto e : mview) {
                    auto [mcomp, mtransform] = mview.get<ModelComponent, TransformComponent>(e);
                    if (shouldRenderModel(mcomp.model, mtransform.modelTransform(), lightSpaceMatrices_[shadowLightCount_], 0.0f)) {
                        shouldRenderSpot = true;
                        break;
                    }
                }
            }

            if (shouldRenderSpot) {
                renderToShadowMapMesh(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
                shadowLightCount_++;
            }
        }
    }

    void ShadowSystem::renderPointShadows(FrameInfo& frameInfo, const ShadowSettings& settings) {
        // Existing implementation handles per-face rendering and CPU culls; call it
        // but ensure default ranges come from settings where applicable.
        (void) settings;  // kept for future tuning (batching, range overrides)
        renderPointLightShadowMaps(frameInfo, settings);
    }

    void ShadowSystem::renderShadowMaps(FrameInfo& frameInfo, const ShadowSettings& settings) {
        // Reset per-frame counters
        shadowLightCount_ = 0;

        // Modular, per-light-type rendering (keeps behavior identical)
        renderSpotShadows(frameInfo, settings);
        renderPointShadows(frameInfo, settings);
    }

    void ShadowSystem::renderPointLightShadowMaps(FrameInfo& frameInfo, const ShadowSettings& settings) {
        cubeShadowLightCount_ = 0;

        auto view = frameInfo.scene->getRegistry().view<PointLightComponent, TransformComponent>();
        for (auto entity : view) {
            if (cubeShadowLightCount_ >= MAX_CUBE_SHADOW_MAPS) {
                break;
            }
            auto [pointLight, transform] = view.get<PointLightComponent, TransformComponent>(entity);

            glm::vec3 const position = transform.translation;
            float const     range    = settings.pointLightDefaultRange;

            // Optional CPU cull: skip entire cubemap if no models are within range
            bool shouldRenderPoint = true;
            if (settings.enableShadowCulling) {
                shouldRenderPoint = false;
                auto mview        = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
                for (auto e : mview) {
                    auto [mcomp, mtransform] = mview.get<ModelComponent, TransformComponent>(e);
                    if (!mcomp.model || mcomp.model->getMeshletCount() == 0)
                        continue;
                    const auto& localBounds = mcomp.model->getLocalBounds();
                    AABB        worldBounds = transformAABB(localBounds, mtransform.modelTransform());
                    glm::vec3   center      = worldBounds.center();
                    float       radius      = glm::length(worldBounds.extents());
                    if (shouldRenderModel(mcomp.model, mtransform.modelTransform(), glm::mat4(1.0f), range, position)) {
                        shouldRenderPoint = true;
                        break;
                    }
                }
            }

            if (!shouldRenderPoint) {
                continue;  // skip this point light
            }

            pointLightPositions_[cubeShadowLightCount_] = position;
            pointLightRanges_[cubeShadowLightCount_]    = range;

            renderToCubeShadowMap(frameInfo, *cubeShadowMaps_[cubeShadowLightCount_], position, range);

            cubeShadowLightCount_++;
        }
    }

    glm::mat4 ShadowSystem::calculatePointLightMatrix(const glm::vec3& position, int face, float range) {
        float const nearPlane  = 0.1f;
        glm::mat4   projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, range);
        projection[1][1] *= -1;

        glm::mat4 const view = CubeShadowMap::getFaceViewMatrix(position, face);

        return projection * view;
    }

    bool ShadowSystem::modelIntersectsLightFrustum(const std::shared_ptr<engine::Model>& model, const glm::mat4& modelMatrix, const glm::mat4& lightSpaceMatrix) const {
        if (!model)
            return false;

        const auto& localBounds = model->getLocalBounds();
        AABB        worldBounds = transformAABB(localBounds, modelMatrix);
        glm::vec3   center      = worldBounds.center();
        float       radius      = glm::length(worldBounds.extents());

        // Project center into light clip space
        glm::vec4 clipCenter = lightSpaceMatrix * glm::vec4(center, 1.0f);
        if (!std::isfinite(clipCenter.w) || glm::abs(clipCenter.w) < 1e-6f) {
            // Degenerate projection; conservatively assume intersecting
            return true;
        }

        glm::vec3 ndcCenter = glm::vec3(clipCenter) / clipCenter.w;

        // Project a radius offset to estimate NDC radius
        glm::vec4 clipRadiusPt   = lightSpaceMatrix * glm::vec4(center + glm::vec3(radius, 0.0f, 0.0f), 1.0f);
        glm::vec3 ndcRadiusVec   = glm::abs(glm::vec3(clipRadiusPt) / clipRadiusPt.w - ndcCenter);
        float     radiusNDC      = glm::max(ndcRadiusVec.x, ndcRadiusVec.y);
        float     radiusDepthNDC = ndcRadiusVec.z;

        const float kPad = 0.01f;
        return (ndcCenter.x + radiusNDC) >= (-1.0f - kPad) && (ndcCenter.x - radiusNDC) <= (1.0f + kPad) && (ndcCenter.y + radiusNDC) >= (-1.0f - kPad) && (ndcCenter.y - radiusNDC) <= (1.0f + kPad) &&
               (ndcCenter.z + radiusDepthNDC) >= (0.0f - kPad) && (ndcCenter.z - radiusDepthNDC) <= (1.0f + kPad);
    }

    void ShadowSystem::renderToCubeShadowMap(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, const glm::vec3& position, float range) {
        for (int face = 0; face < 6; face++) {
            glm::mat4 const lightSpaceMatrix = calculatePointLightMatrix(position, face, range);
            renderToCubeFaceMesh(frameInfo, cubeShadowMap, face, lightSpaceMatrix, position, range);
        }
    }

    void ShadowSystem::renderToCubeFaceMesh(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, int face, const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPos, float farPlane) {
        cubeShadowMap.beginRenderPass(frameInfo.commandBuffer, face);
        cubeMeshPipeline_->bind(frameInfo.commandBuffer);

        // Render all shadow-casting objects using mesh shaders
        auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
        for (auto entity : view) {
            auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
            if (!modelComp.model || modelComp.model->getMeshletCount() == 0) {
                continue;
            }

            const auto& model = modelComp.model;

            // Cheap CPU cull: skip entire model for point-light faces when the model's
            // world-space bounding sphere lies completely outside the light's range.
            // This is conservative and avoids issuing mesh-shader work for distant objects.
            if (!shouldRenderModel(model, transform.modelTransform(), lightSpaceMatrix, farPlane, lightPos)) {
                continue;  // skip this model entirely
            }

            // Batch contiguous opaque submeshes for cube-shadow rendering as well.
            const auto& materialsC = model->getMaterials();
            const auto& subMeshesC = model->getSubMeshes();
            size_t      idx        = 0u;
            while (idx < subMeshesC.size()) {
                const auto& first = subMeshesC[idx];
                if (first.meshletCount == 0) {
                    ++idx;
                    continue;
                }

                bool firstOpaque = true;
                if (first.materialId >= 0 && first.materialId < static_cast<int>(materialsC.size())) {
                    firstOpaque = (materialsC[first.materialId].pbrMaterial.alphaMode == engine::AlphaMode::Opaque);
                }
                if (!firstOpaque) {
                    ++idx;
                    continue;
                }

                uint32_t batchOffset = first.meshletOffset;
                uint32_t batchCount  = first.meshletCount;
                size_t   k           = idx + 1u;
                for (; k < subMeshesC.size(); ++k) {
                    const auto& next = subMeshesC[k];
                    if (next.meshletCount == 0)
                        break;
                    if (next.meshletOffset != batchOffset + batchCount)
                        break;
                    if (next.materialId >= 0 && next.materialId < static_cast<int>(materialsC.size())) {
                        if (materialsC[next.materialId].pbrMaterial.alphaMode != engine::AlphaMode::Opaque)
                            break;
                    }
                    batchCount += next.meshletCount;
                }

                CubeShadowMeshPushConstants push{};
                push.modelMatrix             = transform.modelTransform();
                push.lightSpaceMatrix        = lightSpaceMatrix;
                push.lightPosAndFarPlane     = glm::vec4(lightPos, farPlane);
                push.meshletBufferAddress    = model->getMeshletBufferAddress();
                push.meshletVerticesAddress  = model->getMeshletVerticesAddress();
                push.meshletTrianglesAddress = model->getMeshletTrianglesAddress();
                push.vertexBufferAddress     = model->getVertexBufferAddress();
                push.meshletOffset           = batchOffset;
                push.meshletCount            = batchCount;

                vkCmdPushConstants(frameInfo.commandBuffer, cubeMeshPipelineLayout_, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

                uint32_t const groupCount = (batchCount + 31) / 32;
                device_.vkCmdDrawMeshTasksEXT(frameInfo.commandBuffer, groupCount, 1, 1);

                idx = k;
            }
        }

        engine::CubeShadowMap::endRenderPass(frameInfo.commandBuffer);
    }
}  // namespace engine