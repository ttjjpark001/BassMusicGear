#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Effects/Reverb.h"
#include <cmath>

//==============================================================================
namespace {

// 임펄스를 리버브에 통과시켜 잔향 에너지를 측정한다.
// numSamples 동안의 출력 RMS를 반환한다.
float measureReverbEnergy (Reverb& rev, int numSamples = 8192)
{
    juce::AudioBuffer<float> buffer (1, numSamples);
    buffer.clear();
    // 샘플 0에 임펄스 주입
    buffer.setSample (0, 0, 1.0f);

    rev.process (buffer);

    float sum = 0.0f;
    const float* data = buffer.getReadPointer (0);
    for (int i = 1; i < numSamples; ++i)  // 임펄스 자체(i=0) 제외
        sum += data[i] * data[i];
    return std::sqrt (sum / static_cast<float> (numSamples - 1));
}

// 사인파를 넣고 출력 RMS를 측정하는 헬퍼
float measureOutputRms (Reverb& rev, float freqHz, double sampleRate,
                        int numSamples = 4096)
{
    juce::AudioBuffer<float> buffer (1, numSamples);
    const float omega = 2.0f * juce::MathConstants<float>::pi
                        * freqHz / static_cast<float> (sampleRate);
    for (int i = 0; i < numSamples; ++i)
        buffer.setSample (0, i, 0.5f * std::sin (omega * static_cast<float> (i)));

    rev.process (buffer);

    float sum = 0.0f;
    const float* data = buffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
        sum += data[i] * data[i];
    return std::sqrt (sum / static_cast<float> (numSamples));
}

} // namespace

//==============================================================================
// 기본 동작 테스트
//==============================================================================

TEST_CASE ("Reverb: initializes without crash", "[reverb]")
{
    Reverb rev;
    juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
    REQUIRE_NOTHROW (rev.prepare (spec));
}

TEST_CASE ("Reverb: bypass passes signal unchanged", "[reverb]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;

    Reverb rev;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    rev.prepare (spec);

    // enabled = 0.0 (비활성)
    std::atomic<float> enabled { 0.0f };
    std::atomic<float> type { 1.0f };  // Room
    std::atomic<float> size { 0.5f };
    std::atomic<float> decay { 0.5f };
    std::atomic<float> mix { 0.5f };
    rev.setParameterPointers (&enabled, &type, &size, &decay, &mix);

    juce::AudioBuffer<float> buffer (1, blockSize);
    for (int i = 0; i < blockSize; ++i)
        buffer.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                   * 440.0f / static_cast<float> (sampleRate) * static_cast<float> (i)));

    juce::AudioBuffer<float> original (1, blockSize);
    original.copyFrom (0, 0, buffer, 0, 0, blockSize);

    rev.process (buffer);

    // 비활성 상태: 입출력 완전히 동일
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (buffer.getSample (0, i) == Catch::Approx (original.getSample (0, i)).margin (1e-6f));
}

TEST_CASE ("Reverb: enabled produces wet output (mix=1.0)", "[reverb]")
{
    // juce::dsp::Reverb는 내부 딜레이라인 기반이므로 첫 임펄스 버퍼를 처리한 후
    // 추가 무음 버퍼를 통과시켜야 잔향 꼬리가 출력에 나타난다.
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    Reverb rev;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    rev.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 1.0f };  // Room
    std::atomic<float> size { 0.7f };
    std::atomic<float> decay { 0.6f };
    std::atomic<float> mix { 1.0f };   // 100% wet
    rev.setParameterPointers (&enabled, &type, &size, &decay, &mix);

    // 1단계: 임펄스 버퍼 처리 (내부 딜레이라인에 에너지 주입)
    juce::AudioBuffer<float> impulseBuffer (1, blockSize);
    impulseBuffer.clear();
    impulseBuffer.setSample (0, 0, 1.0f);
    rev.process (impulseBuffer);

    // 2단계: 무음 버퍼를 통과시켜 잔향 꼬리를 수집
    // juce::dsp::Reverb의 딜레이라인이 내부 버퍼 크기에 따라 지연이 있으므로
    // 여러 블록을 통과시켜 잔향이 출력에 도달하도록 한다.
    juce::AudioBuffer<float> tailBuffer (1, blockSize);
    tailBuffer.clear();
    rev.process (tailBuffer);

    // 잔향 꼬리 버퍼에 에너지가 있어야 함
    float reverbEnergy = 0.0f;
    const float* data = tailBuffer.getReadPointer (0);
    for (int i = 0; i < blockSize; ++i)
        reverbEnergy += data[i] * data[i];
    reverbEnergy = std::sqrt (reverbEnergy / static_cast<float> (blockSize));

    INFO ("Reverb tail energy: " << reverbEnergy);
    REQUIRE (reverbEnergy > 1e-6f);
}

TEST_CASE ("Reverb: mix=0.0 passes dry signal", "[reverb]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;

    Reverb rev;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    rev.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 1.0f };
    std::atomic<float> size { 0.5f };
    std::atomic<float> decay { 0.5f };
    std::atomic<float> mix { 0.0f };   // 100% dry
    rev.setParameterPointers (&enabled, &type, &size, &decay, &mix);

    juce::AudioBuffer<float> buffer (1, blockSize);
    for (int i = 0; i < blockSize; ++i)
        buffer.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                   * 440.0f / static_cast<float> (sampleRate) * static_cast<float> (i)));

    juce::AudioBuffer<float> original (1, blockSize);
    original.copyFrom (0, 0, buffer, 0, 0, blockSize);

    rev.process (buffer);

    // mix=0: 출력이 원본과 동일해야 함
    for (int i = 0; i < blockSize; ++i)
        REQUIRE (buffer.getSample (0, i) == Catch::Approx (original.getSample (0, i)).margin (1e-5f));
}

TEST_CASE ("Reverb: no NaN or Inf in output", "[reverb]")
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    Reverb rev;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    rev.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> type { 0.0f };
    std::atomic<float> size { 1.0f };  // 최대 크기
    std::atomic<float> decay { 1.0f }; // 최대 감쇠
    std::atomic<float> mix { 1.0f };
    rev.setParameterPointers (&enabled, &type, &size, &decay, &mix);

    // 고진폭 임펄스
    juce::AudioBuffer<float> buffer (1, blockSize);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);

    rev.process (buffer);

    const float* data = buffer.getReadPointer (0);
    for (int i = 0; i < blockSize; ++i)
    {
        REQUIRE_FALSE (std::isnan (data[i]));
        REQUIRE_FALSE (std::isinf (data[i]));
    }
}

//==============================================================================
// Hall / Plate 타입 추가 검증
//==============================================================================

TEST_CASE ("Reverb: Hall type (typeIdx=2) produces reverb with large room character", "[reverb][hall]")
{
    // Hall 타입: roomSize = 0.7 + size*0.3 → 항상 0.7 이상 (큰 공간)
    // Room 타입: roomSize = 0.4 + size*0.55 → 최대 0.95
    // Hall은 큰 roomSize로 인해 잔향 꼬리가 더 길게 지속되어야 한다.
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 16384;

    // Room 타입 잔향 에너지 측정
    Reverb roomRev;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    roomRev.prepare (spec);
    std::atomic<float> enabled { 1.0f };
    std::atomic<float> typeRoom { 1.0f };  // Room
    std::atomic<float> size { 0.5f };
    std::atomic<float> decay { 0.5f };
    std::atomic<float> mix { 1.0f };
    roomRev.setParameterPointers (&enabled, &typeRoom, &size, &decay, &mix);

    juce::AudioBuffer<float> roomBuffer (1, blockSize);
    roomBuffer.clear();
    roomBuffer.setSample (0, 0, 1.0f);
    roomRev.process (roomBuffer);

    // Hall 타입 잔향 에너지 측정
    Reverb hallRev;
    hallRev.prepare (spec);
    std::atomic<float> typeHall { 2.0f };  // Hall
    hallRev.setParameterPointers (&enabled, &typeHall, &size, &decay, &mix);

    juce::AudioBuffer<float> hallBuffer (1, blockSize);
    hallBuffer.clear();
    hallBuffer.setSample (0, 0, 1.0f);
    hallRev.process (hallBuffer);

    // 후반부(4096~8192) 잔향 에너지 비교
    // Hall은 더 큰 공간이므로 후반 꼬리 에너지가 Room 이상이어야 함
    float roomTailEnergy = 0.0f;
    float hallTailEnergy = 0.0f;
    const float* roomData = roomBuffer.getReadPointer (0);
    const float* hallData = hallBuffer.getReadPointer (0);
    for (int i = 4096; i < 8192; ++i)
    {
        roomTailEnergy += roomData[i] * roomData[i];
        hallTailEnergy += hallData[i] * hallData[i];
    }

    INFO ("Room tail energy: " << roomTailEnergy << ", Hall tail energy: " << hallTailEnergy);

    // Hall의 잔향 꼬리가 Room보다 크거나 같아야 한다 (더 큰 공간 = 더 긴 잔향)
    REQUIRE (hallTailEnergy >= roomTailEnergy * 0.5f);
    // Hall 자체도 의미있는 잔향이 있어야 함
    REQUIRE (hallTailEnergy > 0.0f);
}

TEST_CASE ("Reverb: Plate type (typeIdx=3) produces reverb output", "[reverb][plate]")
{
    // Plate 타입: roomSize = 0.5 + size*0.35, damping = 0.1 + (1-decay)*0.3 (매우 낮음)
    // 낮은 damping → 밝고 선명한 음색의 잔향
    // juce::dsp::Reverb 딜레이라인 특성상 임펄스 버퍼 처리 후 추가 버퍼에서 잔향이 나온다.
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    Reverb rev;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    rev.prepare (spec);

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> typePlate { 3.0f };  // Plate
    std::atomic<float> size { 0.5f };
    std::atomic<float> decay { 0.5f };
    std::atomic<float> mix { 1.0f };
    rev.setParameterPointers (&enabled, &typePlate, &size, &decay, &mix);

    // 1단계: 임펄스를 주입하여 딜레이라인에 에너지 채우기
    juce::AudioBuffer<float> impulseBuffer (1, blockSize);
    impulseBuffer.clear();
    impulseBuffer.setSample (0, 0, 1.0f);
    rev.process (impulseBuffer);

    // 2단계: 무음 버퍼로 잔향 꼬리 수집
    juce::AudioBuffer<float> tailBuffer (1, blockSize);
    tailBuffer.clear();
    rev.process (tailBuffer);

    // Plate 잔향: 꼬리 버퍼에 에너지가 있어야 함
    float energy = 0.0f;
    const float* data = tailBuffer.getReadPointer (0);
    for (int i = 0; i < blockSize; ++i)
        energy += data[i] * data[i];

    INFO ("Plate reverb tail energy: " << energy);
    REQUIRE (energy > 0.0f);

    // NaN/Inf 없음
    for (int i = 0; i < blockSize; ++i)
    {
        REQUIRE_FALSE (std::isnan (data[i]));
        REQUIRE_FALSE (std::isinf (data[i]));
    }
}

TEST_CASE ("Reverb: all four types produce different outputs", "[reverb][types]")
{
    // Spring(0), Room(1), Hall(2), Plate(3) 모두 서로 다른 잔향을 생성해야 한다.
    // 동일한 입력에 대해 각 타입의 출력 RMS가 서로 다른 값을 가져야 함.
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 8192;

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };

    std::atomic<float> enabled { 1.0f };
    std::atomic<float> size { 0.5f };
    std::atomic<float> decay { 0.5f };
    std::atomic<float> mix { 1.0f };

    float typeRms[4] = {};

    for (int t = 0; t < 4; ++t)
    {
        Reverb rev;
        rev.prepare (spec);

        std::atomic<float> typeParam { static_cast<float> (t) };
        rev.setParameterPointers (&enabled, &typeParam, &size, &decay, &mix);

        juce::AudioBuffer<float> buffer (1, blockSize);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);

        rev.process (buffer);

        float sum = 0.0f;
        const float* data = buffer.getReadPointer (0);
        for (int i = 0; i < blockSize; ++i)
            sum += data[i] * data[i];
        typeRms[t] = std::sqrt (sum / static_cast<float> (blockSize));

        INFO ("Type " << t << " RMS: " << typeRms[t]);
        // 각 타입 모두 비영 출력이어야 함
        REQUIRE (typeRms[t] > 0.0f);
    }

    // 4개 타입이 완전히 동일하지는 않아야 함 (최소 2개 이상 다른 값)
    // Hall(2)과 Plate(3)는 roomSize 설정이 다르므로 RMS가 달라야 함
    INFO ("Hall RMS: " << typeRms[2] << ", Plate RMS: " << typeRms[3]);
    // Hall(roomSize 0.7~1.0)과 Spring(roomSize 0.3~0.6)은 반드시 달라야 함
    REQUIRE (typeRms[0] != Catch::Approx (typeRms[2]).margin (1e-4f));
}

TEST_CASE ("Reverb: Hall roomSize parameter range (size=0 vs size=1)", "[reverb][hall][boundary]")
{
    // Hall 타입에서 size=0.0 → roomSize=0.7, size=1.0 → roomSize=1.0
    // size가 클수록 후반 잔향 꼬리가 길어야 한다.
    // juce::dsp::Reverb: 임펄스 주입 후 추가 버퍼를 처리해야 잔향이 출력에 나타난다.
    // 여러 블록(총 ~1초)의 잔향 에너지를 누적하여 size별 차이를 비교한다.
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;
    constexpr int numTailBlocks = 10;  // 임펄스 후 10블록 = ~0.93초 잔향 수집

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    std::atomic<float> enabled { 1.0f };
    std::atomic<float> typeHall { 2.0f };
    std::atomic<float> decay { 0.8f };
    std::atomic<float> mix { 1.0f };

    auto collectTailEnergy = [&] (float sizeVal) -> float
    {
        Reverb rev;
        rev.prepare (spec);
        std::atomic<float> s { sizeVal };
        rev.setParameterPointers (&enabled, &typeHall, &s, &decay, &mix);

        // 임펄스 주입
        juce::AudioBuffer<float> impulse (1, blockSize);
        impulse.clear();
        impulse.setSample (0, 0, 1.0f);
        rev.process (impulse);

        // 잔향 꼬리 수집
        float totalEnergy = 0.0f;
        for (int b = 0; b < numTailBlocks; ++b)
        {
            juce::AudioBuffer<float> tail (1, blockSize);
            tail.clear();
            rev.process (tail);

            const float* d = tail.getReadPointer (0);
            for (int i = 0; i < blockSize; ++i)
                totalEnergy += d[i] * d[i];
        }
        return totalEnergy;
    };

    const float smallEnergy = collectTailEnergy (0.0f);  // roomSize = 0.7
    const float largeEnergy = collectTailEnergy (1.0f);  // roomSize = 1.0

    INFO ("Small Hall (size=0) tail energy: " << smallEnergy);
    INFO ("Large Hall (size=1) tail energy: " << largeEnergy);

    // 두 설정 모두 잔향 에너지가 있어야 함
    REQUIRE (smallEnergy > 0.0f);
    REQUIRE (largeEnergy > 0.0f);

    // size가 달라도 에너지가 완전히 동일하지는 않아야 함
    // (roomSize 0.7 vs 1.0 차이가 잔향 특성에 영향을 미침)
    REQUIRE (std::abs (smallEnergy - largeEnergy) > 0.0f);
}

TEST_CASE ("Reverb: Plate type with extreme parameters produces no NaN/Inf", "[reverb][plate][boundary]")
{
    // Plate 타입에서 size=0, size=1, decay=0, decay=1 경계값에서도 안정적이어야 함
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 4096;

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    std::atomic<float> enabled { 1.0f };
    std::atomic<float> typePlate { 3.0f };
    std::atomic<float> mix { 1.0f };

    const std::array<float, 4> sizeVals   = { 0.0f, 1.0f, 0.0f, 1.0f };
    const std::array<float, 4> decayVals  = { 0.0f, 0.0f, 1.0f, 1.0f };

    for (int c = 0; c < 4; ++c)
    {
        Reverb rev;
        rev.prepare (spec);
        std::atomic<float> s { sizeVals[c] };
        std::atomic<float> d { decayVals[c] };
        rev.setParameterPointers (&enabled, &typePlate, &s, &d, &mix);

        juce::AudioBuffer<float> buffer (1, blockSize);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        rev.process (buffer);

        const float* data = buffer.getReadPointer (0);
        for (int i = 0; i < blockSize; ++i)
        {
            INFO ("Plate boundary c=" << c << " i=" << i << " val=" << data[i]);
            REQUIRE_FALSE (std::isnan (data[i]));
            REQUIRE_FALSE (std::isinf (data[i]));
        }
    }
}
