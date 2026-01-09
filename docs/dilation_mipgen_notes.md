# Dilation & Mip Generation — Implementation Notes

Summary
-------
- We implement a CPU reference for seam-aware dilation and mip generation that respects a per-texel validity mask.
- Purpose: ensure padding regions are filled without introducing seams and generate mips that average only valid texels.

Dilation (CPU reference)
------------------------
- Algorithm: iterative 4-neighbor dilation. In each iteration, invalid texels with at least one valid 4-neighbor become valid and take the average radiance of their valid neighbors.
- Pros: simple, deterministic, easy to test.
- Cons: may be slower than a GPU implementation for large atlases; we will add an optimized GPU path later if needed.
- API: `engine::lightmap::dilateBakeTexels(const BakeTexel* src, BakeTexel* dst, int width, int height, int iterations)`
- Complexity: O(iterations * width * height).

Mip Generation
--------------
- Approach: for each 2x2 block, average only the valid texels. If no valid contributors, mark destination invalid.
- API: `engine::lightmap::generateMipLevel(const BakeTexel* src, int srcW, int srcH, BakeTexel* dst)`
- Notes: we ensure odd dimensions are handled by ignoring out-of-bounds contributors; dst resolution is `max(1, floor(srcW/2))` x `max(1, floor(srcH/2))`.

Testing
-------
- Unit tests added that verify zero-iteration behavior, odd-dimension handling, averaging from multiple neighbors, and full integration (dilate then mip).
- Tests are in `tests/dilate_tests.cpp`, `tests/mipgen_tests.cpp`, and `tests/dilate_mip_integration_tests.cpp`.

Next steps
----------
- Add a GPU-accelerated dilation & mipgen (compute shader) with validation against the CPU reference.
- Add a CI smoke test that runs a small bake (tiny atlas) end-to-end (bake -> dilate -> mipgen -> write VTEX) and verifies expected manifest fields.
