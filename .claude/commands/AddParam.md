# /AddParam

APVTS 파라미터 추가에 필요한 보일러플레이트 코드를 한 번에 생성한다.

## 입력

$ARGUMENTS

형식: /AddParam <id> <type> [min max default unit]
예시: /AddParam biamp_freq float 60 500 200 Hz
      /AddParam biamp_enabled bool
      /AddParam comp_ratio float 1 20 4

## 역할

입력된 파라미터 정보를 바탕으로 다음 세 가지 코드 스니펫을 생성한다:

1. **ParameterLayout 등록 코드** — PluginProcessor 생성자의 createParameterLayout()에 추가할 코드
2. **UI Attachment 코드** — PluginEditor 멤버 선언 및 생성자 초기화 코드
3. **오디오 스레드 읽기 코드** — processBlock() 또는 DSP 클래스에서 atomic으로 읽는 코드

## 출력 형식

### 1. PluginProcessor — createParameterLayout() 에 추가
```cpp
<AudioParameterFloat 또는 AudioParameterBool 코드>
```

### 2. PluginEditor — 멤버 선언 및 Attachment 초기화
```cpp
<SliderAttachment 또는 ButtonAttachment 선언 및 초기화 코드>
```

### 3. DSP 클래스 — 오디오 스레드에서 읽기
```cpp
<getRawParameterValue + atomic::load() 코드>
```
