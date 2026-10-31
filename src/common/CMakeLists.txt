# Shared frontend services used by both gen_app and gen_cli: configuration, logging, platform paths.
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(gen_common STATIC
  config.cpp
  logging.cpp
  paths.cpp
  golden_frames.cpp
  png.cpp
  rom_info.cpp
)
target_include_directories(gen_common PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gen_common PUBLIC gen_core nlohmann_json::nlohmann_json spdlog::spdlog)
genesis_set_warnings(gen_common)
