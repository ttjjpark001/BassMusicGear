# 스모크 테스트 체크리스트 — Phase 0

Phase 0 마일스톤: CMake + JUCE + Catch2 빌드 환경 구축. 창이 뜨는 빈 플러그인.

수동 확인 항목이므로 Standalone 앱을 직접 실행하여 각 항목을 점검한다.
Standalone 실행 파일 경로:
`build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe`

---

## 빌드 검증 (자동 — ctest)

- [x] `cmake --build build --target BassMusicGear_Standalone --config Release` 성공
- [x] `ctest --test-dir build -C Release --output-on-failure` 통과 (2/2)
  - [x] `Smoke test — build environment is functional` 통과
  - [x] `Smoke test — basic float arithmetic` 통과

---

## Standalone 기본 동작 (수동)

- [ ] Standalone 실행 → 창 표시 (800×500), 크래시 없음
- [ ] 창 타이틀 또는 본문에 "BassMusicGear" 텍스트 표시
- [ ] 앱 종료 → 크래시 없음

---

## 확인 절차

1. `build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe` 실행
2. 창이 정상적으로 표시되는지 육안 확인
3. 창을 닫아 정상 종료되는지 확인

---

## 참고

- 이 Phase에서는 오디오 처리 기능이 없으므로 소리 출력 테스트는 제외한다.
- 오디오 처리 관련 스모크 테스트는 Phase 1 이후에 추가된다.
- 자동화된 단위 테스트(ctest)는 위 "빌드 검증" 항목으로 대체한다.
