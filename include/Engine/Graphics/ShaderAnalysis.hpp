#pragma once

#include <Engine/Graphics/ShaderMonitor.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <variant>
#include <algorithm>

namespace Engine {

class ShaderMonitor;

// Forward declare the observer interface for use in config
class IShaderAnalysisObserver;

/**
 * @brief Unique key for identifying a shader variant
 */
struct ShaderVariantKey {
    std::uint64_t shaderHash;
    std::string variantType;

    bool operator==(const ShaderVariantKey& other) const {
        return shaderHash == other.shaderHash && variantType == other.variantType;
    }
};

} // namespace Engine

namespace std {

template<>
struct hash<Engine::ShaderVariantKey> {
    std::size_t operator()(const Engine::ShaderVariantKey& k) const {
        return std::hash<std::uint64_t>{}(k.shaderHash) ^
               (std::hash<std::string>{}(k.variantType) << 1);
    }
};

} // namespace std
