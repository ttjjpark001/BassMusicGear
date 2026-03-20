# Tests/CMakeLists.txt
# Catch2 기반 단위 테스트 타겟 정의

cmake_minimum_required(VERSION 3.22)

# Catch2 는 상위 CMakeLists.txt 에서 FetchContent 로 가져온다

#==============================================================================
# 테스트 실행 파일 정의
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
#==============================================================================
target_compile_features(BassMusicGear_Tests PRIVATE cxx_std_17)

# JUCE 헤더 및 플러그인 프로세서 헤더 접근
target_include_directories(BassMusicGear_Tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/Source"
)

#==============================================================================
# 링크 라이브러리
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
#==============================================================================
include(CTest)
include(Catch)
catch_discover_tests(BassMusicGear_Tests)
