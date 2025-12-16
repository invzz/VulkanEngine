#include "Engine/Systems/DustRenderSystem.hpp"

#include <array>
#include <random>

namespace engine {

  struct DustPushConstants
  {
    glm::mat4 viewProjection;
    glm::vec4 cameraPosition; // w = boxSize
    glm::vec4 params;         // x = time, y = size, z = alpha, w = heightFalloff
    glm::vec4 sunDirection;   // xyz = direction, w = unused
    glm::vec4 sunColor;       // xyz = color, w = unused
    glm::vec4 ambientColor;   // xyz = color, w = unused
  };

  DustRenderSystem::DustRenderSystem(Device& device, VkRenderPass renderPass) : device{device}
  {
    createPipelineLayout();
    createPipeline(renderPass);
    createDustBuffer();
  }

  DustRenderSystem::~DustRenderSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
  }

  void DustRenderSystem::createPipelineLayout()
  {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(DustPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 0;
    pipelineLayoutInfo.pSetLayouts            = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create dust pipeline layout!");
    }
  }

  void DustRenderSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    // Input Assembly - Points
    pipelineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    // Rasterization
    pipelineConfig.rasterizationInfo.cullMode    = VK_CULL_MODE_NONE;
    pipelineConfig.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;

    // Depth Stencil - Read Only, No Write
    pipelineConfig.depthStencilInfo.depthTestEnable  = VK_TRUE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthCompareOp   = VK_COMPARE_OP_LESS;

    // Color Blend - Additive
    pipelineConfig.colorBlendAttachment.blendEnable         = VK_TRUE;
    pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE; // Additive
    pipelineConfig.colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipelineConfig.colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    // Vertex Input
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.attributeDescriptions.clear();

    // Binding 0: Position (vec3)
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(glm::vec3);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    pipelineConfig.bindingDescriptions.push_back(bindingDescription);

    VkVertexInputAttributeDescription attributeDescription{};
    attributeDescription.binding  = 0;
    attributeDescription.location = 0;
    attributeDescription.format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescription.offset   = 0;
    pipelineConfig.attributeDescriptions.push_back(attributeDescription);

#ifdef SHADER_PATH
    std::string vertPath = std::string(SHADER_PATH) + "/dust.vert.spv";
    std::string fragPath = std::string(SHADER_PATH) + "/dust.frag.spv";
#else
    std::string vertPath = "assets/shaders/compiled/dust.vert.spv";
    std::string fragPath = "assets/shaders/compiled/dust.frag.spv";
#endif

    pipeline = std::make_unique<Pipeline>(device, vertPath, fragPath, pipelineConfig);
  }

  void DustRenderSystem::createDustBuffer()
  {
    // Generate random points in a unit cube [0, 1]
    // We will scale them in the shader by boxSize

    vertexCount = 10000; // Max particles
    std::vector<glm::vec3> vertices(vertexCount);

    std::default_random_engine            generator;
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

    for (auto& v : vertices)
    {
      v.x = distribution(generator) * 20.0f; // Pre-scale to max expected box size to avoid precision issues?
                                             // Actually shader does mod(pos, boxSize), so initial pos should be large enough range
                                             // or just [0, boxSize]. Let's use [0, 100]
      v.y = distribution(generator) * 20.0f;
      v.z = distribution(generator) * 20.0f;
    }

    VkDeviceSize bufferSize = sizeof(glm::vec3) * vertexCount;

    Buffer stagingBuffer{device, bufferSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    stagingBuffer.map();
    stagingBuffer.writeToBuffer(vertices.data());

    vertexBuffer = std::make_unique<Buffer>(device,
                                            bufferSize,
                                            1,
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), 1, &copyRegion);

    device.endSingleTimeCommands(commandBuffer);
  }

  void DustRenderSystem::render(FrameInfo&          frameInfo,
                                const DustSettings& settings,
                                const glm::vec4&    sunDir,
                                const glm::vec3&    sunColor,
                                const glm::vec3&    ambientColor)
  {
    if (!settings.enabled) return;

    pipeline->bind(frameInfo.commandBuffer);

    DustPushConstants push{};
    push.viewProjection = frameInfo.camera.getProjection() * frameInfo.camera.getView();
    push.cameraPosition = glm::vec4(frameInfo.camera.getPosition(), settings.boxSize);
    push.params         = glm::vec4(frameInfo.frameTime * frameInfo.frameIndex * 0.1f, // Use total time ideally, but frameIndex works for drift
                            settings.particleSize,
                            settings.alpha,
                            settings.heightFalloff);
    push.sunDirection   = sunDir;
    push.sunColor       = glm::vec4(sunColor, 1.0f);
    push.ambientColor   = glm::vec4(ambientColor, 1.0f);

    // Better time:
    static float totalTime = 0.0f;
    totalTime += frameInfo.frameTime;
    push.params.x = totalTime;

    vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DustPushConstants), &push);

    VkBuffer     buffers[] = {vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(frameInfo.commandBuffer, 0, 1, buffers, offsets);

    vkCmdDraw(frameInfo.commandBuffer, static_cast<uint32_t>(settings.particleCount), 1, 0, 0);
  }

} // namespace engine
