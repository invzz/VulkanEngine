#include "Engine/SystemRegistry.hpp"
namespace engine {
    bool SystemRegistry::registerSystem(const std::string& name, std::vector<std::string> dependencies, InitFn initFn, std::string* error) {
        if (name.empty()) {
            if (error != nullptr) {
                *error = "SystemRegistry: system name cannot be empty";
            }
            return false;
        }
        if (initFn == nullptr) {
            if (error != nullptr) {
                *error = "SystemRegistry: init function is null for system '" + name + "'";
            }
            return false;
        }
        if (indexByName_.contains(name)) {
            if (error != nullptr) {
                *error = "SystemRegistry: duplicate system registration for '" + name + "'";
            }
            return false;
        }
        indexByName_[name] = entries_.size();
        entries_.push_back(Entry{.name = name, .dependencies = std::move(dependencies), .initFn = std::move(initFn)});
        return true;
    }
    bool SystemRegistry::initializeAll(std::string* error) {
        initializedOrder_.clear();
        std::vector<size_t> order;
        if (!buildInitializationOrder(order, error)) {
            return false;
        }
        for (size_t index : order) {
            std::string localError;
            if (!entries_[index].initFn(localError)) {
                if (error != nullptr) {
                    if (localError.empty()) {
                        *error = "SystemRegistry: initialization failed for '" + entries_[index].name + "'";
                    } else {
                        *error = "SystemRegistry: initialization failed for '" + entries_[index].name + "': " + localError;
                    }
                }
                return false;
            }
            initializedOrder_.push_back(entries_[index].name);
        }
        return true;
    }
    bool SystemRegistry::hasSystem(const std::string& name) const {
        return indexByName_.contains(name);
    }
    const std::vector<std::string>& SystemRegistry::initializationOrder() const {
        return initializedOrder_;
    }
    void SystemRegistry::clear() {
        entries_.clear();
        indexByName_.clear();
        initializedOrder_.clear();
    }
    bool SystemRegistry::buildInitializationOrder(std::vector<size_t>& order, std::string* error) const {
        order.clear();
        order.reserve(entries_.size());
        std::vector<uint8_t> marks(entries_.size(), 0);
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (marks[i] == 0 && !dfsVisit(i, marks, order, error)) {
                return false;
            }
        }
        return true;
    }
    bool SystemRegistry::dfsVisit(size_t index, std::vector<uint8_t>& marks, std::vector<size_t>& order, std::string* error) const {
        marks[index]       = 1;
        const Entry& entry = entries_[index];
        for (const auto& depName : entry.dependencies) {
            auto depIt = indexByName_.find(depName);
            if (depIt == indexByName_.end()) {
                if (error != nullptr) {
                    *error = "SystemRegistry: system '" + entry.name + "' depends on missing system '" + depName + "'";
                }
                return false;
            }
            const size_t depIndex = depIt->second;
            if (marks[depIndex] == 1) {
                if (error != nullptr) {
                    *error = "SystemRegistry: dependency cycle detected between '" + entry.name + "' and '" + depName + "'";
                }
                return false;
            }
            if (marks[depIndex] == 0 && !dfsVisit(depIndex, marks, order, error)) {
                return false;
            }
        }
        marks[index] = 2;
        order.push_back(index);
        return true;
    }
}  // namespace engine