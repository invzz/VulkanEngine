#include <gtest/gtest.h>
#include <tinyexr.h>

#include <filesystem>
#include <fstream>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"

using namespace engine;

TEST(LightmapEXR, LoadEXRFile)
{
  std::filesystem::create_directories("assets/lightmaps");
  const std::string exrPath = "assets/lightmaps/test_lm.exr";

  // Create a tiny 2x2 float RGBA EXR
  std::vector<float> pixels(2 * 2 * 4, 0.0f);
  // set a non-zero pixel so we can detect non-empty content
  pixels[0] = 1.0f;

  const char* err = nullptr;
  int         ret = SaveEXR(pixels.data(), 2, 2, 4, 0, exrPath.c_str(), &err);
  if (ret != 0 && err)
  {
    std::cerr << "SaveEXR error: " << err << std::endl;
    FreeEXRErrorMessage(err);
  }
  if (ret != 0)
  {
    if (err)
    {
      std::cerr << "SaveEXR failed: " << err << "\n";
      FreeEXRErrorMessage(err);
    }
    FAIL() << "Failed to write EXR file for test";
  }

  Window          window(1, 1, "EXRTest");
  Device          device(window);
  ResourceManager rm(device);

  // Populate manifest data directly
  ResourceManager::LightmapInfo info;
  info.id   = "exr_lm";
  info.file = "lightmaps/test_lm.exr";
  rm.loadSceneLightmapManifest("assets/scenes/does_not_exist.json"); // ensure maps exist (no-op)
  {
    // Inject directly into internal map for test purposes
    // This is a test-only shortcut; in production the manifest parser would fill this
    // (We use friend access by placing tests in same directory, but for simplicity use the public API)
    // Emulate what loadSceneLightmapManifest would do
  }
  // Emplace into ResourceManager via public API: (hack: call register directly after manual load)
  // Instead, use sceneLightmaps_ by calling loadSceneLightmapManifest with a small manifest file.
  std::filesystem::create_directories("assets/scenes");
  {
    std::ofstream out("assets/scenes/exr_manifest.json");
    out << R"({ "version": 1, "lightmaps": [ { "id": "exr_lm", "file": "lightmaps/test_lm.exr", "format": "exr", "resolution": [2,2], "usage": "Lightmap" } ], "lightmapBindings": {} })";
    out.close();
  }

  ASSERT_TRUE(rm.loadSceneLightmapManifest("assets/scenes/exr_manifest.json"));
  // Load textures
  ASSERT_TRUE(rm.loadSceneLightmapTextures("assets"));

  // Check that texture was registered for the id
  // There's no public getter for sceneLightmapTextures_, but we can register and then attempt to find it via applying to a fake scene entity.
  Scene scene;
  auto  ent = scene.createEntity();
  scene.getRegistry().emplace<engine::NameComponent>(ent, "exr_entity");
  scene.getRegistry().emplace<engine::LightmapComponent>(ent, engine::LightmapComponent{"exr_lm", 1, glm::vec2(1.0f), glm::vec2(0.0f), -1});
  scene.getRegistry().emplace<engine::PBRMaterial>(ent);

  rm.applyLoadedLightmapsToScene(scene);

  auto& mat = scene.getRegistry().get<engine::PBRMaterial>(ent);
  EXPECT_TRUE(mat.lightmap != nullptr);
  EXPECT_TRUE(mat.hasLightmap());
}
