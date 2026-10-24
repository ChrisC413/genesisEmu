# Runs gen_cli and checks its exit code and output.
#   -DCLI=<path to gen_cli>  -DARGS=<|-separated arguments>  -DEXPECT_CODE=<n>
#   [-DEXPECT_OUTPUT=<regex>]  [-DCOMPARE_ARGS=<|-separated arguments>]
# With COMPARE_ARGS, the text matching COMPARE_MATCH (default: the SHA-1 line) must be identical in both
# invocations.

string(REPLACE "|" ";" ARGS "${ARGS}")
string(REPLACE "|" ";" COMPARE_ARGS "${COMPARE_ARGS}")

execute_process(COMMAND "${CLI}" ${ARGS}
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}${err}")

if(NOT code EQUAL EXPECT_CODE)
  message(FATAL_ERROR "gen_cli exited with ${code}, expected ${EXPECT_CODE}")
endif()

if(DEFINED EXPECT_OUTPUT AND NOT "${out}${err}" MATCHES "${EXPECT_OUTPUT}")
  message(FATAL_ERROR "output does not match: ${EXPECT_OUTPUT}")
endif()

if(NOT COMPARE_ARGS STREQUAL "")
  execute_process(COMMAND "${CLI}" ${COMPARE_ARGS} RESULT_VARIABLE code2 OUTPUT_VARIABLE out2)
  if(NOT DEFINED COMPARE_MATCH)
    set(COMPARE_MATCH "SHA-1 +[0-9a-f]+")
  endif()
  string(REGEX MATCH "${COMPARE_MATCH}" sha_a "${out}")
  string(REGEX MATCH "${COMPARE_MATCH}" sha_b "${out2}")
  if(NOT code2 EQUAL 0 OR sha_a STREQUAL "" OR NOT sha_a STREQUAL sha_b)
    message(FATAL_ERROR "compared output differs: '${sha_a}' vs '${sha_b}'")
  endif()
endif()
