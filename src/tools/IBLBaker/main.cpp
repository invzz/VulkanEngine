#include <iostream>
#include <string>

#include "Engine/Core/Window.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/IBLSystem.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 3) {
      std::cout << "Usage: IBLBaker <skyboxFolder> <outputDir> [ext]\n";
      std::cout << "Example: IBLBaker \"" << TEXTURE_PATH << "/skybox/Yokohama\" \"" << TEXTURE_PATH << "/ibl/Yokohama\" jpg\n";
      return 1;
    }

    std::string const skyboxFolder = argv[1];
    std::string const outputDir = argv[2];
    std::string const extension = (argc >= 4) ? argv[3] : "jpg";

    // Minimal hidden Vulkan window (required by current Device/Surface design).
    engine::Window window{16, 16, "IBL Baker"};
    engine::Device device{window};

    std::cout << "[IBLBaker] Loading skybox from: " << skyboxFolder << "\n";
    auto skybox = engine::Skybox::loadFromFolder(device, skyboxFolder, extension);
    if (!skybox) {
      std::cerr << "[IBLBaker] Failed to load skybox\n";
      return 2;
    }

    engine::IBLSystem ibl{device};

    // Generate (slow) then write assets. This is intended to be run offline.
    std::cout << "[IBLBaker] Generating IBL...\n";
    ibl.generateFromSkybox(*skybox);

    std::cout << "[IBLBaker] Writing VTEX assets to: " << outputDir << "\n";
    if (!ibl.saveToDisk(outputDir)) {
      std::cerr << "[IBLBaker] Failed to write VTEX assets\n";
      return 3;
    }

    std::cout << "[IBLBaker] Done.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "[IBLBaker] Fatal: " << e.what() << "\n";
    return 10;
  }
}
