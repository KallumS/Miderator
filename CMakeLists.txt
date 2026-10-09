cmake_minimum_required(VERSION 3.22)

project(Miderator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Apple silicon only: no Intel build is wanted. macOS 11 is the first
    # release that runs on it.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Apple silicon")
endif()

option(MIDERATOR_BUILD_APP   "Build the app (downloads JUCE)" ON)
option(MIDERATOR_BUILD_TESTS "Build the tests" ON)

#==============================================================================
# Lua 5.4, vendored. The generators are the family's Lua engines, run as they
# are rather than ported (decision 0003).
#==============================================================================
file(GLOB LUA_SOURCES ThirdParty/lua/*.c)
add_library(MideratorLua STATIC ${LUA_SOURCES})
target_include_directories(MideratorLua PUBLIC ThirdParty/lua)
set_target_properties(MideratorLua PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
if(UNIX AND NOT APPLE)
    target_compile_definitions(MideratorLua PRIVATE LUA_USE_LINUX)
elseif(APPLE)
    target_compile_definitions(MideratorLua PRIVATE LUA_USE_MACOSX)
endif()

#==============================================================================
# The engine files, embedded so the app needs nothing beside itself. The
# generated source is a table of { name, bytes }.
#==============================================================================
file(GLOB_RECURSE MIDERATOR_LUA_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Engines/*.lua)
set(EMBEDDED_LUA_CPP ${CMAKE_CURRENT_BINARY_DIR}/EmbeddedLua.cpp)
add_custom_command(
    OUTPUT ${EMBEDDED_LUA_CPP}
    COMMAND ${CMAKE_COMMAND}
        -DROOT=${CMAKE_CURRENT_SOURCE_DIR}/Engines
        -DOUT=${EMBEDDED_LUA_CPP}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedLua.cmake
    DEPENDS ${MIDERATOR_LUA_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedLua.cmake
    COMMENT "Embedding the Lua engines")

#==============================================================================
# The core: the score, the piano roll's edits and checks, detection, AutoCC,
# MIDI and MusicXML files (the engraver stays for MusicXML, decision 0029) and
# the generator bridge. No JUCE, so it builds and tests in seconds.
#==============================================================================
add_library(MideratorCore STATIC
    Source/Core/Score.cpp
    Source/Core/Instruments.cpp
    Source/Core/Spelling.cpp
    Source/Core/Detect.cpp
    Source/Core/AutoCC.cpp
    Source/Core/Perform.cpp
    Source/Core/MidiFile.cpp
    Source/Core/Edit.cpp
    Source/Core/Engrave.cpp
    Source/Core/ScoreFile.cpp
    Source/Core/Templates.cpp
    Source/Core/Xml.cpp
    Source/Core/MusicXml.cpp
    Source/Core/Roll.cpp
    Source/Engines/LuaEngine.cpp
    Source/Engines/Generators.cpp
    ${EMBEDDED_LUA_CPP})
target_include_directories(MideratorCore PUBLIC Source/Core Source/Engines)
target_link_libraries(MideratorCore PUBLIC MideratorLua)
set_target_properties(MideratorCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(MideratorCore PRIVATE -Wall -Wextra)
endif()

if(MIDERATOR_BUILD_TESTS)
    enable_testing()
    add_executable(MideratorTests
        Tests/TestMain.cpp
        Tests/TestScore.cpp
        Tests/TestEngrave.cpp
        Tests/TestDetect.cpp
        Tests/TestMidi.cpp
        Tests/TestGenerators.cpp
        Tests/TestMusicXml.cpp
        Tests/TestTemplates.cpp
        Tests/TestRoll.cpp
        Tests/TestFollow.cpp)
    target_link_libraries(MideratorTests PRIVATE MideratorCore)
    add_test(NAME Core COMMAND MideratorTests)
endif()

if(NOT MIDERATOR_BUILD_APP)
    return()
endif()

#==============================================================================
# JUCE, pinned to the version ScaleView builds with.
#==============================================================================
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# The drawing the app and the render tool share.
set(MIDERATOR_VIEWS
    Source/App/Theme.cpp
    Source/App/Timeline.cpp
    Source/App/ArrangeView.cpp
    Source/App/PianoRollView.cpp
    Source/App/Workspace.cpp)

juce_add_gui_app(Miderator
    PRODUCT_NAME          "Miderator"
    COMPANY_NAME          "kallums"
    BUNDLE_ID             "com.kallums.miderator"
    VERSION               ${PROJECT_VERSION}
    DOCUMENT_EXTENSIONS   miderator noterator mid midi musicxml mxl
    MICROPHONE_PERMISSION_ENABLED FALSE
    ICON_BIG              ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon.png)

target_include_directories(Miderator PRIVATE Source/App)
target_sources(Miderator PRIVATE
    Source/App/Main.cpp
    Source/App/MainComponent.cpp
    Source/App/Controller.cpp
    Source/App/Panels.cpp
    Source/App/GeneratorPanel.cpp
    Source/App/SettingsList.cpp
    Source/App/BlocksPanel.cpp
    Source/App/AudioEngine.cpp
    Source/App/Exporter.cpp
    ${MIDERATOR_VIEWS})

target_compile_definitions(Miderator PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Miderator,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Miderator,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=0)

target_link_libraries(Miderator
    PRIVATE
        MideratorCore
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# Draws the tracks and the piano roll - a demo, or any .mid, MusicXML or
# project - into a PNG with no window, through the app's own components, which
# is how a drawing change is checked (decision 0010).
#==============================================================================
juce_add_console_app(MideratorRender PRODUCT_NAME "MideratorRender")
target_sources(MideratorRender PRIVATE
    tools/RenderRoll.cpp
    Source/App/Controller.cpp
    Source/App/AudioEngine.cpp
    ${MIDERATOR_VIEWS})
target_include_directories(MideratorRender PRIVATE Source/App)
target_compile_definitions(MideratorRender PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=0)
target_link_libraries(MideratorRender
    PRIVATE MideratorCore juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

#==============================================================================
# The app's own path without a window: controller, generators, export, and
# audio through each synth. On macOS it is what proves Apple's General MIDI
# synth loads and sounds.
#==============================================================================
if(MIDERATOR_BUILD_TESTS)
    juce_add_console_app(MideratorAppTests PRODUCT_NAME "MideratorAppTests")
    target_sources(MideratorAppTests PRIVATE
        Tests/TestApp.cpp
        Source/App/Controller.cpp
        Source/App/AudioEngine.cpp
        Source/App/Exporter.cpp)
    target_include_directories(MideratorAppTests PRIVATE Source/App Tests)
    target_compile_definitions(MideratorAppTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=0)
    target_link_libraries(MideratorAppTests
        PRIVATE MideratorCore juce::juce_audio_utils juce::juce_audio_processors
        PUBLIC juce::juce_recommended_config_flags)
    add_test(NAME App COMMAND MideratorAppTests)
endif()
