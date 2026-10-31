# Trace tests (M02-06).
#   -DCLI=<gen_cli> -DROM=<boot.bin> -DGOLDEN=<golden trace> -DOUT_DIR=<dir>
#   [-DPYTHON=<python> -DTRACE_DIFF=<tools/trace-diff.py>]
# 1. `gen_cli trace` of the synthetic boot ROM must equal the golden trace byte for byte.
# 2. With PYTHON: trace-diff reports "identical" for the pair and finds a divergence injected into a copy.

set(trace "${OUT_DIR}/boot.trace")
execute_process(COMMAND "${CLI}" trace --rom "${ROM}" --frames 3 --first --out "${trace}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "gen_cli trace exited with ${code}: ${out}${err}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${trace}" "${GOLDEN}" RESULT_VARIABLE differ)
if(NOT differ EQUAL 0)
  message(FATAL_ERROR "trace differs from ${GOLDEN}; inspect with tools/trace-diff.py ${trace} ${GOLDEN}")
endif()

if(DEFINED PYTHON)
  execute_process(COMMAND "${PYTHON}" "${TRACE_DIFF}" "${trace}" "${GOLDEN}" RESULT_VARIABLE code OUTPUT_VARIABLE out)
  if(NOT code EQUAL 0 OR NOT out MATCHES "traces identical")
    message(FATAL_ERROR "trace-diff on identical traces: ${code} ${out}")
  endif()
  # Inject a divergence: change the first D0 delta.
  file(READ "${trace}" text)
  string(FIND "${text}" " D0=" at)
  if(at LESS 0)
    message(FATAL_ERROR "no D0 delta in the trace")
  endif()
  math(EXPR digit "${at} + 11")
  string(SUBSTRING "${text}" 0 ${digit} before)
  string(SUBSTRING "${text}" ${digit} 1 old)
  math(EXPR rest "${digit} + 1")
  string(SUBSTRING "${text}" ${rest} -1 after)
  if(old STREQUAL "7")
    set(new "8")
  else()
    set(new "7")
  endif()
  file(WRITE "${OUT_DIR}/boot-injected.trace" "${before}${new}${after}")
  execute_process(COMMAND "${PYTHON}" "${TRACE_DIFF}" "${trace}" "${OUT_DIR}/boot-injected.trace"
    RESULT_VARIABLE code OUTPUT_VARIABLE out)
  message("${out}")
  if(NOT code EQUAL 1 OR NOT out MATCHES "first divergence after [0-9]+ matching records")
    message(FATAL_ERROR "trace-diff missed the injected divergence (exit ${code})")
  endif()
endif()
