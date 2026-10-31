find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# Synthetic ROM builders shared by the unit tests and the CLI test fixture.
add_library(gen_test_support STATIC
  support/boot_rom.cpp
  support/synthetic_rom.cpp
)
target_include_directories(gen_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gen_test_support PUBLIC gen_core)
genesis_set_warnings(gen_test_support)

add_executable(gen_unit_tests
  unit/audio_test.cpp
  unit/bus_test.cpp
  unit/cart_device_test.cpp
  unit/config_test.cpp
  unit/disasm68k_test.cpp
  unit/disasmz80_test.cpp
  unit/hash_test.cpp
  unit/input_script_test.cpp
  unit/io_test.cpp
  unit/logging_test.cpp
  unit/machine_test.cpp
  unit/m68k_test.cpp
  unit/paths_test.cpp
  unit/rom_test.cpp
  unit/skeleton_test.cpp
  unit/png_test.cpp
  unit/vdp_render_test.cpp
  unit/vdp_test.cpp
  unit/z80_system_test.cpp
)
target_include_directories(gen_unit_tests PRIVATE unit)
target_link_libraries(gen_unit_tests PRIVATE gen_analysis gen_common gen_test_support Catch2::Catch2WithMain)
genesis_set_warnings(gen_unit_tests)
catch_discover_tests(gen_unit_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS unit)

# Tests against the user's commercial ROMs (GENESIS_ROM_DIR); they report "skipped" when it is unset.
add_executable(gen_rom_tests
  roms/boot_test.cpp
  roms/frame_test.cpp
  roms/rom_scan.cpp
  roms/rom_identify_test.cpp
  roms/script_test.cpp
)
target_link_libraries(gen_rom_tests PRIVATE gen_common Catch2::Catch2WithMain)
target_compile_definitions(gen_rom_tests PRIVATE GENESIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
genesis_set_warnings(gen_rom_tests)
catch_discover_tests(gen_rom_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS rom SKIP_RETURN_CODE 4)

# SingleStepTests 68000 runner (M01-02). The vectors are fetched by tools/fetch-test-data.py into test-data/;
# without them the tests report "skipped".
add_executable(gen_m68k_sst m68k/sst_runner.cpp)
target_link_libraries(gen_m68k_sst PRIVATE gen_common gen_core)
target_compile_definitions(gen_m68k_sst PRIVATE GENESIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
genesis_set_warnings(gen_m68k_sst)
set(GENESIS_SST_EXCLUSIONS "${CMAKE_CURRENT_SOURCE_DIR}/m68k/sst_exclusions.txt")
# Per push: the first 250 vectors of every file. Nightly: GENESIS_SST_LIMIT=0 runs all ~1M.
set(GENESIS_SST_LIMIT 250 CACHE STRING "Vectors per SingleStepTests file in the m68k.sst test (0 = all)")
add_test(NAME m68k.sst COMMAND gen_m68k_sst --limit ${GENESIS_SST_LIMIT} --show 0
  --exclusions "${GENESIS_SST_EXCLUSIONS}")
add_test(NAME m68k.sst_selftest COMMAND gen_m68k_sst --only NOP --limit 50 --show 0 --inject-fault --expect-fail)
set_tests_properties(m68k.sst m68k.sst_selftest PROPERTIES LABELS m68k SKIP_RETURN_CODE 4)

# SingleStepTests Z80 runner (M04-04). Per push the Debug job runs the first 100 vectors of every opcode file;
# Release runs all 1.6M (a few seconds).
add_executable(gen_z80_sst z80/ssz_runner.cpp)
target_link_libraries(gen_z80_sst PRIVATE gen_core)
target_compile_definitions(gen_z80_sst PRIVATE GENESIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
genesis_set_warnings(gen_z80_sst)
set(GENESIS_Z80_SST_LIMIT 100 CACHE STRING "Vectors per SingleStepTests Z80 file in the z80.sst test (0 = all)")
add_test(NAME z80.sst COMMAND gen_z80_sst --limit ${GENESIS_Z80_SST_LIMIT} --show 3)
add_test(NAME z80.sst_selftest COMMAND gen_z80_sst --only 00 --limit 20 --show 0 --inject-fault --expect-fail)
set_tests_properties(z80.sst z80.sst_selftest PROPERTIES LABELS z80 SKIP_RETURN_CODE 4)

# ZEXDOC / ZEXALL through a CP/M BDOS shim (M04-04). They take tens of seconds, so they run in the nightly
# workflow (label z80-zex) rather than per push.
add_executable(gen_z80_zex z80/zex_runner.cpp)
target_link_libraries(gen_z80_zex PRIVATE gen_core)
target_compile_definitions(gen_z80_zex PRIVATE GENESIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
genesis_set_warnings(gen_z80_zex)
add_test(NAME z80.zexdoc COMMAND gen_z80_zex zexdoc --quiet)
add_test(NAME z80.zexall COMMAND gen_z80_zex zexall --quiet)
set_tests_properties(z80.zexdoc z80.zexall PROPERTIES LABELS z80-zex SKIP_RETURN_CODE 4 TIMEOUT 900)

# 68000 throughput microbenchmark (M01-11); run by hand: gen_m68k_bench [millions-of-instructions]
add_executable(gen_m68k_bench bench/m68k_bench.cpp)
target_link_libraries(gen_m68k_bench PRIVATE gen_core)
genesis_set_warnings(gen_m68k_bench)

# Disassembler round trip through AS (M01-12); "skipped" when AS has not been fetched.
add_executable(gen_disasm_roundtrip analysis/disasm_roundtrip.cpp)
target_link_libraries(gen_disasm_roundtrip PRIVATE gen_analysis gen_common)
target_compile_definitions(gen_disasm_roundtrip PRIVATE GENESIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
genesis_set_warnings(gen_disasm_roundtrip)
add_test(NAME analysis.disasm_roundtrip COMMAND gen_disasm_roundtrip)
set_tests_properties(analysis.disasm_roundtrip PROPERTIES LABELS analysis SKIP_RETURN_CODE 4)

# Z80 disassembler round trip through AS (M04-06).
add_executable(gen_disasmz80_roundtrip analysis/disasmz80_roundtrip.cpp)
target_link_libraries(gen_disasmz80_roundtrip PRIVATE gen_analysis gen_common)
target_compile_definitions(gen_disasmz80_roundtrip PRIVATE GENESIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
genesis_set_warnings(gen_disasmz80_roundtrip)
add_test(NAME analysis.disasmz80_roundtrip COMMAND gen_disasmz80_roundtrip)
set_tests_properties(analysis.disasmz80_roundtrip PROPERTIES LABELS analysis SKIP_RETURN_CODE 4)

add_subdirectory(cli)
if(GENESIS_BUILD_APP)
  add_subdirectory(app)
endif()
