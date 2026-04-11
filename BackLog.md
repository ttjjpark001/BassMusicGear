# BackLog.md

BassMusicGear 구현 백로그.
완료된 Phase들 중 아직 구현되지 않은 항목을 관리한다.
미래 Phase의 항목은 포함하지 않는다.

**마지막 갱신**: 2026-04-10
**기준 Phase**: Phase 0 ~ Phase 8 (완료 기준)
**총 미구현 항목**: 6건 (미구현 0건 / 부분 구현 3건 / 확인 필요 3건)

---

## 미구현 항목

### Phase 2 — 전체 앰프 모델

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 부분 구현 | 앰프 모델별 실제 캐비닛 IR 연결 | Phase 2 신규 | Phase 10 (릴리즈 준비) | AmpModelLibrary.cpp에 "임시 IR" 주석 2건(Italian Clean, Origin Pure) 명시됨. ir_1x15_vintage_wav를 임시로 사용 중. Phase 10에서 실제 IR WAV 취득 후 defaultIRName 교체 필요. |
| 부분 구현 | PowerAmp 곡선 계수 미세 튜닝 | Phase 7 P1 이월 | Phase 10 (릴리즈 준비) | PowerAmp.cpp에 Tube6550/TubeEL34/SolidState/ClassD 4종 분기가 구현됐으나, 실제 측정 데이터 기반 계수 최적화가 남아 있다. 현재 tanh 기반 근사값 사용. |
| 확인 필요 | 앰프 모델별 UI 색상 테마 전체 적용 | P1 이월 (-> Phase 9) | Phase 9 | AmpPanel::paint()에서 themeColour를 섹션 라벨 텍스트 색상에 적용하도록 Phase 6에서 부분 구현됨. 창 배경, 노브 테두리, 탭 강조색 등 전체 LookAndFeel 레벨의 테마 적용은 Phase 9 UI 완성 단계에서 처리 예정. |

---

### Phase 3 — 튜너 + 컴프레서

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 확인 필요 | Compressor 게인 리덕션 VUMeter 연동 | Phase 3 P0 부분 / Phase 7 P1 이월 | Phase 9 | Compressor.cpp에서 gainReductionDb를 atomic으로 저장하고 getGainReductionDb() API가 준비됐으나, Source/UI/VUMeter.h/.cpp 파일이 존재하지 않음. Phase 9에서 VUMeter 구현 시 연동 완성. |
| 부분 구현 | 튜너 참조 주파수 UI (tuner_reference_a) | Phase 3 P0 부분 | Phase 9 | APVTS 파라미터 등록 완료(430~450Hz). Tuner.cpp에서 DSP 적용 중. TunerDisplay에 슬라이더/스피너 UI가 없어 사용자가 조작 불가. Phase 9 UI 완성 단계에서 TunerDisplay에 노브/스피너 추가 필요. |

---

### Phase 6 — Bi-Amp + DI Blend

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 확인 필요 | Cabinet.h 주석 잔류 ("향후 Phase: 여러 캐비닛 모델 추가 예정") | 문서 정리 | Phase 10 | Cabinet.h 18번째 줄에 "향후 Phase: 여러 캐비닛 모델(Fender, Ampeg, Eden 등) 추가 예정" 주석이 남아 있음. 기능 미구현 자체보다는 문서 일관성 차원에서 Phase 10 실제 IR 교체 시 함께 정리 필요. |

---

## TODO / FIXME 코드 주석

소스 코드에서 발견된 미해결 주석 목록.

| 파일 | 내용 |
|------|------|
| Source/DSP/Cabinet.cpp (12, 76번째 줄) | `placeholder IR` 주석 — 실제 캐비닛 IR로 교체 전까지 유지. Phase 10 대상. |
| Source/Models/AmpModelLibrary.cpp (98, 116번째 줄) | `임시 IR (중립적 선택)` 주석 — Italian Clean / Origin Pure 앰프의 기본 IR이 임시값. Phase 10 대상. |

---

## 완료 처리 이력

이 섹션은 백로그에서 제거된 항목을 추적한다.

| 항목 | 원래 Phase | 구현된 Phase | 제거 시각 |
|------|-----------|------------|---------|
| 커스텀 IR 파일 로드 (Cabinet::loadIR(File)) | Phase 1 P1 이월 (-> Phase 6) | Phase 1 | 2026-03-22 |
| PowerAmp Sag 시뮬레이션 | Phase 1 P1 이월 (-> Phase 2 CARRY) | Phase 2 | 2026-03-25 |
| 신호 체인 순서 복원 (ToneStack) | Phase 1 임시 수정 (-> Phase 2 복원) | Phase 2 | 2026-03-25 |
| Overdrive 타입 선택 UI (od_type) | Phase 4 구현 누락 | Phase 5 | 2026-04-05 |
| EnvelopeFilter ef_direction / ef_freq_min / ef_freq_max UI 노출 | Phase 4 이월 (-> Phase 5 CARRY) | Phase 5 | 2026-04-05 |
| Octaver Oct-Up 음질 개선 | Phase 4 P1 이월 (-> Phase 5 CARRY) | Phase 5 | 2026-04-05 |
| Amp Voicing 필터 부재 (5종 앰프 음색 차별화) | Phase 2 설계 누락 (-> Phase 6 CARRY) | Phase 6 | 2026-04-08 |
| NoiseGate EffectBlock UI | Phase 1 누락 (-> Phase 7) | Phase 7 | 2026-04-09 |
| PowerAmp 앰프별 포화 차별화 (Tube6550/TubeEL34/SolidState/ClassD) | Phase 2 P1 이월 (-> Phase 7) | Phase 7 | 2026-04-09 |
| Compressor EffectBlock UI | Phase 3 누락 (-> Phase 7) | Phase 7 | 2026-04-09 |
| Delay BPM Sync | Phase 5 P1 이월 (-> Phase 7) | Phase 7 | 2026-04-09 |
| NoiseGateTest.cpp 단위 테스트 | Phase 1 누락 (-> Phase 8 CARRY) | Phase 8 | 2026-04-10 |
| PreampTest.cpp 단위 테스트 | Phase 1 누락 (-> Phase 8 CARRY) | Phase 8 | 2026-04-10 |
| TunerTest.cpp 단위 테스트 | Phase 3 누락 (-> Phase 8 CARRY) | Phase 8 | 2026-04-10 |
| PresetTest.cpp 단위 테스트 | Phase 8 P0 | Phase 8 | 2026-04-10 |
| InputPadTest.cpp 단위 테스트 | Phase 8 P0 신규 | Phase 8 | 2026-04-10 |
| Active/Passive 입력 패드 (input_active APVTS, -10dB 감쇄) | Phase 8 P0 신규 | Phase 8 | 2026-04-10 |
| PresetManager (ValueTree 직렬화/역직렬화, Export/Import) | Phase 8 P0 | Phase 8 | 2026-04-10 |
| A/B 슬롯 (saveToSlot/loadFromSlot/clearSlot) | Phase 8 P0 | Phase 8 | 2026-04-10 |
| 팩토리 프리셋 XML 15종 | Phase 8 P0 | Phase 8 | 2026-04-10 |
| PresetPanel UI (Save/Delete/Export/Import/A/B 버튼) | Phase 8 P0 | Phase 8 | 2026-04-10 |
| EQ 사용자 프리셋 (GraphicEQPanel 드롭다운 저장/불러오기/삭제) | Phase 8 P0 | Phase 8 | 2026-04-10 |
