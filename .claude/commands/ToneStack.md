# /ToneStack

톤스택 토폴로지와 컨트롤 값을 입력하면 IIR 계수 유도 과정을 안내하고 `updateCoefficients()` 구현 스텁을 생성한다.

## 입력

$ARGUMENTS

형식: /ToneStack <topology> <bass> <mid> <treble>
topology: TMB | James | Baxandall | Markbass
예시: /ToneStack TMB 0.5 0.5 0.5
      /ToneStack James 1.0 0.5 0.0
      /ToneStack Markbass 0.5 0.5 0.5

## 역할

선택된 토폴로지의 아날로그 전달함수 H(s) 유도 과정을 설명하고, Bilinear transform을 이용해 H(z)로 이산화한 후, 입력된 bass/mid/treble 값에 맞춰 계수를 계산하는 C++ 코드를 생성한다.

### 톤스택 토폴로지별 안내

**1. Fender Tweed Bass (TMB)**
- Yeh 2006 CCRMA 논문 방식: 세 컨트롤(Bass/Mid/Treble) RC 네트워크의 상호작용을 전달함수로 모델링
- H(s) 계산 단계:
  1. Bass 포지션별 RC 네트워크 임피던스 Z_B(s)
  2. Mid 포지션별 임피던스 Z_M(s)
  3. Treble 포지션별 임피던스 Z_T(s)
  4. 전체 전달함수 H(s) = V_out / V_in (세 네트워크 상호작용)
- Bilinear transform: 워핑 주파수 적용.
  - 샘플레이트 44.1kHz 기준, 각 극점·영점 s → z 변환
  - 얻어진 b0~b2 (분자), a0~a2 (분모) 계수

**2. James (British Stack)**
- 이름: James(Marshall 가 개발 패턴)
- 토폴로지: Bass/Treble 독립 셸빙 필터 2개 + Mid 피킹 바이쿼드 1개
- H(s) 계산 단계:
  1. Bass 셸빙: 낮은 주파수 부스트/컷 (S-plane pole/zero)
  2. Mid 피킹: Q 값 가변, 중역대 노치/피크 (쌍극점)
  3. Treble 셸빙: 높은 주파수 부스트/컷
  4. 3개 필터의 직렬 연쇄: H(s) = H_Bass(s) × H_Mid(s) × H_Treble(s)
- 각 필터별 bilinear transform 후 3개 2차 필터(biquad) 직렬 구성

**3. Baxandall (Active, Modern Micro)**
- 토폴로지: 4-band 독립 피킹(중역)/셸빙(저역,고역)
- 표준 Active Baxandall 회로: 4개의 가변 필터 병렬 합산
  - 40Hz 저역 셸빙
  - 250Hz 저역 중역 피킹
  - 2kHz 고역 중역 피킹
  - 10kHz 고역 셸빙
- 각 필터가 독립적 Q 값과 gain 값을 제어
- Bilinear로 4개 biquad 계산, 병렬 합산 후 정규화

**4. Markbass (Italian Clean: 4-band + VPF + VLE)**
- 토폴로지: 4개 독립 피킹/셸빙 바이쿼드 + VPF(3필터 합산) + VLE(로우패스)
  - **4-band**: 40Hz, 360Hz, 800Hz, 10kHz 각각 독립 control
  - **VPF (Variable Peak Filter)**: 3개 필터의 합산으로 구성
    - ① 35Hz 셸빙 부스트 (저역 확장)
    - ② 380Hz 노치(컷) (미드 스쿱)
    - ③ 10kHz 셸빙 부스트 (고역 확장)
    - VPF 노브 값: 0(off) ~ 1.0(max). 각 필터의 gain을 VPF 노브 값에 선형 비례로 조절
  - **VLE (Variable Low-End)**: juce::dsp::StateVariableTPTFilter를 로우패스 모드로 사용
    - 컷오프 주파수: VLE 노브 값에 따라 20kHz(0) → 4kHz(max)로 매핑
    - 기울기: 6dB/oct (-1 pole)
  - 배치 순서: 4-band EQ → VPF → VLE (직렬)

## 출력 형식

### 1. 전달함수 계산 과정 (ASCII 수식, LaTeX 없음)

```
[토폴로지명] 전달함수 계산:

주파수 응답:
H(jω) = ...

극점(Poles):
p1 = ..., p2 = ..., ...

영점(Zeros):
z1 = ..., z2 = ..., ...
```

### 2. Bilinear Transform 이산화

```
샘플레이트: fs = 44100 Hz
워핑 기준: ωc (특성 주파수)

S → Z 변환:
s = 2*fs * (z-1)/(z+1)

결과:
b0 = ..., b1 = ..., b2 = ...
a0 = ..., a1 = ..., a2 = ...

정규화 (a0 = 1로):
b0' = b0/a0, b1' = b1/a0, b2' = b2/a0
a1' = a1/a0, a2' = a2/a0
```

### 3. updateCoefficients() C++ 구현 스텁

```cpp
void ToneStack::updateCoefficients(float bass, float mid, float treble)
{
    // 입력 범위 정규화 (0.0~1.0 → 필요시 다른 범위로 매핑)
    float b = bass;    // 0.0 = bass cut, 1.0 = bass boost
    float m = mid;     // 0.0 = mid cut, 1.0 = mid boost
    float t = treble;  // 0.0 = treble cut, 1.0 = treble boost

    // [토폴로지별 계수 계산]
    // 예: TMB의 경우 Bass/Mid/Treble RC 네트워크 상호작용 반영

    // 극점·영점 → 전달함수 H(s)
    // ...

    // Bilinear transform → H(z) 계수 b0~b2, a0~a2
    // ...

    // IIR 필터 계수 업데이트 (정규화: a0 = 1)
    coefficients[0] = {b0_norm, b1_norm, b2_norm, 1.0f, a1_norm, a2_norm};
    // (이 코드는 메인 스레드 전용. processBlock()에서는 호출 금지)
}
```

### 4. 마지막에 노트

```
⚠️  계수 재계산은 반드시 메인 스레드(prepareToPlay, 파라미터 change callback)에서만 수행하세요.
    processBlock()에서는 이미 계산된 계수를 읽기만 하거나, std::atomic 변수로 스왑해서 사용하세요.
```

---

## 참고

- **Yeh et al. 2006**: "Matched Nonlinear Controls for Musical Audio Signal Processing" (CCRMA)
- **Bilinear Transform**: s = 2*fs*(z-1)/(z+1)
- **IIR Biquad 필터**: y[n] = (b0*x[n] + b1*x[n-1] + b2*x[n-2]) - (a1*y[n-1] + a2*y[n-2])
