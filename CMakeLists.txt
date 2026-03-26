# BassMusicGear 루트 CMakeLists.txt
# JUCE 8 기반 베이스 기타 앰프 시뮬레이터 빌드 정의
# 지원 포맷: Standalone, VST3 (Windows/macOS) + AU (macOS 전용)

cmake_minimum_required(VERSION 3.22)

project(BassMusicGear VERSION 0.1.0)

#==============================================================================
# C++ 표준 설정
# C++17을 강제한다. JUCE 8과 프로젝트 코드 모두 C++17 기능을 사용한다.
# EXTENSIONS OFF: GNU 확장을 비활성화해 MSVC/Clang 간 이식성을 유지한다.
#==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#==============================================================================
# JUCE 서브모듈 추가
# JUCE/는 git submodule로 관리된다. 처음 클론 후 아래 명령 실행 필요:
#   git submodule update --init --recursive
#==============================================================================
add_subdirectory(JUCE)

#==============================================================================
# Catch2 테스트 프레임워크 — FetchContent로 자동 다운로드
# 소스 트리에 포함하지 않고 빌드 시점에 v3.7.1을 가져온다.
# 인터넷 연결이 없는 환경에서는 로컬 캐시가 필요하다.
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
# juce_add_plugin()은 지정된 FORMATS 수만큼 빌드 타겟을 생성한다.
#   BassMusicGear_Standalone, BassMusicGear_VST3 (+ AU는 macOS에서 활성화)
#
# PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE: VST3·AU 식별자 (4자리 고유값)
# EDITOR_WANTS_KEYBOARD_FOCUS FALSE: 노브 드래그 중 키보드 이벤트를 DAW가 받도록 허용
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
    # AU는 macOS 전용이므로 Windows 빌드에서는 포함하지 않는다.
    # macOS 빌드 시 아래 두 줄을 활성화한다:
    # FORMATS                   Standalone VST3 AU
    # AU_MAIN_TYPE              "kAudioUnitType_Effect"
)

#==============================================================================
# BinaryData — 리소스 파일을 C++ 바이너리로 임베드
# juce_add_binary_data()는 SOURCES에 나열된 파일들을 BinaryData.h/.cpp로 컴파일해
# 런타임에 파일 경로 없이 메모리에서 직접 접근할 수 있게 한다.
#
# Phase 0: 플레이스홀더 파일 1개만 포함 (타겟이 비어 있으면 빌드 오류 발생)
# Phase 1 이후: Resources/IR/*.wav, Resources/Presets/*.xml이 추가된다.
#==============================================================================
juce_add_binary_data(BassMusicGear_BinaryData
    SOURCES
        Resources/placeholder.txt
        Resources/IR/placeholder_ir.wav
        Resources/IR/ir_8x10_svt.wav
        Resources/IR/ir_4x10_jbl.wav
        Resources/IR/ir_1x15_vintage.wav
        Resources/IR/ir_2x12_british.wav
        Resources/IR/ir_2x10_modern.wav
)

# BinaryData 라이브러리는 공유 라이브러리(.so)로 링크될 수 있으므로
# Position Independent Code를 활성화해 링크 오류를 방지한다.
set_target_properties(BassMusicGear_BinaryData PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

#==============================================================================
# 소스 파일 등록
# Phase 0: PluginProcessor + PluginEditor만 포함
# 이후 Phase에서 DSP/, UI/, Models/, Presets/ 소스 파일이 추가된다.
#==============================================================================
target_sources(BassMusicGear
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
        Source/DSP/Effects/NoiseGate.h
        Source/DSP/Effects/NoiseGate.cpp
        Source/DSP/ToneStack.h
        Source/DSP/ToneStack.cpp
        Source/DSP/Preamp.h
        Source/DSP/Preamp.cpp
        Source/DSP/PowerAmp.h
        Source/DSP/PowerAmp.cpp
        Source/DSP/Cabinet.h
        Source/DSP/Cabinet.cpp
        Source/DSP/SignalChain.h
        Source/DSP/SignalChain.cpp
        Source/UI/Knob.h
        Source/UI/Knob.cpp
        Source/UI/AmpPanel.h
        Source/UI/AmpPanel.cpp
        Source/UI/CabinetSelector.h
        Source/UI/CabinetSelector.cpp
        Source/Models/AmpModel.h
        Source/Models/AmpModelLibrary.h
        Source/Models/AmpModelLibrary.cpp
)

#==============================================================================
# 컴파일 정의 (전처리기 매크로)
# JUCE_WEB_BROWSER / JUCE_USE_CURL: 불필요한 네트워크 기능 비활성화로 빌드 크기 절감
# JUCE_VST3_CAN_REPLACE_VST2: VST2 하위 호환 ID 비활성화 (VST2 라이선스 불필요)
# 참고: JUCE 8은 스플래시 스크린 기능 자체가 제거되어 JUCE_DISPLAY_SPLASH_SCREEN
#       플래그가 무효화됐다. 정의하면 "flag is ignored" 경고가 발생하므로 제거한다.
#==============================================================================
target_compile_definitions(BassMusicGear
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_ASIO=1
)

target_include_directories(BassMusicGear
    PRIVATE
        ASIOSDK/common
        Source
)

#==============================================================================
# 링크 라이브러리
# PRIVATE: 플러그인 구현 코드에서만 사용 (헤더를 외부에 노출하지 않음)
#   juce_audio_utils: AudioProcessor, AudioProcessorEditor 등 플러그인 핵심 클래스
#   juce_dsp: Oversampling, Convolution, IIR/FIR 필터 등 DSP 모듈
# PUBLIC: 이 타겟을 링크하는 하위 타겟에도 전파되는 옵션
#   juce_recommended_config_flags: 릴리즈/디버그 최적화 플래그
#   juce_recommended_warning_flags: 컴파일러 경고 활성화 플래그
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
# 테스트 서브디렉터리 추가
# enable_testing()은 ctest 명령이 이 디렉터리를 인식하도록 등록한다.
# Tests/CMakeLists.txt에서 Catch2 기반 테스트 타겟을 정의한다.
#==============================================================================
enable_testing()
add_subdirectory(Tests)
