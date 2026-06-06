#include "Engine/Systems/ModelRenderSystem.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightingRenderBindings.hpp"
#include "Engine/Systems/MaterialRenderBindings.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "ModelLib/Resources/Texture.hpp"
#include "entt/entity/fwd.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    // ============================================================================
    // CPU Frustum Culling
    // ============================================================================

    /**
 * @brief Frustum representation using 6 normalized planes
 */
    struct Frustum {
        glm::vec4 planes[6];  // Left, Right, Bottom, Top, Near, Far
    };

    /**
 * @brief Extract frustum planes from view-projection matrix (Gribb-Hartmann method)
 */
    inline Frustum extractFrustumFromMatrix(const glm::mat4& vp) {
        Frustum   f;
        glm::mat4 vpT  = glm::transpose(vp);
        glm::vec4 row0 = vpT[0];
        glm::vec4 row1 = vpT[1];
        glm::vec4 row2 = vpT[2];
        glm::vec4 row3 = vpT[3];

        f.planes[0] = row3 + row0;  // Left
        f.planes[1] = row3 - row0;  // Right
        f.planes[2] = row3 + row1;  // Bottom
        f.planes[3] = row3 - row1;  // Top
        f.planes[4] = row2;         // Near
        f.planes[5] = row3 - row2;  // Far

        // Normalize planes
        for (auto& plane : f.planes) {
            float len = glm::length(glm::vec3(plane));
            plane /= len;
        }

        return f;
    }

    /**
 * @brief Test if AABB is inside or intersecting frustum
 * @return true if visible (should be rendered), false if completely outside
 */
    inline bool aabbInFrustum(const AABB& box, const Frustum& f) {
        for (int i = 0; i < 6; ++i) {
            glm::vec3 normal(f.planes[i]);
            float     d = f.planes[i].w;

            // Find the positive vertex (farthest in plane normal direction)
            glm::vec3 pVertex;
            pVertex.x = (normal.x >= 0.0f) ? box.max.x : box.min.x;
            pVertex.y = (normal.y >= 0.0f) ? box.max.y : box.min.y;
            pVertex.z = (normal.z >= 0.0f) ? box.max.z : box.min.z;

            // If positive vertex is outside this plane, AABB is completely outside frustum
            if (glm::dot(normal, pVertex) + d < 0.0f) {
                return false;
            }
        }
        return true;
    }

    /**
 * @brief Check if an entity with model and transform is visible in the camera frustum
 */
    inline bool isEntityVisible(const Model* model, const glm::mat4& modelMatrix, const Frustum& frustum) {
        if (model == nullptr) {
            return false;
        }

        const AABB& localBounds = model->getLocalBounds();
        if (!localBounds.isValid()) {
            // No valid bounds - assume visible
            return true;
        }

        AABB worldBounds = transformAABB(localBounds, modelMatrix);
        return aabbInFrustum(worldBounds, frustum);
    }

    namespace {
        constexpr float kFeatureEpsCpu = 0.01f;

        bool isMeaningfulMaterialOverride(const PBRMaterial& material) {
            const PBRMaterial defaults{};

            if (material.albedo != defaults.albedo || material.metallic != defaults.metallic || material.roughness != defaults.roughness || material.ao != defaults.ao || material.alphaMode != defaults.alphaMode || material.alphaCutoff != defaults.alphaCutoff || material.doubleSided != defaults.doubleSided || material.clearcoat != defaults.clearcoat || material.clearcoatRoughness != defaults.clearcoatRoughness || material.anisotropic != defaults.anisotropic || material.anisotropicRotation != defaults.anisotropicRotation || material.transmission != defaults.transmission || material.ior != defaults.ior || material.thickness != defaults.thickness || material.attenuationColor != defaults.attenuationColor || material.attenuationDistance != defaults.attenuationDistance || material.iridescence != defaults.iridescence || material.iridescenceIOR != defaults.iridescenceIOR || material.iridescenceThickness != defaults.iridescenceThickness || material.emissiveColor != defaults.emissiveColor || material.emissiveStrength != defaults.emissiveStrength || material.useMetallicRoughnessTexture != defaults.useMetallicRoughnessTexture || material.useOcclusionRoughnessMetallicTexture != defaults.useOcclusionRoughnessMetallicTexture || material.useSpecularGlossinessWorkflow != defaults.useSpecularGlossinessWorkflow || material.specularFactor != defaults.specularFactor || material.glossinessFactor != defaults.glossinessFactor || material.uvScale != defaults.uvScale) {
                return true;
            }

            return material.albedoMap != nullptr || material.normalMap != nullptr || material.metallicMap != nullptr || material.roughnessMap != nullptr || material.aoMap != nullptr || material.emissiveMap != nullptr || material.specularGlossinessMap != nullptr || material.transmissionMap != nullptr || material.clearcoatMap != nullptr || material.clearcoatRoughnessMap != nullptr || material.clearcoatNormalMap != nullptr;
        }

        PBRMaterial mergeMaterialOverrides(const PBRMaterial* baseMaterial, const PBRMaterial& overrideMaterial) {
            PBRMaterial merged = (baseMaterial != nullptr) ? *baseMaterial : PBRMaterial{};

            merged.albedo                               = overrideMaterial.albedo;
            merged.metallic                             = overrideMaterial.metallic;
            merged.roughness                            = overrideMaterial.roughness;
            merged.ao                                   = overrideMaterial.ao;
            merged.alphaMode                            = overrideMaterial.alphaMode;
            merged.alphaCutoff                          = overrideMaterial.alphaCutoff;
            merged.doubleSided                          = overrideMaterial.doubleSided;
            merged.clearcoat                            = overrideMaterial.clearcoat;
            merged.clearcoatRoughness                   = overrideMaterial.clearcoatRoughness;
            merged.anisotropic                          = overrideMaterial.anisotropic;
            merged.anisotropicRotation                  = overrideMaterial.anisotropicRotation;
            merged.transmission                         = overrideMaterial.transmission;
            merged.ior                                  = overrideMaterial.ior;
            merged.thickness                            = overrideMaterial.thickness;
            merged.attenuationColor                     = overrideMaterial.attenuationColor;
            merged.attenuationDistance                  = overrideMaterial.attenuationDistance;
            merged.iridescence                          = overrideMaterial.iridescence;
            merged.iridescenceIOR                       = overrideMaterial.iridescenceIOR;
            merged.iridescenceThickness                 = overrideMaterial.iridescenceThickness;
            merged.emissiveColor                        = overrideMaterial.emissiveColor;
            merged.emissiveStrength                     = overrideMaterial.emissiveStrength;
            merged.useMetallicRoughnessTexture          = overrideMaterial.useMetallicRoughnessTexture;
            merged.useOcclusionRoughnessMetallicTexture = overrideMaterial.useOcclusionRoughnessMetallicTexture;
            merged.useSpecularGlossinessWorkflow        = overrideMaterial.useSpecularGlossinessWorkflow;
            merged.specularFactor                       = overrideMaterial.specularFactor;
            merged.glossinessFactor                     = overrideMaterial.glossinessFactor;
            merged.uvScale                              = overrideMaterial.uvScale;

            if (overrideMaterial.albedoMap != nullptr) {
                merged.albedoMap = overrideMaterial.albedoMap;
            }
            if (overrideMaterial.normalMap != nullptr) {
                merged.normalMap = overrideMaterial.normalMap;
            }
            if (overrideMaterial.metallicMap != nullptr) {
                merged.metallicMap = overrideMaterial.metallicMap;
            }
            if (overrideMaterial.roughnessMap != nullptr) {
                merged.roughnessMap = overrideMaterial.roughnessMap;
            }
            if (overrideMaterial.aoMap != nullptr) {
                merged.aoMap = overrideMaterial.aoMap;
            }
            if (overrideMaterial.emissiveMap != nullptr) {
                merged.emissiveMap = overrideMaterial.emissiveMap;
            }
            if (overrideMaterial.specularGlossinessMap != nullptr) {
                merged.specularGlossinessMap = overrideMaterial.specularGlossinessMap;
            }
            if (overrideMaterial.transmissionMap != nullptr) {
                merged.transmissionMap = overrideMaterial.transmissionMap;
            }
            if (overrideMaterial.clearcoatMap != nullptr) {
                merged.clearcoatMap = overrideMaterial.clearcoatMap;
            }
            if (overrideMaterial.clearcoatRoughnessMap != nullptr) {
                merged.clearcoatRoughnessMap = overrideMaterial.clearcoatRoughnessMap;
            }
            if (overrideMaterial.clearcoatNormalMap != nullptr) {
                merged.clearcoatNormalMap = overrideMaterial.clearcoatNormalMap;
            }

            return merged;
        }

        const PBRMaterial* resolveMaterialForSubMesh(FrameInfo& frameInfo,
            entt::entity                                        entity,
            const ModelComponent&                               modelComp,
            const Model::SubMesh&                               subMesh,
            std::optional<PBRMaterial>&                         mergedMaterialStorage) {
            const auto&        materials    = modelComp.model->getMaterials();
            const PBRMaterial* baseMaterial = nullptr;
            if (subMesh.materialId >= 0 && subMesh.materialId < static_cast<int>(materials.size())) {
                baseMaterial = &materials[subMesh.materialId].pbrMaterial;
            }

            if (auto* overrideMaterial = frameInfo.scene->getRegistry().try_get<PBRMaterial>(entity)) {
                if (!isMeaningfulMaterialOverride(*overrideMaterial)) {
                    return baseMaterial;
                }
                mergedMaterialStorage = mergeMaterialOverrides(baseMaterial, *overrideMaterial);
                return &*mergedMaterialStorage;
            }

            return baseMaterial;
        }

        MeshPushConstantData makeMeshPush(const ModelComponent& modelComp, const Model::SubMesh& subMesh, const glm::mat4& modelMatrix) {
            MeshPushConstantData push{};
            push.modelMatrix             = modelMatrix;
            push.normalMatrix            = glm::transpose(glm::inverse(push.modelMatrix));
            push.meshId                  = modelComp.model->getMeshId();
            push.meshletBufferAddress    = modelComp.model->getMeshletBufferAddress();
            push.meshletVerticesAddress  = modelComp.model->getMeshletVerticesAddress();
            push.meshletTrianglesAddress = modelComp.model->getMeshletTrianglesAddress();
            push.vertexBufferAddress     = modelComp.model->getVertexBufferAddress();
            push.meshletOffset           = subMesh.meshletOffset;
            push.meshletCount            = subMesh.meshletCount;

            return push;
        }

        void pushConstantsAndDraw(Device& device, VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const MeshPushConstantData& push, uint32_t meshletCount) {
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstantData), &push);

            if (device.vkCmdDrawMeshTasksEXT != nullptr) {
                uint32_t const groupCount = (meshletCount + 31) / 32;
                device.vkCmdDrawMeshTasksEXT(commandBuffer, groupCount, 1, 1);
            }
        }
    }  // namespace

    ModelRenderSystem::ModelRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout)
        : device(device), renderPass_(renderPass) {
        createSceneColorDescriptorResources();

        lightingBindings_ = std::make_unique<LightingRenderBindings>(device);
        lightingBindings_->createResources();

        materialBindings_ = std::make_unique<MaterialRenderBindings>(device);
        materialBindings_->createResources();

        createPipelineLayout(globalSetLayout, bindlessSetLayout);

        // Defer pipeline creation if no render pass provided (for testing API only)
        if (renderPass != VK_NULL_HANDLE) {
            createPipeline(renderPass);
        }

        // Default: use a single recording thread (serial). Caller may opt-in.
        multithreadedRecordingEnabled_ = false;
        multithreadedRecordingThreads_ = 0;
    }

    ModelRenderSystem::~ModelRenderSystem() {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);

        if (sceneColorDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device.device(), sceneColorDescriptorSetLayout_, nullptr);
        }
    }

    void ModelRenderSystem::createSceneColorDescriptorResources() {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding            = 0;
        binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount    = 1;
        binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &binding;

        if (vkCreateDescriptorSetLayout(device.device(), &layoutInfo, nullptr, &sceneColorDescriptorSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create scene color descriptor set layout");
        }

        const uint32_t count = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

        sceneColorDescriptorPool_ = engine::DescriptorPool::Builder(device).setMaxSets(count).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count).build();

        sceneColorDescriptorSets_.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!sceneColorDescriptorPool_->allocateDescriptor(sceneColorDescriptorSetLayout_, sceneColorDescriptorSets_[i])) {
                throw std::runtime_error("Failed to allocate scene color descriptor sets");
            }
        }
    }

    void ModelRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout) {
        VkPushConstantRange const pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(MeshPushConstantData),
        };

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout,
            bindlessSetLayout,
            lightingBindings_ ? lightingBindings_->getShadowDescriptorSetLayout() : VK_NULL_HANDLE,
            lightingBindings_ ? lightingBindings_->getIBLDescriptorSetLayout() : VK_NULL_HANDLE,
            materialBindings_ ? materialBindings_->getDescriptorSetLayout() : VK_NULL_HANDLE,
            sceneColorDescriptorSetLayout_};

        VkPipelineLayoutCreateInfo const pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
        };
        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create pipeline layout!");
        }
    }

    void ModelRenderSystem::createPipeline(VkRenderPass renderPass) {
        assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

        standardVariantFallbackActive_ = false;
        standardVariantFallbackReason_.clear();

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultMeshPipelineConfigInfo(pipelineConfig);

        // When a depth prepass is used, the main shading pass will often see equal depth values.
        // Using LESS here would reject those fragments, making opaque objects disappear or look wrong.
        pipelineConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        pipelineConfig.renderPass     = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;

        // Create Transparent Pipeline (alpha blend compositor)
        PipelineConfigInfo transparentConfig                       = pipelineConfig;
        transparentConfig.colorBlendAttachment.blendEnable         = VK_TRUE;
        transparentConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        transparentConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        transparentConfig.colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        transparentConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        transparentConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        transparentConfig.colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

        // Fix pointer to attachment
        transparentConfig.colorBlendInfo.pAttachments = &transparentConfig.colorBlendAttachment;

        // Disable depth write for transparent objects
        transparentConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

        transparentPipeline = std::make_unique<Pipeline>(device,
            std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
            std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
            std::string(SHADER_PATH) + R"(pbr_shader.frag.spv)",
            transparentConfig);

        try {
            standardTransparentPipeline = std::make_unique<Pipeline>(device,
                std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                std::string(SHADER_PATH) + R"(pbr_shader_standard.frag.spv)",
                transparentConfig);
        } catch (const std::exception& e) {
            Logger::warn(LogChannel::Render, "Standard transparent variant unavailable, falling back to full variant: ", e.what());
            standardVariantFallbackActive_ = true;
            if (!standardVariantFallbackReason_.empty()) {
                standardVariantFallbackReason_ += "\n";
            }
            standardVariantFallbackReason_ += "Transparent standard variant unavailable: ";
            standardVariantFallbackReason_ += e.what();
            standardTransparentPipeline = std::make_unique<Pipeline>(device,
                std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                std::string(SHADER_PATH) + R"(pbr_shader.frag.spv)",
                transparentConfig);
        }

        // Create Transmission Pipeline (no blending, no depth write; shaded refraction)
        PipelineConfigInfo transmissionConfig                = pipelineConfig;
        transmissionConfig.colorBlendAttachment.blendEnable  = VK_FALSE;
        transmissionConfig.colorBlendInfo.pAttachments       = &transmissionConfig.colorBlendAttachment;
        transmissionConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

        transmissionPipeline = std::make_unique<Pipeline>(device,
            std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
            std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
            std::string(SHADER_PATH) + R"(pbr_shader.frag.spv)",
            transmissionConfig);

        try {
            standardTransmissionPipeline = std::make_unique<Pipeline>(device,
                std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                std::string(SHADER_PATH) + R"(pbr_shader_standard.frag.spv)",
                transmissionConfig);
        } catch (const std::exception& e) {
            Logger::warn(LogChannel::Render, "Standard transmission variant unavailable, falling back to full variant: ", e.what());
            standardVariantFallbackActive_ = true;
            if (!standardVariantFallbackReason_.empty()) {
                standardVariantFallbackReason_ += "\n";
            }
            standardVariantFallbackReason_ += "Transmission standard variant unavailable: ";
            standardVariantFallbackReason_ += e.what();
            standardTransmissionPipeline = std::make_unique<Pipeline>(device,
                std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                std::string(SHADER_PATH) + R"(pbr_shader.frag.spv)",
                transmissionConfig);
        }
    }

    void ModelRenderSystem::hotReloadPipelinesIfNeeded() {
        if (!shaderHotReloadEnabled_) {
            return;
        }

        auto reload = [&](std::unique_ptr<Pipeline>& p, const char* label) {
            if (!p) {
                return;
            }
            std::string status;
            if (p->reloadIfChanged(&status)) {
                Logger::info(LogChannel::Render, "Hot-reloaded ", label, " pipeline");
            }
        };

        reload(gbufferPipeline, "gbuffer");
        reload(depthPrepassPipeline, "depth-prepass");
        reload(transparentPipeline, "transparent-full");
        reload(standardTransparentPipeline, "transparent-standard");
        reload(transmissionPipeline, "transmission-full");
        reload(standardTransmissionPipeline, "transmission-standard");
    }

    Pipeline* ModelRenderSystem::chooseTransparentPipeline(FrameInfo const& frameInfo, const PBRMaterial* material) const {
        switch (variantPolicy_) {
            case VariantPolicy::ForceFull:
                return transparentPipeline.get();
            case VariantPolicy::ForceStandard:
                return standardTransparentPipeline.get();
            case VariantPolicy::Auto:
            default:
                return MaterialRenderBindings::needsFullVariant(frameInfo, material) ? transparentPipeline.get() : standardTransparentPipeline.get();
        }
    }

    Pipeline* ModelRenderSystem::chooseTransmissionPipeline(FrameInfo const& frameInfo, const PBRMaterial* material) const {
        switch (variantPolicy_) {
            case VariantPolicy::ForceFull:
                return transmissionPipeline.get();
            case VariantPolicy::ForceStandard:
                return standardTransmissionPipeline.get();
            case VariantPolicy::Auto:
            default:
                return MaterialRenderBindings::needsFullVariant(frameInfo, material) ? transmissionPipeline.get() : standardTransmissionPipeline.get();
        }
    }

    void ModelRenderSystem::bindBaseDescriptorSets(FrameInfo& frameInfo, bool bindSceneColor) const {
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &frameInfo.globalTextureSet, 0, nullptr);
        if (bindSceneColor && !sceneColorDescriptorSets_.empty()) {
            vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 5, 1, &sceneColorDescriptorSets_[frameInfo.frameIndex], 0, nullptr);
        }
    }

    void ModelRenderSystem::createGbufferPipeline(VkRenderPass renderPass) {
        assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

        // Store for use in secondary command buffer inheritance info
        gbufferRenderPass_ = renderPass;

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultMeshPipelineConfigInfo(pipelineConfig);

        // Depth compare matches the main mesh pipeline behavior when a depth prepass is used.
        pipelineConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        // G-buffer has 4 MRTs (Normal, Albedo, Material, HDR emissive); disable blending for all.
        std::array<VkPipelineColorBlendAttachmentState, 4> attachments{};
        for (auto& a : attachments) {
            a             = pipelineConfig.colorBlendAttachment;
            a.blendEnable = VK_FALSE;
        }
        pipelineConfig.colorBlendInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        pipelineConfig.colorBlendInfo.pAttachments    = attachments.data();

        pipelineConfig.renderPass     = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;

        gbufferPipeline = std::make_unique<Pipeline>(device,
            std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
            std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
            std::string(SHADER_PATH) + R"(gbuffer.frag.spv)",
            pipelineConfig);
    }

    void ModelRenderSystem::renderGbuffer(FrameInfo& frameInfo) {
        if (!gbufferPipeline) {
            // G-buffer not enabled; no-op.
            return;
        }

        // Extract frustum for CPU culling
        glm::mat4 const vp      = frameInfo.camera.getProjection() * frameInfo.camera.getView();
        Frustum const   frustum = extractFrustumFromMatrix(vp);

        auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();

        Pipeline* boundPipeline        = nullptr;
        auto      bindPipelineIfNeeded = [&](Pipeline* p) {
            if (p != nullptr && boundPipeline != p) {
                p->bind(frameInfo.commandBuffer);
                boundPipeline = p;
            }
        };

        // When multithreading is enabled, we use VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS,
        // which means we cannot record any commands (including pipeline/descriptor binds) to the
        // primary command buffer within the render pass. All binding is done in secondary CBs.
        if (!multithreadedRecordingEnabled_) {
            // Bind pipeline + common descriptor sets once on the primary command buffer.
            bindPipelineIfNeeded(gbufferPipeline.get());
            bindBaseDescriptorSets(frameInfo, true);
        }

        // Collect work items (entity + submesh) after culling so we can partition them.
        struct RenderWorkItem {
            entt::entity          entity;
            const Model::SubMesh* subMesh;
            const PBRMaterial*    material;
            glm::mat4             modelMatrix;
        };

        std::vector<RenderWorkItem> workItems;
        workItems.reserve(256);

        for (auto entity : view) {
            auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
            if (!modelComp.model) {
                continue;
            }

            // CPU frustum culling: skip entire object if outside view
            glm::mat4 const modelMatrix = transform.modelTransform();
            if (!isEntityVisible(modelComp.model.get(), modelMatrix, frustum)) {
                continue;
            }

            const auto& subMeshes = modelComp.model->getSubMeshes();

            for (const auto& subMesh : subMeshes) {
                if (subMesh.meshletCount == 0) {
                    continue;
                }

                std::optional<PBRMaterial> mergedMaterial;
                const PBRMaterial*         pMaterial = resolveMaterialForSubMesh(frameInfo, entity, modelComp, subMesh, mergedMaterial);

                bool hasTransmission = false;
                bool isAlphaBlend    = false;
                if (pMaterial != nullptr) {
                    hasTransmission = (pMaterial->transmission > kFeatureEpsCpu) || pMaterial->hasTransmissionMap();
                    isAlphaBlend    = (pMaterial->alphaMode == AlphaMode::Blend);
                }

                if (isAlphaBlend || hasTransmission) {
                    continue;
                }

                // Add to work list for potential parallel recording
                workItems.push_back({entity, &subMesh, pMaterial, modelMatrix});
            }
        }

        // If multithreaded recording not enabled, fall back to serial inline path.
        if (!multithreadedRecordingEnabled_) {
            // Serial replay of collected items - record directly to primary command buffer
            for (const auto& item : workItems) {
                auto&                      modelComp = view.get<ModelComponent>(item.entity);
                MeshPushConstantData const push      = makeMeshPush(modelComp, *item.subMesh, item.modelMatrix);

                float const isSelected = ((uint32_t) item.entity == frameInfo.selectedObjectId) ? 1.0f : 0.0f;
                if (materialBindings_ != nullptr) {
                    materialBindings_->bindMaterial(frameInfo, pipelineLayout, item.material, isSelected);
                }

                pushConstantsAndDraw(device, frameInfo.commandBuffer, pipelineLayout, push, item.subMesh->meshletCount);
            }
            return;
        }

        // --- Secondary command buffer path (multithreaded recording enabled) ---
        // When multithreading is enabled, we MUST use secondary command buffers because the render pass
        // was begun with VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS.
        // If there's no work, we're done (no secondary CBs to execute is valid).
        if (workItems.empty()) {
            return;
        }

        // If the workload is too small for parallelism, use a single secondary CB for serial recording.
        const uint32_t threadCount =
            (workItems.size() < 32 || multithreadedRecordingThreads_ <= 1) ? 1 : std::min<uint32_t>(multithreadedRecordingThreads_, std::max<uint32_t>(1u, std::thread::hardware_concurrency() - 1));
        const size_t chunkSize = (workItems.size() + threadCount - 1) / threadCount;

        std::vector<VkCommandBuffer> secondaryBuffers;
        secondaryBuffers.reserve(threadCount);

        std::vector<std::thread> workers;
        std::atomic<size_t>      nextIndex{0};

        // Worker lambda: allocate a secondary CB, record assigned items, then return the CB.
        auto workerFn = [&](uint32_t workerId) {
            VkCommandBuffer sec = VK_NULL_HANDLE;
            if (device.allocateSecondaryCommandBuffer(&sec) != VK_SUCCESS) {
                // Allocation failed for this worker; bail out by leaving sec == VK_NULL_HANDLE
                return;
            }

            // Secondary CB must be recorded with inheritance info for the active render pass/subpass.
            // Use gbufferRenderPass_ since we're recording commands for the G-buffer pass.
            VkCommandBufferInheritanceInfo inherit{};
            inherit.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
            inherit.renderPass  = gbufferRenderPass_;
            inherit.subpass     = 0;
            inherit.framebuffer = VK_NULL_HANDLE;  // allow compatibility with the active framebuffer

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags            = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            beginInfo.pInheritanceInfo = &inherit;

            if (vkBeginCommandBuffer(sec, &beginInfo) != VK_SUCCESS) {
                device.freeSecondaryCommandBuffer(sec);
                return;
            }

            // Set dynamic state locally in the secondary CB to be independent of primary.
            // Viewport/scissor depend on frame extent.
            VkViewport vp{
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<float>(frameInfo.extent.width),
                .height   = static_cast<float>(frameInfo.extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            VkRect2D scissor{{0, 0}, frameInfo.extent};
            vkCmdSetViewport(sec, 0, 1, &vp);
            vkCmdSetScissor(sec, 0, 1, &scissor);

            // Secondary command buffers do NOT inherit pipeline state from the primary buffer.
            // We must bind the pipeline in each secondary command buffer.
            gbufferPipeline->bind(sec);

            // Local FrameInfo to pass into binding helpers (uses the secondary CB)
            FrameInfo localFrame     = frameInfo;
            localFrame.commandBuffer = sec;

            // Bind base descriptor sets in the secondary command buffer as well
            bindBaseDescriptorSets(localFrame, true);

            size_t start = nextIndex.fetch_add(chunkSize);
            while (start < workItems.size()) {
                const size_t end = std::min(workItems.size(), start + chunkSize);
                for (size_t i = start; i < end; ++i) {
                    const auto& item = workItems[i];

                    // Short critical section around material binding to avoid races in
                    // MaterialRenderBindings (allocation of dynamic offsets).
                    if (materialBindings_ != nullptr) {
                        // Diagnostic: log before and after the material bind in worker-recorded CBs.
                        std::lock_guard<std::mutex> lk(multithreadBindMutex_);
                        materialBindings_->bindMaterial(localFrame, pipelineLayout, item.material, ((uint32_t) item.entity == frameInfo.selectedObjectId) ? 1.0f : 0.0f);
                    }

                    // Compute push constants and draw (pushConstantsAndDraw is thread-safe for recording)
                    auto&                      modelComp = view.get<ModelComponent>(item.entity);
                    MeshPushConstantData const push      = makeMeshPush(modelComp, *item.subMesh, item.modelMatrix);
                    pushConstantsAndDraw(device, sec, pipelineLayout, push, item.subMesh->meshletCount);
                }

                start = nextIndex.fetch_add(chunkSize);
            }

            if (vkEndCommandBuffer(sec) != VK_SUCCESS) {
                device.freeSecondaryCommandBuffer(sec);
                return;
            }

            // Push recorded secondary buffer into the shared vector (synchronized by mutex)
            {
                std::lock_guard<std::mutex> lk(multithreadBindMutex_);
                secondaryBuffers.push_back(sec);
            }
        };

        // --- Pre-worker fast-fail validation: ensure per-frame material descriptor set is valid for workers.
        if (materialBindings_ != nullptr) {
            if (!materialBindings_->frameDescriptorSetValid(frameInfo.frameIndex)) {
                // Defensive: fail early and fall back to serial path in non-release builds so the test fails with a clear message.
                std::cerr << "[ModelRenderSystem] ERROR: material descriptor set for frame " << frameInfo.frameIndex << " is VK_NULL_HANDLE before multithreaded recording\n";
                assert(false && "material descriptor set invalid before multithreaded recording");
            }
        }

        // Launch workers
        for (uint32_t t = 0; t < threadCount; ++t) {
            workers.emplace_back(workerFn, t);
        }

        // Join workers
        for (auto& w : workers) {
            if (w.joinable())
                w.join();
        }

        // Execute recorded secondary command buffers on the primary command buffer
        if (!secondaryBuffers.empty()) {
            vkCmdExecuteCommands(frameInfo.commandBuffer, static_cast<uint32_t>(secondaryBuffers.size()), secondaryBuffers.data());
        }

        // Defer freeing secondary command buffers until the frame is complete.
        // Freeing them immediately invalidates the primary command buffer's recording.
        Device* devicePtr = &device;
        for (auto cb : secondaryBuffers) {
            device.deferDestroy([devicePtr, cb](VkDevice) { devicePtr->freeSecondaryCommandBuffer(cb); });
        }
    }

    void ModelRenderSystem::beginFrame(int frameIndex) {
        hotReloadPipelinesIfNeeded();
        if (materialBindings_ != nullptr) {
            materialBindings_->beginFrame(frameIndex);
        }
    }

    void ModelRenderSystem::enableMultiThreadedRecording(bool enable, uint32_t threadCount) {
        multithreadedRecordingEnabled_ = enable;
        if (!enable) {
            multithreadedRecordingThreads_ = 0;
            return;
        }

        if (threadCount == 0) {
            uint32_t hw                    = std::thread::hardware_concurrency();
            multithreadedRecordingThreads_ = (hw > 1) ? (hw - 1) : 1;
        } else {
            multithreadedRecordingThreads_ = threadCount;
        }
    }

    void ModelRenderSystem::updateSceneColorDescriptor(int frameIndex, VkDescriptorImageInfo const& sceneColorInfo) {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(sceneColorDescriptorSets_.size())) {
            return;
        }

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = sceneColorDescriptorSets_[frameIndex];
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &sceneColorInfo;

        vkUpdateDescriptorSets(device.device(), 1, &write, 0, nullptr);
    }

    void ModelRenderSystem::createDepthPrepassPipeline(VkRenderPass renderPass) {
        PipelineConfigInfo prepassConfig{};
        Pipeline::defaultMeshPipelineConfigInfo(prepassConfig);

        // Depth-only: no color attachments in the render pass.
        prepassConfig.colorBlendInfo.attachmentCount = 0;
        prepassConfig.colorBlendInfo.pAttachments    = nullptr;

        prepassConfig.depthStencilInfo.depthTestEnable  = VK_TRUE;
        prepassConfig.depthStencilInfo.depthWriteEnable = VK_TRUE;
        prepassConfig.depthStencilInfo.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

        prepassConfig.renderPass     = renderPass;
        prepassConfig.pipelineLayout = pipelineLayout;

        depthPrepassPipeline = std::make_unique<Pipeline>(device,
            std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
            std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
            std::string(SHADER_PATH) + R"(depth_only.frag.spv)",
            prepassConfig);
    }

    void ModelRenderSystem::setShadowSystem(ShadowSystem* shadowSystem) {
        if (lightingBindings_ != nullptr) {
            lightingBindings_->setShadowSystem(shadowSystem);
        }
    }

    void ModelRenderSystem::setIBLSystem(IBLSystem* iblSystem) {
        if (lightingBindings_ != nullptr) {
            lightingBindings_->setIBLSystem(iblSystem);
        }
    }

    MaterialDescriptorCacheStats ModelRenderSystem::getMaterialDescriptorCacheStats() const {
        if (!materialBindings_) {
            return MaterialDescriptorCacheStats{};
        }
        return materialBindings_->getCacheStats();
    }

    void ModelRenderSystem::resetMaterialDescriptorCacheStats() {
        if (!materialBindings_) {
            return;
        }
        materialBindings_->resetCacheStats();
    }

    void ModelRenderSystem::renderTransmission(FrameInfo& frameInfo) {
        // Extract frustum for CPU culling
        glm::mat4 const vp      = frameInfo.camera.getProjection() * frameInfo.camera.getView();
        Frustum const   frustum = extractFrustumFromMatrix(vp);

        Pipeline* boundPipeline        = nullptr;
        auto      bindPipelineIfNeeded = [&](Pipeline* p) {
            if (p != nullptr && boundPipeline != p) {
                p->bind(frameInfo.commandBuffer);
                boundPipeline = p;
            }
        };

        bindPipelineIfNeeded(standardTransmissionPipeline.get());
        bindBaseDescriptorSets(frameInfo, true);
        if (lightingBindings_ != nullptr) {
            lightingBindings_->bindShadow(frameInfo, pipelineLayout);
            lightingBindings_->bindIBL(frameInfo, pipelineLayout);
        }

        auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();

        auto renderItem = [&](entt::entity entity, const Model::SubMesh& subMesh, const PBRMaterial* pMaterial, const glm::mat4& modelMatrix) {
            auto& modelComp = view.get<ModelComponent>(entity);

            MeshPushConstantData const push = makeMeshPush(modelComp, subMesh, modelMatrix);

            float const isSelected = ((uint32_t) entity == frameInfo.selectedObjectId) ? 1.0f : 0.0f;
            if (materialBindings_ != nullptr) {
                materialBindings_->bindMaterial(frameInfo, pipelineLayout, pMaterial, isSelected);
            }

            pushConstantsAndDraw(device, frameInfo.commandBuffer, pipelineLayout, push, subMesh.meshletCount);
        };

        for (auto entity : view) {
            auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
            if (!modelComp.model) {
                continue;
            }

            // CPU frustum culling: skip entire object if outside view
            glm::mat4 const modelMatrix = transform.modelTransform();
            if (!isEntityVisible(modelComp.model.get(), modelMatrix, frustum)) {
                continue;
            }

            const auto& subMeshes = modelComp.model->getSubMeshes();

            for (const auto& subMesh : subMeshes) {
                if (subMesh.meshletCount == 0) {
                    continue;
                }

                std::optional<PBRMaterial> mergedMaterial;
                const PBRMaterial*         pMaterial = resolveMaterialForSubMesh(frameInfo, entity, modelComp, subMesh, mergedMaterial);

                bool hasTransmission = false;
                if (pMaterial != nullptr) {
                    hasTransmission = (pMaterial->transmission > kFeatureEpsCpu) || pMaterial->hasTransmissionMap();
                }

                if (!hasTransmission) {
                    continue;
                }

                Pipeline* desired = chooseTransmissionPipeline(frameInfo, pMaterial);
                bindPipelineIfNeeded(desired);
                renderItem(entity, subMesh, pMaterial, transform.modelTransform());
            }
        }
    }

    void ModelRenderSystem::renderAlphaBlend(FrameInfo& frameInfo) {
        // Extract frustum for CPU culling
        glm::mat4 const vp      = frameInfo.camera.getProjection() * frameInfo.camera.getView();
        Frustum const   frustum = extractFrustumFromMatrix(vp);

        Pipeline* boundPipeline        = nullptr;
        auto      bindPipelineIfNeeded = [&](Pipeline* p) {
            if (p != nullptr && boundPipeline != p) {
                p->bind(frameInfo.commandBuffer);
                boundPipeline = p;
            }
        };

        bindPipelineIfNeeded(standardTransparentPipeline.get());
        bindBaseDescriptorSets(frameInfo, true);
        if (lightingBindings_ != nullptr) {
            lightingBindings_->bindShadow(frameInfo, pipelineLayout);
            lightingBindings_->bindIBL(frameInfo, pipelineLayout);
        }

        auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();

        struct TransparentRenderItem {
            entt::entity          entity;
            const Model::SubMesh* subMesh;
            const PBRMaterial*    material;
            glm::mat4             modelMatrix;
            float                 distance;
        };

        std::vector<TransparentRenderItem> transparentItems;

        auto renderItem = [&](entt::entity entity, const Model::SubMesh& subMesh, const PBRMaterial* pMaterial, const glm::mat4& modelMatrix) {
            auto& modelComp = view.get<ModelComponent>(entity);

            MeshPushConstantData const push = makeMeshPush(modelComp, subMesh, modelMatrix);

            float const isSelected = ((uint32_t) entity == frameInfo.selectedObjectId) ? 1.0f : 0.0f;
            if (materialBindings_ != nullptr) {
                materialBindings_->bindMaterial(frameInfo, pipelineLayout, pMaterial, isSelected);
            }

            pushConstantsAndDraw(device, frameInfo.commandBuffer, pipelineLayout, push, subMesh.meshletCount);
        };

        for (auto entity : view) {
            auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
            if (!modelComp.model) {
                continue;
            }

            // CPU frustum culling: skip entire object if outside view
            glm::mat4 const modelMatrix = transform.modelTransform();
            if (!isEntityVisible(modelComp.model.get(), modelMatrix, frustum)) {
                continue;
            }

            const auto& subMeshes = modelComp.model->getSubMeshes();

            for (const auto& subMesh : subMeshes) {
                if (subMesh.meshletCount == 0) {
                    continue;
                }

                std::optional<PBRMaterial> mergedMaterial;
                const PBRMaterial*         pMaterial = resolveMaterialForSubMesh(frameInfo, entity, modelComp, subMesh, mergedMaterial);

                bool hasTransmission = false;
                bool isAlphaBlend    = false;
                if (pMaterial != nullptr) {
                    hasTransmission = (pMaterial->transmission > kFeatureEpsCpu) || pMaterial->hasTransmissionMap();
                    isAlphaBlend    = (pMaterial->alphaMode == AlphaMode::Blend);
                }

                if (!isAlphaBlend || hasTransmission) {
                    continue;
                }

                glm::vec3 const worldPos = glm::vec3(transform.modelTransform()[3]);
                float const     dist     = glm::distance(worldPos, frameInfo.camera.getPosition());
                transparentItems.push_back({entity, &subMesh, pMaterial, transform.modelTransform(), dist});
            }
        }

        std::sort(transparentItems.begin(), transparentItems.end(), [](const TransparentRenderItem& a, const TransparentRenderItem& b) { return a.distance > b.distance; });

        boundPipeline = nullptr;
        for (const auto& item : transparentItems) {
            Pipeline* desired = chooseTransparentPipeline(frameInfo, item.material);
            bindPipelineIfNeeded(desired);
            renderItem(item.entity, *item.subMesh, item.material, item.modelMatrix);
        }
    }

    void ModelRenderSystem::renderDepthPrepass(FrameInfo& frameInfo) {
        if (!depthPrepassPipeline) {
            // Depth prepass is optional until wired into RenderGraph.
            return;
        }

        // Extract frustum for CPU culling
        glm::mat4 const vp      = frameInfo.camera.getProjection() * frameInfo.camera.getView();
        Frustum const   frustum = extractFrustumFromMatrix(vp);

        Pipeline* boundPipeline        = nullptr;
        auto      bindPipelineIfNeeded = [&](Pipeline* p) {
            if (p != nullptr && boundPipeline != p) {
                p->bind(frameInfo.commandBuffer);
                boundPipeline = p;
            }
        };

        bindPipelineIfNeeded(depthPrepassPipeline.get());

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &frameInfo.globalTextureSet, 0, nullptr);

        // Bind lighting sets (pipeline layout expects it, even if not used by the depth-only fragment shader)
        if (lightingBindings_ != nullptr) {
            lightingBindings_->bindShadow(frameInfo, pipelineLayout);
            lightingBindings_->bindIBL(frameInfo, pipelineLayout);
        }

        auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();

        auto renderItem = [&](entt::entity entity, const Model::SubMesh& subMesh, const PBRMaterial* pMaterial, const glm::mat4& modelMatrix) {
            auto& modelComp = view.get<ModelComponent>(entity);

            // Depth prepass skips HZB culling (bit 2) to ensure complete depth buffer for HZB generation
            MeshPushConstantData const push = makeMeshPush(modelComp, subMesh, modelMatrix);

            // Populate a default material record so the dynamic UBO binding is always valid.
            if (materialBindings_ != nullptr) {
                materialBindings_->bindMaterial(frameInfo, pipelineLayout, nullptr, 0.0f);
            }

            pushConstantsAndDraw(device, frameInfo.commandBuffer, pipelineLayout, push, subMesh.meshletCount);
        };

        // Render opaque-only; skip masked/transparent materials for now.
        for (auto entity : view) {
            auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
            if (!modelComp.model) {
                continue;
            }

            // CPU frustum culling: skip entire object if outside view
            glm::mat4 const modelMatrix = transform.modelTransform();
            if (!isEntityVisible(modelComp.model.get(), modelMatrix, frustum)) {
                continue;
            }

            const auto& subMeshes = modelComp.model->getSubMeshes();

            for (const auto& subMesh : subMeshes) {
                if (subMesh.meshletCount == 0) {
                    continue;
                }

                std::optional<PBRMaterial> mergedMaterial;
                const PBRMaterial*         pMaterial = resolveMaterialForSubMesh(frameInfo, entity, modelComp, subMesh, mergedMaterial);

                bool prepassEligible = true;
                if (pMaterial != nullptr) {
                    if (pMaterial->alphaMode != AlphaMode::Opaque) {
                        prepassEligible = false;
                    }
                    if (pMaterial->transmission > 0.0f) {
                        prepassEligible = false;
                    }
                }

                if (!prepassEligible) {
                    continue;
                }

                bindPipelineIfNeeded(depthPrepassPipeline.get());
                renderItem(entity, subMesh, pMaterial, transform.modelTransform());
            }
        }
    }
}  // namespace engine
