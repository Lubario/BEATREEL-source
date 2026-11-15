cmake_minimum_required(VERSION 3.22)

project(BEATREEL VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# JUCE: either a sibling checkout (./JUCE) or fetched automatically.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Plugin formats: VST3 everywhere, AU on macOS, plus a Standalone app.
set(BEATREEL_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND BEATREEL_FORMATS AU)
endif()

juce_add_binary_data(BeatReelData SOURCES
    Resources/fonts/Anton-Regular.ttf
    Resources/fonts/ArchivoBlack-Regular.ttf
    Resources/fonts/BarlowCondensed-SemiBold.ttf
    Resources/fonts/BebasNeue-Regular.ttf
    Resources/fonts/JetBrainsMono.ttf
    Resources/fonts/PlayfairDisplay.ttf
    Resources/fonts/PressStart2P-Regular.ttf
    Resources/fonts/RubikMonoOne-Regular.ttf
    Resources/fonts/SpaceGrotesk.ttf
    Resources/fonts/UnifrakturMaguntia-Book.ttf)
set_target_properties(BeatReelData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(BEATREEL_CORE_SOURCES
    Source/Analysis.cpp
    Source/Style.cpp
    Source/Renderer.cpp
    Source/Exporter.cpp
    Source/License.cpp
    Source/Demo.cpp)

juce_add_plugin(BeatReel
    COMPANY_NAME "BeatReel"
    COMPANY_WEBSITE "https://example.com"
    BUNDLE_ID com.beatreel.beatreel
    PLUGIN_MANUFACTURER_CODE Btrl
    PLUGIN_CODE Brl1
    FORMATS ${BEATREEL_FORMATS}
    PRODUCT_NAME "BEATREEL"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "BEATREEL standalone records the audio you route into it."
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(BeatReel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${BEATREEL_CORE_SOURCES})

target_include_directories(BeatReel PRIVATE Source)

target_compile_definitions(BeatReel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BeatReel
    PRIVATE
        BeatReelData
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Command-line tools
#   brl_render : renders a reel from a WAV (+ optional .mid) without a DAW.
#                Used for testing, and handy on its own.
#   brl_keygen : creates the RSA key pair and signs license keys (seller only,
#                never ship this).
# ---------------------------------------------------------------------------
juce_add_console_app(brl_render PRODUCT_NAME "brl_render")
target_sources(brl_render PRIVATE tools/render/Main.cpp ${BEATREEL_CORE_SOURCES})
target_include_directories(brl_render PRIVATE Source)
target_compile_definitions(brl_render PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(brl_render PRIVATE BeatReelData
    juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography
    juce::juce_recommended_config_flags)

juce_add_console_app(brl_keygen PRODUCT_NAME "brl_keygen")
target_sources(brl_keygen PRIVATE tools/keygen/Main.cpp Source/License.cpp)
target_include_directories(brl_keygen PRIVATE Source)
target_compile_definitions(brl_keygen PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(brl_keygen PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography
    juce::juce_recommended_config_flags)

# Capture test (not shipped): checks bit-exact recording and loop-end detection.
juce_add_console_app(brl_captest PRODUCT_NAME "brl_captest")
target_sources(brl_captest PRIVATE tools/captest/Main.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp ${BEATREEL_CORE_SOURCES})
target_include_directories(brl_captest PRIVATE Source)
target_compile_definitions(brl_captest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="BEATREEL")
target_link_libraries(brl_captest PRIVATE BeatReelData juce::juce_audio_utils juce::juce_gui_extra juce::juce_dsp
    juce::juce_cryptography juce::juce_recommended_config_flags)
