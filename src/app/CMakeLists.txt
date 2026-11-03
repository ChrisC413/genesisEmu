find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

set(app_sources
  app.cpp
  input.cpp
  main.cpp
  panels/game_view.cpp
  panels/rom_info.cpp
  platform/file_dialog.cpp
  platform/window.cpp
)

if(WIN32)
  add_executable(gen_app WIN32 ${app_sources} resources/genesisEmu.rc)
elseif(APPLE)
  set(icon "${CMAKE_CURRENT_SOURCE_DIR}/resources/genesisEmu.icns")
  set_source_files_properties("${icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  add_executable(gen_app MACOSX_BUNDLE ${app_sources} "${icon}")
  set_target_properties(gen_app PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME genesisEmu
    MACOSX_BUNDLE_GUI_IDENTIFIER com.chriscowin.genesisEmu
    MACOSX_BUNDLE_ICON_FILE genesisEmu.icns
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
  target_compile_definitions(gen_app PRIVATE GL_SILENCE_DEPRECATION)
else()
  add_executable(gen_app ${app_sources})
endif()

set_target_properties(gen_app PROPERTIES OUTPUT_NAME genesisEmu)
target_include_directories(gen_app PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gen_app PRIVATE gen_analysis gen_common SDL3::SDL3 imgui::imgui OpenGL::GL)
genesis_set_warnings(gen_app)
