#pragma once

#include <cstddef>
#include <functional>

namespace engine {
  // from https://stackoverflow.com/a/57595105
  template <typename T, typename... Rest> void hashCombine(std::size_t& seed, const T& value, const Rest&... args)
  {
    seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hashCombine(seed, args), ...);
  }
} // namespace engine
