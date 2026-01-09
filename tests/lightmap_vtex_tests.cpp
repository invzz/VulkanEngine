#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "Engine/Systems/IBLSystem.hpp"

using namespace engine;

TEST(LightmapVTEX, LoadVTEXFileFromDisk)
{
  // Ensure directories exist
  std::filesystem::create_directories("assets/lightmaps/ibltest");
  std::filesystem::create_directories("assets/scenes");

  // Create a minimal Vulkan device
  Window window(1, 1, "VTEXTestWindow");
  Device device(window);

  // Write a minimal, valid VTEX file directly to disk
  {
    using namespace engine::ibl_detail::vtex;
    const std::filesystem::path dir = "assets/lightmaps/ibltest";
    std::filesystem::create_directories(dir);
    std::ofstream out((dir / "brdf_lut.vtex").generic_string(), std::ios::binary);

    Header header{};
    header.vkFormat   = static_cast<uint32_t>(VK_FORMAT_R16G16B16A16_SFLOAT);
    header.width      = 4;
    header.height     = 4;
    header.mipLevels  = 1;
    header.layers     = 1;
    header.bytesPerPx = bytesPerPixelFor(static_cast<VkFormat>(header.vkFormat));

    size_t const      dataBytes = static_cast<size_t>(header.width) * static_cast<size_t>(header.height) * header.bytesPerPx * header.layers;
    std::vector<char> data(dataBytes, 0);

    ASSERT_TRUE(out.good());
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
  }

  // Prepare a manifest that references the generated brdf_lut.vtex file
  const std::string manifestPath = "assets/scenes/vtex_manifest.json";
  {
    std::ofstream out(manifestPath);
    out << R"({ "version": 1, "lightmaps": [ { "id": "vtex_lm", "file": "lightmaps/ibltest/brdf_lut.vtex", "format": "vtex", "resolution": [1,1], "usage": "Lightmap" } ], "lightmapBindings": {} })";
    out.close();
  }

  ResourceManager rm(device);
  ASSERT_TRUE(rm.loadSceneLightmapManifest(manifestPath));

  // Ensure file exists and load textures (should pick up the .vtex and use the VTEX loader)
  std::filesystem::path vtexFile("assets/lightmaps/ibltest/brdf_lut.vtex");
  ASSERT_TRUE(std::filesystem::exists(vtexFile)) << "Expected vtex file to exist: " << vtexFile.generic_string();

  try
  {
    ASSERT_TRUE(rm.loadSceneLightmapTextures("assets"));
  }
  catch (const std::exception& e)
  {
    FAIL() << "Exception during loadSceneLightmapTextures: " << e.what();
  }

  // Verify that the loaded texture can be applied to a material via scene binding mechanics
  Scene scene;
  auto  ent = scene.createEntity();
  scene.getRegistry().emplace<engine::NameComponent>(ent, "vtex_entity");
  scene.getRegistry().emplace<engine::LightmapComponent>(ent, engine::LightmapComponent{"vtex_lm", 1, glm::vec2(1.0f), glm::vec2(0.0f), -1});
  scene.getRegistry().emplace<engine::PBRMaterial>(ent);

  rm.applyLoadedLightmapsToScene(scene);

  auto& mat = scene.getRegistry().get<engine::PBRMaterial>(ent);
  EXPECT_TRUE(mat.lightmap != nullptr);
  EXPECT_TRUE(mat.hasLightmap());
}
