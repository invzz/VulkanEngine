#include "Engine/Graphics/DebugMessenger.hpp"

#include <iostream>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine {

  DebugMessenger::~DebugMessenger()
  {
    reset();
  }

  void DebugMessenger::create(VkInstance instance)
  {
    if (messenger_ != VK_NULL_HANDLE)
    {
      return; // already created
    }

    instance_ = instance;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    Device::populateDebugMessengerCreateInfo(createInfo);

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr)
    {
      throw engine::RuntimeException("vkCreateDebugUtilsMessengerEXT not available");
    }

    if (func(instance_, &createInfo, nullptr, &messenger_) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create debug messenger");
    }
  }

  void DebugMessenger::reset() noexcept
  {
    if (messenger_ == VK_NULL_HANDLE || instance_ == VK_NULL_HANDLE)
    {
      messenger_ = VK_NULL_HANDLE;
      return;
    }

    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
      func(instance_, messenger_, nullptr);
    }
    messenger_ = VK_NULL_HANDLE;
  }

} // namespace engine
