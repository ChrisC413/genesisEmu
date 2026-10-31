add_library(gen_analysis STATIC
  analysis.cpp
)
target_include_directories(gen_analysis PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gen_analysis PUBLIC gen_instrument)
genesis_set_warnings(gen_analysis)
