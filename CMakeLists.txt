cmake_minimum_required(VERSION 3.22)

project(BassMusicGear VERSION 0.1.0)

#==============================================================================
# C++ 표준
#==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#==============================================================================
# JUCE 서브모듈
#==============================================================================
add_subdirectory(JUCE)

#==============================================================================
# Catch2 — FetchContent 로 가져옴
#==============================================================================
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

#==============================================================================
# 플러그인 타겟 정의
#==============================================================================
juce_add_plugin(BassMusicGear
    COMPANY_NAME                "YourStudio"
    PLUGIN_MANUFACTURER_CODE    Bmgr
    PLUGIN_CODE                 Bssg
    FORMATS                     Standalone VST3
    PRODUCT_NAME                "BassMusicGear"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Fx|Distortion"
    # AU 는 macOS 전용이므로 Windows 빌드에서는 포함하지 않음
    # macOS 빌드 시 아래 두 줄 활성화:
    # FORMATS                   Standalone VST3 AU
    # AU_MAIN_TYPE              "kAudioUnitType_Effect"
)

#==============================================================================
# BinaryData — 리소스 임베드
# Phase 0: 플레이스홀더 파일 1개만 포함
# Phase 1 이후 IR WAV, Preset XML 이 추가된다
#==============================================================================
juce_add_binary_data(BassMusicGear_BinaryData
    SOURCES
        Resources/placeholder.txt
)

# BinaryData 타겟이 경고 없이 빌드되도록 설정
set_target_properties(BassMusicGear_BinaryData PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

#==============================================================================
# 소스 파일 등록
#==============================================================================
target_sources(BassMusicGear
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

#==============================================================================
# 컴파일 정의
#==============================================================================
target_compile_definitions(BassMusicGear
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

#==============================================================================
# 링크 라이브러리
#==============================================================================
target_link_libraries(BassMusicGear
    PRIVATE
        BassMusicGear_BinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

#==============================================================================
# 테스트 서브디렉터리
#==============================================================================
enable_testing()
add_subdirectory(Tests)
