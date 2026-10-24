add_library(gen_core STATIC
  cart/known_roms.cpp
  cart/rom.cpp
  m68k/m68k.cpp
  m68k/ops_arith.cpp
  m68k/ops_bcd.cpp
  m68k/ops_flow.cpp
  m68k/ops_logic.cpp
  m68k/ops_move.cpp
  m68k/ops_shift.cpp
  m68k/ops_system.cpp
  util/hash.cpp
  version.cpp
)
target_include_directories(gen_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(gen_core PRIVATE GENESIS_VERSION="${PROJECT_VERSION}")
genesis_set_warnings(gen_core)
if(MSVC)
  # The decode table instantiates many handler templates per translation unit.
  target_compile_options(gen_core PRIVATE /bigobj)
endif()
