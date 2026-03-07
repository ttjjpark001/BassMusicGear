# Bass Amplifier Simulator — Product Requirements Document

## Overview

JUCE(C++) 기반 베이스 기타 앰프 시뮬레이터. 클래식/모던 베이스 앰프의 톤을 정확하게 재현하며, DAW 플러그인(VST3/AU)과 독립 실행형(Standalone) 앱으로 배포된다.

**지원 플랫폼**: Windows 10+, macOS 12+
**빌드 타겟**: Standalone, VST3 (Windows/macOS), AU (macOS only)
**대상 사용자**: 홈 레코딩 베이시스트, 라이브 연주자, DAW 사용자

---

## 신호 체인 (Signal Chain)

```
입력
  └─ 노이즈 게이트
       └─ 튜너
            └─ 컴프레서 (독립 블록)
                 └─ Bi-Amp 크로스오버 (ON/OFF, 기준 주파수 가변)
                      ├─ LP (저역) 또는 [OFF 시] 전대역 ──[클린 DI]──────┐
                      └─ HP (고역) 또는 [OFF 시] 전대역                   │
                           └─ Pre-FX (오버드라이브, 옥타버, 엔벨로프 필터) │
                                └─ 프리앰프 (Gain, 캐릭터)                │
                                     └─ 톤스택 EQ                        │
                                          └─ 그래픽 EQ (10밴드)          │
                                               └─ Post-FX / FX Loop    │
                                                    └─ 파워앰프          │
                                                         └─ 캐비닛 IR   │
                                                              └─ DI Blend ◄┘
                                                                   └─ 출력 레벨
```

---

## MVP 기능 명세

### 1. 앰프 모델 (5종)

| 모델명 | 원본 참고 | 캐릭터 |
|---|---|---|
| **American Vintage** | Ampeg SVT | 미드 강조, 튜브 그라울, 클래식 록 |
| **Tweed Bass** | Fender Bassman 5F6-A | 워밍, 빈티지, 컨트리/재즈 |
| **British Stack** | Orange AD200 | 풀레인지, EL34 새추레이션, 스무스 |
| **Modern Micro** | Darkglass B3K | 패러렐 드라이브, JFET 클리핑, 모던 메탈 |
| **Italian Clean** | Markbass Little Mark III | 투명, 하이파이, 클래스D 클린 헤드룸, 모던 펑크/재즈 |

각 앰프 모델은:
- 고유한 톤스택 토폴로지 (TMB / James / 액티브 Baxandall / 4-band Active + VPF/VLE)
- 고유한 프리앰프 게인 스테이징 (12AX7 캐스케이드 vs JFET vs 솔리드스테이트 선형)
- 파워앰프 캐릭터 (6550 vs EL34 vs 솔리드스테이트 vs Class D)
- 기본 매칭 캐비닛 IR

### 2. 프리앰프 섹션

- **Input Gain**: 입력 레벨 / 드라이브량
- **Volume**: 프리앰프 출력 레벨
- **톤스택**: 앰프 모델별 EQ
  - American Vintage: Bass / Mid (5-포지션 주파수 선택 스위치) / Treble
  - Tweed Bass: Bass / Mid / Treble (Fender TMB, 패시브 RC 상호작용 포함)
  - British Stack: Bass / Mid / Treble (James 토폴로지)
  - Modern Micro: Grunt (저역 드라이브 필터) / Attack (고역 드라이브 필터) + Bass / Mid / Treble
  - Italian Clean: Bass (40Hz) / Low-Mid (360Hz) / High-Mid (800Hz) / Treble (10kHz) / **VPF** / **VLE**
    - **VPF** (Variable Pre-shape Filter): 미드 스쿱 필터. 0에서 플랫, 최대에서 380Hz 노치 컷 + 35Hz 서브 부스트 + 10kHz 프레즌스 부스트 동시 적용
    - **VLE** (Vintage Loudspeaker Emulator): 가변 1차 로우패스 필터. 0에서 전대역, 최대에서 약 4kHz 이상 점진적 롤오프 (빈티지 스피커 특성 모사)

### 3. 캐비닛 시뮬레이션

- 내장 IR 프리셋 (앰프 모델당 최소 2종: close/dark)
  - 8x10 SVT 캐비닛 (SM57 클로즈)
  - 4x10 JBL (Condenser 클로즈)
  - 1x15 빈티지 (리본 마이크)
  - 2x12 British (SM57)
- 커스텀 IR 파일 로드 (WAV, 최대 500ms / 48kHz)
- 마이크 포지션 가상 조정 (pre-captured IR 간 블렌딩)
- 캐비닛 바이패스 (DI 신호만 사용 시)

### 4. 이펙터

#### Pre-FX (컴프레서 뒤, 프리앰프 앞)

| 이펙터 | 핵심 파라미터 |
|---|---|
| **오버드라이브** | Drive, Tone, Dry Blend (항상 클린 블렌드 포함), Type (Tube/JFET/Fuzz) |
| **옥타버** | Sub Level, Oct-Up Level, Dry Level |
| **엔벨로프 필터** | Sensitivity, Freq Range, Resonance, Direction (Up/Down) |

#### Post-FX / FX Loop (그래픽 EQ 뒤, 파워앰프 앞)

| 이펙터 | 핵심 파라미터 |
|---|---|
| **코러스** | Rate, Depth, Mix (Bass 모드: 크로스오버로 고역만 처리) |
| **딜레이** | Time (ms / BPM sync), Feedback, Damping, Mix |
| **리버브** | Type (Spring/Room), Size, Decay, Mix |

### 5. 튜너

- 크로매틱 튜너, 노이즈 게이트 직후 고정 배치
- **Mute 모드**: 튜닝 중 출력 신호 뮤트 (라이브 사용 시 소리 차단)
- **Pass-through 모드**: 뮤트 없이 피치 표시만 (연습/모니터링 중 사용)
- 표시 정보: 가장 가까운 음이름(C~B), 샤프/플랫, 센트 편차(−50 ~ +50¢), 인디케이터 바
- 참조 주파수 A = 440Hz 기본, 430–450Hz 범위에서 조정 가능
- 피치 감지: YIN 알고리즘 기반 모노 피치 트래킹 (베이스 저역 41Hz~330Hz 대응)
- 에디터 상단 상시 표시 (별도 ON/OFF 토글 없음)

### 6. Bi-Amp 크로스오버 (독립 블록)

컴프레서 직후, Pre-FX 앞에 배치. 클린 DI 분기 지점에서 저역과 고역을 분리한다.

- **ON 시**: Linkwitz-Riley 4차(LR4, 24dB/oct) 크로스오버 필터 적용
  - **LP 출력** (기준 주파수 이하): 클린 DI 경로로 전달 — 앰프/이펙터 처리 없이 원음 유지
  - **HP 출력** (기준 주파수 이상): 나머지 신호 체인(Pre-FX → 프리앰프 → … → 캐비닛 IR)으로 전달
  - DI Blend에서 두 경로를 혼합하면 저역은 클린, 고역은 앰프 캐릭터를 입힌 진정한 바이앰핑 구현
- **OFF 시**: 필터 없이 전대역 신호가 클린 DI와 앰프 체인 양쪽으로 그대로 분기 (기존 동작)
- **Crossover Freq**: 조절 가능 범위 60–500Hz (베이스 바이앰핑 실용 범위)
- LR4 선택 이유: LP + HP를 합산하면 전대역에서 위상이 일치하고 진폭이 평탄하게 복원됨

### 7. 컴프레서 (독립 블록)

노이즈 게이트 직후, Pre-FX 앞에 고정 배치. 드라이브/왜곡 전에 다이나믹을 먼저 정리하는 표준 순서.

- **Threshold**: 압축 시작 레벨
- **Ratio**: 압축 비율 (2:1 ~ ∞:1)
- **Attack / Release**: 압축 반응 속도
- **Gain**: 메이크업 게인
- **Dry Blend**: 패러렐 컴프레션 (클린 신호와 혼합)
- ON/OFF 바이패스 가능

### 8. 그래픽 EQ (10밴드, 독립 블록)

톤스택 EQ 직후, Post-FX 앞에 배치. 앰프 모델의 톤스택과는 별개로 추가적인 주파수 조정을 위한 독립 블록.

| 밴드 | 주파수 | 용도 |
|---|---|---|
| 1 | 31 Hz | 서브 저역 (룸 노이즈, 부밍) |
| 2 | 63 Hz | 베이스 펀치, 킥과의 관계 |
| 3 | 125 Hz | 저역 바디, 웜스 |
| 4 | 250 Hz | 머드 존 (과하면 탁해짐) |
| 5 | 500 Hz | 미드 바디, 풀니스 |
| 6 | 1 kHz | 어퍼 미드, 프레즌스 |
| 7 | 2 kHz | 어택, 클릭 |
| 8 | 4 kHz | 스트링 노이즈, 픽 어택 |
| 9 | 8 kHz | 브릴리언스 |
| 10 | 16 kHz | 에어 (베이스에선 거의 미사용) |

- 각 밴드 ±12dB, 슬라이더 UI
- 전체 플랫 리셋 버튼
- ON/OFF 바이패스 가능

### 9. 노이즈 게이트

- Threshold, Attack, Hold, Release
- 신호 체인 최앞단 고정

### 10. DI Blend (독립 블록)

캐비닛 IR 이후, 출력 직전에 위치. 클린 DI 경로와 프로세스드 경로를 혼합하는 블록.

**두 경로 정의:**
- **클린 DI**: Bi-Amp 크로스오버 직후 분기. Bi-Amp ON 시 LP(저역)만, OFF 시 전대역
- **프로세스드**: Pre-FX → 프리앰프 → 톤스택 → 그래픽 EQ → Post-FX → 파워앰프 → 캐비닛 IR 전체 통과 신호

**컨트롤:**
- **Blend**: 혼합 비율 노브 (0–100%)
  - 0% = 클린 DI 100%, 프로세스드 0%
  - 50% = 클린 DI 50%, 프로세스드 50% (동등 혼합)
  - 100% = 클린 DI 0%, 프로세스드 100%
- **Clean Level**: 클린 DI 경로 개별 레벨 트림 (−12 ~ +12 dB) — Blend 적용 전 각 경로의 레벨을 독립적으로 맞추기 위함
- **Processed Level**: 프로세스드 경로 개별 레벨 트림 (−12 ~ +12 dB)

**출력 계산:**
```
output = (cleanDI × cleanLevel × (1 − blend)) + (processed × processedLevel × blend)
```

저역 펀치와 어택은 클린 DI에서, 앰프 캐릭터와 하모닉 컬러는 프로세스드에서 가져오는 것이 일반적인 사용 패턴.

### 11. 출력 섹션

- **Master Volume**: 최종 출력 레벨
- **VU 미터**: 입력 / 출력 레벨 표시
- **클리핑 인디케이터**: 입력/출력 각각

### 12. 프리셋 시스템

- 팩토리 프리셋: 앰프 모델별 Clean / Driven / Heavy 3종 × 5개 앰프 = 15종
- 유저 프리셋: 플랫폼 표준 경로에 XML 파일로 저장 (JUCE `ValueTree` 직렬화)
- 프리셋 파일 Export / Import (`.bmg` 확장자, XML 기반)
- A/B 비교: 두 프리셋 슬롯 즉시 전환

### 13. 오디오 설정 페이지 (Standalone 전용)

플러그인(VST3/AU) 모드에서는 DAW가 오디오 라우팅을 관리하므로 이 페이지는 Standalone 앱에서만 표시된다.

#### 드라이버 / 장치 선택

| 항목 | Windows | macOS |
|---|---|---|
| 드라이버 타입 | ASIO / WASAPI Exclusive / WASAPI Shared | Core Audio |
| 입력 장치 | 연결된 오디오 인터페이스 목록 | 연결된 오디오 인터페이스 목록 |
| 출력 장치 | 스피커/헤드폰 장치 목록 | 스피커/헤드폰 장치 목록 |

- 드라이버 타입 변경 시 해당 타입에서 사용 가능한 장치 목록 즉시 갱신
- ASIO 드라이버 선택 시 "ASIO 패널 열기" 버튼 제공 (제조사 ASIO 컨트롤 패널 실행)

#### 채널 선택

- **입력 채널**: 선택한 장치의 입력 채널 목록 표시, 베이스가 연결된 채널 하나를 선택 (모노 입력)
  - 예: Input 1, Input 2, … (장치가 제공하는 채널 이름 그대로 표시)
- **출력 채널**: 모니터링/헤드폰으로 사용할 L/R 출력 채널 쌍 선택
  - 예: Output 1/2, Output 3/4, …

#### 버퍼 / 샘플레이트

| 항목 | 선택지 |
|---|---|
| 샘플레이트 | 44100 / 48000 / 96000 Hz (장치가 지원하는 값만 표시) |
| 버퍼 크기 | 32 / 64 / 128 / 256 / 512 samples (드라이버가 지원하는 값만 표시) |
| 예상 레이턴시 | 현재 버퍼 크기 기준 입출력 레이턴시 ms 실시간 표시 (읽기 전용) |

#### 설정 저장

- 설정은 `AudioDeviceManager::createStateXml()`으로 직렬화하여 앱 설정 파일에 저장
- 앱 재시작 시 마지막 설정 자동 복원. 이전에 쓰던 장치가 없으면 시스템 기본 장치로 폴백
- 설정 페이지는 메인 에디터 내 탭 또는 별도 모달 창으로 접근

### 14. UI

- 각 앰프 모델의 실제 하드웨어를 연상시키는 패널 디자인
- 회전 노브 (마우스 드래그 / 스크롤 휠 조작)
- 신호 체인 블록 시각화 (블록 ON/OFF 가능)
- 다크 테마 기본
- 플러그인 포맷에서도 리사이즈 가능한 에디터 창
- 상단 툴바: 프리셋 선택, A/B, 설정(⚙) 버튼 — Standalone에서는 설정 버튼이 오디오 설정 페이지를 열고, 플러그인에서는 비활성화 또는 미표시

---

## 기술 요구사항

### 플랫폼 및 빌드 타겟

| 타겟 | Windows | macOS |
|---|---|---|
| Standalone | ✅ (WASAPI / ASIO) | ✅ (Core Audio) |
| VST3 | ✅ | ✅ |
| AU | ❌ | ✅ |

### 오디오 장치 관리 (Standalone)

| 항목 | 내용 |
|---|---|
| JUCE 클래스 | `juce::AudioDeviceManager` |
| Windows 드라이버 | ASIO (저지연, 권장), WASAPI Exclusive, WASAPI Shared |
| macOS 드라이버 | Core Audio |
| 입력 라우팅 | 다중 채널 장치에서 단일 모노 채널 선택 |
| 출력 라우팅 | 출력 채널 쌍 선택 (스테레오) |
| 설정 저장 | `AudioDeviceManager::createStateXml()` → 앱 설정 디렉터리 |
| 플러그인 모드 | DAW 호스트가 모든 오디오 라우팅 관리 — `AudioDeviceManager` 미사용 |

### 오디오 엔진

| 항목 | 사양 |
|---|---|
| 프레임워크 | JUCE 8 (C++17) |
| 샘플레이트 | 44.1kHz / 48kHz / 96kHz |
| 버퍼 크기 | 32–512 samples (Standalone에서 사용자 선택) |
| 지연 시간 | Windows ASIO: 3–6ms / Core Audio: 3–5ms |
| 내부 오버샘플링 | 비선형 스테이지 4x (`juce::dsp::Oversampling`) |
| 캐비닛 컨볼루션 | `juce::dsp::Convolution` (파티션드 FFT) |
| IR 최대 길이 | 500ms (22050 samples @ 44.1kHz) |
| 파라미터 관리 | `AudioProcessorValueTreeState` (APVTS) |

### 톤스택 DSP

- Fender TMB: 세 컨트롤 상호작용을 포함한 수동 RC 네트워크 정확 이산화 (Yeh 2006 참조)
- James 토폴로지: 독립적 Bass/Treble 셸빙 필터
- Active Baxandall: 피킹 + 셸빙 바이쿼드 조합
- 단순 3개 독립 필터로 구현하지 않음 — 컨트롤 상호작용이 앰프 캐릭터의 핵심

### 오버드라이브 / 새추레이션

- 모든 웨이브쉐이핑 단계에서 최소 4x 오버샘플링 (`juce::dsp::Oversampling`)
- Tube 모드: 비대칭 tanh/arctan 소프트 클리핑 (짝수 고조파 강조)
- JFET 모드: 패러렐 클린+드라이브 구조, 비대칭 클리핑
- Fuzz 모드: 하드 클리핑 (사각파 근사)
- 모든 오버드라이브에 Dry Blend 파라미터 필수

### 오디오 스레드 규칙

- `processBlock()` 내부에서 메모리 할당(`new`/`delete`), 파일 I/O, mutex, 시스템 콜 금지
- UI 스레드 ↔ 오디오 스레드 통신: APVTS 파라미터(atomic) 또는 `juce::AbstractFifo` 기반 락프리 큐
- IR 파일 로드: 백그라운드 스레드에서 디코딩 후 `juce::dsp::Convolution`에 atomic swap

---

## 2차 확장 기능 (Post-MVP)

- 추가 앰프 모델: Ampeg B-15 (스튜디오 빈티지), GK 800RB (솔리드스테이트 클린), Mesa Subway D-800
- 파라메트릭 EQ 블록 (독립 인서트)
- 멀티 마이크 IR 동시 블렌딩 (클로즈 + 룸)
- MIDI CC 매핑 (노브 실시간 제어, `juce::MidiMessage`)
- AAX 빌드 타겟 추가 (Pro Tools 지원, Avid AAX SDK 필요)
- CLAP 포맷 지원 (`clap-juce-extensions`)
- 오디오 캡처 / WAV 내보내기 (Standalone 전용)

---

## 범위 외 (Out of Scope — MVP)

- Pro Tools AAX (Avid 파트너십 및 별도 SDK 필요 — Post-MVP)
- 멀티트랙 시퀀서/DAW 기능
- 네트워크 프리셋 공유 / 클라우드 동기화
- 모바일 (iOS/Android)
- 물리적 회로 시뮬레이션 (WDF) — 추후 정확도 향상 목적으로 검토
