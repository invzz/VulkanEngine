# REFACTOR PLAN — Updates & Immediate Actions

## Recent findings (from latest test run)
- Ran tests: **73 total** — **57 passed**, **16 skipped**.
- Validation-layer messages observed (informational but actionable):
  - Object-tracking warnings during `vkDestroyDevice()` (child objects like VkImage/VkBuffer/VkDeviceMemory/VkImageView/VkSampler not destroyed prior to device destroy).
  - Descriptor usage warnings: descriptor set used in draw/dispatch but not updated.
  - Image/layout warnings for copy/present operations (unexpected src/dst layouts).
- Fix implemented: moved destruction of `threadLocalCommandPools_` to occur before destroying the logical device in `Device::~Device()` to prevent `vkDestroyCommandPool` being called with an invalid device. Added `Device.ThreadLocalPoolsDestroyedBeforeDevice` unit test reproducing the previous crash; test now passes. 

## Action checklist (short-term)
- [ ] Add unit tests reproducing object-tracking warnings for a chosen resource (Buffer or Image); fix teardown ordering.
- [ ] Add unit tests to assert descriptors are updated before draw/dispatch; add defensive assertions.
- [ ] Add integration tests for layout-transition-heavy paths (copyImageToBuffer, present) to detect layout misuse early.
- [ ] Create an issue labeled `validation-layers` with entries for each observed message and the tests reproducing them.
- [ ] Add unit tests covering `DebugMessenger` RAII behavior (create/reset/destruct).
- [ ] Open draft PR for DebugMessenger RAII pilot (include tests, changelog note, and PR checklist).

## PR checklist (for small pilots)
- Branch: `refactor/<topic>` (e.g., `refactor/debug-messenger-raii`)
- Commits: implementation | tests | changelog/REFACTOR_PLAN update
- Verify: local build + full test suite run green
- PR meta: short description, checklist, reviewers (Device/Graphics), link to `validation-layers` issue if applicable

---

*This updates file summarizes the immediate follow-ups from the last test run and provides a short checklist for pilot PRs.*
