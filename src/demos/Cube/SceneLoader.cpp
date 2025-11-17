#include "SceneLoader.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "3dEngine/AnimationController.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/Texture.hpp"

namespace engine {

  void SceneLoader::loadScene(Device& device, GameObject::Map& gameObjects)
  {
    if (!gameObjects.empty())
    {
      return;
    }

    createLights(gameObjects);
    //  createFloor(device, gameObjects);
    // createApple(device, gameObjects);
    // createCylinderEngine(device, gameObjects);
    createAnimatedCube(device, gameObjects);
  }

  void SceneLoader::createFromFile(Device& device, GameObject::Map& gameObjects, const std::string& modelPath)
  {
    if (!gameObjects.empty())
    {
      return;
    }

    auto modelPtr               = Model::createModelFromFile(device, modelPath, false, true, true);
    auto model                  = GameObject::makePBRObject(std::move(modelPtr));
    model.transform.scale       = {1.0f, 1.f, 1.0f};
    model.transform.translation = {0.0f, 0.0f, 0.0f};
    gameObjects.try_emplace(model.getId(), std::move(model));
  }

  void SceneLoader::createApple(Device& device, GameObject::Map& gameObjects)
  {
    auto modelPtr               = Model::createModelFromFile(device, "/3DApple002_SQ-4K-JPG.obj", false, true, true);
    auto model                  = GameObject::makePBRObject(std::move(modelPtr));
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
        model.pbrMaterial->albedoMap = std::make_shared<Texture>(device, basePath + mat.diffuseTexPath, true);
      }

      if (!mat.normalTexPath.empty())
      {
        model.pbrMaterial->normalMap = std::make_shared<Texture>(device, basePath + mat.normalTexPath, false);
      }

      if (!mat.roughnessTexPath.empty())
      {
        model.pbrMaterial->roughnessMap = std::make_shared<Texture>(device, basePath + mat.roughnessTexPath, false);
      }

      if (!mat.aoTexPath.empty())
      {
        model.pbrMaterial->aoMap = std::make_shared<Texture>(device, basePath + mat.aoTexPath, false);
      }
    }

    model.pbrMaterial->uvScale = 1.0f;
    gameObjects.try_emplace(model.getId(), std::move(model));
  }

  void SceneLoader::createSpaceShip(Device& device, GameObject::Map& gameObjects)
  {
    auto spaceShipModel = std::shared_ptr<Model>(Model::createModelFromFile(device, "/SpaceShipModeling2.obj", false, true, false));

    auto spaceShip                  = GameObject::create();
    spaceShip.model                 = spaceShipModel;
    spaceShip.transform.scale       = {0.2f, 0.2f, 0.2f};
    spaceShip.transform.translation = {0.0f, 0.0f, 0.0f};
    gameObjects.try_emplace(spaceShip.getId(), std::move(spaceShip));
  }

  void SceneLoader::createLights(GameObject::Map& gameObjects, float radius)
  {
    std::vector<glm::vec3> allColors{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f, 1.f}, {0.f, 1.f, 1.f}, {1.f, 1.f, 0.f}};

    std::vector<glm::vec3> lightColors = allColors;

    for (size_t i = 0; i < lightColors.size(); i++)
    {
      auto pointLight  = GameObject::makePointLightObject(1.0f, lightColors[i], 0.05f);
      pointLight.color = lightColors[i];

      auto rotateLight =
              glm::rotate(glm::mat4(1.0f), (glm::two_pi<float>() * static_cast<float>(i)) / static_cast<float>(lightColors.size()), glm::vec3(0.f, -2.f, 0.f));
      pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4{-radius, -1.f, -radius, 1.f});

      gameObjects.try_emplace(pointLight.getId(), std::move(pointLight));
    }
  }

  void SceneLoader::createFloor(Device& device, GameObject::Map& gameObjects)
  {
    auto floor                  = GameObject::makePBRObject(Model::createModelFromFile(device, "/quad.obj"), {0.5f, 0.5f, 0.5f}, 0.1f, 0.1f, 0.3f);
    floor.transform.scale       = {4.0f, 1.f, 4.0f};
    floor.transform.translation = {0.0f, 0.0f, 0.0f};

    // Load asphalt textures with correct formats:
    // - BaseColor: sRGB (true) - color texture needs gamma correction
    // - Normal/Roughness/AO: Linear (false) - data textures, no gamma correction
    // - Normal map is DirectX format (Y-down), shader flips Y to convert to OpenGL/Vulkan convention (Y-up)

    auto textureName = "RedStoneWall01";
    auto path        = std::string(TEXTURE_PATH) + "/" + textureName + "_MR_4K" + "/" + textureName + "_4K_";

    floor.pbrMaterial->albedoMap    = std::make_shared<Texture>(device, path + "BaseColor.png", true);
    floor.pbrMaterial->normalMap    = std::make_shared<Texture>(device, path + "Normal.png", false);
    floor.pbrMaterial->roughnessMap = std::make_shared<Texture>(device, path + "Roughness.png", false);
    floor.pbrMaterial->aoMap        = std::make_shared<Texture>(device, path + "Height.png", false);
    floor.pbrMaterial->uvScale      = 8.0f; // Tile the texture 16 times
    gameObjects.try_emplace(floor.getId(), std::move(floor));
  }

  void SceneLoader::createBmw(Device& device, GameObject::Map& gameObjects)
  {
    // Load the BMW model (MTL materials are loaded automatically)
    auto bmwModel = std::shared_ptr<Model>(Model::createModelFromFile(device, "/bmw.obj", true, true, true));

    // BMW model now has all materials from MTL file loaded automatically
    // Create a simple GameObject with the multi-material model
    auto bmw                  = GameObject::create();
    bmw.model                 = bmwModel;
    bmw.transform.scale       = {0.004f, 0.004f, 0.004f};
    bmw.transform.translation = {0.0f, -0.03f, 0.0f};
    gameObjects.try_emplace(bmw.getId(), std::move(bmw));
  }

  void SceneLoader::createDragonGrid(Device& device, GameObject::Map& gameObjects)
  {
    auto dragon = std::shared_ptr<Model>(Model::createModelFromFile(device, "/dragon.obj", true));

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

        GameObject dragonObj = GameObject::makePBRObject(dragon, albedo, metallic, roughness, 1.0f);

        dragonObj.transform.translation = {
                static_cast<float>(col) * spacingX - originX,
                -0.5f,
                static_cast<float>(row) * spacingZ - originZ,
        };

        dragonObj.transform.scale = {0.5f, 0.5f, 0.5f};

        gameObjects.try_emplace(dragonObj.getId(), std::move(dragonObj));
      }
    }
  }

  void SceneLoader::createCylinderEngine(Device& device, GameObject::Map& gameObjects)
  {
    auto modelPtr               = Model::createModelFromGLTF(device, MODEL_PATH "/glTF/StainedGlassLamp/glTF/StainedGlassLamp.gltf", false, true, true);
    auto model                  = GameObject::makePBRObject(std::move(modelPtr));
    model.transform.scale       = {1.0f, 1.0f, 1.0f};
    model.transform.translation = {0.0f, 0.0f, 0.0f};

    // Load textures for all materials in the glTF model
    auto& materials = model.model->getMaterials();
    for (auto& mat : materials)
    {
      if (!mat.diffuseTexPath.empty())
      {
        mat.pbrMaterial.albedoMap = std::make_shared<Texture>(device, mat.diffuseTexPath, true);
      }

      if (!mat.normalTexPath.empty())
      {
        mat.pbrMaterial.normalMap = std::make_shared<Texture>(device, mat.normalTexPath, false);
      }

      if (!mat.roughnessTexPath.empty())
      {
        mat.pbrMaterial.roughnessMap = std::make_shared<Texture>(device, mat.roughnessTexPath, false);
      }

      if (!mat.aoTexPath.empty())
      {
        mat.pbrMaterial.aoMap = std::make_shared<Texture>(device, mat.aoTexPath, false);
      }
    }

    gameObjects.try_emplace(model.getId(), std::move(model));
  }

  void SceneLoader::createAnimatedCube(Device& device, GameObject::Map& gameObjects)
  {
    // AnimatedCube - uses rotation+scale animation (works)
    // AnimatedTriangle - uses rotation animation (works)
    // AnimatedMorphCube/Sphere - use morph targets (not yet supported)
    auto modelPtr               = Model::createModelFromGLTF(device, MODEL_PATH "/glTF/AnimatedTriangle/glTF/AnimatedTriangle.gltf", false, true, true);
    auto model                  = GameObject::makePBRObject(std::move(modelPtr));
    model.transform.scale       = {1.0f, 1.0f, 1.0f};
    model.transform.translation = {0.0f, 0.0f, 0.0f};

    // Load textures for all materials if present
    auto& materials = model.model->getMaterials();
    for (auto& mat : materials)
    {
      if (!mat.diffuseTexPath.empty())
      {
        mat.pbrMaterial.albedoMap = std::make_shared<Texture>(device, mat.diffuseTexPath, true);
      }
      if (!mat.normalTexPath.empty())
      {
        mat.pbrMaterial.normalMap = std::make_shared<Texture>(device, mat.normalTexPath, false);
      }
      if (!mat.roughnessTexPath.empty())
      {
        mat.pbrMaterial.roughnessMap = std::make_shared<Texture>(device, mat.roughnessTexPath, false);
      }
      if (!mat.aoTexPath.empty())
      {
        mat.pbrMaterial.aoMap = std::make_shared<Texture>(device, mat.aoTexPath, false);
      }
    }

    // Setup animation if available
    if (model.model->hasAnimations())
    {
      model.animationController = std::make_unique<AnimationController>(model.model);
      model.animationController->play(0, true); // Play first animation in loop
    }

    gameObjects.try_emplace(model.getId(), std::move(model));
  }

} // namespace engine
