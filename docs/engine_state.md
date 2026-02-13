# EngineState — single source of truth

This project now centralizes most runtime state in `EngineState`.

## Purpose

- Own systems (render, animation, lighting, shadow, IBL, etc.) and subsystem resources (descriptor pools, per-frame descriptor sets, skybox, settings).
- Provide a single pointer/reference (`EngineState*`) that render passes, UI panels and systems can use to access runtime data without long parameter lists.
- Improve testability and make serialization / snapshotting easier by keeping runtime state in one place.

## Responsibilities

- System creation and initialization (`initialize()`)
- Descriptor pools, set layouts and per-frame descriptor sets
- Scene storage (`scene`), selection and camera entity tracking
- UI managers and ImGui state
- Settings (sky, dust, fog, HZB, shadow, post-process)

## Migration notes

- `App` is now an orchestrator: it creates `Window`, `Device`, `Renderer` and calls `engineState.initialize(...)`.
- Render passes and many UI panels accept `EngineState*` instead of receiving many independent pointers.
- Prefer `engineState` for accessing scene, systems and settings.

## Tip

- Keep mutable runtime data inside `EngineState`. Pass const references where possible for read-only access in concurrent contexts.
