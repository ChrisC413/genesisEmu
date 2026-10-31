add_executable(gen_cli
  main.cpp
)
target_link_libraries(gen_cli PRIVATE gen_analysis gen_common)
genesis_set_warnings(gen_cli)
