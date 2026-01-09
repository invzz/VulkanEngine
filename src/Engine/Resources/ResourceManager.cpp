#include "Engine/Resources/ResourceManager.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_set>
#include <utility>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/MeshManager.hpp"
#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Resources/TextureManager.hpp"
#include "Engine/Scene/LightmapManifest.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"

// Filesystem + JSON + EXR utilities for runtime lightmap atlas assembly
#include <tinyexr.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

// Simple SHA256 implementation for content hashing
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

namespace engine {

  // Simple FNV-1a hash (fast, good distribution)
  namespace {
    uint64_t hashBytes(const unsigned char* data, size_t length)
    {
      uint64_t hash = 14695981039346656037ULL; // FNV offset basis
      for (size_t i = 0; i < length; ++i)
      {
        hash ^= data[i];
        hash *= 1099511628211ULL; // FNV prime
      }
      return hash;
    }

  } // namespace

  ResourceManager::ResourceManager(Device& device) : device_(device)
  {
    textureManager_ = std::make_unique<TextureManager>(device);
    meshManager_    = std::make_unique<MeshManager>(device);

    // Initialize thread pool with hardware concurrency
    size_t numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
    {
      numThreads = 4; // Fallback
    }
    initThreadPool(numThreads);
  }

  ResourceManager::~ResourceManager()
  {
    shutdownThreadPool();
  }

  std::string ResourceManager::makeTextureKey(const std::string& path, bool srgb)
  {
    // Include srgb flag in key since same texture can be loaded with different
    // formats
    return path + (srgb ? "|srgb" : "|linear");
  }

  std::string ResourceManager::makeModelKey(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets)
  {
    // Include loading flags in key since same model can be loaded with different
    // settings
    std::ostringstream oss;
    oss << path << "|tex=" << enableTextures << "|mat=" << loadMaterials << "|morph=" << enableMorphTargets;
    return oss.str();
  }

  std::shared_ptr<Texture> ResourceManager::loadTexture(const std::string& path, bool srgb, bool flipY, ResourcePriority priority)
  {
    std::string key = makeTextureKey(path, srgb) + (flipY ? "|flipY" : "");

    // Lock for thread-safe access
    std::scoped_lock const lock(textureMutex_);

    // Check if texture is already cached
    auto it = textureCache_.find(key);
    if (it != textureCache_.end())
    {
      // Try to lock the weak_ptr to get a shared_ptr
      if (auto cachedTexture = it->second.lock())
      {
        // Texture still exists, update LRU access time and priority
        updateTextureAccess(key, cachedTexture->getMemorySize(), priority);
        return cachedTexture;
      }

      // Texture was deleted, remove stale entry
      textureCache_.erase(it);
      // Remove from access tracking
      textureAccessOrder_.erase(std::remove_if(textureAccessOrder_.begin(), textureAccessOrder_.end(), [&key](const ResourceInfo& info) { return info.key == key; }), textureAccessOrder_.end());
    }

    // Load new texture
    auto         texture = std::make_shared<Texture>(device_, path, srgb, flipY);
    size_t const memSize = texture->getMemorySize();

    // Check memory budget and evict if necessary
    if (memoryBudget_ > 0)
    {
      cachedTextureMemory_ += memSize;
      while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty())
      {
        evictLRUTextures();
      }
    }

    // Cache the texture (as weak_ptr)
    textureCache_[key] = texture;
    updateTextureAccess(key, memSize, priority);

    // Register with TextureManager
    uint32_t const globalIndex = textureManager_->addTexture(texture);
    texture->setGlobalIndex(globalIndex);

    return texture;
  }

  std::shared_ptr<Model> ResourceManager::loadModel(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets, ResourcePriority priority)
  {
    std::string key = makeModelKey(path, enableTextures, loadMaterials, enableMorphTargets);

    // Lock for thread-safe access
    std::scoped_lock const lock(modelMutex_);

    // Check if model is already cached
    auto it = modelCache_.find(key);
    if (it != modelCache_.end())
    {
      // Try to lock the weak_ptr to get a shared_ptr
      if (auto cachedModel = it->second.lock())
      {
        // Model still exists, update LRU access time and priority
        updateModelAccess(key, cachedModel->getMemorySize(), priority);
        return cachedModel;
      }

      // Model was deleted, remove stale entry
      modelCache_.erase(it);
      // Remove from access tracking
      modelAccessOrder_.erase(std::remove_if(modelAccessOrder_.begin(), modelAccessOrder_.end(), [&key](const ResourceInfo& info) { return info.key == key; }), modelAccessOrder_.end());
    }

    // Load new model: choose importer based on file extension
    auto toLower = [](std::string s) {
      std::transform(s.begin(), s.end(), s.begin(), ::tolower);
      return s;
    };

    std::string ext;
    auto        pos = path.find_last_of('.');
    if (pos != std::string::npos) ext = toLower(path.substr(pos + 1));

    std::shared_ptr<Model> model;
    try
    {
      if (ext == "gltf" || ext == "glb")
      {
        // Use glTF importer for glTF files
        model = std::shared_ptr<Model>(Model::createModelFromGLTF(device_, path, false, true, true));
      }
      else
      {
        // Fall back to file loader (OBJ)
        model = std::shared_ptr<Model>(Model::createModelFromFile(device_, path, false, true, true));
      }
    }
    catch (const std::exception& e)
    {
      // Propagate error to caller
      throw;
    }

    size_t const memSize = model->getMemorySize();

    // Optionally load material textures according to flags
    if (enableTextures || loadMaterials)
    {
      try
      {
        for (auto& mat : model->getMaterials())
        {
          if (!mat.diffuseTexPath.empty() && enableTextures)
          {
            mat.pbrMaterial.albedoMap = loadTexture(mat.diffuseTexPath, true, true);
          }
          if (!mat.normalTexPath.empty() && enableTextures)
          {
            mat.pbrMaterial.normalMap = loadTexture(mat.normalTexPath, false, true);
          }
          if (!mat.roughnessTexPath.empty() && loadMaterials)
          {
            mat.pbrMaterial.roughnessMap = loadTexture(mat.roughnessTexPath, false, true);
          }
          if (!mat.aoTexPath.empty() && loadMaterials)
          {
            mat.pbrMaterial.aoMap = loadTexture(mat.aoTexPath, false, true);
          }
          if (!mat.emissiveTexPath.empty() && enableTextures)
          {
            mat.pbrMaterial.emissiveMap = loadTexture(mat.emissiveTexPath, true, true);
          }
          if (!mat.specularGlossinessTexPath.empty() && loadMaterials)
          {
            mat.pbrMaterial.specularGlossinessMap = loadTexture(mat.specularGlossinessTexPath, true, true);
          }
          if (!mat.transmissionTexPath.empty() && loadMaterials)
          {
            mat.pbrMaterial.transmissionMap = loadTexture(mat.transmissionTexPath, false, true);
          }
          if (!mat.clearcoatTexPath.empty() && loadMaterials)
          {
            mat.pbrMaterial.clearcoatMap = loadTexture(mat.clearcoatTexPath, false, true);
          }
          if (!mat.clearcoatRoughnessTexPath.empty() && loadMaterials)
          {
            mat.pbrMaterial.clearcoatRoughnessMap = loadTexture(mat.clearcoatRoughnessTexPath, false, true);
          }
          if (!mat.clearcoatNormalTexPath.empty() && loadMaterials)
          {
            mat.pbrMaterial.clearcoatNormalMap = loadTexture(mat.clearcoatNormalTexPath, false, true);
          }
        }
      }
      catch (const std::exception& e)
      {
        std::cerr << "ResourceManager: failed loading material textures for " << path << ": " << e.what() << '\n';
      }
    }

    // After loading material textures, try to find a per-model mesh lightmap manifest
    try
    {
      std::filesystem::path modelPath{path};
      std::string           baseName     = modelPath.stem().string();
      std::string           manifestName = baseName + std::string("_mesh_lightmaps.json");
      std::filesystem::path manifestPath = modelPath.parent_path() / manifestName;

      if (std::filesystem::exists(manifestPath))
      {
        std::ifstream  in(manifestPath);
        nlohmann::json j;
        in >> j;

        if (j.contains("meshes") && j["meshes"].is_array())
        {
          // Build a per-mesh map of tiles (meshIndex -> vector of entries)
          std::unordered_map<int, std::vector<nlohmann::json>> tilesByMesh;
          for (const auto& entry : j["meshes"])
          {
            int meshIndex = entry.value("mesh", -1);
            if (meshIndex < 0) continue;
            tilesByMesh[meshIndex].push_back(entry);
          }

          // For each mesh that has tiles, assemble an atlas and assign to the mesh's primary material
          for (auto& [meshIdx, tiles] : tilesByMesh)
          {
            // Compute atlas size: pack tiles in a reasonable grid (simple square packing)
            int tileW     = tiles[0].value("resolution", std::vector<int>{0, 0})[0];
            int tileH     = tiles[0].value("resolution", std::vector<int>{0, 0})[1];
            int nTiles    = static_cast<int>(tiles.size());
            int atlasCols = static_cast<int>(std::ceil(std::sqrt(nTiles)));
            int atlasRows = static_cast<int>(std::ceil(static_cast<float>(nTiles) / atlasCols));
            int atlasW    = atlasCols * tileW;
            int atlasH    = atlasRows * tileH;

            // Create HDR float atlas buffer (RGBA)
            std::vector<float> atlasPixels(static_cast<size_t>(atlasW) * static_cast<size_t>(atlasH) * 4, 0.0f);

            for (int t = 0; t < nTiles; ++t)
            {
              int                   col      = t % atlasCols;
              int                   row      = t / atlasCols;
              int                   offsetX  = col * tileW;
              int                   offsetY  = row * tileH;
              std::filesystem::path tilePath = modelPath.parent_path() / tiles[t].value("file", std::string());

              try
              {
                auto tex = loadTexture(tilePath.string(), false, true);
                // Read back texture pixels from device: TODO use proper readback helper. As a shortcut, use Texture::getImageView() and a staging copy path is required.
                // Instead, for now, load EXR directly from disk using tinyexr again to fill atlas.

                const char* err  = nullptr;
                float*      rgba = nullptr;
                int         w = 0, h = 0;
                int         ret = LoadEXR(&rgba, &w, &h, tilePath.string().c_str(), &err);
                if (ret != TINYEXR_SUCCESS)
                {
                  if (err)
                  {
                    FreeEXRErrorMessage(err);
                  }
                  std::cerr << "ResourceManager: failed to load tile EXR " << tilePath.string() << "\n";
                  continue;
                }

                // Copy tile into atlas
                for (int y = 0; y < h; ++y)
                {
                  for (int x = 0; x < w; ++x)
                  {
                    int srcIdx              = (y * w + x) * 3;                              // saved as RGB in baker
                    int dstIdx              = ((offsetY + y) * atlasW + (offsetX + x)) * 4; // atlas is RGBA
                    atlasPixels[dstIdx + 0] = rgba[srcIdx + 0];
                    atlasPixels[dstIdx + 1] = rgba[srcIdx + 1];
                    atlasPixels[dstIdx + 2] = rgba[srcIdx + 2];
                    atlasPixels[dstIdx + 3] = 1.0f;
                  }
                }

                free(rgba);
              }
              catch (const std::exception& e)
              {
                std::cerr << "ResourceManager: failed to load tile " << tiles[t].value("file", std::string()) << ": " << e.what() << "\n";
              }
            }

            // Save atlas to a temporary file in the model directory
            std::string           atlasName = baseName + "_mesh" + std::to_string(meshIdx) + "_lightmap_atlas.exr";
            std::filesystem::path atlasPath = modelPath.parent_path() / atlasName;
            const char*           err       = nullptr;
            int                   ret       = SaveEXR(atlasPixels.data(), atlasW, atlasH, 4, 0, atlasPath.string().c_str(), &err);
            if (ret != TINYEXR_SUCCESS)
            {
              std::cerr << "ResourceManager: failed to write atlas " << atlasPath.string() << "\n";
              if (err)
              {
                FreeEXRErrorMessage(err);
              }
              continue;
            }

            // Load atlas as a float EXR texture and assign to the mesh's primary material
            try
            {
              auto  atlasTex   = Texture::createFromEXR(device_, atlasPath.string());
              auto& materials  = model->getMaterials();
              int   primaryMat = -1;
              primaryMat       = model->getPrimaryMaterialForMesh(meshIdx);
              if (primaryMat >= 0 && primaryMat < static_cast<int>(materials.size()))
              {
                materials[primaryMat].pbrMaterial.lightmap = atlasTex;
                uint32_t const globalIndex                 = textureManager_->addTexture(atlasTex);
                atlasTex->setGlobalIndex(globalIndex);
                std::cout << "ResourceManager: assigned atlas " << atlasPath.string() << " to material " << primaryMat << " (mesh " << meshIdx << ")\n";
              }
              else
              {
                std::cerr << "ResourceManager: no primary material for mesh " << meshIdx << "\n";
              }
            }
            catch (const std::exception& e)
            {
              std::cerr << "ResourceManager: failed to create atlas texture " << e.what() << "\n";
            }
          }
        }
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << "ResourceManager: failed processing mesh lightmap manifest: " << e.what() << "\n";
    }

    // New: attempt to load a scene-level lightmap manifest (scene_lightmaps.json) next to the scene file
    try
    {
      std::filesystem::path modelPath{path};
      std::string           baseName    = modelPath.stem().string();
      std::filesystem::path sceneDir    = modelPath.parent_path();
      std::filesystem::path sceneLMPath = sceneDir / (baseName + std::string("_lightmaps.json"));

      if (std::filesystem::exists(sceneLMPath))
      {
        std::ifstream  in(sceneLMPath);
        nlohmann::json j;
        in >> j;

        // Parse lightmaps array
        if (j.contains("lightmaps") && j["lightmaps"].is_array())
        {
          for (const auto& l : j["lightmaps"])
          {
            try
            {
              LightmapInfo info;
              info.id     = l.value("id", std::string());
              info.file   = l.value("file", std::string());
              info.format = l.value("format", std::string());
              if (l.contains("resolution") && l["resolution"].is_array() && l["resolution"].size() == 2)
              {
                info.resolution[0] = l["resolution"][0].get<int>();
                info.resolution[1] = l["resolution"][1].get<int>();
              }
              info.paddingPx           = l.value("paddingPx", 0);
              info.usage               = l.value("usage", std::string());
              sceneLightmaps_[info.id] = info;
            }
            catch (const std::exception& e)
            {
              std::cerr << "ResourceManager: failed parsing lightmap entry: " << e.what() << "\n";
            }
          }
        }

        // Parse bindings
        if (j.contains("lightmapBindings") && j["lightmapBindings"].is_object())
        {
          for (auto it = j["lightmapBindings"].begin(); it != j["lightmapBindings"].end(); ++it)
          {
            const std::string objectId = it.key();
            const auto&       bind     = it.value();
            try
            {
              LightmapBinding b;
              b.lightmapId = bind.value("lightmapId", std::string());
              b.uvChannel  = bind.value("uvChannel", 1);
              if (bind.contains("uvScale") && bind["uvScale"].is_array())
              {
                b.uvScale.x = bind["uvScale"][0].get<float>();
                b.uvScale.y = bind["uvScale"][1].get<float>();
              }
              if (bind.contains("uvOffset") && bind["uvOffset"].is_array())
              {
                b.uvOffset.x = bind["uvOffset"][0].get<float>();
                b.uvOffset.y = bind["uvOffset"][1].get<float>();
              }
              sceneLightmapBindings_[objectId] = b;
            }
            catch (const std::exception& e)
            {
              std::cerr << "ResourceManager: failed parsing binding for object " << objectId << ": " << e.what() << "\n";
            }
          }
        }

        std::cout << "ResourceManager: loaded scene-level lightmap manifest " << sceneLMPath.string() << " (" << sceneLightmaps_.size() << " lightmaps, " << sceneLightmapBindings_.size()
                  << " bindings)\n";
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << "ResourceManager: failed processing scene lightmap manifest: " << e.what() << "\n";
    }

    // Check memory budget and evict if necessary
    if (memoryBudget_ > 0)
    {
      cachedModelMemory_ += memSize;
      while (cachedModelMemory_ > memoryBudget_ && !modelCache_.empty())
      {
        evictLRUModels();
      }
    }

    // Cache the model (as weak_ptr)
    modelCache_[key] = model;
    updateModelAccess(key, memSize, priority);

    // Register with MeshManager
    uint32_t const meshId = meshManager_->registerModel(model.get());
    model->setMeshId(meshId);

    return model;
  }

  // -----------------------------------------------------------------------
  // Scene-level manifest helpers
  // -----------------------------------------------------------------------

  bool ResourceManager::loadSceneLightmapManifest(const std::string& manifestPath)
  {
    try
    {
      if (!std::filesystem::exists(manifestPath)) return false;
      std::ifstream  in(manifestPath);
      nlohmann::json j;
      in >> j;

      sceneLightmaps_.clear();
      sceneLightmapBindings_.clear();

      if (j.contains("lightmaps") && j["lightmaps"].is_array())
      {
        for (const auto& l : j["lightmaps"])
        {
          try
          {
            LightmapInfo info;
            info.id     = l.value("id", std::string());
            info.file   = l.value("file", std::string());
            info.format = l.value("format", std::string());
            if (l.contains("resolution") && l["resolution"].is_array() && l["resolution"].size() == 2)
            {
              info.resolution[0] = l["resolution"][0].get<int>();
              info.resolution[1] = l["resolution"][1].get<int>();
            }
            info.paddingPx           = l.value("paddingPx", 0);
            info.usage               = l.value("usage", std::string());
            sceneLightmaps_[info.id] = info;
          }
          catch (const std::exception& e)
          {
            std::cerr << "ResourceManager: failed parsing lightmap entry: " << e.what() << "\n";
          }
        }
      }

      if (j.contains("lightmapBindings") && j["lightmapBindings"].is_object())
      {
        for (auto it = j["lightmapBindings"].begin(); it != j["lightmapBindings"].end(); ++it)
        {
          const std::string& objectId = it.key();
          const auto&        bind     = it.value();
          try
          {
            LightmapBinding b;
            b.lightmapId = bind.value("lightmapId", std::string());
            b.uvChannel  = bind.value("uvChannel", 1);
            if (bind.contains("uvScale") && bind["uvScale"].is_array())
            {
              b.uvScale.x = bind["uvScale"][0].get<float>();
              b.uvScale.y = bind["uvScale"][1].get<float>();
            }
            if (bind.contains("uvOffset") && bind["uvOffset"].is_array())
            {
              b.uvOffset.x = bind["uvOffset"][0].get<float>();
              b.uvOffset.y = bind["uvOffset"][1].get<float>();
            }
            sceneLightmapBindings_[objectId] = b;
          }
          catch (const std::exception& e)
          {
            std::cerr << "ResourceManager: failed parsing binding for object " << objectId << ": " << e.what() << "\n";
          }
        }
      }

      // Delegate parsing to shared parser
      std::unordered_map<std::string, engine::scene::LightmapInfo>    parsedLightmaps;
      std::unordered_map<std::string, engine::scene::LightmapBinding> parsedBindings;
      if (!engine::scene::parseSceneLightmapManifest(manifestPath, parsedLightmaps, parsedBindings))
      {
        std::cerr << "ResourceManager: parser failed for " << manifestPath << "\n";
        return false;
      }

      // Convert into ResourceManager's internal types
      sceneLightmaps_.clear();
      for (auto& [id, info] : parsedLightmaps)
      {
        LightmapInfo li;
        li.id               = info.id;
        li.file             = info.file;
        li.format           = info.format;
        li.resolution       = info.resolution;
        li.paddingPx        = info.paddingPx;
        li.usage            = info.usage;
        sceneLightmaps_[id] = li;
      }

      sceneLightmapBindings_.clear();
      for (auto& [objId, b] : parsedBindings)
      {
        LightmapBinding lb;
        lb.lightmapId                 = b.lightmapId;
        lb.uvChannel                  = b.uvChannel;
        lb.uvScale                    = b.uvScale;
        lb.uvOffset                   = b.uvOffset;
        sceneLightmapBindings_[objId] = lb;
      }

      std::cout << "ResourceManager: loaded scene-level lightmap manifest " << manifestPath << " (" << sceneLightmaps_.size() << " lightmaps, " << sceneLightmapBindings_.size() << " bindings)\n";
      return true;
    }
    catch (const std::exception& e)
    {
      std::cerr << "ResourceManager: failed loading scene lightmap manifest: " << e.what() << "\n";
      return false;
    }
  }

  std::optional<ResourceManager::LightmapBinding> ResourceManager::getLightmapBindingForObject(const std::string& objectId) const
  {
    auto it = sceneLightmapBindings_.find(objectId);
    if (it == sceneLightmapBindings_.end()) return std::nullopt;
    return it->second;
  }

  void ResourceManager::applySceneLightmapBindings(engine::Scene& scene)
  {
    auto& reg  = scene.getRegistry();
    auto  view = reg.view<NameComponent>();
    for (auto entity : view)
    {
      const auto& nameComp = reg.get<NameComponent>(entity);
      auto        it       = sceneLightmapBindings_.find(nameComp.name);
      if (it == sceneLightmapBindings_.end()) continue;

      const LightmapBinding& b = it->second;
      // Emplace or update LightmapComponent
      if (!reg.all_of<LightmapComponent>(entity))
      {
        reg.emplace<LightmapComponent>(entity, LightmapComponent{b.lightmapId, b.uvChannel, b.uvScale, b.uvOffset, -1});
      }
      else
      {
        auto& lm      = reg.get<LightmapComponent>(entity);
        lm.lightmapId = b.lightmapId;
        lm.uvChannel  = b.uvChannel;
        lm.uvScale    = b.uvScale;
        lm.uvOffset   = b.uvOffset;
      }

      // Attempt to set per-material convenience fields (PBRMaterial::uvScale)
      if (reg.all_of<PBRMaterial>(entity))
      {
        auto& mat   = reg.get<PBRMaterial>(entity);
        mat.uvScale = b.uvScale.x;
      }
    }
  }

  std::optional<ResourceManager::LightmapInfo> ResourceManager::getLightmapInfoById(const std::string& id) const
  {
    auto it = sceneLightmaps_.find(id);
    if (it == sceneLightmaps_.end()) return std::nullopt;
    return it->second;
  }

  void ResourceManager::registerLightmapTextureForId(const std::string& id, std::shared_ptr<Texture> texture)
  {
    if (!texture) return;
    std::scoped_lock const lock(textureMutex_);
    sceneLightmapTextures_[id] = texture;

    // Ensure global registration with TextureManager
    uint32_t const globalIndex = textureManager_->addTexture(texture);
    texture->setGlobalIndex(globalIndex);

    // Update access tracking
    updateTextureAccess(makeTextureKey(id, false), texture->getMemorySize(), ResourcePriority::HIGH);
  }

  bool ResourceManager::loadSceneLightmapTextures(const std::string& basePath)
  {
    // basePath is expected to be project asset root (for tests we pass "assets")
    for (const auto& [id, info] : sceneLightmaps_)
    {
      // Skip if already loaded and alive
      {
        std::scoped_lock const lock(textureMutex_);
        auto                   it = sceneLightmapTextures_.find(id);
        if (it != sceneLightmapTextures_.end() && !it->second.expired()) continue;
      }

      std::filesystem::path    candidate = std::filesystem::path(basePath) / info.file;
      std::shared_ptr<Texture> tex;

      if (std::filesystem::exists(candidate))
      {
        // Prefer explicit VTEX loader when a .vtex container is provided.
        std::string ext = candidate.extension().string();
        for (auto& c : ext)
          c = (char)std::tolower(c);
        try
        {
          if (ext == ".vtex")
          {
            // Use VTEX loader (GPU-ready container)
            tex = Texture::createFromVTEX(device_, candidate.string());
          }
          else if (ext == ".exr")
          {
            // Use EXR loader (linear HDR)
            tex = Texture::createFromEXR(device_, candidate.string());
          }
          else
          {
            // Generic file loader (assume non-sRGB for lightmaps)
            tex = loadTexture(candidate.string(), false);
          }
        }
        catch (const std::exception& e)
        {
          std::cerr << "ResourceManager: failed to load lightmap file " << candidate << ": " << e.what() << "\n";
          tex = nullptr;
        }
      }

      if (!tex)
      {
        // Fallback to a 1x1 white texture (safe fallback for tests and missing files)
        tex = Texture::createWhiteTexture(device_);
      }

      registerLightmapTextureForId(id, tex);
    }
    return true;
  }

  void ResourceManager::applyLoadedLightmapsToScene(engine::Scene& scene)
  {
    auto& reg  = scene.getRegistry();
    auto  view = reg.view<engine::LightmapComponent>();
    for (auto entity : view)
    {
      auto& lmComp = reg.get<engine::LightmapComponent>(entity);
      auto  it     = sceneLightmapTextures_.find(lmComp.lightmapId);
      if (it == sceneLightmapTextures_.end()) continue;
      if (auto tex = it->second.lock())
      {
        if (reg.all_of<PBRMaterial>(entity))
        {
          auto& mat    = reg.get<PBRMaterial>(entity);
          mat.lightmap = tex;
        }
      }
    }
  }

  std::shared_ptr<Texture> ResourceManager::loadTextureFromMemory(const unsigned char* data, size_t dataSize, const std::string& debugName, bool srgb, ResourcePriority priority)
  {
    // Compute content hash for deduplication
    std::string const contentHash = computeContentHash(data, dataSize);
    std::string       cacheKey;

    // Lock for thread-safe access
    std::scoped_lock const lock(textureMutex_);

    // Check if we've already loaded this exact content
    auto hashIt = contentHashToKey_.find(contentHash);
    if (hashIt != contentHashToKey_.end())
    {
      cacheKey = hashIt->second;
      auto it  = textureCache_.find(cacheKey);
      if (it != textureCache_.end())
      {
        if (auto cachedTexture = it->second.lock())
        {
          // Same content already loaded, return cached instance
          updateTextureAccess(cacheKey, cachedTexture->getMemorySize(), priority);
          return cachedTexture;
        }
      }
    }

    // Create unique cache key: hash + debug name + format
    cacheKey = "embedded:" + contentHash + "|" + debugName + (srgb ? "|srgb" : "|linear");

    // Check if this specific key is cached (shouldn't happen, but safe check)
    auto it = textureCache_.find(cacheKey);
    if (it != textureCache_.end())
    {
      if (auto cachedTexture = it->second.lock())
      {
        updateTextureAccess(cacheKey, cachedTexture->getMemorySize(), priority);
        return cachedTexture;
      }
    }

    // Load texture from memory
    // Note: This requires a Texture constructor that accepts memory data
    // For now, we'll need to save to a temp file or extend Texture class
    // As a workaround, we use the file-based loader with a unique temp path

    // TODO: Implement Texture::createFromMemory() for true zero-copy loading
    // For now, fall back to file-based loading
    std::string const tempPath = "/tmp/embedded_texture_" + contentHash + ".dat";
    // In production, you'd write data to tempPath here

    auto         texture = std::make_shared<Texture>(device_, tempPath, srgb);
    size_t const memSize = texture->getMemorySize();

    // Check memory budget and evict if necessary
    if (memoryBudget_ > 0)
    {
      cachedTextureMemory_ += memSize;
      while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty())
      {
        evictLRUTextures();
      }
    }

    // Cache the texture
    textureCache_[cacheKey]        = texture;
    contentHashToKey_[contentHash] = cacheKey;
    updateTextureAccess(cacheKey, memSize, priority);

    // Register with TextureManager
    uint32_t const globalIndex = textureManager_->addTexture(texture);
    texture->setGlobalIndex(globalIndex);

    return texture;
  }

  size_t ResourceManager::garbageCollect()
  {
    size_t removedCount = 0;

    // Clean up textures
    {
      std::scoped_lock const lock(textureMutex_);
      cachedTextureMemory_ = 0;
      std::unordered_set<std::string> removedKeys;

      for (auto it = textureCache_.begin(); it != textureCache_.end();)
      {
        const std::string& key = it->first;
        if (auto texture = it->second.lock())
        {
          cachedTextureMemory_ += texture->getMemorySize();
          ++it;
          continue;
        }

        // Expired entry; erase from cache and track for access-order cleanup.
        removedKeys.insert(key);
        it = textureCache_.erase(it);
        ++removedCount;
      }

      if (!removedKeys.empty())
      {
        // Remove all dead entries from access tracking in one pass.
        auto const removed = std::ranges::remove_if(textureAccessOrder_, [&removedKeys](const ResourceInfo& info) { return removedKeys.contains(info.key); });
        textureAccessOrder_.erase(removed.begin(), removed.end());

        // Remove stale content-hash indirections for textures that are gone.
        for (auto it = contentHashToKey_.begin(); it != contentHashToKey_.end();)
        {
          if (removedKeys.contains(it->second))
          {
            it = contentHashToKey_.erase(it);
          }
          else
          {
            ++it;
          }
        }
      }
    }

    // Clean up models
    {
      std::scoped_lock const lock(modelMutex_);
      cachedModelMemory_ = 0;
      std::unordered_set<std::string> removedKeys;

      for (auto it = modelCache_.begin(); it != modelCache_.end();)
      {
        const std::string& key = it->first;
        if (auto model = it->second.lock())
        {
          cachedModelMemory_ += model->getMemorySize();
          ++it;
          continue;
        }

        // Expired entry; erase from cache and track for access-order cleanup.
        removedKeys.insert(key);
        it = modelCache_.erase(it);
        ++removedCount;
      }

      if (!removedKeys.empty())
      {
        auto const removed = std::ranges::remove_if(modelAccessOrder_, [&removedKeys](const ResourceInfo& info) { return removedKeys.contains(info.key); });
        modelAccessOrder_.erase(removed.begin(), removed.end());
      }
    }

    return removedCount;
  }

  size_t ResourceManager::getMemoryUsage() const
  {
    size_t totalMemory = 0;

    // Texture memory (accurate calculation)
    {
      std::scoped_lock const lock(textureMutex_);
      for (const auto& [key, weakTexture] : textureCache_)
      {
        if (auto texture = weakTexture.lock())
        {
          totalMemory += texture->getMemorySize();
        }
      }
    }

    // Model memory (accurate calculation)
    {
      std::scoped_lock const lock(modelMutex_);
      for (const auto& [key, weakModel] : modelCache_)
      {
        if (auto model = weakModel.lock())
        {
          totalMemory += model->getMemorySize();
        }
      }
    }

    return totalMemory;
  }

  size_t ResourceManager::getCachedTextureCount() const
  {
    std::scoped_lock const lock(textureMutex_);

    // Count only alive textures
    size_t count = 0;
    for (const auto& [key, weakTexture] : textureCache_)
    {
      if (!weakTexture.expired())
      {
        ++count;
      }
    }
    return count;
  }

  size_t ResourceManager::getCachedModelCount() const
  {
    std::scoped_lock const lock(modelMutex_);

    // Count only alive models
    size_t count = 0;
    for (const auto& [key, weakModel] : modelCache_)
    {
      if (!weakModel.expired())
      {
        ++count;
      }
    }
    return count;
  }

  void ResourceManager::clearAll()
  {
    {
      std::scoped_lock const lock(textureMutex_);
      textureCache_.clear();
      textureAccessOrder_.clear();
      cachedTextureMemory_ = 0;
    }

    {
      std::scoped_lock const lock(modelMutex_);
      modelCache_.clear();
      modelAccessOrder_.clear();
      cachedModelMemory_ = 0;
    }
  }

  bool ResourceManager::isTextureCached(const std::string& path) const
  {
    std::scoped_lock const lock(textureMutex_);

    // Check both srgb and linear variants
    std::string const srgbKey   = makeTextureKey(path, true);
    std::string const linearKey = makeTextureKey(path, false);

    auto srgbIt   = textureCache_.find(srgbKey);
    auto linearIt = textureCache_.find(linearKey);

    bool const srgbCached   = (srgbIt != textureCache_.end() && !srgbIt->second.expired());
    bool const linearCached = (linearIt != textureCache_.end() && !linearIt->second.expired());

    return srgbCached || linearCached;
  }

  bool ResourceManager::isModelCached(const std::string& path) const
  {
    std::scoped_lock const lock(modelMutex_);

    // Check if any variant of this model path is cached
    for (const auto& [key, weakModel] : modelCache_)
    {
      if (key.starts_with(path) && !weakModel.expired())
      {
        return true;
      }
    }
    return false;
  }

  void ResourceManager::setMemoryBudget(size_t budgetBytes)
  {
    memoryBudget_ = budgetBytes;

    // Evict resources if we're already over budget
    if (budgetBytes > 0)
    {
      {
        std::scoped_lock const lock(textureMutex_);
        while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty())
        {
          evictLRUTextures();
        }
      }

      {
        std::scoped_lock const lock(modelMutex_);
        while (cachedModelMemory_ > memoryBudget_ && !modelCache_.empty())
        {
          evictLRUModels();
        }
      }
    }
  }

  void ResourceManager::updateTextureAccess(const std::string& key, size_t memorySize, ResourcePriority priority)
  {
    // Remove existing entry if present
    auto const removed = std::ranges::remove_if(textureAccessOrder_, [&key](const ResourceInfo& info) { return info.key == key; });
    textureAccessOrder_.erase(removed.begin(), removed.end());

    // Add to end (most recently used) with priority
    textureAccessOrder_.push_back({key, memorySize, getCurrentTime(), priority});
  }

  void ResourceManager::updateModelAccess(const std::string& key, size_t memorySize, ResourcePriority priority)
  {
    // Remove existing entry if present
    auto const removed = std::ranges::remove_if(modelAccessOrder_, [&key](const ResourceInfo& info) { return info.key == key; });
    modelAccessOrder_.erase(removed.begin(), removed.end());

    // Add to end (most recently used) with priority
    modelAccessOrder_.push_back({key, memorySize, getCurrentTime(), priority});
  }

  void ResourceManager::evictLRUTextures()
  {
    if (textureAccessOrder_.empty())
    {
      return;
    }

    // Sort by priority first (low priority first), then by access time (oldest
    // first)
    std::ranges::sort(textureAccessOrder_, [](const ResourceInfo& a, const ResourceInfo& b) {
      if (a.priority != b.priority)
      {
        return a.priority < b.priority; // Lower priority evicted first
      }
      return a.lastAccessTime < b.lastAccessTime; // Then oldest
    });

    // Skip CRITICAL priority resources
    size_t evictIndex = 0;
    while (evictIndex < textureAccessOrder_.size() && textureAccessOrder_[evictIndex].priority == ResourcePriority::CRITICAL)
    {
      ++evictIndex;
    }

    if (evictIndex >= textureAccessOrder_.size())
    {
      // All resources are CRITICAL, cannot evict
      return;
    }

    // Evict resource at evictIndex
    const auto& toEvict = textureAccessOrder_[evictIndex];
    auto        it      = textureCache_.find(toEvict.key);
    if (it != textureCache_.end())
    {
      textureCache_.erase(it);
      cachedTextureMemory_ -= toEvict.memorySize;
    }
    textureAccessOrder_.erase(textureAccessOrder_.begin() + evictIndex);
  }

  void ResourceManager::evictLRUModels()
  {
    if (modelAccessOrder_.empty())
    {
      return;
    }

    // Sort by priority first (low priority first), then by access time (oldest
    // first)
    std::ranges::sort(modelAccessOrder_, [](const ResourceInfo& a, const ResourceInfo& b) {
      if (a.priority != b.priority)
      {
        return a.priority < b.priority; // Lower priority evicted first
      }
      return a.lastAccessTime < b.lastAccessTime; // Then oldest
    });

    // Skip CRITICAL priority resources
    size_t evictIndex = 0;
    while (evictIndex < modelAccessOrder_.size() && modelAccessOrder_[evictIndex].priority == ResourcePriority::CRITICAL)
    {
      ++evictIndex;
    }

    if (evictIndex >= modelAccessOrder_.size())
    {
      // All resources are CRITICAL, cannot evict
      return;
    }

    // Evict resource at evictIndex
    const auto& toEvict = modelAccessOrder_[evictIndex];
    auto        it      = modelCache_.find(toEvict.key);
    if (it != modelCache_.end())
    {
      modelCache_.erase(it);
      cachedModelMemory_ -= toEvict.memorySize;
    }
    modelAccessOrder_.erase(modelAccessOrder_.begin() + evictIndex);
  }

  uint64_t ResourceManager::getCurrentTime()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  std::string ResourceManager::computeContentHash(const unsigned char* data, size_t dataSize)
  {
    // Use FNV-1a hash for fast content-based deduplication
    uint64_t const hash = hashBytes(data, dataSize);

    // Convert to hex string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
  }

  // ============================================================================
  // ASYNC LOADING IMPLEMENTATION
  // ============================================================================

  void ResourceManager::initThreadPool(size_t numThreads)
  {
    workerThreads_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i)
    {
      workerThreads_.emplace_back(&ResourceManager::workerThreadLoop, this);
    }
  }

  void ResourceManager::shutdownThreadPool()
  {
    {
      std::scoped_lock const lock(taskQueueMutex_);
      shutdownThreadPool_ = true;
    }
    taskQueueCV_.notify_all();

    for (auto& thread : workerThreads_)
    {
      if (thread.joinable())
      {
        thread.join();
      }
    }
    workerThreads_.clear();
  }

  void ResourceManager::workerThreadLoop()
  {
    while (true)
    {
      std::function<void()> task;

      {
        std::unique_lock<std::mutex> lock(taskQueueMutex_);
        taskQueueCV_.wait(lock, [this] { return shutdownThreadPool_ || !taskQueue_.empty(); });

        if (shutdownThreadPool_ && taskQueue_.empty())
        {
          return;
        }

        if (!taskQueue_.empty())
        {
          task = std::move(taskQueue_.front());
          taskQueue_.pop();
          activeTasks_++;
        }
      }

      if (task)
      {
        task();
        activeTasks_--;
      }
    }
  }

  std::future<std::shared_ptr<Texture>> ResourceManager::loadTextureAsync(const std::string& path, bool srgb, ResourcePriority priority)
  {
    // Check if already cached (fast path)
    std::string const key = makeTextureKey(path, srgb);
    {
      std::scoped_lock const lock(textureMutex_);
      auto                   it = textureCache_.find(key);
      if (it != textureCache_.end())
      {
        if (auto existingTexture = it->second.lock())
        {
          // Update access time
          updateTextureAccess(key, existingTexture->getMemorySize(), priority);

          // Return immediately resolved future
          std::promise<std::shared_ptr<Texture>> promise;
          promise.set_value(existingTexture);
          return promise.get_future();
        }
      }
    }

    // Create promise/future pair
    auto                                  promise = std::make_shared<std::promise<std::shared_ptr<Texture>>>();
    std::future<std::shared_ptr<Texture>> future  = promise->get_future();

    // Enqueue async task
    {
      std::scoped_lock const lock(taskQueueMutex_);
      taskQueue_.emplace([this, path, srgb, priority, promise]() {
        try
        {
          // Load texture synchronously on worker thread
          auto texture = loadTexture(path, srgb, false, priority);
          promise->set_value(texture);
        }
        catch (const std::exception& /*e*/)
        {
          promise->set_exception(std::current_exception());
        }
      });
    }
    taskQueueCV_.notify_one();

    return future;
  }

  std::future<std::shared_ptr<Model>> ResourceManager::loadModelAsync(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets, ResourcePriority priority)
  {
    // Check if already cached (fast path)
    std::string const key = makeModelKey(path, enableTextures, loadMaterials, enableMorphTargets);
    {
      std::scoped_lock const lock(modelMutex_);
      auto                   it = modelCache_.find(key);
      if (it != modelCache_.end())
      {
        if (auto existingModel = it->second.lock())
        {
          // Update access time
          updateModelAccess(key, existingModel->getMemorySize(), priority);

          // Return immediately resolved future
          std::promise<std::shared_ptr<Model>> promise;
          promise.set_value(existingModel);
          return promise.get_future();
        }
      }
    }

    // Create promise/future pair
    auto                                promise = std::make_shared<std::promise<std::shared_ptr<Model>>>();
    std::future<std::shared_ptr<Model>> future  = promise->get_future();

    // Enqueue async task
    {
      std::scoped_lock const lock(taskQueueMutex_);
      taskQueue_.emplace([this, path, enableTextures, loadMaterials, enableMorphTargets, priority, promise]() {
        try
        {
          // Load model synchronously on worker thread
          auto model = loadModel(path, enableTextures, loadMaterials, enableMorphTargets, priority);
          promise->set_value(model);
        }
        catch (const std::exception& /*e*/)
        {
          promise->set_exception(std::current_exception());
        }
      });
    }
    taskQueueCV_.notify_one();

    return future;
  }

  size_t ResourceManager::getPendingAsyncLoads() const
  {
    std::scoped_lock const lock(taskQueueMutex_);
    return taskQueue_.size() + activeTasks_;
  }

  void ResourceManager::waitForAsyncLoads() const
  {
    while (getPendingAsyncLoads() > 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

} // namespace engine
