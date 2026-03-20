# Tests/CMakeLists.txt
# Catch2 기반 단위 테스트 타겟 정의
# Catch2는 상위 CMakeLists.txt에서 FetchContent로 가져온다.
# cmake_minimum_required는 루트 CMakeLists.txt에서 이미 설정되므로 여기서는 생략한다.

#==============================================================================
# 테스트 실행 파일 정의
# Catch2::Catch2WithMain을 링크하므로 main() 함수를 직접 작성할 필요가 없다.
#
# Phase 0: SmokeTest.cpp만 포함 (빌드 환경 검증용)
# 이후 Phase에서 아래 파일들이 주석 해제되어 추가된다:
#   ToneStackTest.cpp    — 톤스택 주파수 응답 단위 테스트
#   OverdriveTest.cpp    — 웨이브쉐이퍼 앨리어싱 테스트
#   CompressorTest.cpp   — 컴프레서 타임 컨스턴트 및 게인 리덕션 검증
#   BiAmpCrossoverTest.cpp — LR4 크로스오버 주파수 응답 및 LP+HP 합산 평탄도 검증
#   DIBlendTest.cpp      — Blend 경계값(0%/50%/100%) 및 레벨 트림 정확도 검증
#   PresetTest.cpp       — ValueTree 직렬화 테스트
#==============================================================================
add_executable(BassMusicGear_Tests
    SmokeTest.cpp
    # Phase 1 이후 테스트 파일이 여기 추가된다:
    # ToneStackTest.cpp
    # OverdriveTest.cpp
    # CompressorTest.cpp
    # BiAmpCrossoverTest.cpp
    # DIBlendTest.cpp
    # PresetTest.cpp
)

#==============================================================================
# 컴파일 옵션
# cxx_std_17: 테스트 코드도 C++17 표준을 사용한다.
#             구조적 바인딩, if constexpr 등 C++17 기능을 테스트 코드에서 활용할 수 있다.
#==============================================================================
target_compile_features(BassMusicGear_Tests PRIVATE cxx_std_17)

# Source/ 디렉터리를 포함 경로에 추가해 PluginProcessor.h 등 플러그인 헤더에
# 접근할 수 있도록 한다. DSP 클래스를 직접 단위 테스트할 때 필요하다.
target_include_directories(BassMusicGear_Tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/Source"
)

#==============================================================================
# 링크 라이브러리
# Catch2::Catch2WithMain: Catch2 프레임워크 + main() 자동 생성
# juce_audio_basics: AudioBuffer, AudioSampleBuffer 등 기본 오디오 타입
# juce_audio_processors: AudioProcessor, APVTS 등 플러그인 인프라
# juce_dsp: Oversampling, IIR 필터 등 DSP 단위 테스트에 필요한 모듈
#==============================================================================
target_link_libraries(BassMusicGear_Tests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

#==============================================================================
# CTest 등록 (Catch2 CMake 통합)
# catch_discover_tests()는 실행 파일을 실행해 TEST_CASE 목록을 자동으로 수집하고
# 각 케이스를 CTest 테스트 항목으로 등록한다.
# 이를 통해 ctest --test-dir build -R <패턴>으로 특정 테스트만 선택 실행할 수 있다.
#==============================================================================
include(CTest)
include(Catch)
catch_discover_tests(BassMusicGear_Tests)
