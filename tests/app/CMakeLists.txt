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

# M06-00a: boot straight into the save state cli.run_save_state writes (frame 10 of the boot ROM).
add_test(NAME app.smoke_state
  COMMAND gen_app --smoke-test=10 "--state=${CMAKE_BINARY_DIR}/tests/cli/boot10.gst"
          "${CMAKE_BINARY_DIR}/tests/cli/roms/boot.bin")
set_tests_properties(app.smoke_state PROPERTIES
  ENVIRONMENT "GENESISEMU_HOME=${home}"
  FIXTURES_REQUIRED "app_home;cli_roms;cli_state"
  SKIP_RETURN_CODE 77
  LABELS gui)

# M06: the debugger layout with every panel open, an execution breakpoint on the boot ROM's V-int handler
# ($340) that must stop the emulation there, and a pause at a frame (the app checks both before exiting).
add_test(NAME app.smoke_debugger
  COMMAND gen_app --smoke-test=20 --debugger --break=340 "--focus=Call Stack" --focus=Tiles
          "--screenshot=${CMAKE_CURRENT_BINARY_DIR}/debugger.png" "${CMAKE_BINARY_DIR}/tests/cli/roms/boot.bin")
set_tests_properties(app.smoke_debugger PROPERTIES
  ENVIRONMENT "GENESISEMU_HOME=${home}"
  FIXTURES_REQUIRED "app_home;cli_roms"
  SKIP_RETURN_CODE 77
  LABELS gui)
add_test(NAME app.smoke_pause_at
  COMMAND gen_app --smoke-test=20 --debugger --pause-at=5 "${CMAKE_BINARY_DIR}/tests/cli/roms/boot.bin")
set_tests_properties(app.smoke_pause_at PROPERTIES
  ENVIRONMENT "GENESISEMU_HOME=${home}"
  FIXTURES_REQUIRED "app_home;cli_roms"
  SKIP_RETURN_CODE 77
  LABELS gui)
