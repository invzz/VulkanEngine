#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include <array>
#include <stdexcept>

#include "Engine/Graphics/SwapChain.hpp"

namespace engine {

  struct SkyboxPushConstants
  {
    glm::mat4 viewProjection;
  };

  SkyboxRenderSystem::SkyboxRenderSystem(Device& device, VkRenderPass renderPass) : device_{device}
  {
    createDescriptorSetLayout();
    createPipelineLayout();
    createPipeline(renderPass);
  }

  SkyboxRenderSystem::~SkyboxRenderSystem()
  {
    if (descriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device_.device(), descriptorPool_, nullptr);
    }
    if (pipelineLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorSetLayout(device_.device(), descriptorSetLayout_, nullptr);
    }
  }

  void SkyboxRenderSystem::createDescriptorSetLayout()
  {
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

    if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create skybox descriptor set layout");
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create skybox descriptor pool");
    }

    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(SwapChain::maxFramesInFlight(), descriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
    allocInfo.pSetLayouts        = layouts.data();

    descriptorSets_.resize(SwapChain::maxFramesInFlight());
    if (vkAllocateDescriptorSets(device_.device(), &allocInfo, descriptorSets_.data()) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate skybox descriptor sets");
    }
  }

  void SkyboxRenderSystem::createPipelineLayout()
  {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(SkyboxPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &descriptorSetLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create skybox pipeline layout");
    }
  }

  void SkyboxRenderSystem::createPipeline(VkRenderPass renderPass)
  {
    PipelineConfigInfo configInfo{};
    Pipeline::defaultPipelineConfigInfo(configInfo);

    // No vertex input - we generate vertices in the shader
    configInfo.bindingDescriptions.clear();
    configInfo.attributeDescriptions.clear();

    // Draw triangles
    configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Disable depth test - skybox renders first, everything else will overdraw
    configInfo.depthStencilInfo.depthTestEnable  = VK_FALSE;
    configInfo.depthStencilInfo.depthWriteEnable = VK_FALSE;

    // Disable culling for debugging
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

    configInfo.renderPass     = renderPass;
    configInfo.pipelineLayout = pipelineLayout_;

    pipeline_ = std::make_unique<Pipeline>(device_, SHADER_PATH "/skybox.vert.spv", SHADER_PATH "/skybox.frag.spv", configInfo);
  }

  void SkyboxRenderSystem::render(FrameInfo& frameInfo, Skybox& skybox)
  {
    // Update descriptor set with skybox texture
    VkDescriptorImageInfo imageInfo = skybox.getDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = descriptorSets_[frameInfo.frameIndex];
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);

    // Bind pipeline
    pipeline_->bind(frameInfo.commandBuffer);

    // Bind descriptor set
    vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_,
                            0, // first set
                            1, // set count
                            &descriptorSets_[frameInfo.frameIndex],
                            0,
                            nullptr);

    // Create view-projection matrix without translation
    // This keeps the skybox centered on the camera
    glm::mat4 view = frameInfo.camera.getView();
    view[3]        = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Remove translation

    SkyboxPushConstants push{};
    push.viewProjection = frameInfo.camera.getProjection() * view;

    vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SkyboxPushConstants), &push);

    // Draw cube (36 vertices, no vertex buffer)
    vkCmdDraw(frameInfo.commandBuffer, 36, 1, 0, 0);
  }

} // namespace engine
