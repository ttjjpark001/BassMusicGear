# BackLog.md

BassMusicGear 구현 백로그.
완료된 Phase들 중 아직 구현되지 않은 항목을 관리한다.
미래 Phase의 항목은 포함하지 않는다.

**마지막 갱신**: 2026-04-08 (Phase 6 완료 기준 + Phase 7 신규 추가)
**기준 Phase**: Phase 0 ~ Phase 6 (완료 기준)
**총 미구현 항목**: 7건 (미구현 2건 / 부분 구현 3건 / 확인 필요 2건)

> **참고**: Phase 7이 "이월 작업 정리" 단계로 신규 추가되어, 기존 Phase 7~9는 Phase 8~10으로 한 단계씩 밀렸습니다.

---

## 미구현 항목

### Phase 2 — 전체 앰프 모델

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 부분 구현 | PowerAmp 앰프별 포화 특성 차별화 | Phase 2 P1 이월 | **Phase 7 (신규)** | PowerAmp.h에 PowerAmpType 열거형(Tube6550/TubeEL34/SolidState/ClassD)이 선언되고 setPowerAmpType()으로 currentType이 저장되지만, PowerAmp::process() 내부에서 타입 분기가 없다. 모든 앰프 모델이 동일한 tanh 포화 곡선을 사용한다. Phase 7에서 4종 곡선 분기 구현 예정. |
| 부분 구현 | 앰프 모델별 실제 캐비닛 IR 연결 | Phase 2 신규 | Phase 10 (릴리즈 준비) | AmpModelLibrary.cpp 주석에 American Vintage("ir_8x10_svt_wav", "임시 placeholder IR") 등 임시 IR 사용 중임을 명시. Phase 10에서 무료 IR 라이브러리(Torpedo WoS, Celestion Free, OpenIR 등)에서 실제 IR WAV를 취득하여 Resources/IR/에 추가하고 AmpModelLibrary.cpp의 defaultIRName 필드와 CabinetSelector.cpp의 switch 케이스를 교체해야 함. |
| 확인 필요 | 앰프 모델별 UI 색상 테마 전체 적용 | P1 이월 (-> Phase 9) | Phase 9 | AmpPanel::paint()에서 themeColour를 섹션 라벨 텍스트 색상에 적용하도록 Phase 6에서 부분 구현됨. 창 배경, 노브 테두리, 탭 강조색 등 전체 LookAndFeel 레벨의 테마 적용은 Phase 9 UI 완성 단계에서 ✅ CARRY로 처리. |
| 미구현 | NoiseGate EffectBlock UI | Phase 1 누락 | **Phase 7 (신규)** | DSP는 Phase 1에서 완료(Threshold/Attack/Hold/Release/enabled APVTS 등록)됐으나 UI가 없어 사용자가 조작 불가능. Phase 7에서 EffectBlock 패턴 재사용하여 노브 4개 + enabled 토글 추가. |

---

### Phase 3 — 튜너 + 컴프레서

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 미구현 | Compressor EffectBlock UI | Phase 3 누락 | **Phase 7 (신규)** | DSP는 Phase 3에서 완료(Threshold/Ratio/Attack/Release/Makeup/DryBlend/enabled APVTS 등록)됐으나 EffectBlock UI가 없어 사용자가 조작 불가능. Phase 7에서 노브 6개 + enabled 토글 + 우클릭 리셋 동작 추가. |
| 확인 필요 | Compressor 게인 리덕션 VUMeter 연동 | Phase 3 P0 부분 | Phase 9 | Compressor.cpp에서 gainReductionDb를 atomic으로 저장하고 getGainReductionDb() API가 준비되어 있으나, Source/UI/VUMeter.h/.cpp 파일이 아직 존재하지 않음. Phase 9에서 VUMeter 구현 시 Compressor::getGainReductionDb()를 읽어 게인 리덕션 표시까지 완성해야 함. |
| 확인 필요 | 튜너 참조 주파수 UI (`tuner_reference_a`) | Phase 3 P0 미구현 | Phase 9 | Phase 3에서 APVTS 파라미터 미등록. Phase 9 UI 완성 단계의 ✅ CARRY로 처리. TunerDisplay에 430~450Hz 슬라이더/스피너 추가 예정. |

---

### Phase 5 — 그래픽 EQ + Post-FX

| 심각도 | 항목 | 원래 분류 | 처리 예정 Phase | 설명 |
|--------|------|---------|--------------|------|
| 부분 구현 | Delay BPM Sync | P1 이월 (Phase 5 -> Phase 6 -> Phase 7) | **Phase 7 (신규)** | Delay.h 주석에 "BPM 싱크 추가 예정"으로 명시. 현재 delay_time 노브(ms 단위)만 존재. Phase 7에서 delay_bpm_sync (bool) + delay_note_value (choice) APVTS 추가, juce::AudioPlayHead로 BPM 추출, Sync ON 시 delay_time_ms 자동 계산. |

---

## TODO / FIXME 코드 주석

소스 코드에서 발견된 미해결 주석 목록.

| 파일 | 내용 |
|------|------|
| Source/DSP/Effects/Delay.h (23번째 줄 주석) | `개선 (P1): BPM 싱크 추가 예정 (Phase 7)` |

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
