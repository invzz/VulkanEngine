#include "Engine/Systems/MaterialRenderBindings.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/SwapChain.hpp"

#include "ModelLib/Resources/MaterialUniformData.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "ModelLib/Resources/Texture.hpp"

namespace engine {
    namespace {
        constexpr float kFeatureEpsCpu = 0.01f;

        bool materialDebugLoggingEnabled() {
            static bool const enabled = []() {
                const char* value = std::getenv("ENGINE_DEBUG_MATERIAL_BINDINGS");
                return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
            }();
            return enabled;
        }

        void logMaterialBindingSummaryOnce(const PBRMaterial& material,
            uint32_t                                          textureFlags,
            uint32_t                                          albedoIndex,
            uint32_t                                          normalIndex,
            uint32_t                                          metallicIndex,
            uint32_t                                          roughnessIndex,
            uint32_t                                          aoIndex,
            uint32_t                                          emissiveIndex) {
            if (!materialDebugLoggingEnabled()) {
                return;
            }

            static std::mutex                      logMutex;
            static std::unordered_set<std::string> loggedSummaries;

            std::string summary = std::string("flags=") + std::to_string(textureFlags) + " albedo=" + std::to_string(albedoIndex) + " normal=" + std::to_string(normalIndex) + " metallic=" + std::to_string(metallicIndex) + " roughness=" + std::to_string(roughnessIndex) + " ao=" + std::to_string(aoIndex) + " emissive=" + std::to_string(emissiveIndex) + " packedMR=" + (material.useMetallicRoughnessTexture ? "1" : "0") + " packedORM=" + (material.useOcclusionRoughnessMetallicTexture ? "1" : "0") + " hasAlbedo=" + (material.hasAlbedoMap() ? "1" : "0") + " hasNormal=" + (material.hasNormalMap() ? "1" : "0") + " hasRoughness=" + (material.hasRoughnessMap() ? "1" : "0") + " hasAO=" + (material.hasAOMap() ? "1" : "0");

            std::lock_guard<std::mutex> lock(logMutex);
            if (loggedSummaries.size() >= 32 || loggedSummaries.contains(summary)) {
                return;
            }

            loggedSummaries.insert(summary);
            engine::Logger::info(engine::LogChannel::Render, "[MaterialBinding] ", summary);
        }
    }  // namespace

    MaterialRenderBindings::MaterialRenderBindings(Device& device) : device_(device) {}

    MaterialRenderBindings::~MaterialRenderBindings() {
        if (descriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_.device(), descriptorSetLayout_, nullptr);
        }
    }

    void MaterialRenderBindings::createResources() {
        createDescriptorSetLayout();

        atomSize_ = materialAtomSize();

        createBuffers();
        createPoolAndSets();
        updateDescriptorSets();

        dynamicOffsetIndexByFrame_.resize(SwapChain::maxFramesInFlight(), 0u);
        materialOffsetCacheByFrame_.resize(SwapChain::maxFramesInFlight());
    }

    void MaterialRenderBindings::beginFrame(int frameIndex) {
        if (frameIndex < 0)
            return;
        if (dynamicOffsetIndexByFrame_.empty()) {
            dynamicOffsetIndexByFrame_.resize(SwapChain::maxFramesInFlight(), 0u);
        }
        if (frameIndex < static_cast<int>(dynamicOffsetIndexByFrame_.size())) {
            dynamicOffsetIndexByFrame_[frameIndex] = 0u;
            if (frameIndex < static_cast<int>(materialOffsetCacheByFrame_.size())) {
                materialOffsetCacheByFrame_[frameIndex].clear();
            }
        }
    }

    MaterialDescriptorCacheStats MaterialRenderBindings::getCacheStats() const {
        std::lock_guard<std::mutex> lk(allocMutex_);
        return cacheStats_;
    }

    void MaterialRenderBindings::resetCacheStats() {
        std::lock_guard<std::mutex> lk(allocMutex_);
        cacheStats_ = MaterialDescriptorCacheStats{};
    }

    // Return true if the per-frame descriptor set (used for material binding) is present and non-null.
    bool MaterialRenderBindings::frameDescriptorSetValid(int frameIndex) const {
        if (frameIndex < 0)
            return false;
        if (frameIndex >= static_cast<int>(descriptorSets_.size()))
            return false;
        return descriptorSets_[frameIndex] != VK_NULL_HANDLE;
    }

    void MaterialRenderBindings::enableBindCapture(bool enable) {
        std::lock_guard<std::mutex> lk(captureMutex_);
        captureEnabled_ = enable;
        if (!enable) {
            capturedBinds_.clear();
        }
    }

    std::vector<VkDescriptorSet> MaterialRenderBindings::getCapturedBinds() const {
        std::lock_guard<std::mutex> lk(captureMutex_);
        return capturedBinds_;
    }

    VkDescriptorSet MaterialRenderBindings::getFrameDescriptorSet(int frameIndex) const {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(descriptorSets_.size()))
            return VK_NULL_HANDLE;
        return descriptorSets_[frameIndex];
    }

    VkDeviceSize MaterialRenderBindings::materialAtomSize() const {
        VkDeviceSize const minAlignment = device_.getProperties().limits.minUniformBufferOffsetAlignment;
        VkDeviceSize       atomSize     = sizeof(MaterialUniformData);
        if (minAlignment > 0) {
            atomSize = (atomSize + minAlignment - 1) & ~(minAlignment - 1);
        }
        return atomSize;
    }

    void MaterialRenderBindings::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding            = 0;
        binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        binding.descriptorCount    = 1;
        binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &binding;

        if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create material descriptor set layout");
        }
    }

    void MaterialRenderBindings::createBuffers() {
        buffers_.resize(SwapChain::maxFramesInFlight());
        for (size_t i = 0; std::cmp_less(i, SwapChain::maxFramesInFlight()); i++) {
            buffers_[i] =
                std::make_unique<Buffer>(device_, atomSize_, kMaxMaterialRecordsPerFrame, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            buffers_[i]->map();
        }
    }

    void MaterialRenderBindings::createPoolAndSets() {
        const uint32_t count = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

        descriptorPool_ = engine::DescriptorPool::Builder(device_).setMaxSets(count).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, count).build();

        descriptorSets_.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!descriptorPool_->allocateDescriptor(descriptorSetLayout_, descriptorSets_[i])) {
                throw std::runtime_error("Failed to allocate material descriptor sets");
            }
        }
    }

    void MaterialRenderBindings::updateDescriptorSets() {
        for (size_t i = 0; std::cmp_less(i, SwapChain::maxFramesInFlight()); i++) {
            VkDescriptorBufferInfo const bufferInfo = buffers_[i]->descriptorInfoForIndex(0);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet          = descriptorSets_[i];
            descriptorWrite.dstBinding      = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo     = &bufferInfo;

            vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    namespace {
        MaterialUniformData buildMaterialUniformData(const PBRMaterial* pMaterial, float isSelected) {
            MaterialUniformData matData{};

            if (pMaterial != nullptr) {
                const auto& material = *pMaterial;

                uint32_t textureFlags            = 0;
                uint32_t albedoIndex             = 0;
                uint32_t normalIndex             = 0;
                uint32_t metallicIndex           = 0;
                uint32_t roughnessIndex          = 0;
                uint32_t aoIndex                 = 0;
                uint32_t emissiveIndex           = 0;
                uint32_t specularGlossinessIndex = 0;
                uint32_t transmissionIndex       = 0;
                uint32_t clearcoatIndex          = 0;
                uint32_t clearcoatRoughnessIndex = 0;
                uint32_t clearcoatNormalIndex    = 0;

                if (material.hasAlbedoMap()) {
                    textureFlags |= (1 << 0);
                    albedoIndex = material.albedoMap->getGlobalIndex();
                }
                if (material.hasNormalMap()) {
                    textureFlags |= (1 << 1);
                    normalIndex = material.normalMap->getGlobalIndex();
                }
                if (material.hasMetallicMap()) {
                    textureFlags |= (1 << 2);
                    metallicIndex = material.metallicMap->getGlobalIndex();
                }
                if (material.hasRoughnessMap()) {
                    textureFlags |= (1 << 3);
                    roughnessIndex = material.roughnessMap->getGlobalIndex();
                }
                if (material.hasAOMap()) {
                    textureFlags |= (1 << 4);
                    aoIndex = material.aoMap->getGlobalIndex();
                }
                if (material.hasEmissiveMap()) {
                    textureFlags |= (1 << 5);
                    emissiveIndex = material.emissiveMap->getGlobalIndex();
                }

                if (material.specularGlossinessMap) {
                    textureFlags |= (1 << 8);
                    specularGlossinessIndex = material.specularGlossinessMap->getGlobalIndex();
                }

                if (material.hasTransmissionMap()) {
                    textureFlags |= (1 << 9);
                    transmissionIndex = material.transmissionMap->getGlobalIndex();
                }
                if (material.hasClearcoatMap()) {
                    textureFlags |= (1 << 10);
                    clearcoatIndex = material.clearcoatMap->getGlobalIndex();
                }
                if (material.hasClearcoatRoughnessMap()) {
                    textureFlags |= (1 << 11);
                    clearcoatRoughnessIndex = material.clearcoatRoughnessMap->getGlobalIndex();
                }
                if (material.hasClearcoatNormalMap()) {
                    textureFlags |= (1 << 12);
                    clearcoatNormalIndex = material.clearcoatNormalMap->getGlobalIndex();
                }

                if (material.useMetallicRoughnessTexture) {
                    textureFlags |= (1 << 6);
                }
                if (material.useOcclusionRoughnessMetallicTexture) {
                    textureFlags |= (1 << 7);
                }

                logMaterialBindingSummaryOnce(material, textureFlags, albedoIndex, normalIndex, metallicIndex, roughnessIndex, aoIndex, emissiveIndex);

                matData.albedo                   = material.albedo;
                matData.emissiveInfo             = glm::vec4(material.emissiveColor, material.emissiveStrength);
                matData.specularGlossinessFactor = glm::vec4(material.specularFactor, material.glossinessFactor);
                matData.attenuationColorAndDist  = glm::vec4(material.attenuationColor, material.attenuationDistance);

                // Col 0
                matData.params[0][0] = material.metallic;
                matData.params[0][1] = material.roughness;
                matData.params[0][2] = material.ao;
                matData.params[0][3] = isSelected;
                // Col 1
                matData.params[1][0] = material.clearcoat;
                matData.params[1][1] = material.clearcoatRoughness;
                matData.params[1][2] = material.anisotropic;
                matData.params[1][3] = material.anisotropicRotation;
                // Col 2
                matData.params[2][0] = material.transmission;
                matData.params[2][1] = material.ior;
                matData.params[2][2] = material.iridescence;
                matData.params[2][3] = material.iridescenceIOR;
                // Col 3
                matData.params[3][0] = material.iridescenceThickness;
                matData.params[3][1] = material.uvScale;
                matData.params[3][2] = material.alphaCutoff;
                matData.params[3][3] = material.thickness;

                matData.flagsAndIndices0.x = textureFlags;
                matData.flagsAndIndices0.y = static_cast<uint32_t>(material.alphaMode);
                matData.flagsAndIndices0.z = albedoIndex;
                matData.flagsAndIndices0.w = normalIndex;

                matData.indices1.x = metallicIndex;
                matData.indices1.y = roughnessIndex;
                matData.indices1.z = aoIndex;
                matData.indices1.w = emissiveIndex;

                matData.indices2.x = specularGlossinessIndex;
                matData.indices2.y = material.useSpecularGlossinessWorkflow ? 1 : 0;
                matData.indices2.z = transmissionIndex;
                matData.indices2.w = clearcoatIndex;

                matData.indices3.x = clearcoatRoughnessIndex;
                matData.indices3.y = clearcoatNormalIndex;
                matData.indices3.z = 0;  // Reserved (was lightmapIndex)
            } else {
                matData.albedo                   = glm::vec4(1.0f);
                matData.emissiveInfo             = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                matData.specularGlossinessFactor = glm::vec4(1.0f);
                matData.attenuationColorAndDist  = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

                matData.params[0][0] = 0.0f;  // metallic
                matData.params[0][1] = 0.5f;  // roughness
                matData.params[0][2] = 1.0f;  // ao
                matData.params[0][3] = isSelected;

                matData.params[1][0] = 0.0f;   // clearcoat
                matData.params[1][1] = 0.03f;  // clearcoatRoughness
                matData.params[1][2] = 0.0f;   // anisotropic
                matData.params[1][3] = 0.0f;   // anisotropicRotation

                matData.params[2][0] = 0.0f;  // transmission
                matData.params[2][1] = 1.5f;  // ior
                matData.params[2][2] = 0.0f;  // iridescence
                matData.params[2][3] = 1.3f;  // iridescenceIOR

                matData.params[3][0] = 100.0f;  // iridescenceThickness
                matData.params[3][1] = 1.0f;    // uvScale
                matData.params[3][2] = 0.5f;    // alphaCutoff
                matData.params[3][3] = 0.0f;    // thickness

                matData.flagsAndIndices0 = glm::uvec4(0);
                matData.indices1         = glm::uvec4(0);
                matData.indices2         = glm::uvec4(0);
            }

            return matData;
        }

    }  // namespace

    void MaterialRenderBindings::writeAndBind(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const void* data, VkDeviceSize dataSize) {
        if (frameInfo.frameIndex < 0 || frameInfo.frameIndex >= static_cast<int>(buffers_.size())) {
            return;
        }

        uint32_t& dynamicOffsetIndex = dynamicOffsetIndexByFrame_[frameInfo.frameIndex];
        if (dynamicOffsetIndex >= kMaxMaterialRecordsPerFrame) {
            return;
        }

        {
            std::lock_guard<std::mutex> allocLk(allocMutex_);

            // Defensive: ensure the per-frame descriptor set is valid before binding.
            if (descriptorSets_[frameInfo.frameIndex] == VK_NULL_HANDLE) {
                return;
            }

            char* mappedData = reinterpret_cast<char*>(buffers_[frameInfo.frameIndex]->getMappedMemory());
            std::memcpy(mappedData + (dynamicOffsetIndex * atomSize_), data, static_cast<size_t>(dataSize));

            uint32_t const dynamicOffset = static_cast<uint32_t>(dynamicOffsetIndex * atomSize_);
            assert(descriptorSets_[frameInfo.frameIndex] != VK_NULL_HANDLE && "MaterialRenderBindings: descriptor set is null");
            vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, kMaterialSetIndex, 1, &descriptorSets_[frameInfo.frameIndex], 1, &dynamicOffset);

            // Capture bind for diagnostic/tests (thread-safe)
            {
                std::lock_guard<std::mutex> lk(captureMutex_);
                if (captureEnabled_) {
                    capturedBinds_.push_back(descriptorSets_[frameInfo.frameIndex]);
                }
            }

            dynamicOffsetIndex++;
        }
    }

    void MaterialRenderBindings::bindMaterial(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const PBRMaterial* material, float isSelected) {
        MaterialUniformData const matData = buildMaterialUniformData(material, isSelected);

        if (frameInfo.frameIndex < 0 || frameInfo.frameIndex >= static_cast<int>(buffers_.size())) {
            return;
        }

        auto fnv1a64 = [](const void* data, size_t size) -> uint64_t {
            auto const* bytes = reinterpret_cast<const uint8_t*>(data);
            uint64_t    hash  = 14695981039346656037ULL;
            for (size_t i = 0; i < size; ++i) {
                hash ^= static_cast<uint64_t>(bytes[i]);
                hash *= 1099511628211ULL;
            }
            return hash;
        };

        uint64_t const materialKey = fnv1a64(&matData, sizeof(MaterialUniformData));

        std::lock_guard<std::mutex> allocLk(allocMutex_);

        if (descriptorSets_[frameInfo.frameIndex] == VK_NULL_HANDLE) {
            return;
        }

        auto&      cache = materialOffsetCacheByFrame_[frameInfo.frameIndex];
        auto const found = cache.find(materialKey);
        if (found != cache.end()) {
            uint32_t const dynamicOffset = found->second;
            assert(descriptorSets_[frameInfo.frameIndex] != VK_NULL_HANDLE && "MaterialRenderBindings: descriptor set is null");
            vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, kMaterialSetIndex, 1, &descriptorSets_[frameInfo.frameIndex], 1, &dynamicOffset);

            {
                std::lock_guard<std::mutex> lk(captureMutex_);
                if (captureEnabled_) {
                    capturedBinds_.push_back(descriptorSets_[frameInfo.frameIndex]);
                }
            }

            cacheStats_.cacheHits++;
            return;
        }

        uint32_t& dynamicOffsetIndex = dynamicOffsetIndexByFrame_[frameInfo.frameIndex];
        if (dynamicOffsetIndex >= kMaxMaterialRecordsPerFrame) {
            return;
        }

        char* mappedData = reinterpret_cast<char*>(buffers_[frameInfo.frameIndex]->getMappedMemory());
        std::memcpy(mappedData + (dynamicOffsetIndex * atomSize_), &matData, static_cast<size_t>(sizeof(MaterialUniformData)));

        uint32_t const dynamicOffset = static_cast<uint32_t>(dynamicOffsetIndex * atomSize_);
        assert(descriptorSets_[frameInfo.frameIndex] != VK_NULL_HANDLE && "MaterialRenderBindings: descriptor set is null");
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, kMaterialSetIndex, 1, &descriptorSets_[frameInfo.frameIndex], 1, &dynamicOffset);

        {
            std::lock_guard<std::mutex> lk(captureMutex_);
            if (captureEnabled_) {
                capturedBinds_.push_back(descriptorSets_[frameInfo.frameIndex]);
            }
        }

        cache[materialKey] = dynamicOffset;
        dynamicOffsetIndex++;
        cacheStats_.cacheMisses++;
        cacheStats_.bufferWrites++;
    }

    bool MaterialRenderBindings::needsFullVariant(FrameInfo const& frameInfo, const PBRMaterial* mat) {
        if (frameInfo.debugMode != 0) {
            return true;
        }
        if (mat == nullptr) {
            return false;
        }
        if (mat->useSpecularGlossinessWorkflow) {
            return true;
        }
        if (mat->iridescence > kFeatureEpsCpu) {
            return true;
        }
        if (mat->transmission > kFeatureEpsCpu || mat->hasTransmissionMap()) {
            return true;
        }
        if (mat->clearcoat > kFeatureEpsCpu || mat->hasClearcoatMap() || mat->hasClearcoatRoughnessMap() || mat->hasClearcoatNormalMap()) {
            return true;
        }
        if (mat->anisotropic > kFeatureEpsCpu || mat->anisotropic < -kFeatureEpsCpu) {
            return true;
        }
        return false;
    }

}  // namespace engine
