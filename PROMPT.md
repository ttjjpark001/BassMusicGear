# PROMPT.md — Phase별 Claude Code 실행 프롬프트

각 Phase 시작 시 해당 섹션의 프롬프트를 Claude Code 세션에 붙여넣어 실행한다.
참조 문서: `PRD.md`, `CLAUDE.md`, `PLAN.md`, `TOOLING.md`

---

## Phase 0 — 프로젝트 스켈레톤

```
PLAN.md의 Phase 0를 구현해줘.

## 이번 Phase 목표
CMake + JUCE 8 + Catch2 빌드 환경을 구축하고, 창이 뜨는 빈 플러그인을 만든다.

## TOOL 먼저 구현 (코드 작업 전에 완료)
1. scripts/clean-build.sh 작성
   - 내용: build/ 디렉터리 완전 삭제 후 cmake -B build -DCMAKE_BUILD_TYPE=Debug 실행

## 구현할 파일
1. CMakeLists.txt
   - cmake_minimum_required(VERSION 3.22), C++17
   - JUCE git submodule (add_subdirectory(JUCE))
   - juce_add_plugin: Standalone/VST3/AU 타겟, CLAUDE.md의 핵심 구조 참고
   - Catch2 FetchContent로 추가
   - Tests/ 서브디렉터리 포함
2. Tests/CMakeLists.txt
   - Catch2 테스트 타겟 정의 (BassMusicGear_Tests)
3. Tests/SmokeTest.cpp
   - TEST_CASE("build sanity") { REQUIRE(true); } — 항상 통과하는 더미 테스트
4. Source/PluginProcessor.h/.cpp
   - AudioProcessor 상속, prepareToPlay/processBlock/releaseResources 스텁
   - APVTS 멤버 선언 (파라미터 레이아웃은 빈 채로)
5. Source/PluginEditor.h/.cpp
   - AudioProcessorEditor 상속, 800×500 크기
   - 중앙에 "BassMusicGear - Work in Progress" 텍스트 표시

## 완료 기준 (모두 통과해야 Phase 종료)
- cmake -B build && cmake --build build --target BassMusicGear_Standalone 성공
- cmake --build build --target BassMusicGear_Tests && ctest --test-dir build --output-on-failure 통과
- Standalone 실행 시 창 표시, 크래시 없음

## 완료 후
git add, commit, push 해줘. 커밋 메시지: "feat(phase0): project skeleton — JUCE + CMake + Catch2"
```

---

## Phase 1 — 핵심 신호 체인

```
PLAN.md의 Phase 1을 구현해줘. PRD.md 섹션 2, 3, 9와 CLAUDE.md의 핵심 JUCE 패턴을 참고한다.

## TOOL 먼저 구현 (코드 작업 전에 완료)
아래 3개 파일을 생성한다:

1. .claude/commands/add-param.md
   내용: 사용자가 "/add-param <id> <type> [min max default unit]" 형태로 호출하면
   PluginProcessor.cpp의 createParameterLayout()에 추가할 AudioParameterFloat/Bool 코드 스니펫,
   SliderAttachment/ButtonAttachment 바인딩 코드,
   getRawParameterValue() 오디오 스레드 읽기 예시를 출력한다.
   $ARGUMENTS를 파라미터 설명으로 받는다.

2. .claude/commands/dsp-audit.md
   내용: 사용자가 "/dsp-audit <파일경로>" 로 호출하면 해당 DSP 파일을 읽고
   다음을 점검한다:
   - processBlock() 내 new/delete, mutex, 파일 I/O, 시스템 콜 존재 여부
   - 비선형 스테이지에서 juce::dsp::Oversampling 사용 여부
   - 파라미터 읽기 방식이 getRawParameterValue()->load() 인지 여부
   - 필터 계수 재계산이 오디오 스레드에서 이루어지는지 여부
   - setLatencySamples() 누락 여부 (PluginProcessor 파일인 경우)
   각 항목 통과/위반 여부와 위반 시 수정 방법을 출력한다.

3. .claude/agents/dsp-reviewer.md
   내용: DSP 구현 파일 검토에 특화된 에이전트 정의.
   검사 항목: RT 안전성, 오버샘플링 대칭성, 계수 재계산 위치, Dry Blend 누락.

## 구현 순서 및 파일 목록

### DSP 레이어
1. Source/DSP/Effects/NoiseGate.h/.cpp
   - 파라미터: threshold(-80~0dBFS), attack(1~100ms), hold(0~500ms), release(10~1000ms)
   - 히스테리시스: open_threshold > close_threshold + 3dB
   - processBlock(): APVTS atomic 읽기, 게이트 상태 머신 (CLOSED/ATTACK/HOLD/RELEASE/OPEN)

2. Source/DSP/ToneStack.h/.cpp
   - enum ToneStackType { TMB_FENDER, JAMES, BAXANDALL, BAXANDALL_MODERN, MARKBASS }
   - Tweed Bass(TMB_FENDER)만 구현. CLAUDE.md 톤스택 규칙 참고.
   - updateCoefficients(float bass, float mid, float treble, float sampleRate): IIR 계수 재계산
   - 계수 재계산은 메인 스레드에서만. std::atomic으로 오디오 스레드에 전달.

3. Source/DSP/Preamp.h/.cpp
   - 파라미터: inputGain(0~40dB), volume(0~1)
   - juce::dsp::Oversampling<float> 4x 적용
   - 웨이브쉐이핑: 비대칭 tanh 소프트 클리핑 (짝수 고조파)
   - prepareToPlay()에서 oversampling.initProcessing() 호출

4. Source/DSP/PowerAmp.h/.cpp
   - 파라미터: drive(0~1), presence(0~1)
   - Sag는 이번 Phase 미구현 (P1 이월)
   - soft saturation + presence 고역 부스트 (간단한 shelving)

5. Source/DSP/Cabinet.h/.cpp
   - juce::dsp::Convolution 멤버
   - loadIR(const juce::File&): 백그라운드에서 IR 로드
   - loadBuiltinIR(int index): BinaryData에서 로드 (이번 Phase는 placeholder IR 1개)
   - processBlock(): 모노 입력 → 스테레오 출력 (IR이 스테레오인 경우)
   - 바이패스 파라미터 지원

6. Source/DSP/SignalChain.h/.cpp
   - prepare(double sampleRate, int samplesPerBlock): 모든 DSP 모듈 prepareToPlay 호출
   - process(juce::AudioBuffer<float>&): NoiseGate→Preamp→ToneStack→PowerAmp→Cabinet 순서
   - 모든 DSP 모듈 소유 (unique_ptr)

7. Source/PluginProcessor.h/.cpp 업데이트
   - createParameterLayout(): gate_threshold, gate_attack, gate_hold, gate_release,
     input_gain, volume, bass, mid, treble, poweramp_drive, poweramp_presence,
     cabinet_bypass, master_volume 파라미터 등록
   - prepareToPlay(): signalChain.prepare() 호출, setLatencySamples(oversamplingLatency + convolutionLatency)
   - processBlock(): signalChain.process(buffer) 호출

### Resources
8. Resources/IR/placeholder_8x10_svt.wav
   - 44100Hz, 모노, 1024 samples, 임펄스(sample[0]=1.0, 나머지=0.0)로 생성
   - CMakeLists.txt의 BinaryData에 포함

### UI 레이어
9. Source/UI/Knob.h/.cpp
   - juce::Slider(Rotary) 래퍼
   - 마우스 드래그: LinearDragSensitivity 적용
   - 우클릭: 기본값으로 리셋
   - 레이블 텍스트 표시

10. Source/PluginEditor.h/.cpp 업데이트
    - Knob 컴포넌트로 InputGain, Volume, Bass, Mid, Treble, Drive, Presence 배치
    - Cabinet Bypass 토글 버튼
    - SliderAttachment/ButtonAttachment로 APVTS 바인딩

## 완료 기준
- cmake --build → 에러 없음
- Tests/ToneStackTest.cpp 작성 및 통과:
  - Tweed Bass, Bass=0.5/Mid=0.5/Treble=0.5 시 100Hz~5kHz 구간 ±3dB 이내
  - Tweed Bass, Bass=1.0 시 100Hz 게인 > 0dB 게인 (저역 부스트 확인)
- Tests/OverdriveTest.cpp 작성 및 통과:
  - 4x 오버샘플, 10kHz 사인파 입력(클리핑 유발), FFT로 앨리어싱 성분 -60dBFS 이하 확인
- /dsp-audit으로 SignalChain.cpp, Preamp.cpp 점검 후 이슈 없음 확인
- Standalone 실행 → 오디오 인터페이스 입력 → 처리된 소리 출력

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase1): core signal chain — Gate/Preamp/ToneStack/PowerAmp/Cabinet"
```

---

## Phase 2 — 전체 앰프 모델 (5종)

```
PLAN.md의 Phase 2를 구현해줘. PRD.md 섹션 1, 2와 CLAUDE.md 톤스택 구현 규칙을 참고한다.

## 이전 Phase P1 이월 항목 (P0로 처리)
- PowerAmp Sag 시뮬레이션 구현 (튜브 모델: American Vintage, Tweed Bass, British Stack)

## TOOL 먼저 구현
1. .claude/commands/tone-stack.md
   사용자가 "/tone-stack <topology> <bass> <mid> <treble>" 로 호출하면
   해당 토폴로지의 전달 함수에 기반한 IIR 계수 계산 과정을 단계별로 출력하고
   updateCoefficients() C++ 구현 스텁을 제시한다.
   지원 topology: fender_tmb, james, baxandall, markbass

2. .claude/commands/new-amp-model.md
   사용자가 "/new-amp-model <name> <type>" 으로 호출하면
   AmpModel.h에 추가할 구조체 항목, ToneStack switch-case 분기,
   AmpModelLibrary에 추가할 등록 코드,
   팩토리 프리셋 XML 3종 스텁(Clean/Driven/Heavy)을 출력한다.

## 구현할 내용

### 1. ToneStack 나머지 4종 추가 (CLAUDE.md 톤스택 구현 규칙 엄수)
- American Vintage: Active Baxandall 계열. Bass/Mid(5포지션 스위치)/Treble.
  Mid 스위치: 5개 주파수(250/500/800/1500/3000Hz) 피킹 필터 전환
- British Stack: James 토폴로지. Bass/Treble 독립 셸빙 + Mid 피킹.
- Modern Micro: Active Baxandall + Grunt 필터(저역 드라이브 HPF+LPF 합성) + Attack 필터(고역 HPF).
- Italian Clean: 4개 독립 바이쿼드(40/360/800/10kHz) + VPF(35Hz 셸빙+380Hz 노치+10kHz 셸빙 합성) + VLE(StateVariableTPTFilter 로우패스 20kHz→4kHz)

### 2. PowerAmp Sag 구현 (P1 이월)
- Sag 파라미터(0~1). 튜브 모델만 활성화.
- RMS 엔벨로프 팔로워로 동적 게인 리덕션 시뮬레이션
- Modern Micro / Italian Clean: Sag=0 고정, 파라미터 비활성화

### 3. Preamp 모델별 게인 스테이징
- American Vintage: 12AX7 3단 캐스케이드 시뮬레이션 (각 스테이지 gain + soft clip)
- Tweed Bass: 기존 구현 유지
- British Stack: EL34 캐릭터 (짝수+홀수 고조파 혼합)
- Modern Micro: JFET parallel 구조 (클린 패스 + 드라이브 패스 합산)
- Italian Clean: 선형 솔리드스테이트 (클리핑 없음, 게인 스테이징만)

### 4. AmpModel 데이터
- Source/Models/AmpModel.h: struct AmpModel { String name; ToneStackType toneStack; PreampType preampType; PowerAmpType powerAmpType; int defaultIrIndex; Colour uiColour; }
- Source/Models/AmpModelLibrary.h/.cpp: 5종 모델 등록, getModel(int index), getModelCount()

### 5. 캐비닛 IR 파일 (placeholder)
- Resources/IR/ 에 5종 추가:
  placeholder_4x10_jbl.wav, placeholder_1x15_vintage.wav,
  placeholder_2x12_british.wav, placeholder_2x10_modern.wav
  (모두 44100Hz 모노, 임펄스 파형)
- CMakeLists.txt BinaryData에 모두 포함

### 6. UI
- Source/UI/AmpPanel.h/.cpp: ComboBox 모델 선택기 + 모델별 노브 레이아웃
  - 모델 변경 시 톤스택 노브 구성 변경 (VPF/VLE 숨김/표시 포함)
- Source/UI/CabinetSelector.h/.cpp: 내장 IR 선택 ComboBox

## 완료 기준
- ToneStackTest 확장: 5종 토폴로지 각각 검증 항목 통과
  - Italian Clean VPF max: 380Hz -6dB 이상 노치 확인
  - Italian Clean VLE max: 8kHz -12dB 이상 롤오프 확인
  - British Stack: Bass/Treble 독립 동작 (Bass 변경 시 8kHz 응답 변화 없음)
- OverdriveTest 확장: JFET 병렬 구조 DryBlend 선형성 확인
- Standalone: 5종 모델 전환 가능, 각 모델 청각적 차이 확인
- /new-amp-model 실행 테스트 (명령 자체 동작 확인)

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase2): all 5 amp models — ToneStack/Preamp/PowerAmp variants"
```

---

## Phase 3 — 튜너 + 컴프레서

```
PLAN.md의 Phase 3을 구현해줘. PRD.md 섹션 5, 7을 참고한다.

## 구현할 내용

### 1. Source/DSP/Tuner.h/.cpp
- YIN 알고리즘 구현 (피치 탐지 범위: 41Hz ~ 330Hz)
  - 버퍼 크기: 최소 2048 samples
  - threshold 파라미터: 0.1 (YIN 기본값)
  - pitchToNoteAndCents(float freq): (noteIndex, cents) 반환
  - 결과를 std::atomic<float> detectedPitch, std::atomic<int> noteIndex, std::atomic<float> centsDeviation 으로 저장
  - UI 스레드에서 Timer로 polling
- 참조 주파수: A4 = 440Hz 기본, 430~450Hz 조정 가능 (APVTS 파라미터)
- Mute 모드: bool muteOutput atomic 플래그. 오디오 스레드에서 버퍼 silence 처리

### 2. Source/DSP/Effects/Compressor.h/.cpp
- juce::dsp::Compressor<float> 내부 사용 또는 직접 구현
- 파라미터: threshold(-60~0dBFS), ratio(1~∞), attack(0.1~200ms), release(10~2000ms), makeupGain(0~30dB), dryBlend(0~1)
- DryBlend: output = dryBlend * input + (1-dryBlend) * compressed
- ON/OFF 바이패스 APVTS 파라미터
- 게인 리덕션 값을 std::atomic으로 저장 (VUMeter/UI에서 표시용)

### 3. SignalChain 수정
- 신호 순서: NoiseGate → Tuner → Compressor → [BiAmpCrossover 자리 확보] → Preamp → ...
- BiAmpCrossover는 아직 bypass 패스스루로 placeholder 삽입

### 4. Source/UI/TunerDisplay.h/.cpp
- juce::Component 상속, juce::Timer 상속
- timerCallback() 30Hz: Tuner DSP에서 atomic 값 읽어 갱신
- 표시: 음이름(C~B) + 샤프/플랫, 센트 편차 바 (-50~+50¢), Mute 버튼
- 에디터 상단에 항상 표시 (별도 ON/OFF 없음)
- 색상: 인튜닝(±5¢) 녹색, 벗어남 노란/빨간

### 5. PluginProcessor APVTS 파라미터 추가
- tuner_reference_a (430~450, 기본 440)
- tuner_mute (bool)
- comp_enabled (bool), comp_threshold, comp_ratio, comp_attack, comp_release, comp_makeup, comp_dry_blend

### 6. 컴프레서 EffectBlock (임시 UI)
- 간단한 패널: enabled 토글 + 6개 파라미터 노브 배치
- Phase 4에서 EffectBlock 공용 컴포넌트로 교체 예정

## 완료 기준
- Tests/CompressorTest.cpp 작성 및 통과:
  - attack 10ms, ratio 8:1, threshold -20dBFS: 10ms 시점 게인 리덕션 -6dB ±1.5dB
  - dryBlend=0.0: 입출력 상관관계 압축된 신호와 일치
  - dryBlend=1.0: 입출력 동일 (게인 리덕션 없음)
  - ON/OFF 바이패스: OFF 시 입력 = 출력
- Standalone: E1(41Hz) 연주 → 튜너 "E" 표시 및 센트 편차 움직임
- Standalone: Mute 버튼 ON → 출력 무음, OFF → 소리 복귀
- /dsp-audit으로 Compressor.cpp, Tuner.cpp 점검

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase3): tuner (YIN) + compressor with dry blend"
```

---

## Phase 4 — Pre-FX (오버드라이브 + 옥타버 + 엔벨로프 필터)

```
PLAN.md의 Phase 4를 구현해줘. PRD.md 섹션 4(Pre-FX), CLAUDE.md 오버드라이브 규칙을 참고한다.

## TOOL 먼저 구현
- .claude/agents/build-doctor.md 작성
  CMake 빌드 실패 시 오류 메시지를 분류하고 수정 방법을 제시하는 에이전트.
  진단 범주: JUCE 모듈 누락, 링크 오류, 플랫폼별 헤더 충돌, AU 빌드 오류.

## 구현할 내용

### 1. Source/DSP/Effects/Overdrive.h/.cpp
- 파라미터: drive(0~1), tone(0~1), dryBlend(0~1), type(Tube/JFET/Fuzz), enabled(bool)
- Tube: 비대칭 tanh/arctan 소프트 클리핑. 짝수 고조파 강조. 4x Oversampling 필수.
- JFET: 클린 패스 + 드라이브 패스 병렬 합산. 비대칭 클리핑. 4x Oversampling 필수.
- Fuzz: 하드 클리핑 (사각파 근사). 8x Oversampling 권장.
- output = dryBlend * input + (1-dryBlend) * clipped  ← CLAUDE.md 규칙 엄수
- Tone 파라미터: 클리핑 후 간단한 1차 LPF/HPF 블렌드

### 2. Source/DSP/Effects/Octaver.h/.cpp
- 파라미터: subLevel(0~1), octUpLevel(0~1), dryLevel(0~1), enabled(bool)
- YIN 피치 트래킹 (Tuner.cpp의 YIN 로직 재활용 또는 별도 인스턴스)
- Sub 옥타브: 감지된 피치의 0.5x 주파수 사인파 합성
- Oct-Up: 감지된 피치의 2x 주파수 사인파 합성
- output = dry*dryLevel + sub*subLevel + octUp*octUpLevel
- 피치 미감지 시 sub/octUp 레벨 fade out

### 3. Source/DSP/Effects/EnvelopeFilter.h/.cpp
- 파라미터: sensitivity(0~1), freqRange(200~2000Hz), resonance(0~1), direction(Up/Down), enabled(bool)
- 엔벨로프 팔로워: RMS 또는 peak 추적 (attack 5ms, release 100ms 고정)
- juce::dsp::StateVariableTPTFilter 로우패스/밴드패스 전환
- direction Up: 엔벨로프 증가 시 컷오프 주파수 증가
- direction Down: 엔벨로프 증가 시 컷오프 주파수 감소

### 4. Source/UI/EffectBlock.h/.cpp (공용 컴포넌트)
- 생성자: EffectBlock(AudioProcessor&, APVTS&, String enabledParamId, vector<String> paramIds)
- ON/OFF 토글 버튼 (LED 스타일)
- 파라미터 노브들 자동 배치 (최대 6개)
- 블록 이름 레이블
- 모든 Pre-FX, Post-FX에서 공용 사용

### 5. SignalChain 수정
- Pre-FX 블록 삽입: [Compressor] → [BiAmp placeholder] → Overdrive → Octaver → EnvelopeFilter → Preamp

### 6. PluginProcessor APVTS 파라미터 추가
- od_enabled, od_drive, od_tone, od_dry_blend, od_type
- oct_enabled, oct_sub_level, oct_octup_level, oct_dry_level
- ef_enabled, ef_sensitivity, ef_freq_range, ef_resonance, ef_direction

## 완료 기준
- OverdriveTest 확장 및 통과:
  - Tube 4x: 10kHz 클리핑 후 FFT, 앨리어싱 -60dBFS 이하
  - Fuzz 8x: 사각파 생성 확인 (THD > 50%)
  - dryBlend=0.0: wet 신호만 출력
  - dryBlend=1.0: dry 신호만 출력 (오차 -96dBFS 이하)
- /dsp-audit으로 Overdrive.cpp, Octaver.cpp, EnvelopeFilter.cpp 각각 점검 후 이슈 없음
- Standalone: Overdrive Tube 타입 ON → 베이스 왜곡 소리 확인
- Standalone: EnvelopeFilter ON, 강하게 연주 → 와우와우 필터 스윕 확인

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase4): pre-fx — overdrive/octaver/envelope-filter + EffectBlock UI"
```

---

## Phase 5 — 그래픽 EQ + Post-FX

```
PLAN.md의 Phase 5를 구현해줘. PRD.md 섹션 4(Post-FX), 섹션 8을 참고한다.

## TOOL 먼저 구현
- scripts/gen-binary-data.sh 작성
  Resources/IR/ 와 Resources/Presets/ 의 파일 목록을 자동 스캔하여
  CMakeLists.txt 의 juce_add_binary_data SOURCES 항목을 갱신하는 스크립트.

## 이전 Phase P1 이월 항목 (해당 시)
- Octaver 품질 개선 필요 시 이번 Phase에서 처리

## 구현할 내용

### 1. Source/DSP/GraphicEQ.h/.cpp
- 10밴드 Constant-Q 피킹 바이쿼드 필터
- 고정 중심 주파수: 31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 Hz
- 각 밴드 게인: -12dB ~ +12dB (APVTS float 파라미터)
- Q factor: sqrt(2) (Constant-Q 1옥타브 밴드폭)
- ON/OFF 바이패스
- 계수 재계산: 게인 변경 시 메인 스레드에서 atomic swap

### 2. Source/DSP/Effects/Chorus.h/.cpp
- LFO 딜레이 모듈레이션
- 파라미터: rate(0.1~5Hz), depth(0~1), mix(0~1), enabled(bool)
- 딜레이 버퍼 크기: 최대 50ms
- 스테레오 확장 (L/R 위상 반전)

### 3. Source/DSP/Effects/Delay.h/.cpp
- 파라미터: time(1~1000ms), feedback(0~0.95), damping(0~1), mix(0~1), enabled(bool)
- 딜레이 버퍼: 최대 2초
- Damping: 피드백 루프 내 1차 LPF (20kHz→500Hz 범위)
- BPM Sync는 P1 이월

### 4. Source/DSP/Effects/Reverb.h/.cpp
- juce::dsp::Reverb 활용 또는 간단한 Schroeder/Moorer 알고리즘
- 파라미터: type(Spring/Room), size(0~1), decay(0~1), mix(0~1), enabled(bool)
- Spring: 더 밝고 탄성 있는 캐릭터
- Room: 넓고 자연스러운 캐릭터

### 5. SignalChain 수정
- Post-FX 위치: ToneStack → GraphicEQ → Chorus → Delay → Reverb → PowerAmp

### 6. Source/UI/GraphicEQPanel.h/.cpp
- 10개 수직 슬라이더 (juce::Slider::LinearVertical)
- 각 슬라이더 아래 주파수 레이블
- "FLAT" 리셋 버튼: 모든 밴드 0dB로 초기화
- ON/OFF 바이패스 버튼

### 7. PluginProcessor APVTS 파라미터 추가
- geq_enabled, geq_band_31, geq_band_63, geq_band_125, ..., geq_band_16k
- chorus_enabled, chorus_rate, chorus_depth, chorus_mix
- delay_enabled, delay_time, delay_feedback, delay_damping, delay_mix
- reverb_enabled, reverb_type, reverb_size, reverb_decay, reverb_mix

## 완료 기준
- Tests/GraphicEQTest.cpp 작성 및 통과:
  - 31Hz 밴드 +12dB: 31Hz 측정 게인 +11.5~+12.5dB
  - 1kHz 밴드 0dB (플랫): 1kHz 측정 게인 ±0.1dB
  - 바이패스 ON: 입력 파형 = 출력 파형 (수치 일치)
  - 전 밴드 0dB: 20Hz~20kHz ±0.5dB 평탄
- Standalone: 그래픽 EQ 31Hz 슬라이더 +12dB → 서브 저역 명확히 강조
- Standalone: Chorus Mix 100% → 모듈레이션 효과 청취
- Standalone: Delay Time 500ms, Feedback 50% → 반복 에코 청취

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase5): graphic-EQ 10-band + post-fx chorus/delay/reverb"
```

---

## Phase 6 — Bi-Amp 크로스오버 + DI Blend

```
PLAN.md의 Phase 6를 구현해줘. PRD.md 섹션 6, 10, CLAUDE.md SignalChain 설명을 참고한다.

## 이전 Phase P1 이월 항목 (P0로 처리)
- 커스텀 IR 파일 로드: Cabinet::loadIR(const juce::File&) 실제 구현 (이전까지 stub 상태였다면)
- Delay BPM Sync (해당 시)

## 구현할 내용

### 1. Source/DSP/BiAmpCrossover.h/.cpp
- Linkwitz-Riley 4차(LR4) 크로스오버: 2차 Butterworth LP 두 번 직렬 연결 → LP/HP
- 파라미터: enabled(bool), crossoverFreq(60~500Hz)
- LP 출력: cleanDI 경로로 (주파수 이하 성분)
- HP 출력: 앰프 체인으로 (주파수 이상 성분)
- OFF 시: 입력 신호를 cleanDI와 ampChain 양쪽에 동일하게 분기 (필터 없음)
- LP + HP 합산 = 원 신호 (위상/진폭 평탄 조건 유지)

### 2. Source/DSP/DIBlend.h/.cpp
- 파라미터: blend(0~1), cleanLevel(-12~+12dB), processedLevel(-12~+12dB), irPosition(Pre/Post)
- 출력 계산:
  mixed = (cleanDI * cleanLevel_linear * (1-blend)) + (processed * processedLevel_linear * blend)
- Post-IR 모드 (기본): mixed를 그대로 출력 (Cabinet IR은 processed 경로에서 이미 적용됨)
- Pre-IR 모드: mixed를 Cabinet으로 전달하여 IR 적용 후 출력
- cleanDI, processed 두 버퍼 입력을 받는 인터페이스

### 3. SignalChain 대규모 개편
- cleanDI 버퍼 분리: BiAmpCrossover LP 출력을 별도 AudioBuffer<float>에 저장
- IR Position 파라미터에 따른 동적 라우팅:
  - Post-IR: [ampChain] → Cabinet → DIBlend(cleanDI, processedWithCabinet) → 출력
  - Pre-IR: [ampChain] → PowerAmp → DIBlend(cleanDI, processedNoCabinet) → Cabinet → 출력
- prepareToPlay()에서 cleanDI 버퍼 할당 (processBlock에서 new 절대 금지)

### 4. Cabinet 업데이트
- loadIR(const juce::File& wavFile): 실제 WAV 파일 읽기 + Convolution 로드
  (백그라운드 스레드 처리는 juce::dsp::Convolution 내부 자동 처리)
- 커스텀 IR 로드 UI 연동 준비

### 5. Source/UI/BiAmpPanel.h/.cpp
- ON/OFF 토글 버튼
- Crossover Freq 노브 (Knob 컴포넌트 사용)

### 6. Source/UI/DIBlendPanel.h/.cpp
- Blend 노브
- Clean Level 노브, Processed Level 노브
- IR Position 토글 버튼 (Pre-IR / Post-IR)

### 7. Source/UI/CabinetSelector 업데이트
- 커스텀 IR 로드 버튼 (FileChooser → Cabinet::loadIR 호출)

### 8. PluginProcessor APVTS 파라미터 추가
- biamp_enabled, biamp_freq
- di_blend, di_clean_level, di_processed_level, di_ir_position

## 완료 기준
- Tests/BiAmpCrossoverTest.cpp 작성 및 통과:
  - LR4 200Hz: LP와 HP 합산 → 20Hz~10kHz ±0.1dB 평탄
  - LR4 200Hz: LP의 -6dB 지점이 195~205Hz 이내
  - OFF 모드: cleanDI 신호 = ampChain 입력 신호 (전대역, 위상 동일)
- Tests/DIBlendTest.cpp 작성 및 통과:
  - Blend=0.0: 출력 = cleanDI * cleanLevel_linear (오차 1e-6 이하)
  - Blend=1.0: 출력 = processed * processedLevel_linear
  - Blend=0.5, CleanLevel=+6dB: cleanDI 기여분 +6dB 확인
  - IR Position Post→Pre 전환: 동일 입력에서 다른 출력 확인 (IRtimbre 차이)
- Standalone: Bi-Amp ON + Crossover 200Hz → 저음 클린, 고음 앰프처리 확인
- Standalone: 커스텀 WAV IR 파일 드래그 또는 로드 → 즉시 캐비닛 음색 변경
- /dsp-audit으로 BiAmpCrossover.cpp, DIBlend.cpp, SignalChain.cpp 점검

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase6): bi-amp LR4 crossover + DI blend with IR position switching"
```

---

## Phase 7 — 프리셋 시스템

```
PLAN.md의 Phase 7을 구현해줘. PRD.md 섹션 12와 CLAUDE.md 프리셋 섹션을 참고한다.

## TOOL 먼저 구현
- .claude/agents/preset-migrator.md 작성
  파라미터 추가/삭제/이름 변경 후 프리셋 호환성 검증 에이전트.
  기존 팩토리 프리셋 XML과 현재 APVTS ParameterLayout 비교,
  누락 파라미터 탐지 및 기본값 주입 코드 생성,
  setStateInformation() 하위 호환 처리 안내.

## 구현할 내용

### 1. Source/Presets/PresetManager.h/.cpp
- savePreset(String name): APVTS.copyState() → XML 직렬화 → File 저장
  저장 경로: juce::File::getSpecialLocation(userApplicationDataDirectory) / "BassMusicGear" / "Presets" / name + ".bmg"
- loadPreset(File): XML 파싱 → APVTS.replaceState()
- listUserPresets(): 저장 경로의 .bmg 파일 목록 반환
- exportPreset(File destination): .bmg 파일을 지정 경로에 복사
- importPreset(File source): 지정 경로의 .bmg 파일을 프리셋 디렉터리에 복사
- deletePreset(String name): 파일 삭제

### 2. A/B 슬롯
- PluginProcessor에 ValueTree slotA, slotB 멤버 추가
- saveToSlot(int slot): 현재 APVTS 상태를 slot에 저장
- loadFromSlot(int slot): slot의 상태를 APVTS에 적용
- 슬롯은 앱 생명주기 내에서만 유지 (파일 저장 불필요)

### 3. Factory Presets XML (15종)
Resources/Presets/ 에 생성. 각 파일은 APVTS ValueTree를 XML로 직렬화한 형태.
- american_vintage_clean.bmg, american_vintage_driven.bmg, american_vintage_heavy.bmg
- tweed_bass_clean.bmg, tweed_bass_driven.bmg, tweed_bass_heavy.bmg
- british_stack_clean.bmg, british_stack_driven.bmg, british_stack_heavy.bmg
- modern_micro_clean.bmg, modern_micro_driven.bmg, modern_micro_heavy.bmg
- italian_clean_clean.bmg, italian_clean_driven.bmg, italian_clean_heavy.bmg
각 프리셋은 해당 앰프 모델의 특성을 잘 표현하는 파라미터 값으로 설정.

### 4. scripts/gen-binary-data.sh 실행
Resources/Presets/ 파일 15종이 CMakeLists.txt BinaryData에 포함되었는지 확인 및 갱신.

### 5. PluginProcessor 업데이트
- getStateInformation(): APVTS.copyState()를 XML → binary로 직렬화
- setStateInformation(): binary → XML → APVTS.replaceState() + 하위 호환 처리
  (없는 파라미터는 기본값으로 채움)

### 6. Source/UI/PresetPanel.h/.cpp
- 팩토리 프리셋 / 유저 프리셋 구분 목록 (juce::ListBox)
- 프리셋 선택 → 즉시 로드
- Save 버튼: 현재 세팅을 이름 입력 후 저장
- Delete 버튼: 선택된 유저 프리셋 삭제
- Export/Import 버튼: FileChooser로 .bmg 파일 선택
- A 슬롯 저장 / B 슬롯 저장 / A↔B 전환 버튼

## 완료 기준
- Tests/PresetTest.cpp 작성 및 통과:
  - 전체 파라미터 저장 → 즉시 불러오기: 모든 값 일치 (float 오차 1e-5 이하)
  - A/B 독립성: slotA에 저장 후 파라미터 변경 → loadFromSlot(A) → 원래값 복귀
  - 팩토리 프리셋 15종 모두 파싱 오류 없음
  - .bmg Export → 다른 경로에서 Import → 파라미터 값 일치
  - setStateInformation()에 없는 파라미터 포함 시 기본값으로 채워짐 (크래시 없음)
- Standalone: 커스텀 프리셋 저장 → 앱 재시작 → 불러오기 → 값 일치
- Standalone: A/B 버튼 전환 → 즉시 음색 변화 확인

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase7): preset system — manager, 15 factory presets, A/B slots, PresetPanel"
```

---

## Phase 8 — UI 완성 + 출력 섹션

```
PLAN.md의 Phase 8을 구현해줘. PRD.md 섹션 11, 14를 참고한다.

## 이전 Phase P1 이월 항목 (P0로 처리)
- Delay BPM Sync (해당 시)
- 앰프 모델별 UI 색상 테마 (Phase 2 P1)

## 구현할 내용

### 1. Source/UI/VUMeter.h/.cpp
- juce::Component + juce::Timer (30Hz)
- 입력 레벨 바 + 출력 레벨 바 (각각 독립)
- Peak hold: 3초 후 서서히 하강
- 클리핑 인디케이터: 0dBFS 초과 시 빨간 LED, 2초 유지
- PluginProcessor에서 std::atomic<float> inputPeakLevel, outputPeakLevel 제공
- processBlock()에서 peak level 계산 후 atomic 저장

### 2. Source/UI/SignalChainView.h/.cpp
- 신호 체인 블록들을 가로로 나열 시각화
- 블록: NoiseGate, Tuner, Compressor, BiAmp, Overdrive, Octaver, EnvFilter, Preamp, ToneStack, GraphicEQ, Chorus, Delay, Reverb, PowerAmp, Cabinet, DIBlend
- 각 블록: 이름 텍스트 + ON/OFF LED + 클릭으로 enabled 파라미터 토글
- 비활성 블록: 반투명 처리
- 블록 간 연결선 표시

### 3. Master Volume
- APVTS 파라미터 master_volume (0~1, 기본 0.8)
- SignalChain 출력 직전 gain 적용
- PluginEditor에 Master Volume 노브 추가

### 4. PluginEditor 전체 레이아웃 완성
- 상단 바: TunerDisplay(좌) + 프리셋 드롭다운/A-B 버튼(중) + 설정 버튼(우)
- 중단 좌: AmpPanel (앰프 모델 선택 + 톤스택 노브)
- 중단 우: EffectBlock 그리드 (Pre-FX 3종 + 컴프레서 + Post-FX 3종)
- 하단 좌: GraphicEQPanel
- 하단 우: DIBlendPanel + BiAmpPanel + CabinetSelector
- 최하단: SignalChainView + VUMeter + Master Volume 노브
- 다크 테마 LookAndFeel 전면 적용

### 5. 앰프 모델별 UI 색상 테마 (✅ CARRY)
- American Vintage: 따뜻한 주황/갈색
- Tweed Bass: 크림/베이지
- British Stack: 오렌지/검정
- Modern Micro: 녹색/검정 (Darkglass 스타일)
- Italian Clean: 파랑/회색 (Markbass 스타일)
- AmpPanel과 상단 바 배경색이 선택된 모델 컬러로 변경

### 6. 리사이즈 지원
- setResizable(true, true) + setResizeLimits(800, 500, 1600, 1000)
- 모든 컴포넌트: resized()에서 비율 기반 레이아웃 재계산

## 완료 기준
- 모든 기존 테스트 그린 유지 (회귀 없음)
- VUMeter: 사인파 0dBFS 입력 → 클립 인디케이터 점등 확인
- SignalChainView: Overdrive 블록 클릭 → 이펙터 ON/OFF 전환 확인
- 창 리사이즈 800×500 → 1600×1000 → 레이아웃 깨짐 없음
- 앰프 모델 전환 → 상단 바/AmpPanel 색상 변경 확인
- Master Volume 0 → 완전 무음

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase8): full UI — VUMeter/SignalChainView/dark-theme/model-colors/resize"
```

---

## Phase 9 — 오디오 설정 + 릴리즈 준비

```
PLAN.md의 Phase 9를 구현해줘. PRD.md 섹션 13, CLAUDE.md AudioDeviceManager 섹션을 참고한다.

## TOOL 먼저 구현
1. .claude/commands/install-plugin.md
   사용자가 "/install-plugin <format> <config>" 로 호출하면
   빌드 아티팩트를 시스템 플러그인 경로에 복사하는 명령을 생성한다.
   Windows VST3: %COMMONPROGRAMFILES%\VST3\
   macOS VST3: ~/Library/Audio/Plug-Ins/VST3/
   macOS AU: ~/Library/Audio/Plug-Ins/Components/

2. scripts/run-tests.sh
   선택적 필터 인자를 받아 Catch2 테스트를 실행하고 결과를 요약 출력.
   usage: ./scripts/run-tests.sh [TestName]

3. scripts/bump-version.sh
   인자: major | minor | patch
   CMakeLists.txt의 project(...VERSION x.y.z) 를 갱신하고
   git tag vx.y.z 를 생성한다.

## 구현할 내용

### 1. Source/UI/SettingsPage.h/.cpp (Standalone 전용)
CLAUDE.md의 AudioDeviceManager 코드 패턴을 그대로 따른다.

- 드라이버 타입 ComboBox: ASIO / WASAPI Exclusive / WASAPI Shared (Windows), Core Audio (macOS)
- 입력 장치 ComboBox: 선택된 드라이버에서 사용 가능한 장치 목록
- 입력 채널 ComboBox: 선택된 장치의 입력 채널 목록 (모노 1채널 선택)
- 출력 장치 ComboBox: 출력 장치 목록
- 출력 채널 쌍 ComboBox: Output 1/2, Output 3/4, ... 형태
- 샘플레이트 ComboBox: 장치 지원 값만 표시 (44100/48000/96000)
- 버퍼 크기 ComboBox: 드라이버 지원 값만 표시 (32/64/128/256/512)
- 예상 레이턴시 레이블: (bufferSize / sampleRate * 1000)ms, 읽기 전용
- ASIO 패널 열기 버튼: ASIOAudioIODevice::showControlPanel() 호출
- 설정 저장: deviceManager.createStateXml() → userAppDir/BassMusicGear/settings.xml
- 앱 시작 시 복원: PluginProcessor 생성자에서 settings.xml 읽어 deviceManager.initialise()

### 2. Standalone/Plugin 분기
- PluginEditor 생성자에서 JUCEApplication::isStandaloneApp() 확인
- Standalone: 설정 버튼(⚙) 클릭 → SettingsPage 다이얼로그 열기
- Plugin: 설정 버튼 비표시 또는 비활성화 (회색)

### 3. 전체 테스트 스위트 실행 및 그린 확인
scripts/run-tests.sh 실행 → 전체 통과 확인.
실패 항목이 있으면 수정 후 재실행.
대상 테스트: ToneStackTest, OverdriveTest, CompressorTest, GraphicEQTest,
            BiAmpCrossoverTest, DIBlendTest, PresetTest, SmokeTest

### 4. 릴리즈 준비
- scripts/bump-version.sh patch → CMakeLists.txt VERSION 0.1.0 → 0.1.0 태그
- cmake --build build --config Release (전체 릴리즈 빌드)
- /install-plugin vst3 release 실행 → 시스템 VST3 경로에 설치

## 완료 기준
- scripts/run-tests.sh 실행 → 전체 테스트 PASSED (실패 0건)
- Standalone: SettingsPage에서 오디오 드라이버 변경 → 오디오 경로 즉시 변경
- Standalone: 샘플레이트 48000Hz 변경 → 레이턴시 표시 업데이트
- Standalone: 설정 저장 → 앱 재시작 → 마지막 설정 자동 복원
- VST3: DAW에서 BassMusicGear.vst3 로드 → 플러그인 정상 동작 확인
- VST3: 플러그인 모드에서 설정 버튼 비표시/비활성화 확인
- 전체 신호 체인 통합: Standalone에서 베이스 입력 → 전체 체인 통과 → 출력 (크래시/아티팩트 없음)

## 완료 후
git add, commit, push. 커밋 메시지: "feat(phase9): audio settings page + release v0.1.0"
scripts/bump-version.sh patch 실행 후 git push --tags
```
