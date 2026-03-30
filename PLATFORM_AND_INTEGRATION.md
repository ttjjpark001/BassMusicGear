# 플랫폼 지원 및 통합 가이드

## 1. 앱 아이콘

### Claude Code의 한계
Claude Code는 직접 이미지를 생성할 수 없습니다. 아이콘은 외부 도구로 제작 후 프로젝트에 적용합니다.

### 아이콘 제작 방법

**AI 이미지 생성 도구**
- Adobe Firefly, Midjourney, DALL-E 등
- 프롬프트 예시:
  ```
  bass guitar amplifier icon, dark background, orange glow,
  minimalist flat design, app icon style, 1024x1024
  ```

**디자인 도구**
- Figma (무료) — 벡터로 직접 제작
- Canva — 템플릿 활용

**아이콘 소스 사이트**
- Flaticon, Icons8 — 기존 아이콘 커스터마이징

### 필요한 파일 규격

| 플랫폼 | 형식 | 크기 |
|--------|------|------|
| Windows | `.ico` | 256x256 (다중 해상도 포함) |
| macOS | `.icns` | 1024x1024 (다중 해상도 포함) |
| JUCE 공통 | PNG | 1024x1024 권장 |

1024x1024 PNG 하나만 준비하면 `iconutil`(macOS), `ImageMagick`(Windows/macOS) 으로 `.icns`와 `.ico`를 자동 변환할 수 있습니다.

### JUCE 프로젝트에 적용
아이콘 이미지가 준비되면 Claude Code에 요청하면 적용해드립니다.
```
앱 아이콘 적용해줘  (이미지 파일 첨부)
```

---

## 2. 모바일 플랫폼 지원

### 현재 상태
현재 `CMakeLists.txt`는 데스크탑 포맷만 빌드합니다:
```cmake
FORMATS  Standalone VST3 AU
```

### iOS

| 항목 | 내용 |
|------|------|
| 기술적 가능성 | JUCE가 iOS 빌드 지원 |
| 플러그인 포맷 | AUv3만 가능 (VST3/AU는 iOS 미지원) |
| 추가 작업 | `FORMATS`에 `AUv3` 추가 + Xcode iOS 타겟 설정 |
| 실용성 | 제한적 — 실시간 레이턴시, DSP 성능 이슈 가능 |

### Android

| 항목 | 내용 |
|------|------|
| 기술적 가능성 | JUCE가 Android 빌드 지원 |
| 플러그인 포맷 | 없음 — Standalone 앱으로만 배포 가능 |
| 실용성 | 낮음 — 오디오 생태계 미성숙, 레이턴시 문제 |

### 모바일 오디오 인터페이스와 함께 사용하는 경우

iRig, Apogee Jam, Focusrite iTrack 같은 모바일 오디오 인터페이스를 스마트폰/태블릿에 연결해 베이스 앰프 앱으로 쓰는 시나리오입니다.

#### iOS + 모바일 오디오 인터페이스

iOS는 이 시나리오에서 가장 현실적인 선택지입니다.

**레이턴시 현실**

| 구성 | 왕복 레이턴시 |
|------|--------------|
| iPhone + iRig (Lightning/USB-C) | ~5–10ms (Core Audio, 저버퍼 모드) |
| iPad + Focusrite iTrack Solo | ~5–8ms |
| 참고: 데스크탑 ASIO | ~3–6ms |

iOS Core Audio는 모바일 플랫폼 중 가장 낮은 레이턴시를 제공하며, 실시간 연주에 실용적인 수준입니다.

**현재 코드베이스에서 iOS Standalone 빌드 가능 여부**

결론부터 말하면: **DSP/신호처리 코드는 그대로 사용 가능, UI와 빌드 설정은 수정 필요**

| 레이어 | 현재 상태 | iOS 빌드 시 작업 필요 여부 |
|--------|-----------|--------------------------|
| DSP 코드 (`Source/DSP/`) | 순수 C++17, 플랫폼 무관 | **불필요** — 그대로 컴파일 됨 |
| JUCE DSP 모듈 | 플랫폼 추상화 완비 | **불필요** |
| `PluginProcessor` | 플랫폼 무관 | **불필요** |
| `AudioDeviceManager` | JUCE가 iOS Core Audio 래핑 | **불필요** (API 동일) |
| `SettingsPage` — ASIO 패널 | `showControlPanel()` iOS 미존재 | **필요** — `#if JUCE_IOS` 분기 처리 |
| `Knob.h/.cpp` — 드래그 조작 | 마우스 이벤트 기반 | **필요** — JUCE는 터치를 마우스로 변환하므로 동작은 되나, 멀티터치 UX 개선 권장 |
| UI 레이아웃 | 데스크탑 고정 크기 상정 | **필요** — 화면 크기 대응 |
| CMakeLists.txt | `FORMATS Standalone VST3 AU` | **필요** — iOS 크로스 컴파일 툴체인 추가 |

**iOS 빌드를 위한 최소 변경 사항**

```cmake
# CMakeLists.txt — iOS Standalone 타겟 추가 시
juce_add_plugin(BassMusicGear
    ...
    FORMATS  Standalone VST3 AU  # AUv3 추가 가능
    ...
)

# iOS 크로스 컴파일 시 cmake 호출 방법
cmake -B build-ios \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="Apple Development" \
  -GXcode
```

```cpp
// SettingsPage.cpp — ASIO 전용 코드 분기
#if JUCE_WINDOWS
    // ASIO 컨트롤 패널 버튼
    asioButton.onClick = [&]() {
        if (auto* device = deviceManager.getCurrentAudioDevice())
            if (auto* asio = dynamic_cast<juce::ASIOAudioIODevice*>(device))
                asio->showControlPanel();
    };
#endif
```

#### Android + 모바일 오디오 인터페이스

Android는 현실적으로 **권장하지 않습니다.**

| 항목 | 현황 |
|------|------|
| 레이턴시 | USB 오디오 인터페이스 연결 시 20–50ms 수준 (기기마다 다름) |
| JUCE 지원 | Oboe 백엔드로 Android 빌드 지원 — 기술적으로는 가능 |
| USB 오디오 클래스 지원 | Android 5.0+ 공식 지원이나 기기별 드라이버 호환성 불안정 |
| 오디오 인터페이스 호환 | iRig 등 iOS 전용 제품은 Android 미지원 |
| 실용 결론 | 실시간 연주용으로는 레이턴시 문제가 해결되지 않음 |

### 결론

| 시나리오 | 현실성 |
|----------|--------|
| **iOS + 모바일 인터페이스 (iRig 등)** | 실용적 — 레이턴시 허용 범위, 코드 재사용률 높음 |
| **Android + 모바일 인터페이스** | 비권장 — 레이턴시·호환성 이슈 미해결 |
| **데스크탑 Standalone (현재)** | 메인 타겟 — ASIO/Core Audio 저레이턴시 |

**iOS Standalone 앱이 필요해지는 시점**에 작업하면 됩니다. DSP 코드를 전혀 건드릴 필요 없이 UI 레이아웃과 빌드 설정만 수정하면 되므로 추가 비용이 상대적으로 낮습니다.

나중에 iOS 지원을 추가하고 싶다면 그때 별도로 작업합니다.

---

## 3. 다른 앱과 함께 사용 (플러그인 체이닝)

### Cabinet Bypass 기능

현재 프로젝트에는 **Cabinet Bypass(`cab_bypass`) 파라미터가 이미 구현**되어 있습니다.
이를 활용해 캐비닛 시뮬레이션만 외부 플러그인에 맡길 수 있습니다.

### DAW에서 체이닝 (권장 방법)

```
베이스 입력
    ↓
[BassMusicGear VST3]  ← Cabinet Bypass ON
Gate → Preamp → ToneStack → Effects → PowerAmp
    ↓
[외부 캐비닛 시뮬레이터 VST3]
Celestion IR / OwnHammer IR / Two Notes Torpedo 등
    ↓
출력
```

DAW(Reaper, Ableton, Logic 등)의 트랙에 플러그인을 순서대로 삽입하면 됩니다.

### 추천 캐비닛 시뮬레이터

| 이름 | 가격 | 특징 |
|------|------|------|
| Two Notes Torpedo Wall of Sound | 유료 | 베이스 전용 IR 다수, 고품질 |
| Bogren Digital Ampknob | 유료 | 고품질 베이스 캐비닛 |
| Ignite Amps NadIR | 무료 | 간단한 IR 로더 |
| Kefir | 무료 | 경량 IR 로더 |

### Standalone 앱끼리 연결 (비권장)

Standalone 앱 간 오디오 직렬 연결은 가상 오디오 케이블(VB-Audio Cable, BlackHole 등)로 가능하지만:
- ASIO는 한 앱이 독점 → 두 앱이 동시에 ASIO 사용 불가
- 레이턴시 증가 문제
- DAW 체이닝 방식을 강력히 권장

### 역방향 사용 (외부 앰프 + 이 앱의 Cabinet만 사용)

반대로, 다른 앰프 시뮬레이터에서 처리 후 이 앱의 Cabinet IR만 사용하는 것도 동일한 방식으로 가능합니다.
