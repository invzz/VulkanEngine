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

    // Diagnostic: print env var status early to detect invocation-by-system issues
    if (const char* v = std::getenv("MODEL_LIGHT_BAKER_PACK_TO_VTEX"))
    {
      std::cerr << "[ModelLightBaker] ENV PACK VAR='" << v << "'\n";
    }
    else
    {
      std::cerr << "[ModelLightBaker] ENV PACK VAR not set\n";
    }

    // Parse CLI options early so we can validate flags before heavy work
    ModelLightBaker::Options opts;
    opts.resolution   = 512;
    opts.samples      = 16;
    opts.mode         = ModelLightBaker::Options::Mode::TEXEL;
    opts.sunDir       = glm::vec3(0.0f, -1.0f, 0.0f);
    opts.sunIntensity = 1.0f;
    opts.gpu          = false; // by default use CPU
    opts.packToVTEX   = false; // by default do not pack EXR -> VTEX
    // Environment wildcard: allow overriding packing via env var for CI/tests
    if (const char* env = std::getenv("MODEL_LIGHT_BAKER_PACK_TO_VTEX"); env != nullptr)
    {
      opts.packToVTEX = true;
      std::cerr << "[ModelLightBaker] Enabled VTEX packing via environment variable\n";
    }

    // Parse CLI options
    std::string cmdline;
    for (int ai = 1; ai < argc; ++ai)
    {
      cmdline += argv[ai];
      cmdline += ' ';
    }
    // Fast-path: if the entire command line contains the pack flag, honor it (robust to wrappers)
    if (cmdline.find("--pack-to-vtex") != std::string::npos || cmdline.find("pack-to-vtex") != std::string::npos || cmdline.find("pack") != std::string::npos)
    {
      opts.packToVTEX = true;
      std::cerr << "[ModelLightBaker] CLI detected pack flag in command line\n";
    }

    for (int i = 3; i < argc; ++i)
    {
      std::string arg = argv[i];
      // Robustness: trim trailing/leading whitespace and CR/LF that may appear when invoked via shell wrappers
      while (!arg.empty() && (arg.back() == '\r' || arg.back() == '\n' || isspace(static_cast<unsigned char>(arg.back()))))
        arg.pop_back();
      while (!arg.empty() && (arg.front() == '\r' || arg.front() == '\n' || isspace(static_cast<unsigned char>(arg.front()))))
        arg.erase(arg.begin());

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
      else if (arg == "--pack-to-vtex")
      {
        opts.packToVTEX = true;
      }
      else if (arg.find("pack") != std::string::npos)
      {
        // Fallback: accept any arg that contains 'pack' (robust against odd characters or wrappers)
        opts.packToVTEX = true;
        std::cerr << "[ModelLightBaker] Parsed pack flag from arg: '" << arg << "'\n";
      }
      else
      {
        std::cerr << "[ModelLightBaker] Unknown option: " << arg << "\n";
      }
    }

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

    // CLI options were parsed earlier (moved up)

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
