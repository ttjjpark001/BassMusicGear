# PLAN.md — BassMusicGear 단계별 구현 계획

## 태그 정의

| 태그 | 의미 |
|---|---|
| **P0** | 해당 Phase 내 반드시 완료. 미완료 시 Phase 종료 불가 |
| **P1** | 해당 Phase 목표이나 다음 Phase로 이월 허용. 이월 시 다음 Phase 첫 우선순위로 처리 |
| **🔧 TOOL** | Phase 시작 전 구현할 Tooling (slash command / agent / script). 실제 코드 작업 전에 완료 |
| **✅ CARRY** | 이전 Phase에서 이월된 P1 항목. 이 Phase의 P0로 처리 |

---

## Phase 개요

| Phase | 이름 | 핵심 산출물 | 실행 가능 마일스톤 |
|---|---|---|---|
| **0** | 프로젝트 스켈레톤 | CMakeLists, 빈 Plugin, Catch2 셋업 | Standalone 창이 뜬다 |
| **1** | 핵심 신호 체인 | Gate + Preamp(Tweed) + Cabinet + PowerAmp + SignalChain | 베이스 소리가 처리되어 나온다 |
| **2** | 전체 앰프 모델 | 5종 ToneStack + AmpModelLibrary + AmpPanel | 5종 모델 전환 가능 |
| **3** | 튜너 + 컴프레서 | YIN Tuner + Compressor + TunerDisplay | 튜너 동작, 컴프레서 게인 리덕션 확인 |
| **4** | Pre-FX | Overdrive + Octaver + EnvelopeFilter + EffectBlock UI | Pre-FX 3종 on/off 동작 |
| **5** | 그래픽 EQ + Post-FX | 10-band EQ + Chorus + Delay + Reverb | EQ 슬라이더 + 공간계 이펙터 동작 |
| **6** | Bi-Amp + DI Blend | LR4 크로스오버 + DIBlend + IR Position | 신호 분기/혼합 경로 전환 가능 |
| **7** | 프리셋 시스템 | PresetManager + 팩토리 프리셋 15종 + PresetPanel | 프리셋 저장/불러오기/A-B 비교 |
| **8** | UI 완성 + 출력 | VUMeter + SignalChainView + Knob + Master Volume | 전체 UI 완성 |
| **9** | 오디오 설정 + 릴리즈 | SettingsPage + 전체 테스트 + 설치 | VST3 DAW 로드 + Standalone 장치 설정 |

---

## Phase 0 — 프로젝트 스켈레톤

**목표**: CMake + JUCE + Catch2 빌드 환경 구축. 창이 뜨는 빈 플러그인.

### 🔧 TOOL (Phase 시작 전)
- `scripts/clean-build.sh` — 빌드 디렉터리 삭제 후 CMake 재구성

### P0 구현 항목
- `CMakeLists.txt` — `juce_add_plugin`, Standalone/VST3/AU 타겟, Catch2 FetchContent
- `Tests/CMakeLists.txt` — Catch2 테스트 타겟 정의
- `Source/PluginProcessor.h/.cpp` — 빈 AudioProcessor, APVTS 빈 레이아웃
- `Source/PluginEditor.h/.cpp` — 800×500 빈 창, 타이틀 텍스트

### 테스트
- `Tests/SmokeTest.cpp` — `TEST_CASE("build sanity")`: 항상 pass하는 더미 테스트 1개

### 스모크 테스트
- `cmake -B build && cmake --build build --target BassMusicGear_Standalone` 성공
- `cmake --build build --target BassMusicGear_Tests && ctest` 통과
- Standalone 실행 → 창 표시, 크래시 없음

---

## Phase 1 — 핵심 신호 체인

**목표**: 베이스 신호가 Gate → Preamp → ToneStack → PowerAmp → Cabinet → 출력까지 흐른다.

### 🔧 TOOL (Phase 시작 전)
- `.claude/commands/add-param.md` — `/add-param` slash command 구현
- `.claude/commands/dsp-audit.md` — `/dsp-audit` slash command 구현
- `.claude/agents/dsp-reviewer.md` — `dsp-reviewer` agent 구현

### P0 구현 항목
**DSP:**
- `Source/DSP/Effects/NoiseGate.h/.cpp` — Threshold/Attack/Hold/Release, 히스테리시스
- `Source/DSP/ToneStack.h/.cpp` — Tweed Bass(Fender TMB) 토폴로지만 구현. `updateCoefficients()` 포함
- `Source/DSP/Preamp.h/.cpp` — InputGain, 비대칭 tanh 소프트클리핑, 4x Oversampling
- `Source/DSP/PowerAmp.h/.cpp` — Drive, Presence 파라미터. Sag는 P1
- `Source/DSP/Cabinet.h/.cpp` — `juce::dsp::Convolution` 래퍼, 1개 내장 IR 로드
- `Source/DSP/SignalChain.h/.cpp` — Gate→Preamp→ToneStack→PowerAmp→Cabinet 순서 조립
- `Source/PluginProcessor.h/.cpp` — `processBlock()` → SignalChain 호출, `prepareToPlay()`에서 `setLatencySamples()` 계산/호출, APVTS 파라미터 정의
- `Resources/IR/` — 44.1kHz 모노 placeholder IR WAV 1개 (sine sweep 또는 impulse)

**UI:**
- `Source/UI/Knob.h/.cpp` — RotarySlider 래퍼, 마우스 드래그 + 우클릭 리셋
- `Source/PluginEditor.h/.cpp` — InputGain / Volume / Bass / Mid / Treble Knob 배치, Cabinet 바이패스 토글

### P1 항목
- PowerAmp Sag 시뮬레이션 (Phase 2로 이월)
- 커스텀 IR 파일 로드 (Phase 6으로 이월)

### 테스트
- `Tests/ToneStackTest.cpp` — Tweed Bass TMB: Bass=5/Mid=5/Treble=5 시 ±3dB 이내 평탄 응답, Bass 최대 시 저역 부스트 확인
- `Tests/OverdriveTest.cpp` — Preamp 클리핑: 4x 오버샘플 후 5kHz 입력 시 앨리어싱 -60dBFS 이하

### 스모크 테스트
- Standalone에서 오디오 인터페이스 입력 → 처리된 신호 출력
- 각 노브 조작 시 소리 변화 확인
- `/dsp-audit`으로 SignalChain, Preamp 파일 점검

---

## Phase 2 — 전체 앰프 모델 (5종)

**목표**: 5종 앰프 모델 전체 구현, 모델 전환 UI 동작.

### ✅ CARRY (Phase 1 P1)
- PowerAmp Sag 시뮬레이션

### 🔧 TOOL (Phase 시작 전)
- `.claude/commands/tone-stack.md` — `/tone-stack` slash command 구현
- `.claude/commands/new-amp-model.md` — `/new-amp-model` slash command 구현

### P0 구현 항목
**DSP:**
- `ToneStack` 확장 — American Vintage(Active Baxandall), British Stack(James), Modern Micro(Baxandall + Grunt/Attack), Italian Clean(4-band + VPF + VLE) 추가
- `PowerAmp` — Sag 파라미터 추가 (✅ CARRY). 튜브 모델만 활성화 로직
- `Preamp` — 모델별 게인 스테이징 분기 (12AX7 cascade / JFET / Class D)
- `Source/Models/AmpModel.h` — 앰프 모델 구성 데이터 구조체
- `Source/Models/AmpModelLibrary.h/.cpp` — 5종 모델 등록/조회
- `Resources/IR/` — 5종 캐비닛 IR placeholder WAV (8x10 SVT, 4x10 JBL, 1x15 Vintage, 2x12 British, 2x10 Modern)

**UI:**
- `Source/UI/AmpPanel.h/.cpp` — 앰프 모델 선택 드롭다운, 모델별 노브 레이아웃 전환
- `Source/UI/CabinetSelector.h/.cpp` — 내장 IR 선택 콤보박스

### P1 항목
- 모델별 UI 색상 테마 (Phase 8로 이월)

### 테스트
- `Tests/ToneStackTest.cpp` 확장 — 5종 토폴로지 각각 주파수 응답 검증
  - TMB: Bass 최대 → 100Hz 부스트 +6dB 이상
  - James: Bass/Treble 독립 동작 검증
  - Italian Clean VPF: 380Hz 노치 컷 확인
  - Italian Clean VLE: 컷오프 4kHz → 12kHz 이상 롤오프 확인
- `Tests/OverdriveTest.cpp` 확장 — JFET/Fuzz 웨이브쉐이퍼 앨리어싱 검증

### 스모크 테스트
- 5종 모델 전환 시 노브 레이아웃 변경 확인
- 각 모델이 청각적으로 다른 톤 캐릭터를 가짐
- `/new-amp-model`로 모델 추가 시나리오 테스트

---

## Phase 3 — 튜너 + 컴프레서

**목표**: 신호 체인 앞단(Gate→Tuner→Compressor)이 완성된다.

### P0 구현 항목
**DSP:**
- `Source/DSP/Tuner.h/.cpp` — YIN 알고리즘 피치 트래킹 (41Hz~330Hz). 음이름, 센트 편차 계산. DSP 전용, 결과는 atomic으로 UI 스레드에 전달
- `Source/DSP/Effects/Compressor.h/.cpp` — VCA 컴프레서. Threshold/Ratio/Attack/Release/MakeupGain/DryBlend. `juce::dsp::Compressor` 확장
- `SignalChain` 수정 — Gate→Tuner→Compressor→BiAmp→Preamp… 순서로 재조립

**UI:**
- `Source/UI/TunerDisplay.h/.cpp` — 음이름, 샤프/플랫 기호, 센트 편차 바 (-50¢~+50¢), Mute/Pass-through 토글. 에디터 상단 상시 표시
- 컴프레서 EffectBlock 추가 (EffectBlock 컴포넌트는 Phase 4에서 공용화)

### 테스트
- `Tests/CompressorTest.cpp`
  - Attack 10ms: 10ms 후 게인 리덕션 -6dB ± 1dB 정확도
  - Release 100ms: 릴리즈 타임 상수 검증
  - DryBlend 0.5: 출력 = (dry×0.5 + wet×0.5) 선형성 검증
  - Ratio ∞:1: 임계치 초과 시 하드 리밋 동작

### 스모크 테스트
- 베이스 E1(41Hz) 연주 → 튜너에 "E" 표시 및 센트 편차 표시
- 컴프레서 Ratio 8:1 / Threshold -20dBFS → 피크 레벨 명확히 감소
- Mute 모드 → 출력 무음, Pass-through → 소리 유지

---

## Phase 4 — Pre-FX

**목표**: Pre-FX 3종(오버드라이브, 옥타버, 엔벨로프 필터) 체인에 삽입.

### 🔧 TOOL (Phase 시작 전)
- `.claude/agents/build-doctor.md` — `build-doctor` agent 구현

### P0 구현 항목
**DSP:**
- `Source/DSP/Effects/Overdrive.h/.cpp` — Tube(비대칭 tanh)/JFET(parallel)/Fuzz(hard clip) 웨이브쉐이퍼. 4x Oversampling. DryBlend 필수
- `Source/DSP/Effects/Octaver.h/.cpp` — YIN 피치 트래킹 → 서브 옥타브/옥타브업 사인파 합성. Sub Level / Oct-Up Level / Dry Level
- `Source/DSP/Effects/EnvelopeFilter.h/.cpp` — State Variable Filter + 엔벨로프 팔로워. Sensitivity / Freq Range / Resonance / Direction(Up/Down)
- `SignalChain` — Pre-FX 블록 삽입: Compressor→BiAmp→[Overdrive→Octaver→EnvelopeFilter]→Preamp

**UI:**
- `Source/UI/EffectBlock.h/.cpp` — ON/OFF 토글 + 파라미터 노브 범용 컴포넌트. Pre-FX/Post-FX 블록에서 공용 사용

### P1 항목
- Octaver Oct-Up 오버토운 품질 개선 (Phase 5로 이월 가능)

### 테스트
- `Tests/OverdriveTest.cpp` 확장
  - Tube 모드: 4x 오버샘플, 10kHz 입력 → 앨리어싱 -60dBFS 이하
  - DryBlend=0.0: wet only; DryBlend=1.0: dry only (오차 -96dBFS 이하)
  - Fuzz 하드클리핑: 입력 +6dBFS → 출력 클리핑 확인
- `/dsp-audit` — Overdrive, Octaver, EnvelopeFilter 파일 각각 점검

### 스모크 테스트
- 오버드라이브 각 타입(Tube/JFET/Fuzz) ON/OFF → 청각적 차이 확인
- DryBlend 0% → 완전히 왜곡된 소리, 100% → 클린 소리
- EnvelopeFilter: 강하게 연주 시 필터 스윕 동작

---

## Phase 5 — 그래픽 EQ + Post-FX

**목표**: 그래픽 EQ 10밴드 + Post-FX 3종(Chorus/Delay/Reverb) 완성.

### ✅ CARRY (Phase 4 P1)
- Octaver 품질 개선 (해당 사항 있을 경우)

### 🔧 TOOL (Phase 시작 전)
- `scripts/gen-binary-data.sh` — `Resources/IR/`, `Resources/Presets/` 파일 목록 자동 스캔

### P0 구현 항목
**DSP:**
- `Source/DSP/GraphicEQ.h/.cpp` — 10밴드 Constant-Q 피킹 바이쿼드 (31/63/125/250/500/1k/2k/4k/8k/16kHz). 각 밴드 ±12dB. ON/OFF 바이패스
- `Source/DSP/Effects/Chorus.h/.cpp` — LFO 딜레이 모듈레이션. Rate/Depth/Mix
- `Source/DSP/Effects/Delay.h/.cpp` — Time(ms/BPM sync)/Feedback/Damping/Mix
- `Source/DSP/Effects/Reverb.h/.cpp` — 알고리즘 리버브. Type(Spring/Room)/Size/Decay/Mix
- `SignalChain` — ToneStack→GraphicEQ→[Chorus→Delay→Reverb]→PowerAmp 순서

**UI:**
- `Source/UI/GraphicEQPanel.h/.cpp` — 10개 수직 슬라이더 + 전체 플랫 리셋 버튼
- Post-FX 3종 EffectBlock UI 추가

### P1 항목
- Delay BPM Sync (Phase 8로 이월 가능)

### 테스트
- `Tests/GraphicEQTest.cpp` (신규)
  - 각 10밴드 중심 주파수 정확도 ±5% 이내
  - 밴드 이득 +12dB: 실측 +11.5~+12.5dB
  - 밴드 이득 0dB (플랫): 해당 주파수 ±0.1dB 이내
  - 바이패스: 입력 = 출력 (수치 일치)

### 스모크 테스트
- 그래픽 EQ 31Hz 슬라이더 +12dB → 서브 저역 명확히 부스트
- 코러스 Mix 100% → 모듈레이션 효과 청취
- 딜레이 피드백 50% → 반복 에코 청취

---

## Phase 6 — Bi-Amp 크로스오버 + DI Blend

**목표**: 신호 분기(BiAmpCrossover) + 혼합(DIBlend) + IR Position 전환이 정확하게 동작.

### ✅ CARRY (이전 Phase P1)
- 커스텀 IR 파일 로드 (Phase 1에서 이월된 항목) — `Cabinet::loadIR(File)` 구현
- Delay BPM Sync (Phase 5에서 이월된 경우)

### P0 구현 항목
**DSP:**
- `Source/DSP/BiAmpCrossover.h/.cpp` — Linkwitz-Riley 4차(LR4) 크로스오버. ON: LP→cleanDI / HP→amp chain. OFF: 전대역 양쪽 분기. Crossover Freq 60~500Hz
- `Source/DSP/DIBlend.h/.cpp` — Blend(0~100%), Clean Level(±12dB), Processed Level(±12dB), IR Position(Pre/Post)
  - Post-IR: `mixed = clean*(1-blend) + processed*blend` → 출력 (Cabinet은 processed 경로 내 포함)
  - Pre-IR: `mixed = clean*(1-blend) + processed*blend` → cabinetIR(mixed)
- `SignalChain` 대규모 수정 — IR Position에 따라 Cabinet ↔ DIBlend 연결 순서 동적 전환
- `Cabinet` 수정 — 커스텀 IR WAV 파일 로드 (`loadIR(File)`) 구현

**UI:**
- `Source/UI/BiAmpPanel.h/.cpp` — ON/OFF 토글, Crossover Freq 노브
- `Source/UI/DIBlendPanel.h/.cpp` — Blend 노브, Clean/Processed Level 트림 노브, IR Position Pre/Post 토글

### 테스트
- `Tests/BiAmpCrossoverTest.cpp`
  - LR4 LP + HP 합산: 전 대역 ±0.1dB 평탄
  - LR4 크로스오버 주파수 정확도: 설정 200Hz → 측정 -6dB 지점 195~205Hz
  - OFF 모드: LP = HP = 전대역 (위상/진폭 동일)
- `Tests/DIBlendTest.cpp`
  - Blend=0%: 출력 = cleanDI × cleanLevel (processedLevel 영향 없음)
  - Blend=100%: 출력 = processed × processedLevel (cleanLevel 영향 없음)
  - Blend=50%, CleanLevel=+6dB: 출력에서 클린 신호 +6dB 증가 확인
  - IR Position 전환: Pre-IR → 캐비닛이 혼합 신호 전체에 적용됨 확인

### 스모크 테스트
- Bi-Amp ON + 크로스오버 200Hz: 저음 베이스 줄은 클린 DI, 고음은 앰프 처리 확인
- DI Blend 50%: 클린 + 처리된 신호 혼합 청취
- IR Position Pre↔Post 전환: 서로 다른 공간감/캐비닛 색감 확인
- 커스텀 IR WAV 파일 로드 → 즉시 반영 확인

---

## Phase 7 — 프리셋 시스템

**목표**: 프리셋 저장/불러오기/A-B 비교/팩토리 프리셋 15종.

### 🔧 TOOL (Phase 시작 전)
- `.claude/agents/preset-migrator.md` — `preset-migrator` agent 구현

### P0 구현 항목
**DSP/Logic:**
- `Source/Presets/PresetManager.h/.cpp` — APVTS ValueTree 직렬화/역직렬화. 파일 저장 경로: `userApplicationDataDirectory/BassMusicGear/Presets/`. Import/Export `.bmg` 형식(XML)
- `getStateInformation()` / `setStateInformation()` 완성 — 모든 파라미터 포함 검증
- `Resources/Presets/` — 팩토리 프리셋 XML 15개 (Clean/Driven/Heavy × 5 앰프 모델)
- `scripts/gen-binary-data.sh` — 실제 실행하여 `CMakeLists.txt` SOURCES 갱신

**UI:**
- `Source/UI/PresetPanel.h/.cpp` — 프리셋 목록 브라우저(팩토리/유저 구분), Save/Load/Delete 버튼, A/B 슬롯 전환 버튼, Export/Import 버튼
- 상단 툴바에 프리셋 드롭다운 + A/B 버튼 통합

### P1 항목
- 프리셋 검색/필터 기능 (Post-MVP)

### 테스트
- `Tests/PresetTest.cpp`
  - 전체 파라미터 저장 → 불러오기: 모든 값 일치 (부동소수점 오차 1e-6 이하)
  - A/B 슬롯 독립성: A 변경 시 B 상태 불변
  - 팩토리 프리셋 XML 파싱: 15종 모두 오류 없이 로드
  - `.bmg` Export → Import 라운드트립: 파라미터 값 일치

### 스모크 테스트
- 커스텀 프리셋 저장 → 앱 재시작 → 불러오기 성공
- A/B 버튼으로 두 세팅 즉시 전환 (딜레이 없음)
- 팩토리 프리셋 "Tweed Bass Clean" 로드 → 해당 모델/파라미터로 세팅 변경 확인

---

## Phase 8 — UI 완성 + 출력 섹션

**목표**: VUMeter, SignalChainView, 전체 UI 완성. Master Volume 동작.

### ✅ CARRY (이전 Phase P1)
- Delay BPM Sync (해당 사항 있을 경우)
- 모델별 UI 색상 테마 (Phase 2에서 이월)

### P0 구현 항목
**UI:**
- `Source/UI/VUMeter.h/.cpp` — 입력/출력 레벨 바 미터. Peak hold. 클리핑 인디케이터(빨간불). `juce::Timer` 30Hz 갱신
- `Source/UI/SignalChainView.h/.cpp` — 신호 체인 블록 시각화. 각 블록 클릭으로 ON/OFF 토글. 블록 순서 표시
- Master Volume 노브 (APVTS `master_volume` 파라미터, SignalChain 출력단 적용)
- `Source/PluginEditor` 전체 레이아웃 완성 — 상단(TunerDisplay + 툴바) / 중단(AmpPanel + EffectBlocks) / 하단(SignalChainView + VUMeter) 배치
- 다크 테마 CSS/LookAndFeel 구현
- 플러그인 에디터 리사이즈 지원 (최소 800×500, 최대 1600×1000)
- 앰프 모델별 UI 색상 테마 (✅ CARRY)

### P1 항목
- 애니메이션 트랜지션 (블록 ON/OFF 페이드, 프리셋 전환)

### 테스트
- UI 통합 테스트 — `PluginEditor` 생성/소멸 크래시 없음
- 리사이즈 테스트 — 최소/최대 크기에서 레이아웃 깨짐 없음
- VUMeter 범위 — -60dBFS 입력 시 미터 하단, 0dBFS 입력 시 클립 표시

### 스모크 테스트
- 오디오 재생 중 VUMeter 입력/출력 바 움직임 확인
- SignalChainView에서 Overdrive 블록 클릭 → 이펙터 ON/OFF 전환 확인
- 창 리사이즈 → 레이아웃 정상 재조정
- Master Volume 0 → 무음

---

## Phase 9 — 오디오 설정 + 릴리즈 준비

**목표**: Standalone 오디오 설정 완성. 전체 테스트 그린. VST3 DAW 로드 확인.

### 🔧 TOOL (Phase 시작 전)
- `.claude/commands/install-plugin.md` — `/install-plugin` slash command 구현
- `scripts/run-tests.sh` — Catch2 필터 테스트 실행 + 결과 요약
- `scripts/bump-version.sh` — VERSION 필드 + Git 태그 갱신

### P0 구현 항목
**Standalone 전용:**
- `Source/UI/SettingsPage.h/.cpp` — `StandalonePluginHolder::getInstance()->deviceManager` 접근
  - 드라이버 타입 선택 (ASIO / WASAPI Exclusive / WASAPI Shared / Core Audio)
  - 입력 장치 + 입력 채널(모노) 선택
  - 출력 장치 + 출력 채널 쌍 선택
  - 샘플레이트 / 버퍼 크기 선택
  - 예상 레이턴시 표시
  - ASIO 패널 열기 버튼
  - 설정 저장 (`createStateXml()`) / 앱 시작 시 복원
- 설정 버튼(⚙) — `isStandaloneApp()` 체크: Standalone → SettingsPage 열기, Plugin → 비활성화

**릴리즈:**
- 전체 테스트 스위트 그린 확인 (`scripts/run-tests.sh` 실행)
- `scripts/bump-version.sh patch` → 버전 0.1.0 태그
- `/install-plugin vst3 release` → 시스템 VST3 경로에 설치
- 빌드 아티팩트 정리

### 테스트
- 전체 테스트 스위트 재실행: ToneStack, Overdrive, Compressor, GraphicEQ, BiAmpCrossover, DIBlend, Preset 전체 통과
- SettingsPage 테스트 — 장치 변경 후 `AudioDeviceManager` 설정 반영 확인

### 스모크 테스트
- Standalone — 오디오 설정 페이지에서 ASIO 장치 전환 → 오디오 경로 변경 확인
- VST3 — DAW(예: Reaper)에서 BassMusicGear.vst3 로드 → 오디오 처리 확인
- DAW 플러그인 모드 — 설정 버튼(⚙) 비활성화 확인
- 프리셋 저장 → 앱 재시작 → 복원 확인

---

## P1 이월 추적표

이 표는 구현 중 갱신한다. 이월된 항목은 다음 Phase의 ✅ CARRY로 반드시 처리.

| Phase 발생 | 항목 | 이월 대상 Phase | 완료 여부 |
|---|---|---|---|
| Phase 1 | PowerAmp Sag | Phase 2 | ☐ |
| Phase 1 | 커스텀 IR 파일 로드 | Phase 6 | ☐ |
| Phase 2 | 모델별 UI 색상 테마 | Phase 8 | ☐ |
| Phase 4 | Octaver 품질 개선 | Phase 5 | ☐ |
| Phase 5 | Delay BPM Sync | Phase 8 | ☐ |
