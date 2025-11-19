#include "3dEngine/ImGuiManager.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <iostream>
#include <stdexcept>

namespace engine {

  ImGuiManager::ImGuiManager(Window& window, Device& device, VkRenderPass renderPass, uint32_t imageCount)
      : window_(window), device_(device), renderPass_(renderPass)
  {
    initImGui();
    setupVulkanBackend(imageCount);
  }

  ImGuiManager::~ImGuiManager()
  {
    // Cleanup ImGui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup Vulkan resources
    if (imguiDescriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device_.device(), imguiDescriptorPool_, nullptr);
    }
  }

  void ImGuiManager::initImGui()
  {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Initialize GLFW backend
    ImGui_ImplGlfw_InitForVulkan(window_.getGLFWwindow(), true);
  }

  void ImGuiManager::setupVulkanBackend(uint32_t imageCount)
  {
    // Create descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets                    = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount              = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes                 = pool_sizes;

    if (vkCreateDescriptorPool(device_.device(), &pool_info, nullptr, &imguiDescriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    // Initialize ImGui Vulkan backend
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
    init_info.CheckVkResultFn           = nullptr;

    ImGui_ImplVulkan_Init(&init_info);
  }

  void ImGuiManager::newFrame()
  {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
  }

  void ImGuiManager::render(VkCommandBuffer commandBuffer)
  {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
  }

  void ImGuiManager::updateAfterResize()
  {
    // ImGui handles this automatically through GLFW callbacks
  }

} // namespace engine
