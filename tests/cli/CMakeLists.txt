add_executable(make_test_roms make_test_roms.cpp)
target_link_libraries(make_test_roms PRIVATE gen_test_support)
genesis_set_warnings(make_test_roms)

set(rom_dir "${CMAKE_CURRENT_BINARY_DIR}/roms")

add_test(NAME cli.make_test_roms COMMAND make_test_roms "${rom_dir}")
set_tests_properties(cli.make_test_roms PROPERTIES FIXTURES_SETUP cli_roms LABELS cli)

# genesis_cli_test(<name> <expected exit code> <|-separated args> [EXPECT <regex>] [COMPARE <|-separated args> [COMPARE_MATCH <regex>]])
function(genesis_cli_test name code args)
  cmake_parse_arguments(PARSE_ARGV 3 opt "" "EXPECT;COMPARE;COMPARE_MATCH" "")
  set(defs -DCLI=$<TARGET_FILE:gen_cli> "-DARGS=${args}" -DEXPECT_CODE=${code})
  if(DEFINED opt_EXPECT)
    list(APPEND defs "-DEXPECT_OUTPUT=${opt_EXPECT}")
  endif()
  if(DEFINED opt_COMPARE)
    list(APPEND defs "-DCOMPARE_ARGS=${opt_COMPARE}")
  endif()
  if(DEFINED opt_COMPARE_MATCH)
    list(APPEND defs "-DCOMPARE_MATCH=${opt_COMPARE_MATCH}")
  endif()
  add_test(NAME cli.${name} COMMAND "${CMAKE_COMMAND}" ${defs} -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cli.cmake")
  set_tests_properties(cli.${name} PROPERTIES FIXTURES_REQUIRED cli_roms LABELS cli)
endfunction()

genesis_cli_test(info_bin 0 "info|--rom|${rom_dir}/synthetic.bin"
  EXPECT "Identified +unknown.*SHA-1 +[0-9a-f]+.*Domestic title +SYNTHETIC CLI ROM.*Checksum +\\$[0-9A-F]+ \\(valid\\)")
genesis_cli_test(info_smd_matches_bin 0 "info|--rom|${rom_dir}/synthetic.smd"
  EXPECT "Format +SMD"
  COMPARE "info|--rom|${rom_dir}/synthetic.bin")
genesis_cli_test(info_json 0 "info|--rom|${rom_dir}/synthetic.bin|--json"
  EXPECT "\"identified\": null.*\"sha1\": \"[0-9a-f]+\"")
genesis_cli_test(info_missing_rom 2 "info|--rom|${rom_dir}/does-not-exist.bin" EXPECT "Cannot open")
genesis_cli_test(info_too_small 2 "info|--rom|${rom_dir}/tiny.bin" EXPECT "too small")
genesis_cli_test(info_requires_rom 1 "info")
genesis_cli_test(run_requires_rom 1 "run|--frames|10" EXPECT "requires --rom")
genesis_cli_test(run_boot 0 "run|--rom|${rom_dir}/boot.bin|--frames|60"
  EXPECT "frames 60, instructions [0-9]+.*V-ints [0-9]+ .*faults none")
genesis_cli_test(run_boot_json 0 "run|--rom|${rom_dir}/boot.bin|--frames|10|--json"
  EXPECT "\"faults\": 0.*\"frames\": 10.*\"stateHash\": \"[0-9a-f]+\""
  COMPARE "run|--rom|${rom_dir}/boot.bin|--frames|10|--json" COMPARE_MATCH "\"stateHash\": \"[0-9a-f]+\"")
genesis_cli_test(run_faulting_rom 3 "run|--rom|${rom_dir}/synthetic.bin" EXPECT "faults [0-9]+: first vector")
genesis_cli_test(run_bad_frames 1 "run|--frames|ten")
genesis_cli_test(help 0 "--help" EXPECT "info --rom.*run --rom.*trace --rom.*help \\[command\\].*Exit codes")
genesis_cli_test(help_command 0 "help" EXPECT "Commands:")
genesis_cli_test(no_command 1 "" EXPECT "Usage:")
genesis_cli_test(unknown_command 1 "frobnicate" EXPECT "unknown command")
genesis_cli_test(unknown_option 1 "info|--bogus" EXPECT "unknown option")
