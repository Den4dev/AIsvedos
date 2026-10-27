cmake_minimum_required(VERSION 3.22)

project(VocalPocket VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(JUCE)

set(VP_CORE_SOURCES
    Source/dsp/Analyzer.cpp
    Source/dsp/Compressor.cpp
    Source/dsp/DeEsser.cpp
    Source/dsp/EqChain.cpp
    Source/dsp/MixEngine.cpp
    Source/dsp/Presets.cpp
    Source/dsp/ReferenceAnalysis.cpp
    Source/dsp/SessionBus.cpp
    Source/dsp/Space.cpp
)

add_library(vp_core STATIC ${VP_CORE_SOURCES})
target_include_directories(vp_core PUBLIC Source)
target_link_libraries(vp_core PUBLIC
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
)
target_compile_definitions(vp_core PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

juce_add_plugin(VocalPocket
    COMPANY_NAME "Den4dev"
    BUNDLE_ID "com.den4dev.vocalpocket"
    PLUGIN_MANUFACTURER_CODE Den4
    PLUGIN_CODE Vpkt
    FORMATS VST3 Standalone
    PRODUCT_NAME "VocalPocket"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Dynamics" "EQ"
)

target_sources(VocalPocket PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_link_libraries(VocalPocket PRIVATE
    vp_core
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)

target_compile_definitions(VocalPocket PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
)

juce_add_console_app(render_cli PRODUCT_NAME "render_cli")

target_sources(render_cli PRIVATE tools/render_cli.cpp)

target_link_libraries(render_cli PRIVATE
    vp_core
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_events
    juce::juce_recommended_config_flags
)

juce_add_console_app(host_test PRODUCT_NAME "host_test")

target_sources(host_test PRIVATE tools/host_test.cpp)

target_link_libraries(host_test PRIVATE
    vp_core
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_basics
    juce::juce_events
    juce::juce_recommended_config_flags
)

target_compile_definitions(host_test PRIVATE JUCE_PLUGINHOST_VST3=1)
