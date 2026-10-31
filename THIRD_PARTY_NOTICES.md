# Third-party notices

genesisEmu is MIT licensed (see [LICENSE](LICENSE)). Per D-014, only permissively licensed third-party
code is used or consulted. Dependencies are fetched and built by vcpkg (manifest: [vcpkg.json](vcpkg.json));
none are vendored in this repository yet.

| Component | Used by | License | Upstream |
|-----------|---------|---------|----------|
| SDL3 | `gen_app` (window, input, audio, file dialogs) | Zlib | <https://github.com/libsdl-org/SDL> |
| Dear ImGui (docking) | `gen_app` (UI) | MIT | <https://github.com/ocornut/imgui> |
| spdlog | `gen_common` (logging) | MIT | <https://github.com/gabime/spdlog> |
| {fmt} (via spdlog) | `gen_common` | MIT | <https://github.com/fmtlib/fmt> |
| nlohmann/json | `gen_common` (configuration) | MIT | <https://github.com/nlohmann/json> |
| Catch2 | tests only (not distributed) | BSL-1.0 | <https://github.com/catchorg/Catch2> |

Dependencies listed in the architecture spec but not yet used (SQLite, libcurl, xxHash, ImPlot, ymfm) will
be added here when the milestone that needs them lands.

The full license texts of the vcpkg-built dependencies are installed by vcpkg under
`vcpkg_installed/<triplet>/share/<port>/copyright` and must be shipped alongside binary distributions.

## Data sources

The known-ROM table (`src/core/cart/known_roms.cpp`) lists SHA-1/CRC32 values from the No-Intro
Sega Mega Drive / Genesis DAT (as mirrored by libretro-database). It contains hashes only, no ROM data.

## Downloaded for testing (never committed or distributed)

`tools/fetch-test-data.py` downloads these into the ignored `test-data/` folder, pinned in
`tools/test-data.lock.json`:

| Item | Use | License | Source |
|------|-----|---------|--------|
| SingleStepTests 680x0 (68000 v1 vectors) | CPU test suite | none stated in the repository | <https://github.com/SingleStepTests/680x0> |
| The Macro Assembler AS 1.42 build 315 (Windows binaries) | disassembler round-trip test | GPL-2.0 | <http://john.ccac.rwth-aachen.de:8000/as/> |
