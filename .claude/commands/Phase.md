# /Phase

BassMusicGear 프로젝트의 특정 Phase를 처음부터 끝까지 자동으로 진행한다.
ToolCreator → CodeDeveloper → CodeCommenter → CodeReviewer → CodeBuilder → CodeTester
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
파일 경로: C:/Users/남편/.claude/projects/E--Vibe-Coding-Claude-Code-BassMusicGear/memory/MEMORY.md
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
- ✅ CARRY: [이월 항목, 있을 경우]
- P0 구현 항목: [구현 항목 목록]
- P1 항목 (건너뜀): [P1 항목 목록]
- 실행 프롬프트: [실행 프롬프트 전문]

PRD.md와 CLAUDE.md의 관련 섹션을 함께 참고해서 구현해줘.
P1 항목은 건너뛰고 P0 항목만 구현한다."
```

**완료 확인**: CodeDeveloper가 구현 보고를 반환하면 생성/수정된 파일 목록을 기록하고 STEP 3으로 진행.

```
✅ STEP 2 완료 — 구현된 파일 N개
```

---

### STEP 3: CodeCommenter — 주석 작성

**CodeCommenter 에이전트 호출:**
```
"STEP 2에서 CodeDeveloper가 생성/수정한 다음 파일들에 한글 주석을 달아줘:
 [STEP 2에서 기록한 파일 목록]

 클래스·함수 단위 Doxygen 주석과 DSP 수식 인라인 주석을 우선으로 작성하고,
 자명한 코드에는 주석을 생략해줘."
```

**완료 확인**: CodeCommenter가 완료 보고를 반환하면 STEP 4로 진행.

```
✅ STEP 3 완료 — 주석 추가 완료
```

---

### STEP 4: CodeReviewer — 코드 리뷰

**CodeReviewer 에이전트 호출:**
```
"STEP 2~3에서 작성된 다음 파일들을 종합 검토해줘:
 [STEP 2에서 기록한 파일 목록]

 버그 탐지 및 수정, 데드 코드 제거, 코딩 스타일 통일, 일관성 점검을
 순서대로 수행하고, DSP 파일은 DspReviewer와 협업해서 RT 안전성도 확인해줘."
```

**완료 확인**:
- CodeReviewer가 CRITICAL 항목을 발견하고 수정했다면 → 수정된 파일을 기록
- CodeReviewer 완료 보고가 반환되면 STEP 5로 진행

```
✅ STEP 4 완료 — CRITICAL N건 수정, WARNING M건 수정
```

---

### STEP 5: CodeBuilder — 빌드

**CodeBuilder 에이전트 호출:**
```
"Phase N 구현 코드를 Debug → Release 순서로 순차 빌드해줘.
 빌드 중 에러나 경고가 발생하면 수정 후 CodeReviewer를 통해 재리뷰하고
 다시 빌드를 진행해줘.
 두 구성 모두 클린하게 빌드되는 상태로 만들어줘."
```

**완료 확인**:
- Debug 빌드 ✅, Release 빌드 ✅ 모두 성공한 경우에만 STEP 6으로 진행
- 어느 하나라도 최종 실패 시 → 사용자에게 보고하고 중단

```
✅ STEP 5 완료 — Debug ✅ Release ✅
```

---

### STEP 6: CodeTester — 테스트

**CodeTester 에이전트 호출:**
```
"Phase N의 테스트 기준을 바탕으로 단위 테스트와 스모크 테스트를 작성하고 실행해줘.

Phase N 테스트 기준:
[0-C에서 추출한 테스트 기준 전문]

테스트 실패 시:
- 테스트 코드 문제면 직접 수정 후 재실행
- 앱 코드 문제면 CodeReviewer 리뷰 후 수정하여 재실행

모든 테스트가 통과할 때까지 반복해줘."
```

**완료 확인**:
- 모든 단위 테스트 통과 시 STEP 7로 진행
- 3회 반복 후에도 실패 항목이 남으면 → 사용자에게 보고하고 중단

```
✅ STEP 6 완료 — 전체 테스트 통과
```

---

### STEP 7: 메모리 갱신

모든 단계가 성공적으로 완료된 경우에만 메모리를 갱신한다.

메모리 파일을 다음과 같이 수정한다:
```
파일: C:/Users/남편/.claude/projects/E--Vibe-Coding-Claude-Code-BassMusicGear/memory/MEMORY.md

변경:
- last_completed_phase: N  (N = 방금 완료한 phase 번호)
- Phase N 완료 이력 행: ☐ → ✅, 완료 시각 기재 (현재 날짜/시각)
```

---

### STEP 8: 완료 보고

```
╔══════════════════════════════════════════════════════╗
║         /Phase N 완료                                ║
╚══════════════════════════════════════════════════════╝

Phase N: [Phase 이름]
마일스톤: [해당 Phase 마일스톤]

단계별 결과:
  STEP 1 ToolCreator   ✅  [생성 파일 수]개 생성
  STEP 2 CodeDeveloper ✅  [구현 파일 수]개 작성
  STEP 3 CodeCommenter ✅  주석 추가 완료
  STEP 4 CodeReviewer  ✅  CRITICAL N건 / WARNING M건 수정
  STEP 5 CodeBuilder   ✅  Debug + Release 빌드 성공
  STEP 6 CodeTester    ✅  단위 테스트 N건 전체 통과

P1 이월 항목 (다음 Phase에서 처리):
  - [P1 항목 목록, 없으면 "없음"]

다음 실행 가능 Phase: [N+1] — "/Phase [N+1]"로 진행하세요.

스모크 테스트 체크리스트:
  Tests/SmokeTest_Phase[N].md 를 참고하여 Standalone에서 수동 확인 바랍니다.
```

---

## 중단 및 재시작 안내

어떤 STEP에서든 치명적인 문제로 진행이 불가능한 경우:
1. 현재 상황을 상세히 보고한다.
2. 메모리는 갱신하지 않는다 (Phase가 완료되지 않았으므로).
3. 문제 해결 후 동일한 `/Phase N` 명령으로 재시작할 수 있다.
   - 단, 재시작 시 이미 완료된 STEP은 건너뛰지 않고 처음부터 다시 수행한다.
   - 각 에이전트가 이미 존재하는 파일을 인식하고 불필요한 중복 작업을 스스로 최소화한다.
