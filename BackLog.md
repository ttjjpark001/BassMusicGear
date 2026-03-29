# BackLog.md

BassMusicGear 구현 백로그.
완료된 Phase들 중 아직 구현되지 않은 항목을 관리한다.
미래 Phase의 항목은 포함하지 않는다.

**마지막 갱신**: 2026-03-29
**기준 Phase**: Phase 0 ~ Phase 3 (완료 기준)
**총 미구현 항목**: 5건 (미구현 1건 / 부분 구현 2건 / 확인 필요 2건)

> 📌 **Amp Voicing 처리 Phase 결정됨**: Phase 6에서 구현. PLAN.md Phase 6 ✅ CARRY 항목으로 등록 완료.

---

## 미구현 항목

### 설계 결함 — 앰프 음색 차별화 부재

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 🔴 미구현 | Amp Voicing 필터 부재 | Phase 2 설계 누락 | **Phase 6** (`AmpVoicingPlan.md` 참고, PLAN.md Phase 6 ✅ CARRY 등록 완료) | 현재 5종 앰프의 음색 차이가 거의 없음. 원인: ①Cabinet IR이 모든 앰프에 동일한 placeholder 사용 ②PowerAmp가 모든 앰프에 동일한 모듈 사용 ③앰프 고유의 고정 Voicing 필터가 없음 ④Tube12AX7 Preamp가 3종 앰프(American Vintage/Tweed Bass/British Stack)에서 동일하게 사용됨. 근본 해결은 각 앰프에 고정 Voicing 필터(AmpVoicing DSP 모듈)를 Preamp~ToneStack 사이에 추가하는 것. 세부 계획은 `AmpVoicingPlan.md` 참고. |
| 🟡 부분 구현 | PowerAmp 앰프별 차별화 미적용 | Phase 2 P1 이월 | Amp Voicing 구현 이후 2차 작업 | 현재 PowerAmp는 모든 앰프에 동일한 모듈. 실제로는 6550(Ampeg)/6L6(Fender)/EL34(Orange)/Solid-State(Darkglass)/ClassD(Markbass)로 포화 특성이 다름. Amp Voicing 구현 완료 후 후속 작업으로 진행. |

---

### Phase 2 — 전체 앰프 모델

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 🟡 부분 구현 | 앰프 모델별 실제 캐비닛 IR 연결 | Phase 2 신규 | Phase 9 (릴리즈 준비) | AmpModelLibrary.cpp 주석에 American Vintage("ir_8x10_svt_wav", "임시 placeholder IR")와 Italian Clean("ir_1x15_vintage_wav", "Italian Clean 전용 IR 미확보, 1x15 Vintage로 대체")이 임시 IR 사용 중임을 명시. CabinetSelector.cpp 주석도 "8x10 SVT (placeholder)"로 표기. Phase 9에서 무료 IR 라이브러리(Torpedo WoS, Celestion Free, OpenIR 등)에서 실제 IR WAV를 취득하여 Resources/IR/에 추가하고 AmpModelLibrary.cpp의 defaultIRName 필드와 CabinetSelector.cpp의 switch 케이스를 교체해야 함. |
| 🟢 확인 필요 | 앰프 모델별 UI 색상 테마 | P1 이월 (→ Phase 8) | Phase 8 | AmpModel.h에 themeColour 필드가 선언되어 있고 AmpModelLibrary.cpp에 5종 색상값(American Vintage 주황/Tweed 크림/British 진한주황/Modern 초록/Italian 파랑)이 등록되어 있으나, Source 전체에서 themeColour를 읽거나 LookAndFeel에 적용하는 코드가 없음. Phase 8 CARRY로 처리 예정. |

### Phase 3 — 튜너 + 컴프레서

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 🟢 확인 필요 | Compressor 게인 리덕션 VUMeter 연동 | Phase 3 P0 부분 | Phase 8 | Compressor.cpp에서 gainReductionDb를 atomic으로 저장하고 getGainReductionDb() API가 준비되어 있으나, Source/UI/VUMeter.h/.cpp 파일이 아직 존재하지 않음. Phase 8에서 VUMeter 구현 시 Compressor::getGainReductionDb()를 읽어 게인 리덕션 표시까지 완성해야 함. |

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
| PowerAmp Sag 시뮬레이션 | Phase 1 P1 이월 (-> Phase 2 CARRY) | Phase 2 | 2026-03-25 |
| 신호 체인 순서 복원 (ToneStack) | Phase 1 임시 수정 (-> Phase 2 복원) | Phase 2 | 2026-03-25 |
