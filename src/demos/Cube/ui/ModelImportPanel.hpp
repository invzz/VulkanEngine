#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "UIPanel.hpp"

namespace engine {

  class Texture;

  struct ModelEntry
  {
    std::string              name;
    std::string              relativePath;
    std::string              screenshotPath;
    std::shared_ptr<Texture> screenshotTexture;
    VkDescriptorSet          descriptorSet = VK_NULL_HANDLE;
  };

  /**
   * @brief Panel for importing glTF models
   */
  class ModelImportPanel : public UIPanel
  {
  public:
    ModelImportPanel(Device& device, Scene& scene, AnimationSystem& animationSystem, ResourceManager& resourceManager);

    void render(FrameInfo& frameInfo) override;
    bool isSeparateWindow() const override { return true; }

  private:
    void loadModelIndex();
    void loadModel(const std::string& path, const std::string& name = "ImportedModel");

    Device&                 device_;
    Scene&                  scene_;
    AnimationSystem&        animationSystem_;
    ResourceManager&        resourceManager_;
    char                    modelPath_[256] = "glTF/DamagedHelmet/glTF/DamagedHelmet.gltf";
    std::vector<ModelEntry> availableModels_;
  };

} // namespace engine
