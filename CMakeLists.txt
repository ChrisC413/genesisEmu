cmake_minimum_required(VERSION 3.25)

project(genesisEmu
  VERSION 0.0.1
  DESCRIPTION "Sega Genesis emulator and reverse-engineering workbench"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GENESIS_BUILD_APP "Build the SDL3/ImGui desktop application" ON)
option(GENESIS_BUILD_TESTS "Build the test suites" ON)
option(GENESIS_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer (clang/gcc)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GenesisWarnings)

if(GENESIS_SANITIZE)
  if(MSVC)
    message(FATAL_ERROR "GENESIS_SANITIZE is only supported with clang/gcc")
  endif()
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(src/core)
add_subdirectory(src/instrument)
add_subdirectory(src/analysis)
add_subdirectory(src/common)
add_subdirectory(src/cli)
if(GENESIS_BUILD_APP)
  add_subdirectory(src/app)
endif()

if(GENESIS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
