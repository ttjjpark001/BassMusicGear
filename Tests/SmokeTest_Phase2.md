# 스모크 테스트 체크리스트 — Phase 2

Phase 2 목표: 5종 앰프 모델 전체 구현, 모델 전환 UI 동작.

자동화 실행 결과:
- Standalone 프로세스 기동 확인: PASS (4초 실행 후 크래시 없음)
- Standalone 경로: `build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

아래 항목은 Standalone을 직접 실행하여 확인한다.

---

## 기본 동작

- [x] Standalone 실행 → 창 표시, 크래시 없음 (Phase 3 테스트 중 확인 완료)
- [x] 오디오 인터페이스 입력 → 처리된 소리 출력 (Phase 3 튜너 테스트 중 확인 완료)
- [x] 앱 종료 → 크래시 없음 (Phase 3 테스트 중 확인 완료)

---

## 앰프 모델 전환

- [x] 5종 모델 선택 ComboBox 표시 확인 (확인 완료)
- [x] American Vintage(Baxandall) 선택 → Bass/Mid/Treble/Mid Position 노브 표시 (확인 완료)
- [x] Tweed Bass(TMB) 선택 → Bass/Mid/Treble 노브 표시, 상호작용형 특성 (확인 완료)
- [x] British Stack(James) 선택 → Bass/Mid/Treble 노브 표시, 독립 셸빙 특성 (확인 완료)
- [x] Modern Micro(BaxandallGrunt) 선택 → Bass/Mid/Treble/Grunt/Attack 노브 표시 (확인 완료)
- [x] Italian Clean(MarkbassFourBand) 선택 → Bass/Mid/Treble/VPF/VLE 노브 표시 (확인 완료)
- [x] 모델 전환 시 노브 레이아웃이 즉시 변경됨 (확인 완료)

---

## 각 모델의 청각적 특성

> ⚠️ **Phase 9 이후 재확인 필요**: 현재 모든 캐비닛 IR이 placeholder(동일한 더미 파일)로 구성되어 있어
> 앰프 모델 간 음색 차이가 IR에 의해 희석된다. 실제 IR 파일 연결(Phase 9) 완료 후 아래 항목을 다시 점검한다.

- [ ] American Vintage: Mid Position 스위치 5단계 전환 시 중역 중심 주파수 변화 청취 (Phase 9 이후 재확인)
- [ ] British Stack: Bass 노브 최대/최소 변경 시 고역(8kHz 이상)이 거의 변하지 않음 (Phase 9 이후 재확인)
  - 단위 테스트로 검증됨: Bass 변경 시 8kHz 변화량 2dB 이내
- [ ] Modern Micro: Grunt 최대 → 저중역(250Hz) 부스트 청취 (Phase 9 이후 재확인)
- [ ] Italian Clean VPF 최대 → 380Hz 미드 스쿱(노치), 35Hz 서브 부스트, 10kHz 고음 부스트 확인 (Phase 9 이후 재확인)
  - 단위 테스트로 검증됨: 1kHz 대비 380Hz 감쇠 6dB 이상
- [ ] Italian Clean VLE 최대 → 고역 롤오프 확인 (소리가 어두워짐, 8kHz 크게 감쇠) (Phase 9 이후 재확인)
  - 단위 테스트로 검증됨: 500Hz 대비 8kHz 감쇠 12dB 이상

---

## Preamp 모델별 특성

> ⚠️ **Phase 9 이후 재확인 필요**: placeholder IR로는 앰프별 캐비닛 특성이 동일하게 나타나 Preamp 특성을
> 청각적으로 구분하기 어렵다. 실제 IR 파일 연결(Phase 9) 완료 후 아래 항목을 다시 점검한다.

- [ ] American Vintage/Tweed Bass/British Stack: Tube12AX7 프리앰프 — 드라이브 시 짝수 고조파 왜곡(따뜻한 톤) 청취 (Phase 9 이후 재확인)
  - 단위 테스트로 검증됨: JFET 출력 신호 무결성(NaN/Inf 없음, RMS 유효 범위)
- [ ] Modern Micro: JFET 병렬 구조 — 클린 톤 유지하면서 드라이브 시 미세한 그릿 추가 청취 (Phase 9 이후 재확인)
  - 단위 테스트로 검증됨: JFET 출력 신호 무결성(NaN/Inf 없음, RMS 유효 범위)
- [ ] Italian Clean: ClassDLinear — 드라이브를 올려도 왜곡 없는 클린 톤 유지 (Phase 9 이후 재확인)
  - 단위 테스트로 검증됨: 선형성 비율 2:1 ±10%

---

## Cabinet IR 선택

- [x] CabinetSelector: 내장 IR 목록 표시 (4x10 JBL, 1x15 Vintage, 2x12 British, 2x10 Modern 등) (확인 완료)
- [ ] ⚠️ IR 전환 시 음색 변화 청취 가능 (Phase 9 작업 후 확인 필요 — 현재 모든 IR이 placeholder로 음색 차이 없음)
- [x] Cabinet bypass 토글 ON → OFF 전환 시 음색 변화 (확인 완료)

---

## 신호 체인 안정성

- [x] 모델 전환 중 팝/클릭 노이즈 없음 (확인 완료)
- [x] 노브 급격한 변경 시 노이즈 없음 (확인 완료)
- [x] 연속 재생 중 10분 이상 크래시 없음 (확인 완료)

---

## 자동화된 단위 테스트 결과 (참고)

Phase 2 단위 테스트 전체 통과 확인:

| 테스트 케이스 | 태그 | 결과 |
|---|---|---|
| ToneStack ItalianClean VPF max: 380Hz notch -6dB or more relative to 1kHz | [markbass][vpf][phase2] | PASS |
| ToneStack ItalianClean VLE max: 8kHz rolloff -12dB or more relative to 500Hz | [markbass][vle][phase2] | PASS |
| ToneStack James: Bass change does not affect 8kHz (independence) | [james][phase2] | PASS |
| ToneStack Baxandall: Bass boost at 100Hz | [baxandall][phase2] | PASS |
| Preamp JFET: model switch produces non-zero bounded output | [jfet][phase2] | PASS |
| Preamp JFET: hard drive input is bounded by nonlinear saturation | [jfet][phase2] | PASS |
| Preamp JFET: clean path contribution is audible at low drive | [jfet][phase2] | PASS |
| Preamp: ClassDLinear (Italian Clean) is truly linear — no saturation | [classd][phase2] | PASS |

전체 단위 테스트: 26개 케이스, 50개 assertion 모두 통과.
