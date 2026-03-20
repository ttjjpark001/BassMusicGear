/**
 * @file SmokeTest.cpp
 * @brief Phase 0 빌드 환경 검증 테스트
 *
 * CMake + JUCE + Catch2 툴체인이 올바르게 구성되었는지 확인한다.
 * 실제 DSP 로직을 검증하는 것이 아니라, 테스트 인프라 자체가 동작하는지
 * 확인하는 것이 목적이다. 이 파일의 테스트들은 항상 통과해야 한다.
 *
 * Phase 1 이후에는 ToneStackTest.cpp, OverdriveTest.cpp 등이 추가된다.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// 빌드 시스템(CMake), JUCE 헤더, Catch2가 모두 올바르게 연결되었는지 확인한다.
// 이 케이스가 컴파일·링크·실행까지 통과하면 개발 환경이 정상적으로 구성된 것이다.
TEST_CASE("Smoke test — build environment is functional", "[smoke]")
{
    REQUIRE(1 + 1 == 2);
}

// float 산술이 기대값과 정확히 일치하는지 확인한다.
// Catch2::Approx를 사용해 부동소수점 비교 시 epsilon 오차를 허용한다.
// 직접 == 비교는 컴파일러 최적화나 FPU 설정에 따라 예기치 않게 실패할 수 있다.
TEST_CASE("Smoke test — basic float arithmetic", "[smoke]")
{
    float a = 0.5f;
    float b = 0.5f;
    REQUIRE(static_cast<double>(a + b) == Catch::Approx(1.0));
}
