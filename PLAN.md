# PLAN.md — BassMusicGear 단계별 구현 계획 및 실행 프롬프트

각 Phase 섹션에 계획(P0/P1 항목, 테스트 기준)과 실행 프롬프트가 함께 담겨 있다.
새 Claude Code 세션을 시작할 때 해당 Phase의 **실행 프롬프트** 블록을 붙여넣어 실행한다.

참조 문서: `PRD.md`, `CLAUDE.md`, `TOOLING.md`

---

## 태그 정의

| 태그 | 의미 |
|---|---|
| **P0** | 해당 Phase 내 반드시 완료. 미완료 시 Phase 종료 불가 |
| **P1** | 해당 Phase 목표이나 다음 Phase로 이월 허용. 이월 시 다음 Phase 첫 우선순위로 처리 |
| **🔧 TOOL** | Phase 시작 전 구현할 Tooling (slash command / agent / script). 코드 작업 전에 완료 |
| **✅ CARRY** | 이전 Phase에서 이월된 P1 항목. 이 Phase에서 P0로 처리 |

---

## Phase 개요

| Phase | 이름 | 핵심 산출물 | 실행 가능 마일스톤 |
|---|---|---|---|
| **0** | 프로젝트 스켈레톤 | CMakeLists, 빈 Plugin, Catch2 셋업 | Standalone 창이 뜬다 |
| **1** | 핵심 신호 체인 | Gate + Preamp(Tweed) + Cabinet + PowerAmp | 베이스 소리가 처리되어 나온다 |
| **2** | 전체 앰프 모델 | 5종 ToneStack + AmpModelLibrary + AmpPanel | 5종 모델 전환 가능 |
| **3** | 튜너 + 컴프레서 | YIN Tuner + Compressor + TunerDisplay | 튜너 동작, 컴프레서 게인 리덕션 확인 |
| **4** | Pre-FX | Overdrive + Octaver + EnvelopeFilter + EffectBlock UI | Pre-FX 3종 on/off 동작 |
| **5** | 그래픽 EQ + Post-FX | 10-band EQ + Chorus + Delay + Reverb | EQ 슬라이더 + 공간계 이펙터 동작 |
| **6** | Bi-Amp + DI Blend | LR4 크로스오버 + DIBlend + IR Position | 신호 분기/혼합 경로 전환 가능 |
| **7** | 이월 작업 정리 | PowerAmp 포화 차별화 + Delay BPM Sync + NoiseGate/Compressor UI | 4건 이월 항목 해소, DSP·UI 누락분 정리 |
| **8** | 프리셋 시스템 | PresetManager + 팩토리 프리셋 15종 + PresetPanel | 프리셋 저장/불러오기/A-B 비교 |
| **9** | UI 완성 + 출력 | VUMeter + SignalChainView + Knob + Master Volume | 전체 UI 완성 |
| **10** | 오디오 설정 + 릴리즈 | SettingsPage + 전체 테스트 + 설치 | VST3 DAW 로드 + Standalone 장치 설정 |

---

## Phase 0 — 프로젝트 스켈레톤

**목표**: CMake + JUCE + Catch2 빌드 환경 구축. 창이 뜨는 빈 플러그인.
**마일스톤**: Standalone 실행 시 창 표시, 더미 테스트 통과.

### 🔧 TOOL
- `scripts/CleanBuild.sh` — `build/` 삭제 후 `cmake -B build -DCMAKE_BUILD_TYPE=Debug` 실행

### P0 구현 항목
- `CMakeLists.txt` — `juce_add_plugin` (Standalone/VST3/AU), Catch2 FetchContent, Tests 서브디렉터리
- `Tests/CMakeLists.txt` + `Tests/SmokeTest.cpp` — 항상 pass하는 더미 테스트
- `Source/PluginProcessor.h/.cpp` — 빈 AudioProcessor, APVTS 빈 레이아웃
- `Source/PluginEditor.h/.cpp` — 800×500 창, "BassMusicGear - Work in Progress" 텍스트

### 테스트 기준
- `cmake --build build --target BassMusicGear_Standalone` 성공
- `ctest --test-dir build --output-on-failure` 통과 (더미 테스트)

### 스모크 테스트
- Standalone 실행 → 창 표시, 크래시 없음

### 실행 프롬프트
```
PLAN.md의 Phase 0를 구현해줘. PRD.md와 CLAUDE.md의 CMakeLists 핵심 구조를 참고한다.

scripts/CleanBuild.sh를 먼저 작성한 뒤 다음 파일들을 구현한다:
- CMakeLists.txt (JUCE 서브모듈, Standalone/VST3/AU 타겟, Catch2 FetchContent)
- Tests/CMakeLists.txt + Tests/SmokeTest.cpp (항상 pass 더미 테스트)
- Source/PluginProcessor.h/.cpp (빈 AudioProcessor, 빈 APVTS 레이아웃)
- Source/PluginEditor.h/.cpp (800×500, 타이틀 텍스트)

완료 기준: cmake 빌드 성공 + ctest 통과 + Standalone 창 표시.
완료 후 git add/commit/push. 메시지: "feat(phase0): project skeleton — JUCE + CMake + Catch2"
```

---

## Phase 1 — 핵심 신호 체인

**목표**: 베이스 신호가 Gate → Preamp → ToneStack → PowerAmp → Cabinet → 출력까지 흐른다.
**마일스톤**: Standalone에서 베이스 소리가 처리되어 나온다.

### 🔧 TOOL
- `.claude/commands/AddParam.md` — `/AddParam <id> <type> [min max default unit]` 호출 시 APVTS 파라미터 추가 보일러플레이트(ParameterLayout 코드 + SliderAttachment 코드 + atomic 읽기 스니펫) 출력
- `.claude/commands/DspAudit.md` — `/DspAudit <파일경로>` 호출 시 해당 파일의 RT 안전성(processBlock 내 금지 패턴, 오버샘플링 누락, 계수 재계산 위치, setLatencySamples 누락) 점검 후 통과/위반 항목 출력
- `.claude/agents/DspReviewer.md` — DSP 파일 RT 안전성·오버샘플링 대칭성·Dry Blend 누락 검사 에이전트

### P0 구현 항목
- `Source/DSP/Effects/NoiseGate.h/.cpp` — Threshold/Attack/Hold/Release, 히스테리시스
- `Source/DSP/ToneStack.h/.cpp` — Tweed Bass(Fender TMB)만 구현. `updateCoefficients()` 포함. 계수 재계산은 메인 스레드에서만
- `Source/DSP/Preamp.h/.cpp` — InputGain, 비대칭 tanh 소프트클리핑, `juce::dsp::Oversampling` 4x
- `Source/DSP/PowerAmp.h/.cpp` — Drive, Presence. Sag는 P1
- `Source/DSP/Cabinet.h/.cpp` — `juce::dsp::Convolution` 래퍼, placeholder IR 1개 (44.1kHz 모노 임펄스)
- `Source/DSP/SignalChain.h/.cpp` — Gate→Preamp→ToneStack→PowerAmp→Cabinet 순서
- `Source/PluginProcessor.h/.cpp` — `processBlock()` → SignalChain, `prepareToPlay()`에서 `setLatencySamples(oversamplingLatency + convolutionLatency)` 호출, APVTS 파라미터 등록
- `Source/UI/Knob.h/.cpp` — RotarySlider 래퍼, 우클릭 리셋
- `Source/PluginEditor.h/.cpp` — InputGain/Volume/Bass/Mid/Treble/Drive/Presence 노브 + Cabinet 바이패스 토글

### P1 항목
- PowerAmp Sag → Phase 2로 이월
- 커스텀 IR 파일 로드 → Phase 6으로 이월

### 테스트 기준
- `Tests/ToneStackTest.cpp` — TMB Bass=0.5/Mid=0.5/Treble=0.5 시 100Hz~5kHz ±3dB 이내; Bass=1.0 시 100Hz 게인 부스트 확인
- `Tests/OverdriveTest.cpp` — 4x 오버샘플 후 10kHz 입력 앨리어싱 -60dBFS 이하
- `/DspAudit`으로 SignalChain.cpp, Preamp.cpp 점검 후 이슈 없음

### 스모크 테스트
- Standalone → 오디오 인터페이스 입력 → 처리된 소리 출력
- 노브 조작 시 음색 변화 확인

### 실행 프롬프트
```
PLAN.md의 Phase 1을 구현해줘. PRD.md 섹션 2, 3, 9와 CLAUDE.md 핵심 JUCE 패턴 및 오버샘플링 규칙을 참고한다.

먼저 TOOL 3개를 구현한다:
1. .claude/commands/AddParam.md
2. .claude/commands/DspAudit.md
3. .claude/agents/DspReviewer.md
(각 파일의 역할은 이 문서 Phase 1 🔧 TOOL 섹션 참고)

그 다음 DSP 레이어(NoiseGate, ToneStack(TMB), Preamp(4x oversample), PowerAmp, Cabinet, SignalChain)와
UI 레이어(Knob, PluginEditor 기본 노브 배치)를 구현한다.
PowerAmp Sag는 P1로 건너뛴다.

setLatencySamples()는 prepareToPlay()에서 반드시 호출할 것.
구현 완료 후 /DspAudit으로 SignalChain.cpp, Preamp.cpp를 점검한다.

완료 기준: ToneStackTest + OverdriveTest 통과, Standalone에서 실제 소리 출력.
완료 후 git add/commit/push. 메시지: "feat(phase1): core signal chain — Gate/Preamp/ToneStack/PowerAmp/Cabinet"
```

---

## Phase 2 — 전체 앰프 모델 (5종)

**목표**: 5종 앰프 모델 전체 구현, 모델 전환 UI 동작.
**마일스톤**: 5종 모델 전환 가능, 각 모델이 청각적으로 다른 톤 캐릭터를 가짐.

### ✅ CARRY
- PowerAmp Sag 시뮬레이션 (Phase 1 P1) — 튜브 모델(American Vintage/Tweed/British)만 활성화

### 🔧 TOOL
- `.claude/commands/ToneStack.md` — `/ToneStack <topology> <bass> <mid> <treble>` 호출 시 해당 토폴로지 전달함수 계산 과정 + `updateCoefficients()` 스텁 출력
- `.claude/commands/NewAmpModel.md` — `/NewAmpModel <name> <type>` 호출 시 AmpModel 항목 + ToneStack 분기 + AmpModelLibrary 등록 코드 + 프리셋 XML 스텁 출력

### P0 구현 항목
- `ToneStack` 나머지 4종 추가 (CLAUDE.md 톤스택 구현 규칙 엄수)
  - American Vintage: Active Baxandall + Mid 5포지션 스위치 (250/500/800/1500/3000Hz)
  - British Stack: James 토폴로지 (Bass/Treble 독립 셸빙 + Mid 피킹)
  - Modern Micro: Baxandall + Grunt(저역 드라이브 HPF+LPF) + Attack(고역 HPF)
  - Italian Clean: 4-band 독립 바이쿼드(40/360/800/10kHz) + VPF(35Hz 셸빙+380Hz 노치+10kHz 셸빙) + VLE(StateVariableTPTFilter LP, 20kHz→4kHz)
- `PowerAmp` — Sag 파라미터 추가 (✅ CARRY). 솔리드스테이트/Class D 모델에서 비활성화
- `Preamp` — 모델별 게인 스테이징 분기 (12AX7 cascade / JFET parallel / Class D linear)
- `Source/Models/AmpModel.h` + `AmpModelLibrary.h/.cpp` — 5종 등록
- `Resources/IR/` — 5종 placeholder IR WAV 추가 (4x10 JBL, 1x15 Vintage, 2x12 British, 2x10 Modern)
- `Source/UI/AmpPanel.h/.cpp` — 모델 선택 ComboBox + 모델별 노브 레이아웃 전환 (VPF/VLE 표시 분기 포함)
- `Source/UI/CabinetSelector.h/.cpp` — 내장 IR 선택 ComboBox

### P1 항목
- 앰프 모델별 UI 색상 테마 → Phase 8로 이월

### 테스트 기준
- `ToneStackTest` 확장 — 5종 토폴로지 검증
  - Italian Clean VPF max: 380Hz 노치 -6dB 이상 확인
  - Italian Clean VLE max: 8kHz -12dB 이상 롤오프 확인
  - British Stack: Bass 변경 시 8kHz 응답 불변 (독립성 검증)
- `OverdriveTest` 확장 — JFET 병렬 구조 DryBlend 선형성

### 스모크 테스트
- 5종 모델 전환 시 노브 레이아웃 변경, 청각적 차이 확인

### 실행 프롬프트
```
PLAN.md의 Phase 2를 구현해줘. PRD.md 섹션 1, 2와 CLAUDE.md 톤스택 구현 규칙을 참고한다.

먼저 TOOL 2개를 구현한다:
1. .claude/commands/ToneStack.md
2. .claude/commands/NewAmpModel.md

그 다음:
- ToneStack 나머지 4종 (American Vintage / British Stack / Modern Micro / Italian Clean)
- PowerAmp Sag (✅ CARRY — 튜브 모델만 활성화)
- Preamp 모델별 게인 스테이징 분기
- AmpModel.h + AmpModelLibrary (5종 등록)
- IR placeholder WAV 4종 추가 + CMakeLists BinaryData 갱신
- AmpPanel UI (모델 선택 + 모델별 노브 레이아웃), CabinetSelector

모델별 UI 색상 테마는 P1로 건너뛴다.

완료 기준: ToneStackTest 5종 통과, Standalone에서 5종 모델 전환 및 청각적 차이 확인.
완료 후 git add/commit/push. 메시지: "feat(phase2): all 5 amp models — ToneStack/Preamp/PowerAmp variants"
```

---

## Phase 3 — 튜너 + 컴프레서

**목표**: 신호 체인 앞단(Gate → Tuner → Compressor)이 완성된다.
**마일스톤**: 튜너 동작 확인, 컴프레서 게인 리덕션 청취 가능.

### P0 구현 항목
- `Source/DSP/Tuner.h/.cpp` — YIN 알고리즘 (41Hz~330Hz, 버퍼 2048 samples). 음이름+센트 편차를 `std::atomic`으로 UI 스레드에 전달. Mute 모드 플래그
- `Source/DSP/Effects/Compressor.h/.cpp` — Threshold/Ratio/Attack/Release/MakeupGain/DryBlend. `juce::dsp::Compressor` 확장. 게인 리덕션 값 atomic 저장
- `SignalChain` 수정 — Gate→Tuner→Compressor→[BiAmp 자리 확보]→Preamp 순서
- `Source/UI/TunerDisplay.h/.cpp` — 음이름/센트 편차 바/Mute 버튼. `juce::Timer` 30Hz 갱신. 에디터 상단 상시 표시
- APVTS 파라미터 추가 — `tuner_reference_a`, `tuner_mute`, `comp_enabled`, `comp_threshold`, `comp_ratio`, `comp_attack`, `comp_release`, `comp_makeup`, `comp_dry_blend`

### 테스트 기준
- `Tests/CompressorTest.cpp`
  - Attack 10ms, Ratio 8:1, Threshold -20dBFS: 10ms 시점 게인 리덕션 -6dB ±1.5dB
  - DryBlend=1.0: 입출력 동일 (bypass 동작)
  - Ratio ∞:1: Threshold 초과 시 하드 리밋 동작

### 스모크 테스트
- E1(41Hz) 연주 → 튜너에 "E" 표시 및 센트 편차 움직임
- Mute ON → 무음, OFF → 소리 복귀
- Compressor Ratio 8:1, Threshold -20dBFS → 피크 레벨 감소 확인

### 실행 프롬프트
```
PLAN.md의 Phase 3을 구현해줘. PRD.md 섹션 5, 7을 참고한다.

구현 목록:
- Source/DSP/Tuner.h/.cpp (YIN 알고리즘, 41–330Hz, atomic으로 UI 전달, Mute 플래그)
- Source/DSP/Effects/Compressor.h/.cpp (Threshold/Ratio/Attack/Release/MakeupGain/DryBlend)
- SignalChain 수정 (Gate→Tuner→Compressor→BiAmp placeholder→Preamp)
- Source/UI/TunerDisplay.h/.cpp (음이름, 센트 바, Mute 버튼, 상단 상시 표시)
- APVTS 파라미터 추가 (/AddParam 활용)

완료 기준: CompressorTest 통과, E1 연주 시 튜너 "E" 표시 확인.
완료 후 git add/commit/push. 메시지: "feat(phase3): tuner (YIN) + compressor with dry blend"
```

---

## Phase 4 — Pre-FX

**목표**: Pre-FX 3종(오버드라이브, 옥타버, 엔벨로프 필터) 체인 삽입.
**마일스톤**: 각 Pre-FX ON/OFF 동작, 오버드라이브 3종 청각적 차이 확인.

### 🔧 TOOL
- `.claude/agents/BuildDoctor.md` — CMake 빌드 실패 시 오류를 JUCE 모듈 누락/링크 오류/플랫폼 헤더 충돌/AU 빌드 오류로 분류하고 수정안 제시하는 에이전트

### P0 구현 항목
- `Source/DSP/Effects/Overdrive.h/.cpp` — Tube(비대칭 tanh)/JFET(parallel)/Fuzz(hard clip). 4x 오버샘플링(Fuzz는 8x). `output = dryBlend * input + (1-dryBlend) * clipped`
- `Source/DSP/Effects/Octaver.h/.cpp` — YIN 기반 서브옥타브/옥타브업 사인파 합성. Sub/Oct-Up/Dry Level
- `Source/DSP/Effects/EnvelopeFilter.h/.cpp` — StateVariableTPTFilter + 엔벨로프 팔로워. Sensitivity/FreqRange/Resonance/Direction
- `Source/UI/EffectBlock.h/.cpp` — ON/OFF 토글 + 파라미터 노브 범용 컴포넌트 (Pre-FX/Post-FX 공용)
- `SignalChain` 수정 — [Compressor]→[BiAmp placeholder]→Overdrive→Octaver→EnvelopeFilter→Preamp

### P1 항목
- Octaver Oct-Up 음질 개선 → Phase 5로 이월 가능

### 테스트 기준
- `OverdriveTest` 확장
  - Tube 4x: 10kHz 클리핑 후 앨리어싱 -60dBFS 이하
  - DryBlend=0.0: wet only, DryBlend=1.0: dry only (오차 -96dBFS 이하)
  - Fuzz 8x: 하드클리핑 확인 (THD > 50%)
- `/DspAudit`으로 Overdrive.cpp, Octaver.cpp, EnvelopeFilter.cpp 각각 점검

### 스모크 테스트
- Overdrive Tube 타입 ON → 베이스 왜곡 소리 확인
- DryBlend 0% → 완전 왜곡, 100% → 클린
- EnvelopeFilter ON, 강하게 연주 → 와우와우 스윕 확인
- **[Phase 3 이월]** 컴프레서 파라미터 노브 우클릭 → 기본값 리셋 동작 확인 (Phase 3에서 컴프레서 UI 없어 미확인)

### 실행 프롬프트
```
PLAN.md의 Phase 4를 구현해줘. PRD.md 섹션 4(Pre-FX)와 CLAUDE.md 오버드라이브 블렌드 규칙을 참고한다.

먼저 BuildDoctor agent(.claude/agents/BuildDoctor.md)를 구현한다.

그 다음:
- Overdrive (Tube/JFET/Fuzz, 4x/8x oversample, DryBlend 필수)
- Octaver (YIN 기반, Sub/Oct-Up/Dry Level)
- EnvelopeFilter (SVF + 엔벨로프 팔로워, Direction Up/Down)
- EffectBlock 공용 UI 컴포넌트
- SignalChain Pre-FX 블록 삽입

모든 DSP 파일 구현 후 /DspAudit으로 각각 점검한다.
Octaver Oct-Up 음질 개선은 P1로 건너뛴다.

완료 기준: OverdriveTest 통과, Standalone에서 3종 이펙터 ON/OFF 동작 확인.
완료 후 git add/commit/push. 메시지: "feat(phase4): pre-fx — overdrive/octaver/envelope-filter + EffectBlock UI"
```

---

## Phase 5 — 그래픽 EQ + Post-FX

**목표**: 10밴드 그래픽 EQ + Post-FX 3종(Chorus/Delay/Reverb) 완성.
**마일스톤**: EQ 슬라이더로 주파수 응답 변경, 공간계 이펙터 청취 가능.

### ✅ CARRY
- Octaver Oct-Up 음질 개선 (Phase 4 P1 — 해당 시 처리)
- EnvelopeFilter 누락 UI 파라미터 (Phase 4 이월) — EffectBlock에 ComboBox 지원 추가 후 아래 3개 파라미터 노출:
  - `ef_freq_min` / `ef_freq_max` 노브 (FreqRange 범위 조절)
  - `ef_direction` ComboBox (Up/Down 선택)

### 🔧 TOOL
- `scripts/GenBinaryData.sh` — `Resources/IR/`, `Resources/Presets/` 파일 목록 자동 스캔 → `CMakeLists.txt` BinaryData SOURCES 갱신

### P0 구현 항목
- `Source/DSP/GraphicEQ.h/.cpp` — 10밴드 Constant-Q 피킹 바이쿼드 (31/63/125/250/500/1k/2k/4k/8k/16kHz). ±12dB. ON/OFF 바이패스
- `Source/DSP/Effects/Chorus.h/.cpp` — LFO 딜레이 모듈레이션. Rate/Depth/Mix
- `Source/DSP/Effects/Delay.h/.cpp` — Time/Feedback/Damping/Mix. BPM Sync는 P1
- `Source/DSP/Effects/Reverb.h/.cpp` — `juce::dsp::Reverb` 활용. Type(Spring/Room)/Size/Decay/Mix
- `SignalChain` 수정 — ToneStack→GraphicEQ→Chorus→Delay→Reverb→PowerAmp
- `Source/UI/GraphicEQPanel.h/.cpp` — 10개 수직 슬라이더 + FLAT 리셋 버튼
- `EffectBlock` 확장 — ComboBox 지원 추가 (✅ CARRY: EnvelopeFilter ef_direction 노출)

### P1 항목
- Delay BPM Sync → Phase 8로 이월

### 테스트 기준
- `Tests/GraphicEQTest.cpp` (신규)
  - 각 밴드 +12dB 설정 시 해당 주파수 측정값 +11.5~+12.5dB
  - 전 밴드 0dB 시 20Hz~20kHz ±0.5dB 평탄
  - 바이패스 ON: 입력 = 출력 (수치 일치)

### 스모크 테스트
- 31Hz 슬라이더 +12dB → 서브 저역 강조 확인
- Chorus Mix 100% → 모듈레이션 효과, Delay 500ms → 에코 청취

### 실행 프롬프트
```
PLAN.md의 Phase 5를 구현해줘. PRD.md 섹션 4(Post-FX), 8을 참고한다.

먼저 scripts/GenBinaryData.sh를 구현한다.

그 다음:
- GraphicEQ (10밴드 Constant-Q 바이쿼드, ±12dB, 바이패스)
- Chorus (LFO 딜레이 모듈레이션)
- Delay (Time/Feedback/Damping/Mix, BPM Sync는 P1 건너뜀)
- Reverb (juce::dsp::Reverb 활용, Spring/Room)
- SignalChain Post-FX 블록 삽입
- GraphicEQPanel UI (10개 슬라이더 + FLAT 버튼)

완료 기준: GraphicEQTest 통과, Standalone에서 EQ 슬라이더 동작 + 공간계 이펙터 청취 확인.
완료 후 git add/commit/push. 메시지: "feat(phase5): graphic-EQ 10-band + post-fx chorus/delay/reverb"
```

---

## Phase 6 — Bi-Amp 크로스오버 + DI Blend + Amp Voicing

**목표**: 신호 분기(BiAmpCrossover)와 혼합(DIBlend) + IR Position 전환 + 앰프별 고정 Voicing 필터 적용.
**마일스톤**: Bi-Amp ON 시 저역 클린/고역 앰프 분리 청취, IR Position Pre↔Post 차이 확인, 6종 앰프의 음색이 Cabinet bypass + ToneStack flat 상태에서도 청취로 구분 가능.

### ✅ CARRY
- ~~커스텀 IR 파일 로드 `Cabinet::loadIR(File)` (Phase 1 P1)~~ → Phase 1에서 조기 완료
- Delay BPM Sync (Phase 5 P1 — 해당 시 처리)
- **Amp Voicing 필터 부재** (BackLog 🔴 이월) — 5종 앰프의 음색 차별화. `AmpVoicingPlan.md` 참고

### P0 구현 항목
- `Source/DSP/BiAmpCrossover.h/.cpp` — LR4 4차 크로스오버 (2차 Butterworth LP 직렬 2회). ON: LP→cleanDI / HP→amp chain. OFF: 전대역 양쪽 분기. 60~500Hz
- `Source/DSP/DIBlend.h/.cpp` — **Enabled 토글(`di_blend_on`)**, Blend(0~100%), Clean/Processed Level(±12dB), IR Position(Pre/Post)
  - OFF: 프로세스드 신호만 출력 (DI 혼합 우회), UI에서 다른 컨트롤 비활성 표시
  - ON Post-IR: `mixed` 그대로 출력 (Cabinet은 processed 경로 내 포함)
  - ON Pre-IR: `cabinetIR(mixed)` 출력
- `Source/DSP/AmpVoicing.h/.cpp` — 앰프별 고정 Voicing 필터 (2~3개 바이쿼드). `setModel(AmpModelType)` 호출 시 해당 앰프의 필터 계수 로드. `AmpVoicingPlan.md` 섹션 3 참고
- `Source/Models/AmpModel.h` 수정 — `struct VoicingBand { float freq, gainDb, q; FilterType type; }`, `std::array<VoicingBand, 3> voicingBands` 추가
- `Source/Models/AmpModelLibrary.cpp` 수정 — 5종 모델에 Voicing 값 등록 (`AmpVoicingPlan.md` 섹션 3-1~3-5) + **Origin Pure** 6번째 앰프 추가
  - Origin Pure 스펙: SolidState Preamp + Active Baxandall ToneStack + Class D PowerAmp + Flat Voicing(바이쿼드 계수 모두 0dB) + UI 색상 실버(#c0c0c8)
  - Italian Clean(Markbass)과 구분점: VPF/VLE 없음, Voicing 완전 플랫 → 가장 투명한 사운드
- `SignalChain` 대규모 수정 — ① IR Position에 따라 Cabinet ↔ DIBlend 연결 순서 동적 전환. cleanDI 버퍼 별도 할당 (`prepareToPlay`에서). ② `AmpVoicing`을 Preamp와 ToneStack 사이에 삽입. 앰프 모델 전환 시 `ampVoicing.setModel(model)` 호출
- `Cabinet` 수정 — `loadIR(const juce::File&)` 실제 구현 (✅ CARRY)
- `Source/UI/BiAmpPanel.h/.cpp` — ON/OFF 토글, Crossover Freq 노브
- `Source/UI/DIBlendPanel.h/.cpp` — **Enabled 토글(헤더)**, Blend 노브, Clean/Processed Level 노브, IR Position 토글. OFF 상태에서는 Blend 이하 컨트롤 비활성화

### 구현 후 변경 사항 (Phase 6 완료 시점 반영)
- **DI Blend ON/OFF 토글 추가**: 초기 계획에는 없던 `di_blend_on` APVTS 파라미터를 추가. 사용자가 DI 혼합 경로 자체를 우회할 수 있도록 헤더에 토글 노출 (DIBlendPanel 제목 라벨을 ToggleButton으로 대체).
- **AmpVoicing 게인 값 강화**: 초기 AmpVoicingPlan 설계값(±1.5~3dB)으로는 ToneStack Flat 상태에서 앰프 간 음색 차이가 청감상 미묘했다. Phase 6 튜닝 과정에서 전 앰프 Voicing 게인을 약 2배로 강화(최대 ±8dB)하여 가청 차별화를 확보. `AmpVoicingPlan.md` 섹션 3-1~3-5 및 `AmpModelLibrary.cpp`의 `voicingBands` 동기화 완료.

### 테스트 기준
- `Tests/BiAmpCrossoverTest.cpp`
  - LR4 200Hz: LP+HP 합산 20Hz~10kHz ±0.1dB 평탄
  - LR4 200Hz: -6dB 지점 195~205Hz 이내
  - OFF: cleanDI = ampChain 입력 (전대역 동일)
- `Tests/DIBlendTest.cpp`
  - Blend=0.0: 출력 = cleanDI × cleanLevel (오차 1e-6 이하)
  - Blend=1.0: 출력 = processed × processedLevel
  - IR Position 전환: 동일 입력에서 서로 다른 출력 확인
- `Tests/AmpVoicingTest.cpp` (신규)
  - Cabinet bypass + ToneStack flat 상태에서 6종 앰프의 주파수 응답이 서로 다름 확인
  - American Vintage: 80Hz +3dB(±1dB), 1.5kHz 감쇠 확인
  - British Stack: 500Hz 부스트, 60Hz 이하 HP 감쇠 확인
  - Modern Micro: 3kHz +4dB(±1dB) 확인
  - Origin Pure: 전 대역 ±0.5dB 이내 (Voicing 플랫 확인)

### 스모크 테스트
- Bi-Amp ON + Crossover 200Hz → 저음 클린/고음 앰프처리 확인
- IR Position Pre↔Post 전환 시 서로 다른 공간감 확인
- 커스텀 WAV IR 로드 → 즉시 반영
- Cabinet bypass + ToneStack flat 상태에서 6종 앰프 순차 전환 → 음색 차이 청취 확인 (`AmpVoicingPlan.md` 섹션 5-3 기준)
- Origin Pure 선택 → 노브 센터에서 가장 투명한 사운드 확인 (VPF/VLE 없음)

### 실행 프롬프트
```
PLAN.md의 Phase 6을 구현해줘. PRD.md 섹션 6, 10과 CLAUDE.md SignalChain 설명, AmpVoicingPlan.md를 참고한다.

✅ CARRY 항목 먼저 처리:
- Cabinet::loadIR(const juce::File&) 실제 구현 (지금까지 stub이었다면)
- Delay BPM Sync (Phase 5 이월 시)
- Amp Voicing (BackLog 이월) — AmpVoicingPlan.md의 설계 그대로 구현

그 다음:
- AmpVoicing.h/.cpp (앰프별 고정 바이쿼드 필터 체인, setModel()으로 계수 교체)
- AmpModel.h 수정 (VoicingBand 구조체 + voicingBands 배열)
- AmpModelLibrary.cpp 수정 (5종 Voicing 값 등록 + **Origin Pure 6번째 앰프 추가**: SolidState Preamp + Baxandall ToneStack + Class D PowerAmp + Flat Voicing + 실버 색상 #c0c0c8)
- SignalChain 수정 — AmpVoicing을 Preamp~ToneStack 사이 삽입 + 앰프 전환 시 setModel() 호출
- BiAmpCrossover (LR4 크로스오버, ON/OFF, 60~500Hz)
- DIBlend (Enabled 토글 `di_blend_on` / Blend / Clean Level / Processed Level / IR Position Pre/Post)
- SignalChain 대규모 수정 (IR Position에 따른 동적 라우팅. cleanDI 버퍼는 prepareToPlay에서 할당)
- BiAmpPanel UI, DIBlendPanel UI

주의: processBlock 내에서 버퍼 new 절대 금지. /DspAudit으로 AmpVoicing, BiAmpCrossover, DIBlend, SignalChain 점검.

완료 기준: BiAmpCrossoverTest + DIBlendTest + AmpVoicingTest 통과, Standalone에서 분기/혼합 경로 동작 + 6종 앰프 음색 차이 청취 확인 (Origin Pure는 가장 투명한 사운드로 구분).
완료 후 git add/commit/push. 메시지: "feat(phase6): bi-amp LR4 crossover + DI blend + amp voicing filters"
```

---

## Phase 7 — 이월 작업 정리 (DSP 차별화 + 누락 UI)

**목표**: Phase 1~6에서 이월된 DSP·UI 누락 항목 4건을 한꺼번에 처리하여, 프리셋 시스템(Phase 8) 진입 전에 모든 파라미터·블록을 안정화한다.
**마일스톤**: 6종 앰프의 포화 특성이 청각으로 구분되고, NoiseGate/Compressor가 UI에서 조작 가능하며, Delay BPM Sync가 동작한다.

### ✅ CARRY
- (없음 — Phase 6은 모든 P0/P1을 마무리하여 신규 CARRY 없음)

### P0 구현 항목

**1. PowerAmp 앰프별 포화 차별화** (Phase 2 P1 이월)
- `Source/DSP/PowerAmp.cpp` — `process()` 내부에 `currentType` 분기 추가
  - **Tube6550** (American Vintage / Tweed Bass): 부드러운 비대칭 포화. 짝수 고조파 강조. 높은 헤드룸. (`tanh` 기반 + 비대칭 오프셋)
  - **TubeEL34** (British Stack): 빠른 포화, 낮은 헤드룸, 미드 중심 따뜻함. (`tanh` 곡률 강화 + 미드 부스트)
  - **SolidState** (Modern Micro): 경하드 클리핑, 고조파 풍부. (`x / (1 + |x|^k)` 또는 폴리노미얼)
  - **ClassD** (Italian Clean / Origin Pure): 거의 선형, 매우 높은 헤드룸, 최소 왜곡. (선형 + 미세 saturation)
- 모든 곡선은 `juce::dsp::Oversampling` 4x 적용 (현 PowerAmp 구조 유지)
- Sag 시뮬레이션과 호환되도록 sagEnabled=true 조건에서만 트랜스포머 전압 새깅 적용
- DspReviewer로 RT 안전성 점검 (분기 내부에 alloc/lock 없음 확인)

**2. Delay BPM Sync** (Phase 5 P1 이월)
- `Source/DSP/Effects/Delay.h/.cpp`
  - APVTS 신규 파라미터: `delay_bpm_sync` (bool, 기본 OFF), `delay_note_value` (choice: 1/4, 1/8, 1/8 dotted, 1/16, 1/4 triplet 등)
  - BPM 소스: `juce::AudioPlayHead::getCurrentPosition()` 호출 → bpm 추출. Standalone 모드에서 PlayHead 없을 시 기본값 120 BPM 사용
  - Sync ON 시 `delay_time_ms = (60000 / bpm) * noteFraction` 으로 자동 계산
  - 기존 `delay_time` 노브는 Sync OFF일 때만 활성화 (UI에서 회색 처리)
- `Source/UI/EffectBlock.cpp` 또는 신규 DelayPanel — Sync 토글 + Note 선택 ComboBox 추가

**3. NoiseGate EffectBlock UI** (Phase 1 누락)
- `Source/UI/EffectBlock` 패턴 재사용 (Phase 4에서 확립)
- 노브 4개: Threshold (-60~0 dB), Attack (0.1~50 ms), Hold (10~500 ms), Release (10~1000 ms)
- enabled 토글 (`gate_enabled` APVTS 바인딩)
- PluginEditor의 ContentComponent에 NoiseGate 블록 추가 (신호 체인 시작점에 시각적으로 배치)

**4. Compressor EffectBlock UI** (Phase 3 누락)
- 노브 6개: Threshold, Ratio, Attack, Release, Makeup Gain, Dry Blend
- enabled 토글 (`comp_enabled` APVTS 바인딩)
- 파라미터 노브 우클릭 → 기본값 리셋 동작 확인 (Knob 컴포넌트 기본 동작)
- PluginEditor ContentComponent에 NoiseGate 다음 위치에 배치

### P1 항목
- Compressor 게인 리덕션 시각화 — Phase 9 VUMeter 작업과 연계하여 이월
- PowerAmp 곡선 계수 미세 튜닝(실제 측정 데이터 기반) — 후속 작업

### 테스트 기준
- `Tests/PowerAmpTest.cpp` (신규)
  - 4개 PowerAmpType 각각 동일 입력에 대해 서로 다른 출력 RMS·THD 생성
  - 모든 타입이 0dBFS 사인 입력에서 NaN/Inf 없음
  - Sag enabled vs disabled 출력 차이 검증 (Tube 타입만)
- `Tests/DelayBpmSyncTest.cpp` (신규)
  - bpm=120, note=1/4 → delay_time_ms ≈ 500
  - bpm=140, note=1/8 → delay_time_ms ≈ 214.3
  - Sync OFF → delay_time 노브 값 그대로 사용
- 기존 92개 테스트 그린 유지

### 스모크 테스트
- Cabinet bypass + ToneStack flat에서 6종 앰프 전환 → AmpVoicing 차이 + PowerAmp 포화 차이 모두 청취 가능
- High Gain 입력 시 American Vintage(Tube6550) vs Modern Micro(SolidState) 클리핑 캐릭터 명확히 다름
- NoiseGate Threshold 최대 → 베이스 연주 시 소리 차단 (Phase 1 이월 항목 해소)
- NoiseGate Threshold 최소 → 소리 통과
- Compressor Threshold -20dB + Ratio 8:1 → 강타 시 게인 리덕션 청취 (Phase 3 이월 항목 해소)
- Compressor Dry Blend 1.0 → 원음 통과 확인
- Delay BPM Sync ON + DAW 120BPM + 1/4 → 500ms 딜레이 청취
- Delay BPM Sync OFF → delay_time 노브 직접 조정 동작

### 실행 프롬프트
```
PLAN.md의 Phase 7을 구현해줘. PRD.md 섹션 4·5와 CLAUDE.md PowerAmp/Delay/EffectBlock 섹션을 참고한다.

이 Phase는 Phase 1~6에서 이월된 DSP·UI 누락 항목 4건을 정리하는 단계다.

P0 구현 순서:

1. PowerAmp 앰프별 포화 차별화 (Phase 2 P1 이월)
   - PowerAmp.cpp의 process() 내부에 currentType 분기 추가
   - Tube6550/TubeEL34/SolidState/ClassD 4종 곡선 구현
   - 4x 오버샘플링 유지, Sag와 호환
   - DspReviewer로 RT 안전성 점검

2. Delay BPM Sync (Phase 5 P1 이월)
   - delay_bpm_sync (bool), delay_note_value (choice) APVTS 추가
   - juce::AudioPlayHead로 BPM 추출, Standalone에서 120 fallback
   - Sync ON 시 delay_time_ms 자동 계산, OFF 시 노브 값 사용
   - DelayPanel(또는 EffectBlock 확장)에 Sync 토글 + Note ComboBox

3. NoiseGate EffectBlock UI (Phase 1 누락)
   - EffectBlock 패턴으로 Threshold/Attack/Hold/Release 노브 + enabled 토글
   - PluginEditor ContentComponent의 신호 체인 시작점에 배치

4. Compressor EffectBlock UI (Phase 3 누락)
   - Threshold/Ratio/Attack/Release/Makeup/DryBlend 노브 + enabled 토글
   - NoiseGate 다음 위치에 배치
   - 노브 우클릭 리셋 동작 확인

테스트:
- PowerAmpTest.cpp 신규 (4종 타입 응답 차이 + NaN/Inf 검증)
- DelayBpmSyncTest.cpp 신규 (BPM/note → delay_time_ms 변환 검증)
- 기존 92개 테스트 그린 유지

완료 기준: 신규 테스트 통과, 스모크 테스트 8건 모두 청취 확인, 6종 앰프 포화 차이가 명확히 들림.
완료 후 git add/commit/push. 메시지: "feat(phase7): carryover cleanup — power amp saturation + delay BPM sync + gate/compressor UI"
```

---

## Phase 8 — 프리셋 시스템

**목표**: 프리셋 저장/불러오기/A-B 비교/팩토리 프리셋 15종.
**마일스톤**: 커스텀 프리셋 저장 → 앱 재시작 → 복원, A-B 즉시 전환.

### 🔧 TOOL
- `.claude/agents/PresetMigrator.md` — 기존 프리셋 XML과 현재 APVTS ParameterLayout 비교. 누락 파라미터 탐지 및 기본값 주입 코드 생성. `setStateInformation()` 하위 호환 처리 안내

### P0 구현 항목
- `Source/Presets/PresetManager.h/.cpp` — APVTS ValueTree 직렬화/역직렬화. 저장 경로: `userApplicationDataDirectory/BassMusicGear/Presets/*.bmg`. Export/Import 지원
- `PluginProcessor` — `getStateInformation()` / `setStateInformation()` 완성. 없는 파라미터는 기본값으로 채움
- A/B 슬롯 — `PluginProcessor`에 `slotA`, `slotB` ValueTree 멤버. `saveToSlot(int)` / `loadFromSlot(int)`
- `Resources/Presets/` — 팩토리 프리셋 XML 15종 (Clean/Driven/Heavy × 5 모델). `scripts/GenBinaryData.sh` 실행하여 CMakeLists 갱신
- `Source/UI/PresetPanel.h/.cpp` — 팩토리/유저 구분 목록, Save/Load/Delete/Export/Import 버튼, A/B 슬롯 버튼
- **EQ 사용자 프리셋** — `GraphicEQPanel` 드롭다운에 사용자 프리셋 저장/불러오기/삭제 기능 추가
  - 저장 경로: `userApplicationDataDirectory/BassMusicGear/EQPresets/*.xml`
  - 드롭다운 하단에 "Save Preset..." 항목 추가 → 이름 입력 다이얼로그(AlertWindow) → XML 저장
  - 저장된 사용자 프리셋은 빌트인 프리셋 아래 구분선과 함께 나열
  - 우클릭 또는 Delete 버튼으로 삭제

### 테스트 기준
- `Tests/PresetTest.cpp`
  - 전체 파라미터 저장 → 불러오기 값 일치 (오차 1e-5 이하)
  - A/B 독립성: A 저장 후 변경 → A 로드 → 원래값 복귀
  - 팩토리 프리셋 15종 파싱 오류 없음
  - `.bmg` Export → Import 라운드트립 일치

### 스모크 테스트
- 커스텀 프리셋 저장 → 앱 재시작 → 불러오기 성공
- A/B 버튼 전환 → 즉시 음색 변화

### 실행 프롬프트
```
PLAN.md의 Phase 8을 구현해줘. PRD.md 섹션 12와 CLAUDE.md 프리셋 섹션을 참고한다.

먼저 PresetMigrator agent(.claude/agents/PresetMigrator.md)를 구현한다.

그 다음:
- PresetManager (ValueTree 직렬화, .bmg 파일 저장/로드, Export/Import)
- PluginProcessor getStateInformation/setStateInformation (없는 파라미터는 기본값 처리)
- A/B 슬롯 (slotA/slotB ValueTree, saveToSlot/loadFromSlot)
- 팩토리 프리셋 XML 15종 (Resources/Presets/)
- scripts/GenBinaryData.sh 실행 → CMakeLists BinaryData 갱신
- PresetPanel UI (목록, Save/Load/Delete/Export/Import, A/B 버튼)
- EQ 사용자 프리셋: GraphicEQPanel 드롭다운에 "Save Preset..." 항목 추가 → AlertWindow로 이름 입력 → userApplicationDataDirectory/BassMusicGear/EQPresets/*.xml 저장. 사용자 프리셋은 빌트인 항목 아래 구분선으로 구분하여 나열. 삭제 기능 포함.

완료 기준: PresetTest 통과, 앱 재시작 후 프리셋 복원 확인, A/B 전환 동작 확인, EQ 사용자 프리셋 저장/불러오기/삭제 동작 확인.
완료 후 git add/commit/push. 메시지: "feat(phase8): preset system — manager, 15 factory presets, A/B slots, PresetPanel, EQ user presets"
```

---

## Phase 9 — UI 완성 + 출력 섹션

**목표**: VUMeter, SignalChainView, 전체 UI 레이아웃 완성. Master Volume 동작.
**마일스톤**: 전체 UI 완성, 미터 애니메이션, 블록 ON/OFF 시각화.

> 📝 **Phase 9 시작 전 검토 사항 — Pre-FX 순서 변경 기능**
> 현재 Pre-FX(Overdrive → Octaver → EnvelopeFilter) 순서는 SignalChain.cpp에 하드코딩되어 있음.
> Phase 9에서 SignalChainView를 구현할 때 드래그로 블록 순서를 바꾸는 기능을 추가할지 검토할 것.
> - 구현 시: SignalChain에 블록 순서 배열 + process() 동적 디스패치 구조 필요
> - 미구현 시: 현재 고정 순서 유지, SignalChainView는 시각화 + ON/OFF 토글만 담당

### ✅ CARRY
- 앰프 모델별 UI 색상 테마 전체 적용 (Phase 2 P1 → Phase 6 부분 적용 → 전면 LookAndFeel 통합)
- 튜너 참조 주파수 UI (Phase 3 P0 미구현 이월) — `tuner_reference_a` APVTS 파라미터 등록 + TunerDisplay에 참조 주파수 슬라이더/스피너 추가 (430~450Hz, 기본 440Hz)
- Compressor 게인 리덕션 VUMeter 연동 (Phase 3 P0 부분) — VUMeter 구현 시 `Compressor::getGainReductionDb()`를 읽어 게인 리덕션 미터 표시

### P0 구현 항목
- `Source/UI/VUMeter.h/.cpp` — 입력/출력 레벨 바. Peak hold 3초. 클리핑 LED. `juce::Timer` 30Hz. `PluginProcessor`에서 `std::atomic<float>` inputPeak/outputPeak 제공
- `Source/UI/SignalChainView.h/.cpp` — 블록 가로 배열 시각화. 클릭으로 enabled 파라미터 토글. 비활성 블록 반투명
- Master Volume 노브 (`master_volume` APVTS 파라미터, SignalChain 출력단 gain)
- `PluginEditor` 전체 레이아웃 — 상단(TunerDisplay + 툴바) / 중단(AmpPanel + EffectBlock 그리드) / 하단(GraphicEQPanel + DIBlendPanel + BiAmpPanel + CabinetSelector) / 최하단(SignalChainView + VUMeter + Master Volume)
- 다크 테마 LookAndFeel 전면 적용
- 앰프 모델별 UI 색상 테마 전면 적용 (✅ CARRY — Phase 6에서 라벨 색상에만 부분 적용된 것을 LookAndFeel에 통합하여 창 배경/노브 테두리/탭 강조색까지 모두 themeColour 반영. American Vintage 주황/Tweed 크림/British 오렌지/Modern 녹색/Italian 파랑/Origin Pure 실버)
- 리사이즈 지원 — `setResizeLimits(800, 500, 1600, 1000)`, 비율 기반 레이아웃

### P1 항목
- UI 애니메이션 트랜지션 (블록 ON/OFF 페이드) → Post-MVP

### 테스트 기준
- PluginEditor 생성/소멸 크래시 없음
- 창 800×500 → 1600×1000 리사이즈 시 레이아웃 정상
- VUMeter: 0dBFS 입력 → 클립 LED 점등

### 스모크 테스트
- VUMeter 바 움직임 확인
- SignalChainView 블록 클릭 → ON/OFF 전환
- 앰프 모델 전환 → UI 색상 변경 (창 배경 + 노브 테두리 + 탭 강조까지 themeColour 반영 확인)
- Master Volume 0 → 완전 무음
- **[Phase 3 이월]** 튜너 참조 주파수 A=445Hz로 변경 → 센트 편차 수치 변화 확인 (Phase 3에서 UI 없어 미확인 — 고정 440Hz)
- **[Phase 3 이월]** 창 리사이즈 → 레이아웃 비율 유지 확인 (Phase 3에서 리사이즈 미지원 — 최소화/최대화만 동작)
- **[Phase 3 이월]** Compressor 게인 리덕션 미터 청취 + 미터 동시 확인 (VUMeter 구현 후 검증)

### 실행 프롬프트
```
PLAN.md의 Phase 9를 구현해줘. PRD.md 섹션 11, 14를 참고한다.

✅ CARRY 항목 먼저 처리:
- 앰프 모델별 UI 색상 테마 전면 적용 (Phase 6 라벨 부분 적용 → LookAndFeel 전면 통합)
- 튜너 참조 주파수 UI (tuner_reference_a APVTS 등록 + TunerDisplay 슬라이더)
- Compressor 게인 리덕션 VUMeter 연동

그 다음:
- VUMeter (입출력 레벨 바, Peak hold, 클립 LED, 30Hz Timer, Compressor GR 미터 연동)
- SignalChainView (블록 시각화, 클릭 ON/OFF, 비활성 반투명)
- Master Volume 노브 + SignalChain 출력단 gain 적용
- PluginEditor 전체 레이아웃 완성
- 다크 테마 LookAndFeel + 앰프 themeColour 전면 통합
- 리사이즈 지원 (setResizeLimits 800×500 ~ 1600×1000)

완료 기준: 전체 기존 테스트 그린 유지, 리사이즈/VUMeter/SignalChainView/색상 테마 동작 확인.
완료 후 git add/commit/push. 메시지: "feat(phase9): full UI — VUMeter/SignalChainView/dark-theme/model-colors/resize"
```

---

## Phase 10 — 오디오 설정 + 릴리즈 준비

**목표**: Standalone 오디오 설정 완성. 전체 테스트 그린. VST3 DAW 로드 확인.
**마일스톤**: Standalone 장치 설정 동작 + VST3 DAW 로드 + 전체 테스트 통과.

### 🔧 TOOL
- `.claude/commands/InstallPlugin.md` — `/InstallPlugin <format> <config>` 호출 시 빌드 아티팩트를 시스템 플러그인 경로에 복사 (Windows: `%COMMONPROGRAMFILES%\VST3\`, macOS: `~/Library/Audio/Plug-Ins/VST3/`)
- `scripts/RunTests.sh` — Catch2 필터 테스트 실행 + 결과 요약 출력
- `scripts/BumpVersion.sh` — `CMakeLists.txt` VERSION + `git tag` 갱신

### P0 구현 항목
- `Source/UI/SettingsPage.h/.cpp` — CLAUDE.md AudioDeviceManager 코드 패턴 그대로 적용
  - 드라이버 타입 / 입력 장치+채널(모노) / 출력 장치+채널쌍 / SR / 버퍼 / 예상 레이턴시 표시
  - ASIO 패널 열기 버튼
  - 설정 저장(`createStateXml()`) + 앱 시작 시 복원
- Standalone/Plugin 분기 — `isStandaloneApp()` 체크: Standalone → SettingsPage, Plugin → 설정 버튼 비표시
- 전체 테스트 스위트 그린 확인 (`scripts/RunTests.sh`)
- `scripts/BumpVersion.sh patch` → VERSION 0.1.0, `/InstallPlugin vst3 release`

### 테스트 기준
- 전체 테스트 스위트: ToneStackTest/OverdriveTest/CompressorTest/GraphicEQTest/BiAmpCrossoverTest/DIBlendTest/PresetTest 전부 통과
- SettingsPage: 장치 변경 후 AudioDeviceManager 반영 확인

### 스모크 테스트
- Standalone: SettingsPage에서 ASIO 장치 전환 → 오디오 경로 변경
- Standalone: 설정 저장 → 앱 재시작 → 마지막 설정 자동 복원
- VST3: DAW에서 플러그인 로드 → 오디오 처리 정상
- Plugin 모드: 설정 버튼 비표시 확인
- **[Phase 2 이월]** Cabinet IR 전환 시 음색 변화 청취 확인 (Phase 2에서 모든 IR이 placeholder로 음색 차이 없어 미확인 — 실제 IR 파일 연결 후 검증 필요)
- **[Phase 2 이월]** 각 모델의 청각적 특성 재확인 — American Vintage Mid Position 주파수 변화 / British Stack Bass 독립성 / Modern Micro Grunt 저중역 부스트 / Italian Clean VPF·VLE 동작 (placeholder IR로 구분 어려워 미확인 — 실제 IR 연결 후 SmokeTest_Phase2.md 항목 점검)
- **[Phase 2 이월]** Preamp 모델별 청각 특성 재확인 — Tube12AX7 짝수 고조파 / JFET 그릿 / ClassDLinear 클린 (placeholder IR 제거 후 각 앰프 특성 구분 가능해지면 점검)
- **[Phase 3 이월]** 5종 앰프 모델 전환 → 각 모델별 음색 차이 확인 (Phase 3에서 모든 IR이 placeholder로 음색 차이 없어 미확인 — 실제 IR 연결 후 검증)

### 실행 프롬프트
```
PLAN.md의 Phase 10을 구현해줘. PRD.md 섹션 13과 CLAUDE.md AudioDeviceManager 섹션을 참고한다.

먼저 TOOL 3개를 구현한다:
1. .claude/commands/InstallPlugin.md
2. scripts/RunTests.sh
3. scripts/BumpVersion.sh

그 다음:
- SettingsPage (Standalone 전용 — CLAUDE.md AudioDeviceManager 코드 패턴 그대로 적용)
  드라이버/장치/채널/SR/버퍼/레이턴시 표시/ASIO 패널/설정 저장·복원
- isStandaloneApp() 분기 (Standalone → SettingsPage, Plugin → 설정 버튼 비표시)

구현 완료 후:
1. scripts/RunTests.sh 실행 → 전체 테스트 그린 확인 (실패 시 수정 후 재실행)
2. scripts/BumpVersion.sh patch
3. cmake --build build --config Release
4. /InstallPlugin vst3 release

완료 기준: 전체 테스트 통과, Standalone 장치 설정 동작, VST3 DAW 로드 확인.
완료 후 git add/commit/push. 메시지: "feat(phase10): audio settings page + release v0.1.0"
그 후 git push --tags
```

---

## P1 이월 추적표

구현 중 갱신한다. 이월 항목은 다음 Phase의 ✅ CARRY로 반드시 처리.

| Phase 발생 | 항목 | 이월 대상 | 완료 여부 |
|---|---|---|---|
| Phase 1 | PowerAmp Sag | Phase 2 | ✅ (Phase 2에서 완료) |
| Phase 1 | 커스텀 IR 파일 로드 (`Cabinet::loadIR(File)`) | Phase 6 | ✅ (Phase 1에서 조기 완료) |
| Phase 1 | NoiseGate EffectBlock UI | Phase 7 | ☐ |
| Phase 2 | PowerAmp 앰프별 포화 차별화 | Phase 7 | ☐ |
| Phase 2 | 앰프 모델별 UI 색상 테마 (Phase 6 라벨 부분 적용 → Phase 9 LookAndFeel 전면 통합) | Phase 9 | ☐ (부분 적용됨) |
| Phase 2 | 실제 캐비닛 IR 파일 연결 (8x10 SVT / 4x10 JBL / 1x15 Vintage / 2x12 British / 2x10 Modern) | Phase 10 | ☐ |
| Phase 3 | Compressor EffectBlock UI | Phase 7 | ☐ |
| Phase 3 | Compressor 게인 리덕션 VUMeter 연동 | Phase 9 | ☐ |
| Phase 3 | 튜너 참조 주파수 UI (`tuner_reference_a`) | Phase 9 | ☐ |
| Phase 4 | Octaver Oct-Up 음질 개선 | Phase 8 | ✅ (Phase 5에서 완료) |
| Phase 5 | Delay BPM Sync | Phase 7 | ☐ |
