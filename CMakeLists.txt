cmake_minimum_required(VERSION 3.22)

project(Noterator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Apple silicon only: no Intel build is wanted. macOS 11 is the first
    # release that runs on it.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Apple silicon")
endif()

option(NOTERATOR_BUILD_APP   "Build the app (downloads JUCE)" ON)
option(NOTERATOR_BUILD_TESTS "Build the tests" ON)

#==============================================================================
# Lua 5.4, vendored. The generators are the family's Lua engines, run as they
# are rather than ported (decision 0003).
#==============================================================================
file(GLOB LUA_SOURCES ThirdParty/lua/*.c)
add_library(NoteratorLua STATIC ${LUA_SOURCES})
target_include_directories(NoteratorLua PUBLIC ThirdParty/lua)
set_target_properties(NoteratorLua PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
if(UNIX AND NOT APPLE)
    target_compile_definitions(NoteratorLua PRIVATE LUA_USE_LINUX)
elseif(APPLE)
    target_compile_definitions(NoteratorLua PRIVATE LUA_USE_MACOSX)
endif()

#==============================================================================
# The engine files, embedded so the app needs nothing beside itself. The
# generated source is a table of { name, bytes }.
#==============================================================================
file(GLOB_RECURSE NOTERATOR_LUA_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Engines/*.lua)
set(EMBEDDED_LUA_CPP ${CMAKE_CURRENT_BINARY_DIR}/EmbeddedLua.cpp)
add_custom_command(
    OUTPUT ${EMBEDDED_LUA_CPP}
    COMMAND ${CMAKE_COMMAND}
        -DROOT=${CMAKE_CURRENT_SOURCE_DIR}/Engines
        -DOUT=${EMBEDDED_LUA_CPP}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedLua.cmake
    DEPENDS ${NOTERATOR_LUA_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedLua.cmake
    COMMENT "Embedding the Lua engines")

#==============================================================================
# The core: the score, the engraver, detection, AutoCC, MIDI files and the
# generator bridge. No JUCE, so it builds and tests in seconds.
#==============================================================================
add_library(NoteratorCore STATIC
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
    Source/Engines/LuaEngine.cpp
    Source/Engines/Generators.cpp
    ${EMBEDDED_LUA_CPP})
target_include_directories(NoteratorCore PUBLIC Source/Core Source/Engines)
target_link_libraries(NoteratorCore PUBLIC NoteratorLua)
set_target_properties(NoteratorCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(NoteratorCore PRIVATE -Wall -Wextra)
endif()

if(NOTERATOR_BUILD_TESTS)
    enable_testing()
    add_executable(NoteratorTests
        Tests/TestMain.cpp
        Tests/TestScore.cpp
        Tests/TestEngrave.cpp
        Tests/TestDetect.cpp
        Tests/TestMidi.cpp
        Tests/TestGenerators.cpp
        Tests/TestMusicXml.cpp
        Tests/TestTemplates.cpp)
    target_link_libraries(NoteratorTests PRIVATE NoteratorCore)
    add_test(NAME Core COMMAND NoteratorTests)
endif()

if(NOT NOTERATOR_BUILD_APP)
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

juce_add_binary_data(NoteratorAssets SOURCES
    Resources/Fonts/Bravura.otf)
set_target_properties(NoteratorAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(NOTERATOR_SHARED_UI
    Source/App/ScoreRenderer.cpp
    Source/App/Theme.cpp)

juce_add_gui_app(Noterator
    PRODUCT_NAME          "Noterator"
    COMPANY_NAME          "kallums"
    BUNDLE_ID             "com.kallums.noterator"
    VERSION               ${PROJECT_VERSION}
    DOCUMENT_EXTENSIONS   noterator mid midi musicxml mxl
    MICROPHONE_PERMISSION_ENABLED FALSE
    ICON_BIG              ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon.png)

target_include_directories(Noterator PRIVATE Source/App)
target_sources(Noterator PRIVATE
    Source/App/Main.cpp
    Source/App/MainComponent.cpp
    Source/App/Controller.cpp
    Source/App/ScoreView.cpp
    Source/App/Panels.cpp
    Source/App/GeneratorPanel.cpp
    Source/App/SettingsList.cpp
    Source/App/BlocksPanel.cpp
    Source/App/AudioEngine.cpp
    Source/App/Exporter.cpp
    ${NOTERATOR_SHARED_UI})

target_compile_definitions(Noterator PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Noterator,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Noterator,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=0)

target_link_libraries(Noterator
    PRIVATE
        NoteratorCore
        NoteratorAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# Renders a score - a .mid or a .noterator - to a PNG with no window, which is
# how a drawing change is checked (decision 0010).
#==============================================================================
juce_add_console_app(NoteratorRender PRODUCT_NAME "NoteratorRender")
target_sources(NoteratorRender PRIVATE tools/RenderScore.cpp ${NOTERATOR_SHARED_UI})
target_include_directories(NoteratorRender PRIVATE Source/App)
target_compile_definitions(NoteratorRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(NoteratorRender
    PRIVATE NoteratorCore NoteratorAssets juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

#==============================================================================
# The app's own path without a window: controller, generators, export, and
# audio through each synth. On macOS it is what proves Apple's General MIDI
# synth loads and sounds.
#==============================================================================
if(NOTERATOR_BUILD_TESTS)
    juce_add_console_app(NoteratorAppTests PRODUCT_NAME "NoteratorAppTests")
    target_sources(NoteratorAppTests PRIVATE
        Tests/TestApp.cpp
        Source/App/Controller.cpp
        Source/App/AudioEngine.cpp
        Source/App/Exporter.cpp)
    target_include_directories(NoteratorAppTests PRIVATE Source/App Tests)
    target_compile_definitions(NoteratorAppTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=0)
    target_link_libraries(NoteratorAppTests
        PRIVATE NoteratorCore juce::juce_audio_utils juce::juce_audio_processors
        PUBLIC juce::juce_recommended_config_flags)
    add_test(NAME App COMMAND NoteratorAppTests)
endif()
