#pragma once
#include <cassert>
#include <string>
#include <vector>

#include "Device.hpp"

namespace engine {

  struct PipelineConfigInfo
  {
    explicit PipelineConfigInfo() = default;
    // delete copy operations
    PipelineConfigInfo(const PipelineConfigInfo&)            = delete;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

    VkPipelineViewportStateCreateInfo      viewportInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo   multisampleInfo;
    VkPipelineColorBlendAttachmentState    colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo    colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo  depthStencilInfo;
    std::vector<VkDynamicState>            dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo       dynamicStateInfo;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass     renderPass     = VK_NULL_HANDLE;
    uint32_t         subpass        = 0;
  };

  class Pipeline
  {
  public:
    Pipeline(Device& device, const std::string& vertFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo);

    ~Pipeline()
    {
      vkDestroyShaderModule(device.device(), vertShaderModule, nullptr);
      vkDestroyShaderModule(device.device(), fragShaderModule, nullptr);
      vkDestroyPipeline(device.device(), graphicsPipeline, nullptr);
    };

    // not copyable
    Pipeline& operator=(const Pipeline&) = delete;

    // prevent copies
    Pipeline(const Pipeline&) = delete;

    // static function to get default config
    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);

    // function to bind pipeline to command buffer
    void bind(VkCommandBuffer commandBuffer);

  private:
    static std::vector<char> readFile(const std::string& filePath);
    void                     createGraphicsPipeline(const std::string& vertFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo);

    void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

    // could potentially be memory unsafe, need to ensure device lives
    // longer than pipeline aggregation relationship

    Device& device;

    // handle to the graphics pipeline module
    // typedef to pointer to opaque struct
    // always check what the actual type is
    VkPipeline graphicsPipeline;

    // handle to the vertex shader module
    // typedef to pointer to opaque struct
    // always check what the actual type is
    VkShaderModule vertShaderModule;

    // handle to the fragment shader module
    // typedef to pointer to opaque struct
    // always check what the actual type is
    VkShaderModule fragShaderModule;
  };
} // namespace engine
