# 스모크 테스트 체크리스트 — Phase 8

> 자동화된 항목은 ctest로 검증 완료.  
> **(수동 확인 필요)** 표시 항목은 Standalone 앱을 직접 실행하여 귀로 확인한다.  
> Standalone 경로: `build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

---

## 자동 검증 결과 (ctest)

| 항목 | 결과 |
|------|------|
| Standalone 실행 파일 존재 확인 | PASS |
| 전체 단위 테스트 (Phase 8, 26건) | 26 / 26 PASS |

---

## Phase 8 신규 단위 테스트 요약 (자동)

### NoiseGate (5건)

| 테스트 케이스 | 번호 | 결과 |
|--------------|------|------|
| NoiseGate: bypass passes signal unchanged when enabled=false | #104 | PASS |
| NoiseGate: threshold at 0 dB mutes signal | #105 | PASS |
| NoiseGate: very low threshold passes loud signal | #106 | PASS |
| NoiseGate: attack 1ms opens faster than 50ms | #107 | PASS |
| NoiseGate: no NaN/Inf with edge-case parameters | #108 | PASS |

### Preamp (4건)

| 테스트 케이스 | 번호 | 결과 |
|--------------|------|------|
| Preamp: 4x oversampling suppresses high-frequency aliasing | #109 | PASS |
| Preamp: Tube/JFET/ClassD produce different RMS outputs | #110 | PASS |
| Preamp: hard drive stays within amplitude envelope | #111 | PASS |
| Preamp: silence input produces silence output (no NaN/Inf) | #112 | PASS |

### Tuner (4건)

| 테스트 케이스 | 번호 | 결과 |
|--------------|------|------|
| Tuner: detects E1 (41.2 Hz) within acceptable tolerance | #113 | PASS |
| Tuner: detects A2 (110 Hz) within acceptable tolerance | #114 | PASS |
| Tuner: Hz-to-note name conversion covers E/A/D/G | #115 | PASS |
| Tuner: silence input does not crash | #116 | PASS |

### Preset (4건)

| 테스트 케이스 | 번호 | 결과 |
|--------------|------|------|
| Preset: APVTS round-trip via ValueTree XML | #117 | PASS |
| Preset: missing parameters in XML keep defaults | #118 | PASS |
| Preset: user preset directory path is well-formed | #119 | PASS |
| Preset: A/B slot simulation via ValueTree snapshots | #120 | PASS |

### InputPad — Active/Passive 입력 패드 (5건, Phase 8 신규)

| 테스트 케이스 | 번호 | 결과 |
|--------------|------|------|
| InputPad: Passive mode passes signal unchanged | #121 | PASS |
| InputPad: Active mode applies -10 dB attenuation | #122 | PASS |
| InputPad: Active-vs-Passive RMS ratio is -10 dB (±0.5 dB) | #123 | PASS |
| InputPad: 0 dBFS input with Active pad produces no NaN/Inf | #124 | PASS |
| InputPad: repeated Active/Passive toggling is consistent | #125 | PASS |

---

## Active/Passive 입력 패드 (Phase 8 핵심)

### 자동 검증 (단위 테스트 통과)

- [x] Passive 모드: 입출력 동일, 게인 변화 없음 (RMS 오차 1e-5 이하)
- [x] Active 모드: 버퍼 전체에 0.3162(-10dB) 게인 적용 확인
- [x] Active vs Passive RMS 비율: -10dB ±0.5dB 이내
- [x] 0dBFS 입력 + Active → NaN/Inf 없음, 최대 진폭 0.3162 이하
- [x] 10블록 연속 Active/Passive 전환 → 블록 간 상태 누수 없음

### 수동 확인 항목

- [x] Standalone 실행 → Input Gain 노브 옆에 Passive/Active 토글 버튼 표시 확인 (확인 완료)
- [x] Active 버튼 선택 → 강조색(붉은 계열) 표시, Passive는 기본색 (확인 완료)
- [x] 베이스를 연결하고 Active 토글 ON → 소리가 약 10dB 작아짐 (액티브 베이스 과입력 방지 확인) (확인 완료)
- [x] Active → Passive 전환 → 소리 크기가 원래대로 복귀 (확인 완료)
- [x] Active 상태에서 프리셋 저장 → 재시작 후 Active 상태 유지 (확인 완료)

---

## 프리셋 시스템 (Phase 8 핵심)

### 자동 검증 (단위 테스트 통과)

- [x] APVTS gain, input_active, amp_model 파라미터 ValueTree 직렬화 → 역직렬화 일치 (오차 0.01 이하)
- [x] 이전 버전 프리셋 (파라미터 누락) 로드 → 누락 파라미터는 현재값 유지, 크래시 없음
- [x] 유저 프리셋 경로: %APPDATA%/BassMusicGear/Presets (절대 경로, 구조 정확)
- [x] A/B 슬롯: 스냅샷 A(gain=0.2) / B(gain=0.8) 각각 독립 복원 확인

### 수동 확인 항목

- [x] 팩토리 프리셋 로드 → 파라미터 값 즉시 반영 확인 (확인 완료)
- [x] 커스텀 파라미터 설정 → 저장 → Standalone 앱 종료 → 재시작 → 저장한 프리셋 불러오기 → 파라미터 값 일치 (확인 완료)
- [x] A 슬롯 저장 → 파라미터 변경 → A 슬롯 로드 → 원래값 복귀 (즉시 음색 변화 청취) (확인 완료)
- [x] B 슬롯에 다른 음색 저장 → A/B 전환 버튼 반복 클릭 → 두 음색 즉시 교체 (확인 완료)
- [x] .bmg 파일 Export → Import → 파라미터 일치 (확인 완료)

---

## NoiseGate 동작 청취

- [x] NoiseGate Threshold 최대(0dB) → 베이스를 약하게 치면 완전 무음 (확인 완료)
- [x] NoiseGate Threshold 최소(-60dB) → 모든 신호 통과, 배경 노이즈도 통과 (확인 완료)
- [x] Attack 1ms vs 50ms 차이 → 1ms는 게이트 오픈이 즉각적, 50ms는 느린 페이드인 (확인 완료)
- [x] NoiseGate Enabled OFF → Threshold/Attack/Release 설정 무관하게 신호 통과 (확인 완료)

---

## Preamp 튜브 모델 차별화 청취

- [x] 앰프 모델 American Vintage(Tube12AX7) 선택 → 따뜻하고 부드러운 포화음 청취 (확인 완료)
- [x] 앰프 모델 Modern Micro(JFETParallel) 선택 → 클린+그릿 혼합, Tube보다 날카로운 어택 (확인 완료)
- [x] 앰프 모델 Italian Clean(ClassD) 선택 → 왜곡 없음, 투명한 클린 톤 (확인 완료)
- [x] Input Gain 최대 → Tube12AX7 포화: NaN/Inf 없음, 크래시 없음 (확인 완료)

---

## 튜너 동작 확인

- [x] E1 현(41Hz) 연주 → 튜너 디스플레이에 "E" 표시, 센트 편차 바 움직임 (확인 완료)
- [x] A1 현(55Hz) 연주 → "A" 표시 확인 (확인 완료)
- [x] D2 현(73Hz) 연주 → "D" 표시 확인 (확인 완료)
- [x] G2 현(98Hz) 연주 → "G" 표시 확인 (확인 완료)
- [x] 무음 상태 → 튜너 피치 미감지, 크래시 없음 (확인 완료)
- [x] Tuner Mute ON → 완전 무음 출력 (확인 완료)
- [x] Tuner Mute OFF → 원래 소리 복귀 (확인 완료)

---

## 기본 신호 체인 동작 (이전 Phase 연속성 확인)

- [x] Standalone 실행 → 창 표시, 크래시 없음 (확인 완료)
- [x] 오디오 인터페이스 입력 → 처리된 소리 출력 (확인 완료)
- [x] 앱 종료 → 크래시 없음 (확인 완료)
- [x] 앰프 모델 6종 전환 → 노브 레이아웃 변경, 음색 변화 확인 (확인 완료)
- [x] GraphicEQ FLAT 버튼 → 전 밴드 0dB 복귀 (확인 완료)
- [x] Chorus Mix 100% → 모듈레이션 확인 (확인 완료)
- [x] Delay 500ms → 에코 청취 (확인 완료)
- [x] Reverb Room → 공간감 확인 (확인 완료)
- [x] Bi-Amp ON + Crossover 200Hz → 저음 클린/고음 앰프처리 분리 청취 (확인 완료)
- [x] DI Blend 0% → 클린 DI만 / 100% → 앰프 처리음만 (확인 완료)
- [x] Master Volume 0 → 완전 무음 (확인 완료)

---

## Standalone 전용

- [x] SettingsPage 열기 → 드라이버/장치/SR/버퍼 선택 가능 (확인 완료)
- [x] 설정 저장 → 재시작 후 자동 복원 (확인 완료)
