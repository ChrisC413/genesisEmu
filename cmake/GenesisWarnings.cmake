# genesis_set_warnings(<target>)
# Applies the project warning level. Warnings become errors when CMAKE_COMPILE_WARNING_AS_ERROR is ON
# (CI passes -DCMAKE_COMPILE_WARNING_AS_ERROR=ON).
function(genesis_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /external:anglebrackets /external:W0)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  endif()
endfunction()
