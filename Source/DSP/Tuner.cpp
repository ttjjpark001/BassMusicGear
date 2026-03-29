#include "Tuner.h"
#include <cmath>

static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F",
                                    "F#", "G", "G#", "A", "A#", "B" };

const char* Tuner::getNoteName (int index)
{
    if (index < 0 || index > 11) return "?";
    return noteNames[index];
}

void Tuner::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reset();
}

void Tuner::reset()
{
    inputBuffer.fill (0.0f);
    inputWritePos = 0;
    bufferFull = false;
    detectedHz.store (0.0f);
    centsDeviation.store (0.0f);
    noteIndex.store (-1);
    noteDetected.store (false);
}

void Tuner::setParameterPointers (std::atomic<float>* referenceA,
                                   std::atomic<float>* muteEnabled)
{
    referenceAParam = referenceA;
    muteParam       = muteEnabled;
}

void Tuner::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const float* readPtr = buffer.getReadPointer (0);

    // 샘플을 내부 링버퍼에 누적
    for (int i = 0; i < numSamples; ++i)
    {
        inputBuffer[(size_t) inputWritePos] = readPtr[i];
        ++inputWritePos;

        if (inputWritePos >= kYinBufferSize)
        {
            inputWritePos = 0;
            bufferFull = true;
        }
    }

    // 버퍼가 가득 찼으면 피치 감지 수행
    if (bufferFull)
    {
        bufferFull = false;
        detectPitch();
    }

    // Mute 모드: 튜닝 중 출력 신호 뮤트
    if (muteParam != nullptr && muteParam->load() >= 0.5f)
    {
        buffer.clear();
    }
}

void Tuner::detectPitch()
{
    const int halfSize = kYinBufferSize / 2;

    // Step 1: Difference function
    // d(tau) = sum_{j=0}^{W-1} (x[j] - x[j+tau])^2
    for (int tau = 0; tau < halfSize; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < halfSize; ++j)
        {
            float diff = inputBuffer[(size_t) j] - inputBuffer[(size_t) (j + tau)];
            sum += diff * diff;
        }
        yinBuffer[(size_t) tau] = sum;
    }

    // Step 2: Cumulative mean normalized difference function
    // d'(0) = 1
    // d'(tau) = d(tau) / ((1/tau) * sum_{j=1}^{tau} d(j))
    yinBuffer[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau < halfSize; ++tau)
    {
        runningSum += yinBuffer[(size_t) tau];
        if (runningSum > 0.0f)
            yinBuffer[(size_t) tau] *= static_cast<float> (tau) / runningSum;
        else
            yinBuffer[(size_t) tau] = 1.0f;
    }

    // Step 3: Absolute threshold
    // 최소 tau 범위를 주파수 범위로 제한: 41Hz~330Hz
    // tau_min = sampleRate / 330, tau_max = sampleRate / 41
    const int tauMin = std::max (2, static_cast<int> (sampleRate / 330.0));
    const int tauMax = std::min (halfSize - 1, static_cast<int> (sampleRate / 41.0));

    int bestTau = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (yinBuffer[(size_t) tau] < kYinThreshold)
        {
            // 임계값 이하인 첫 번째 딥의 가장 작은 값을 찾는다
            while (tau + 1 <= tauMax && yinBuffer[(size_t) (tau + 1)] < yinBuffer[(size_t) tau])
                ++tau;
            bestTau = tau;
            break;
        }
    }

    if (bestTau < 0)
    {
        // 피치 감지 실패
        noteDetected.store (false);
        return;
    }

    // Step 4: Parabolic interpolation
    float betterTau = static_cast<float> (bestTau);
    if (bestTau > 0 && bestTau < halfSize - 1)
    {
        float s0 = yinBuffer[(size_t) (bestTau - 1)];
        float s1 = yinBuffer[(size_t) bestTau];
        float s2 = yinBuffer[(size_t) (bestTau + 1)];
        float denom = 2.0f * s1 - s0 - s2;
        if (std::abs (denom) > 1e-12f)
            betterTau = static_cast<float> (bestTau) + (s2 - s0) / (2.0f * denom);
    }

    float hz = static_cast<float> (sampleRate) / betterTau;

    // 범위 검증
    if (hz < 41.0f || hz > 330.0f)
    {
        noteDetected.store (false);
        return;
    }

    detectedHz.store (hz);
    noteDetected.store (true);

    // Hz -> Note + Cents 변환
    float refA = 440.0f;
    if (referenceAParam != nullptr)
        refA = referenceAParam->load();

    hzToNoteAndCents (hz, refA);
}

void Tuner::hzToNoteAndCents (float hz, float referenceA)
{
    // MIDI 노트 = 69 + 12 * log2(hz / referenceA)
    // A4 = MIDI 69
    float semitonesFromA4 = 12.0f * std::log2 (hz / referenceA);
    float midiNote = 69.0f + semitonesFromA4;

    int nearestMidi = static_cast<int> (std::round (midiNote));
    float cents = (midiNote - static_cast<float> (nearestMidi)) * 100.0f;

    // MIDI note % 12 -> note index (0=C, 1=C#, ..., 9=A, 10=A#, 11=B)
    int note = nearestMidi % 12;
    if (note < 0) note += 12;

    noteIndex.store (note);
    centsDeviation.store (cents);
}
