# TOOLING.md

BassMusicGear 프로젝트에서 활용할 수 있는 Claude Code Skills, Agents, Shell Scripts 목록.
실제 구현은 우선순위에 따라 순차적으로 진행한다.

---

## Slash Commands (`.claude/commands/`)

Claude Code에서 `/command-name`으로 호출하는 재사용 프롬프트.

### `/dsp-audit`
작성한 DSP 코드를 오디오 스레드 안전성 관점에서 점검.

- `processBlock()` 내 금지 패턴 탐지 (`new`/`delete`, mutex, 파일 I/O, 시스템 콜)
- 비선형 스테이지의 오버샘플링 누락 여부
- 파라미터 읽기 방식 (`getRawParameterValue()->load()` vs 직접 접근)
- 필터 계수 재계산을 오디오 스레드에서 하는지 여부
- `setLatencySamples()` 누락 여부

### `/new-amp-model`
새 앰프 모델 추가 시 필요한 파일·항목을 일괄 스캐폴딩.

- `AmpModel` 데이터 구조 항목 (`AmpModelLibrary` 등록 포함)
- `ToneStack` switch-case 분기 스텁
- 기본 매칭 캐비닛 IR 항목 및 `Resources/IR/` 슬롯
- 팩토리 프리셋 XML 3종 (Clean / Driven / Heavy)

사용 예:
```
/new-amp-model "GK 800RB" solid-state
```

### `/add-param`
APVTS 파라미터 추가 보일러플레이트를 한 번에 생성.

- `ParameterLayout`에 `AudioParameterFloat` / `AudioParameterBool` 코드
- `SliderAttachment` / `ButtonAttachment` UI 바인딩 코드
- `getRawParameterValue()` 오디오 스레드 읽기 스니펫

사용 예:
```
/add-param biamp_freq float 60 500 200 Hz
/add-param biamp_enabled bool
```

### `/tone-stack`
톤스택 토폴로지 이름과 컴포넌트 값을 입력하면 IIR 계수 유도 과정을 안내.

- Fender TMB / James / Baxandall / Markbass 각 전달함수 계산 단계
- Bilinear transform 이산화 (Yeh 2006 방식)
- `updateCoefficients()` 구현 스텁 생성

### `/install-plugin`
빌드된 플러그인을 시스템 플러그인 경로에 설치 (DAW 테스트용).

| 플랫폼 | VST3 경로 | AU 경로 |
|---|---|---|
| Windows | `%COMMONPROGRAMFILES%\VST3\` | — |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |

사용 예:
```
/install-plugin vst3 release
```

### `/ir-validate`
`Resources/IR/` 내 WAV 파일을 일괄 검증.

- 샘플레이트 (44.1k / 48k / 96kHz 여부)
- 최대 길이 500ms(22050 samples @ 44.1kHz) 초과 여부
- 채널 수 (모노 / 스테레오)
- 파일명 컨벤션 일치 여부

---

## Agents (`.claude/agents/`)

특정 역할에 특화된 서브에이전트.

### `dsp-reviewer`
DSP 구현 파일 작성 완료 후 자동 실행하여 품질을 검증.

**검사 항목:**
- RT 안전성 위반 탐지
- 오버샘플링 up/down 대칭성 (업샘플 필터 지연 == 다운샘플 필터 지연)
- 필터 계수 재계산 위치 (메인 스레드 → atomic/FIFO → 오디오 스레드)
- 오버드라이브 Dry Blend 파라미터 누락 검사

### `juce-pattern-advisor`
새 JUCE 컴포넌트 또는 DSP 모듈 작성 시 올바른 패턴을 제안.

**커버 범위:**
- APVTS 연동 패턴 (Attachment 방식 vs 직접 setValue 금지)
- `prepareToPlay()` vs `processBlock()` 책임 분리
- `setLatencySamples()` 계산식 (oversampling latency + convolution latency)
- Standalone / Plugin 분기 처리 (`JUCEApplication::isStandaloneApp()`)
- IR 로드 백그라운드 스레드 패턴

### `build-doctor`
CMake 빌드 실패 시 오류를 분류하고 수정안을 제시.

**진단 범주:**
- JUCE 모듈 누락 (`target_link_libraries` 항목 확인)
- 링크 오류 (심볼 미정의, 중복 정의)
- 플랫폼별 헤더 충돌 (Windows/macOS 조건부 컴파일)
- AU 빌드 전용 오류 (macOS SDK 버전, Info.plist)

### `preset-migrator`
파라미터 추가/삭제/이름 변경 후 프리셋 호환성을 유지.

**동작:**
- 기존 팩토리 프리셋 XML과 현재 APVTS `ParameterLayout` 비교
- 누락·불일치 파라미터 탐지 및 기본값 주입 코드 생성
- `setStateInformation()` 하위 호환 처리 스텁

---

## Shell Scripts (`scripts/`)

반복 작업을 자동화하는 프로젝트 전용 스크립트.

| 스크립트 | 설명 |
|---|---|
| `scripts/clean-build.sh` | 빌드 디렉터리 완전 삭제 후 CMake 재구성 |
| `scripts/gen-binary-data.sh` | `Resources/IR/`, `Resources/Presets/` 파일 목록 자동 스캔 → `CMakeLists.txt` `SOURCES` 갱신 |
| `scripts/copy-ir.sh <src>` | 외부 IR WAV 검증(길이·SR·채널) 후 `Resources/IR/`로 복사 |
| `scripts/run-tests.sh [filter]` | Catch2 필터 적용 테스트 실행 + 결과 요약 출력 |
| `scripts/bump-version.sh <major\|minor\|patch>` | `CMakeLists.txt` VERSION 필드 + Git 태그 동시 갱신 |

---

## 구현 우선순위

| 우선순위 | 항목 | 시점 |
|---|---|---|
| 1 | `/dsp-audit`, `/add-param` | 코드 작성 시작 즉시 |
| 2 | `/new-amp-model` | 두 번째 앰프 모델 추가 전 |
| 3 | `dsp-reviewer` agent | 첫 DSP 모듈 구현 완료 후 |
| 4 | `build-doctor` agent, `scripts/gen-binary-data.sh` | 빌드 시스템 안정화 후 |
| 5 | `preset-migrator` agent | 프리셋 시스템 완성 후 |
| 6 | `/tone-stack`, `juce-pattern-advisor` | 필요 시 |
