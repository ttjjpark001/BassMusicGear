# /Phase

BassMusicGear 프로젝트의 특정 Phase를 처음부터 끝까지 자동으로 진행한다.
ToolCreator → CodeDeveloper → CodeReviewer → CodeBuilder → CodeCommenter → CodeReviewer → CodeTester
순서로 전문 에이전트를 순차 호출하여 해당 Phase를 완성한다.

## 입력

$ARGUMENTS

사용 예:
```
/Phase 0
/Phase 1
/Phase 3
```

---

## 실행 지침

아래 단계를 순서대로 수행한다. 각 단계를 시작하기 전에 이전 단계가 완료되었음을 반드시 확인한다.

---

### STEP 0: 사전 검증

**0-A. 입력 파싱**

`$ARGUMENTS`에서 phase 번호를 파싱한다.
- 유효 범위: 0~9 (정수)
- 범위를 벗어나거나 숫자가 아닌 경우 → 즉시 중단:
  ```
  ❌ 오류: Phase 번호는 0~9 사이의 정수여야 합니다.
  ```

**0-B. 메모리 확인**

메모리 파일을 읽어 마지막으로 완료된 Phase를 확인한다:
```
파일 경로: E:/Vibe Coding/Claude Code/BassMusicGear/memory/MEMORY.md
읽을 값: last_completed_phase
```

`last_completed_phase` 값을 `LAST`라 하고, 요청된 phase 번호를 `N`이라 할 때:

| 조건 | 처리 |
|------|------|
| `N <= LAST` | 이미 완료된 Phase → **중단** |
| `N > LAST + 1` | Phase를 건너뜀 → **중단** |
| `N == LAST + 1` | 정상 — 다음 단계로 진행 |
| `N == 0` 이고 `LAST == -1` | 정상 (첫 번째 Phase) — 다음 단계로 진행 |

**중단 메시지 예시:**
```
❌ 중단: Phase 3은 이미 완료된 Phase입니다. (마지막 완료: Phase 3)

❌ 중단: Phase 5를 실행하려면 먼저 Phase 4가 완료되어야 합니다.
         마지막 완료 Phase: 2
         다음 실행 가능 Phase: 3
```

**0-C. PLAN.md에서 Phase 정보 로드**

`PLAN.md`를 읽어 `Phase N`의 다음 정보를 추출한다:
- 목표 및 마일스톤
- `🔧 TOOL` 섹션 (있을 경우)
- `✅ CARRY` 섹션 (있을 경우)
- `P0 구현 항목`
- `P1 항목`
- `테스트 기준`
- `실행 프롬프트`

**0-D. CLAUDE.md에서 Phase N 적용 규칙 추출**

`CLAUDE.md`를 읽어 Phase N의 P0 구현 항목에 해당하는 DSP/UI 모듈의 규칙을 추출한다.

추출 기준:
- 0-C에서 파악한 **구현 대상 모듈** (Preamp, ToneStack, Overdrive, Cabinet 등)과 관련된 규칙만 선별
- "금지" / "필수" / "~해야 한다" / "~하지 말 것" 표현을 포함한 규칙 우선 추출
- 아키텍처 규칙(processBlock RT 금지, APVTS 패턴 등)은 매 Phase 공통 포함

추출 결과를 `CLAUDE_RULES`로 기록한다. 형식 예시:
```
CLAUDE_RULES:
- [TMB] RC 네트워크 전달 함수 이산화. 독립 필터 3개로 대체 금지
- [VPF] 3필터 모두 구현: ①35Hz 저셸프 ②380Hz 노치 ③10kHz 고셸프 (생략 금지)
- [Preamp/Overdrive] 최소 4x 오버샘플링 (Fuzz는 8x)
- [공통] processBlock 내 new/delete/mutex/파일I/O 절대 금지
- [공통] setLatencySamples(total) 호출 필수
```

**0-E. BackLog.md에서 이월 항목 로드**

`BackLog.md`를 읽어 Phase N에서 처리해야 할 항목을 추출한다:
```
파일 경로: E:/Vibe Coding/Claude Code/BassMusicGear/BackLog.md
```

다음 두 종류의 항목을 추출한다:

1. **미구현 항목** — `## 미구현 항목` 섹션에서 `Phase N` 이하(≤N)에 해당하는 항목
   - PLAN.md의 `✅ CARRY` 섹션에 이미 포함된 항목은 중복 제외
   - 포함되지 않은 항목만 **BackLog 이월 항목**으로 기록

2. **임시 수정 복원 항목** — `## 임시 수정 — Phase N에서 복원 필요` 섹션의 항목
   - 해당 Phase가 N과 일치하는 항목만 추출

추출 결과를 `BACKLOG_CARRY`로 기록한다.
항목이 없으면 `BACKLOG_CARRY = 없음`으로 기록하고 넘어간다.

**PRD.md에서 앰프 모델 기준 확인**

Phase N이 앰프 모델(ToneStack, Preamp, PowerAmp, AmpModelLibrary 등)을 구현하는 경우,
`PRD.md`의 앰프 모델 표를 읽어 각 모델의 실제 앰프 이름을 확인하고 `CLAUDE_RULES`에 추가한다:
```
- [앰프 모델] 주석/이름은 PRD.md 기준 실제 앰프와 일치시킬 것
  (예: American Vintage=Ampeg SVT, Tweed Bass=Fender Bassman,
       British Stack=Orange AD200, Modern Micro=Darkglass B3K,
       Italian Clean=Markbass Little Mark III)
```

---

### STEP 1: ToolCreator — 커맨드 및 스크립트 준비

`🔧 TOOL` 섹션이 존재하는 경우에만 수행한다.

**ToolCreator 에이전트 호출:**
```
"PLAN.md의 Phase N 🔧 TOOL 섹션에 명시된 슬래쉬 커맨드와 셸 스크립트를
 TOOLING.md 명세에 따라 .claude/commands/ 및 scripts/ 에 생성해줘.
 이미 존재하는 파일은 스킵해줘."
```

**완료 확인**: ToolCreator가 생성 완료 보고를 반환하면 STEP 2로 진행.
`🔧 TOOL` 섹션이 없으면 이 단계를 건너뛴다.

```
✅ STEP 1 완료 — [생성된 파일 목록] 또는 [🔧 TOOL 없음, 스킵]
```

---

### STEP 2: CodeDeveloper — 코드 작성

**CodeDeveloper 에이전트 호출:**
```
"PLAN.md의 Phase N을 구현해줘.

Phase N 정보:
- 목표: [0-C에서 추출한 목표]
- ✅ CARRY (PLAN): [PLAN.md 이월 항목, 있을 경우]
- ✅ CARRY (BackLog): [0-E에서 추출한 BACKLOG_CARRY, 없으면 '없음']
- P0 구현 항목: [구현 항목 목록]
- P1 항목 (건너뜀): [P1 항목 목록]
- 실행 프롬프트: [실행 프롬프트 전문]

━━━━ 반드시 준수할 CLAUDE.md 규칙 ━━━━
[0-D에서 추출한 CLAUDE_RULES 전체 목록]

위 규칙은 구현 편의를 위해 단순화하거나 생략할 수 없다.
특히 '금지' 항목은 어떤 이유로도 우회하지 않는다.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

BackLog CARRY 항목은 P0 항목과 동일한 우선순위로 반드시 구현한다.
P1 항목은 건너뛰고 P0 항목만 구현한다."
```

**완료 확인**: CodeDeveloper가 구현 보고를 반환하면 생성/수정된 파일 목록을 기록하고 STEP 3으로 진행.

```
✅ STEP 2 완료 — 구현된 파일 N개
```

---

### STEP 3: CodeReviewer — 코드 리뷰 (REVIEW LOOP)

**어떤 이유로든 코드가 수정되면 반드시 이 STEP을 통과해야 한다.**
CRITICAL 항목이 없을 때까지 CodeReviewer 내부에서 반복한다.

**CodeReviewer 에이전트 호출:**
```
"STEP 2에서 작성된 다음 파일들을 종합 검토해줘:
 [STEP 2에서 기록한 파일 목록]

 검토 순서:
 1. CLAUDE.md 규칙 준수 여부 항목별 대조
    아래 규칙이 코드에 실제로 반영되어 있는지 파일을 직접 읽어 확인한다.
    위반 발견 시 CRITICAL로 분류하고 즉시 수정한다:
    [0-D에서 추출한 CLAUDE_RULES 전체 목록]

 2. 주석 정확성 검증
    - 주석이 실제 코드 동작과 일치하는지 확인 (코드는 A인데 주석은 B라고 설명하는 경우 수정)
    - 앰프 모델명, 회로 토폴로지, 컴포넌트 값 등 명칭이 CLAUDE.md/PRD.md 기준과 일치하는지 확인
      (예: "Marshall/Hiwatt" → "Orange AD200", "Markbass/Aguilar" → "Darkglass B3K")
    - 위반 발견 시 즉시 수정한다

 3. 버그 탐지 및 수정
 4. 데드 코드 제거, 코딩 스타일 통일, 일관성 점검
 5. DSP 파일은 DspReviewer와 협업해서 RT 안전성 확인

 수정이 발생하면 수정된 부분을 재검토해 CRITICAL이 없는 상태로 만들어줘."
```

**완료 확인**:
- CRITICAL 0건 상태가 될 때까지 CodeReviewer 내부에서 반복
- 최종적으로 CRITICAL 0건이면 STEP 4로 진행

```
✅ STEP 3 완료 — CRITICAL 0건 (수정 N건), WARNING M건
```

---

### STEP 4: CodeBuilder — 빌드 (BUILD LOOP)

**빌드 에러로 코드를 수정하면 반드시 STEP 3(CodeReviewer)으로 돌아간다.**
빌드는 코드가 리뷰를 통과한 상태에서만 재시도한다.

**CodeBuilder 에이전트 호출:**
```
"Phase N 구현 코드를 Debug → Release 순서로 순차 빌드해줘.

빌드 에러 발생 시 처리 규칙:
 1. 에러 원인을 파악하고 코드를 수정한다.
 2. 수정된 코드를 CodeReviewer에게 전달해 리뷰를 받는다.
    (빌드 픽스도 새 버그를 만들 수 있으므로 리뷰 필수)
 3. 리뷰 통과 후 해당 구성(Debug 또는 Release)부터 빌드를 재시도한다.
 4. 클린 빌드가 될 때까지 위 루프를 반복한다.

두 구성(Debug + Release) 모두 클린하게 빌드되는 상태로 만들어줘."
```

**완료 확인**:
- Debug 빌드 ✅, Release 빌드 ✅ 모두 성공한 경우에만 STEP 5로 진행
- 어느 하나라도 최종 실패 시 → 사용자에게 보고하고 중단

```
✅ STEP 4 완료 — Debug ✅ Release ✅
```

---

### STEP 5: CodeCommenter — 주석 작성 (빌드 클린 확인 후 1회)

**빌드가 클린한 상태의 최종 코드에 대해 단 한 번 실행한다.**
이후 테스트에서 앱 코드가 수정되면 영향을 받은 파일에 한해 재실행한다.

**CodeCommenter 에이전트 호출:**
```
"STEP 2~4를 거쳐 최종 확정된 다음 파일들에 한글 주석을 달아줘:
 [STEP 2에서 기록한 파일 목록 + STEP 4에서 수정된 파일 목록 합산]

 클래스·함수 단위 Doxygen 주석과 DSP 수식 인라인 주석을 우선으로 작성하고,
 자명한 코드에는 주석을 생략해줘."
```

**완료 확인**: CodeCommenter가 완료 보고를 반환하면 즉시 CodeReviewer를 호출해 주석을 검토한다.

**CodeReviewer 에이전트 호출 (주석 검토):**
```
"STEP 5에서 주석이 추가된 다음 파일들의 주석을 검토해줘:
 [STEP 5에서 주석이 추가된 파일 목록]

 검토 항목:
 1. 주석이 실제 코드 동작과 일치하는지 확인 (코드는 A인데 주석은 B라고 설명하는 경우 수정)
 2. 앰프 모델명, 회로 토폴로지, 컴포넌트 값 등 명칭이 CLAUDE.md/PRD.md 기준과 일치하는지 확인
 3. 오탈자, 문법 오류, 부정확한 수식 설명 수정
 4. 자명한 코드에 불필요하게 달린 주석 제거

 주석 내용만 수정한다. 로직 코드는 이 단계에서 변경하지 않는다.
 CRITICAL(주석-코드 불일치) 항목이 없는 상태로 만들어줘."
```

주석 리뷰에서 **로직 코드 버그**가 발견된 경우:
- 로직 수정 없이 주석만 교정하고 STEP 6으로 진행한다.
- 버그 내용은 STEP 6 보고에 포함해 사용자에게 알린다 (로직 수정은 별도 처리).

```
✅ STEP 5 완료 — 주석 추가 + 주석 리뷰 완료
```

---

### STEP 6: CodeTester — 단위 테스트 + 스모크 테스트 (TEST LOOP)

**테스트 실패로 앱 코드를 수정하면 반드시 다음 순서를 거친다:**
`수정 → STEP 3(CodeReviewer) → STEP 4 Release 빌드 → STEP 5(CodeCommenter, 영향 파일만) → STEP 5 주석 리뷰(CodeReviewer, 영향 파일만) → 테스트 재실행`

테스트 코드 자체의 문제라면 테스트만 수정하고 바로 재실행한다.

**CodeTester 에이전트 호출:**
```
"Phase N의 테스트 기준을 바탕으로 단위 테스트와 스모크 테스트를 모두 작성하고 실행해줘.

Phase N 테스트 기준:
[0-C에서 추출한 테스트 기준 전문]

Phase N 스모크 테스트:
[0-C에서 추출한 스모크 테스트 항목 전문]

단위 테스트:
- ctest로 실행 가능한 Catch2 테스트를 작성하고 통과시킨다.
- 테스트 실패 원인 분류:
  ① 테스트 코드 문제 → 테스트만 수정 후 재실행
  ② 앱 코드 문제 → 앱 코드 수정 → CodeReviewer 리뷰 → Release 빌드 → CodeCommenter(영향 파일) → CodeReviewer 주석 리뷰(영향 파일) → 테스트 재실행
- 모든 단위 테스트가 통과할 때까지 반복한다.

스모크 테스트:
- Release 빌드의 Standalone 실행 파일을 실제로 실행해서 기본 동작을 확인한다.
- 실행 경로: build/BassMusicGear_artefacts/Release/Standalone/BassMusicGear.exe
- 프로세스가 정상 기동되고 즉시 크래시하지 않는지 확인한다 (수 초간 실행 후 종료).
- 오디오 입출력 실제 청취처럼 자동화가 불가능한 항목은 Tests/SmokeTest_Phase[N].md에
  체크리스트로 작성하고, 항목 옆에 '(수동 확인 필요)' 표시를 붙인다.

완료 후 단위 테스트 통과 건수와 스모크 테스트 결과를 함께 보고해줘."
```

**완료 확인**:
- 모든 단위 테스트 통과 + 스모크 테스트 실행 완료 시 STEP 7로 진행
- 3회 루프 후에도 단위 테스트 실패 항목이 남으면 → 사용자에게 보고하고 중단
- 스모크 테스트 중 Standalone이 즉시 크래시하면 → 앱 코드 수정 → STEP 3 → STEP 4 Release → STEP 5 → 재시도

```
✅ STEP 6 완료 — 단위 테스트 N건 통과, 스모크 테스트 실행 완료
```

---

### STEP 7: git add / commit / push

모든 구현·수정·주석·테스트 파일을 커밋하고 원격 저장소에 푸시한다.

```bash
git status
```

스테이징 전 확인 사항:
- `.env`, 대용량 바이너리(`.exe`, `.dll`), `build/` 디렉터리가 포함되지 않도록 확인
- 빌드 결과물이 포함된 경우 `build/` 제외 후 add

```bash
git add -A
git commit -m "Phase N 구현: [Phase 이름] — [P0 핵심 항목 한 줄 요약]

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push
```

push 실패 시:

| 원인 | 처리 |
|------|------|
| upstream 미설정 | `git push -u origin main` |
| 원격 브랜치 충돌 | 사용자에게 상황 보고 후 중단 (force push 금지) |
| 네트워크 오류 | 재시도 1회, 실패 시 사용자에게 보고 |

```
✅ STEP 7 완료 — 커밋: "Phase N 구현: [요약]" / push ✅
```

---

### STEP 8: 완료 보고

**메모리는 갱신하지 않는다.** Phase 완료는 사용자가 직접 앱을 실행해 최종 확인한 뒤 `/Complete N` 명령으로 처리한다.

```
╔══════════════════════════════════════════════════════╗
║         /Phase N — 코드 작업 완료                    ║
╚══════════════════════════════════════════════════════╝

Phase N: [Phase 이름]
마일스톤: [해당 Phase 마일스톤]

단계별 결과:
  STEP 1 ToolCreator   ✅  [생성 파일 수]개 생성
  STEP 2 CodeDeveloper ✅  [구현 파일 수]개 작성
  STEP 3 CodeReviewer  ✅  CRITICAL N건 수정 (리뷰 M회)
  STEP 4 CodeBuilder   ✅  Debug + Release 빌드 성공 (빌드 L회)
  STEP 5 CodeCommenter ✅  주석 추가 완료
  STEP 6 CodeTester    ✅  단위 테스트 N건 통과 / 스모크 테스트 완료
  STEP 7 Git           ✅  커밋 push 완료

스모크 테스트 결과:
  [자동 확인 항목 결과 목록]
  [수동 확인 필요 항목 목록 — Tests/SmokeTest_Phase[N].md 참고]

BackLog 이월 항목 처리:
  - [BACKLOG_CARRY 항목별 구현 완료 여부, 없으면 "없음"]

P1 이월 항목 (다음 Phase에서 처리):
  - [P1 항목 목록, 없으면 "없음"]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  앱을 직접 실행해서 위 수동 확인 항목을 점검하세요.
  확인 완료 후 → "/Complete N" 으로 Phase를 완료 처리하세요.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## 중단 및 재시작 안내

어떤 STEP에서든 치명적인 문제로 진행이 불가능한 경우:
1. 현재 상황을 상세히 보고한다.
2. 메모리는 갱신하지 않는다 (Phase가 완료되지 않았으므로).
3. 문제 해결 후 동일한 `/Phase N` 명령으로 재시작할 수 있다.
   - 단, 재시작 시 이미 완료된 STEP은 건너뛰지 않고 처음부터 다시 수행한다.
   - 각 에이전트가 이미 존재하는 파일을 인식하고 불필요한 중복 작업을 스스로 최소화한다.
