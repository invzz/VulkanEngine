#include <filesystem>
#include <iostream>
#include <string>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/Model.hpp"
#include "ModelLightBaker.hpp"

int main(int argc, char** argv)
{
  try
  {
    if (argc < 3)
    {
      std::cout << "Usage: ModelLightBaker <modelFile> <outputDir> [options]\n";
      std::cout << "Options:\n  --res <px>      : bake resolution (default 512)\n  --samples <n>   : samples for soft penumbra (default 16)\n  --sun-dir x y z : sun direction (default 0 -1 0)\n  "
                   "--sun-intensity f: sun intensity (default 1.0)\n  --mode <texel|vertex> : bake granularity (default texel)\n  --epsilon <exp> : ray epsilon exponent (e.g. -6 means 1e-6)\n  "
                   "--padding <f>   : BVH padding fraction of scene extent (default 0.02)\n";
      return 1;
    }

    std::string const modelFile = argv[1];
    std::string const outputDir = argv[2];

    // Minimal hidden Vulkan window (required by current Device/Surface design).
    engine::Window window{16, 16, "Model Light Baker"};
    engine::Device device{window};

    std::cout << "[ModelLightBaker] Loading model: " << modelFile << "\n";
    std::unique_ptr<engine::Model> model;
    // Choose importer based on extension (.gltf/.glb -> glTF importer)
    std::string ext;
    {
      std::filesystem::path p(modelFile);
      ext = p.extension().string();
      for (auto& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    try
    {
      if (ext == ".gltf" || ext == ".glb")
      {
        model = engine::Model::createModelFromGLTF(device, modelFile);
      }
      else
      {
        model = engine::Model::createModelFromFile(device, modelFile);
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << "[ModelLightBaker] Failed to load model: " << e.what() << "\n";
      return 2;
    }
    if (!model)
    {
      std::cerr << "[ModelLightBaker] Failed to load model (null)\n";
      return 2;
    }

    ModelLightBaker::Options opts;
    opts.resolution   = 512;
    opts.samples      = 16;
    opts.mode         = ModelLightBaker::Options::Mode::TEXEL;
    opts.sunDir       = glm::vec3(0.0f, -1.0f, 0.0f);
    opts.sunIntensity = 1.0f;
    opts.gpu          = false; // by default use CPU

    // Parse CLI options
    for (int i = 3; i < argc; ++i)
    {
      std::string arg = argv[i];
      if (arg == "--res" && i + 1 < argc)
      {
        opts.resolution = std::stoi(argv[++i]);
      }
      else if (arg == "--samples" && i + 1 < argc)
      {
        opts.samples = std::stoi(argv[++i]);
      }
      else if (arg == "--sun-dir" && i + 3 < argc)
      {
        opts.sunDir.x = std::stof(argv[++i]);
        opts.sunDir.y = std::stof(argv[++i]);
        opts.sunDir.z = std::stof(argv[++i]);
      }
      else if (arg == "--sun-intensity" && i + 1 < argc)
      {
        opts.sunIntensity = std::stof(argv[++i]);
      }
      else if (arg == "--mode" && i + 1 < argc)
      {
        std::string m = argv[++i];
        if (m == "texel")
          opts.mode = ModelLightBaker::Options::Mode::TEXEL;
        else if (m == "vertex")
          opts.mode = ModelLightBaker::Options::Mode::VERTEX;
        else if (m == "mesh")
          opts.mode = ModelLightBaker::Options::Mode::MESH;
      }
      else if (arg == "--mesh")
      {
        opts.mode = ModelLightBaker::Options::Mode::MESH;
      }
      else if (arg == "--chunk-size" && i + 1 < argc)
      {
        opts.meshChunkSize = std::stof(argv[++i]);
        std::cout << "[ModelLightBaker] Using mesh chunk size=" << opts.meshChunkSize << " m\n";
      }
      else if (arg == "--epsilon" && i + 1 < argc)
      {
        // Exponent form: e.g. -6 => 1e-6
        opts.sampleEpsilonExponent = std::stoi(argv[++i]);
      }
      else if (arg == "--padding" && i + 1 < argc)
      {
        opts.bvhPaddingFraction = std::stof(argv[++i]);
      }
      else if (arg == "--gpu")
      {
        opts.gpu = true;
      }
      else if (arg == "--preview")
      {
        opts.preview = true; // set preview flag if present
        // optional size next
        if (i + 1 < argc && argv[i + 1][0] != '-')
        {
          opts.previewMaxSize = std::stoi(argv[++i]);
        }
      }
      else
      {
        std::cerr << "[ModelLightBaker] Unknown option: " << arg << "\n";
      }
    }

    ModelLightBaker::ModelLightBaker baker{device, *model, opts, outputDir};

    std::cout << "[ModelLightBaker] Baking (direct sun shadows + soft penumbra + ambient)...\n";
    baker.bake();

    std::cout << "[ModelLightBaker] Saving outputs to: " << outputDir << "\n";
    if (!baker.saveToDisk())
    {
      std::cerr << "[ModelLightBaker] Failed to save bake outputs\n";
      return 3;
    }

    std::cout << "[ModelLightBaker] Done.\n";
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "[ModelLightBaker] Fatal: " << e.what() << "\n";
    return 10;
  }
}
