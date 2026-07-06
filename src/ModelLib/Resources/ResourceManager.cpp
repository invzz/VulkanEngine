#include "ModelLib/Resources/ResourceManager.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stb_image.h>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include "Engine/Graphics/Device.hpp"

#include "ModelLib/Resources/MeshManager.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "ModelLib/Resources/Texture.hpp"
#include "ModelLib/Resources/TextureManager.hpp"
namespace engine {
    namespace {
        uint64_t hashBytes(const unsigned char* data, size_t length) {
            uint64_t hash = 14695981039346656037ULL;
            for (size_t i = 0; i < length; ++i) {
                hash ^= data[i];
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        template <typename T>
        std::future<T> makeReadyFuture(T value) {
            std::promise<T> p;
            p.set_value(std::move(value));
            return p.get_future();
        }
    }  // namespace
    ResourceManager::ResourceManager(Device& device) : device_(device) {
        textureManager_   = std::make_unique<TextureManager>(device);
        meshManager_      = std::make_unique<MeshManager>(device);
        size_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4;
        }
        initThreadPool(numThreads);

        // Wire up content-hash cleanup when GC removes textures
        textureCache_.onKeysRemoved = [this](const std::unordered_set<std::string>& removedKeys) {
            for (auto it = contentHashToKey_.begin(); it != contentHashToKey_.end();) {
                if (removedKeys.contains(it->second)) {
                    it = contentHashToKey_.erase(it);
                } else {
                    ++it;
                }
            }
        };
    }
    ResourceManager::~ResourceManager() {
        shutdownThreadPool();
    }
    std::string ResourceManager::makeTextureKey(const std::string& path, bool srgb) {
        return path + (srgb ? "|srgb" : "|linear");
    }
    std::string ResourceManager::makeModelKey(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets) {
        std::ostringstream oss;
        oss << path << "|tex=" << enableTextures << "|mat=" << loadMaterials << "|morph=" << enableMorphTargets;
        return oss.str();
    }
    std::shared_ptr<Texture> ResourceManager::loadTexture(const std::string& path, bool srgb, bool flipY, ResourcePriority priority) {
        std::string const key = makeTextureKey(path, srgb) + (flipY ? "|flipY" : "");
        if (auto cached = textureCache_.find(key, priority)) {
            return cached;
        }
        auto        texture = std::make_shared<Texture>(device_, path, srgb, flipY);
        auto        stored  = textureCache_.insert(key, texture, memoryBudget_, priority);
        uint32_t const globalIndex = textureManager_->addTexture(stored);
        stored->setGlobalIndex(globalIndex);
        return stored;
    }
    std::shared_ptr<Model> ResourceManager::loadModel(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets, ResourcePriority priority) {
        std::string const key = makeModelKey(path, enableTextures, loadMaterials, enableMorphTargets);
        if (auto cached = modelCache_.find(key, priority)) {
            return cached;
        }
        auto toLower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        };
        std::string ext;
        auto        pos = path.find_last_of('.');
        if (pos != std::string::npos) {
            ext = toLower(path.substr(pos + 1));
        }
        std::shared_ptr<Model> model;
        if (ext == "gltf" || ext == "glb") {
            model = std::shared_ptr<Model>(Model::createModelFromGLTF(device_, path, false, true, true));
        } else {
            model = std::shared_ptr<Model>(Model::createModelFromFile(device_, path, false, true, true));
        }
        if (enableTextures || loadMaterials) {
            try {
                for (auto& mat : model->getMaterials()) {
                    if (!mat.diffuseTexPath.empty() && enableTextures) {
                        mat.pbrMaterial.albedoMap = loadTexture(mat.diffuseTexPath, true, true);
                    }
                    if (!mat.normalTexPath.empty() && enableTextures) {
                        mat.pbrMaterial.normalMap = loadTexture(mat.normalTexPath, false, true);
                    }
                    if (!mat.roughnessTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.roughnessMap = loadTexture(mat.roughnessTexPath, false, true);
                    }
                    if (!mat.aoTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.aoMap = loadTexture(mat.aoTexPath, false, true);
                    }
                    if (!mat.emissiveTexPath.empty() && enableTextures) {
                        mat.pbrMaterial.emissiveMap = loadTexture(mat.emissiveTexPath, true, true);
                    }
                    if (!mat.specularGlossinessTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.specularGlossinessMap = loadTexture(mat.specularGlossinessTexPath, true, true);
                    }
                    if (!mat.transmissionTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.transmissionMap = loadTexture(mat.transmissionTexPath, false, true);
                    }
                    if (!mat.clearcoatTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.clearcoatMap = loadTexture(mat.clearcoatTexPath, false, true);
                    }
                    if (!mat.clearcoatRoughnessTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.clearcoatRoughnessMap = loadTexture(mat.clearcoatRoughnessTexPath, false, true);
                    }
                    if (!mat.clearcoatNormalTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.clearcoatNormalMap = loadTexture(mat.clearcoatNormalTexPath, false, true);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "ResourceManager: failed loading material textures for " << path << ": " << e.what() << '\n';
            }
        }
        auto stored = modelCache_.insert(key, model, memoryBudget_, priority);
        uint32_t const meshId = meshManager_->registerModel(stored.get());
        stored->setMeshId(meshId);
        // Build BLAS for raytracing if available
        if (accelBuilder_ != nullptr) {
            accelBuilder_->buildBlas(*stored);
        }
        return stored;
    }
    std::shared_ptr<Texture> ResourceManager::loadTextureFromMemory(const unsigned char* data, size_t dataSize, const std::string& debugName, bool srgb, ResourcePriority priority) {
        std::string const contentHash = computeContentHash(data, dataSize);
        std::string       cacheKey;
        // Check via content-hash map first
        {
            auto hashIt = contentHashToKey_.find(contentHash);
            if (hashIt != contentHashToKey_.end()) {
                cacheKey = hashIt->second;
                if (auto cached = textureCache_.find(cacheKey, priority)) {
                    return cached;
                }
                // expired entry in the hash map — will be rebuilt below
            }
        }
        cacheKey = "embedded:" + contentHash + "|" + debugName + (srgb ? "|srgb" : "|linear");
        // Check direct key (e.g. if hash-to-key mapping was lost but texture still alive)
        if (auto cached = textureCache_.find(cacheKey, priority)) {
            contentHashToKey_[contentHash] = cacheKey; // repair the hash map
            return cached;
        }
        std::string const tempPath = "/tmp/embedded_texture_" + contentHash + ".dat";
        auto              texture  = std::make_shared<Texture>(device_, tempPath, srgb);
        auto              stored   = textureCache_.insert(cacheKey, texture, memoryBudget_, priority);
        contentHashToKey_[contentHash] = cacheKey;
        uint32_t const globalIndex = textureManager_->addTexture(stored);
        stored->setGlobalIndex(globalIndex);
        return stored;
    }
    size_t ResourceManager::garbageCollect() {
        size_t removed = textureCache_.garbageCollect();
        removed += modelCache_.garbageCollect();
        return removed;
    }
    size_t ResourceManager::getMemoryUsage() const {
        return textureCache_.memoryUsage() + modelCache_.memoryUsage();
    }
    size_t ResourceManager::getCachedTextureCount() const {
        return textureCache_.cachedCount();
    }
    size_t ResourceManager::getCachedModelCount() const {
        return modelCache_.cachedCount();
    }
    void ResourceManager::clearAll() {
        textureCache_.clear();
        modelCache_.clear();
        contentHashToKey_.clear();
    }
    bool ResourceManager::isTextureCached(const std::string& path) const {
        std::string const srgbKey   = makeTextureKey(path, true);
        std::string const linearKey = makeTextureKey(path, false);
        return textureCache_.anyCached([&](const std::string& k) { return k == srgbKey || k == linearKey; });
    }
    bool ResourceManager::isModelCached(const std::string& path) const {
        // Check all four flag combinations since callers don't know the flags
        for (bool tex : {false, true}) {
            for (bool mat : {false, true}) {
                for (bool morph : {false, true}) {
                    std::string const key = makeModelKey(path, tex, mat, morph);
                    if (modelCache_.anyCached([&](const std::string& k) { return k == key; }))
                        return true;
                }
            }
        }
        return false;
    }
    void ResourceManager::setMemoryBudget(size_t budgetBytes) {
        memoryBudget_ = budgetBytes;
        if (budgetBytes > 0) {
            textureCache_.applyBudget(budgetBytes);
            modelCache_.applyBudget(budgetBytes);
        }
    }
    std::string ResourceManager::computeContentHash(const unsigned char* data, size_t dataSize) {
        uint64_t const     hash = hashBytes(data, dataSize);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << hash;
        return oss.str();
    }
    void ResourceManager::initThreadPool(size_t numThreads) {
        workerThreads_.reserve(numThreads);
        for (size_t i = 0; i < numThreads; ++i) {
            workerThreads_.emplace_back(&ResourceManager::workerThreadLoop, this);
        }
    }
    void ResourceManager::shutdownThreadPool() {
        {
            std::scoped_lock const lock(taskQueueMutex_);
            shutdownThreadPool_ = true;
        }
        taskQueueCV_.notify_all();
        for (auto& thread : workerThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        workerThreads_.clear();
    }
    void ResourceManager::workerThreadLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(taskQueueMutex_);
                taskQueueCV_.wait(lock, [this] { return shutdownThreadPool_ || !taskQueue_.empty(); });
                if (shutdownThreadPool_ && taskQueue_.empty()) {
                    return;
                }
                if (!taskQueue_.empty()) {
                    task = std::move(taskQueue_.front());
                    taskQueue_.pop();
                    activeTasks_++;
                }
            }
            if (task) {
                task();
                activeTasks_--;
            }
        }
    }
    std::future<std::shared_ptr<Texture>> ResourceManager::loadTextureAsync(const std::string& path, bool srgb, ResourcePriority priority) {
        std::string const key = makeTextureKey(path, srgb);
        if (auto cached = textureCache_.find(key, priority)) {
            return makeReadyFuture<std::shared_ptr<Texture>>(cached);
        }
        auto                                  promise = std::make_shared<std::promise<std::shared_ptr<Texture>>>();
        std::future<std::shared_ptr<Texture>> future  = promise->get_future();
        {
            std::scoped_lock const lock(taskQueueMutex_);
            taskQueue_.emplace([this, path, srgb, priority, promise]() {
                try {
                    auto texture = loadTexture(path, srgb, false, priority);
                    promise->set_value(texture);
                } catch (const std::exception& /*e*/) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        taskQueueCV_.notify_one();
        return future;
    }
    std::future<std::shared_ptr<Model>> ResourceManager::loadModelAsync(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets, ResourcePriority priority) {
        std::string const key = makeModelKey(path, enableTextures, loadMaterials, enableMorphTargets);
        if (auto cached = modelCache_.find(key, priority)) {
            return makeReadyFuture<std::shared_ptr<Model>>(cached);
        }
        auto                                promise = std::make_shared<std::promise<std::shared_ptr<Model>>>();
        std::future<std::shared_ptr<Model>> future  = promise->get_future();
        {
            std::scoped_lock const lock(taskQueueMutex_);
            taskQueue_.emplace([this, path, enableTextures, loadMaterials, enableMorphTargets, priority, promise]() {
                try {
                    auto model = loadModel(path, enableTextures, loadMaterials, enableMorphTargets, priority);
                    promise->set_value(model);
                } catch (const std::exception& /*e*/) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        taskQueueCV_.notify_one();
        return future;
    }
    AsyncLoadId ResourceManager::enqueueModelLoad(
        const std::string&                                 path,
        bool                                               enableTextures,
        bool                                               loadMaterials,
        bool                                               enableMorphTargets,
        ResourcePriority                                   priority,
        std::function<void(const std::shared_ptr<Model>&)> onComplete,
        std::function<void(const std::string&)>            onFailed) {
        AsyncLoadId const                          id     = nextAsyncLoadId_.fetch_add(1);
        std::future<std::shared_ptr<Model>>        future = loadModelAsync(path, enableTextures, loadMaterials, enableMorphTargets, priority);
        std::shared_future<std::shared_ptr<Model>> shared = std::move(future).share();
        AsyncModelTaskRecord                       record;
        record.id         = id;
        record.path       = path;
        record.status     = LoadStatus::PENDING;
        record.progress   = 0.0f;
        record.future     = shared;
        record.onComplete = std::move(onComplete);
        record.onFailed   = std::move(onFailed);
        {
            std::scoped_lock const lock(asyncTasksMutex_);
            asyncModelTasks_[id] = std::move(record);
        }
        return id;
    }
    bool ResourceManager::tryGetModelLoadResult(AsyncLoadId id, std::shared_ptr<Model>& outModel, std::string* outError) {
        std::shared_future<std::shared_ptr<Model>> shared;
        {
            std::scoped_lock const lock(asyncTasksMutex_);
            auto                   it = asyncModelTasks_.find(id);
            if (it == asyncModelTasks_.end()) {
                if (outError != nullptr) {
                    *outError = "Invalid async load id";
                }
                return true;
            }
            auto& task = it->second;
            if (task.cancelled) {
                if (outError != nullptr) {
                    *outError = "Cancelled";
                }
                return true;
            }
            if (task.status == LoadStatus::COMPLETE) {
                outModel = task.result;
                if (outError != nullptr) {
                    outError->clear();
                }
                return true;
            }
            if (task.status == LoadStatus::FAILED) {
                if (outError != nullptr) {
                    *outError = task.error;
                }
                return true;
            }
            shared = task.future;
        }
        if (!shared.valid() || shared.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }
        try {
            auto result = shared.get();
            {
                std::scoped_lock const lock(asyncTasksMutex_);
                auto                   it = asyncModelTasks_.find(id);
                if (it != asyncModelTasks_.end()) {
                    it->second.result   = result;
                    it->second.status   = LoadStatus::COMPLETE;
                    it->second.progress = 1.0f;
                }
            }
            outModel = std::move(result);
            if (outError != nullptr) {
                outError->clear();
            }
        } catch (const std::exception& e) {
            std::string error = e.what();
            {
                std::scoped_lock const lock(asyncTasksMutex_);
                auto                   it = asyncModelTasks_.find(id);
                if (it != asyncModelTasks_.end()) {
                    it->second.error    = error;
                    it->second.status   = LoadStatus::FAILED;
                    it->second.progress = 1.0f;
                }
            }
            if (outError != nullptr) {
                *outError = error;
            }
        }
        return true;
    }
    std::vector<AsyncLoadSnapshot> ResourceManager::getAsyncLoadSnapshots() const {
        std::vector<AsyncLoadSnapshot> snapshots;
        std::scoped_lock const         lock(asyncTasksMutex_);
        snapshots.reserve(asyncModelTasks_.size());
        for (const auto& [id, task] : asyncModelTasks_) {
            AsyncLoadSnapshot snap;
            snap.id       = id;
            snap.status   = task.cancelled ? LoadStatus::FAILED : task.status;
            snap.path     = task.path;
            snap.progress = task.progress;
            snap.isModel  = true;
            snap.hasError = !task.error.empty();
            snap.error    = task.error;
            snapshots.push_back(std::move(snap));
        }
        return snapshots;
    }
    void ResourceManager::updateAsyncCallbacks() {
        std::vector<AsyncLoadId> toDispatch;
        {
            std::scoped_lock const lock(asyncTasksMutex_);
            for (auto& [id, task] : asyncModelTasks_) {
                if (task.cancelled) {
                    task.callbackDispatched = true;
                    continue;
                }
                if (task.status == LoadStatus::COMPLETE || task.status == LoadStatus::FAILED) {
                    if (!task.callbackDispatched && (task.onComplete || task.onFailed)) {
                        toDispatch.push_back(id);
                    }
                    continue;
                }
                if (task.status == LoadStatus::PENDING) {
                    task.status   = LoadStatus::LOADING;
                    task.progress = 0.25f;
                }
                if (task.future.valid() && task.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    try {
                        task.result   = task.future.get();
                        task.status   = LoadStatus::COMPLETE;
                        task.progress = 1.0f;
                    } catch (const std::exception& e) {
                        task.error    = e.what();
                        task.status   = LoadStatus::FAILED;
                        task.progress = 1.0f;
                    }
                    if (task.onComplete || task.onFailed) {
                        toDispatch.push_back(id);
                    }
                }
            }
        }
        for (AsyncLoadId id : toDispatch) {
            std::function<void(const std::shared_ptr<Model>&)> onComplete;
            std::function<void(const std::string&)>            onFailed;
            std::shared_ptr<Model>                             result;
            std::string                                        error;
            bool                                               success = false;
            {
                std::scoped_lock const lock(asyncTasksMutex_);
                auto                   it = asyncModelTasks_.find(id);
                if (it == asyncModelTasks_.end() || it->second.callbackDispatched || it->second.cancelled) {
                    continue;
                }
                onComplete                    = it->second.onComplete;
                onFailed                      = it->second.onFailed;
                result                        = it->second.result;
                error                         = it->second.error;
                success                       = (it->second.status == LoadStatus::COMPLETE);
                it->second.callbackDispatched = true;
            }
            if (success) {
                if (onComplete) {
                    onComplete(result);
                }
            } else {
                if (onFailed) {
                    onFailed(error.empty() ? std::string("Async model load failed") : error);
                }
            }
        }
        {
            std::scoped_lock const lock(asyncTasksMutex_);
            for (auto it = asyncModelTasks_.begin(); it != asyncModelTasks_.end();) {
                const auto& task     = it->second;
                const bool  terminal = task.cancelled || task.status == LoadStatus::COMPLETE || task.status == LoadStatus::FAILED;
                if (terminal && task.callbackDispatched) {
                    it = asyncModelTasks_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    void ResourceManager::cancelModelLoad(AsyncLoadId id) {
        std::scoped_lock const lock(asyncTasksMutex_);
        auto                   it = asyncModelTasks_.find(id);
        if (it != asyncModelTasks_.end()) {
            it->second.cancelled = true;
            it->second.status    = LoadStatus::FAILED;
            it->second.progress  = 1.0f;
            if (it->second.error.empty()) {
                it->second.error = "Cancelled";
            }
        }
    }
    size_t ResourceManager::getPendingAsyncLoads() const {
        std::scoped_lock const lock(taskQueueMutex_);
        return taskQueue_.size() + activeTasks_;
    }
    void ResourceManager::waitForAsyncLoads() const {
        while (getPendingAsyncLoads() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}  // namespace engine