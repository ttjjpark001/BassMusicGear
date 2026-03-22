# 스모크 테스트 체크리스트 — Phase 1

Phase 1 마일스톤: 베이스 신호가 Gate → Preamp → ToneStack → PowerAmp → Cabinet 순으로 처리되어 출력된다.

자동화 불가능한 항목은 Standalone 앱을 실행해 직접 확인한다.
체크 완료 시 `[ ]` → `[x]`로 변경한다.

실행 파일 경로:
`E:/Vibe Coding/Claude Code/BassMusicGear/build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

---

## 기본 실행

- [x] Standalone 실행 → 3초 이상 유지 후 정상 종료, 크래시 없음 (자동 확인 완료)
- [ ] Standalone 실행 → 창 표시 확인 (수동 확인 필요 — 헤드리스 환경에서 창 렌더링 불확인)
- [ ] 앱 정상 종료 → 크래시 없음 (수동 확인 필요)

---

## 오디오 입출력

- [ ] 오디오 인터페이스 연결 → 입력 신호 감지 (수동 확인 필요)
- [ ] 베이스 기타 연주 → 처리된 소리 출력 (수동 확인 필요)
- [ ] 무입력 상태 → 노이즈 게이트 동작, 무음 또는 매우 작은 출력 (수동 확인 필요)

---

## 노이즈 게이트

- [ ] NoiseGate Threshold 최대 → 베이스 연주 시 소리 차단 (수동 확인 필요)
- [ ] NoiseGate Threshold 최소 → 베이스 연주 시 소리 통과 (수동 확인 필요)

---

## 프리앰프 (Tweed Bass 모델)

- [ ] Input Gain 노브 최소 → 아주 작은 출력 또는 무음 (수동 확인 필요)
- [ ] Input Gain 노브 최대 → 강한 드라이브(포화) 소리 출력 (수동 확인 필요)
- [ ] Volume 노브 조절 → 출력 레벨 변화 확인 (수동 확인 필요)

---

## 톤스택 (Fender TMB)

- [ ] Bass 노브 최대 → 저역 증가 청취 확인 (수동 확인 필요)
- [ ] Treble 노브 최대 → 고역 증가 청취 확인 (수동 확인 필요)
- [ ] Mid 노브 최소 → 미드 스쿱 청취 확인 (수동 확인 필요)
- [ ] 세 노브 중간(0.5) → 전대역 비교적 균일한 응답 (수동 확인 필요)

---

## 파워앰프

- [ ] Drive 노브 증가 → 포화감(새추레이션) 증가 청취 (수동 확인 필요)
- [ ] Presence 노브 증가 → 고역 공기감 밝기 증가 청취 (수동 확인 필요)

---

## 캐비닛 시뮬레이션

- [ ] Cabinet 바이패스 OFF → 캐비닛 IR 컬러링 포함 소리 출력 (수동 확인 필요)
- [ ] Cabinet 바이패스 ON → 캐비닛 없이 더 평탄한 DI 소리 출력 (수동 확인 필요)

---

## 자동화 완료 항목 (단위 테스트로 검증됨)

아래 항목은 `ctest`로 자동 검증된다. 수동 확인 불필요.

- [x] ToneStack TMB: prepare() 크래시 없음
- [x] ToneStack TMB: Bass=0.5/Mid=0.5/Treble=0.5 → 100Hz~5kHz ±3dB 이내
- [x] ToneStack TMB: Bass=1.0 시 100Hz 게인이 Bass=0.5 대비 증가
- [x] ToneStack TMB: Treble=1.0 시 5kHz 게인이 Treble=0.5 대비 증가
- [x] ToneStack TMB: 극단값(0.0/1.0 조합)에서 NaN/Inf 없음
- [x] Preamp: prepare() 크래시 없음
- [x] Preamp: 4x 오버샘플링 후 앨리어싱 성분 -60dBFS 이하 (또는 기음 대비 -60dB 이하)
- [x] Preamp: 출력이 0이 아닌 유효 신호
- [x] Preamp: 강한 드라이브에서 NaN/Inf 없음
- [x] Preamp: tanh 소프트 클리핑으로 출력 진폭 제한

---

## 참고사항

- Standalone 실행 파일이 없으면 먼저 빌드한다:
  ```bash
  cmake --build build --target BassMusicGear_Standalone --config Release
  ```
- 단위 테스트 실행:
  ```bash
  cmake --build build --target BassMusicGear_Tests --config Release
  ctest --test-dir build --config Release --output-on-failure
  ```
