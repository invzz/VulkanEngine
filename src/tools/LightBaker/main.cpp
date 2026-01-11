#include <tinyexr.h>

#include <Engine/Resources/Model.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Tools/LightBaker/LightBaker.hpp"
#include "Tools/LightmapBakerLib/Geometry.hpp"
#include "Tools/LightmapBakerLib/Scene.hpp"
namespace {
  void print_usage()
  {
    std::cout << "LightBaker - simple replacement for ModelLightBaker\n";
    std::cout << "Usage:\n";
    std::cout << "  --model <model_path>       Bake a single model (path to model file)\n";
    std::cout << "  --scene <scene.json>       Bake an entire scene (collects instances)\n";
    std::cout << "  --out <outdir>             Output directory (default: ./lightbake_out)\n";
    std::cout << "  --resolution <int>         Force resolution (optional)\n";
    std::cout << "  --sun-intensity <float>    Sun intensity (default: 1.0)\n";
  }
} // namespace

int main(int argc, char** argv)
{
  std::string modelPath;
  std::string scenePath;
  std::string outDir       = "lightbake_out";
  int         resolution   = 0;
  float       sunIntensity = 1.0f;

  for (int i = 1; i < argc; ++i)
  {
    std::string a = argv[i];
    if (a == "--model" && i + 1 < argc)
    {
      modelPath = argv[++i];
    }
    else if (a == "--scene" && i + 1 < argc)
    {
      scenePath = argv[++i];
    }
    else if (a == "--out" && i + 1 < argc)
    {
      outDir = argv[++i];
    }
    else if (a == "--resolution" && i + 1 < argc)
    {
      resolution = std::stoi(argv[++i]);
    }
    else if (a == "--sun-intensity" && i + 1 < argc)
    {
      sunIntensity = std::stof(argv[++i]);
    }
    else if (a == "--help" || a == "-h")
    {
      print_usage();
      return 0;
    }
    else
    {
      std::cerr << "Unknown arg: " << a << "\n";
      print_usage();
      return 1;
    }
  }

  std::filesystem::create_directories(outDir);

  try
  {
    if (!scenePath.empty())
    {
      // Scene bake
      LightmapBaker::Scene scene;
      std::string          sceneErr;
      if (!scene.loadFromFile(scenePath, &sceneErr))
      {
        std::cerr << "Failed to load scene: " << scenePath << " - " << sceneErr << "\n";
        return 1;
      }

      // model loader callback: try to load model via Builder
      auto loader = [&](const std::string& modelPathStr, engine::Model::Builder& out) -> bool {
        try
        {
          std::filesystem::path p(modelPathStr);
          std::string           ext = p.extension().string();
          for (auto& c : ext)
          {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
          }
          if (ext == ".gltf" || ext == ".glb")
          {
            out.loadModelFromGLTF(modelPathStr);
          }
          else
          {
            out.loadModelFromFile(modelPathStr);
          }
          return true;
        }
        catch (const std::exception& e)
        {
          std::cerr << "Model loader failed for " << modelPathStr << ": " << e.what() << "\n";
          return false;
        }
      };

      auto tris = LightmapBaker::collectTrianglesFromScene(scene, loader);
      if (tris.empty())
      {
        std::cerr << "No triangles extracted from scene\n";
        return 1;
      }

      auto res = LightBaker::bakeTriangles(tris, resolution, sunIntensity);

      // Save EXR
      std::filesystem::path exrPath = std::filesystem::path(outDir) / "scene_bake.exr";
      const char*           err     = nullptr;
      int                   r       = SaveEXR(res.hdrPixels.data(), res.width, res.height, 3, 0, exrPath.string().c_str(), &err);
      if (r != TINYEXR_SUCCESS)
      {
        std::cerr << "SaveEXR error: " << ((err != nullptr) ? err : "unknown") << "\n";
        return 1;
      }

      // Save a simple manifest
      nlohmann::json m;
      m["file"]   = exrPath.filename().string();
      m["width"]  = res.width;
      m["height"] = res.height;
      std::ofstream outf(std::filesystem::path(outDir) / "scene_bake_manifest.json");
      outf << m.dump(2);
      std::cout << "Scene bake complete, output -> " << exrPath << "\n";
      return 0;
    }
    if (!modelPath.empty())
    {
      // Single model bake
      engine::Model::Builder builder;
      try
      {
        std::filesystem::path p(modelPath);
        std::string           ext = p.extension().string();
        for (auto& c : ext)
        {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext == ".gltf" || ext == ".glb")
        {
          builder.loadModelFromGLTF(modelPath);
        }
        else
        {
          builder.loadModelFromFile(modelPath);
        }
      }
      catch (const std::exception& e)
      {
        std::cerr << "Failed to load model: " << e.what() << "\n";
        return 1;
      }

      auto tris = LightmapBaker::collectTrianglesFromBuilder(builder, glm::mat4(1.0f));
      if (tris.empty())
      {
        std::cerr << "No triangles found in model\n";
        return 1;
      }

      auto                  res     = LightBaker::bakeTriangles(tris, resolution, sunIntensity);
      std::filesystem::path exrPath = std::filesystem::path(outDir) / (std::filesystem::path(modelPath).stem().string() + std::string(".exr"));
      const char*           err     = nullptr;
      int                   r       = SaveEXR(res.hdrPixels.data(), res.width, res.height, 3, 0, exrPath.string().c_str(), &err);
      if (r != TINYEXR_SUCCESS)
      {
        std::cerr << "SaveEXR error: " << ((err != nullptr) ? err : "unknown") << "\n";
        return 1;
      }

      nlohmann::json m;
      m["file"]   = exrPath.filename().string();
      m["width"]  = res.width;
      m["height"] = res.height;
      std::ofstream outstd(std::filesystem::path(outDir) / (std::filesystem::path(modelPath).stem().string() + std::string(".manifest.json")));
      outstd << m.dump(2);
      std::cout << "Model bake complete, output -> " << exrPath << "\n";
      return 0;
    }

    std::cerr << "No --model or --scene provided\n";
    print_usage();
    return 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Unhandled exception: " << e.what() << "\n";
    return 1;
  }
}
