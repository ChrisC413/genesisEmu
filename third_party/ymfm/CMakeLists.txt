# ymfm (BSD-3-Clause, https://github.com/aaronsgiles/ymfm), vendored at commit
# 81aec25ccbb98f4873a255f7551ac4dadac59b4a: only the OPN family sources the YM2612 needs, unmodified.
add_library(ymfm STATIC
  src/ymfm_adpcm.cpp
  src/ymfm_opn.cpp
  src/ymfm_ssg.cpp
)
target_include_directories(ymfm SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(ymfm PUBLIC cxx_std_14)
# Third-party code: never fail the build on its warnings.
set_target_properties(ymfm PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
if(MSVC)
  target_compile_options(ymfm PRIVATE /W0)
else()
  target_compile_options(ymfm PRIVATE -w)
endif()
