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

- 유효 범위: 0~10 (정수)
- 범위를 벗어나거나 숫자가 아닌 경우 → 즉시 중단:
  ```
  ❌ 오류: Phase 번호는 0~10 사이의 정수여야 합니다.
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

### STEP 3: 미커밋 코드 변경 점검 및 정리 (CODE FIX LOOP)

`/Phase N` 완료 이후 추가로 수정된 코드가 있으면 리뷰 → 빌드 → 주석 → 테스트를 모두 통과한 뒤 커밋한다.

#### 3-A. 미커밋 변경 확인

```bash
git status
git diff --stat
```

변경된 파일 중 **소스 코드 파일** (`.cpp`, `.h`, `.cmake` 등) 이 있는지 확인한다.
- 소스 코드 변경 없음 → 이 단계를 건너뛰고 STEP 4로 진행
- 소스 코드 변경 있음 → 3-B로 진행

변경 파일 목록을 `CHANGED_FILES`로 기록한다.

---

#### 3-B. 코드 리뷰 (REVIEW LOOP)

**CodeReviewer 에이전트 호출:**
```
"다음 파일들에 미커밋 변경사항이 있어. 종합 검토해줘:
 [CHANGED_FILES 목록]

 검토 순서:
 1. CLAUDE.md 규칙 준수 여부 (RT 안전성, 오버샘플링, APVTS 패턴 등)
    위반 발견 시 CRITICAL로 분류하고 즉시 수정한다.
 2. 주석 정확성 — 코드 동작과 주석이 일치하는지 확인, 불일치 시 수정
 3. 버그 탐지 및 수정
 4. DSP 파일은 DspReviewer와 협업해서 RT 안전성 확인

 수정이 발생하면 수정된 부분을 재검토해 CRITICAL이 없는 상태로 만들어줘."
```

CRITICAL 0건이 될 때까지 CodeReviewer 내부에서 반복한다.

---

#### 3-C. 빌드 (BUILD LOOP)

**CodeBuilder 에이전트 호출:**
```
"변경된 코드를 Debug → Release 순서로 빌드해줘.

빌드 에러 발생 시:
 1. 에러 원인을 파악하고 코드를 수정한다.
 2. 수정된 코드를 CodeReviewer에게 전달해 리뷰를 받는다.
 3. 리뷰 통과 후 해당 구성부터 빌드를 재시도한다.

두 구성(Debug + Release) 모두 클린하게 빌드되는 상태로 만들어줘."
```

빌드 에러로 코드가 수정되면 3-B(CodeReviewer)부터 다시 진행한다.

---

#### 3-D. 주석 갱신

**CodeCommenter 에이전트 호출:**
```
"리뷰·빌드를 거쳐 최종 확정된 다음 파일들의 한글 주석을 갱신해줘:
 [CHANGED_FILES + 빌드 수정 파일 합산]

 변경된 코드에 맞게 기존 주석을 수정하고, 주석이 없는 새 로직에는 추가해줘.
 자명한 코드에는 주석을 생략해줘."
```

---

#### 3-E. 주석 리뷰

**CodeReviewer 에이전트 호출 (주석 검토):**
```
"3-D에서 주석이 추가·갱신된 다음 파일들의 주석을 검토해줘:
 [CHANGED_FILES + 빌드 수정 파일 합산]

 검토 항목:
 1. 주석이 실제 코드 동작과 일치하는지 확인 (코드는 A인데 주석은 B라고 설명하는 경우 수정)
 2. 앰프 모델명, 회로 토폴로지, 컴포넌트 값 등 명칭이 CLAUDE.md/PRD.md 기준과 일치하는지 확인
 3. 오탈자, 문법 오류, 부정확한 수식 설명 수정
 4. 자명한 코드에 불필요하게 달린 주석 제거

 주석 내용만 수정한다. 로직 코드는 이 단계에서 변경하지 않는다.
 CRITICAL(주석-코드 불일치) 항목이 없는 상태로 만들어줘."
```

주석 리뷰에서 **로직 코드 버그**가 발견된 경우:
- 로직 수정 없이 주석만 교정하고 3-F로 진행한다.
- 버그 내용은 3-F 보고에 포함해 사용자에게 알린다 (로직 수정은 별도 처리).

---

#### 3-F. 테스트

**CodeTester 에이전트 호출:**
```
"변경된 코드와 관련된 단위 테스트를 실행해줘.
 기존 테스트가 모두 통과하는지 확인하고, 새로 추가된 기능이 있다면 테스트를 보완해줘.

 테스트 실패 원인 분류:
  ① 테스트 코드 문제 → 테스트만 수정 후 재실행
  ② 앱 코드 문제 → 앱 코드 수정 → CodeReviewer → Release 빌드 → CodeCommenter(영향 파일) → CodeReviewer 주석 리뷰(영향 파일) → 재실행

 모든 테스트가 통과하면 결과를 보고해줘."
```

3회 루프 후에도 실패 항목이 남으면 → 사용자에게 보고하고 중단.

---

#### 3-G. 코드 변경분 커밋

Review ✅ Build ✅ Comment ✅ CommentReview ✅ Test ✅ 확인 후 코드 변경분을 커밋한다.

```bash
git status
git add -A
git commit -m "Phase N 코드 수정: [수정 내용 한 줄 요약]

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push
```

빌드 결과물(`build/`, `.exe`, `.dll`)이 스테이징에 포함되지 않도록 확인한다.

```
✅ STEP 3 완료 — 코드 변경 없음 [스킵] 또는
✅ STEP 3 완료 — Review ✅ Build ✅ Comment ✅ CommentReview ✅ Test ✅ / 커밋 push 완료
```

---

### STEP 4: 스모크 테스트 수동 확인 항목 점검

이 단계는 사용자와 인터랙티브하게 진행한다.
**STEP 4가 완전히 끝난 후에 STEP 5로 넘어간다.**

#### 4-A. 대상 항목 수집

다음 두 소스에서 수동 확인 대상 항목을 수집한다.

**① 이번 Phase 스모크 테스트 (`Tests/SmokeTest_PhaseN.md`)**

파일을 읽어 `[ ]`로 표시된 항목 전체를 수집한다.
각 항목을 다음 세 종류로 분류한다:

| 종류 | 판별 기준 | 처리 |
|------|---------|------|
| **확인 대상** | `[ ]`이고 명시적 이월 사유("Phase X 이후 확인 필요" 등)가 없음 | 사용자에게 확인 여부 묻기 |
| **이미 이월됨** | `[ ]`이고 "Phase X 이후 확인 필요"(X > N)가 명시됨 | 이월 목록에 기록, 사용자 질문 없이 스킵 |
| **이번 Phase가 이월 대상** | `[ ]`이고 "Phase N 이후 확인 필요" 또는 "Phase N에서 확인" 명시 | 확인 대상으로 포함 |

**② 이전 Phase 스모크 테스트에서 이번 Phase로 이월된 항목**

`Tests/SmokeTest_Phase0.md` ~ `Tests/SmokeTest_Phase(N-1).md`를 읽어,
`[ ]`이고 "Phase N 이후 확인 필요" 또는 "Phase N에서 확인"이 명시된 항목을 수집한다.
(파일이 없으면 스킵)

수집된 이월 항목을 **이전 Phase 이월 확인 대상**으로 분류한다.

---

#### 4-B. 수집 결과 보고

수집 결과를 사용자에게 다음 형식으로 보여준다:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  스모크 테스트 수동 확인 항목 점검 (Phase N)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[확인 대상] Phase N 항목: A건
[확인 대상] 이전 Phase 이월 항목: B건
[스킵] 미래 Phase로 이미 이월된 항목: C건

이월된 항목 (참고):
  - ⚠️ [Phase N 항목명] → Phase X에서 확인 예정
  - ...
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

**확인 대상 항목이 0건이면** 이 단계를 완료로 처리하고 STEP 5로 진행한다.

---

#### 4-C. 항목별 사용자 확인

확인 대상 항목을 하나씩 사용자에게 제시한다.
(이전 Phase 이월 항목의 경우 출처 파일명도 함께 표시)

```
[N번째 / 전체 M건]
📋 항목: [항목 내용]
   출처: Tests/SmokeTest_PhaseX.md

이 항목을 확인하셨나요?
  y  — 확인 완료
  n  — 아직 확인 못함 (이유를 알려주세요)
  s  — 건너뜀 (이번에는 확인 안 함, 변경 없이 유지)
```

사용자 응답에 따라:

**응답 `y` (확인 완료):**
- 해당 항목이 있는 스모크 테스트 파일에서 `[ ]` → `[x]` 로 변경
- 항목 끝에 `(확인 완료)` 추가 (이미 있으면 스킵)
- `⚠️` 아이콘이 있었다면 함께 제거
- 예시:
  ```
  [x] 창 리사이즈 → 레이아웃 비율 유지 (확인 완료)
  ```

**응답 `n` (확인 못함):**
사용자에게 이유만 묻는다:
```
확인하지 못한 이유를 간략히 알려주세요 (예: "컴프레서 UI 없음"):
> [사용자 입력]
```

이유를 받은 후, **Claude가 PLAN.md를 읽어 이월 대상 Phase를 직접 판단한다.**

판단 기준:
- 이유에 언급된 기능(UI, 파라미터, IR 등)이 PLAN.md의 어느 Phase P0 구현 항목에 포함되는지 확인
- 해당 기능 구현 Phase가 X라면 → "Phase X 이후 확인 가능"으로 결정
- 명확히 판단하기 어려운 경우, 가장 가까운 미래 Phase 중 관련성이 높은 것으로 결정
- 결정한 Phase와 근거를 사용자에게 한 줄로 보여준다:
  ```
  → Phase X 이후 확인 가능 (근거: PLAN.md Phase X — [관련 구현 항목])
  ```

결정된 Phase X를 바탕으로:
1. 해당 스모크 테스트 파일의 항목에 이월 사유 추가:
   ```
   - [ ] ⚠️ [항목 내용] (Phase X 이후 확인 필요 — [이유])
   ```
2. `PLAN.md`의 Phase X 스모크 테스트 섹션에 이월 항목 추가:
   ```
   - [ ] [Phase N 이월] [항목 내용]
   ```
   (해당 섹션이 없으면 `### 스모크 테스트 이월 항목` 섹션을 새로 만들어 추가)

**응답 `s` (건너뜀):**
- 해당 항목을 변경 없이 그대로 유지한다.

---

#### 4-D. 점검 완료 보고

모든 항목 처리 후 결과를 요약한다:

```
✅ STEP 4 완료 — 스모크 테스트 점검
   확인 완료: A건 ([x] 처리됨)
   이월 처리: B건 (PLAN.md 갱신 포함)
   건너뜀:    C건
   기존 이월: D건 (변경 없음)
```

---

### STEP 5: PLAN.md에서 Phase 요약 추출

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
Phase 7 완료: 이월 작업 정리 — PowerAmp 포화 차별화 + Delay BPM Sync + NoiseGate/Compressor UI
Phase 8 완료: 프리셋 시스템 — PresetManager + 팩토리 프리셋 15종 + A/B 슬롯 + PresetPanel
Phase 9 완료: UI 완성 + 출력 — VUMeter + SignalChainView + 다크 테마 + 앰프 색상 + 리사이즈
Phase 10 완료: 오디오 설정 + 릴리즈 — SettingsPage + 전체 테스트 통과 + VST3 설치 v0.1.0
```

---

### STEP 6: 메모리 갱신

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

### STEP 7: PLAN.md P1 이월 추적표 갱신

`PLAN.md` 하단의 **P1 이월 추적표**를 읽어, Phase N에서 완료된 항목을 `✅`로 표시한다.

**갱신 방법:**
1. `PLAN.md`의 `## P1 이월 추적표` 섹션을 읽는다.
2. `이월 대상` 열이 `Phase N`인 행을 찾는다.
3. 해당 행의 `완료 여부` 열을 `☐` → `✅ (Phase N에서 완료)`로 수정한다.
4. Phase N의 ✅ CARRY 섹션에 나열된 항목도 동일하게 처리한다.
   (CARRY 항목은 이전 Phase에서 이월된 것이므로 `이월 대상`이 N보다 작을 수 있음)

**주의:**
- 실제로 구현된 항목만 ✅ 처리한다. P1으로 다시 이월된 항목은 `이월 대상`만 다음 Phase로 변경한다.
- `PLAN.md`의 변경 내용도 이후 git add 대상에 포함된다.

```
✅ STEP 7 완료 — PLAN.md P1 이월 추적표 갱신 (완료 처리 N건)
```

---

### STEP 8: BackLogManager — 백로그 갱신

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

**완료 확인**: BackLogManager가 보고를 반환하면 STEP 9로 진행.
`BackLog.md`의 변경 내용도 이후 git add 대상에 포함된다.

```
✅ STEP 8 완료 — BackLog.md 갱신 (제거 N건 / 추가 M건)
```

---

### STEP 9: git add

변경된 파일을 스테이징한다.
`BackLog.md`, `MEMORY.md`, `PLAN.md`, `Tests/SmokeTest_Phase*.md`를 포함한 모든 변경 파일이 대상이다.

```bash
git status
git add -A
```

추가되는 파일 중 `.env`, 대용량 바이너리(`.exe`, `.dll` 등 빌드 결과물),
`build/` 디렉터리가 포함되지 않도록 확인한다.

빌드 결과물이 포함된 경우:
```bash
git reset HEAD build/
git add -A
```

---

### STEP 10: git commit

STEP 5에서 구성한 커밋 메시지로 커밋한다.

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

### STEP 11: git push

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

### STEP 12: 완료 보고

```
╔══════════════════════════════════════════════════════╗
║         /Complete N 완료                             ║
╚══════════════════════════════════════════════════════╝

Phase N: [Phase 이름]
마일스톤: [마일스톤]

코드 변경 정리 (STEP 3):
  [변경 없음 — 스킵] 또는
  [Review ✅ Build ✅ Comment ✅ CommentReview ✅ Test ✅ — 수정 파일 N개 커밋]

스모크 테스트 점검:
  확인 완료: A건 ([x] 처리됨)
  이월 처리: B건 (PLAN.md 갱신 포함)
  건너뜀:    C건
  기존 이월: D건 (변경 없음)

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

Phase 10 완료 시에는 다음 메시지를 추가한다:
```
🎉 모든 Phase(0~10) 완료! BassMusicGear v0.1.0 릴리즈 준비 완료.
   "/InstallPlugin vst3 release" 로 DAW 테스트를 진행하세요.
```
