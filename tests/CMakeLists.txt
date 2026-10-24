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

add_subdirectory(cli)
if(GENESIS_BUILD_APP)
  add_subdirectory(app)
endif()
