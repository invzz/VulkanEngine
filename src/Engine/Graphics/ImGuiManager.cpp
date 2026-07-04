#include "Engine/Graphics/ImGuiManager.hpp"

#include <imgui.h>

#include <exception>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <sstream>
#include <stdexcept>

#include "Engine/Core/Logger.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"

#include "IconsFontAwesome6.h"
#include "vulkan/vulkan_core.h"
namespace engine {
    ImGuiManager::ImGuiManager(Window& window, Device& device, VkRenderPass renderPass, uint32_t imageCount) : window_(window), device_(device), renderPass_(renderPass) {
        initImGui();
        setupVulkanBackend(imageCount);
    }
    ImGuiManager::~ImGuiManager() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_.device(), imguiDescriptorPool_, nullptr);
        }
    }
    void ImGuiManager::initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void) io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        const float fontSize = 16.0f;
        io.Fonts->AddFontDefault();
        ImFontConfig iconConfig;
        iconConfig.MergeMode              = true;
        iconConfig.PixelSnapH             = true;
        static const ImWchar iconRanges[] = {
            ICON_MIN_FA, ICON_MAX_FA, 0};
        io.Fonts->AddFontFromFileTTF(
            FONT_PATH "fa-solid-900.ttf", fontSize, &iconConfig, iconRanges);
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window_.getGLFWwindow(), true);
    }
    void ImGuiManager::setupVulkanBackend(uint32_t imageCount) {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 100},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100},
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets                    = 1000;
        pool_info.poolSizeCount              = (uint32_t) IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes                 = pool_sizes;
        VkResult const poolRes               = vkCreateDescriptorPool(device_.device(), &pool_info, nullptr, &imguiDescriptorPool_);
        if (poolRes != VK_SUCCESS) {
            engine::Logger::error(engine::LogChannel::Render, "vkCreateDescriptorPool failed with code: ", poolRes);
            throw std::runtime_error("Failed to create ImGui descriptor pool!");
        }
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance                  = device_.getInstance();
        init_info.PhysicalDevice            = device_.getPhysicalDevice();
        init_info.Device                    = device_.device();
        init_info.QueueFamily               = device_.findPhysicalQueueFamilies().graphicsFamily;
        init_info.Queue                     = device_.graphicsQueue();
        init_info.PipelineCache             = VK_NULL_HANDLE;
        init_info.DescriptorPool            = imguiDescriptorPool_;
        init_info.RenderPass                = renderPass_;
        init_info.Subpass                   = 0;
        init_info.MinImageCount             = imageCount;
        init_info.ImageCount                = imageCount;
        init_info.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator                 = nullptr;
        init_info.CheckVkResultFn           = [](VkResult err) {
            if (err != VK_SUCCESS) {
            }
        };
        engine::Logger::info(engine::LogChannel::Render, "ImGui init: Instance=", init_info.Instance, " PhysicalDevice=", init_info.PhysicalDevice, " Device=", init_info.Device, " DescriptorPool=", init_info.DescriptorPool, " RenderPass=", init_info.RenderPass, " QueueFamily=", init_info.QueueFamily, " GraphicsQueue=", device_.graphicsQueue(), " ImageCount=", init_info.ImageCount);
        if (init_info.Device == VK_NULL_HANDLE || init_info.Instance == VK_NULL_HANDLE || init_info.PhysicalDevice == VK_NULL_HANDLE) {
            engine::Logger::error(engine::LogChannel::Render, "Invalid Vulkan handles detected before ImGui init. Aborting ImGui init.");
            return;
        }
        if (init_info.Queue == VK_NULL_HANDLE) {
            engine::Logger::error(engine::LogChannel::Render, "ImGui init: graphics queue handle is null. Aborting ImGui init.");
            return;
        }
        VkResult const r1 = vkDeviceWaitIdle(init_info.Device);
        engine::Logger::info(engine::LogChannel::Render, "vkDeviceWaitIdle returned: ", r1);
        VkResult const r2 = vkQueueWaitIdle(init_info.Queue);
        engine::Logger::info(engine::LogChannel::Render, "vkQueueWaitIdle returned: ", r2);
        VkSampler           testSampler = VK_NULL_HANDLE;
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = VK_FILTER_LINEAR;
        samplerInfo.minFilter               = VK_FILTER_LINEAR;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkResult const sres                 = vkCreateSampler(device_.device(), &samplerInfo, nullptr, &testSampler);
        engine::Logger::info(engine::LogChannel::Render, "vkCreateSampler returned: ", sres, " sampler=", testSampler);
        if (sres == VK_SUCCESS && testSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device_.device(), testSampler, nullptr);
        }
        VkDescriptorSetLayout        testLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayoutBinding binding{};
        binding.binding            = 0;
        binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount    = 1;
        binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = nullptr;
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &binding;
        VkResult const lres     = vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &testLayout);
        engine::Logger::info(engine::LogChannel::Render, "vkCreateDescriptorSetLayout returned: ", lres, " layout=", testLayout);
        if (lres == VK_SUCCESS && testLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_.device(), testLayout, nullptr);
        }
        VkPipelineLayout           testPipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 0;
        pipelineLayoutInfo.pSetLayouts            = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges    = nullptr;
        VkResult const pres                       = vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &testPipelineLayout);
        engine::Logger::info(engine::LogChannel::Render, "vkCreatePipelineLayout returned: ", pres, " pipelineLayout=", testPipelineLayout);
        if (pres == VK_SUCCESS && testPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_.device(), testPipelineLayout, nullptr);
        }
        init_info.ApiVersion = VK_API_VERSION_1_3;
        struct ImGuiLoaderUser {
            VkInstance instance;
            VkDevice   device;
        } loaderUser{device_.getInstance(), device_.device()};
        bool const loaded = ImGui_ImplVulkan_LoadFunctions(
            VK_API_VERSION_1_3,
            [](const char* name, void* user_data) -> PFN_vkVoidFunction {
                auto* u = reinterpret_cast<ImGuiLoaderUser*>(user_data);
                auto  f = vkGetInstanceProcAddr(u->instance, name);
                if (f)
                    return f;
                if (u->device != VK_NULL_HANDLE) {
                    return vkGetDeviceProcAddr(u->device, name);
                }
                return nullptr;
            },
            (void*) &loaderUser);
        engine::Logger::info(engine::LogChannel::Render, "ImGui_ImplVulkan_LoadFunctions returned: ", loaded);
        try {
            ImGui_ImplVulkan_Init(&init_info);
            engine::Logger::info(engine::LogChannel::Render, "ImGui Vulkan backend initialized successfully.");
        } catch (const std::exception& e) {
            engine::Logger::error(engine::LogChannel::Render, "Exception during ImGui_ImplVulkan_Init: ", e.what());
            return;
        }
    }
    void ImGuiManager::newFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
    void ImGuiManager::render(VkCommandBuffer commandBuffer) {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }
    void ImGuiManager::updateAfterResize() {
    }
}  // namespace engine
