# 스모크 테스트 체크리스트 — Phase 5 (그래픽 EQ + Post-FX)

자동화 가능 항목은 실행 결과를 함께 기록한다.
수동 확인 항목에는 `(수동 확인 필요)` 표시.

---

## 자동화 확인 항목

### Standalone 기동 테스트

- [x] Standalone 실행 → 프로세스 정상 기동, 5초간 즉시 크래시 없음
  - 확인 방법: 프로세스 백그라운드 실행 후 kill -0 으로 생존 확인
  - 결과: `BassMusicGear.exe` PID 748 정상 기동, 5초 후 정상 종료
  - 실행 경로: `build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

### 단위 테스트 전체 통과

- [x] `ctest -C Release`: 57/57 통과 (0.86 sec)
  - Phase 5 신규 테스트 #51~#53 전체 통과 확인
  - #51: GraphicEQ Phase5: each band +12dB boost accuracy (±0.5dB)
  - #52: GraphicEQ Phase5: all bands 0dB flat response 20Hz-20kHz (±0.5dB)
  - #53: GraphicEQ Phase5: bypass ON — output equals input numerically

---

## 수동 확인 항목

### 그래픽 EQ

- [x] Standalone 실행 → 창 표시, 크래시 없음 (확인 완료)
- [x] GraphicEQ 패널 표시 확인 — 10개 슬라이더(31/63/125/250/500/1k/2k/4k/8k/16kHz) 표시 (확인 완료)
- [x] 31Hz 슬라이더 +12dB → 서브 저역 강조 확인 (배음 풍부, 저음 두꺼워짐) (확인 완료)
- [x] 1kHz 슬라이더 +12dB → 해당 주파수 명확히 강조됨 (확인 완료)
- [x] 16kHz 슬라이더 +12dB → 공기감/존재감 향상 확인 (확인 완료)
- [x] 500Hz 슬라이더 -12dB → 흐릿한 중역 감소 확인 (확인 완료)
- [x] FLAT 버튼 클릭 → 전 밴드 슬라이더 즉시 0dB 복귀 (확인 완료)
- [x] GraphicEQ 블록 ON/OFF 전환 → SignalChainView 반투명/불투명 전환 확인 (확인 완료)
- [x] GraphicEQ ON/OFF → 소리 차이 즉시 확인 (ON 시 EQ 적용, OFF 시 원음) (확인 완료)

### Chorus (Post-FX)

- [x] Chorus 블록 ON → 모듈레이션 효과 청취 (소리가 넓어지고 퍼지는 느낌) (확인 완료)
- [x] Chorus Mix 100% → 강한 모듈레이션 효과, 원음 없이 순수 코러스 (확인 완료)
- [x] Chorus Mix 0% → 원음 통과, 코러스 효과 없음 (확인 완료)
- [x] Chorus Rate 최대 → 빠른 피치 변조 확인 (확인 완료)
- [x] Chorus Rate 최소 → 매우 느린 피치 변조 확인 (확인 완료)
- [x] Chorus Depth 최대 → 넓은 피치 스윕, 풍부한 코러스 확인 (확인 완료)

### Delay (Post-FX)

- [x] Delay 블록 ON → 에코 효과 청취 (확인 완료)
- [x] Delay Time 500ms → 에코가 0.5초 간격으로 들림 (확인 완료)
  - 기대: 베이스 노트를 뚝 끊으면 0.5초 후 동일 노트 에코 청취
- [x] Delay Feedback 최대 → 에코가 점점 쌓이며 페이드아웃 (확인 완료)
- [x] Delay Mix 0% → 에코 없이 원음만 (확인 완료)
- [x] Delay Mix 100% → 원음 없이 에코음만 (확인 완료)

### Reverb (Post-FX)

- [x] Reverb 블록 ON → 공간감/잔향 확인 (확인 완료)
- [x] Reverb Room 크기 최대 → 큰 홀 느낌, 긴 잔향 (확인 완료)
- [x] Reverb Room 크기 최소 → 짧은 잔향, 데드한 공간감 (확인 완료)
- [x] Reverb Mix 0% → 잔향 없음, 원음만 (확인 완료)

### 신호 체인 전체 흐름

- [x] Pre-FX → GraphicEQ → Chorus → Delay → Reverb 순서로 신호 체인 확인 (확인 완료)
- [x] 각 블록 ON/OFF 전환 시 음색 즉시 변화 확인 (확인 완료)
- [x] Master Volume 0 → 완전 무음 확인 (확인 완료)
- [x] 앱 종료 → 크래시 없음 확인 (확인 완료)

### [Phase 4 이월] EnvelopeFilter 미완성 항목

- [x] EnvelopeFilter Direction Down → 어택 시 컷오프 하강 확인 (확인 완료)
- [x] EnvelopeFilter FreqMin/FreqMax 범위 조절 → 스윕 범위 변화 확인 (확인 완료)

---

## 단위 테스트 상세 결과 (Phase 5 신규)

| 테스트 번호 | 테스트명 | 결과 |
|-----------|---------|------|
| 51 | GraphicEQ Phase5: each band +12dB boost accuracy (±0.5dB) | PASS |
| 52 | GraphicEQ Phase5: all bands 0dB flat response 20Hz-20kHz (±0.5dB) | PASS |
| 53 | GraphicEQ Phase5: bypass ON — output equals input numerically | PASS |
| 54 | GraphicEQ: boosting 1kHz band increases gain at 1kHz | PASS |
| 55 | GraphicEQ: cutting 250Hz band decreases gain at 250Hz | PASS |
| 56 | GraphicEQ: flat response when all bands at 0dB | PASS |
| 57 | GraphicEQ: bypass passes signal unchanged | PASS |

Phase 5 커버 범위:
- 각 밴드(31~16kHz) +12dB 설정 시 해당 주파수 +11.5~+12.5dB 확인 (테스트 #51)
- 전 밴드 0dB 시 20Hz~20kHz ±0.5dB 평탄도 확인 — 12개 주파수 포인트 (테스트 #52)
- 바이패스 ON 시 입력=출력 ±0.1dB 수치 일치 확인 (테스트 #53)

---

## RT 안전성 확인

GraphicEQ DSP 파일 RT 안전성:
- `process()` 내에서 `new`/`delete`, 파일 I/O, mutex 없음
- 파라미터는 `atomic<float>*` 포인터를 통한 락프리 읽기
- 계수 변경은 JUCE IIR ReferenceCountedObjectPtr atomic swap을 통해 안전하게 반영
- `enabledParam == nullptr`일 때 enabled=true 폴백으로 RT-safe 초기화 처리
