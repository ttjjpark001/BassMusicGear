# BackLog.md

BassMusicGear 구현 백로그.
완료된 Phase들 중 아직 구현되지 않은 항목을 관리한다.
미래 Phase의 항목은 포함하지 않는다.

**마지막 갱신**: 2026-03-22
**기준 Phase**: Phase 0 ~ Phase 1 (완료 기준)
**총 미구현 항목**: 2건 (미구현 1건 / 부분 구현 0건 / 임시 수정 복원 필요 1건)

---

## 미구현 항목

### Phase 1 — 핵심 신호 체인

| 심각도 | 항목 | 원래 분류 | 설명 |
|--------|------|---------|------|
| 미구현 | PowerAmp Sag 시뮬레이션 | P1 이월 (Phase 1 -> Phase 2 CARRY) | `PowerAmp.h` 23번째 줄에 "Phase 2 예정 기능" 주석만 존재. `PowerAmp.cpp`에 Sag 관련 파라미터·로직 없음. Phase 2에서 튜브 모델(American Vintage / Tweed Bass / British Stack)에서만 활성화하는 조건 분기 포함하여 구현 필요. |

---

## 임시 수정 — Phase 2에서 복원 필요

### [Phase1-이월] 신호 체인 순서 원위치 (ToneStack)

- **현재 상태**: `Gate -> Preamp -> PowerAmp -> ToneStack -> Cabinet`
- **원래 스펙**: `Gate -> Preamp -> ToneStack -> PowerAmp -> Cabinet`
- **임시 수정 이유**: Phase 1의 PowerAmp Drive(`pow(40,x)` + tanh)가 과도하게 공격적이어서
  ToneStack Treble 부스트를 tanh가 다시 압축, 노브 효과가 들리지 않는 문제
- **Phase 2 복원 조건**: 앰프 모델별 게인 스테이징 구현 후 Drive가 모델에 맞게 재튜닝되면
  `SignalChain.cpp`의 `process()` 순서를 원위치로 되돌린다.
- **파일**: `Source/DSP/SignalChain.cpp` — `process()` 함수 (99~113번째 줄)

---

## TODO / FIXME 코드 주석

소스 코드에서 발견된 미해결 주석 목록.

| 파일 | 줄 | 내용 |
|------|----|------|
| (없음) | — | — |

---

## 완료 처리 이력

이 섹션은 백로그에서 제거된 항목을 추적한다.

| 항목 | 원래 Phase | 구현된 Phase | 제거 시각 |
|------|-----------|------------|---------|
| 커스텀 IR 파일 로드 (`Cabinet::loadIR(File)`) | Phase 1 P1 이월 (-> Phase 6) | Phase 1 | 2026-03-22 |
