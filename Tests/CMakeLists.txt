# Tests/CMakeLists.txt
# Catch2 based unit tests for BassMusicGear DSP modules

add_executable(BassMusicGear_Tests
    SmokeTest.cpp
    ToneStackTest.cpp
    OverdriveTest.cpp
)

# Add DSP source files needed by tests (compiled separately from plugin target)
target_sources(BassMusicGear_Tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/Source/DSP/ToneStack.cpp"
        "${CMAKE_SOURCE_DIR}/Source/DSP/Preamp.cpp"
)

target_compile_features(BassMusicGear_Tests PRIVATE cxx_std_17)

target_include_directories(BassMusicGear_Tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/Source"
)

target_link_libraries(BassMusicGear_Tests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# JUCE module compile definitions needed for standalone test compilation
target_compile_definitions(BassMusicGear_Tests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=0
        JUCE_MODULE_AVAILABLE_juce_audio_basics=1
        JUCE_MODULE_AVAILABLE_juce_audio_processors=1
        JUCE_MODULE_AVAILABLE_juce_dsp=1
        JUCE_MODULE_AVAILABLE_juce_core=1
        JUCE_MODULE_AVAILABLE_juce_data_structures=1
        JUCE_MODULE_AVAILABLE_juce_events=1
        JUCE_MODULE_AVAILABLE_juce_audio_formats=1
        JUCE_MODULE_AVAILABLE_juce_audio_devices=1
        JUCE_MODULE_AVAILABLE_juce_graphics=1
        JUCE_MODULE_AVAILABLE_juce_gui_basics=1
        JUCE_MODULE_AVAILABLE_juce_gui_extra=1
        JUCE_MODULE_AVAILABLE_juce_audio_utils=1
)

include(CTest)
include(Catch)
catch_discover_tests(BassMusicGear_Tests)
