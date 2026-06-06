#pragma once

#include <cstdint>
#include <iosfwd>
#include <ostream>

namespace engine {

    /**
 * @brief Opaque handle to a loaded model resource.
 *
 * Domain-layer replacement for std::shared_ptr<Model>. The handle carries
 * just an identifier; resolution to the concrete Model is the responsibility
 * of the Infrastructure layer (via IModelResourcePort / ResourceManager).
 *
 * A value of 0 represents an invalid/null handle (similar to a null pointer).
 */
    struct ModelHandle {
        static constexpr uint64_t kInvalid = 0;

        uint64_t id = kInvalid;

        ModelHandle() = default;
        explicit ModelHandle(uint64_t id_) : id(id_) {}

        explicit operator bool() const {
            return id != kInvalid;
        }
        bool isValid() const {
            return id != kInvalid;
        }

        bool operator==(const ModelHandle& other) const = default;
        bool operator!=(const ModelHandle& other) const = default;
        bool operator<(const ModelHandle& other) const {
            return id < other.id;
        }

        friend std::ostream& operator<<(std::ostream& os, const ModelHandle& h) {
            os << h.id;
            return os;
        }
    };

    /**
 * @brief Sentinel for "no model".
 */
    inline constexpr ModelHandle kInvalidModelHandle{};

}  // namespace engine
