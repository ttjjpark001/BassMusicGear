# 스모크 테스트 체크리스트 — Phase 3 (튜너 + 컴프레서)

실행 파일: `build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

자동화 가능 항목은 ctest로 검증 완료. 아래 항목은 오디오 입출력 및 UI 상호작용이
필요하므로 Standalone 앱을 직접 실행하여 수동 확인한다.

---

## 자동화 검증 결과 (ctest)

| 테스트 | 결과 |
|--------|------|
| Standalone 프로세스 기동 후 4초간 크래시 없음 | PASS (자동) |
| 컴프레서 Ratio 8:1 + Threshold -20dBFS 게인 리덕션 발생 | PASS (단위 테스트) |
| 컴프레서 DryBlend=1.0 → bypass (원본 신호 보존) | PASS (단위 테스트) |
| 컴프레서 Ratio 20:1 하드 리밋 → 출력이 입력보다 낮아짐 | PASS (단위 테스트) |

---

## 수동 확인 항목

### 기본 동작
- [x] Standalone 실행 → 창 표시, 크래시 없음 (확인 완료)
- [x] 오디오 인터페이스 베이스 입력 → 처리된 소리 출력 (확인 완료)
- [x] 앱 종료 → 크래시 없음 (확인 완료)

### 튜너 (YIN 피치 트래킹)
- [x] E1(41Hz) 연주 → 튜너 디스플레이에 "E" 음이름 표시 (버퍼 확장 후 확인 완료)
- [x] E1 연주 중 센트 편차 바가 움직임 (확인 완료 — EMA 스무딩 적용으로 안정화)
- [x] 다른 음(A, D, G) 연주 → 각각 "A", "D", "G" 표시 (확인 완료)
- [x] 오픈 현 5개(B0/E1/A1/D2/G2) 연주 → 각 음이름 정확히 표시 (5현 베이스로 확인 완료)
- [x] Mute ON 버튼 클릭 → 오디오 출력이 무음으로 전환 (확인 완료)
- [x] Mute OFF 버튼 클릭 → 오디오 출력 복귀 (확인 완료)
- [ ] ⚠️ 참조 주파수 A=445Hz로 변경 → 센트 편차 수치 변화 확인 (Phase 9 이후 확인 필요 — 현재 참조 주파수 UI 없음, 고정 440Hz)
- [x] 무음 상태에서 튜너 디스플레이 상태 (음이름 표시 안정적이거나 "--" 표시) (확인 완료)

### 컴프레서
> Phase 7에서 Compressor EffectBlock UI(Threshold/Ratio/Attack/Release/Makeup/DryBlend 노브 + enabled 토글) 구현 완료.

- [x] Compressor 블록 ON → Ratio 8:1, Threshold -20dBFS 설정 후 베이스 연주 시 피크 레벨 감소 확인 (Phase 7에서 확인 완료)
- [ ] ⚠️ VUMeter 또는 게인 리덕션 표시가 압축량 반영 (Phase 9 이후 확인 필요 — VUMeter 미구현)
- [x] Compressor DryBlend 0% → 완전 압축음 (wet only) (Phase 7에서 확인 완료)
- [x] Compressor DryBlend 100% → 원본 신호 (dry only, 컴프레서 없는 소리) (Phase 7에서 확인 완료)
- [x] Compressor DryBlend 50% → 원본과 압축음의 혼합 (Phase 7에서 확인 완료)
- [x] Compressor OFF (bypass) → DryBlend 값에 무관하게 원본 신호 통과 (Phase 7에서 확인 완료)
- [x] Attack 1ms → 빠른 피크 억제, Attack 100ms → 어택 트랜지언트 통과 후 압축 (Phase 7에서 확인 완료)
- [x] MakeupGain +6dB → 압축 후 레벨 증가 확인 (Phase 7에서 확인 완료)

### 신호 체인 전체 동작
- [x] NoiseGate Threshold 최대 → 완전 무음 (확인 완료)
- [x] NoiseGate OFF → 신호 통과 (확인 완료)
- [ ] ⚠️ 5종 앰프 모델 전환 → 각 모델별 음색 차이 확인 (Phase 9 이후 확인 필요 — 현재 모든 IR이 placeholder로 모델 간 음색 차이 없음)

### UI / 레이아웃
- [ ] ⚠️ 창 리사이즈 → 레이아웃 비율 유지 (Phase 9 이후 확인 필요 — 현재 리사이즈 미지원, 최소화/최대화만 동작)
- [x] 컴프레서 파라미터 노브 우클릭 → 기본값 리셋 동작 (확인 완료)

---

## 테스트 실행 환경

- 플랫폼: Windows 10
- 빌드 구성: Release
- 테스트 실행일: (실행 시 기입)
- 테스터: (이름 기입)
- 오디오 인터페이스: (장치명 기입)
- 입력 채널: (채널 번호 기입)
