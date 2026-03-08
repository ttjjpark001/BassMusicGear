# DEVENV.md — 개발 환경 사양 검토

BassMusicGear(JUCE C++ 오디오 플러그인) 프로젝트 기준으로 현재 보유 장비의 충분성을 평가하고 권장 사양을 정리한다.

---

## 현재 보유 장비

| 항목 | Windows PC (주 개발 머신) | Mac Mini (macOS / AU 빌드) |
|---|---|---|
| CPU | Intel Core i7-10700K @ 3.8GHz (8코어 16스레드, 터보 5.1GHz) | Apple M4 (10코어 CPU: 4P+6E) |
| GPU | Intel UHD Graphics 630 (공유 VRAM 128MB) | Apple M4 10코어 GPU (통합, Metal 3) |
| RAM | 32GB DDR4 | 16GB 통합 메모리 (Unified Memory) |
| OS | Windows 10/11 | macOS 15 (Sequoia) |
| 오디오 I/F | Focusrite Scarlett 18i8 (ASIO) | Focusrite Scarlett 4i4 (Core Audio) |
| 주 용도 | MSVC 2022, CMake, Standalone/VST3 빌드·테스트 | Clang, AU 빌드, macOS 플랫폼 검증 |

---

## 평가 결과 요약

| 항목 | Windows PC | Mac Mini | 비고 |
|---|---|---|---|
| C++ 컴파일 속도 | ✅ 충분 | ✅ 매우 우수 | M4 Clang 빌드 속도 탁월 |
| 실시간 오디오 처리 | ✅ 충분 | ✅ 우수 | 양쪽 모두 3–5ms 레이턴시 목표 달성 가능 |
| JUCE UI 렌더링 | ✅ 충분 | ✅ 충분 | JUCE는 CPU 소프트웨어 렌더링이 기본 |
| GPU 가속 | ✅ 충분 | ✅ 충분 | 이 프로젝트에서 GPU 병목 없음 |
| RAM | ✅ 이상적 (32GB) | ✅ 충분 (16GB 통합) | 모두 확인 완료 |
| 오디오 인터페이스 | ✅ 최적 (18i8 ASIO) | ✅ 충분 (4i4 Core Audio) | 저지연 테스트 환경 확보 |
| NAM (Post-MVP) | ⚠️ 주의 | ✅ 매우 우수 | M4 Neural Engine 38 TOPS |

**결론: 현재 보유 장비로 MVP 전 구간 및 Post-MVP 개발 모두 가능. 추가 구매 불필요.**

---

## 항목별 상세 평가

### 1. CPU

**Windows PC — i7-10700K**

| 작업 | 평가 | 근거 |
|---|---|---|
| CMake 풀 빌드 (JUCE + 플러그인) | ✅ | 8코어 16스레드로 `cmake --build --parallel` 활용 |
| 증분 빌드 (파일 1~3개 수정) | ✅ | 터보 클럭 5.1GHz로 단일 파일 컴파일 신속 |
| 실시간 오디오 처리 (processBlock) | ✅ | 버퍼 128 samples @ 44.1kHz = 2.9ms 처리 여유 충분 |
| 오버샘플링 4x (Preamp/Overdrive) | ✅ | 처리 사이클 여유 있음 |
| 캐비닛 IR 컨볼루션 (juce::dsp::Convolution) | ✅ | 파티션드 FFT는 CPU 부담 낮음 |
| NAM 실시간 추론 (Post-MVP) | ⚠️ | LSTM 계열 경량 NAM은 가능. WaveNet 대형 모델은 버퍼 크기를 256 이상으로 키워야 안정적 |

**Mac Mini — Apple M4**

| 작업 | 평가 | 근거 |
|---|---|---|
| CMake 풀 빌드 | ✅ 매우 빠름 | M4 + LLVM Clang, i7-10700K 대비 2–3배 빠른 빌드 예상 |
| 실시간 오디오 처리 | ✅ 우수 | 전력 효율 극대화, 저레이턴시 안정적 유지 |
| NAM 실시간 추론 | ✅ 매우 우수 | Neural Engine 38 TOPS — M2(15.8 TOPS)의 2.4배. 대형 NAM 모델도 실시간 처리 가능 |
| AU 빌드 + 검증 | ✅ | `auval` 명령어로 AU 컴포넌트 검증 가능 |

> **M4 Neural Engine 38 TOPS**: NeuralAmpModelerCore가 ANE(Apple Neural Engine)를 활용할 경우, 실시간 CPU 부하 없이 NAM 추론 가능. Post-MVP NAM 기능의 Mac 구현에서 큰 강점.

---

### 2. GPU

**Windows PC — Intel UHD Graphics 630**

- JUCE 플러그인 UI는 기본적으로 **CPU 소프트웨어 렌더링**을 사용. GPU는 직접적 병목이 아님.
- OpenGL 4.6 지원 → `juce::OpenGLContext` 사용 시에도 동작함.
- 단, 다음 상황에서 제한 발생 가능:

| 상황 | 영향 | 대응 |
|---|---|---|
| 고해상도(4K) 모니터 + 복잡한 UI 애니메이션 | ⚠️ 렌더링 드롭 가능 | JUCE OpenGL 렌더러 비활성화 유지 (기본값) |
| 다중 모니터 (3대 이상) | ⚠️ 출력 포트 제한 | 모니터 2대 이내 운용 |
| GPU 기반 신호 처리 (미래 기능) | ⚠️ 미지원 | CUDA 미지원 (NAM CPU 추론으로 대체) |

**결론**: 이 프로젝트 전 구간(MVP + Post-MVP)에서 GPU는 병목이 되지 않는다.

**Mac Mini — Apple M4 통합 GPU (10코어, Metal 3)**

- Metal 3 지원, macOS 네이티브 렌더링 파이프라인 최적화.
- JUCE의 macOS Metal/OpenGL 렌더링 경로에서 충분한 성능.
- 통합 메모리 구조로 GPU 메모리 대역폭이 120 GB/s (M4 기준) — 렌더링 병목 없음.

---

### 3. RAM

**Windows PC — 32GB DDR4** ✅ 확인 완료

| 용도 | 점유량 추정 |
|---|---|
| Visual Studio 2022 (IDE + IntelliSense) | 3–6GB |
| CMake 빌드 프로세스 (병렬 컴파일) | 4–8GB |
| DAW (Reaper/Ableton 등) 테스트 실행 | 1–3GB |
| 기타 (OS, 브라우저, 터미널) | 2–4GB |
| **최대 동시 사용 추정** | **~20GB** |
| **여유** | **~12GB** |

32GB는 이 프로젝트에서 **이상적**. 대용량 IR 파일 다수 로드, NAM 모델 여러 개 동시 로드 시에도 여유 있음.

**Mac Mini — 16GB Unified Memory** ✅ 충분

Apple Silicon 통합 메모리는 CPU/GPU/Neural Engine이 공유하는 고대역폭 메모리(M4: 120 GB/s).
일반 DDR4 16GB보다 체감 효율이 높아 이 프로젝트 전 구간에서 충분.

---

### 4. 오디오 인터페이스

**Windows PC — Focusrite Scarlett 18i8 (3rd/4th Gen)** ✅ 최적

| 항목 | 사양 | 프로젝트 적합성 |
|---|---|---|
| 입력 구성 | 2× Combo XLR/TRS (프리앰프), 8× Line In (DB25), 8× ADAT | ✅ 베이스 DI 연결 여유 충분 |
| 출력 구성 | Main L/R, 6× Line Out (DB25), 헤드폰 1 | ✅ 스테레오 모니터링 완비 |
| Windows 드라이버 | **Focusrite ASIO** 전용 드라이버 | ✅ 저지연 ASIO 테스트 가능 |
| 지원 샘플레이트 | 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz | ✅ PRD 전 샘플레이트 커버 |
| 비트 뎁스 | 24-bit | ✅ |
| 예상 왕복 레이턴시 | 버퍼 64 samples @ 96kHz ≈ 2.7ms | ✅ ASIO 3–6ms 목표 하회 |
| 연결 | USB-C | ✅ |

> PRD에서 요구하는 "입력 채널 선택" 기능 (SettingsPage)은 18i8의 다채널 입력 구조로 완벽히 검증 가능.
> ASIO 패널 열기 기능 (`showControlPanel()`)도 Focusrite ASIO 드라이버에서 지원됨.

**Mac Mini — Focusrite Scarlett 4i4 (3rd/4th Gen)** ✅ 충분

| 항목 | 사양 | 프로젝트 적합성 |
|---|---|---|
| 입력 구성 | 2× Combo XLR/TRS (프리앰프), 2× Line In | ✅ 베이스 DI 연결 충분 |
| 출력 구성 | Main L/R, 2× Line Out, 헤드폰 1 | ✅ 스테레오 모니터링 완비 |
| macOS 드라이버 | **Core Audio** (드라이버 설치 불필요) | ✅ macOS 네이티브 |
| 지원 샘플레이트 | 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz | ✅ |
| 예상 왕복 레이턴시 | 버퍼 64 samples @ 96kHz ≈ 2.7ms | ✅ Core Audio 3–5ms 목표 하회 |
| 연결 | USB-C | ✅ |

---

### 5. 스토리지

**Windows PC** ✅ 충분

| 드라이브 | 여유 공간 | 용도 권장 |
|---|---|---|
| 메인 SSD | **214GB** | 소스 코드, Visual Studio, 빌드 디렉터리 |
| 외장 SSD | **1.4TB** | DAW 프로젝트, IR 컬렉션, NAM 모델, 백업 |

→ 메인 SSD 214GB는 이 프로젝트 전 구간(빌드 + IDE + 여러 DAW)을 수용하고도 남는다.

**Mac Mini** ⚠️ 메인 SSD 주의 필요

| 드라이브 | 여유 공간 | 상태 |
|---|---|---|
| 메인 SSD | **19.6GB** | ⚠️ 빠듯함 — 즉각 조치 필요 |
| 외장 SSD | **584GB** | ✅ 충분 — 빌드 디렉터리 이전 대상 |

**Mac 메인 SSD 19.6GB는 Xcode + JUCE 빌드 디렉터리를 그대로 메인에 두면 공간 부족이 발생한다.**

| 용도 | 필요 용량 |
|---|---|
| Xcode (최신 버전) | ~12–14GB |
| JUCE 소스 (git submodule) | ~500MB |
| 빌드 디렉터리 Debug+Release | ~5–8GB |
| **합계** | **~18–23GB** |

메인 SSD 19.6GB 여유로는 Xcode 설치 후 빌드 디렉터리를 메인에 둘 여유가 없다.

**권장 조치 (Mac):**

1. **빌드 디렉터리를 외장 SSD로 이전** (가장 효과적, 즉시 적용 가능)
   ```bash
   # cmake 구성 시 빌드 경로를 외장 SSD로 지정
   cmake -B /Volumes/ExternalSSD/BassMusicGear-build -DCMAKE_BUILD_TYPE=Debug
   cmake --build /Volumes/ExternalSSD/BassMusicGear-build --config Release
   ```
   > 외장 SSD가 USB-C/Thunderbolt 연결이면 빌드 속도 영향 미미.

2. **Xcode 캐시 및 파생 데이터를 외장 SSD로 이전**
   - Xcode → Settings → Locations → Derived Data 경로를 외장 SSD로 변경
   - 절약 가능 용량: 수 GB ~ 수십 GB

3. **macOS 시스템 정리** (조치 전 확인)
   - `~/Library/Caches`, `~/Library/Developer/Xcode/iOS DeviceSupport` 정리
   - `brew cleanup` 실행

4. **장기적으로 메인 SSD 여유를 30GB 이상 확보** 권장
   - macOS 가상 메모리(swap) 파일이 메인 SSD를 사용하므로 20GB 미만은 시스템 전체 성능에 영향 줄 수 있음

---

## 권장 사양 (이 프로젝트 기준)

### 최소 사양

| 항목 | Windows | macOS |
|---|---|---|
| CPU | Intel Core i5-8세대 이상 / AMD Ryzen 5 3600 이상 (6코어) | Apple Silicon M1 이상 |
| RAM | **16GB** | 8GB (통합 메모리) |
| Storage | SSD 50GB 여유 | SSD 50GB 여유 |
| GPU | DirectX 11 / OpenGL 3.3 이상 (내장 그래픽 가능) | 내장 그래픽 |
| OS | Windows 10 64-bit | macOS 12 (Monterey) 이상 |
| 오디오 I/F | ASIO 지원 장치 (Focusrite Scarlett 계열 등) | Core Audio 지원 장치 |

### 권장 사양

| 항목 | Windows | macOS |
|---|---|---|
| CPU | **Intel Core i7-10세대 이상** / AMD Ryzen 7 (8코어 이상) | **Apple Silicon M2 이상** |
| RAM | **32GB** | **16GB (통합 메모리)** |
| Storage | NVMe SSD 100GB 여유 | NVMe SSD 100GB 여유 |
| GPU | 내장 그래픽 가능 (JUCE UI 렌더링에 불필요) | 내장 그래픽 |
| 오디오 I/F | **Focusrite Scarlett 계열 ASIO** (저지연 검증) | **Focusrite Scarlett 계열 Core Audio** |

### NAM Post-MVP 추가 권장

| 항목 | 내용 |
|---|---|
| CPU (Windows) | 12코어 이상 권장 (AMD Ryzen 9 / Intel Core i9). 경량 NAM은 i7-10700K로 가능 |
| GPU (Windows) | NVIDIA GPU (CUDA) 있으면 NAM 학습·추론 가속. 추론 전용이면 CPU로 가능 |
| Mac | **Apple Silicon M2 이상** — M4의 Neural Engine(38 TOPS)이면 대형 모델도 실시간 처리 가능 |

---

## 현재 환경 최종 판정

### Windows PC (i7-10700K + UHD 630 + 32GB + Scarlett 18i8)

| 항목 | 판정 | 비고 |
|---|---|---|
| CPU | ✅ **충분** | 추가 조치 불필요 |
| GPU | ✅ **충분** | JUCE UI에 GPU 가속 불필요. 현 상태 유지 |
| RAM | ✅ **이상적** | 32GB — 빌드/DAW 테스트/NAM 동시 실행 여유 |
| 오디오 I/F | ✅ **최적** | Scarlett 18i8 ASIO — 저지연 테스트 완벽 지원 |
| SSD 여유 | ✅ **충분** | 메인 214GB + 외장 1.4TB |

### Mac Mini (Apple M4 + 16GB + Scarlett 4i4)

| 항목 | 판정 | 비고 |
|---|---|---|
| CPU | ✅ **매우 우수** | M4 10코어, Clang 빌드 속도 i7 대비 2–3배 |
| GPU | ✅ **충분** | M4 10코어 GPU, Metal 3, JUCE 렌더링 문제없음 |
| RAM | ✅ **충분** | 16GB Unified Memory (대역폭 120 GB/s) |
| 오디오 I/F | ✅ **충분** | Scarlett 4i4 Core Audio — AU 빌드 테스트 충분 |
| NAM (Post-MVP) | ✅ **매우 우수** | Neural Engine 38 TOPS — M2 대비 2.4배 |
| AU 빌드 | ✅ | macOS + Xcode + `auval` 검증 환경 완비 |
| 메인 SSD 여유 | ⚠️ **주의** | 19.6GB — Xcode 설치 후 빌드 디렉터리를 외장 SSD로 이전 필수 |
| 외장 SSD 여유 | ✅ **충분** | 584GB — 빌드 디렉터리 이전 대상 |

---

## 개발 환경 셋업 체크리스트

### Windows PC

- [x] RAM 32GB 확인
- [x] ASIO 오디오 인터페이스 확보 (Focusrite Scarlett 18i8)
- [x] 메인 SSD 여유 214GB 확인
- [x] 외장 SSD 여유 1.4TB 확인
- [ ] Visual Studio 2022 Community 설치 (C++ 데스크톱 개발 워크로드 포함)
- [ ] CMake 3.22+ 설치
- [ ] Git 설치 + `git submodule update --init --recursive`
- [ ] Focusrite Control 앱 설치 + 최신 ASIO 드라이버 설치
- [ ] ASIO 패널에서 버퍼 크기 64~128 samples 설정 확인
- [ ] Windows SDK 최신 버전 확인

### Mac Mini

- [x] Apple M4 확인
- [x] 오디오 인터페이스 확보 (Focusrite Scarlett 4i4)
- [x] 외장 SSD 여유 584GB 확인
- [ ] **⚠️ 메인 SSD 공간 확보** — 아래 중 하나 선택:
  - [ ] 불필요한 파일 정리로 30GB 이상 확보
  - [ ] Xcode Derived Data 경로를 외장 SSD로 이전 (Xcode → Settings → Locations)
  - [ ] cmake 빌드 경로를 외장 SSD로 지정 (`cmake -B /Volumes/ExternalSSD/...`)
- [ ] macOS 15 (Sequoia) 이상 확인
- [ ] Xcode 최신 버전 설치 (`xcode-select --install`) ← **메인 SSD 12–14GB 사용**
- [ ] CMake 3.22+ 설치 (`brew install cmake`)
- [ ] Git + `git submodule update --init --recursive`
- [ ] Focusrite Control 앱 설치 (4i4 펌웨어 업데이트)
- [ ] Audio Unit Validation Tool 확인 (`auval -a` 명령어)
- [ ] 코드 사이닝 설정 (AU 배포 시 필요, 개발 중은 불필요)
