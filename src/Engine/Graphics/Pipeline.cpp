#include "Engine/Graphics/Pipeline.hpp"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/ShaderMonitor.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    namespace {
        void normalizePipelineConfigPointers(PipelineConfigInfo& configInfo, std::vector<VkPipelineColorBlendAttachmentState>& ownedColorBlendAttachments) {
            configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();

            uint32_t const attachmentCount = configInfo.colorBlendInfo.attachmentCount;
            if (attachmentCount == 0u) {
                ownedColorBlendAttachments.clear();
                configInfo.colorBlendInfo.pAttachments = nullptr;
                return;
            }

            if (attachmentCount == 1u) {
                if (configInfo.colorBlendInfo.pAttachments != nullptr) {
                    configInfo.colorBlendAttachment = configInfo.colorBlendInfo.pAttachments[0];
                }
                ownedColorBlendAttachments.clear();
                configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
                return;
            }

            if (configInfo.colorBlendInfo.pAttachments != nullptr) {
                ownedColorBlendAttachments.assign(configInfo.colorBlendInfo.pAttachments, configInfo.colorBlendInfo.pAttachments + attachmentCount);
                configInfo.colorBlendInfo.pAttachments = ownedColorBlendAttachments.data();
            } else if (!ownedColorBlendAttachments.empty()) {
                configInfo.colorBlendInfo.pAttachments = ownedColorBlendAttachments.data();
            }
        }
    }  // namespace

    Pipeline::Pipeline(Device& device, const std::string& vertFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo) : device(device)

    {
        isMeshPipeline_ = false;
        vertFilePath_   = vertFilePath;
        fragFilePath_   = fragFilePath;
        configInfo_     = configInfo;
        normalizePipelineConfigPointers(configInfo_, colorBlendAttachments_);

        shaderMonitor_ = std::make_unique<ShaderMonitor>();
        shaderMonitor_->addShader(vertFilePath);
        shaderMonitor_->addShader(fragFilePath);

        createGraphicsPipeline(vertFilePath_, fragFilePath_, configInfo_);
        engine::Logger::info(engine::LogChannel::Render, "[Pipeline] vert: ", std::filesystem::path(vertFilePath).filename().string(), " frag: ", std::filesystem::path(fragFilePath).filename().string());
    }

    Pipeline::Pipeline(Device& device, const std::string& taskFilePath, const std::string& meshFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo) : device(device) {
        isMeshPipeline_ = true;
        taskFilePath_   = taskFilePath;
        meshFilePath_   = meshFilePath;
        fragFilePath_   = fragFilePath;
        configInfo_     = configInfo;
        normalizePipelineConfigPointers(configInfo_, colorBlendAttachments_);

        shaderMonitor_ = std::make_unique<ShaderMonitor>();
        shaderMonitor_->addShader(taskFilePath);
        shaderMonitor_->addShader(meshFilePath);
        shaderMonitor_->addShader(fragFilePath);

        createMeshPipeline(taskFilePath_, meshFilePath_, fragFilePath_, configInfo_);
        engine::Logger::info(engine::LogChannel::Render, "[Pipeline] task: ", std::filesystem::path(taskFilePath).filename().string(), " mesh: ", std::filesystem::path(meshFilePath).filename().string(), " frag: ", std::filesystem::path(fragFilePath).filename().string());
    }

    void Pipeline::destroyPipelineResources() {
        if (vertShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device.device(), vertShaderModule, nullptr);
            vertShaderModule = VK_NULL_HANDLE;
        }
        if (fragShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device.device(), fragShaderModule, nullptr);
            fragShaderModule = VK_NULL_HANDLE;
        }
        if (taskShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device.device(), taskShaderModule, nullptr);
            taskShaderModule = VK_NULL_HANDLE;
        }
        if (meshShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device.device(), meshShaderModule, nullptr);
            meshShaderModule = VK_NULL_HANDLE;
        }
        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device.device(), graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }
    }

    bool Pipeline::hasAnyShaderChanged(std::string* changedShaderPath) const {
        if (shaderMonitor_ == nullptr) {
            return false;
        }
        return shaderMonitor_->hasAnyShaderChanged(changedShaderPath);
    }

    bool Pipeline::rebuild(std::string* statusMessage) {
        destroyPipelineResources();
        normalizePipelineConfigPointers(configInfo_, colorBlendAttachments_);

        try {
            if (isMeshPipeline_) {
                createMeshPipeline(taskFilePath_, meshFilePath_, fragFilePath_, configInfo_);
            } else {
                createGraphicsPipeline(vertFilePath_, fragFilePath_, configInfo_);
            }

            if (shaderMonitor_ != nullptr) {
                shaderMonitor_->refreshTimestamps();
            }

            if (statusMessage != nullptr) {
                *statusMessage = "Pipeline rebuilt";
            }
            return true;
        } catch (const std::exception& e) {
            if (statusMessage != nullptr) {
                *statusMessage = e.what();
            }
            Logger::error(LogChannel::Render, "Pipeline rebuild failed: ", e.what());
            return false;
        }
    }

    bool Pipeline::reloadIfChanged(std::string* statusMessage) {
        std::string changed;
        if (!hasAnyShaderChanged(&changed)) {
            if (statusMessage != nullptr) {
                *statusMessage = "No shader changes";
            }
            return false;
        }

        Logger::info(LogChannel::Render, "Detected shader change, rebuilding pipeline for ", std::filesystem::path(changed).filename().string());
        return rebuild(statusMessage);
    }

    bool Pipeline::forceReload(std::string* statusMessage) {
        return rebuild(statusMessage);
    }

    std::vector<char> Pipeline::readFile(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw ReadFileException(std::string("failed to open file: " + filePath).c_str());
        }

        auto fileSize = static_cast<size_t>(file.tellg());

        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
        file.close();

        return buffer;
    }

    void Pipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo) {
        configInfo.viewportInfo = {
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports    = nullptr,
            .scissorCount  = 1,
            .pScissors     = nullptr,
        };

        configInfo.inputAssemblyInfo = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        configInfo.rasterizationInfo = {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_NONE,
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable         = VK_FALSE,
            .lineWidth               = 1.0f,
        };

        configInfo.multisampleInfo = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 1.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
        };

        configInfo.colorBlendAttachment = {
            .blendEnable         = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        configInfo.colorBlendInfo = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments    = &configInfo.colorBlendAttachment,
            .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
        };

        configInfo.depthStencilInfo = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable       = VK_TRUE,
            .depthWriteEnable      = VK_TRUE,
            .depthCompareOp        = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = VK_FALSE,
            .minDepthBounds        = 0.0f,
            .maxDepthBounds        = 1.0f,
        };

        configInfo.dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        configInfo.dynamicStateInfo = {
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size()),
            .pDynamicStates    = configInfo.dynamicStateEnables.data(),
        };

        configInfo.pipelineLayout = VK_NULL_HANDLE;
        configInfo.renderPass     = VK_NULL_HANDLE;
        configInfo.subpass        = 0;

        configInfo.bindingDescriptions   = Model::Vertex::getBindingDescriptions();
        configInfo.attributeDescriptions = Model::Vertex::getAttributeDescriptions();
    }

    void Pipeline::defaultMeshPipelineConfigInfo(PipelineConfigInfo& configInfo) {
        defaultPipelineConfigInfo(configInfo);

        configInfo.bindingDescriptions.clear();
        configInfo.attributeDescriptions.clear();
        configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    void Pipeline::bind(VkCommandBuffer commandBuffer) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    }

    void Pipeline::createMeshPipeline(const std::string& taskFilePath, const std::string& meshFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo) {
        assert(configInfo.pipelineLayout != VK_NULL_HANDLE &&
               "Cannot create graphics pipeline: no pipeline layout provided in "
               "configInfo");
        assert(configInfo.renderPass != VK_NULL_HANDLE && "Cannot create graphics pipeline: no render pass provided in configInfo");

        auto taskShaderCode = readFile(taskFilePath);
        auto meshShaderCode = readFile(meshFilePath);
        auto fragShaderCode = readFile(fragFilePath);

        createShaderModule(taskShaderCode, &taskShaderModule);
        createShaderModule(meshShaderCode, &meshShaderModule);
        createShaderModule(fragShaderCode, &fragShaderModule);

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext  = nullptr,
                .flags  = 0,
                .stage  = VK_SHADER_STAGE_TASK_BIT_EXT,
                .module = taskShaderModule,
                .pName  = "main",
            },
            {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext  = nullptr,
                .flags  = 0,
                .stage  = VK_SHADER_STAGE_MESH_BIT_EXT,
                .module = meshShaderModule,
                .pName  = "main",
            },
            {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext  = nullptr,
                .flags  = 0,
                .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragShaderModule,
                .pName  = "main",
            },
        };

        VkPipelineVertexInputStateCreateInfo const vertexInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };

        VkGraphicsPipelineCreateInfo const pipelineInfo{
            .sType             = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount        = 3,
            .pStages           = shaderStages,
            .pVertexInputState = &vertexInputInfo,

            .pInputAssemblyState = &configInfo.inputAssemblyInfo,
            .pViewportState      = &configInfo.viewportInfo,
            .pRasterizationState = &configInfo.rasterizationInfo,
            .pMultisampleState   = &configInfo.multisampleInfo,
            .pDepthStencilState  = &configInfo.depthStencilInfo,
            .pColorBlendState    = &configInfo.colorBlendInfo,
            .pDynamicState       = &configInfo.dynamicStateInfo,
            .layout              = configInfo.pipelineLayout,
            .renderPass          = configInfo.renderPass,
            .subpass             = configInfo.subpass,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        };

        if (vkCreateGraphicsPipelines(device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw GraphicsPipelineCreationException("failed to create mesh pipeline!");
        }
    }

    void Pipeline::createGraphicsPipeline(const std::string& vertFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo) {
        assert(configInfo.pipelineLayout != VK_NULL_HANDLE &&
               "Cannot create graphics pipeline: no pipeline layout provided "
               "in configInfo");

        assert(configInfo.renderPass != VK_NULL_HANDLE &&
               "Cannot create graphics pipeline: no render pass provided in "
               "configInfo");

        auto vertShaderCode = readFile(vertFilePath);
        auto fragShaderCode = readFile(fragFilePath);

        createShaderModule(vertShaderCode, &vertShaderModule);
        createShaderModule(fragShaderCode, &fragShaderModule);

        VkPipelineShaderStageCreateInfo shaderStages[2] = {{
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

        auto& bindingDescriptions   = configInfo.bindingDescriptions;
        auto& attributeDescriptions = configInfo.attributeDescriptions;

        VkPipelineVertexInputStateCreateInfo const vertexInputInfo{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = static_cast<uint32_t>(bindingDescriptions.size()),
            .pVertexBindingDescriptions      = bindingDescriptions.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions    = attributeDescriptions.data(),
        };

        if (VkGraphicsPipelineCreateInfo const pipelineInfo{
                .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount          = 2,
                .pStages             = shaderStages,
                .pVertexInputState   = &vertexInputInfo,
                .pInputAssemblyState = &configInfo.inputAssemblyInfo,
                .pViewportState      = &configInfo.viewportInfo,
                .pRasterizationState = &configInfo.rasterizationInfo,
                .pMultisampleState   = &configInfo.multisampleInfo,
                .pDepthStencilState  = &configInfo.depthStencilInfo,
                .pColorBlendState    = &configInfo.colorBlendInfo,
                .pDynamicState       = &configInfo.dynamicStateInfo,
                .layout              = configInfo.pipelineLayout,
                .renderPass          = configInfo.renderPass,
                .subpass             = configInfo.subpass,
                .basePipelineHandle  = VK_NULL_HANDLE,
                .basePipelineIndex   = -1,
            };
            vkCreateGraphicsPipelines(device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw GraphicsPipelineCreationException("failed to create graphics pipeline!");
        }
    }

    void Pipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {
        VkShaderModuleCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        createInfo.codeSize = code.size();

        std::vector<uint32_t> codeAligned((code.size() + 3) / 4);
        std::memcpy(codeAligned.data(), code.data(), code.size());
        createInfo.pCode = codeAligned.data();

        if (vkCreateShaderModule(device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
            throw ShaderModuleCreationException("failed to create shader module!");
        }
    }

}  // namespace engine