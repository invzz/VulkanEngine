#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include <array>
#include <cstdint>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Scene/SunLight.hpp"
#include "Engine/Systems/IBL/IBLHelpers.hpp"

#include "vulkan/vulkan_core.h"
namespace engine {
    struct SkyLUTComputePushConstants {
        glm::vec4 betaRayleighAndG;
        glm::vec4 betaMieAndSunIntensity;
        glm::vec4 sunDirectionAndGroundRadius;
        glm::vec4 atmosphereAndScaleHeights;
    };

    struct SkyboxPushConstants {
        glm::mat4 viewProjection;
        glm::vec4 debugParams;      // x = debugCubemapFaces, y = proceduralSky, z = useSkyLUT
        glm::vec4 sunDirection;     // xyz = direction to sun, w = unused
        glm::vec4 sunColor;         // rgb = sun color, w = sun angular radius (radians, default 0.015)
        glm::vec4 skyParams;        // x = timeOfDay (0-24), y = skyIntensity, zw = unused
    };

    SkyboxRenderSystem::SkyboxRenderSystem(Device& device, VkRenderPass renderPass) : device_{device} {
        createDescriptorSetLayout();
        createPipelineLayout();
        createPipeline(renderPass);
        createSkyLUTResources();
        createSkyLUTComputeResources();
    }

    SkyboxRenderSystem::~SkyboxRenderSystem() {
        VkDevice dev = device_.device();

        if (skyLUTComputePipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(dev, skyLUTComputePipeline_, nullptr);
        }
        if (skyLUTComputeLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(dev, skyLUTComputeLayout_, nullptr);
        }
        if (skyLUTComputePool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(dev, skyLUTComputePool_, nullptr);
        }
        if (skyLUTComputeSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(dev, skyLUTComputeSetLayout_, nullptr);
        }
        if (skyLUTSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(dev, skyLUTSampler_, nullptr);
        }
        if (skyLUTImageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, skyLUTImageView_, nullptr);
        }
        if (skyLUTImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(dev, skyLUTImage_, nullptr);
        }
        if (skyLUTMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(dev, skyLUTMemory_, nullptr);
        }

        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(dev, pipelineLayout_, nullptr);
        }
        if (descriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(dev, descriptorSetLayout_, nullptr);
        }
    }

    void SkyboxRenderSystem::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding cubemapBinding{};
        cubemapBinding.binding            = 0;
        cubemapBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        cubemapBinding.descriptorCount    = 1;
        cubemapBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        cubemapBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding skyLUTBinding{};
        skyLUTBinding.binding            = 1;
        skyLUTBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        skyLUTBinding.descriptorCount    = 1;
        skyLUTBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        skyLUTBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {cubemapBinding, skyLUTBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings    = bindings.data();
        if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create skybox descriptor set layout");
        }

        const uint32_t count = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
        descriptorPool_      = engine::DescriptorPool::Builder(device_)
            .setMaxSets(count)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count * 2)
            .build();
        descriptorSets_.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!descriptorPool_->allocateDescriptor(descriptorSetLayout_, descriptorSets_[i])) {
                throw std::runtime_error("Failed to allocate skybox descriptor sets");
            }
        }
    }

    void SkyboxRenderSystem::createPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(SkyboxPushConstants);
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &descriptorSetLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushConstantRange;
        if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create skybox pipeline layout");
        }
    }

    void SkyboxRenderSystem::createPipeline(VkRenderPass renderPass) {
        PipelineConfigInfo configInfo{};
        Pipeline::defaultPipelineConfigInfo(configInfo);
        configInfo.bindingDescriptions.clear();
        configInfo.attributeDescriptions.clear();
        configInfo.inputAssemblyInfo.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        configInfo.depthStencilInfo.depthTestEnable  = VK_TRUE;
        configInfo.depthStencilInfo.depthWriteEnable = VK_FALSE;
        configInfo.depthStencilInfo.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
        configInfo.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;
        configInfo.renderPass                        = renderPass;
        configInfo.pipelineLayout                    = pipelineLayout_;
        pipeline_                                    = std::make_unique<Pipeline>(device_, std::string(SHADER_PATH) + R"(skybox_fullscreen.vert.spv)", std::string(SHADER_PATH) + R"(skybox_fullscreen.frag.spv)", configInfo);
    }

    void SkyboxRenderSystem::createSkyLUTResources() {
        ibl_detail::createImage(device_,
            kSkyLUTWidth,
            kSkyLUTHeight,
            1,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            skyLUTImage_,
            skyLUTMemory_);

        skyLUTImageView_ = ibl_detail::createImageView(device_, skyLUTImage_, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = VK_FILTER_LINEAR;
        samplerInfo.minFilter               = VK_FILTER_LINEAR;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = 0.0f;
        if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &skyLUTSampler_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Sky LUT sampler");
        }

        ibl_detail::transitionImageLayout(device_, skyLUTImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1);
        {
            VkClearColorValue clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};
            VkCommandBuffer cmd = device_.memory().beginSingleTimeCommands();
            VkImageSubresourceRange range{};
            range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel   = 0;
            range.levelCount     = 1;
            range.baseArrayLayer = 0;
            range.layerCount     = 1;
            vkCmdClearColorImage(cmd, skyLUTImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
            device_.memory().endSingleTimeCommands(cmd);
        }
        ibl_detail::transitionImageLayout(device_, skyLUTImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
        skyLUTInGeneralLayout_ = false;
    }

    void SkyboxRenderSystem::createSkyLUTComputeResources() {
        VkDescriptorSetLayoutBinding storageBinding{};
        storageBinding.binding            = 0;
        storageBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        storageBinding.descriptorCount    = 1;
        storageBinding.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
        storageBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &storageBinding;
        if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &skyLUTComputeSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Sky LUT compute descriptor set layout");
        }

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset     = 0;
        pushRange.size       = sizeof(SkyLUTComputePushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &skyLUTComputeSetLayout_;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &pushRange;
        if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &skyLUTComputeLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Sky LUT compute pipeline layout");
        }

        auto compCode = Pipeline::readFile(std::string(SHADER_PATH) + "sky_lut.comp.spv");
        VkShaderModule compModule = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = compCode.size();
        shaderInfo.pCode    = reinterpret_cast<const uint32_t*>(compCode.data());
        if (vkCreateShaderModule(device_.device(), &shaderInfo, nullptr, &compModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Sky LUT compute shader module");
        }

        VkPipelineShaderStageCreateInfo shaderStage{};
        shaderStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStage.module = compModule;
        shaderStage.pName  = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage  = shaderStage;
        pipelineInfo.layout = skyLUTComputeLayout_;
        if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &skyLUTComputePipeline_) != VK_SUCCESS) {
            vkDestroyShaderModule(device_.device(), compModule, nullptr);
            throw std::runtime_error("Failed to create Sky LUT compute pipeline");
        }
        vkDestroyShaderModule(device_.device(), compModule, nullptr);

        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = 1;
        if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &skyLUTComputePool_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Sky LUT compute descriptor pool");
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = skyLUTComputePool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &skyLUTComputeSetLayout_;
        if (vkAllocateDescriptorSets(device_.device(), &allocInfo, &skyLUTComputeSet_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate Sky LUT compute descriptor set");
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView   = skyLUTImageView_;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = skyLUTComputeSet_;
        write.dstBinding      = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo;
        vkUpdateDescriptorSets(device_.device(), 1, &write, 0, nullptr);
    }

    void SkyboxRenderSystem::updateSkyLUTIfNeeded(const SkyboxSettings& settings, const glm::vec3& sunDirection) {
        if (!settings.proceduralSky || !settings.useSkyLUT) {
            return;
        }

        constexpr float updateThreshold = 0.01f;
        bool needsUpdate = !skyLUTReady_ ||
                          (std::fabs(settings.timeOfDay - skyLUTLastTimeOfDay_) > updateThreshold) ||
                          (std::fabs(settings.latitude - skyLUTLastLatitude_) > 0.01f) ||
                          (settings.dayOfYear != skyLUTLastDayOfYear_);
        if (!needsUpdate) {
            return;
        }

        if (!skyLUTInGeneralLayout_) {
            ibl_detail::transitionImageLayout(device_, skyLUTImage_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1);
            skyLUTInGeneralLayout_ = true;
        }

        SkyLUTComputePushConstants push{};
        push.betaRayleighAndG         = glm::vec4(static_cast<float>(settings.betaRayleigh.x), static_cast<float>(settings.betaRayleigh.y), static_cast<float>(settings.betaRayleigh.z), settings.mieG);
        push.betaMieAndSunIntensity   = glm::vec4(static_cast<float>(settings.betaMie.x), static_cast<float>(settings.betaMie.y), static_cast<float>(settings.betaMie.z), static_cast<float>(settings.sunIntensity));
        push.sunDirectionAndGroundRadius = glm::vec4(glm::normalize(sunDirection), 6360e3f);
        push.atmosphereAndScaleHeights   = glm::vec4(static_cast<float>(settings.atmosphereRadius), static_cast<float>(settings.rayleighScaleHeight), static_cast<float>(settings.mieScaleHeight), 0.0f);

        VkCommandBuffer cmd = device_.memory().beginSingleTimeCommands();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, skyLUTComputePipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, skyLUTComputeLayout_, 0, 1, &skyLUTComputeSet_, 0, nullptr);
        vkCmdPushConstants(cmd, skyLUTComputeLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SkyLUTComputePushConstants), &push);

        constexpr uint32_t kGroup = 16;
        uint32_t groupX = (kSkyLUTWidth + (kGroup - 1)) / kGroup;
        uint32_t groupY = (kSkyLUTHeight + (kGroup - 1)) / kGroup;
        vkCmdDispatch(cmd, groupX, groupY, 1);
        device_.memory().endSingleTimeCommands(cmd);

        ibl_detail::transitionImageLayout(device_, skyLUTImage_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
        skyLUTInGeneralLayout_ = false;
        skyLUTReady_           = true;
        skyLUTLastTimeOfDay_   = settings.timeOfDay;
        skyLUTLastLatitude_    = settings.latitude;
        skyLUTLastDayOfYear_   = settings.dayOfYear;
    }

    void SkyboxRenderSystem::render(FrameInfo& frameInfo, Skybox* skybox, const SkyboxSettings& settings) {
        const bool procedural = settings.proceduralSky && (skybox == nullptr);

        // Descriptor set 0 always expects a valid combined image sampler.
        // In procedural mode the shader ignores the cubemap sample path, but
        // Vulkan still requires a valid bound descriptor.
        Skybox* sampledSkybox = skybox;
        if (sampledSkybox == nullptr) {
            if (!fallbackSkybox_) {
                try {
                    fallbackSkybox_ = Skybox::loadFromFolder(device_, std::string(TEXTURE_PATH) + "/skybox/Yokohama", "jpg");
                } catch (const std::exception& e) {
                    Logger::warn(LogChannel::Render,
                        "SkyboxRenderSystem: fallback cubemap load failed; skipping sky draw this frame: ", e.what());
                }
            }
            sampledSkybox = fallbackSkybox_.get();
        }
        if (sampledSkybox == nullptr) {
            return;
        }

        // Build view matrix without translation so sky stays centered on camera
        glm::mat4 view = frameInfo.camera.getView();
        view[3]        = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::mat4 vp   = frameInfo.camera.getProjection() * view;

        // Compute sun direction from time of day
        const glm::vec3 sunDir = sunDirectionFromTimeOfDay(settings.timeOfDay, settings.latitude, static_cast<float>(settings.dayOfYear));

        updateSkyLUTIfNeeded(settings, sunDir);
        const bool useSkyLUT = procedural && settings.useSkyLUT && skyLUTReady_;

        // Physically-derived sun colour: chromatic transmittance of direct
        // sunlight through the atmosphere (reddens at the horizon, white at
        // zenith). Matches the LUT's extinction exactly.
        glm::vec3 sunCol = computeSunDirectColor(sunDir,
            static_cast<float>(settings.atmosphereRadius),
            glm::max(glm::vec3(settings.betaRayleigh), glm::vec3(0.0f)),
            glm::max(glm::vec3(settings.betaMie), glm::vec3(0.0f)),
            static_cast<float>(settings.rayleighScaleHeight),
            static_cast<float>(settings.mieScaleHeight));
        if (glm::all(glm::equal(sunCol, glm::vec3(0.0f)))) {
            sunCol = glm::vec3(1.0f, 0.35f, 0.1f);  // deep below horizon: keep a warm ember
        }

        // Night darkening factor
        const float nightFactor = glm::smoothstep(-0.05f, 0.15f, sunDir.y);
        const float effectiveIntensity = settings.skyIntensity * glm::mix(0.02f, 1.0f, nightFactor);

        SkyboxPushConstants push{};
        push.viewProjection                   = vp;
        push.debugParams                      = glm::vec4(
            settings.debugCubemapFaces ? 1.0f : 0.0f,   // x
            procedural ? 1.0f : 0.0f,                     // y
            useSkyLUT ? 1.0f : 0.0f,                      // z
            0.0f);
        push.sunDirection                     = glm::vec4(sunDir, 0.0f);
        push.sunColor                         = glm::vec4(sunCol, 0.015f);
        push.skyParams                        = glm::vec4(settings.timeOfDay, effectiveIntensity, 0.0f, 0.0f);

        // Descriptor 0: cubemap fallback/sampling.
        VkDescriptorImageInfo cubemapImageInfo = sampledSkybox->getDescriptorInfo();

        // Descriptor 1: Sky LUT (always bound, used only when debugParams.z > 0.5).
        VkDescriptorImageInfo skyLUTImageInfo{};
        skyLUTImageInfo.sampler     = skyLUTSampler_;
        skyLUTImageInfo.imageView   = skyLUTImageView_;
        skyLUTImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = descriptorSets_[frameInfo.frameIndex];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo      = &cubemapImageInfo;

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = descriptorSets_[frameInfo.frameIndex];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo      = &skyLUTImageInfo;

        vkUpdateDescriptorSets(device_.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        pipeline_->bind(frameInfo.commandBuffer);
        assert(descriptorSets_[frameInfo.frameIndex] != VK_NULL_HANDLE && "SkyboxRenderSystem: descriptor set is null");
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[frameInfo.frameIndex],
            0,
            nullptr);
        vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyboxPushConstants), &push);
        vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
    }
}  // namespace engine
