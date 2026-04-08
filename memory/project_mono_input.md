---
name: 모노 입력 처리 필요
description: 베이스 입력은 모노이므로 입력 채널을 모노로 받도록 수정 필요
type: project
---

현재 Standalone 앱의 입력/출력이 모두 스테레오로 설정되어 있음.

**Why:** 베이스 기타는 모노 신호이므로 입력은 모노 1채널로 받아야 함. 출력은 스테레오 유지.

**How to apply:** SettingsPage 구현 시 (Phase 10) 입력 채널 선택을 모노 1채널로 제한. CLAUDE.md의 채널 선택 패턴 참조:
- `setup.inputChannels.setRange(selectedInputChannel, 1, true)` — 선택한 채널 1개만 활성화
- `processBlock()`에서 `buffer.getReadPointer(0)`으로 모노 채널 처리
- 출력은 `setup.outputChannels.setRange(0, 2, true)` 스테레오 유지
