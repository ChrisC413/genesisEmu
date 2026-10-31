add_library(gen_core STATIC
  audio/register_log.cpp
  bus/io.cpp
  bus/system_bus.cpp
  cart/known_roms.cpp
  cart/rom.cpp
  cart/sram.cpp
  m68k/m68k.cpp
  m68k/ops_arith.cpp
  m68k/ops_bcd.cpp
  m68k/ops_flow.cpp
  m68k/ops_logic.cpp
  m68k/ops_move.cpp
  m68k/ops_shift.cpp
  m68k/ops_system.cpp
  sched/machine.cpp
  state/state_hash.cpp
  util/hash.cpp
  vdp/vdp.cpp
  vdp/vdp_render.cpp
  version.cpp
  z80/z80.cpp
  z80/z80_control.cpp
  z80/z80_ops.cpp
)
find_package(xxHash CONFIG REQUIRED)
target_link_libraries(gen_core PRIVATE xxHash::xxhash)
target_include_directories(gen_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(gen_core PRIVATE GENESIS_VERSION="${PROJECT_VERSION}")
genesis_set_warnings(gen_core)
if(MSVC)
  # The decode table instantiates many handler templates per translation unit.
  target_compile_options(gen_core PRIVATE /bigobj)
endif()
