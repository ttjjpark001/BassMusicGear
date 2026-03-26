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

### 결론

베이스 앰프 시뮬레이터 특성상 **모바일은 우선순위가 낮습니다.**
실시간 저레이턴시 처리와 외부 오디오 인터페이스 연결이 모바일에서 제약이 많기 때문입니다.
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
