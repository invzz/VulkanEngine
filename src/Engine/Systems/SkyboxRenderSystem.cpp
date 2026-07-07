#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Scene/Skybox.hpp"

#include "vulkan/vulkan_core.h"
namespace engine {
    struct SkyboxPushConstants {
        glm::mat4 viewProjection;
        glm::vec4 debugParams;      // x = debugCubemapFaces (1/0), y = proceduralSky (1/0)
        glm::vec4 sunDirection;     // xyz = direction to sun, w = unused
        glm::vec4 sunColor;         // rgb = sun color, w = sun angular radius (radians, default 0.015)
        glm::vec4 skyParams;        // x = timeOfDay (0-24), y = skyIntensity, zw = unused
    };
    SkyboxRenderSystem::SkyboxRenderSystem(Device& device, VkRenderPass renderPass) : device_{device} {
        createDescriptorSetLayout();
        createPipelineLayout();
        createPipeline(renderPass);
    }
    SkyboxRenderSystem::~SkyboxRenderSystem() {
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
        }
        if (descriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_.device(), descriptorSetLayout_, nullptr);
        }
    }
    void SkyboxRenderSystem::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding            = 0;
        samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.descriptorCount    = 1;
        samplerBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerBinding.pImmutableSamplers = nullptr;
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &samplerBinding;
        if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create skybox descriptor set layout");
        }
        const uint32_t count = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
        descriptorPool_      = engine::DescriptorPool::Builder(device_).setMaxSets(count).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count).build();
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
        pushConstantRange.size       = sizeof(SkyboxPushConstants);  // 80 bytes
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
    void SkyboxRenderSystem::render(FrameInfo& frameInfo, Skybox* skybox, const SkyboxSettings& settings) {
        const bool procedural = settings.proceduralSky && (skybox == nullptr);

        // Build view matrix without translation so sky stays centered on camera
        glm::mat4 view = frameInfo.camera.getView();
        view[3]        = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::mat4 vp   = frameInfo.camera.getProjection() * view;

        // Compute sun direction from time of day
        const float t = settings.timeOfDay;
        const float elev = sinf((t - 6.0f) / 24.0f * 6.2831853f);
        const float cosElev = sqrtf(fmaxf(1.0f - elev * elev, 0.0f));
        const float azimuth = (t - 6.0f) / 24.0f * 6.2831853f;
        const glm::vec3 sunDir(cosElev * cosf(azimuth), elev, cosElev * sinf(azimuth));

        // Sun color: warm at horizon, white at zenith
        const float elevNorm = glm::clamp(elev, 0.0f, 1.0f);
        const glm::vec3 sunHorizon(1.0f, 0.55f, 0.2f);
        const glm::vec3 sunZenith(1.0f, 0.98f, 0.92f);
        const glm::vec3 sunCol = glm::mix(sunHorizon, sunZenith, glm::smoothstep(0.0f, 0.3f, elevNorm));

        // Night darkening factor
        const float nightFactor = glm::smoothstep(-0.05f, 0.15f, elev);
        const float effectiveIntensity = settings.skyIntensity * glm::mix(0.02f, 1.0f, nightFactor);

        SkyboxPushConstants push{};
        push.viewProjection                   = vp;
        push.debugParams                      = glm::vec4(
            settings.debugCubemapFaces ? 1.0f : 0.0f,   // x
            procedural ? 1.0f : 0.0f,                     // y
            0.0f, 0.0f);
        push.sunDirection                     = glm::vec4(sunDir, 0.0f);
        push.sunColor                         = glm::vec4(sunCol, 0.015f);
        push.skyParams                        = glm::vec4(settings.timeOfDay, effectiveIntensity, 0.0f, 0.0f);

        // Descriptor: always bind a sampler (shader ignores it in procedural mode)
        VkDescriptorImageInfo imageInfo{};
        if (!procedural && skybox != nullptr) {
            imageInfo = skybox->getDescriptorInfo();
        }
        // Otherwise leave as zero — shader takes procedural path via debugParams.y > 0.5

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = descriptorSets_[frameInfo.frameIndex];
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo      = &imageInfo;
        vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

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
