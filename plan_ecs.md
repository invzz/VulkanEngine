# Plan: Transition to Data-Oriented ECS

This plan outlines the steps to migrate the current `GameObject` / `Component` architecture to a Data-Oriented Entity Component System (ECS) using **EnTT**.

## Phase 1: Preparation & Dependencies

1.  **Add EnTT Dependency**
    *   Update `xmake.lua` to include `add_requires("entt")` and `add_packages("entt")`.
    *   Verify build configuration.

2.  **Create ECS Core**
    *   Create `include/Engine/Scene/Scene.hpp` (or `World.hpp`) to replace or augment `GameObjectManager`.
    *   This class will hold the `entt::registry`.
    *   Define `using Entity = entt::entity;`.

## Phase 2: Component Migration

The goal is to ensure all components are POD (Plain Old Data) or simple structs, decoupled from the `GameObject` class.

1.  **Analyze Existing Components**
    *   `TransformComponent`: Already a struct. Good.
    *   `PointLightComponent`, `DirectionalLightComponent`, `SpotLightComponent`: Check if they inherit from `Component`. If so, remove inheritance.
    *   `LODComponent`: Check inheritance.
    *   `Model`: Currently a `std::shared_ptr` in `GameObject`. Create a `MeshComponent` or `ModelComponent` struct that holds this handle.
    *   `PBRMaterial`: Create a `MaterialComponent`.

2.  **Refactor Components**
    *   Remove `Component` base class inheritance from all components.
    *   Remove `owner` pointer from components.
    *   Ensure components have no logic that depends on `GameObject`.

## Phase 3: GameObject & Scene Refactoring

1.  **Refactor `GameObject`**
    *   Change `GameObject` to be a lightweight wrapper around `entt::entity` and `Scene*` (or `Registry*`).
    *   Remove `std::vector<std::unique_ptr<Component>>`.
    *   Remove hardcoded members like `transform` and `model`.
    *   Update `addComponent<T>` to call `registry.emplace<T>(entity, ...)` or `registry.emplace_or_replace<T>(entity, ...)`.
    *   Update `getComponent<T>` to call `registry.get<T>(entity)`.
    *   Update `hasComponent<T>` to call `registry.all_of<T>(entity)`.

2.  **Refactor `GameObjectManager` -> `Scene`**
    *   `GameObjectManager` currently maintains separate vectors (`pbrObjects_`, `pointLights_`, etc.).
    *   Replace this with `entt::registry`. The registry *is* the storage.
    *   Remove manual categorization logic (`categorizeObject`).
    *   Implement `createEntity()` which returns a `GameObject` (wrapper).
    *   Implement `destroyEntity(GameObject obj)`.

## Phase 4: System Refactoring

Systems currently iterate over `std::vector<GameObject*>`. They must be updated to iterate over component views.

1.  **Update `MeshRenderSystem`**
    *   Old: Iterate `pbrObjects`.
    *   New: `auto view = registry.view<TransformComponent, ModelComponent>();`
    *   Iterate view to get transform and model for rendering.

2.  **Update Light Systems**
    *   Old: Iterate `pointLights_`, etc.
    *   New: `auto view = registry.view<TransformComponent, PointLightComponent>();`

3.  **Update `SceneSerializer`**
    *   Update serialization logic to iterate the registry using `registry.each()` or `registry.storage<T>()` to save all entities and their components.
    *   Update deserialization to create entities in the registry.

## Phase 5: Cleanup

1.  **Remove Legacy Code**
    *   Delete `Component` base class.
    *   Remove `GameObjectManager` if fully replaced by `Scene`.
    *   Remove `dynamic_cast` usages.

2.  **Testing**
    *   Verify `Cube` demo still works.
    *   Check performance (though likely not noticeable with small scene, code structure is the goal).

## Execution Order

1.  **Step 1:** Add EnTT to `xmake.lua`.
2.  **Step 2:** Create `Scene` class with `entt::registry`.
3.  **Step 3:** Refactor `TransformComponent` (remove any legacy dependencies if any) and create `ModelComponent`.
4.  **Step 4:** Modify `GameObject` to wrap `entt::entity`.
5.  **Step 5:** Update `MeshRenderSystem` to use the registry view.
6.  **Step 6:** Update `App` to use `Scene` instead of `GameObjectManager`.
