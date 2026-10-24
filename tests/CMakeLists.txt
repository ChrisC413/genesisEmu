find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# Synthetic ROM builders shared by the unit tests and the CLI test fixture.
add_library(gen_test_support STATIC
  support/synthetic_rom.cpp
)
target_include_directories(gen_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gen_test_support PUBLIC gen_core)
genesis_set_warnings(gen_test_support)

add_executable(gen_unit_tests
  unit/config_test.cpp
  unit/hash_test.cpp
  unit/logging_test.cpp
  unit/m68k_test.cpp
  unit/paths_test.cpp
  unit/rom_test.cpp
  unit/skeleton_test.cpp
)
target_include_directories(gen_unit_tests PRIVATE unit)
target_link_libraries(gen_unit_tests PRIVATE gen_analysis gen_common gen_test_support Catch2::Catch2WithMain)
genesis_set_warnings(gen_unit_tests)
catch_discover_tests(gen_unit_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS unit)

# Tests against the user's commercial ROMs (GENESIS_ROM_DIR); they report "skipped" when it is unset.
add_executable(gen_rom_tests
  roms/rom_identify_test.cpp
)
target_link_libraries(gen_rom_tests PRIVATE gen_common Catch2::Catch2WithMain)
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

# 68000 throughput microbenchmark (M01-11); run by hand: gen_m68k_bench [millions-of-instructions]
add_executable(gen_m68k_bench bench/m68k_bench.cpp)
target_link_libraries(gen_m68k_bench PRIVATE gen_core)
genesis_set_warnings(gen_m68k_bench)

add_subdirectory(cli)
if(GENESIS_BUILD_APP)
  add_subdirectory(app)
endif()
