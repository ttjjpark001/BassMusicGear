# /NewAmpModel

새 앰프 모델 추가에 필요한 모든 파일 항목과 코드 스니펫을 한 번에 스캐폴딩한다.

## 입력

$ARGUMENTS

형식: /NewAmpModel "<모델명>" <타입>
타입: tube | solid-state | class-d
예시: /NewAmpModel "GK 800RB" solid-state
      /NewAmpModel "Ampeg SVT" tube
      /NewAmpModel "MarkBass Little Mark" class-d

## 역할

입력된 모델명과 타입을 바탕으로 다음을 생성한다:

1. **AmpModel.h 데이터 항목** — 모델ID enum 값, AmpModel 구조체 초기화 코드
2. **AmpModelLibrary::createModels()에 추가할 코드** — 모델 등록
3. **ToneStack::setModel() switch-case 분기 스텁** — 모델별 톤스택 토폴로지 선택
4. **Resources/IR/ 슬롯 플레이스홀더** — CMakeLists.txt BinaryData SOURCES에 추가할 IR 경로
5. **팩토리 프리셋 XML 3종** — Clean/Driven/Heavy 프리셋 스텁 (Resources/Presets/)

### 타입별 처리

**tube 모델:**
- PowerAmpSag 파라미터 활성화 (기본값 0.5)
- Preamp 게인 스테이징: 12AX7 cascade (온난하고 부드러운 특성)
- 추천 톤스택: TMB 또는 James

**solid-state 모델:**
- PowerAmpSag 파라미터 비활성화 (회색 처리, 항상 0.0)
- Preamp 게인 스테이징: JFET parallel (밝고 투명한 특성)
- 추천 톤스택: Baxandall 또는 James

**class-d 모델:**
- PowerAmpSag 파라미터 비활성화 (회색 처리, 항상 0.0)
- Preamp 게인 스테이징: Class D linear (매우 중립적)
- 추천 톤스택: Markbass

## 출력 형식

### 1. AmpModel.h — Enum과 구조체

```cpp
// AmpModelId enum에 추가
enum class AmpModelId : int {
    // 기존 항목들...
    [신규모델ID] = [다음 순번],
};

// AmpModel 구조체에서 등록할 초기화 코드
{
    AmpModelId::[신규모델ID],
    "[모델명]",
    AmpModelType::[tube|solidState|classD],
    ToneStackType::[TMB|James|Baxandall|Markbass],
    "[캐비닛명]",  // 예: "1x15 Vintage"
    0x[색상코드],  // UI 색상 (16진수 RGB)
    [초기 IR 인덱스],
}
```

### 2. AmpModelLibrary::createModels() 에 추가

```cpp
models.push_back(AmpModel{ /* 위 데이터 */ });
```

### 3. ToneStack::setModel() switch-case 분기

```cpp
case AmpModelId::[신규모델ID]:
    toneStackType = ToneStackType::[TMB|James|Baxandall|Markbass];
    // /ToneStack 커맨드로 구체적 계수 구현 (TODO)
    break;
```

### 4. CMakeLists.txt BinaryData에 추가할 IR 경로

```cmake
juce_add_binary_data(BassMusicGear_BinaryData
    SOURCES
        # 기존 항목...
        Resources/IR/[신규모델]_Cabinet.wav   # 예: Resources/IR/GK800RB_Cabinet.wav
)
```

### 5. 팩토리 프리셋 XML 스텁

**파일 위치:**
- `Resources/Presets/[신규모델]_Clean.xml`
- `Resources/Presets/[신규모델]_Driven.xml`
- `Resources/Presets/[신규모델]_Heavy.xml`

**예시 구조:**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<preset>
  <name>[모델명] - Clean</name>
  <model>[신규모델ID]</model>
  <parameters>
    <!-- APVTS 파라미터 목록 -->
    <!-- input_gain, amp_bass, amp_mid, amp_treble, ... -->
    <!-- 타입별 권장값: -->
    <!--   tube: input_gain=0.3, bass=0.5, mid=0.5, treble=0.5, sag=0.2 -->
    <!--   solid-state: input_gain=0.25, bass=0.4, mid=0.5, treble=0.6 -->
    <!--   class-d: input_gain=0.2, bass=0.5, mid=0.5, treble=0.5 -->
  </parameters>
</preset>
```

---

## 다음 단계 체크리스트

구현 완료 후 반드시 다음을 수행하세요:

- [ ] ToneStack 계수 구현
  - `/ToneStack <topology> <bass> <mid> <treble>` 커맨드로 선택한 토폴로지에 맞춰 `updateCoefficients()` 구현
- [ ] 캐비닛 IR WAV 파일 추가
  - `Resources/IR/[신규모델]_Cabinet.wav` (48kHz 모노, 최대 500ms)
  - 파일 추가 후 `scripts/GenBinaryData.sh` 실행하여 CMakeLists 자동 갱신
- [ ] 프리셋 XML 파라미터 값 조정
  - Clean/Driven/Heavy 각각의 파라미터 값을 모델별 톤 캐릭터에 맞춰 조정
- [ ] 빌드 및 테스트
  - `cmake --build build --config Release`
  - Standalone에서 모델 전환 후 음색 차이 확인
  - 프리셋 로드 후 예상 톤 복현 확인

---

## 참고

**모델 타입별 UI 색상 기본값:**
- Tube: 0xFF8C4513 (주황)
- Solid-State: 0xFF228B22 (녹색)
- Class-D: 0xFF1E90FF (파랑)

**추천 캐비닛 조합:**
- Tube: 1x15 Vintage, 2x10 Modern, 4x10 JBL
- Solid-State: 2x10 Modern, 2x12 British
- Class-D: 1x10 Modern, 2x10 Modern (부스트용)

**ToneStackType 선택 가이드:**
- 따뜻하고 클래식한 톤: TMB (Fender) 또는 James (Marshall)
- 현대적이고 깨끗한 톤: Baxandall (Ampeg) 또는 Markbass (Markbass)
