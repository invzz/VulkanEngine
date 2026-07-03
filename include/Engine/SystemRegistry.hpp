#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMREGISTRY_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMREGISTRY_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

    class SystemRegistry {
       public:
        using InitFn = std::function<bool(std::string&)>;

        bool registerSystem(const std::string& name, std::vector<std::string> dependencies, InitFn initFn, std::string* error = nullptr);
        bool initializeAll(std::string* error = nullptr);

        [[nodiscard]] bool                            hasSystem(const std::string& name) const;
        [[nodiscard]] const std::vector<std::string>& initializationOrder() const;

        void clear();

       private:
        struct Entry {
            std::string              name;
            std::vector<std::string> dependencies;
            InitFn                   initFn;
        };

        bool buildInitializationOrder(std::vector<size_t>& order, std::string* error) const;
        bool dfsVisit(size_t index, std::vector<uint8_t>& marks, std::vector<size_t>& order, std::string* error) const;

        std::vector<Entry>                      entries_;
        std::unordered_map<std::string, size_t> indexByName_;
        std::vector<std::string>                initializedOrder_;
    };

}  // namespace engine

#endif