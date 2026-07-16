#ifndef VULKANENGINE_INCLUDE_MODELIB_RESOURCES_RESOURCELRUCACHE_HPP
#define VULKANENGINE_INCLUDE_MODELIB_RESOURCES_RESOURCELRUCACHE_HPP

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine {

    /**
 * @brief Resource priority for eviction policy.
 * Higher-priority resources are evicted last.
 */
    enum class ResourcePriority : std::uint8_t {
        LOW      = 0,
        MEDIUM   = 1,
        HIGH     = 2,
        CRITICAL = 3
    };

    /**
 * @brief Thread-safe LRU cache with weak-pointer lifetime tracking,
 *        memory accounting, and priority-aware eviction.
 *
 * @tparam T Resource type (must provide getMemorySize()).
 *
 * The cache supports:
 *  - find(key, priority): returns shared_ptr if alive, else nullptr
 *  - insert(key, shared_ptr, budget, priority): stores, evicts under budget
 *  - applyBudget(budget): evicts down to a target
 *  - garbageCollect(): removes expired weak_ptrs
 *  - memoryUsage() / cachedCount()
 *  - clear()
 *  - onKeysRemoved: optional hook for side-effects (e.g. content-hash map)
 */
    template <typename T>
    class ResourceLRUCache {
       public:
        using Ptr = std::shared_ptr<T>;

        struct ResourceInfo {
            std::string      key;
            size_t           memorySize;
            uint64_t         lastAccessTime;
            ResourcePriority priority;
        };

        /// Look up a cached resource. Returns nullptr if missing or expired.
        std::shared_ptr<T> find(const std::string& key, ResourcePriority priority) {
            std::scoped_lock const lock(mutex_);
            auto                   it = cache_.find(key);
            if (it == cache_.end())
                return nullptr;
            if (auto cached = it->second.lock()) {
                touch(key, cached->getMemorySize(), priority);
                return cached;
            }
            eraseLocked(key);
            return nullptr;
        }

        /// Insert or update a resource. Applies memory-budget eviction afterward.
        /// Returns the stored shared_ptr (same as input).
        std::shared_ptr<T> insert(const std::string& key, std::shared_ptr<T> resource,
            size_t memoryBudget, ResourcePriority priority) {
            std::scoped_lock const lock(mutex_);
            size_t const           memSize = resource->getMemorySize();
            if (memoryBudget > 0) {
                cachedMemory_ += memSize;
                while (cachedMemory_ > memoryBudget && !cache_.empty()) {
                    evictOneLocked();
                }
            }
            cache_[key] = resource;
            touch(key, memSize, priority);
            return resource;
        }

        /// Evict resources until total memory is under the given budget.
        void applyBudget(size_t memoryBudget) {
            std::scoped_lock const lock(mutex_);
            while (memoryBudget > 0 && cachedMemory_ > memoryBudget && !cache_.empty()) {
                evictOneLocked();
            }
        }

        /// Remove expired (unreferenced) entries. Returns count removed.
        size_t garbageCollect() {
            std::scoped_lock const lock(mutex_);
            size_t                 removedCount = 0;
            cachedMemory_                       = 0;
            std::unordered_set<std::string> removedKeys;
            for (auto it = cache_.begin(); it != cache_.end();) {
                if (auto res = it->second.lock()) {
                    cachedMemory_ += res->getMemorySize();
                    ++it;
                    continue;
                }
                removedKeys.insert(it->first);
                it = cache_.erase(it);
                ++removedCount;
            }
            if (!removedKeys.empty()) {
                auto const removed = std::ranges::remove_if(
                    accessOrder_, [&](const ResourceInfo& info) { return removedKeys.contains(info.key); });
                accessOrder_.erase(removed.begin(), removed.end());
                if (onKeysRemoved)
                    onKeysRemoved(removedKeys);
            }
            return removedCount;
        }

        /// Total memory of all alive cached resources.
        size_t memoryUsage() const {
            std::scoped_lock const lock(mutex_);
            size_t                 total = 0;
            for (const auto& [key, weak] : cache_) {
                if (auto res = weak.lock())
                    total += res->getMemorySize();
            }
            return total;
        }

        /// Number of alive cached resources.
        size_t cachedCount() const {
            std::scoped_lock const lock(mutex_);
            size_t                 count = 0;
            for (const auto& [key, weak] : cache_) {
                if (!weak.expired())
                    ++count;
            }
            return count;
        }

        /// Remove all entries (even alive ones).
        void clear() {
            std::scoped_lock const lock(mutex_);
            cache_.clear();
            accessOrder_.clear();
            cachedMemory_ = 0;
        }

        /// Returns true if any cached key matches the predicate and is alive.
        template <typename Pred>
        bool anyCached(Pred&& pred) const {
            std::scoped_lock const lock(mutex_);
            return std::ranges::any_of(cache_, [&](const auto& p) {
                return pred(p.first) && !p.second.expired();
            });
        }

        /// Optional hook invoked after garbageCollect removes keys.
        /// Useful for cleaning up secondary maps (e.g. content-hash → key).
        std::function<void(const std::unordered_set<std::string>&)> onKeysRemoved;

        /// Exposed for locking across multiple cache operations.
        mutable std::mutex mutex_;

       private:
        void touch(const std::string& key, size_t memSize, ResourcePriority priority) {
            auto const removed = std::ranges::remove_if(accessOrder_, [&](const ResourceInfo& i) { return i.key == key; });
            accessOrder_.erase(removed.begin(), removed.end());
            accessOrder_.push_back({key, memSize, getCurrentTimeMs(), priority});
        }

        void eraseLocked(const std::string& key) {
            cache_.erase(key);
            auto const removed = std::ranges::remove_if(accessOrder_, [&](const ResourceInfo& i) { return i.key == key; });
            accessOrder_.erase(removed.begin(), removed.end());
        }

        void evictOneLocked() {
            if (accessOrder_.empty())
                return;
            std::ranges::sort(accessOrder_, [](const ResourceInfo& a, const ResourceInfo& b) {
                if (a.priority != b.priority)
                    return a.priority < b.priority;
                return a.lastAccessTime < b.lastAccessTime;
            });
            size_t idx = 0;
            while (idx < accessOrder_.size() && accessOrder_[idx].priority == ResourcePriority::CRITICAL)
                ++idx;
            if (idx >= accessOrder_.size())
                return;  // everything is CRITICAL; refuse to evict
            const auto& victim = accessOrder_[idx];
            cache_.erase(victim.key);
            cachedMemory_ -= victim.memorySize;
            accessOrder_.erase(accessOrder_.begin() + static_cast<std::ptrdiff_t>(idx));
        }

        static uint64_t getCurrentTimeMs() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }

        std::unordered_map<std::string, std::weak_ptr<T>> cache_;
        std::vector<ResourceInfo>                         accessOrder_;
        size_t                                            cachedMemory_ = 0;
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_MODELIB_RESOURCES_RESOURCELRUCACHE_HPP