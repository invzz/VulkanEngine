# Clean Architecture (Target State)

## Layer Model

Delivery (Editor, tools, app bootstrap)
-> Application (use cases, orchestration)
-> Domain (scene/entities/components/rules)

Application
-> Ports (interfaces)
<- Infrastructure (Vulkan, filesystem, serializers, physics adapters)

Runtime systems should consume Domain models directly where possible and only cross infrastructure through ports.

## Current Mapping

- Domain: include/Engine/Scene, include/Engine/Core (domain-safe parts), component/value objects.
- Application: include/Engine/Application, src/Engine/Application.
- Ports: include/Engine/Application/Ports.
- Infrastructure: src/Engine/Graphics, src/EngineSceneIO, src/ModelLib and concrete adapters in include/Editor/Infrastructure + src/Editor/Infrastructure.
- Delivery: src/Editor, src/tools.

## Boundary Rules

- Domain must not include Editor, Vulkan, filesystem, or serializer implementations.
- Application depends on Domain + Ports only.
- Infrastructure implements Ports and may depend on external frameworks.
- Delivery composes concrete Infrastructure into Application use cases.

## Next Refactor Slices

1. Add ports for input/render commands where editor currently touches runtime systems directly.
2. Split EngineState into narrower state services (render/input/scene runtime). (in progress)
3. Add Infrastructure-side boundary tests for adapter and external-dependency placement.
4. Migrate SceneSerializer runtime settings bindings behind Application workflows.
