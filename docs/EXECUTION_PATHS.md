# VulkanEngine Execution Paths

This document turns the project improvement review into concrete delivery paths.

## Goal
Ship improvements in 90 days without breaking editor usability or runtime stability.

## Path Structure
1. Phase 1: Foundation and Safety (Days 1-30)
2. Phase 2: Performance and Robustness (Days 31-60)
3. Phase 3: Architecture and Scale (Days 61-90)

## Current Status
- Phase 1: Done (April 9, 2026)
- Phase 2: Active
- Phase 3: Active (started April 9, 2026)

## Success Metrics
- Zero validation errors during resize stress test.
- No device-lost event in 30 minutes of editor interaction.
- Frame-time variance reduced to less than 1.0 ms in test scene.
- Cold model import no longer blocks render thread for more than 16 ms.

## Documents
- [Phase 1 Execution Path](execution-paths/phase-1-foundation-safety.md)
- [Phase 2 Execution Path](execution-paths/phase-2-performance-robustness.md)
- [Phase 3 Execution Path](execution-paths/phase-3-architecture-scale.md)

## Working Rules
- Every task must include file targets, acceptance criteria, and rollback notes.
- Keep validation enabled in debug path and disabled by default in release path.
- Run xmake after each milestone and run Editor smoke checks.
- Add tests before refactors when possible.

## Delivery Cadence
- Weekly milestone PRs, each with benchmark notes and risk updates.
- End-of-phase report in changelog with metrics delta.
