#include "Engine/Systems/ShadowSystem.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/CubeShadowMap.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/ShadowMap.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "glm/common.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float4.hpp"
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
        // Create multiple shadow maps for directional/spot lights
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

    glm::mat4 ShadowSystem::calculateDirectionalCascadeMatrix(const glm::vec3& lightDirection,
        const Camera&                                                          camera,
        float                                                                  cascadeNear,
        float                                                                  cascadeFar,
        int                                                                    cascadeIndex,
        uint32_t                                                               shadowMapSize,
        glm::vec3*                                                             outMinLS,
        glm::vec3*                                                             outMaxLS,
        float*                                                                 outWorldUnitsPerTexel) {
        // cascadeIndex reserved for future per-cascade tuning (e.g., bias scaling)
        (void) cascadeIndex;

        // Normalize light direction
        glm::vec3 const lightDir = glm::normalize(lightDirection);

        // Get camera matrices
        glm::mat4 const proj    = camera.getProjectionMatrix();
        glm::mat4 const invView = camera.getInverseView();

        // Extract near/far planes from projection matrix
        float const A         = proj[2][2];
        float const B         = proj[3][2];
        float       nearPlane = 0.1f;
        if (glm::abs(A) > 1e-6f) {
            nearPlane = glm::max(0.001f, -B / A);
        }

        float farPlane = nearPlane + 100.0f;
        if (glm::abs(A - 1.0f) > 1e-6f) {
            farPlane = (A * nearPlane) / (A - 1.0f);
        }

        // Extract field of view and aspect ratio
        float const tanHalfFovy = 1.0f / glm::max(proj[1][1], 1e-6f);
        float const aspect      = proj[1][1] / glm::max(proj[0][0], 1e-6f);

        // Clamp cascade distances to valid range
        float const sliceNear = glm::clamp(cascadeNear, nearPlane, farPlane - 0.01f);
        float const sliceFar  = glm::clamp(cascadeFar, sliceNear + 0.01f, farPlane);

        // Calculate frustum dimensions at near and far planes
        float const nearHeight = 2.0f * tanHalfFovy * sliceNear;
        float const nearWidth  = nearHeight * aspect;
        float const farHeight  = 2.0f * tanHalfFovy * sliceFar;
        float const farWidth   = farHeight * aspect;

        // Get camera position and orientation
        glm::vec3 const camPos   = glm::vec3(invView[3]);
        glm::vec3 const camRight = glm::normalize(glm::vec3(invView[0]));
        glm::vec3 const camUp    = glm::normalize(glm::vec3(invView[1]));
        glm::vec3 const camFwd   = glm::normalize(glm::vec3(invView[2]));

        // Calculate frustum corners in world space
        glm::vec3   frustumCorners[8];
        float const nearZ = sliceNear;
        float const farZ  = sliceFar;
        float const nx    = nearWidth * 0.5f;
        float const ny    = nearHeight * 0.5f;
        float const fx    = farWidth * 0.5f;
        float const fy    = farHeight * 0.5f;

        frustumCorners[0] = camPos + camFwd * nearZ - camRight * nx - camUp * ny;  // Near bottom left
        frustumCorners[1] = camPos + camFwd * nearZ + camRight * nx - camUp * ny;  // Near bottom right
        frustumCorners[2] = camPos + camFwd * nearZ + camRight * nx + camUp * ny;  // Near top right
        frustumCorners[3] = camPos + camFwd * nearZ - camRight * nx + camUp * ny;  // Near top left
        frustumCorners[4] = camPos + camFwd * farZ - camRight * fx - camUp * fy;   // Far bottom left
        frustumCorners[5] = camPos + camFwd * farZ + camRight * fx - camUp * fy;   // Far bottom right
        frustumCorners[6] = camPos + camFwd * farZ + camRight * fx + camUp * fy;   // Far top right
        frustumCorners[7] = camPos + camFwd * farZ - camRight * fx + camUp * fy;   // Far top left

        // Calculate frustum center (used for bounds, not for light view)
        glm::vec3 frustumCenter{0.0f};
        for (glm::vec3 const& corner : frustumCorners) {
            frustumCenter += corner;
        }
        frustumCenter /= 8.0f;

        // Choose up vector for light view matrix
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::abs(glm::dot(lightDir, up)) > 0.99f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        // Calculate the bounding sphere radius of the frustum slice first
        float maxRadius = 0.0f;
        for (glm::vec3 const& corner : frustumCorners) {
            float dist = glm::length(corner - frustumCenter);
            maxRadius  = glm::max(maxRadius, dist);
        }

        // Position light far enough back to encompass the entire scene
        // Use the bounding sphere radius to determine how far back to place the light
        float const     lightDistance = maxRadius + 500.0f;  // Extra distance to capture shadow casters behind the frustum
        glm::mat4 const lightView     = glm::lookAt(frustumCenter - lightDir * lightDistance, frustumCenter, up);

        // Transform frustum corners to light space and find AABB
        glm::vec3 minLS(std::numeric_limits<float>::infinity());
        glm::vec3 maxLS(-std::numeric_limits<float>::infinity());
        for (glm::vec3 const& cornerWS : frustumCorners) {
            glm::vec4 const cornerLS4 = lightView * glm::vec4(cornerWS, 1.0f);
            glm::vec3 const cornerLS  = glm::vec3(cornerLS4);
            minLS                     = glm::min(minLS, cornerLS);
            maxLS                     = glm::max(maxLS, cornerLS);
        }

        // Use the radius to compute a stable, square ortho projection
        float const stableSize = maxRadius * 2.0f;
        float const halfSize   = stableSize * 0.5f;

        // Texel snapping for stable shadows
        float const worldUnitsPerTexel = stableSize / static_cast<float>(shadowMapSize);

        // The frustum center in light space - should be near the look-at target
        glm::vec4 const frustumCenterLS4 = lightView * glm::vec4(frustumCenter, 1.0f);
        glm::vec3 const frustumCenterLS  = glm::vec3(frustumCenterLS4);

        // Snap to texel grid
        float snappedCenterX = glm::floor(frustumCenterLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
        float snappedCenterY = glm::floor(frustumCenterLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

        // Set XY bounds centered on snapped position
        minLS.x = snappedCenterX - halfSize;
        maxLS.x = snappedCenterX + halfSize;
        minLS.y = snappedCenterY - halfSize;
        maxLS.y = snappedCenterY + halfSize;

        // Provide texel size to caller when requested (diagnostics / debug views)
        if (outWorldUnitsPerTexel != nullptr) {
            *outWorldUnitsPerTexel = worldUnitsPerTexel;
        }

        // For Z: use fixed near=0 and far based on how far back we placed the light
        // This ensures all geometry between the light and frustum is captured
        float const orthoNear = 0.0f;
        float const orthoFar  = lightDistance + maxRadius + 100.0f;

        glm::mat4 lightProj = glm::orthoZO(minLS.x, maxLS.x, minLS.y, maxLS.y, orthoNear, orthoFar);
        lightProj[1][1] *= -1;  // Flip Y for Vulkan

        // Output bounds if requested
        if (outMinLS != nullptr)
            *outMinLS = minLS;
        if (outMaxLS != nullptr)
            *outMaxLS = maxLS;

        return lightProj * lightView;
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
    void ShadowSystem::renderDirectionalShadows(FrameInfo& frameInfo, const ShadowSettings& settings) {
        // Only the first directional light is used for cascades (classic CSM)
        auto dirView = frameInfo.scene->getRegistry().view<DirectionalLightComponent, TransformComponent>();
        for (auto entity : dirView) {
            if (shadowLightCount_ >= MAX_SHADOW_MAPS)
                break;

            auto [dirLight, transform] = dirView.get<DirectionalLightComponent, TransformComponent>(entity);
            glm::vec3 const lightDir   = transform.getForwardDir();

            // Extract near/far from camera projection (keeps existing behavior)
            glm::mat4 const proj   = frameInfo.camera.getProjectionMatrix();
            float const     A      = proj[2][2];
            float const     B      = proj[3][2];
            float           nearPl = 0.1f;
            if (glm::abs(A) > 1e-6f)
                nearPl = glm::max(0.001f, -B / A);

            float farPl = nearPl + 100.0f;
            if (glm::abs(A - 1.0f) > 1e-6f)
                farPl = (A * nearPl) / (A - 1.0f);

            float const csmFar = glm::clamp(settings.shadowDistance, nearPl + 0.5f, farPl);

            // Split distribution
            float const lambda = settings.cascadeLambda;
            float       splits[DIRECTIONAL_CASCADE_COUNT];
            for (int i = 0; i < DIRECTIONAL_CASCADE_COUNT; ++i) {
                float const p        = static_cast<float>(i + 1) / static_cast<float>(DIRECTIONAL_CASCADE_COUNT);
                float const logSplit = nearPl * glm::pow(csmFar / nearPl, p);
                float const uniSplit = nearPl + ((csmFar - nearPl) * p);
                splits[i]            = glm::mix(uniSplit, logSplit, lambda);
            }

            // Per-cascade
            for (int cascade = 0; cascade < DIRECTIONAL_CASCADE_COUNT; ++cascade) {
                float const sliceNear = (cascade == 0) ? nearPl : splits[cascade - 1];
                float const sliceFar  = splits[cascade];

                directionalCascadeSplits_[cascade] = splits[cascade];
                if (shadowLightCount_ >= MAX_SHADOW_MAPS)
                    break;

                float cascadeTexelSize                 = 0.0f;
                lightSpaceMatrices_[shadowLightCount_] = calculateDirectionalCascadeMatrix(lightDir, frameInfo.camera, sliceNear, sliceFar, cascade, shadowMapSize_, nullptr, nullptr, &cascadeTexelSize);

                // store diagnostic texel-size for UI/inspection
                directionalCascadeWorldUnitsPerTexel_[cascade] = cascadeTexelSize;

                bool shouldRenderCascade = true;
                if (settings.enableShadowCulling) {
                    shouldRenderCascade = false;
                    auto mview          = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
                    for (auto e : mview) {
                        auto [mcomp, mtransform] = mview.get<ModelComponent, TransformComponent>(e);
                        if (shouldRenderModel(mcomp.model, mtransform.modelTransform(), lightSpaceMatrices_[shadowLightCount_], 0.0f)) {
                            shouldRenderCascade = true;
                            break;
                        }
                    }
                }

                if (shouldRenderCascade) {
                    directionalCascadeCount_++;
                    renderToShadowMapMesh(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
                    shadowLightCount_++;
                }
            }

            break;  // Only first directional light
        }
    }

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
        shadowLightCount_            = 0;
        directionalCascadeCount_     = 0;
        directionalCascadeBaseIndex_ = 0;
        for (float& directionalCascadeSplit : directionalCascadeSplits_) {
            directionalCascadeSplit = 0.0f;
        }
        // Reset diagnostic texel-size values
        for (int i = 0; i < DIRECTIONAL_CASCADE_COUNT; ++i) {
            directionalCascadeWorldUnitsPerTexel_[i] = 0.0f;
        }

        // Modular, per-light-type rendering (keeps behavior identical)
        renderDirectionalShadows(frameInfo, settings);
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