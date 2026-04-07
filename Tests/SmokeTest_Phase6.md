# 스모크 테스트 체크리스트 — Phase 6

실행 파일: `build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

자동화 검증 결과 (2026-04-07):
- Standalone 프로세스 기동 확인: PASSED (4초 실행 후 정상 종료)

---

## 기본 동작

- [ ] Standalone 실행 → 창 표시, 크래시 없음 (수동 확인 필요)
- [ ] 오디오 인터페이스 입력 → 처리된 소리 출력 (수동 확인 필요)
- [ ] 앱 종료 → 크래시 없음 (수동 확인 필요)

---

## Bi-Amp + Crossover

- [ ] Bi-Amp ON + Crossover 200Hz → 저음은 클린 DI 경로로, 고음은 앰프 처리 경로로 분리됨을 청각으로 확인 (수동 확인 필요)
  - 베이스 개방현(E1=41Hz) 연주 시 저역이 투명하게 들리고 고역에만 앰프 색감이 느껴져야 함
  - Crossover 주파수를 올리거나 내릴 때 분리 지점 변화 확인
- [ ] Bi-Amp OFF → 전대역이 앰프 체인을 통과함 (클린 DI 경로 비활성) (수동 확인 필요)
- [ ] Crossover 주파수 최소(60Hz) → 거의 전 대역이 앰프 처리 경로 (수동 확인 필요)
- [ ] Crossover 주파수 최대(500Hz) → 저역 클린 비율 증가, 음색 변화 확인 (수동 확인 필요)

---

## DI Blend + IR Position

- [ ] Blend 0% → 클린 DI만 출력 (앰프 처리음 없음, 깨끗한 베이스 직결 사운드) (수동 확인 필요)
- [ ] Blend 100% → 앰프 처리음만 출력 (DI 신호 없음) (수동 확인 필요)
- [ ] Blend 50% → 두 신호의 균형잡힌 혼합 확인 (수동 확인 필요)
- [ ] IR Position Pre ↔ Post 전환 → 서로 다른 공간감 확인 (수동 확인 필요)
  - Pre: Cabinet IR이 혼합 신호 전체에 적용 → 더 자연스러운 공간감
  - Post: Cabinet IR이 processed 경로에만 적용 → 클린 DI는 드라이하게 유지
- [ ] Clean Level +12dB → 클린 신호 레벨 증가 확인 (수동 확인 필요)
- [ ] Processed Level -12dB → 앰프 처리음 레벨 감소 확인 (수동 확인 필요)

---

## Cabinet IR

- [ ] 커스텀 WAV IR 파일 로드 → Cabinet 음색이 즉시 변경됨 (수동 확인 필요)
  - 테스트: 임의의 모노 WAV 파일(48kHz, 1초 이하)을 CabinetSelector에서 로드
  - 로드 후 베이스 연주 시 즉시 새 IR 반영 확인
  - 크래시 없이 정상 동작 확인
- [ ] 내장 IR 선택(콤보박스) → 선택한 캐비닛 특성 반영 (수동 확인 필요)

---

## 앰프 모델 Voicing 차별화 (Cabinet bypass + ToneStack flat 기준)

테스트 조건:
1. Cabinet bypass ON (Cabinet 블록 비활성)
2. 각 앰프 ToneStack의 모든 노브를 센터(12시) 위치로 설정 (flat)
3. 앰프 모델을 순차 전환하며 음색 차이 청취

- [ ] American Vintage 선택 → 저역 80Hz 따뜻함, 1.5kHz 어퍼-미드 약간 스쿱 확인 (수동 확인 필요)
- [ ] Tweed Bass 선택 → 60Hz 저역 부스트, 600Hz 미드 스쿱, 5kHz 이상 고역 롤오프 확인 (수동 확인 필요)
- [ ] British Stack 선택 → 60Hz 이하 서브베이스 타이트, 500Hz 공격적 미드 부스트 확인 (수동 확인 필요)
- [ ] Modern Micro 선택 → 80Hz 이하 타이트, 3kHz 선명한 클래리티 부스트 확인 (수동 확인 필요)
- [ ] Italian Clean 선택 → 전 대역 거의 평탄, 6kHz 미세한 선명도만 확인 (수동 확인 필요)
- [ ] Origin Pure 선택 → 노브 센터에서 가장 투명한 사운드, VPF/VLE 없음 확인 (수동 확인 필요)
  - 6종 중 가장 색감이 없어야 함
  - Baxandall ToneStack만을 통한 음색 조절 가능

---

## 앰프 모델 UI 테마

- [ ] American Vintage → 주황색 테마 (수동 확인 필요)
- [ ] Tweed Bass → 크림색 테마 (수동 확인 필요)
- [ ] British Stack → 진한 주황색 테마 (수동 확인 필요)
- [ ] Modern Micro → 초록색 테마 (수동 확인 필요)
- [ ] Italian Clean → 파란색 테마, VPF/VLE 노브 표시 확인 (수동 확인 필요)
- [ ] Origin Pure → 실버 테마 (수동 확인 필요)

---

## 신호 체인 안정성

- [ ] 모든 6종 앰프 모델을 빠르게 전환해도 크래시 없음 (수동 확인 필요)
- [ ] Bi-Amp ON/OFF 빠른 전환 → 팝 노이즈 없거나 최소화 (수동 확인 필요)
- [ ] IR Position Pre/Post 빠른 전환 → 크래시 없음 (수동 확인 필요)
- [ ] Blend 노브 실시간 드래그 → 연속적 음색 변화, 크래시 없음 (수동 확인 필요)

---

## 자동화 검증 결과 요약

| 항목 | 결과 |
|------|------|
| Standalone 프로세스 기동 (4초 실행) | PASSED |
| 단위 테스트 전체 (92/92) | PASSED |
