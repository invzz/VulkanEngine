#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_RESOURCEMANAGER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_RESOURCEMANAGER_HPP
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/Device.hpp"

#include "Engine/Graphics/AccelBuilder.hpp"
#include "ModelLib/Resources/MeshManager.hpp"
namespace engine {
    class Texture;
    class Model;
    class TextureManager;
    class Scene;
    /**
 * @brief Async loading status for tracking resource load progress
 */
    enum class LoadStatus : std::uint8_t {
        PENDING,
        LOADING,
        COMPLETE,
        FAILED
    };
    using AsyncLoadId = uint64_t;
    struct AsyncLoadSnapshot {
        AsyncLoadId id{0};
        LoadStatus  status{LoadStatus::PENDING};
        std::string path;
        float       progress{0.0f};
        bool        isModel{true};
        bool        hasError{false};
        std::string error;
    };
    /**
 * @brief Handle for tracking async resource loading
 */
    template <typename T>
    struct AsyncLoadHandle {
        std::future<std::shared_ptr<T>> future;
        LoadStatus                      status;
        std::string                     path;
        float                           progress;
    };
    /**
 * @brief Resource priority for eviction policy
 * Higher priority resources are kept in cache longer
 */
    enum class ResourcePriority : std::uint8_t {
        LOW      = 0,
        MEDIUM   = 1,
        HIGH     = 2,
        CRITICAL = 3
    };
    /**
 * @brief Centralized resource management with automatic deduplication and
 * lifetime tracking
 *
 * Features:
 * - Automatic resource deduplication (same path loaded once)
 * - Memory tracking and budgeting
 * - Automatic cleanup of unused resources via weak_ptr
 * - Thread-safe resource access
 * - Prepared for future async loading support
 */
    class ResourceManager {
       public:
        explicit ResourceManager(Device& device);
        ~ResourceManager();
        ResourceManager(const ResourceManager&)            = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&)                 = delete;
        ResourceManager& operator=(ResourceManager&&)      = delete;
        /**
   * @brief Load a texture from file with automatic caching
   * @param path Absolute or relative path to texture file
   * @param srgb Whether to load as sRGB format (true for color textures, false
   * for data)
   * @param priority Resource priority for eviction policy
   * @return Shared pointer to texture (returns cached instance if already
   * loaded)
   */
        std::shared_ptr<Texture> loadTexture(const std::string& path, bool srgb = true, bool flipY = false, ResourcePriority priority = ResourcePriority::MEDIUM);
        /**
   * @brief Load a texture from memory with automatic caching (for embedded
   * textures)
   * @param data Texture data in memory
   * @param dataSize Size of texture data in bytes
   * @param debugName Debug name for the texture (used for cache key)
   * @param srgb Whether to load as sRGB format
   * @param priority Resource priority for eviction policy
   * @return Shared pointer to texture (returns cached instance if same data
   * already loaded)
   */
        std::shared_ptr<Texture> loadTextureFromMemory(const unsigned char* data, size_t dataSize, const std::string& debugName, bool srgb = true, ResourcePriority priority = ResourcePriority::MEDIUM);
        /**
   * @brief Load a model from file with automatic caching
   * @param path Absolute or relative path to model file
   * @param enableTextures Whether to load textures from MTL file
   * @param loadMaterials Whether to load materials from MTL file
   * @param enableMorphTargets Whether to enable morph target support
   * @param priority Resource priority for eviction policy
   * @return Shared pointer to model (returns cached instance if already loaded)
   */
        std::shared_ptr<Model>
        loadModel(const std::string& path, bool enableTextures = false, bool loadMaterials = true, bool enableMorphTargets = false, ResourcePriority priority = ResourcePriority::MEDIUM);
        /**
   * @brief Remove unused resources from cache (those with no external
   * references) Call periodically (e.g., after scene transitions) to free
   * memory
   * @return Number of resources removed
   */
        size_t garbageCollect();
        /**
   * @brief Get actual memory usage of all cached resources
   * @return Memory usage in bytes (actual, not estimated)
   */
        size_t getMemoryUsage() const;
        /**
   * @brief Get number of cached textures
   */
        size_t getCachedTextureCount() const;
        /**
   * @brief Get number of cached models
   */
        size_t getCachedModelCount() const;
        /**
   * @brief Set memory budget (resources evicted when exceeded)
   * @param budgetBytes Memory budget in bytes (0 = unlimited)
   */
        void setMemoryBudget(size_t budgetBytes);
        /**
   * @brief Get current memory budget
   * @return Memory budget in bytes (0 = unlimited)
   */
        size_t getMemoryBudget() const {
            return memoryBudget_;
        }
        [[nodiscard]] Device& getDevice() const {
            return device_;
        }
        /**
   * @brief Clear all cached resources immediately
   * Warning: This will invalidate all external shared_ptr references
   */
        void clearAll();
        /**
   * @brief Check if a texture is cached
   */
        bool isTextureCached(const std::string& path) const;
        /**
   * @brief Check if a model is cached
   */
        bool isModelCached(const std::string& path) const;
        /**
   * @brief Load a texture asynchronously in background thread
   * @param path Absolute or relative path to texture file
   * @param srgb Whether to load as sRGB format
   * @param priority Resource priority for eviction policy
   * @return Future that resolves to texture when loading completes
   */
        std::future<std::shared_ptr<Texture>> loadTextureAsync(const std::string& path, bool srgb = true, ResourcePriority priority = ResourcePriority::MEDIUM);
        /**
   * @brief Load a model asynchronously in background thread
   * @param path Absolute or relative path to model file
   * @param enableTextures Whether to load textures from MTL file
   * @param loadMaterials Whether to load materials from MTL file
   * @param enableMorphTargets Whether to enable morph target support
   * @param priority Resource priority for eviction policy
   * @return Future that resolves to model when loading completes
   */
        std::future<std::shared_ptr<Model>>
        loadModelAsync(const std::string& path, bool enableTextures = false, bool loadMaterials = true, bool enableMorphTargets = false, ResourcePriority priority = ResourcePriority::MEDIUM);
        /**
   * @brief Enqueue a model load and track it via a non-blocking handle
   * @param path Model path
   * @param enableTextures Whether to load textures
   * @param loadMaterials Whether to load materials
   * @param enableMorphTargets Whether to enable morph targets
   * @param priority Resource priority
   * @param onComplete Callback invoked on main thread by updateAsyncCallbacks()
   * @param onFailed Callback invoked on main thread by updateAsyncCallbacks()
   * @return Handle id for polling
   */
        AsyncLoadId enqueueModelLoad(
            const std::string&                                 path,
            bool                                               enableTextures,
            bool                                               loadMaterials,
            bool                                               enableMorphTargets,
            ResourcePriority                                   priority,
            std::function<void(const std::shared_ptr<Model>&)> onComplete = {},
            std::function<void(const std::string&)>            onFailed   = {});
        /**
   * @brief Poll async model result without blocking
   * @return true if load finished (success or failure), false if still pending
   */
        bool tryGetModelLoadResult(AsyncLoadId id, std::shared_ptr<Model>& outModel, std::string* outError = nullptr);
        /**
   * @brief Get snapshot list for UI progress bars
   */
        std::vector<AsyncLoadSnapshot> getAsyncLoadSnapshots() const;
        /**
   * @brief Dispatch completion/failure callbacks for finished async tasks
   * Call from main thread each frame.
   */
        void updateAsyncCallbacks();
        /**
   * @brief Cancel a tracked async model load callback delivery
   * Note: if loading has already started, underlying IO may still complete.
   */
        void cancelModelLoad(AsyncLoadId id);
        /**
   * @brief Check if async loading is ready (non-blocking)
   * @return True if future is ready, false if still loading
   */
        template <typename T>
        static bool isReady(const std::future<std::shared_ptr<T>>& future) {
            return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }
        /**
   * @brief Get number of pending async load tasks
   */
        size_t getPendingAsyncLoads() const;
        /**
   * @brief Wait for all pending async loads to complete
   */
        void waitForAsyncLoads() const;
        /**
   * @brief Get the Texture Manager for bindless rendering
   */
        TextureManager& getTextureManager() const {
            return *textureManager_;
        }
        /**
   * @brief Get the Mesh Manager for bindless rendering
   */
        MeshManager& getMeshManager() const {
            return *meshManager_;
        }

        void setAccelBuilder(AccelBuilder* builder) {
            accelBuilder_ = builder;
        }

       private:
        Device&                                                 device_;
        std::unique_ptr<TextureManager>                         textureManager_;
        std::unique_ptr<MeshManager>                            meshManager_;
        AccelBuilder*                                           accelBuilder_ = nullptr;
        mutable std::mutex                                      textureMutex_;
        std::unordered_map<std::string, std::weak_ptr<Texture>> textureCache_;
        mutable std::mutex                                      modelMutex_;
        std::unordered_map<std::string, std::weak_ptr<Model>>   modelCache_;
        struct ResourceInfo {
            std::string      key;
            size_t           memorySize;
            uint64_t         lastAccessTime;
            ResourcePriority priority;
        };
        std::vector<ResourceInfo>                    textureAccessOrder_;
        std::vector<ResourceInfo>                    modelAccessOrder_;
        std::unordered_map<std::string, std::string> contentHashToKey_;
        size_t                                       memoryBudget_        = 0;
        mutable size_t                               cachedTextureMemory_ = 0;
        mutable size_t                               cachedModelMemory_   = 0;
        static std::string                           makeTextureKey(const std::string& path, bool srgb);
        static std::string                           makeModelKey(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets);
        void                                         updateTextureAccess(const std::string& key, size_t memorySize, ResourcePriority priority);
        void                                         updateModelAccess(const std::string& key, size_t memorySize, ResourcePriority priority);
        void                                         evictLRUTextures();
        void                                         evictLRUModels();
        static uint64_t                              getCurrentTime();
        static std::string                           computeContentHash(const unsigned char* data, size_t dataSize);
        std::vector<std::thread>                     workerThreads_;
        std::queue<std::function<void()>>            taskQueue_;
        mutable std::mutex                           taskQueueMutex_;
        std::condition_variable                      taskQueueCV_;
        std::atomic<bool>                            shutdownThreadPool_{false};
        std::atomic<size_t>                          activeTasks_{0};
        struct AsyncModelTaskRecord {
            AsyncLoadId                                        id{0};
            std::string                                        path;
            LoadStatus                                         status{LoadStatus::PENDING};
            float                                              progress{0.0f};
            std::string                                        error;
            std::shared_future<std::shared_ptr<Model>>         future;
            std::shared_ptr<Model>                             result;
            bool                                               callbackDispatched{false};
            bool                                               cancelled{false};
            std::function<void(const std::shared_ptr<Model>&)> onComplete;
            std::function<void(const std::string&)>            onFailed;
        };
        mutable std::mutex                                    asyncTasksMutex_;
        std::unordered_map<AsyncLoadId, AsyncModelTaskRecord> asyncModelTasks_;
        std::atomic<AsyncLoadId>                              nextAsyncLoadId_{1};
        void                                                  initThreadPool(size_t numThreads = 4);
        void                                                  shutdownThreadPool();
        void                                                  workerThreadLoop();
        template <typename T>
        void enqueueTask(std::function<void()> task);
    };
}  // namespace engine
#endif
