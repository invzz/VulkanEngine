#include "ModelImportPanel.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  ModelImportPanel::ModelImportPanel(Device& device, Scene& scene, AnimationSystem& animationSystem, ResourceManager& resourceManager)
      : device_(device), scene_(scene), animationSystem_(animationSystem), resourceManager_(resourceManager)
  {
    loadModelIndex();
  }

  void ModelImportPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Import Model", ImGuiTreeNodeFlags_DefaultOpen))
    {
      ImGui::InputText("glTF Path", modelPath_, sizeof(modelPath_));
      ImGui::SameLine();
      if (ImGui::Button("Browse"))
      {
        // Set default path to models directory
        std::string defaultPath = std::string(MODEL_PATH) + "/glTF/";
        strncpy(modelPath_, defaultPath.c_str(), sizeof(modelPath_) - 1);
      }

      if (ImGui::Button("Load Model"))
      {
        // Create full path if it's relative
        std::string fullPath = modelPath_;
        if (fullPath[0] != '/')
        {
          fullPath = std::string(MODEL_PATH) + "/" + fullPath;
        }
        loadModel(fullPath);
      }
    }

    if (ImGui::CollapsingHeader("Available Models"))
    {
      float       windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
      ImGuiStyle& style           = ImGui::GetStyle();
      float       buttonSize      = 128.0f;
      float       spacing         = style.ItemSpacing.x;

      for (size_t i = 0; i < availableModels_.size(); i++)
      {
        const auto& entry = availableModels_[i];

        ImGui::PushID((int)i);

        // Image Button
        if (entry.descriptorSet != VK_NULL_HANDLE)
        {
          if (ImGui::ImageButton("##image", (ImTextureID)entry.descriptorSet, ImVec2(buttonSize, buttonSize)))
          {
            std::string fullPath = std::string(MODEL_PATH) + "/" + entry.relativePath;
            loadModel(fullPath);
          }
        }
        else
        {
          if (ImGui::Button(entry.name.c_str(), ImVec2(buttonSize, buttonSize)))
          {
            std::string fullPath = std::string(MODEL_PATH) + "/" + entry.relativePath;
            loadModel(fullPath);
          }
        }

        // Tooltip
        if (ImGui::IsItemHovered())
        {
          ImGui::BeginTooltip();
          ImGui::Text("%s", entry.name.c_str());
          ImGui::EndTooltip();
        }

        // Layout logic for grid
        float lastButtonX2 = ImGui::GetItemRectMax().x;
        float nextButtonX2 = lastButtonX2 + spacing + buttonSize;
        if (i + 1 < availableModels_.size() && nextButtonX2 < windowVisibleX2) ImGui::SameLine();

        ImGui::PopID();
      }
    }
  }

  void ModelImportPanel::loadModelIndex()
  {
    std::string   indexPath = std::string(MODEL_PATH) + "/glTF/model-index.json";
    std::ifstream f(indexPath);
    if (!f.is_open())
    {
      std::cerr << "Failed to open model index: " << indexPath << std::endl;
      return;
    }

    try
    {
      nlohmann::json j;
      f >> j;

      for (const auto& item : j)
      {
        ModelEntry entry;
        if (!item.contains("name")) continue;
        entry.name = item["name"];

        // Get screenshot path
        if (item.contains("screenshot"))
        {
          std::string screenshotRel = item["screenshot"];
          // Construct full path: MODEL_PATH + /glTF/ + name + / + screenshotRel
          // Note: screenshotRel is usually "screenshot/screenshot.png"
          // And the folder is assets/models/glTF/<name>/
          std::string fullScreenshotPath = std::string(MODEL_PATH) + "/glTF/" + entry.name + "/" + screenshotRel;
          entry.screenshotPath           = fullScreenshotPath;

          // Load texture
          // Use flipY=false for UI images
          try
          {
            entry.screenshotTexture = resourceManager_.loadTexture(fullScreenshotPath, true, false);
            if (entry.screenshotTexture)
            {
              entry.descriptorSet = ImGui_ImplVulkan_AddTexture(entry.screenshotTexture->getSampler(),
                                                                entry.screenshotTexture->getImageView(),
                                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
          }
          catch (const std::exception& e)
          {
            std::cerr << "Failed to load screenshot for " << entry.name << ": " << e.what() << std::endl;
          }
        }

        // Get glTF path
        if (item.contains("variants") && item["variants"].contains("glTF"))
        {
          std::string variantFile = item["variants"]["glTF"];
          // Path relative to MODEL_PATH
          // assets/models/glTF/<name>/glTF/<variantFile>
          // But wait, is it always in a glTF subfolder?
          // Based on 2CylinderEngine and DamagedHelmet, yes.
          // But let's be careful. The variantFile might be just the filename.
          // The structure seems to be: assets/models/glTF/<name>/glTF/<filename>
          // Or maybe assets/models/glTF/<name>/<filename> if the variant key is just "glTF"?
          // Let's assume the standard structure for this repo.
          // Actually, looking at 2CylinderEngine:
          // "variants": { "glTF": "2CylinderEngine.gltf" }
          // File: .../2CylinderEngine/glTF/2CylinderEngine.gltf
          // So there is an extra "glTF" folder.
          // Is it possible that the "glTF" key implies the "glTF" folder?
          // Let's assume so for now.
          entry.relativePath = "glTF/" + entry.name + "/glTF/" + variantFile;
        }

        if (!entry.relativePath.empty())
        {
          availableModels_.push_back(entry);
        }
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << "Error parsing model index: " << e.what() << std::endl;
    }
  }

  void ModelImportPanel::loadModel(const std::string& fullPath)
  {
    try
    {
      // Load the model
      auto modelPtr = Model::createModelFromGLTF(device_, fullPath, false, true, true);

      // Load textures for materials
      for (auto& mat : modelPtr->getMaterials())
      {
        if (!mat.diffuseTexPath.empty())
        {
          mat.pbrMaterial.albedoMap = resourceManager_.loadTexture(mat.diffuseTexPath, true, true);
        }
        if (!mat.normalTexPath.empty())
        {
          mat.pbrMaterial.normalMap = resourceManager_.loadTexture(mat.normalTexPath, false, true);
        }
        if (!mat.roughnessTexPath.empty())
        {
          mat.pbrMaterial.roughnessMap = resourceManager_.loadTexture(mat.roughnessTexPath, false, true);
        }
        if (!mat.aoTexPath.empty())
        {
          mat.pbrMaterial.aoMap = resourceManager_.loadTexture(mat.aoTexPath, false, true);
        }
        if (!mat.specularGlossinessTexPath.empty())
        {
          mat.pbrMaterial.specularGlossinessMap = resourceManager_.loadTexture(mat.specularGlossinessTexPath,
                                                                               true,
                                                                               true); // sRGB? Specular is color, glossiness is linear. Usually sRGB for color.
        }
        if (!mat.emissiveTexPath.empty())
        {
          mat.pbrMaterial.emissiveMap = resourceManager_.loadTexture(mat.emissiveTexPath, true, true);
        }
        if (!mat.transmissionTexPath.empty())
        {
          mat.pbrMaterial.transmissionMap = resourceManager_.loadTexture(mat.transmissionTexPath, false, true);
        }
        if (!mat.clearcoatTexPath.empty())
        {
          mat.pbrMaterial.clearcoatMap = resourceManager_.loadTexture(mat.clearcoatTexPath, false, true);
        }
        if (!mat.clearcoatRoughnessTexPath.empty())
        {
          mat.pbrMaterial.clearcoatRoughnessMap = resourceManager_.loadTexture(mat.clearcoatRoughnessTexPath, false, true);
        }
        if (!mat.clearcoatNormalTexPath.empty())
        {
          mat.pbrMaterial.clearcoatNormalMap = resourceManager_.loadTexture(mat.clearcoatNormalTexPath, false, true);
        }
      }

      auto entity = scene_.createEntity();
      scene_.getRegistry().emplace<TransformComponent>(entity);
      scene_.getRegistry().emplace<ModelComponent>(entity, std::move(modelPtr));
      scene_.getRegistry().emplace<NameComponent>(entity, "ImportedModel");

      auto& transform       = scene_.getRegistry().get<TransformComponent>(entity);
      transform.scale       = {1.0f, 1.0f, 1.0f};
      transform.translation = {0.0f, 0.0f, 0.0f};

      auto& modelComp = scene_.getRegistry().get<ModelComponent>(entity);

      // Check for animations
      if (modelComp.model->hasAnimations())
      {
        scene_.getRegistry().emplace<AnimationComponent>(entity, modelComp.model);
      }

      // Check for morph targets
      if (modelComp.model->hasMorphTargets())
      {
        // If not already registered (e.g. if it had animations it was registered above)
        if (!scene_.getRegistry().all_of<AnimationComponent>(entity))
        {
          scene_.getRegistry().emplace<AnimationComponent>(entity, modelComp.model);
        }
      }
      std::cout << "Loaded model: " << fullPath << std::endl;
    }
    catch (const std::exception& e)
    {
      std::cerr << "Failed to load model: " << e.what() << std::endl;
    }
  }

} // namespace engine
