#include "Engine/Scene/LightmapManifest.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"

namespace engine::scene {

  bool parseSceneLightmapManifest(const std::string& manifestPath, std::unordered_map<std::string, LightmapInfo>& outLightmaps, std::unordered_map<std::string, LightmapBinding>& outBindings)
  {
    try
    {
      if (!std::filesystem::exists(manifestPath)) return false;
      std::ifstream in(manifestPath);
      if (!in) return false;

      nlohmann::json j;
      in >> j;

      outLightmaps.clear();
      outBindings.clear();

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
            info.paddingPx = l.value("paddingPx", 0);
            info.usage     = l.value("usage", std::string());
            if (!info.id.empty()) outLightmaps[info.id] = info;
          }
          catch (const std::exception& e)
          {
            std::cerr << "LightmapManifest: failed parsing lightmap entry: " << e.what() << "\n";
          }
        }
      }

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
            if (bind.contains("uvScale") && bind["uvScale"].is_array() && bind["uvScale"].size() == 2)
            {
              b.uvScale.x = bind["uvScale"][0].get<float>();
              b.uvScale.y = bind["uvScale"][1].get<float>();
            }
            if (bind.contains("uvOffset") && bind["uvOffset"].is_array() && bind["uvOffset"].size() == 2)
            {
              b.uvOffset.x = bind["uvOffset"][0].get<float>();
              b.uvOffset.y = bind["uvOffset"][1].get<float>();
            }
            outBindings[objectId] = b;
          }
          catch (const std::exception& e)
          {
            std::cerr << "LightmapManifest: failed parsing binding for object " << objectId << ": " << e.what() << "\n";
          }
        }
      }

      return true;
    }
    catch (const std::exception& e)
    {
      std::cerr << "LightmapManifest: failed loading manifest: " << e.what() << "\n";
      return false;
    }
  }

  void applyBindingsToScene(const std::unordered_map<std::string, LightmapBinding>& bindings, engine::Scene& scene)
  {
    auto& reg  = scene.getRegistry();
    auto  view = reg.view<engine::NameComponent>();
    for (auto entity : view)
    {
      const auto& nameComp = reg.get<engine::NameComponent>(entity);
      auto        it       = bindings.find(nameComp.name);
      if (it != bindings.end())
      {
        const LightmapBinding& b = it->second;
        // Emplace LightmapComponent
        if (!reg.all_of<engine::LightmapComponent>(entity))
        {
          reg.emplace<engine::LightmapComponent>(entity, engine::LightmapComponent{b.lightmapId, b.uvChannel, b.uvScale, b.uvOffset, -1});
        }
        else
        {
          auto& lm      = reg.get<engine::LightmapComponent>(entity);
          lm.lightmapId = b.lightmapId;
          lm.uvChannel  = b.uvChannel;
          lm.uvScale    = b.uvScale;
          lm.uvOffset   = b.uvOffset;
        }

        // Also try to set material uvScale approx from x component for convenience
        if (reg.all_of<engine::PBRMaterial>(entity))
        {
          auto& mat   = reg.get<engine::PBRMaterial>(entity);
          mat.uvScale = b.uvScale.x; // approximate, per-instance scale stored in LightmapComponent
        }
      }
    }
  }

} // namespace engine::scene
