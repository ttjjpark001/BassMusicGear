//==============================================================================
// SmokeTest.cpp — Phase 0 더미 테스트
//
// 빌드 환경(CMake + JUCE + Catch2) 이 올바르게 구성되었는지 확인한다.
// 이 테스트는 항상 통과한다.
//==============================================================================

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Smoke test — build environment is functional", "[smoke]")
{
    // 빌드 시스템, JUCE, Catch2 가 모두 올바르게 연결되었으면 이 테스트가 통과한다
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("Smoke test — basic arithmetic", "[smoke]")
{
    // 부동소수점 기본 연산 확인
    float a = 0.5f;
    float b = 0.5f;
    REQUIRE(a + b == 1.0f);
}
