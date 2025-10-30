#include "Pipeline.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace engine {

    Pipeline::Pipeline(Device&                   device,
                       const std::string&        vertFilePath,
                       const std::string&        fragFilePath,
                       const PipelineConfigInfo& configInfo)
        : device(device)

    {
        createGraphicsPipeline(vertFilePath, fragFilePath, configInfo);
    }

    std::vector<char> Pipeline::readFile(const std::string& filePath)
    {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file: " + filePath);
        }

        size_t fileSize = static_cast<size_t>(file.tellg());

        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    PipelineConfigInfo Pipeline::defaultPipelineConfigInfo(uint32_t width, uint32_t height)
    {
        float widthf  = static_cast<float>(width);
        float heightf = static_cast<float>(height);

        std::cout << "Creating default pipeline config with width: " << widthf
                  << " height: " << heightf << std::endl;

        // first stage of pipeline creation is input assembly
        // sType must be specified for all structs
        // topology defines how to interpret vertices
        // primitiveRestartEnable allows breaking up of strips
        PipelineConfigInfo configInfo{
                .viewport =
                        {
                                // top left x,y
                                .x = 0.0f,
                                .y = 0.0f,
                                // width and height of viewport
                                .width  = widthf,
                                .height = heightf,
                                // depth range 0.0 to 1.0
                                .minDepth = 0.0f,
                                .maxDepth = 1.0f,
                        },
                .scissor =
                        {
                                // offset
                                .offset = {0, 0},
                                // extent
                                .extent = {width, height},
                        },

                .inputAssemblyInfo =
                        {
                                .sType =
                                        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                .primitiveRestartEnable = VK_FALSE,
                        },

                .rasterizationInfo =
                        {
                                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                .depthClampEnable        = VK_FALSE,
                                .rasterizerDiscardEnable = VK_FALSE,
                                .polygonMode             = VK_POLYGON_MODE_FILL,
                                .cullMode                = VK_CULL_MODE_NONE,
                                .frontFace               = VK_FRONT_FACE_CLOCKWISE,
                                .depthBiasEnable         = VK_FALSE,
                                .lineWidth               = 1.0f,
                        },
                .multisampleInfo =
                        {
                                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
                                .sampleShadingEnable   = VK_FALSE,
                                .minSampleShading      = 1.0f,
                                .pSampleMask           = nullptr,
                                .alphaToCoverageEnable = VK_FALSE,
                                .alphaToOneEnable      = VK_FALSE,
                        },
                .colorBlendAttachment =
                        {
                                .blendEnable         = VK_FALSE,
                                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                                .colorBlendOp        = VK_BLEND_OP_ADD,
                                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                                .alphaBlendOp        = VK_BLEND_OP_ADD,
                                .colorWriteMask =
                                        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                        },
                .colorBlendInfo =
                        {
                                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                .logicOpEnable   = VK_FALSE,
                                .logicOp         = VK_LOGIC_OP_COPY,
                                .attachmentCount = 1,
                                .pAttachments    = &configInfo.colorBlendAttachment,
                                .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
                        },
                .depthStencilInfo =
                        {
                                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                .depthTestEnable       = VK_TRUE,
                                .depthWriteEnable      = VK_TRUE,
                                .depthCompareOp        = VK_COMPARE_OP_LESS,
                                .depthBoundsTestEnable = VK_FALSE,
                                .stencilTestEnable     = VK_FALSE,
                                .minDepthBounds        = 0.0f,
                                .maxDepthBounds        = 1.0f,
                        },

        };
        return configInfo;
    }

    void Pipeline::bind(VkCommandBuffer commandBuffer)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    }

    void Pipeline::createGraphicsPipeline(const std::string&        vertFilePath,
                                          const std::string&        fragFilePath,
                                          const PipelineConfigInfo& configInfo)
    {
        assert(configInfo.pipelineLayout != VK_NULL_HANDLE &&
               "Cannot create graphics pipeline: no pipeline layout provided in configInfo");

        assert(configInfo.renderPass != VK_NULL_HANDLE &&
               "Cannot create graphics pipeline: no render pass provided in configInfo");

        auto vertShaderCode = readFile(vertFilePath);
        auto fragShaderCode = readFile(fragFilePath);

        createShaderModule(vertShaderCode, &vertShaderModule);
        createShaderModule(fragShaderCode, &fragShaderModule);

        VkPipelineShaderStageCreateInfo shaderStages[2] = {
                {
                        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext               = nullptr,
                        .flags               = 0,
                        .stage               = VK_SHADER_STAGE_VERTEX_BIT,
                        .module              = vertShaderModule,
                        .pName               = "main",
                        .pSpecializationInfo = nullptr,
                },
                {
                        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext               = nullptr,
                        .flags               = 0,
                        .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .module              = fragShaderModule,
                        .pName               = "main",
                        .pSpecializationInfo = nullptr,
                }};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount   = 0,
                .pVertexBindingDescriptions      = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions    = nullptr,
        };

        VkPipelineViewportStateCreateInfo viewportInfo{
                .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .pViewports    = &configInfo.viewport,
                .pNext         = nullptr,
                .scissorCount  = 1,
                .pScissors     = &configInfo.scissor,
                .flags         = 0,
        };

        VkGraphicsPipelineCreateInfo pipelineInfo{
                .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount          = 2,
                .pStages             = shaderStages,
                .pVertexInputState   = &vertexInputInfo,
                .pInputAssemblyState = &configInfo.inputAssemblyInfo,
                .pViewportState      = &viewportInfo,
                .pRasterizationState = &configInfo.rasterizationInfo,
                .pMultisampleState   = &configInfo.multisampleInfo,
                .pDepthStencilState  = &configInfo.depthStencilInfo,
                .pColorBlendState    = &configInfo.colorBlendInfo,
                .pDynamicState       = nullptr,
                .layout              = configInfo.pipelineLayout,
                .renderPass          = configInfo.renderPass,
                .subpass             = configInfo.subpass,
                .basePipelineHandle  = VK_NULL_HANDLE,
                .basePipelineIndex   = -1,
        };

        if (vkCreateGraphicsPipelines(device.device(),
                                      VK_NULL_HANDLE,
                                      1,
                                      &pipelineInfo,
                                      nullptr,
                                      &graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        std::cout << "Vertex Shader Size: " << vertShaderCode.size() << " bytes\n";
        std::cout << "Fragment Shader Size: " << fragShaderCode.size() << " bytes\n";
    }

    void Pipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule)
    {
        VkShaderModuleCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        createInfo.codeSize = code.size();
        // uint32_t and char are different sizes, need to cast
        // ensure proper alignment with reinterpret_cast
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        if (vkCreateShaderModule(device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }
    }

} // namespace engine
