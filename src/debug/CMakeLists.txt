add_library(gen_debug STATIC
  debugger.cpp
  expr.cpp
  listing.cpp
  memory.cpp
)
target_include_directories(gen_debug PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gen_debug PUBLIC gen_analysis)
genesis_set_warnings(gen_debug)
