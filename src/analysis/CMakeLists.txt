add_library(gen_analysis STATIC
  analysis.cpp
  disasm68k/disasm68k.cpp
  disasmz80/disasmz80.cpp
)
target_include_directories(gen_analysis PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gen_analysis PUBLIC gen_instrument)
genesis_set_warnings(gen_analysis)
