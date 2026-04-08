# BackLog.md

BassMusicGear 구현 백로그.
완료된 Phase들 중 아직 구현되지 않은 항목을 관리한다.
미래 Phase의 항목은 포함하지 않는다.

**마지막 갱신**: 2026-04-08 (Phase 6 완료 기준)
**기준 Phase**: Phase 0 ~ Phase 6 (완료 기준)
**총 미구현 항목**: 5건 (미구현 0건 / 부분 구현 3건 / 확인 필요 2건)

---

## 미구현 항목

### Phase 2 — 전체 앰프 모델

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 부분 구현 | PowerAmp 앰프별 포화 특성 차별화 | Phase 2 P1 이월 (BackLog 유지) | Phase 8 또는 후속 작업 | PowerAmp.h에 PowerAmpType 열거형(Tube6550/TubeEL34/SolidState/ClassD)이 선언되고 setPowerAmpType()으로 currentType이 저장되지만, PowerAmp::process() 내부에서 타입 분기가 없다. 모든 앰프 모델이 동일한 tanh 포화 곡선을 사용한다. Tube6550(부드러운 포화, 높은 헤드룸) / TubeEL34(빠른 포화, 낮은 헤드룸) / SolidState(경하드 클리핑) / ClassD(선형, 최소 왜곡)로 각각 다른 웨이브쉐이핑 곡선이 적용되어야 함. AmpVoicing으로 앰프 간 주파수 특성 차이는 확보됐으나 포화 특성 차이는 여전히 미구현 상태. |
| 부분 구현 | 앰프 모델별 실제 캐비닛 IR 연결 | Phase 2 신규 | Phase 9 (릴리즈 준비) | AmpModelLibrary.cpp 주석에 American Vintage("ir_8x10_svt_wav", "임시 placeholder IR")와 Italian Clean("ir_1x15_vintage_wav", "Italian Clean 전용 IR 미확보, 1x15 Vintage로 대체")이 임시 IR 사용 중임을 명시. CabinetSelector.cpp 주석도 "8x10 SVT (placeholder)"로 표기. Phase 9에서 무료 IR 라이브러리(Torpedo WoS, Celestion Free, OpenIR 등)에서 실제 IR WAV를 취득하여 Resources/IR/에 추가하고 AmpModelLibrary.cpp의 defaultIRName 필드와 CabinetSelector.cpp의 switch 케이스를 교체해야 함. |
| 확인 필요 | 앰프 모델별 UI 색상 테마 전체 적용 | P1 이월 (-> Phase 8) | Phase 8 | AmpPanel::paint()에서 themeColour를 섹션 라벨 텍스트 색상에 적용하도록 Phase 6에서 부분 구현됨. 그러나 창 배경, 노브 테두리, 탭 강조색 등 전체 LookAndFeel 레벨의 테마 적용은 아직 없음. Phase 8에서 VUMeter/SignalChainView 등 전체 UI 완성 시 LookAndFeel에 themeColour를 연동해야 함. Phase 8 CARRY로 처리 예정. |

---

### Phase 3 — 튜너 + 컴프레서

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 확인 필요 | Compressor 게인 리덕션 VUMeter 연동 | Phase 3 P0 부분 | Phase 8 | Compressor.cpp에서 gainReductionDb를 atomic으로 저장하고 getGainReductionDb() API가 준비되어 있으나, Source/UI/VUMeter.h/.cpp 파일이 아직 존재하지 않음. Phase 8에서 VUMeter 구현 시 Compressor::getGainReductionDb()를 읽어 게인 리덕션 표시까지 완성해야 함. |

---

### Phase 5 — 그래픽 EQ + Post-FX

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 부분 구현 | Delay BPM Sync | P1 이월 (Phase 5 -> Phase 6 -> Phase 8) | Phase 8 | Delay.h 주석에 "BPM 싱크 추가 예정 (Phase 8)"으로 명시. Phase 5에서 P1으로 이월, Phase 6 CARRY로 재등록됐으나 구현되지 않음. 현재 delay_time 노브(ms 단위)만 존재하고 BPM/박자 단위 sync 기능 없음. Phase 8에서 Delay EffectBlock에 BPM Sync 토글 및 음표 단위 선택 ComboBox를 추가해야 함. |

---

## TODO / FIXME 코드 주석

소스 코드에서 발견된 미해결 주석 목록.

| 파일 | 내용 |
|------|------|
| Source/DSP/Effects/Delay.h (23번째 줄 주석) | `개선 (P1): BPM 싱크 추가 예정 (Phase 8)` |

---

## 완료 처리 이력

이 섹션은 백로그에서 제거된 항목을 추적한다.

| 항목 | 원래 Phase | 구현된 Phase | 제거 시각 |
|------|-----------|------------|---------|
| 커스텀 IR 파일 로드 (`Cabinet::loadIR(File)`) | Phase 1 P1 이월 (-> Phase 6) | Phase 1 | 2026-03-22 |
| PowerAmp Sag 시뮬레이션 | Phase 1 P1 이월 (-> Phase 2 CARRY) | Phase 2 | 2026-03-25 |
| 신호 체인 순서 복원 (ToneStack) | Phase 1 임시 수정 (-> Phase 2 복원) | Phase 2 | 2026-03-25 |
| Overdrive 타입 선택 UI (od_type) | Phase 4 구현 누락 | Phase 5 | 2026-04-05 |
| EnvelopeFilter ef_direction / ef_freq_min / ef_freq_max UI 노출 | Phase 4 이월 (-> Phase 5 CARRY) | Phase 5 | 2026-04-05 |
| Octaver Oct-Up 음질 개선 | Phase 4 P1 이월 (-> Phase 5 CARRY) | Phase 5 | 2026-04-05 |
| Amp Voicing 필터 부재 (5종 앰프 음색 차별화) | Phase 2 설계 누락 (-> Phase 6 CARRY) | Phase 6 | 2026-04-08 |
