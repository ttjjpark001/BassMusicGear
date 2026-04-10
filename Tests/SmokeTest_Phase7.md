# 스모크 테스트 체크리스트 — Phase 7

> 자동화된 항목은 ctest로 검증 완료.  
> **(수동 확인 필요)** 표시 항목은 Standalone 앱을 직접 실행하여 귀로 확인한다.  
> Standalone 경로: `build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

---

## 자동 검증 결과 (ctest)

| 항목 | 결과 |
|------|------|
| Standalone 실행 파일 존재 확인 | PASS |
| Standalone 3초 기동 후 크래시 없음 | PASS (exit code 124 = timeout, 정상) |
| 전체 단위 테스트 (103개) | 103 / 103 PASS |

---

## Phase 7 신규 단위 테스트 요약 (자동)

| 테스트 케이스 | 번호 | 결과 |
|--------------|------|------|
| PowerAmp: 4 types produce different output RMS | #93 | PASS |
| PowerAmp: 4 types produce different THD | #94 | PASS |
| PowerAmp: no NaN or Inf on 0dBFS input (all types) | #95 | PASS |
| PowerAmp: no NaN or Inf with Drive=0 for Tube | #96 | PASS |
| PowerAmp: Sag enabled vs disabled differ (Tube) | #97 | PASS |
| PowerAmp: SolidState/ClassD unaffected by Sag param | #98 | PASS |
| DelayBpmSync: 120BPM 1/4박 = 500ms | #99 | PASS |
| DelayBpmSync: 140BPM 1/8박 = 214.3ms | #100 | PASS |
| DelayBpmSync: Sync OFF uses manual time knob | #101 | PASS |
| DelayBpmSync: note fraction formula correctness | #102 | PASS |
| DelayBpmSync: Sync ON vs OFF 시간 차이 | #103 | PASS |

---

## PowerAmp 포화 차별화 — 귀 청취 (수동)

### 기본 동작
- [ ] Standalone 실행 → 창 표시, 크래시 없음 (수동 확인 필요)
- [ ] 앱 종료 → 크래시 없음 (수동 확인 필요)

### AmpVoicing + PowerAmp 차별화 (Phase 7 핵심)

Cabinet bypass + ToneStack flat 상태에서 6종 앰프를 전환하며 청취한다.

- [ ] American Vintage(Tube6550) 선택 → 부드러운 포화, 80Hz 저음 강조, 1.5kHz 약간 감쇠 (수동 확인 필요)
- [ ] Tweed Bass(Tube6550) 선택 → 60Hz 저음 부스트, 600Hz 미드 스쿱, 5kHz 약간 롤오프 (수동 확인 필요)
- [ ] British Stack(TubeEL34) 선택 → 빠른 포화 시작, 500Hz 중역 강조, 60Hz HP 컷 (수동 확인 필요)
- [ ] Modern Micro(SolidState) 선택 → 날카로운 클리핑, 3kHz 공격적 부스트 (수동 확인 필요)
- [ ] Italian Clean(ClassD) 선택 → 거의 선형, 6kHz 약간의 클래리티만 (수동 확인 필요)
- [ ] Origin Pure(ClassD) 선택 → 완전 플랫, 가장 투명한 사운드 (수동 확인 필요)
- [ ] 6종 전환 시 AmpVoicing 차이 + PowerAmp 포화 차이 모두 청각적으로 구분 가능 (수동 확인 필요)

### High Gain 클리핑 캐릭터 비교 (Phase 7 핵심)

Drive 노브를 최대(100%)로 올린 상태에서 베이스를 강하게 연주한다.

- [ ] American Vintage(Tube6550) — 부드러운 비대칭 tanh 포화, 짝수 고조파 풍부, 따뜻한 톤 (수동 확인 필요)
- [ ] Modern Micro(SolidState) — x/(1+|x|) 날카로운 클리핑, 홀수 고조파 공격적, 명확한 어택 (수동 확인 필요)
- [ ] American Vintage vs Modern Micro: 클리핑 캐릭터가 명확히 다름을 귀로 확인 (수동 확인 필요)

### PowerAmp Sag 동작 (Tube 타입만)

- [ ] American Vintage / Tweed Bass / British Stack: Sag 노브 100% → 강타 후 레벨이 일시적으로 눌리는 느낌 (수동 확인 필요)
- [ ] Sag 0% vs 100%: 강하게 연주 시 동적 응답 차이 청취 가능 (수동 확인 필요)
- [ ] Modern Micro / Italian Clean / Origin Pure: Sag 노브를 올려도 변화 없음 (SolidState/ClassD는 sagEnabled=false) (수동 확인 필요)

---

## NoiseGate (Phase 1 이월 항목 해소)

- [ ] NoiseGate Threshold 최대 → 베이스 연주 시 소리 완전 차단 (수동 확인 필요)
- [ ] NoiseGate Threshold 최소(0) → 모든 신호 통과 (수동 확인 필요)
- [ ] Threshold 중간값 → 약한 노이즈는 차단, 강한 연주는 통과 (수동 확인 필요)

---

## Compressor (Phase 3 이월 항목 해소)

- [ ] Compressor Threshold -20dB + Ratio 8:1 → 강타 시 VUMeter 게인 리덕션 표시 (수동 확인 필요)
- [ ] Compressor Dry Blend 1.0 → 원음 그대로 통과 (압축 없음) (수동 확인 필요)
- [ ] Compressor Dry Blend 0.0 → 완전 압축 신호만 출력 (수동 확인 필요)

---

## Delay BPM Sync (Phase 7 핵심)

- [ ] Delay BPM Sync ON + Standalone 기본 120BPM + 1/4박 → 500ms 딜레이 에코 청취 (수동 확인 필요)
- [ ] BPM Sync ON + 1/8박 → 250ms 딜레이 청취 (4분의1박 에코보다 빠름) (수동 확인 필요)
- [ ] BPM Sync OFF → delay_time 노브 직접 조정 → 노브 값대로 딜레이 시간 변화 (수동 확인 필요)
- [ ] BPM Sync ON/OFF 전환 시 갑작스러운 팝/클릭 노이즈 없음 (수동 확인 필요)

---

## 기본 신호 체인 동작 (이전 Phase 연속성 확인)

- [ ] 앰프 모델 5종 전환 → 노브 레이아웃 변경, 음색 변화 확인 (수동 확인 필요)
- [ ] GraphicEQ FLAT 버튼 → 전 밴드 0dB 복귀 (수동 확인 필요)
- [ ] Chorus Mix 100% → 모듈레이션 확인 (수동 확인 필요)
- [ ] Delay 500ms → 에코 청취 (수동 확인 필요)
- [ ] Reverb Room → 공간감 확인 (수동 확인 필요)
- [ ] Bi-Amp ON + Crossover 200Hz → 저음/고음 분리 청취 (수동 확인 필요)
- [ ] DI Blend 0% → 클린 DI만 / 100% → 앰프 처리음만 (수동 확인 필요)

---

## 프리셋

- [ ] 팩토리 프리셋 로드 → 파라미터 값 반영 확인 (수동 확인 필요)
- [ ] 커스텀 프리셋 저장 → 앱 재시작 → 복원 확인 (수동 확인 필요)
- [ ] A/B 슬롯 전환 → 즉시 음색 변화 (수동 확인 필요)

---

## UI / 레이아웃

- [ ] 창 리사이즈 → 레이아웃 비율 유지 (수동 확인 필요)
- [ ] VUMeter 바 움직임 / 클립 LED 점등 (0dBFS 입력 시) (수동 확인 필요)
- [ ] Master Volume 0 → 완전 무음 (수동 확인 필요)

---

## Standalone 전용

- [ ] SettingsPage: 드라이버/장치/SR/버퍼 선택 가능 (수동 확인 필요)
- [ ] 설정 저장 → 재시작 후 자동 복원 (수동 확인 필요)
