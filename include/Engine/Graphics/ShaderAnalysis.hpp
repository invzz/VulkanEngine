#pragma once

#include <Engine/Graphics/ShaderMonitor.hpp>
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace Engine {

    class ShaderMonitor;

    class IShaderAnalysisObserver;

    /**
 * @brief Unique key for identifying a shader variant
 */
    struct ShaderVariantKey {
        std::uint64_t shaderHash;
        std::string   variantType;

        bool operator==(const ShaderVariantKey& other) const {
            return shaderHash == other.shaderHash && variantType == other.variantType;
        }
    };

}  // namespace Engine

namespace std {

    template <>
    struct hash<Engine::ShaderVariantKey> {
        std::size_t operator()(const Engine::ShaderVariantKey& k) const {
            return std::hash<std::uint64_t>{}(k.shaderHash) ^
                   (std::hash<std::string>{}(k.variantType) << 1);
        }
    };

}  // namespace std
