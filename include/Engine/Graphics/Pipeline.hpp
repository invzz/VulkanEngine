#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_PIPELINE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_PIPELINE_HPP
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/ShaderMonitor.hpp"
namespace engine {
    struct PipelineConfigInfo {
        explicit PipelineConfigInfo() = default;
        std::vector<VkVertexInputBindingDescription>   bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        VkPipelineViewportStateCreateInfo              viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo         inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo         rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo           multisampleInfo;
        VkPipelineColorBlendAttachmentState            colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo            colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo          depthStencilInfo;
        std::vector<VkDynamicState>                    dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo               dynamicStateInfo;
        VkPipelineLayout                               pipelineLayout = VK_NULL_HANDLE;
        VkRenderPass                                   renderPass     = VK_NULL_HANDLE;
        uint32_t                                       subpass        = 0;
    };
    class Pipeline {
       public:
        Pipeline(Device& device, const std::string& vertFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo);
        Pipeline(Device& device, const std::string& taskFilePath, const std::string& meshFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo);
        ~Pipeline() {
            if (vertShaderModule != nullptr)
                vkDestroyShaderModule(device.device(), vertShaderModule, nullptr);
            if (fragShaderModule != nullptr)
                vkDestroyShaderModule(device.device(), fragShaderModule, nullptr);
            if (taskShaderModule != nullptr)
                vkDestroyShaderModule(device.device(), taskShaderModule, nullptr);
            if (meshShaderModule != nullptr)
                vkDestroyShaderModule(device.device(), meshShaderModule, nullptr);
            vkDestroyPipeline(device.device(), graphicsPipeline, nullptr);
        };
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(const Pipeline&)            = delete;
        static void              defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
        static void              defaultMeshPipelineConfigInfo(PipelineConfigInfo& configInfo);
        static std::vector<char> readFile(const std::string& filePath);
        bool                     reloadIfChanged(std::string* statusMessage = nullptr);
        bool                     forceReload(std::string* statusMessage = nullptr);
        void                     bind(VkCommandBuffer commandBuffer);
        [[nodiscard]] VkPipeline getPipeline() const {
            return graphicsPipeline;
        }

       private:
        void                                             createGraphicsPipeline(const std::string& vertFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo);
        void                                             createMeshPipeline(const std::string& taskFilePath, const std::string& meshFilePath, const std::string& fragFilePath, const PipelineConfigInfo& configInfo);
        void                                             destroyPipelineResources();
        bool                                             hasAnyShaderChanged(std::string* changedShaderPath = nullptr) const;
        bool                                             rebuild(std::string* statusMessage);
        void                                             createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
        Device&                                          device;
        VkPipeline                                       graphicsPipeline = VK_NULL_HANDLE;
        VkShaderModule                                   vertShaderModule = VK_NULL_HANDLE;
        VkShaderModule                                   fragShaderModule = VK_NULL_HANDLE;
        VkShaderModule                                   taskShaderModule = VK_NULL_HANDLE;
        VkShaderModule                                   meshShaderModule = VK_NULL_HANDLE;
        bool                                             isMeshPipeline_  = false;
        PipelineConfigInfo                               configInfo_{};
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments_{};
        std::string                                      vertFilePath_;
        std::string                                      fragFilePath_;
        std::string                                      taskFilePath_;
        std::string                                      meshFilePath_;
        std::unique_ptr<ShaderMonitor>                   shaderMonitor_;
    };
}  // namespace engine
#endif
