# Smoke tests for the desktop app: open a window, load a synthetic ROM, render frames, self-check, exit.
# They exit with 77 (reported as skipped) where no OpenGL 3.3 context is available, e.g. on CI runners.

set(home "${CMAKE_CURRENT_BINARY_DIR}/home")
set(rom "${CMAKE_BINARY_DIR}/tests/cli/roms/synthetic.bin")

add_test(NAME app.clean_home COMMAND "${CMAKE_COMMAND}" -E rm -rf "${home}")
set_tests_properties(app.clean_home PROPERTIES FIXTURES_SETUP app_home LABELS gui)

add_test(NAME app.smoke_first_run COMMAND gen_app --smoke-test=30 "${rom}")
set_tests_properties(app.smoke_first_run PROPERTIES
  ENVIRONMENT "GENESISEMU_HOME=${home}"
  FIXTURES_REQUIRED "app_home;cli_roms"
  FIXTURES_SETUP app_layout
  SKIP_RETURN_CODE 77
  LABELS gui)

add_test(NAME app.smoke_layout_restored
  COMMAND gen_app --smoke-test=30 --expect-restored-layout "--screenshot=${CMAKE_CURRENT_BINARY_DIR}/screenshot.png"
          "${rom}")
set_tests_properties(app.smoke_layout_restored PROPERTIES
  ENVIRONMENT "GENESISEMU_HOME=${home}"
  FIXTURES_REQUIRED "app_layout;cli_roms"
  SKIP_RETURN_CODE 77
  LABELS gui)
