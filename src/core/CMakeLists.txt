add_library(gen_core STATIC
  version.cpp
)
target_include_directories(gen_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(gen_core PRIVATE GENESIS_VERSION="${PROJECT_VERSION}")
genesis_set_warnings(gen_core)
