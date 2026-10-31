add_library(gen_instrument STATIC
  instrument.cpp
  trace_writer.cpp
)
target_include_directories(gen_instrument PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gen_instrument PUBLIC gen_core)
genesis_set_warnings(gen_instrument)
