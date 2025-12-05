# Plan: System & Asset Management Improvements

This plan outlines the steps to improve System Management and Asset Management, building upon the ECS transition.

## Part 1: System Management

**Goal:** Decouple systems from the `App` class and automate their lifecycle and dependency injection.

### 1. Create `SystemManager` (or `World`)
*   **Location:** `include/Engine/Core/SystemManager.hpp`
*   **Responsibility:**
    *   Store and manage a list of active systems.
    *   Handle system initialization (`init()`), updates (`update(dt)`), and cleanup (`shutdown()`).
    *   Provide a way to register systems.

### 2. Define `ISystem` Interface
*   **Location:** `include/Engine/Core/ISystem.hpp`
*   **Interface:**
    ```cpp
    class ISystem {
    public:
        virtual ~ISystem() = default;
        virtual void init(entt::registry& registry) = 0;
        virtual void update(entt::registry& registry, float dt) = 0;
        // Optional: render(), fixedUpdate()
    };
    ```

### 3. Refactor Existing Systems
*   **Current:** Systems are standalone classes instantiated in `App::run`.
*   **New:** Inherit from `ISystem`.
*   **Example (`MeshRenderSystem`):**
    *   Implement `init` to setup pipelines (if needed).
    *   Implement `update` (or `render`) to iterate the registry and record commands.
    *   Remove manual dependency passing in constructors where possible (use the registry to find what is needed, or a `ServiceLocator` for global services like `Device`).

### 4. Implement Automatic Injection (Service Locator)
*   **Problem:** Systems need access to `Device`, `Window`, `Input`, etc.
*   **Solution:** A simple `ServiceLocator` or `Context` struct passed to `init`.
    ```cpp
    struct SystemContext {
        Device& device;
        Window& window;
        ResourceManager& resourceManager;
        // ...
    };
    ```
*   Update `ISystem::init` to take `SystemContext&`.

### 5. Update `App` Class
*   **Current:** Manually creates `ObjectSelectionSystem`, `InputSystem`, etc.
*   **New:**
    ```cpp
    SystemManager systemManager;
    systemManager.register<InputSystem>();
    systemManager.register<MeshRenderSystem>();
    // ...
    systemManager.init(context, registry);
    
    // In game loop:
    systemManager.update(registry, dt);
    ```

## Part 2: Asset Management

**Goal:** Move from raw pointers/shared_ptr to a handle-based system for better lifetime management and async loading.

### 1. Define `AssetHandle`
*   **Location:** `include/Engine/Resources/AssetHandle.hpp`
*   **Structure:** A simple wrapper around a `UUID` or `uint32_t` ID.
    ```cpp
    struct AssetHandle {
        uint32_t id = 0;
        bool isValid() const { return id != 0; }
    };
    ```

### 2. Refactor `ResourceManager`
*   **Current:** Returns `std::shared_ptr<T>`.
*   **New:**
    *   Store assets internally in a map: `std::unordered_map<AssetHandle, std::shared_ptr<T>>`.
    *   `loadTexture(path)` returns `AssetHandle`.
    *   `getTexture(handle)` returns `T*` or `std::shared_ptr<T>`.
    *   Implement a "missing" asset fallback (e.g., a bright pink texture) if the handle is invalid or loading.

### 3. Implement Async Loading
*   **Current:** `ResourceManager` has `AsyncLoadHandle` but `loadTexture` seems synchronous (or mixed).
*   **Improvement:**
    *   Create a `WorkerThread` or `JobSystem` for loading.
    *   `loadTextureAsync(path)` returns a handle immediately.
    *   The asset is marked as "Loading".
    *   `getTexture(handle)` returns the fallback asset until loading is complete.
    *   Once loaded, the main thread uploads to GPU (Vulkan requires main thread or careful synchronization for command buffers).

### 4. Update Components
*   **Current:** `ModelComponent` (planned) will hold `std::shared_ptr<Model>`.
*   **New:** `ModelComponent` holds `AssetHandle modelHandle`.
*   Systems use `ResourceManager::get(handle)` to access the data during rendering.

## Execution Order

1.  **System Manager:** Implement `ISystem` and `SystemManager`.
2.  **Refactor Systems:** Port one system (e.g., `LODSystem`) to the new architecture to test. Then port the rest.
3.  **Asset Handles:** Create `AssetHandle` struct.
4.  **ResourceManager Update:** Modify `ResourceManager` to map Handles to Resources.
5.  **Async Loader:** Implement the background loading thread.
