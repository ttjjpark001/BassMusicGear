# /Complete

주어진 Phase를 완료로 기록하고, 변경된 파일을 git add / commit / push 한다.
`/Phase` 커맨드로 자동 진행하거나, 수동으로 작업을 완료한 후 완료 처리할 때 사용한다.

## 입력

$ARGUMENTS

사용 예:
```
/Complete 0
/Complete 1
/Complete 5
```

---

## 실행 지침

아래 단계를 순서대로 수행한다.

---

### STEP 1: 입력 검증

`$ARGUMENTS`에서 phase 번호를 파싱한다.

- 유효 범위: 0~9 (정수)
- 범위를 벗어나거나 숫자가 아닌 경우 → 즉시 중단:
  ```
  ❌ 오류: Phase 번호는 0~9 사이의 정수여야 합니다.
  ```

파싱된 번호를 `N`으로 부른다.

---

### STEP 2: 메모리 확인 및 검증

메모리 파일을 읽는다:
```
파일: E:/Vibe Coding/Claude Code/BassMusicGear/memory/MEMORY.md
읽을 값: last_completed_phase
```

`last_completed_phase` 값을 `LAST`라 할 때:

| 조건 | 처리 |
|------|------|
| `N <= LAST` | 이미 완료됨 → **중단** |
| `N > LAST + 1` | 이전 Phase 미완료 → **경고 후 확인 요청** |
| `N == LAST + 1` 또는 (`N == 0` & `LAST == -1`) | 정상 → 다음 단계 진행 |

**이미 완료된 경우 메시지:**
```
❌ 중단: Phase N은 이미 완료 처리되어 있습니다.
         현재 last_completed_phase: N
```

**단계 건너뜀 경고 메시지:**
```
⚠️  경고: Phase LAST+1 이(가) 아직 완료되지 않았습니다.
    이전 Phase를 건너뛰고 Phase N을 완료로 기록하시겠습니까?
    계속하려면 다시 "/Complete N force" 로 실행하세요.
    (권장: 먼저 /Complete LAST+1 을 실행하세요.)
```

입력이 `$ARGUMENTS`에 `force`가 포함된 경우 (`/Complete N force`)에는 경고를 무시하고 진행한다.

---

### STEP 3: PLAN.md에서 Phase 요약 추출

`PLAN.md`를 읽어 Phase N의 다음 정보를 추출한다:
- **Phase 이름** (예: `핵심 신호 체인`)
- **마일스톤** (예: `베이스 소리가 처리되어 나온다`)
- **P0 구현 항목** 목록 (커밋 메시지 요약에 활용)

이 정보로 아래 형식의 커밋 메시지를 구성한다:

```
Phase N 완료: [Phase 이름] — [마일스톤 또는 P0 핵심 항목 한 줄 요약]
```

예시:
```
Phase 0 완료: 프로젝트 스켈레톤 — CMake + JUCE + Catch2 빌드 환경 구축, Standalone 창 표시
Phase 1 완료: 핵심 신호 체인 — Gate/Preamp/ToneStack(TMB)/PowerAmp/Cabinet 구현, 실제 소리 출력
Phase 2 완료: 전체 앰프 모델 — 5종 ToneStack + AmpModelLibrary + AmpPanel, 모델 전환 동작
Phase 3 완료: 튜너 + 컴프레서 — YIN 튜너 + VCA 컴프레서 + TunerDisplay 구현
Phase 4 완료: Pre-FX — Overdrive(Tube/JFET/Fuzz) + Octaver + EnvelopeFilter + EffectBlock UI
Phase 5 완료: 그래픽 EQ + Post-FX — 10밴드 Constant-Q EQ + Chorus/Delay/Reverb
Phase 6 완료: Bi-Amp + DI Blend — LR4 크로스오버 + DIBlend + IR Position 동적 라우팅
Phase 7 완료: 프리셋 시스템 — PresetManager + 팩토리 프리셋 15종 + A/B 슬롯 + PresetPanel
Phase 8 완료: UI 완성 + 출력 — VUMeter + SignalChainView + 다크 테마 + 앰프 색상 + 리사이즈
Phase 9 완료: 오디오 설정 + 릴리즈 — SettingsPage + 전체 테스트 통과 + VST3 설치 v0.1.0
```

---

### STEP 4: 메모리 갱신

메모리 파일(`MEMORY.md`)을 다음과 같이 수정한다:

1. `last_completed_phase: LAST` → `last_completed_phase: N`
2. Phase N 완료 이력 행:
   - `☐` → `✅`
   - 완료 시각: 현재 날짜와 시각 (예: `2026-03-18 21:47`)

수정 예시:
```
last_completed_phase: 1

| 1 | 핵심 신호 체인 | ✅ | 2026-03-18 21:47 |
```

---

### STEP 5: BackLogManager — 백로그 갱신

**BackLogManager 에이전트 호출:**
```
"Phase N 완료 처리가 방금 완료됐어.
 MEMORY.md의 last_completed_phase가 N으로 갱신됐으니
 BackLog.md를 최신 상태로 갱신해줘."
```

BackLogManager는 다음을 수행한다:
- Phase 0 ~ N 범위에서 미구현 항목(P1 이월, 부분 구현, TODO/FIXME)을 파악
- 기존 `BackLog.md`에서 이번 Phase 구현으로 완료된 항목 제거
- 새로 발견된 미구현 항목 추가
- `BackLog.md` 저장

**완료 확인**: BackLogManager가 보고를 반환하면 STEP 6으로 진행.
`BackLog.md`의 변경 내용도 이후 git add 대상에 포함된다.

```
✅ STEP 5 완료 — BackLog.md 갱신 (제거 N건 / 추가 M건)
```

---

### STEP 6: git add

변경된 파일을 스테이징한다.
`BackLog.md`와 `MEMORY.md`를 포함한 모든 변경 파일이 대상이다.

```bash
git add -A
```

스테이징 전 `git status`로 변경 파일 목록을 확인하고,
추가되는 파일 중 `.env`, 대용량 바이너리(`.exe`, `.dll` 등 빌드 결과물),
`build/` 디렉터리가 포함되지 않도록 확인한다.

빌드 결과물이 포함된 경우:
```bash
# build/ 디렉터리는 .gitignore로 제외되어 있어야 함
# 만약 포함됐다면 제외 후 add
git reset HEAD build/
git add -A
```

---

### STEP 7: git commit

STEP 3에서 구성한 커밋 메시지로 커밋한다.

```bash
git commit -m "$(cat <<'EOF'
Phase N 완료: [Phase 이름] — [요약]

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

커밋 실패(pre-commit hook 등) 시:
- 오류 메시지를 확인하고 원인을 수정한다.
- `--no-verify`로 hook을 우회하지 않는다.
- 수정 후 재커밋한다.

---

### STEP 8: git push

```bash
git push
```

push 실패 시:

| 원인 | 처리 |
|------|------|
| upstream 미설정 | `git push -u origin main` |
| 원격 브랜치 충돌 | 사용자에게 상황 보고 후 중단 (force push 금지) |
| 네트워크 오류 | 재시도 1회, 실패 시 사용자에게 보고 |

---

### STEP 9: 완료 보고

```
╔══════════════════════════════════════════════════════╗
║         /Complete N 완료                             ║
╚══════════════════════════════════════════════════════╝

Phase N: [Phase 이름]
마일스톤: [마일스톤]

메모리 갱신:
  last_completed_phase: LAST → N
  완료 시각: [현재 시각]

백로그 갱신:
  제거된 항목: A건 (이번 Phase에서 구현 완료)
  추가된 항목: B건 (신규 미구현 발견)
  현재 백로그: C건 (🔴 미구현 / 🟡 부분 구현 / 🟢 확인 필요)

Git:
  커밋: "Phase N 완료: [요약]"
  브랜치: main
  push: ✅ 성공

다음 실행 가능 Phase: N+1
  "/Phase N+1" 또는 작업 완료 후 "/Complete N+1"
```

Phase 9 완료 시에는 다음 메시지를 추가한다:
```
🎉 모든 Phase(0~9) 완료! BassMusicGear v0.1.0 릴리즈 준비 완료.
   "/InstallPlugin vst3 release" 로 DAW 테스트를 진행하세요.
```
