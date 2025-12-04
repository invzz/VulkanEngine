#include "ModelImportPanel.hpp"

#include <imgui.h>

#include <cstring>
#include <iostream>

#include "3dEngine/AnimationController.hpp"
#include "3dEngine/GameObjectManager.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/PBRMaterial.hpp"
#include "3dEngine/Texture.hpp"

namespace engine {

  ModelImportPanel::ModelImportPanel(Device& device, GameObjectManager& objectManager, AnimationSystem& animationSystem)
      : device_(device), objectManager_(objectManager), animationSystem_(animationSystem)
  {}

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
        try
        {
          // Create full path if it's relative
          std::string fullPath = modelPath_;
          if (fullPath[0] != '/')
          {
            fullPath = std::string(MODEL_PATH) + "/" + fullPath;
          }

          // Load the model
          auto modelPtr                   = Model::createModelFromGLTF(device_, fullPath, false, true, true);
          auto newObject                  = GameObject::makePBRObject({.model = std::move(modelPtr)});
          newObject.transform.scale       = {1.0f, 1.0f, 1.0f};
          newObject.transform.translation = {0.0f, 0.0f, 0.0f};

          // Load textures from materials if available
          if (newObject.model && !newObject.model->getMaterials().empty())
          {
            auto& materials = newObject.model->getMaterials(); // Get mutable reference
            for (auto& matInfo : materials)
            {
              // Load textures for each material in the model
              try
              {
                if (!matInfo.diffuseTexPath.empty())
                {
                  matInfo.pbrMaterial.albedoMap = std::make_shared<Texture>(device_, matInfo.diffuseTexPath, true);
                  std::cout << "[ModelImport] Loaded albedo texture: " << matInfo.diffuseTexPath << std::endl;
                }

                if (!matInfo.normalTexPath.empty())
                {
                  matInfo.pbrMaterial.normalMap = std::make_shared<Texture>(device_, matInfo.normalTexPath, false);
                  std::cout << "[ModelImport] Loaded normal texture: " << matInfo.normalTexPath << std::endl;
                }

                if (!matInfo.roughnessTexPath.empty())
                {
                  matInfo.pbrMaterial.roughnessMap = std::make_shared<Texture>(device_, matInfo.roughnessTexPath, false);
                  std::cout << "[ModelImport] Loaded roughness texture: " << matInfo.roughnessTexPath << std::endl;
                }

                if (!matInfo.aoTexPath.empty())
                {
                  matInfo.pbrMaterial.aoMap = std::make_shared<Texture>(device_, matInfo.aoTexPath, false);
                  std::cout << "[ModelImport] Loaded AO texture: " << matInfo.aoTexPath << std::endl;
                }
              }
              catch (const std::exception& e)
              {
                std::cerr << "[ModelImport] Warning: Failed to load texture: " << e.what() << std::endl;
              }
            }
          }

          // Setup animation if available
          if (newObject.model->hasAnimations())
          {
            newObject.animationController = std::make_unique<AnimationController>(newObject.model);
            newObject.animationController->play(0, true);
          }

          GameObject::id_t newId = newObject.getId();
          objectManager_.addObject(std::move(newObject));

          // Register with animation system if needed
          auto* obj = objectManager_.getObject(newId);
          if (obj && (obj->animationController || (obj->model && obj->model->hasMorphTargets())))
          {
            animationSystem_.registerAnimatedObject(newId);
          }

          std::cout << "[ModelImport] Loaded model: " << fullPath << " (ID: " << newId << ")" << std::endl;
          modelPath_[0] = '\0'; // Clear input
        }
        catch (const std::exception& e)
        {
          std::cerr << "[ModelImport] ERROR loading model: " << e.what() << std::endl;
        }
      }

      ImGui::Text("Quick Load:");
      if (ImGui::Button("Animated Cube"))
      {
        strcpy(modelPath_, "glTF/AnimatedMorphCube/glTF/AnimatedMorphCube.gltf");
      }
      ImGui::SameLine();
      if (ImGui::Button("Animated Triangle"))
      {
        strcpy(modelPath_, "glTF/AnimatedTriangle/glTF/AnimatedTriangle.gltf");
      }
    }
  }

} // namespace engine
