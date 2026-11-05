# Keytape as a module for hosts (the Tabletop studio): the sources to compile into the host's own target,
# which brings JUCE (juce_audio_utils, juce_audio_processors, juce_audio_formats, juce_gui_extra). The app and
# the tests use the same lists.
set(KEYTAPE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(KEYTAPE_CORE_SOURCES
  ${KEYTAPE_DIR}/src/core/Model.cpp
  ${KEYTAPE_DIR}/src/core/Voices.cpp
  ${KEYTAPE_DIR}/src/core/Effects.cpp
  ${KEYTAPE_DIR}/src/core/Tape.cpp
  ${KEYTAPE_DIR}/src/core/Sequencers.cpp
  ${KEYTAPE_DIR}/src/core/Instruments.cpp
  ${KEYTAPE_DIR}/src/core/Machine.cpp
  ${KEYTAPE_DIR}/src/core/MachineScreens.cpp)
set(KEYTAPE_UI_SOURCES
  ${KEYTAPE_DIR}/src/engine/Folder.cpp
  ${KEYTAPE_DIR}/src/engine/Engine.cpp
  ${KEYTAPE_DIR}/src/ui/Materials.cpp
  ${KEYTAPE_DIR}/src/ui/Widgets.cpp
  ${KEYTAPE_DIR}/src/ui/Display.cpp
  ${KEYTAPE_DIR}/src/ui/Panel.cpp)
set(KEYTAPE_MODULE_SOURCES ${KEYTAPE_CORE_SOURCES} ${KEYTAPE_UI_SOURCES} ${KEYTAPE_DIR}/src/plugin/KeytapeProcessor.cpp)
# (switches over enums use `default:` on purpose; JUCE's warning set flags every one)
set_source_files_properties(${KEYTAPE_CORE_SOURCES} ${KEYTAPE_UI_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-switch-enum;-Wno-float-equal")
