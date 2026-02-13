#ifndef EDITOR_MODELIMPORTPANEL_HPP
#define EDITOR_MODELIMPORTPANEL_HPP

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

#include "Editor/ui/UIPanel.hpp"
#include "Engine/Graphics/Device.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "ModelLib/Resources/Texture.hpp"

namespace engine {

class EngineState;

struct ModelEntry {
  std::string name;
  std::string relativePath;
  std::string screenshotPath;
  std::shared_ptr<Texture> screenshotTexture;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

/**
 * @brief Panel for importing glTF models
 */
class ModelImportPanel : public UIPanel {
 public:
  ModelImportPanel(Device& device, EngineState* engineState);

  void render(FrameInfo& frameInfo) override;
  [[nodiscard]] bool isSeparateWindow() const override {
    return true;
  }

 private:
  void loadModelIndex();
  void loadModel(const std::string& path, const std::string& name = "ImportedModel");

  Device& device_;
  EngineState* engineState_ = nullptr;
  char modelPath_[256] = "glTF/DamagedHelmet/glTF/DamagedHelmet.gltf";
  std::vector<ModelEntry> availableModels_;

  // Meshlet generation settings
  int meshletMaxVertices_ = 64;     // 1-64
  int meshletMaxTriangles_ = 124;   // 1-124
  float meshletConeWeight_ = 0.0f;  // 0.0 = locality, 1.0 = backface culling
  float meshletMaxRadius_ = 0.0f;   // 0.0 = disabled, >0 = max bounding sphere radius in meters
};

}  // namespace engine

#endif  // EDITOR_MODELIMPORTPANEL_HPP
