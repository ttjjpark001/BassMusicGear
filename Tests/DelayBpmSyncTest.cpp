/**
 * DelayBpmSyncTest.cpp
 *
 * Phase 7 단위 테스트 — Delay BPM Sync 로직 검증
 *
 * 검증 항목:
 *  A. bpm=120, note=1/4(인덱스 0) → delay_time_ms ≈ 500ms
 *  B. bpm=140, note=1/8(인덱스 1) → delay_time_ms ≈ 214.3ms
 *  C. BPM Sync OFF → delay_time 노브 값 그대로 사용 (BPM 무관)
 *  D. 다양한 BPM/노트 조합의 계산값 정확도 검증
 *
 * 구현 방식:
 *  Delay::process()에서 BPM Sync ON 시 아래 공식으로 딜레이 시간을 계산한다.
 *    delay_time_ms = (60000 / bpm) × noteFraction
 *  noteFraction 변환표:
 *    0→1.0(1/4박), 1→0.5(1/8박), 2→0.75(1/8점), 3→0.25(1/16박), 4→2/3(1/4삼연음)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/Effects/Delay.h"
#include <cmath>
#include <array>

namespace {

constexpr double kSampleRate = 44100.0;
constexpr int    kBlockSize  = 4096;   // BPM sync 측정에 충분한 블록 크기

/**
 * BPM + 노트 인덱스로 예상 딜레이 시간(ms)을 계산하는 참조 공식.
 * Delay.cpp 의 noteIndexToFraction + setBpm 공식과 동일.
 */
float expectedDelayMs (double bpm, int noteIndex)
{
    float fraction = 1.0f;
    switch (noteIndex)
    {
        case 0: fraction = 1.0f;        break;  // 1/4박
        case 1: fraction = 0.5f;        break;  // 1/8박
        case 2: fraction = 0.75f;       break;  // 1/8점박
        case 3: fraction = 0.25f;       break;  // 1/16박
        case 4: fraction = 2.0f / 3.0f; break;  // 1/4 삼연음
        default: fraction = 1.0f;       break;
    }
    return static_cast<float> ((60000.0 / bpm) * fraction);
}

/**
 * Delay DSP를 설정하고 BPM Sync ON 상태에서 딜레이 시간을 측정한다.
 *
 * 측정 방법:
 * 1. 첫 번째 샘플에만 임펄스(1.0f)를 주입한다.
 * 2. 버퍼를 처리하면 딜레이 시간 후 임펄스의 에코가 나타난다.
 * 3. 출력 버퍼에서 첫 번째 큰 피크의 위치를 검출하여 딜레이 샘플 수를 구한다.
 * 4. 딜레이 샘플 수 → ms 변환 후 반환한다.
 *
 * @param bpm        DAW BPM 설정값
 * @param noteIndex  노트값 인덱스 (0~4)
 * @param numBlocks  측정에 사용할 버퍼 블록 수 (긴 딜레이는 더 많이 필요)
 * @return           측정된 딜레이 시간 (ms), 피크 미검출 시 -1.0f
 */
float measureDelayTimeMs (double bpm, int noteIndex, int numBlocks = 4)
{
    Delay delay;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = kSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (kBlockSize);
    spec.numChannels      = 1;

    delay.prepare (spec);
    delay.setBpm (bpm);

    // 파라미터 설정: BPM Sync ON, 피드백 0, Mix 100% Wet
    std::atomic<float> enabled   { 1.0f };
    std::atomic<float> timeMs    { 500.0f };  // Sync OFF 시 기본값 (이 테스트에선 무시)
    std::atomic<float> feedback  { 0.0f };    // 피드백 없음 (단일 에코만)
    std::atomic<float> damping   { 0.0f };    // 감쇠 없음 (밝은 에코)
    std::atomic<float> mix       { 1.0f };    // 100% Wet (입력 제거, 딜레이만 출력)
    std::atomic<float> bpmSync   { 1.0f };    // BPM Sync ON
    std::atomic<float> noteValue { static_cast<float> (noteIndex) };

    delay.setParameterPointers (&enabled, &timeMs, &feedback, &damping,
                                &mix, &bpmSync, &noteValue);

    // 총 측정 샘플 수
    const int totalSamples = kBlockSize * numBlocks;
    std::vector<float> allSamples (static_cast<size_t> (totalSamples), 0.0f);

    // 첫 샘플에 임펄스 주입
    allSamples[0] = 1.0f;

    // 블록 단위로 처리
    for (int block = 0; block < numBlocks; ++block)
    {
        juce::AudioBuffer<float> buf (1, kBlockSize);
        const int offset = block * kBlockSize;

        // 버퍼에 샘플 복사
        float* ptr = buf.getWritePointer (0);
        for (int i = 0; i < kBlockSize; ++i)
            ptr[i] = allSamples[static_cast<size_t> (offset + i)];

        delay.process (buf);

        // 처리 결과 저장
        const float* result = buf.getReadPointer (0);
        for (int i = 0; i < kBlockSize; ++i)
            allSamples[static_cast<size_t> (offset + i)] = result[i];
    }

    // 출력에서 첫 번째 큰 피크 위치 검출
    // mix=1.0 (100% Wet) 이므로 샘플 0에는 입력 임펄스가 거의 없고
    // delay 시간 후에 임펄스의 에코가 나타난다.
    // 임펄스가 지연된 위치에서 0.1 이상의 값이 나타나면 그것이 딜레이 시간이다.
    constexpr float peakThreshold = 0.05f;
    constexpr int   searchStart   = 100;   // 첫 100샘플(약 2.3ms)은 건너뜀

    for (int i = searchStart; i < totalSamples; ++i)
    {
        if (std::abs (allSamples[static_cast<size_t> (i)]) > peakThreshold)
        {
            // 딜레이 샘플 수 → ms
            return static_cast<float> (i) / static_cast<float> (kSampleRate) * 1000.0f;
        }
    }

    return -1.0f;  // 피크 미검출
}

/**
 * Delay DSP를 Sync OFF 상태에서 실행하여 timeMs 파라미터 값 그대로 딜레이를 생성하는지 확인.
 * measureDelayTimeMs와 동일한 임펄스 피크 검출 방식 사용.
 */
float measureDelayTimeMsManual (float manualTimeMs, int numBlocks = 4)
{
    Delay delay;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = kSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (kBlockSize);
    spec.numChannels      = 1;

    delay.prepare (spec);
    delay.setBpm (120.0);  // BPM 값은 Sync OFF에서 무시됨

    std::atomic<float> enabled   { 1.0f };
    std::atomic<float> timeMs    { manualTimeMs };
    std::atomic<float> feedback  { 0.0f };
    std::atomic<float> damping   { 0.0f };
    std::atomic<float> mix       { 1.0f };   // 100% Wet
    std::atomic<float> bpmSync   { 0.0f };   // BPM Sync OFF
    std::atomic<float> noteValue { 0.0f };

    delay.setParameterPointers (&enabled, &timeMs, &feedback, &damping,
                                &mix, &bpmSync, &noteValue);

    const int totalSamples = kBlockSize * numBlocks;
    std::vector<float> allSamples (static_cast<size_t> (totalSamples), 0.0f);
    allSamples[0] = 1.0f;

    for (int block = 0; block < numBlocks; ++block)
    {
        juce::AudioBuffer<float> buf (1, kBlockSize);
        const int offset = block * kBlockSize;

        float* ptr = buf.getWritePointer (0);
        for (int i = 0; i < kBlockSize; ++i)
            ptr[i] = allSamples[static_cast<size_t> (offset + i)];

        delay.process (buf);

        const float* result = buf.getReadPointer (0);
        for (int i = 0; i < kBlockSize; ++i)
            allSamples[static_cast<size_t> (offset + i)] = result[i];
    }

    constexpr float peakThreshold = 0.05f;
    constexpr int   searchStart   = 100;

    for (int i = searchStart; i < totalSamples; ++i)
    {
        if (std::abs (allSamples[static_cast<size_t> (i)]) > peakThreshold)
            return static_cast<float> (i) / static_cast<float> (kSampleRate) * 1000.0f;
    }

    return -1.0f;
}

} // namespace

//==============================================================================
// A. bpm=120, note=1/4(인덱스 0) → delay_time_ms ≈ 500ms
//==============================================================================

TEST_CASE ("DelayBpmSync: 120BPM quarter note = 500ms",
           "[delay][bpmsync][phase7]")
{
    constexpr double bpm       = 120.0;
    constexpr int    noteIndex = 0;  // 1/4박 → fraction=1.0
    const float expected = expectedDelayMs (bpm, noteIndex);  // 500.0ms

    INFO ("Expected delay: " << expected << " ms");
    REQUIRE (expected == Catch::Approx (500.0f).margin (0.1f));

    // 실제 Delay DSP가 공식과 동일하게 동작하는지 임펄스 응답으로 검증
    // 500ms = 22050 샘플 → 버퍼 6개(24576 샘플)로 충분히 커버
    const float measured = measureDelayTimeMs (bpm, noteIndex, /*numBlocks=*/ 6);
    INFO ("Measured delay: " << measured << " ms");

    REQUIRE (measured > 0.0f);  // 피크 검출 성공
    // ±10ms 허용 오차 (샘플 정밀도 한계: 1/44100 × 1000 ≈ 0.023ms, 반올림 포함)
    REQUIRE (measured == Catch::Approx (expected).margin (10.0f));
}

//==============================================================================
// B. bpm=140, note=1/8(인덱스 1) → delay_time_ms ≈ 214.3ms
//==============================================================================

TEST_CASE ("DelayBpmSync: 140BPM eighth note = 214.3ms",
           "[delay][bpmsync][phase7]")
{
    constexpr double bpm       = 140.0;
    constexpr int    noteIndex = 1;  // 1/8박 → fraction=0.5
    const float expected = expectedDelayMs (bpm, noteIndex);  // ≈214.286ms

    INFO ("Expected delay: " << expected << " ms");
    // 140 BPM, 1/8박: (60000/140) × 0.5 ≈ 214.286ms
    REQUIRE (expected == Catch::Approx (214.286f).margin (0.1f));

    // 214ms = 9440 샘플 → 버퍼 3개(12288 샘플)로 충분
    const float measured = measureDelayTimeMs (bpm, noteIndex, /*numBlocks=*/ 4);
    INFO ("Measured delay: " << measured << " ms");

    REQUIRE (measured > 0.0f);
    REQUIRE (measured == Catch::Approx (expected).margin (10.0f));
}

//==============================================================================
// C. BPM Sync OFF → delay_time 노브 값 그대로 사용 (BPM 무관)
//==============================================================================

TEST_CASE ("DelayBpmSync: Sync OFF uses manual time knob, ignores BPM",
           "[delay][bpmsync][phase7][syncoff]")
{
    SECTION ("Manual 300ms: BPM Sync OFF에서 300ms 딜레이")
    {
        const float manualTimeMs = 300.0f;
        const float measured = measureDelayTimeMsManual (manualTimeMs, /*numBlocks=*/ 4);

        INFO ("Manual time = " << manualTimeMs << " ms, Measured = " << measured << " ms");
        REQUIRE (measured > 0.0f);
        REQUIRE (measured == Catch::Approx (manualTimeMs).margin (10.0f));
    }

    SECTION ("Manual 100ms: BPM Sync OFF에서 100ms 딜레이")
    {
        const float manualTimeMs = 100.0f;
        const float measured = measureDelayTimeMsManual (manualTimeMs, /*numBlocks=*/ 2);

        INFO ("Manual time = " << manualTimeMs << " ms, Measured = " << measured << " ms");
        REQUIRE (measured > 0.0f);
        REQUIRE (measured == Catch::Approx (manualTimeMs).margin (10.0f));
    }
}

//==============================================================================
// D. 추가 BPM/노트 조합 — 참조 공식 정확도 검증 (계산 단위 테스트)
//==============================================================================

TEST_CASE ("DelayBpmSync: note fraction calculation correctness",
           "[delay][bpmsync][phase7][formula]")
{
    // 공식: delay_ms = (60000 / bpm) × noteFraction
    // noteFraction 변환표 단독 검증 (DSP 없이 참조 공식만)

    SECTION ("120 BPM, 1/4 (idx=0) → 500ms")
    {
        REQUIRE (expectedDelayMs (120.0, 0) == Catch::Approx (500.0f).margin (0.1f));
    }

    SECTION ("120 BPM, 1/8 (idx=1) → 250ms")
    {
        REQUIRE (expectedDelayMs (120.0, 1) == Catch::Approx (250.0f).margin (0.1f));
    }

    SECTION ("120 BPM, 1/8점 (idx=2) → 375ms")
    {
        REQUIRE (expectedDelayMs (120.0, 2) == Catch::Approx (375.0f).margin (0.1f));
    }

    SECTION ("120 BPM, 1/16 (idx=3) → 125ms")
    {
        REQUIRE (expectedDelayMs (120.0, 3) == Catch::Approx (125.0f).margin (0.1f));
    }

    SECTION ("120 BPM, 1/4삼연음 (idx=4) → 333.3ms")
    {
        REQUIRE (expectedDelayMs (120.0, 4) == Catch::Approx (333.33f).margin (0.5f));
    }

    SECTION ("140 BPM, 1/8 (idx=1) → 214.3ms")
    {
        REQUIRE (expectedDelayMs (140.0, 1) == Catch::Approx (214.286f).margin (0.1f));
    }

    SECTION ("90 BPM, 1/4 (idx=0) → 666.7ms")
    {
        REQUIRE (expectedDelayMs (90.0, 0) == Catch::Approx (666.67f).margin (0.5f));
    }

    SECTION ("180 BPM, 1/16 (idx=3) → 83.3ms")
    {
        REQUIRE (expectedDelayMs (180.0, 3) == Catch::Approx (83.33f).margin (0.5f));
    }
}

//==============================================================================
// D-2. BPM Sync ON vs OFF가 다른 딜레이 시간을 생성하는지 비교
//==============================================================================

TEST_CASE ("DelayBpmSync: Sync ON result differs from manual time when they mismatch",
           "[delay][bpmsync][phase7][comparison]")
{
    // Sync ON: 120 BPM, 1/4박 → 500ms
    // Sync OFF: 수동 300ms
    // 두 결과가 서로 달라야 한다

    const float syncOnMs  = measureDelayTimeMs (120.0, 0, 6);   // ≈500ms
    const float syncOffMs = measureDelayTimeMsManual (300.0f, 4); // ≈300ms

    INFO ("Sync ON measured: " << syncOnMs << " ms, Sync OFF measured: " << syncOffMs << " ms");

    REQUIRE (syncOnMs  > 0.0f);
    REQUIRE (syncOffMs > 0.0f);

    // 두 결과의 차이가 50ms 이상 (500ms vs 300ms → 200ms 차이이므로 충분)
    REQUIRE (std::abs (syncOnMs - syncOffMs) > 50.0f);
}
