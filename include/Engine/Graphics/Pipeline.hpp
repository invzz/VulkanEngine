#pragma once
#include <cassert>
#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"

namespace engine {

  struct PipelineConfigInfo
  {
    explicit PipelineConfigInfo() = default;

    std::vector<VkVertexInputBindingDescription>   bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

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
    Pipeline(Device&                   device,
             const std::string&        taskFilePath,
             const std::string&        meshFilePath,
             const std::string&        fragFilePath,
             const PipelineConfigInfo& configInfo);

    ~Pipeline()
    {
      if (vertShaderModule) vkDestroyShaderModule(device.device(), vertShaderModule, nullptr);
      if (fragShaderModule) vkDestroyShaderModule(device.device(), fragShaderModule, nullptr);
      if (taskShaderModule) vkDestroyShaderModule(device.device(), taskShaderModule, nullptr);
      if (meshShaderModule) vkDestroyShaderModule(device.device(), meshShaderModule, nullptr);
      vkDestroyPipeline(device.device(), graphicsPipeline, nullptr);
    };

    // not copyable
    Pipeline& operator=(const Pipeline&) = delete;

    // prevent copies
    Pipeline(const Pipeline&) = delete;

    // static function to get default config
    static void              defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
    static void              defaultMeshPipelineConfigInfo(PipelineConfigInfo& configInfo);
    static std::vector<char> readFile(const std::string& filePath);

    // function to bind pipeline to command buffer
    void bind(VkCommandBuffer commandBuffer);

  private:
    void createGraphicsPipeline(const std::string& vertFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo);
    void
    createMeshPipeline(const std::string& taskFilePath, const std::string& meshFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo);

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
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;

    // handle to the fragment shader module
    // typedef to pointer to opaque struct
    // always check what the actual type is
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;

    VkShaderModule taskShaderModule = VK_NULL_HANDLE;
    VkShaderModule meshShaderModule = VK_NULL_HANDLE;
  };
} // namespace engine
