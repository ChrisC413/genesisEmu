# Shared frontend services used by both gen_app and gen_cli: configuration, logging, platform paths.
add_library(gen_common STATIC
  common.cpp
)
target_include_directories(gen_common PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gen_common PUBLIC gen_core)
genesis_set_warnings(gen_common)
