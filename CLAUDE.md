# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 프로젝트 개요

JUCE(C++17) 기반 베이스 기타 앰프 시뮬레이터. Standalone + VST3 (Windows/macOS) + AU (macOS) 동시 빌드.
전체 기능 명세는 `PRD.md` 참조.

**빌드 시스템**: CMake 3.22+ + JUCE 8 (git submodule)
**컴파일러**: MSVC 2022 (Windows) / Clang (macOS)
**C++ 표준**: C++17

---

## 개발 커맨드

```bash
# 최초 셋업 (JUCE 서브모듈 포함)
git submodule update --init --recursive

# Configure (Debug)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 전체 빌드
cmake --build build --config Release

# 타겟별 빌드
cmake --build build --target BassMusicGear_Standalone --config Release
cmake --build build --target BassMusicGear_VST3      --config Release
cmake --build build --target BassMusicGear_AU        --config Release  # macOS only

# 테스트 실행 (Catch2)
cmake --build build --target BassMusicGear_Tests --config Release
ctest --test-dir build --config Release --output-on-failure

# 특정 테스트 파일만
ctest --test-dir build -R ToneStackTest --output-on-failure
```

### 빌드 결과물 위치
- **Standalone**: `build/BassMusicGear_artefacts/Release/Standalone/`
- **VST3**: `build/BassMusicGear_artefacts/Release/VST3/BassMusicGear.vst3`
- **AU** (macOS): `build/BassMusicGear_artefacts/Release/AU/BassMusicGear.component`

---

## 아키텍처 개요

### 디렉터리 구조

```
BassMusicGear/
  CMakeLists.txt              # 빌드 정의 — JUCE 타겟, 포맷, 소스 목록
  JUCE/                       # JUCE git submodule
  Source/
    PluginProcessor.h/.cpp    # AudioProcessor — processBlock() 진입점, APVTS 정의
    PluginEditor.h/.cpp       # AudioProcessorEditor — UI 루트, 메시지 스레드
    DSP/
      SignalChain.h/.cpp      # 전체 신호 체인 조립 및 순서 관리. IR Position(Pre/Post) 파라미터에 따라 Cabinet ↔ DIBlend 연결 순서를 동적으로 변경
      ToneStack.h/.cpp        # 앰프별 톤스택 (TMB/James/Baxandall) IIR 계수 계산
      Preamp.h/.cpp           # 프리앰프 게인 스테이징 + 웨이브쉐이핑 (Oversampling 포함)
      Cabinet.h/.cpp          # juce::dsp::Convolution 래퍼, IR 로드/교체
      PowerAmp.h/.cpp         # 파워앰프 새추레이션 / Sag 시뮬레이션 (Post-FX 뒤, Cabinet 앞)
      Tuner.h/.cpp            # YIN 피치 트래킹, 크로매틱 표시용 데이터 생성 (DSP only, UI는 별도)
      BiAmpCrossover.h/.cpp   # Linkwitz-Riley 4차(LR4) 크로스오버. LP→클린DI, HP→앰프체인. OFF시 양쪽 전대역 통과
      DIBlend.h/.cpp          # 클린DI + 프로세스드 혼합. Blend(0–100%), Clean/Processed 개별 레벨 트림(±12dB), IR Position(Pre/Post)
      GraphicEQ.h/.cpp        # 10밴드 고정주파수 그래픽 EQ (31/63/125/250/500/1k/2k/4k/8k/16kHz), Constant-Q 피킹 바이쿼드
      Effects/
        Compressor.h/.cpp     # VCA/광학 컴프레서 (juce::dsp::Compressor 확장)
        Overdrive.h/.cpp      # Tube/JFET/Fuzz 웨이브쉐이퍼 + 패러렐 드라이 블렌드
        Octaver.h/.cpp        # YIN 피치 트래킹 기반 옥타버
        EnvelopeFilter.h/.cpp # SVF + 엔벨로프 팔로워
        Chorus.h/.cpp         # LFO 딜레이 모듈레이션
        Delay.h/.cpp          # 딜레이 / 에코
        Reverb.h/.cpp         # 알고리즘 리버브
        NoiseGate.h/.cpp      # 히스테리시스 게이트
    Models/
      AmpModel.h              # 앰프 모델 구성 데이터 (톤스택 타입, 기본 IR, UI 색상)
      AmpModelLibrary.h/.cpp  # 전체 앰프 모델 등록/조회
    UI/
      Knob.h/.cpp             # 커스텀 RotarySlider 래퍼 (드래그 + 우클릭 리셋)
      TunerDisplay.h/.cpp     # 튜너 UI — 음이름, 센트 편차 바, 뮤트 버튼 (에디터 상단 상시 표시)
      GraphicEQPanel.h/.cpp   # 10밴드 슬라이더 UI + 전체 플랫 리셋 버튼
      VUMeter.h/.cpp          # 실시간 레벨 미터 (juce::Timer 기반 갱신)
      AmpPanel.h/.cpp         # 앰프 모델별 톤스택 노브 레이아웃
      EffectBlock.h/.cpp      # 이펙터 블록 (ON/OFF + 파라미터 노브)
      SignalChainView.h/.cpp  # 신호 체인 블록 시각화
      PresetPanel.h/.cpp      # 프리셋 브라우저, A/B 슬롯
      CabinetSelector.h/.cpp  # 내장 IR 선택 + 커스텀 IR 파일 로드
      SettingsPage.h/.cpp     # 오디오 설정 페이지 (Standalone 전용)
    Presets/
      PresetManager.h/.cpp    # ValueTree 직렬화, 파일 저장/불러오기
    Resources/                # BinaryData로 컴파일되는 리소스
      IR/                     # 내장 캐비닛 IR WAV 파일
      Presets/                # 팩토리 프리셋 XML
  Tests/
    CMakeLists.txt
    ToneStackTest.cpp         # 톤스택 주파수 응답 단위 테스트
    OverdriveTest.cpp         # 웨이브쉐이퍼 앨리어싱 테스트
    PresetTest.cpp            # ValueTree 직렬화 테스트
```

### 핵심 JUCE 패턴

**`PluginProcessor` — 오디오 스레드의 진입점**
- `prepareToPlay(sampleRate, samplesPerBlock)`: DSP 모듈 초기화, 버퍼 할당. 오디오 스레드 시작 전 메인 스레드에서 호출됨.
- `processBlock(AudioBuffer<float>&, MidiBuffer&)`: 매 버퍼마다 오디오 스레드에서 호출. 여기서 `new`/`delete`, 파일 I/O, mutex, 시스템 콜 절대 금지.
- `releaseResources()`: 재생 중지 시 호출. 버퍼 해제.

**`AudioProcessorValueTreeState` (APVTS) — 파라미터 관리**
- 모든 노브/슬라이더 파라미터는 APVTS에 등록. `PluginProcessor` 생성자에서 `ParameterLayout` 정의.
- UI 컴포넌트는 `SliderAttachment` / `ButtonAttachment`로 APVTS 파라미터에 바인딩.
- 오디오 스레드에서 파라미터 값 읽기: `apvts.getRawParameterValue("gainId")->load()` (atomic, 락프리).
- 직접 `setValue()` 호출로 파라미터 변경하지 말 것 — 항상 Attachment 또는 `setValueNotifyingHost()` 사용.

**프리셋 — `ValueTree` 직렬화**
- APVTS 상태 전체를 `getStateInformation()` / `setStateInformation()`으로 저장/복원.
- 팩토리 프리셋은 `Resources/Presets/`에 XML로 포함, `BinaryData`로 컴파일됨.
- 유저 프리셋 저장 경로: `juce::File::getSpecialLocation(userApplicationDataDirectory)`.

**`AudioDeviceManager` — Standalone 전용 오디오 장치 관리**

`PluginProcessor`는 장치를 알지 못한다. Standalone 빌드에서는 JUCE의 `StandalonePluginHolder`가 `AudioDeviceManager`를 소유하며, `SettingsPage`는 이 인스턴스에 접근해 UI를 그린다.

```cpp
// SettingsPage에서 DeviceManager 접근
auto* holder = juce::StandalonePluginHolder::getInstance();
auto& deviceManager = holder->deviceManager;

// 사용 가능한 드라이버 타입 열거
for (auto* type : deviceManager.getAvailableDeviceTypes())
    driverCombo.addItem(type->getTypeName(), ...);

// 드라이버 선택 후 해당 타입의 장치 목록 조회
auto* currentType = deviceManager.getCurrentDeviceTypeObject();
auto inputDeviceNames  = currentType->getDeviceNames(true);   // true = input
auto outputDeviceNames = currentType->getDeviceNames(false);  // false = output

// 장치 오픈 (드라이버/장치/샘플레이트/버퍼/채널 한번에 설정)
juce::AudioDeviceManager::AudioDeviceSetup setup;
deviceManager.getAudioDeviceSetup(setup);
setup.inputDeviceName   = selectedInputDevice;
setup.outputDeviceName  = selectedOutputDevice;
setup.sampleRate        = selectedSampleRate;
setup.bufferSize        = selectedBufferSize;
setup.inputChannels.setRange(selectedInputChannel, 1, true);   // 단일 모노 채널
setup.outputChannels.setRange(0, 2, true);                     // 스테레오 출력
deviceManager.setAudioDeviceSetup(setup, true);

// 설정 저장 (앱 종료 시)
auto xml = deviceManager.createStateXml();
// → juce::File::getSpecialLocation(userApplicationDataDirectory) 에 저장

// 앱 시작 시 복원
deviceManager.initialise(1, 2, savedXml.get(), true);
```

**채널 선택 — 입력 모노 / 출력 스테레오**
- 베이스 입력은 항상 모노 1채널. `setup.inputChannels`에서 선택한 채널 비트만 1로 설정.
- `processBlock()`에서 `buffer.getReadPointer(0)`이 선택된 채널의 데이터를 받는다.
- ASIO 드라이버 선택 시: `AudioDeviceManager::getCurrentAudioDevice()`를 캐스팅하여 `showControlPanel()` 호출로 제조사 ASIO 패널 실행 가능.

**플러그인 모드에서는 `AudioDeviceManager` 미사용**
- VST3/AU 플러그인 빌드에서 `StandalonePluginHolder`는 존재하지 않는다.
- `PluginEditor`에서 설정 버튼은 `JUCEApplication::isStandaloneApp()`이 `false`일 때 비표시하거나 비활성화.

---

**IR 로드 — 백그라운드 스레드**
- `juce::dsp::Convolution`은 내부적으로 백그라운드 스레드에서 IR을 로드하고 처리 준비가 되면 오디오 스레드에 atomic swap.
- `Cabinet::loadIR(File)` 호출은 메인 스레드에서. `processBlock()`에서 IR 파일 경로를 직접 로드하지 말 것.

### 오버샘플링 규칙

`Preamp`, `Overdrive` 등 비선형(웨이브쉐이핑) 스테이지는 `juce::dsp::Oversampling<float>` 사용:
```cpp
// prepareToPlay에서 초기화
oversampling.initProcessing(samplesPerBlock);

// processBlock에서
auto oversampledBlock = oversampling.processSamplesUp(inputBlock);
// ... 웨이브쉐이핑 처리 ...
oversampling.processSamplesDown(outputBlock);
```
최소 4x 오버샘플링. Fuzz처럼 고조파 왜곡이 심한 경우 8x.

### 톤스택 구현 규칙

- **Fender TMB** (Tweed Bass): 세 컨트롤의 RC 네트워크 상호작용을 전달 함수로 이산화. 컨트롤 값이 바뀔 때 IIR 계수 재계산 (`ToneStack::updateCoefficients()`). 독립 필터 3개로 대체 금지.
- **James** (British Stack): Bass/Treble 셸빙이 독립적 → 바이쿼드 셸빙 2개 + 미드 피킹 1개.
- **Active Baxandall** (Modern Micro): 4-band 피킹/셸빙 바이쿼드.
- **Markbass 4-band + VPF/VLE** (Italian Clean): 4개 독립 바이쿼드(40/360/800/10kHz) + VPF 합성 필터 + VLE 로우패스.
  - **VPF**: 3개 필터의 합산으로 구현 — ① 35Hz 셸빙 부스트 ② 380Hz 노치(피킹 컷) ③ 10kHz 셸빙 부스트. 세 필터 모두 VPF 노브 값에 선형 비례하여 깊이 조절.
  - **VLE**: `juce::dsp::StateVariableTPTFilter` 로우패스 타입, 컷오프 주파수를 노브 값에 따라 20kHz(0) → 4kHz(max)로 매핑. 6dB/oct 기울기.
  - VPF/VLE는 서로 독립적으로 동작하며 4-band EQ 뒤에 직렬 배치.
- 모든 계수 재계산은 메인 스레드에서 수행 후 `std::atomic` 또는 FIFO로 오디오 스레드에 전달.

### 오버드라이브 블렌드 규칙

모든 오버드라이브/디스토션 DSP에는 `dryBlend` 파라미터가 있어야 한다.
```cpp
// 올바른 구현
output = dryBlend * input + (1.0f - dryBlend) * clipped;
```

---

## CMakeLists.txt 핵심 구조

```cmake
cmake_minimum_required(VERSION 3.22)
project(BassMusicGear VERSION 0.1.0)

add_subdirectory(JUCE)

juce_add_plugin(BassMusicGear
    COMPANY_NAME         "YourStudio"
    PLUGIN_MANUFACTURER_CODE  Bmgr
    PLUGIN_CODE          Bssg
    FORMATS              Standalone VST3 AU
    PRODUCT_NAME         "BassMusicGear"
    IS_SYNTH             FALSE
    NEEDS_MIDI_INPUT     FALSE
    NEEDS_MIDI_OUTPUT    FALSE
    IS_MIDI_EFFECT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES      "Fx|Distortion"
    AU_MAIN_TYPE         "kAudioUnitType_Effect"
)

juce_add_binary_data(BassMusicGear_BinaryData
    SOURCES Resources/IR/... Resources/Presets/...
)

target_link_libraries(BassMusicGear
    PRIVATE
        BassMusicGear_BinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
```

---

## DSP 참고 자료

- Fender TMB 톤스택 이산화: Yeh et al. (2006) CCRMA 논문 `dafx06_yeh.pdf`
- Wave Digital Filters (WDF): RT-WDF 라이브러리 (C++ header-only)
- 피치 트래킹 (옥타버용): YIN 알고리즘
- JUCE DSP 모듈 레퍼런스: `juce::dsp::Convolution`, `juce::dsp::Oversampling`, `juce::dsp::StateVariableTPTFilter`
