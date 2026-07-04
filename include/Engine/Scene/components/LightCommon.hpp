#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_LIGHTCOMMON_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_LIGHTCOMMON_HPP
#include <string>
namespace engine {
    enum class LightMobility {
        Static  = 0,
        Dynamic = 1
    };
    inline const char* to_string(LightMobility m) {
        return (m == LightMobility::Dynamic) ? "dynamic" : "static";
    }
    inline LightMobility mobility_from_string(const std::string& s) {
        if (s == "dynamic")
            return LightMobility::Dynamic;
        return LightMobility::Static;
    }
}  // namespace engine
#endif
