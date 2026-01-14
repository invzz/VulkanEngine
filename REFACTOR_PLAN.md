# REFACTOR PLAN: Graphics / Device Migration

> Concise migration RFC and prioritized plan to improve the Device/Graphics stack: safety, maintainability, and testability.

## Summary 🎯
This plan proposes a sequence of focused changes to the graphics stack (Device, SwapChain, DeviceMemory, descriptor helpers and related systems). It aims to remove UB, reduce duplication, adopt safer RAII patterns, and incrementally split responsibilities to make the codebase easier to maintain and evolve.

---

## Goals
- Eliminate undefined behaviour from uninitialized Vulkan handles and unsafe destructors.
- Remove duplicated extension/feature-checking logic and centralize helpers.
- Improve resource lifetime and exception-safety using RAII patterns or commit-style initialization.
- Reduce cognitive complexity and surface area of `Device` by splitting duties incrementally.
- Keep performance conservative: benchmark before large behavior changes.

---

## Scope
- `include/Engine/Graphics/Device.hpp`, `src/Engine/Graphics/Device.cpp`
- `include/Engine/Graphics/SwapChain.hpp`, `src/Engine/Graphics/SwapChain.cpp`
- `include/Engine/Graphics/DeviceMemory.hpp`, `src/Engine/Graphics/DeviceMemory.cpp`
- Descriptor helpers and systems that construct pipelines/descriptors (various `src/Engine/Systems/*` and tools)
- Unit & integration tests under `tests/` and important tools in `tools/`

---

## Prioritized Changes (short form)

### 1) Immediate (low-risk, small PRs)
- Initialize all Vulkan handles to `VK_NULL_HANDLE` in headers.
- Harden `Device::~Device()` with null checks and swallow/catch errors while logging (do not throw from destructor).
- Rename `hasGflwRequiredInstanceExtensions` → `hasGlfwRequiredInstanceExtensions` and make it return `bool`.
- Add shared helpers for extension enumeration & verification:
  - `enumerateInstanceExtensions()`
  - `enumerateDeviceExtensions(VkPhysicalDevice)`
  - `ensureExtensionsPresent(required, available)`

**Why**: immediate safety wins, minimal risk, easy to review.

### 2) Short-term (low-to-medium risk)
- Replace duplicated extension-checking with the new helpers across the codebase.
- Standardize descriptor pool creation; prefer `DescriptorPool::Builder` in systems.
- Add unit tests for extension helpers and destructor invariants.

### 3) Medium-term (medium risk)
- Introduce small RAII wrappers for Instance, Device, CommandPool (incremental, one resource at a time).
- Prototype `thread_local` per-thread command pools (or a pool manager) to replace per-call temp pools for single-time commands; benchmark thoroughly.

### 4) Long-term (higher risk)
- Split `Device` into smaller, well-scoped classes (InstanceManager, PhysicalDeviceSelector, LogicalDevice, CommandPoolManager), migrating users in small PRs.
- Migrate to consistent RAII/deleter conventions across the engine.

---

## Migration PR Strategy
Make each change small and gated by tests and CI. Example PR progression:

- **PR-1 (Trivial)**: initialize handles + destructor guards. (1–2 hours)
- **PR-2 (Small)**: rename function, add extension helpers & tests. (2–4 hours)
- **PR-3 (Small)**: replace extension checks + unification. (2–4 hours)
- **PR-4 (Small)**: standardize descriptor pool usage across systems. (2–6 hours)
- **PR-5 (Pilot RAII)**: RAII wrapper for Instance & DebugMessenger. (0.5–1 day)
- **PR-6 (Pilot pool)**: per-thread pool and benchmarking (1–2 days)
- **PR-7 (Refactor Device split)**: staged migration into smaller classes (multi-PR, multi-day)

Each PR should:
- Build cleanly and pass existing unit/integration tests.
- Include tests for the new helpers or invariants when applicable.
- Contain a short migration note describing user-facing changes (if any).

---

## Risk Matrix & Mitigations
- **Leak/regression risk**: do small PRs with tests simulating partial failures and run CI.
- **Performance risk (pool changes)**: keep old behavior behind a flag and benchmark pilot implementation before switchover.
- **Ordering bugs after split**: maintain API compatibility during transition; add integration tests covering lifecycle.

---

## Testing Plan
- Unit tests for extension helpers and resource-destructor invariants.
- Integration smoke tests (SwapChain, Render systems, light-baking tests where relevant).
- Microbenchmarks for single-time command path pre/post per-thread pool pilot.

---

## Acceptance Criteria
- No UB resulting from uninitialized handles or throwing destructors after PR-1.
- Extension-check duplication removed with a single helper set (PR-2/3).
- Tests pass in CI for each PR; smoke render tests for PRs touching runtime behavior.

---

## Timeline & Estimates
- PR-1: 1–2 hours
- PR-2: 2–4 hours
- PR-3: 2–4 hours
- PR-4: 2–6 hours
- PR-5: 0.5–1 day
- PR-6: 1–2 days
- PR-7: multiple small PRs (varies)

---

## Questions / Next Steps
- Approve and I will create PR-1 (initializers + destructor guards) and run the build + tests.
- Or, if you prefer, I can prepare the patch branch and open the RFC PR for review first.

---

*Document generated by GitHub Copilot (Raptor mini (Preview)) — concise refactor plan for the Engine graphics stack.*
