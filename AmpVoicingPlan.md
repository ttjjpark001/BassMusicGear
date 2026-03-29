# 앰프 음색 차별화 분석 및 Voicing 구현 계획

작성일: 2026-03-29

---

## 1. 왜 5개 앰프가 비슷하게 들리는가

### 1-1. 현재 구현의 차별화 요소

| 요소 | 현재 상태 | 음색 기여도 |
|------|---------|-----------|
| ToneStack 타입 | 5종 각각 다름 (TMB/Baxandall/James/BaxandallGrunt/MarkbassFourBand) | 낮음~중간 — 노브를 center에 두면 대부분 "거의 플랫"으로 수렴 |
| Preamp 타입 | 3종 (Tube12AX7 / JFET / ClassD) | 낮음 — **Preamp Gain**을 높여야 차이가 남. 클린 레벨에서 거의 동일 |
| PowerAmp | **모든 앰프 동일** | 없음 |
| Cabinet IR | **모든 앰프 동일** (placeholder) | 없음 |
| Amp Voicing | **없음** | 없음 |

### 1-2. Preamp 타입 분포 문제

American Vintage, Tweed Bass, British Stack — 세 모델이 **동일한 Tube12AX7 Preamp**를 사용한다.
Preamp Gain을 아무리 높여도 이 세 모델 사이에는 Preamp 차이가 전혀 없다.

```
Tube12AX7  → American Vintage (Ampeg SVT)
             Tweed Bass (Fender Bassman)      ← 3종 동일
             British Stack (Orange AD200)
JFET       → Modern Micro (Darkglass B3K)
ClassD     → Italian Clean (Markbass)
```

### 1-3. PowerAmp Drive ≠ Preamp 포화

- **PowerAmp Drive**: PowerAmp 모듈을 구동 → 모든 앰프에서 동일한 포화 곡선
- **Preamp Gain**: 각 Preamp 타입(Tube/JFET/ClassD)을 구동 → 타입별 다른 포화 특성

프리앰프 타입 차이를 확인하려면 반드시 **Preamp의 Gain** 노브로 테스트해야 한다.
PowerAmp의 Drive로는 앰프 간 차이를 확인할 수 없다.

### 1-4. 근본 원인 요약

실제 앰프 캐릭터의 구성 비율(추정):
- Cabinet IR: ~50%
- Amp Voicing (회로 고유 주파수 특성): ~30%
- Preamp 포화 특성: ~15%
- PowerAmp 특성: ~5%

현재 구현은 Cabinet IR이 동일하고 Amp Voicing이 없어 **앰프 캐릭터의 80%가 비어 있는** 상태다.

---

## 2. Amp Voicing 이란

각 앰프는 EQ 노브와 무관하게 **회로 자체가 특정 주파수 대역을 강조하거나 감쇠**하는 고유한 특성을 갖는다.
이것을 "Amp Voicing"이라 하며, 사용자가 조절하는 것이 아니라 **앰프 모델 선택 시 자동 적용**되는 고정 필터다.

Voicing은 ToneStack 앞단(Preamp 출력 직후)에 위치하여 앰프의 기본 음색 성격을 확립한다.

---

## 3. 앰프별 Voicing 계획

### 3-1. American Vintage — Ampeg SVT

**캐릭터**: 두툼한 저중역, 따뜻하고 꽉 찬 톤, 클래식 록/펑크 베이스

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | Low Shelf | 80 Hz | +3 dB | — | SVT 특유의 두툼한 저역 |
| V2 | Peak | 300 Hz | +2 dB | 0.8 | 저중역 밀도감 |
| V3 | Peak | 1.5 kHz | -2 dB | 1.2 | 중고역 살짝 후퇴, 따뜻한 인상 |

### 3-2. Tweed Bass — Fender Bassman

**캐릭터**: 빈티지 웜톤, 자연스러운 미드 스쿱, 클래식 R&B/컨트리

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | Low Shelf | 60 Hz | +2 dB | — | 빈티지 저역 풍부함 |
| V2 | Peak | 600 Hz | -3 dB | 0.7 | Bassman 특유의 미드 스쿱 |
| V3 | High Shelf | 5 kHz | -2 dB | — | 고역 자연스러운 롤오프 |

### 3-3. British Stack — Orange AD200

**캐릭터**: 미드 포워드, 공격적, 펀치감, EL34 파워앰프 특성

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | High Pass | 60 Hz | — | 0.7 | 서브 저역 컷, 타이트한 저역 |
| V2 | Peak | 500 Hz | +3 dB | 1.0 | 미드 포워드 성격 |
| V3 | Peak | 2 kHz | +2 dB | 1.5 | 어택감, EL34 어퍼미드 특성 |

### 3-4. Modern Micro — Darkglass B3K

**캐릭터**: 어퍼미드 그릿, 현대 메탈/록, 타이트하고 공격적인 톤

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | High Pass | 80 Hz | — | 0.7 | 서브 저역 컷, 타이트함 |
| V2 | Peak | 900 Hz | +2 dB | 1.2 | B3K 미드 그릿 성격 |
| V3 | Peak | 3 kHz | +4 dB | 1.5 | 어퍼미드 그라인드, B3K 핵심 특성 |

### 3-5. Italian Clean — Markbass Little Mark III

**캐릭터**: 하이파이 클린, 플랫 응답, 펀치감, VPF/VLE가 주요 색깔

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | (flat) | — | 0 dB | — | 의도적으로 플랫 유지 |
| V2 | Peak | 6 kHz | +1.5 dB | 1.5 | 하이파이 클래리티 |

> Markbass의 핵심 색깔은 VPF/VLE 노브로 표현되므로 Voicing은 최소한으로 유지한다.

---

## 4. 추가 개선 사항 — PowerAmp 차별화

Amp Voicing과 함께 PowerAmp도 앰프별로 차별화하면 더 큰 효과를 얻을 수 있다.

| 앰프 | 파워앰프 타입 | 특성 |
|------|------------|------|
| American Vintage | 6550 Tube | 강력, Sag 있지만 비교적 타이트 |
| Tweed Bass | 6L6 Tube | 따뜻한 포화, 이븐 고조파 |
| British Stack | EL34 Tube | 오드 고조파 강조, 미드 어택 |
| Modern Micro | Solid State | Sag 없음, 빠른 트랜지언트 |
| Italian Clean | Class D | 선형, 최대 헤드룸 |

현재 PowerAmp는 모든 앰프에 동일한 모듈이 적용된다.
Amp Voicing 구현 이후 PowerAmp 차별화를 2차 작업으로 진행하는 것을 권장한다.

---

## 5. 구현 방법

### 5-1. 아키텍처

```
신호 흐름:
  Preamp → [AmpVoicing] → ToneStack → PowerAmp → Cabinet
               ↑
          앰프 선택 시 자동 적용되는 고정 필터 (사용자 조작 불가)
```

### 5-2. 필요한 코드 변경

1. **`Source/DSP/AmpVoicing.h/.cpp`** (신규)
   - `juce::dsp::ProcessorChain` 또는 수동 바이쿼드 필터 2~3개
   - `setModel(AmpModelType)` 호출 시 해당 모델의 필터 계수 로드
   - `prepareToPlay` / `processBlock` RT-safe 구현

2. **`Source/Models/AmpModel.h`** (수정)
   - `struct VoicingBand { float freq, gainDb, q; FilterType type; }` 추가
   - `std::array<VoicingBand, 3> voicingBands` 필드 추가

3. **`Source/Models/AmpModelLibrary.cpp`** (수정)
   - 5종 모델에 위 3-1~3-5 표의 Voicing 값 등록

4. **`Source/DSP/SignalChain.h/.cpp`** (수정)
   - `AmpVoicing` 모듈을 Preamp와 ToneStack 사이에 삽입
   - 앰프 모델 전환 시 `ampVoicing.setModel(model)` 호출

### 5-3. 테스트 기준

- 각 앰프의 Voicing 필터를 화이트 노이즈 또는 임펄스에 통과시켜 주파수 응답 확인
- Cabinet bypass + ToneStack flat 상태에서 5개 앰프의 주파수 응답이 서로 다르게 나타남을 확인
- Preamp Gain을 낮게 유지한 상태에서도 앰프 간 음색 차이가 청취로 구분 가능함을 확인

---

## 6. 적용 Phase — **Phase 6 확정**

PLAN.md Phase 6 작업량 분석 결과 Phase 6 구현으로 결정.

**결정 이유:**
- Phase 6에서 어차피 `SignalChain`을 대규모 수정 → AmpVoicing 삽입을 같은 Pass에 처리하면 SignalChain을 두 번 건드리는 낭비 없음
- Phase 9는 릴리즈 준비 Phase로 이미 작업량이 많음 (SettingsPage + 전체 테스트 + 배포)
- Phase 4(Pre-FX), Phase 5(Post-FX)는 아직 미완이므로 Phase 6 적용 시 이 두 Phase의 테스트에도 음색 차별화 효과 없음 → Phase 6이 현실적 최조 시점

**PLAN.md 반영 완료**: Phase 6 ✅ CARRY 및 P0 항목에 추가됨.
**BackLog.md 반영 완료**: 처리 예정 Phase를 "Phase 6"으로 확정.

---

## 7. 관련 파일

- `BackLog.md` — 이슈 추적
- `PLAN.md` — Phase별 작업 계획
- `Source/Models/AmpModel.h` — 앰프 모델 데이터 구조
- `Source/Models/AmpModelLibrary.cpp` — 5종 앰프 등록
- `Source/DSP/SignalChain.cpp` — 신호 체인 조립
