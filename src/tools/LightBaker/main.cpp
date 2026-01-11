#include <tinyexr.h>

#include <ModelLib/Resources/Model.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "Tools/LightBaker/LightBaker.hpp"
#include "Tools/LightmapBakerLib/Geometry.hpp"
#include "Tools/LightmapBakerLib/Pack.hpp"
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
  bool        packToVtex   = false;
  bool        autoUV       = false; // generate UV1 mappings automatically using UVUnwrap

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
    else if (a == "--pack-to-vtex")
    {
      packToVtex = true;
    }
    else if (a == "--auto-uv")
    {
      autoUV = true;
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
      // Note: the loader will apply any precomputed per-instance UVs for models in order of calls.
      // Precomputed per-model UVs (local-space results indexed by node)
      std::unordered_map<std::string, std::vector<LightmapBaker::PerVertexUVResult>> precomputedPerModelUVs;

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

          // Apply precomputed per-model UVs (local-space) for this model if available.
          auto it = precomputedPerModelUVs.find(modelPathStr);
          if (it != precomputedPerModelUVs.end())
          {
            const auto& perNode = it->second;
            // For each node for which we have UVs, copy per-vertex UVs back to the builder
            size_t nodeCount = std::min(perNode.size(), out.nodes.size());
            for (size_t ni = 0; ni < nodeCount; ++ni)
            {
              const auto& uvRes = perNode[ni];
              if (uvRes.used.empty()) continue;
              // Apply UVs to vertices that were marked used
              size_t vertexCount = out.vertices.size();
              for (size_t vi = 0; vi < vertexCount && vi < uvRes.used.size(); ++vi)
              {
                if (uvRes.used[vi]) out.vertices[vi].uv = uvRes.uvPerVertex[vi];
              }
            }
          }

          return true;
        }
        catch (const std::exception& e)
        {
          std::cerr << "Model loader failed for " << modelPathStr << ": " << e.what() << "\n";
          return false;
        }
      };

      // Optionally, compute UV1 mappings per-instance before baking if requested
      nlohmann::json sceneLightmapBindings = nlohmann::json::object();
      if (autoUV)
      {
        std::cout << "Auto-UV enabled: generating per-model UV1 mappings (local-space)...\n";

        // Precompute per-model per-node UVs and per-model mesh entries
        std::unordered_map<std::string, std::vector<LightmapBaker::PerVertexUVResult>>        precomputedPerModel;
        std::unordered_map<std::string, std::unordered_map<int, std::vector<nlohmann::json>>> perModelMeshEntries;

        // Collect unique model paths
        std::unordered_set<std::string> uniqueModels;
        for (const auto& obj : scene.objects)
          if (obj.modelPath) uniqueModels.insert(*obj.modelPath);

        for (const auto& modelPath : uniqueModels)
        {
          try
          {
            engine::Model::Builder builder;
            if (!loader(modelPath, builder))
            {
              std::cerr << "Warning: failed to load model for UV generation: " << modelPath << "\n";
              continue;
            }

            std::vector<LightmapBaker::PerVertexUVResult> perNodeResults;
            perNodeResults.resize(builder.nodes.size());

            for (int ni = 0; ni < static_cast<int>(builder.nodes.size()); ++ni)
            {
              // Generate UVs in local space (identity transform) so instances share the same bake
              auto res           = LightmapBaker::generatePerVertexUVsForNode(builder, ni, glm::mat4(1.0f), /*paddingPx=*/4, /*resolution=*/0);
              perNodeResults[ni] = res;

              // Build mesh entries for each primitive associated with this node
              auto it = builder.nodePrimitiveIndices.find(ni);
              if (it != builder.nodePrimitiveIndices.end())
              {
                for (int primIdx : it->second)
                {
                  nlohmann::json meshEntry;
                  meshEntry["primitiveIndex"] = primIdx;
                  meshEntry["uvChannel"]      = 1;
                  meshEntry["uvScale"]        = {res.atlasResult.uvScale.x, res.atlasResult.uvScale.y};
                  meshEntry["uvOffset"]       = {res.atlasResult.uvOffset.x, res.atlasResult.uvOffset.y};
                  perModelMeshEntries[modelPath][ni].push_back(meshEntry);
                }
              }
            }

            precomputedPerModel[modelPath] = std::move(perNodeResults);
          }
          catch (const std::exception& e)
          {
            std::cerr << "UV generation failed for model " << modelPath << ": " << e.what() << "\n";
          }
        }

        // For each scene object, copy per-model mesh entries into the object's bindings
        for (const auto& obj : scene.objects)
        {
          if (!obj.modelPath) continue;
          auto it = perModelMeshEntries.find(*obj.modelPath);
          if (it == perModelMeshEntries.end())
          {
            // fallback
            nlohmann::json meshEntry;
            meshEntry["primitiveIndex"] = 0;
            meshEntry["uvChannel"]      = 1;
            meshEntry["uvScale"]        = {1.0, 1.0};
            meshEntry["uvOffset"]       = {0.0, 0.0};
            sceneLightmapBindings[obj.id]["meshes"].push_back(meshEntry);
          }
          else
          {
            for (auto& kv : it->second)
              for (const auto& me : kv.second)
                sceneLightmapBindings[obj.id]["meshes"].push_back(me);
          }
        }

        // Move precomputed results into loader-accessible storage
        precomputedPerModelUVs = std::move(precomputedPerModel);
      }

      auto tris = LightmapBaker::collectTrianglesFromScene(scene, loader);
      if (tris.empty())
      {
        std::cerr << "No triangles extracted from scene\n";
        return 1;
      }

      // We'll bake per-model-local primitive lightmaps with deduplication using a canonical key
      struct LightmapInfo
      {
        std::string        id;
        std::string        file;
        std::string        format;
        std::array<int, 2> resolution;
        std::string        usage;
      };

      auto make_lm_id = [](const std::string& keyStr) {
        // Simple stable digest: use std::hash on canonical string and format as hex
        size_t             h = std::hash<std::string>{}(keyStr);
        std::ostringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << (h & 0xffffffffffffffffull);
        std::string s = ss.str();
        // shorten to 8 chars for brevity
        return std::string("lm_") + s.substr(0, 8);
      };

      // Map canonicalKey -> LightmapInfo
      std::unordered_map<std::string, LightmapInfo> producedLightmaps;

      // Helper to compute a canonical key for a primitive
      auto computeCanonicalKey = [&](const std::string& modelPathStr, int nodeIndex, int primIndex, const engine::Model::Builder& builder, int res) {
        std::ostringstream key;
        key << "model=" << modelPathStr << "|node=" << nodeIndex << "|prim=" << primIndex << "|res=" << res;

        // Attempt to include UV atlas params (if precomputed) so different UV layouts don't collide
        int    uvChannel = 1;
        double uvScaleX = 1.0, uvScaleY = 1.0, uvOffsetX = 0.0, uvOffsetY = 0.0;
        auto   itUV = precomputedPerModelUVs.find(modelPathStr);
        if (itUV != precomputedPerModelUVs.end())
        {
          const auto& perNode = itUV->second;
          if (nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < perNode.size())
          {
            uvScaleX  = perNode[nodeIndex].atlasResult.uvScale.x;
            uvScaleY  = perNode[nodeIndex].atlasResult.uvScale.y;
            uvOffsetX = perNode[nodeIndex].atlasResult.uvOffset.x;
            uvOffsetY = perNode[nodeIndex].atlasResult.uvOffset.y;
          }
        }

        // Quantize UVs into the key deterministically
        long long qUvScaleX  = static_cast<long long>(std::llround(uvScaleX * 1000000.0));
        long long qUvScaleY  = static_cast<long long>(std::llround(uvScaleY * 1000000.0));
        long long qUvOffsetX = static_cast<long long>(std::llround(uvOffsetX * 1000000.0));
        long long qUvOffsetY = static_cast<long long>(std::llround(uvOffsetY * 1000000.0));

        key << "|uvc=" << uvChannel << "|uvs=" << qUvScaleX << "," << qUvScaleY << "|uvo=" << qUvOffsetX << "," << qUvOffsetY;

        // include primitive vertex count and a simple quantized position checksum
        std::string k = std::to_string(nodeIndex) + "_" + std::to_string(primIndex);
        if (builder.primitiveVertexOffsets.contains(k))
        {
          uint32_t start = builder.primitiveVertexOffsets.at(k);
          uint32_t count = builder.primitiveVertexCounts.at(k);
          key << "|vcount=" << count;
          // simple checksum: sum of quantized positions
          long long checksum = 0;
          for (uint32_t vi = start; vi < start + count; ++vi)
          {
            auto p = builder.vertices[vi].position;
            checksum += static_cast<long long>(std::llround(p.x * 1000000.0));
            checksum += static_cast<long long>(std::llround(p.y * 1000000.0)) * 7;
            checksum += static_cast<long long>(std::llround(p.z * 1000000.0)) * 13;
          }
          key << "|psum=" << checksum;
        }
        return key.str();
      };

      // For each unique model in the scene, load model and iterate nodes/primitives
      std::unordered_set<std::string> uniqueModels;
      for (const auto& obj : scene.objects)
        if (obj.modelPath) uniqueModels.insert(*obj.modelPath);

      for (const auto& modelPathStr : uniqueModels)
      {
        engine::Model::Builder builder;
        if (!loader(modelPathStr, builder))
        {
          std::cerr << "Warning: failed to load model for baking: " << modelPathStr << "\n";
          continue;
        }

        for (int ni = 0; ni < static_cast<int>(builder.nodes.size()); ++ni)
        {
          auto itPrims = builder.nodePrimitiveIndices.find(ni);
          if (itPrims == builder.nodePrimitiveIndices.end()) continue;

          for (int primIdx : itPrims->second)
          {
            int         bakeRes   = resolution;
            std::string canonical = computeCanonicalKey(modelPathStr, ni, primIdx, builder, bakeRes);
            std::string lmId;
            if (producedLightmaps.contains(canonical))
            {
              lmId = producedLightmaps[canonical].id;
            }
            else
            {
              // collect triangles for this node+primitive in local space
              LightmapBaker::ExtractOptions opts;
              opts.nodeIndex      = ni;
              opts.primitiveIndex = primIdx;

              auto trisPrim = LightmapBaker::collectTrianglesFromBuilder(builder, glm::mat4(1.0f), opts);
              if (trisPrim.empty()) continue;

              auto resPrim = LightBaker::bakeTriangles(trisPrim, bakeRes, sunIntensity);

              // Save EXR and pack to VTEX
              std::filesystem::path lmOutDir = std::filesystem::path(outDir) / "lightmaps" / std::filesystem::path(scenePath).stem();
              std::filesystem::create_directories(lmOutDir);
              std::string modelStem = std::filesystem::path(modelPathStr).stem().string();

              std::filesystem::path exrPath = lmOutDir / (std::string("lm_tmp_") + modelStem + "_n" + std::to_string(ni) + "_p" + std::to_string(primIdx) + ".exr");
              const char*           err     = nullptr;
              int                   r       = SaveEXR(resPrim.hdrPixels.data(), resPrim.width, resPrim.height, 3, 0, exrPath.string().c_str(), &err);
              if (r != TINYEXR_SUCCESS)
              {
                std::cerr << "SaveEXR error: " << ((err != nullptr) ? err : "unknown") << "\n";
                continue;
              }

              lmId                           = make_lm_id(canonical);
              std::filesystem::path vtexPath = lmOutDir / (lmId + std::string("_") + modelStem + "_n" + std::to_string(ni) + "_p" + std::to_string(primIdx) + ".vtex");
              std::string           perr;
              if (!LightmapBaker::exrToVtex(exrPath.string(), vtexPath.string(), "r32", &perr))
              {
                std::cerr << "Failed to pack VTEX: " << perr << "\n";
                continue;
              }

              LightmapInfo info;
              info.id         = lmId;
              info.file       = (std::filesystem::path("lightmaps") / std::filesystem::path(scenePath).stem() / vtexPath.filename()).string();
              info.format     = "vtex";
              info.resolution = {resPrim.width, resPrim.height};
              info.usage      = "Lightmap";

              producedLightmaps[canonical] = info;
              std::cout << "Wrote VTEX: " << vtexPath << " -> id=" << lmId << "\n";

              // Cleanup temporary EXR
              std::filesystem::remove(exrPath);
            }

            // Assign binding for every instance of this model in the scene
            for (const auto& obj : scene.objects)
            {
              if (!obj.modelPath || *obj.modelPath != modelPathStr) continue;
              // find matching mesh entry in sceneLightmapBindings and annotate with lightmap id and resolution
              if (!sceneLightmapBindings.contains(obj.id))
              {
                // create if missing
                nlohmann::json meshEntry;
                meshEntry["primitiveIndex"] = primIdx;
                meshEntry["lightmapId"]     = lmId;
                meshEntry["lightmap"]       = producedLightmaps[canonical].file;
                meshEntry["uvChannel"]      = 1;
                meshEntry["resolution"]     = {producedLightmaps[canonical].resolution[0], producedLightmaps[canonical].resolution[1]};
                sceneLightmapBindings[obj.id]["meshes"].push_back(meshEntry);
              }
              else
              {
                // try to find a matching mesh entry by primitiveIndex
                bool found = false;
                for (auto& me : sceneLightmapBindings[obj.id]["meshes"])
                {
                  if (me.contains("primitiveIndex") && me["primitiveIndex"].get<int>() == primIdx)
                  {
                    me["lightmapId"] = lmId;
                    me["lightmap"]   = producedLightmaps[canonical].file;
                    me["resolution"] = {producedLightmaps[canonical].resolution[0], producedLightmaps[canonical].resolution[1]};
                    found            = true;
                    break;
                  }
                }
                if (!found)
                {
                  nlohmann::json meshEntry;
                  meshEntry["primitiveIndex"] = primIdx;
                  meshEntry["lightmapId"]     = lmId;
                  meshEntry["lightmap"]       = producedLightmaps[canonical].file;
                  meshEntry["uvChannel"]      = 1;
                  meshEntry["resolution"]     = {producedLightmaps[canonical].resolution[0], producedLightmaps[canonical].resolution[1]};
                  sceneLightmapBindings[obj.id]["meshes"].push_back(meshEntry);
                }
              }
            }
          }
        }
      }

      // Write scene-level manifest
      nlohmann::json sceneLm;
      sceneLm["version"]          = 1;
      sceneLm["lightmapBindings"] = sceneLightmapBindings;
      sceneLm["lightmaps"]        = nlohmann::json::array();
      for (const auto& kv : producedLightmaps)
      {
        const auto& info = kv.second;
        sceneLm["lightmaps"].push_back({{"id", info.id}, {"file", info.file}, {"format", info.format}, {"resolution", {info.resolution[0], info.resolution[1]}}, {"usage", info.usage}});
      }

      std::filesystem::path sceneStem = std::filesystem::path(scenePath).stem();
      std::ofstream         sm((std::filesystem::path("assets/scenes") / (sceneStem.string() + std::string("_lightmaps.json"))).string());
      sm << sceneLm.dump(2);
      std::cout << "Wrote scene lightmap manifest: " << (std::filesystem::path("assets/scenes") / (sceneStem.string() + std::string("_lightmaps.json"))).string() << "\n";
      // Also write a lightweight manifest into the provided outDir for CLI consumers/tests
      try
      {
        std::filesystem::create_directories(outDir);
        std::filesystem::path outManifest = std::filesystem::path(outDir) / (sceneStem.string() + std::string("_lightmap.json"));
        std::ofstream         om(outManifest);
        om << sceneLm.dump(2);
        om.close();

        std::filesystem::path bakeManifest = std::filesystem::path(outDir) / std::string("scene_bake_manifest.json");
        nlohmann::json        bakeSummary;
        bakeSummary["lightmaps"]        = sceneLm["lightmaps"];
        bakeSummary["lightmapBindings"] = sceneLm["lightmapBindings"];
        om                              = std::ofstream(bakeManifest);
        om << bakeSummary.dump(2);
        om.close();
        std::cout << "Wrote outdir manifests: " << outManifest << " and " << bakeManifest << "\n";
      }
      catch (const std::exception& e)
      {
        std::cerr << "Failed to write outdir manifests: " << e.what() << "\n";
      }
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

      if (packToVtex)
      {
        std::filesystem::path vtexPath = exrPath;
        vtexPath.replace_extension(".vtex");
        std::string perr;
        if (!LightmapBaker::exrToVtex(exrPath.string(), vtexPath.string(), "r32", &perr))
        {
          std::cerr << "Failed to pack VTEX: " << perr << "\n";
          return 1;
        }
        std::cout << "Wrote VTEX: " << vtexPath << "\n";
      }

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
