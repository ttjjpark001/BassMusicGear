# Mac 빌드 환경 설정 가이드

## 개요

이 프로젝트는 Windows와 macOS 모두에서 빌드 가능합니다.
- **Windows**: Standalone + VST3
- **macOS**: Standalone + VST3 + AU (Audio Unit)

코드베이스는 GitHub를 통해 공유하며, Mac에서 Claude Code를 설치한 뒤 동일한 방식으로 작업합니다.

---

## 전체 흐름

```
Windows에서 작업 → GitHub push → Mac에서 pull → Mac에서 빌드
```

---

## Step 1. Windows에서 GitHub에 올리기

```bash
git add .
git commit -m "작업 내용 요약"
git push
```

GitHub 저장소: https://github.com/ttjjpark001/BassMusicGear

---

## Step 2. Mac 환경 준비

### Xcode 설치
App Store에서 Xcode 설치 후 Command Line Tools 설치:
```bash
xcode-select --install
```

### Homebrew 설치 (없는 경우)
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 필수 도구 설치
```bash
brew install cmake git node
```

---

## Step 3. Claude Code 설치

```bash
npm install -g @anthropic-ai/claude-code
```

---

## Step 4. 프로젝트 받기

```bash
git clone https://github.com/ttjjpark001/BassMusicGear
cd BassMusicGear
git submodule update --init --recursive
```

> **주의**: 마지막 줄을 반드시 실행해야 합니다.
> JUCE가 git submodule로 포함되어 있어서, 이 명령 없이는 JUCE 폴더가 비어있고 빌드가 즉시 실패합니다.

---

## Step 5. Claude Code 실행 및 빌드

```bash
cd BassMusicGear
claude
```

Claude Code 실행 후 빌드 요청:
```
빌드해줘
```

CLAUDE.md에 정의된 빌드 커맨드가 자동으로 적용되며, VST3 + AU + Standalone 세 포맷이 모두 빌드됩니다.

### 빌드 결과물 위치
- **Standalone**: `build/BassMusicGear_artefacts/Release/Standalone/`
- **VST3**: `build/BassMusicGear_artefacts/Release/VST3/BassMusicGear.vst3`
- **AU**: `build/BassMusicGear_artefacts/Release/AU/BassMusicGear.component`

---

## 크로스 컴파일 불가 이유

Windows에서 macOS용 빌드는 현실적으로 불가능합니다:

| 이유 | 설명 |
|------|------|
| Apple 코드 서명 | macOS 앱/플러그인 배포에 Apple Developer 인증서 필요. Windows에서 발급/적용 불가 |
| AU 포맷 | Audio Unit은 macOS 전용 프레임워크. macOS SDK 없이 컴파일 불가 |
| Apple SDK 라이선스 | Apple이 SDK를 macOS에서만 사용 가능하도록 제한 |

---

## 문제 발생 시

Mac에서 빌드 오류가 발생하면 Claude Code에 오류 메시지를 그대로 붙여넣으면 됩니다.
Windows에서와 동일하게 CLAUDE.md 컨텍스트가 적용되어 Mac 환경에 맞게 해결해줍니다.
