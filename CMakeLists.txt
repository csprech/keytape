cmake_minimum_required(VERSION 3.22)
project(Keytape VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
  message(FATAL_ERROR "JUCE not found. Run scripts/bootstrap.sh first.")
endif()
add_subdirectory(external/JUCE)

# The source lists live in keytape-module.cmake, so the Tabletop studio compiles exactly what the app does.
include(keytape-module.cmake)

set(KEYTAPE_JUCE_DEFS
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1
)

option(KEYTAPE_BUILD_APP "Build the GUI app" ON)

if(KEYTAPE_BUILD_APP)
juce_add_gui_app(Keytape
  PRODUCT_NAME "Keytape"
  COMPANY_NAME "Keytape Research"
  BUNDLE_ID "org.keytape.app"
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Keytape's input is your computer's microphone or line in: it samples and records it onto the tape.")

target_sources(Keytape PRIVATE
  ${KEYTAPE_MODULE_SOURCES}
  src/ui/MainComponent.cpp
  src/Main.cpp)
target_include_directories(Keytape PRIVATE src)
target_compile_definitions(Keytape PRIVATE ${KEYTAPE_JUCE_DEFS} JUCE_APPLICATION_NAME_STRING="Keytape"
  JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(Keytape PRIVATE -Wno-float-equal)
target_link_libraries(Keytape PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_gui_extra
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------- tests
juce_add_console_app(KeytapeTests PRODUCT_NAME "KeytapeTests")
target_sources(KeytapeTests PRIVATE
  ${KEYTAPE_CORE_SOURCES}
  tests/TestMain.cpp
  tests/DspTests.cpp
  tests/TapeTests.cpp
  tests/SequencerTests.cpp
  tests/PanelTests.cpp
  tests/FolderTests.cpp
  src/engine/Folder.cpp)
target_include_directories(KeytapeTests PRIVATE src)
target_compile_definitions(KeytapeTests PRIVATE ${KEYTAPE_JUCE_DEFS})
target_compile_options(KeytapeTests PRIVATE -Wno-float-equal)
target_link_libraries(KeytapeTests PRIVATE
  juce::juce_core
  juce::juce_audio_basics
  juce::juce_audio_formats
  juce::juce_audio_devices
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME KeytapeTests COMMAND KeytapeTests)
