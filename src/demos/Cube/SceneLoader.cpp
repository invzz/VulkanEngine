#include "SceneLoader.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Scene/AnimationController.hpp"

namespace engine {

  void SceneLoader::loadScene(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager)
  {
    if (!objectManager.getAllObjects().empty())
    {
      return;
    }
    createFloor(device, objectManager, resourceManager);
    createDemoObject(device, objectManager, resourceManager);
    // createApple(device, objectManager, resourceManager);
    // createCylinderEngine(device, objectManager, resourceManager);
    // createAnimatedCube(device, objectManager, resourceManager);
    createLights(objectManager, 2.0f);
  }

  void SceneLoader::createFromFile(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager, const std::string& modelPath)
  {
    if (!objectManager.getAllObjects().empty())
    {
      return;
    }

    auto modelPtr               = resourceManager.loadModel(modelPath, false, true, true);
    auto model                  = GameObject::makePBRObject({.model = modelPtr});
    model.transform.scale       = {1.0f, 1.f, 1.0f};
    model.transform.translation = {0.0f, 0.0f, 0.0f};
    objectManager.addObject(std::move(model));
  }

  void SceneLoader::createApple(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager)
  {
    auto modelPtr               = resourceManager.loadModel(MODEL_PATH "/3DApple002_SQ-4K-JPG.obj", false, true, true);
    auto model                  = GameObject::makePBRObject({.model = modelPtr});
    model.transform.scale       = {5.0f, 5.f, 5.0f};
    model.transform.translation = {0.0f, 0.0f, 0.0f};

    // Load textures from MTL file material definitions
    // - BaseColor: sRGB (true) - color texture needs gamma correction
    // - Normal/Roughness/AO: Linear (false) - data textures, no gamma correction
    // - Normal map is DirectX format (Y-down), shader flips Y to convert to OpenGL/Vulkan convention (Y-up)

    const auto& materials = model.model->getMaterials();
    if (!materials.empty())
    {
      const auto& mat = materials[0];

      // Build base path for textures (they're relative to the model file location)
      std::string basePath = std::string(TEXTURE_PATH) + "/3DApple002_SQ-4K-JPG/";

      if (!mat.diffuseTexPath.empty())
      {
        model.getComponent<PBRMaterial>()->albedoMap = resourceManager.loadTexture(basePath + mat.diffuseTexPath, true);
      }

      if (!mat.normalTexPath.empty())
      {
        model.getComponent<PBRMaterial>()->normalMap = resourceManager.loadTexture(basePath + mat.normalTexPath, false);
      }

      if (!mat.roughnessTexPath.empty())
      {
        model.getComponent<PBRMaterial>()->roughnessMap = resourceManager.loadTexture(basePath + mat.roughnessTexPath, false);
      }

      if (!mat.aoTexPath.empty())
      {
        model.getComponent<PBRMaterial>()->aoMap = resourceManager.loadTexture(basePath + mat.aoTexPath, false);
      }
    }

    model.getComponent<PBRMaterial>()->uvScale = 1.0f;
    objectManager.addObject(std::move(model));
  }

  void SceneLoader::createSpaceShip(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager)
  {
    auto spaceShipModel = resourceManager.loadModel(MODEL_PATH "/SpaceShipModeling2.obj", false, true, false);

    auto spaceShip                  = GameObject::create();
    spaceShip.model                 = spaceShipModel;
    spaceShip.transform.scale       = {0.2f, 0.2f, 0.2f};
    spaceShip.transform.translation = {0.0f, 0.0f, 0.0f};
    objectManager.addObject(std::move(spaceShip));
  }

  void SceneLoader::createLights(GameObjectManager& objectManager, float radius)
  {
    std::vector<glm::vec3> lightColors = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}};

    // Create point lights in a circle (limit to 4 for shadow maps)
    for (size_t i = 0; i < lightColors.size(); i++)
    {
      auto pointLight  = GameObject::makePointLightObject({.intensity = 1.0f, .color = lightColors[i], .radius = 0.05f});
      pointLight.color = lightColors[i];

      auto rotateLight =
              glm::rotate(glm::mat4(1.0f), (glm::two_pi<float>() * static_cast<float>(i)) / static_cast<float>(lightColors.size()), glm::vec3(0.f, -1.f, 0.f));
      pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4{-radius, -2.f, -radius, 1.f});

      objectManager.addObject(std::move(pointLight));
    }

    // Add a directional light (sun-like, from above)
    auto directionalLight = GameObject::makeDirectionalLightObject({.intensity = 0.5f, .color = {1.0f, 0.95f, 0.9f}}); // Warm sunlight

    directionalLight.transform.rotation = glm::vec3(glm::radians(-45.0f), glm::radians(30.0f), 0.0f); // Angled down
    objectManager.addObject(std::move(directionalLight));

    // Add a spotlight
    auto spotLight1 =
            GameObject::makeSpotLightObject({.intensity = 15.0f, .color = {1.0f, 0.8f, 0.5f}, .innerAngle = 12.5f, .outerAngle = 17.5f}); // Warm spotlight
    spotLight1.transform.translation                              = glm::vec3(3.0f, -3.0f, 3.0f);
    spotLight1.getComponent<SpotLightComponent>()->useTargetPoint = true;
    spotLight1.getComponent<SpotLightComponent>()->targetPoint    = glm::vec3(0.0f, 0.0f, 0.0f);
    objectManager.addObject(std::move(spotLight1));
  }

  void SceneLoader::createFloor(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager)
  {
    auto floor                  = GameObject::makePBRObject({
                             .model     = resourceManager.loadModel(MODEL_PATH "/quad.obj", false, false, false),
                             .albedo    = {1.0f, 1.0f, 1.0f}, // White multiplier (textures define actual color)
                             .metallic  = 0.0f,               // Non-metallic (asphalt is dielectric)
                             .roughness = 1.0f,               // Multiplier for roughness texture
                             .ao        = 1.0f,               // Multiplier for AO texture
    });
    floor.transform.scale       = {4.0f, 1.f, 4.0f};
    floor.transform.translation = {0.0f, 0.0f, 0.0f};

    // Load PBR textures with correct formats:
    // - BaseColor: sRGB (true)  - color texture needs gamma correction (loaded in sRGB, auto-converted to linear)
    // - Normal:    Linear (false) - tangent-space normal data, no gamma correction
    // - Roughness: Linear (false) - scalar roughness data, no gamma correction
    // - AO:        Linear (false) - ambient occlusion data, no gamma correction
    // Note: Normal maps are DirectX format (Y-down), shader flips Y to OpenGL/Vulkan (Y-up) convention

    // Available materials: "Asphalt01", "RedStoneWall01", "MarbleTiles01"
    auto textureName = "Asphalt01";
    auto path        = std::string(TEXTURE_PATH) + "/" + textureName + "_MR_4K" + "/" + textureName + "_4K_";

    floor.getComponent<PBRMaterial>()->albedoMap    = resourceManager.loadTexture(path + "BaseColor.png", true);
    floor.getComponent<PBRMaterial>()->normalMap    = resourceManager.loadTexture(path + "Normal.png", false);
    floor.getComponent<PBRMaterial>()->roughnessMap = resourceManager.loadTexture(path + "Roughness.png", false);
    floor.getComponent<PBRMaterial>()->aoMap        = resourceManager.loadTexture(path + "AO.png", false);
    floor.getComponent<PBRMaterial>()->uvScale      = 8.0f; // Tile the texture 8x across the floor
    objectManager.addObject(std::move(floor));
  }

  void SceneLoader::createBmw(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager)
  {
    // Load the BMW model (MTL materials are loaded automatically)
    auto bmwModel = resourceManager.loadModel(MODEL_PATH "/bmw.obj", true, true, true);

    // BMW model now has all materials from MTL file loaded automatically
    // Create a simple GameObject with the multi-material model
    auto bmw                  = GameObject::create();
    bmw.model                 = bmwModel;
    bmw.transform.scale       = {0.004f, 0.004f, 0.004f};
    bmw.transform.translation = {0.0f, -0.03f, 0.0f};
    objectManager.addObject(std::move(bmw));
  }

  void SceneLoader::createDragonGrid(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager)
  {
    auto dragon = resourceManager.loadModel(MODEL_PATH "/dragon.obj", true, false, false);

    // Create PBR dragons in a 2D grid
    // Columns: varying metallic (0.0 -> 1.0)
    // Rows:    varying roughness (1.0 -> 0.0)
    const int   rows     = 5;
    const int   cols     = 5;
    const float spacingX = 0.6f;
    const float spacingZ = 0.6f;
    const float originX  = (static_cast<float>(cols) - 1) * spacingX / 2.0f;
    const float originZ  = (static_cast<float>(rows) - 1) * spacingZ / 2.0f;

    for (int row = 0; row < rows; row++)
    {
      float roughness = 1.0f - (static_cast<float>(row) / static_cast<float>(rows - 1));
      for (int col = 0; col < cols; col++)
      {
        float     metallic = static_cast<float>(col) / static_cast<float>(cols - 1);
        glm::vec3 albedo   = {0.8f, 0.6f, 0.2f}; // Gold color

        GameObject dragonObj = GameObject::makePBRObject({.model = dragon, .albedo = albedo, .metallic = metallic, .roughness = roughness, .ao = 1.0f});

        dragonObj.transform.translation = {
                static_cast<float>(col) * spacingX - originX,
                -0.5f,
                static_cast<float>(row) * spacingZ - originZ,
        };

        dragonObj.transform.scale = {0.5f, 0.5f, 0.5f};

        objectManager.addObject(std::move(dragonObj));
      }
    }
  }

  void SceneLoader::createCylinderEngine(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager)
  {
    auto modelPtr               = Model::createModelFromGLTF(device, MODEL_PATH "/glTF/StainedGlassLamp/glTF/StainedGlassLamp.gltf", false, true, true);
    auto model                  = GameObject::makePBRObject({.model = std::move(modelPtr)});
    model.transform.scale       = {1.0f, 1.0f, 1.0f};
    model.transform.translation = {0.0f, 0.0f, 0.0f};

    // Load textures for all materials in the glTF model
    auto& materials = model.model->getMaterials();
    for (auto& mat : materials)
    {
      if (!mat.diffuseTexPath.empty())
      {
        mat.pbrMaterial.albedoMap = resourceManager.loadTexture(mat.diffuseTexPath, true);
      }

      if (!mat.normalTexPath.empty())
      {
        mat.pbrMaterial.normalMap = resourceManager.loadTexture(mat.normalTexPath, false);
      }

      if (!mat.roughnessTexPath.empty())
      {
        mat.pbrMaterial.roughnessMap = resourceManager.loadTexture(mat.roughnessTexPath, false);
      }

      if (!mat.aoTexPath.empty())
      {
        mat.pbrMaterial.aoMap = resourceManager.loadTexture(mat.aoTexPath, false);
      }
    }

    objectManager.addObject(std::move(model));
  }

  void SceneLoader::createDemoObject(Device& device, GameObjectManager& objectManager, ResourceManager& resourceManager)
  {
    auto modelPtr               = Model::createModelFromGLTF(device, MODEL_PATH "/glTF/StainedGlassLamp/glTF/StainedGlassLamp.gltf", false, true, true);
    auto model                  = GameObject::create();
    model.model                 = std::move(modelPtr);
    model.transform.scale       = {10.0f, 10.0f, 10.0f};
    model.transform.translation = {0.0f, 0.0f, 0.0f};

    // Load textures for all materials if present
    auto& materials = model.model->getMaterials();
    for (auto& mat : materials)
    {
      if (!mat.diffuseTexPath.empty())
      {
        mat.pbrMaterial.albedoMap = resourceManager.loadTexture(mat.diffuseTexPath, true);
      }
      if (!mat.normalTexPath.empty())
      {
        mat.pbrMaterial.normalMap = resourceManager.loadTexture(mat.normalTexPath, false);
      }
      if (!mat.roughnessTexPath.empty())
      {
        mat.pbrMaterial.roughnessMap = resourceManager.loadTexture(mat.roughnessTexPath, false);
      }
      if (!mat.aoTexPath.empty())
      {
        mat.pbrMaterial.aoMap = resourceManager.loadTexture(mat.aoTexPath, false);
      }
    }

    // Setup animation if available
    if (model.model->hasAnimations())
    {
      auto& animCtrl = model.addComponent<AnimationController>(model.model);
      animCtrl.play(0, true); // Play first animation in loop
    }

    objectManager.addObject(std::move(model));
  }

} // namespace engine
