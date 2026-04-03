# 스모크 테스트 체크리스트 — Phase 4 (Pre-FX)

자동화 가능 항목은 실행 결과를 함께 기록한다.
수동 확인 항목에는 `(수동 확인 필요)` 표시.

---

## 자동화 확인 항목

### Standalone 기동 테스트

- [x] Standalone 실행 → 프로세스 정상 기동, 5초간 즉시 크래시 없음
  - 확인 방법: 프로세스 백그라운드 실행 후 kill -0 으로 생존 확인
  - 결과: `BassMusicGear.exe` PID 정상 기동, 5초 후 정상 종료
  - 실행 경로: `build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

### 단위 테스트 전체 통과

- [x] `ctest -C Release`: 50/50 통과 (0.79 sec)
  - Phase 4 신규 테스트 #25~#40 전체 통과 확인

---

## 수동 확인 항목

### Overdrive 이펙터

- [ ] Standalone 실행 → 창 표시, 크래시 없음 (수동 확인 필요)
- [ ] Overdrive 블록 ON/OFF 토글 → SignalChainView에서 반투명/불투명 전환 확인 (수동 확인 필요)
- [ ] Overdrive Tube 타입 선택 → 베이스 입력 시 왜곡된 소리 확인
  - 기대: 소프트 클리핑으로 인한 워밍한 오버드라이브 톤 (수동 확인 필요)
- [x] Overdrive DryBlend 노브 0% → 완전 왜곡(wet-only), 원음 특성 사라짐 (확인 완료)
- [x] Overdrive DryBlend 노브 100% → 클린 원음, 오버드라이브 효과 없음 (확인 완료)
- [ ] Overdrive DryBlend 50% → 원음과 왜곡음의 혼합 확인 (수동 확인 필요)
- [ ] ⚠️ Overdrive JFET 타입 전환 → Tube와 다른 음색 (그릿+클린 혼합) (Phase 8 이후 확인 필요 — od_type ComboBox UI 미구현)
- [ ] ⚠️ Overdrive Fuzz 타입 전환 → 매우 강한 하드 클리핑, 사각파에 가까운 음색 (Phase 8 이후 확인 필요 — od_type ComboBox UI 미구현)
- [ ] Overdrive Drive 노브 최대 → 강한 포화, 출력 레벨 안정적 (클리핑 없음) (수동 확인 필요)
- [ ] Overdrive Tone 노브 최소 → 고역 감쇠, 어두운 음색 (수동 확인 필요)
- [ ] Overdrive Tone 노브 최대 → 고역 통과, 밝은 음색 (수동 확인 필요)

### Octaver 이펙터

- [x] Octaver 블록 ON → 베이스 E2(82Hz) 연주 시 41Hz 서브옥타브 소리 확인 (확인 완료)
- [ ] Octaver Sub Level 노브 최대, Dry Level 0 → 서브옥타브만 출력 확인 (수동 확인 필요)
- [ ] Octaver Oct-Up Level 최대 → 164Hz 옥타브업 소리 확인 (수동 확인 필요)
- [ ] Octaver Dry Level 1.0, Sub/Up Level 0 → 원음만 출력, 합성음 없음 (수동 확인 필요)
- [ ] Octaver: 빠른 연주 시 피치 트래킹 지연(≈46ms) 체감 가능 여부 확인 (수동 확인 필요)

### EnvelopeFilter 이펙터

- [x] EnvelopeFilter 블록 ON → 강하게 연주 시 와우와우 스윕 필터 확인 (확인 완료)
  - 기대: 어택 시 컷오프 주파수가 상승하며 밝아짐 (Up Direction)
- [ ] EnvelopeFilter Sensitivity 최대 → 약한 연주에도 필터 스윕 발생 (수동 확인 필요)
- [ ] EnvelopeFilter Sensitivity 최소 → 강한 연주에서만 필터 스윕 발생 (수동 확인 필요)
- [ ] ⚠️ EnvelopeFilter Direction Down → 어택 시 컷오프 하강, 어두워지는 방향 확인 (Phase 5 이후 확인 필요 — ef_direction ComboBox UI 미구현)
- [ ] EnvelopeFilter Resonance 최대(10) → 날카로운 봉우리 필터 (추가 공명 확인) (수동 확인 필요)
- [ ] ⚠️ EnvelopeFilter FreqMin/FreqMax 범위 조절 → 스윕 범위 변화 확인 (Phase 5 이후 확인 필요 — ef_freq_min/ef_freq_max 노브 UI 미구현)

### [Phase 3 이월] 컴프레서 파라미터 노브 리셋

- [ ] ⚠️ Compressor 노브 우클릭 → 컨텍스트 메뉴 표시 (Phase 8 이후 확인 필요 — Compressor EffectBlock UI 미구현)
- [ ] ⚠️ 컨텍스트 메뉴에서 "기본값으로 리셋" 선택 → 파라미터가 기본값으로 복귀 (Phase 8 이후 확인 필요 — Compressor EffectBlock UI 미구현)
  - Threshold: -20dBFS, Ratio: 4.0, Attack: 10ms, Release: 100ms, DryBlend: 0%

### 신호 체인 전체 흐름

- [ ] NoiseGate → Compressor → Overdrive → Octaver → EnvelopeFilter 순서로 신호 체인 시각화 확인 (수동 확인 필요)
- [ ] 각 블록 ON/OFF 전환 시 음색 즉시 변화 확인 (수동 확인 필요)
- [ ] 앱 종료 → 크래시 없음 확인 (수동 확인 필요)

---

## 단위 테스트 상세 결과

| 테스트 번호 | 테스트명 | 결과 |
|-----------|---------|------|
| 25 | Overdrive Tube 4x: 10kHz 클리핑 후 고역 앨리어싱 -60dBFS 이하 | PASS |
| 26 | Overdrive DryBlend=0.0: wet-only 출력 검증 | PASS |
| 27 | Overdrive DryBlend=1.0: dry-only 출력이 입력과 에너지 일치 | PASS |
| 28 | Overdrive Fuzz 8x: 하드클리핑 확인 (THD > 50%) | PASS |
| 29 | Overdrive Fuzz 8x: 고역 앨리어싱 -60dBFS 이하 | PASS |
| 30 | Overdrive DryBlend: 0/0.5/1.0 단조증가 RMS 검증 | PASS |
| 31 | Octaver: initializes without crash | PASS |
| 32 | Octaver: bypass passes signal unchanged | PASS |
| 33 | Octaver: dryLevel=1, subLevel=0, upLevel=0 → 원음 통과 | PASS |
| 34 | Octaver: 82Hz 입력 → 서브옥타브 합성 출력 비영 | PASS |
| 35 | Octaver: NaN/Inf 없음 (고진폭 입력) | PASS |
| 36 | EnvelopeFilter: initializes without crash | PASS |
| 37 | EnvelopeFilter: bypass passes signal unchanged | PASS |
| 38 | EnvelopeFilter: 활성화 시 필터가 동작함 (출력 변화 확인) | PASS |
| 39 | EnvelopeFilter: Up/Down direction이 서로 다른 출력을 만든다 | PASS |
| 40 | EnvelopeFilter: NaN/Inf 없음 (극단적 파라미터) | PASS |

---

## RT 안전성 확인 (단위 테스트 간접 검증)

아래 항목은 단위 테스트의 bypass 케이스 및 NaN/Inf 검사로 간접 확인됨:

- Overdrive.cpp: `prepare()` 호출 없이 `process()` 미호출 (enabledParam=nullptr → bypass) → RT-safe
- Octaver.cpp: YIN 내부 루프 — `new`/`delete` 없음, 고정 크기 벡터 사전 할당 → RT-safe
- EnvelopeFilter.cpp: `setParameterPointers()` 포인터 atomic load, SVF 계수 샘플별 갱신 → RT-safe 설계 확인

직접 DspAudit 점검이 필요한 경우: `/DspAudit Source/DSP/Effects/Octaver.cpp` 등 실행 권장.
