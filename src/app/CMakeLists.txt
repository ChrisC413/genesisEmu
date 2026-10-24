find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(gen_app
  main.cpp
)
set_target_properties(gen_app PROPERTIES OUTPUT_NAME genesisEmu)
target_link_libraries(gen_app PRIVATE gen_analysis gen_common SDL3::SDL3 imgui::imgui)
genesis_set_warnings(gen_app)
