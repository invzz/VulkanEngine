Perfect! The build now succeeds with your change — nice work. My next step is to implement runtime manifest parsing and load/apply EXR lightmaps per-material.## Status ✅
Great — the project builds now after adding tinyexr to the Engine target. Next I’ll implement runtime support to parse the per-model manifest and load/apply EXR lightmaps to materials.

Plan — quick runtime mapping (short-term) 🔧
Parse <model>_mesh_lightmaps.json next to the model file when loading a model.
For each mesh entry in the manifest:
Choose a single representative tile for now (policy below).
Load the EXR with Texture::createFromEXR(...) (linear float RGBA).
Register the texture with TextureManager (bindless index) and set PBRMaterial::lightmap.
Set the lightmap bit in material flags (we already added bit 13 and store index in MaterialUniformData.indices3.z).
Add a shader helper to sample globalTextures[nonuniformEXT(indices3.z)] when the flag is set, and combine the baked map with the computed lighting.
Decisions / Questions (needed to proceed) ❓
For meshes that produced multiple chunk files, which strategy do you prefer?
(A) Prefer a full-mesh tile (chunk -1_-1) if present, otherwise pick the first listed chunk (quick and simple).
(B) Assemble tiles into an atlas at load-time (higher-quality, more work).
How should the baked lightmap be applied in shading?
(1) Multiplicative irradiance: outLit *= lightmap (recommended)
(2) Additive: outLit += lightmap
(3) Other (describe)
Next step ✨
Once you confirm the choices above I'll:

Add manifest parsing and EXR loading in ResourceManager::loadModel.
Wire PBRMaterial::lightmap into MaterialRenderBindings (we already added storage/flag support).
Add shader sampling (small change to material_decode.glsl / main lighting shader) and test visually.
Would you like me to proceed with option (A) + multiplicative application (recommended)?

