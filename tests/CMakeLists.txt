find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(gen_unit_tests
  unit/skeleton_test.cpp
)
target_link_libraries(gen_unit_tests PRIVATE gen_analysis gen_common Catch2::Catch2WithMain)
genesis_set_warnings(gen_unit_tests)

catch_discover_tests(gen_unit_tests DISCOVERY_MODE PRE_TEST)
