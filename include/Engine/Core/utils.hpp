#pragma once

#include <cstddef>
#include <functional>
#include <glm/glm.hpp>
#include <type_traits>

namespace engine {
  namespace detail {
    template <class T>
    concept StdHashable = std::is_default_constructible_v<std::hash<T>> && requires(const T& v) {
      { std::hash<T>{}(v) } -> std::convertible_to<std::size_t>;
    };

    template <typename T> inline void hashCombineOne(std::size_t& seed, const T& value)
    {
      if constexpr (StdHashable<T>)
      {
        seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      else
      {
        static_assert(StdHashable<T>,
                      "Type is not hashable: provide std::hash<T> specialization "
                      "or an engine::detail::hashCombineOne overload.");
      }
    }

    template <glm::length_t L, typename T, glm::qualifier Q> inline void hashCombineOne(std::size_t& seed, const glm::vec<L, T, Q>& value)
    {
      for (glm::length_t i = 0; i < L; ++i)
      {
        hashCombineOne(seed, value[i]);
      }
    }
  } // namespace detail

  // from https://stackoverflow.com/a/57595105
  template <typename T, typename... Rest> void hashCombine(std::size_t& seed, const T& value, const Rest&... args)
  {
    detail::hashCombineOne(seed, value);
    (detail::hashCombineOne(seed, args), ...);
  }
} // namespace engine
