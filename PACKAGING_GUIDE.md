# 설치 패키지 제작 가이드

## 개요

빌드 완료 후 Windows와 macOS 각각에 맞는 설치 패키지를 제작할 수 있습니다.
플러그인(VST3, AU)을 올바른 시스템 경로에 자동 설치하고, Standalone 앱을 등록하는 인스톨러를 만드는 방법을 설명합니다.

---

## Windows 설치 패키지

### 옵션 1: Inno Setup (권장, 무료)
- `.exe` 인스톨러 생성
- VST3를 `C:\Program Files\Common Files\VST3\`에 자동 설치
- Standalone을 시작 메뉴 및 바탕화면에 등록
- 다운로드: https://jrsoftware.org/isinfo.php

### 옵션 2: NSIS (무료)
- `.exe` 인스톨러 생성
- 스크립트 기반으로 세밀한 커스터마이징 가능
- 다운로드: https://nsis.sourceforge.io

### 옵션 3: WiX Toolset (무료)
- `.msi` 패키지 생성
- 기업 환경 및 그룹 정책 배포에 적합

### 플러그인 설치 경로 (Windows)
| 포맷 | 경로 |
|------|------|
| VST3 | `C:\Program Files\Common Files\VST3\` |
| Standalone | `C:\Program Files\BassMusicGear\` |

---

## macOS 설치 패키지

### 옵션 1: pkgbuild / productbuild (권장, Xcode 내장)
- `.pkg` 인스톨러 생성
- AU, VST3를 올바른 시스템 경로에 자동 설치
- 별도 도구 설치 불필요

```bash
# AU 패키징 예시
pkgbuild --root build/BassMusicGear_artefacts/Release/AU \
         --install-location /Library/Audio/Plug-Ins/Components \
         BassMusicGear_AU.pkg

# VST3 패키징 예시
pkgbuild --root build/BassMusicGear_artefacts/Release/VST3 \
         --install-location /Library/Audio/Plug-Ins/VST3 \
         BassMusicGear_VST3.pkg

# 통합 인스톨러 생성
productbuild --distribution Distribution.xml \
             --package-path . \
             BassMusicGear_Installer.pkg
```

### 옵션 2: DMG (디스크 이미지)
- `.dmg` 파일로 배포
- 드래그 앤 드롭 방식의 간단한 설치
- 플러그인 폴더 바로가기를 DMG 안에 포함시켜 안내

### 플러그인 설치 경로 (macOS)
| 포맷 | 경로 |
|------|------|
| AU | `/Library/Audio/Plug-Ins/Components/` 또는 `~/Library/Audio/Plug-Ins/Components/` |
| VST3 | `/Library/Audio/Plug-Ins/VST3/` 또는 `~/Library/Audio/Plug-Ins/VST3/` |
| Standalone | `/Applications/` |

---

## 코드 서명 및 공증

배포 시 반드시 고려해야 할 사항입니다.

| 항목 | Windows | macOS |
|------|---------|-------|
| 코드 서명 | 선택사항 (없으면 SmartScreen 경고 표시) | **필수** (없으면 Gatekeeper가 실행 차단) |
| 공증 (Notarization) | 해당 없음 | macOS 10.15 이상 배포 시 **필수** |
| 인증서 비용 | 코드 서명 인증서 약 $200~500/년 | Apple Developer Program $99/년 |

### macOS 공증 절차 요약
```bash
# 1. 앱/패키지 서명
codesign --deep --force --sign "Developer ID Application: ..." BassMusicGear.app

# 2. Apple에 공증 요청
xcrun notarytool submit BassMusicGear_Installer.pkg \
    --apple-id "your@email.com" \
    --team-id "TEAMID" \
    --wait

# 3. 공증 스탬프 첨부
xcrun stapler staple BassMusicGear_Installer.pkg
```

---

## 권장 진행 순서

현재 개발 단계에 맞는 배포 방식을 단계적으로 적용합니다.

| 단계 | 방식 | 시점 |
|------|------|------|
| 1단계 | 빌드 결과물(`.vst3`, `.component`)을 zip으로 배포 | 개발/테스트 중 |
| 2단계 | 인스톨러 스크립트 작성 및 패키지 생성 | 기능 완성 후 |
| 3단계 | 코드 서명 인증서 취득 및 공증 | 공개 배포 전 |

---

## Claude Code로 인스톨러 제작

인스톨러 스크립트 작성도 Claude Code에 요청할 수 있습니다.

```
Inno Setup 스크립트 만들어줘
```
```
macOS pkg 인스톨러 빌드 스크립트 만들어줘
```
