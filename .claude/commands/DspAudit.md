# /DspAudit

DSP 구현 파일의 RT 안전성과 오디오 스레드 패턴을 점검한다.

## 입력

$ARGUMENTS

(사용 예: /DspAudit Source/DSP/Preamp.cpp
          /DspAudit Source/DSP/SignalChain.cpp Source/DSP/Effects/Overdrive.cpp)

## 역할

입력된 파일 경로를 읽고 다음 항목을 검사한다:

1. **processBlock() RT 안전성** — 다음 패턴이 processBlock() 또는 그 호출 체인에 있으면 오류:
   - `new` / `delete` / `malloc` / `free`
   - `std::mutex`, `std::lock_guard`, `std::unique_lock`
   - 파일 I/O: `juce::File`, `std::fstream`, `fopen`
   - 블로킹 콜: `sleep`, `std::this_thread::sleep_for`
   - 컨테이너 메모리 재할당: `push_back`, `insert`, `resize` 등

2. **오버샘플링 누락** — 웨이브쉐이핑(tanh, hard-clip, waveshaper)을 수행하는 클래스에
   `juce::dsp::Oversampling`이 없으면 경고. Preamp/Overdrive는 최소 4x, Fuzz는 8x.

3. **파라미터 읽기 방식** — `processBlock()`에서 `getParameter()->getValue()` 직접 호출은 오류.
   올바른 방식: `apvts.getRawParameterValue("id")->load()`

4. **필터 계수 재계산 위치** — `processBlock()` 내에서 `updateCoefficients()` 직접 호출은 오류.
   계수 계산은 메인 스레드에서 수행하고 atomic/FIFO로 전달해야 한다.

5. **setLatencySamples 누락** — `juce::dsp::Convolution` 또는 `juce::dsp::Oversampling`을 사용하는
   클래스가 있는데 `setLatencySamples()` 호출이 없으면 경고.

6. **Dry Blend 누락** — Overdrive/Distortion/Fuzz 클래스에 dryBlend 파라미터가 없으면 경고.
   올바른 혼합: `output = dryBlend * input + (1.0f - dryBlend) * clipped`

## 출력 형식

각 문제를 다음 형식으로 출력한다:
[ERROR|WARNING] 파일명:줄번호 — 문제 설명
  현재: <문제 코드>
  수정: <올바른 코드>

마지막에 요약: 오류 N건, 경고 M건. 오류가 0이면 "✅ RT-SAFE" 출력.
