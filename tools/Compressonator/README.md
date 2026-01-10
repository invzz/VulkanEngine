Compressonator (packaged CLI)

What’s kept here
- compressonatorcli (wrapper script)
- compressonatorcli-bin (actual ELF binary) — now located under `bin/`
- `pkglibs/` — runtime libraries supplied by the vendor (kept for portability)
- `qt.conf` — configuration for bundled Qt
- `license/` — license text(s) supplied with the binary

Why we keep it in-tree
- Makes BC6H test runs reproducible on developer machines and in CI jobs (optional).
- Avoids painful system dependency installs on ephemeral runners.

How to use locally
- Run using the wrapper so LIB paths are configured:
  ./tools/Compressonator/compressonatorcli <args>

How to use in our build/tests
- The build system (xmake) can inject a compile-time define `COMPRESSONATOR_CLI="/abs/path/to/tools/Compressonator/compressonatorcli"` so tests are deterministically enabled.
- Alternatively, when running `ModelLightBaker`/tests locally, set COMPRESSONATOR_CLI environment or ensure `tools/Compressonator/compressonatorcli` exists and is executable.

Verification and provenance
- We provide a `CHECKSUMS` file with SHA256 for the binary and a small `verify_compressonator.sh` helper that verifies the checksum and presence of `license/`.
- If you replaced the binary, please update `CHECKSUMS` and record the source/version and checksum in `tools/Compressonator/README.md`.

Notes
- We pruned the shipped documentation and example images (to save repo size); if you need local docs, fetch them from the vendor.
- `pkglibs/` is intentionally kept to increase portability; do not remove it without confirming runtime behavior on CI / developer VMs.

License
- See `tools/Compressonator/license/` for vendor license details. If license terms are not acceptable, remove the binary and update tests to skip BC6H.
