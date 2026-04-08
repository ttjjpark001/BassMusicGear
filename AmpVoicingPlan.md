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
| V1 | Low Shelf | 80 Hz | +6 dB | — | SVT 특유의 두툼한 저역 |
| V2 | Peak | 300 Hz | +3 dB | 0.8 | 저중역 밀도감 |
| V3 | Peak | 1.5 kHz | -4 dB | 1.2 | 중고역 살짝 후퇴, 따뜻한 인상 |

### 3-2. Tweed Bass — Fender Bassman

**캐릭터**: 빈티지 웜톤, 자연스러운 미드 스쿱, 클래식 R&B/컨트리

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | Low Shelf | 60 Hz | +4 dB | — | 빈티지 저역 풍부함 |
| V2 | Peak | 600 Hz | -6 dB | 0.7 | Bassman 특유의 미드 스쿱 |
| V3 | High Shelf | 5 kHz | -4 dB | — | 고역 자연스러운 롤오프 |

### 3-3. British Stack — Orange AD200

**캐릭터**: 미드 포워드, 공격적, 펀치감, EL34 파워앰프 특성

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | High Pass | 60 Hz | — | 0.7 | 서브 저역 컷, 타이트한 저역 |
| V2 | Peak | 500 Hz | +6 dB | 1.0 | 미드 포워드 성격 |
| V3 | Peak | 2 kHz | +4 dB | 1.5 | 어택감, EL34 어퍼미드 특성 |

### 3-4. Modern Micro — Darkglass B3K

**캐릭터**: 어퍼미드 그릿, 현대 메탈/록, 타이트하고 공격적인 톤

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | High Pass | 80 Hz | — | 0.7 | 서브 저역 컷, 타이트함 |
| V2 | Peak | 900 Hz | +4 dB | 1.2 | B3K 미드 그릿 성격 |
| V3 | Peak | 3 kHz | +8 dB | 1.5 | 어퍼미드 그라인드, B3K 핵심 특성 |

### 3-5. Italian Clean — Markbass Little Mark III

**캐릭터**: 하이파이 클린, 플랫 응답, 펀치감, VPF/VLE가 주요 색깔

| 필터 | 타입 | 주파수 | 게인 | Q | 설명 |
|------|------|--------|-----|---|------|
| V1 | (flat) | — | 0 dB | — | 의도적으로 플랫 유지 |
| V2 | Peak | 6 kHz | +3 dB | 1.5 | 하이파이 클래리티 |

> Markbass의 핵심 색깔은 VPF/VLE 노브로 표현되므로 Voicing은 최소한으로 유지한다.
>
> **값 강화 노트 (Phase 6 구현 시)**: 위 표의 게인 값은 실제 회로 측정 데이터가 아닌 청취 기반 추정값이다. 최초 설계 대비 약 2배로 강화했으며, Phase 6 구현·테스트 과정에서 ToneStack Flat 상태에서도 앰프 간 음색 차별화가 청감상 명확히 들리도록 조정한 결과이다. 실제 AmpModelLibrary.cpp의 `voicingBands`가 이 표와 동기화되어 있다.

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

---

## 8. 참고 — 프리앰프 타입 종류

베이스 앰프에서 사용되는 프리앰프 회로 토폴로지 정리. 향후 앰프 모델 추가 시 참고.

### 현재 구현된 타입 (Phase 1~2)

| 타입 | 포화 특성 | 고조파 특성 | 대표 앰프 | 사용 모델 |
|------|---------|-----------|---------|---------|
| **Tube (12AX7/ECC83)** | 부드러운 소프트클리핑, 점진적 포화 | 짝수 고조파 강조 → 따뜻하고 풍부한 음색 | Ampeg SVT, Orange AD200, Fender Bassman | American Vintage / Tweed Bass / British Stack |
| **JFET** | 튜브와 유사한 비대칭 클리핑, 패러렐 클린 블렌드 구조 | 튜브보다 짝수 고조파 적음, 타이트하고 그릿 있는 드라이브 | Darkglass B3K/B7K, SansAmp Bass Driver | Modern Micro |
| **Class D (선형)** | 왜곡 없음, 최대 헤드룸 유지 | 고조파 발생 없음 — 완전 클린 | Markbass Little Mark III, TC Electronic | Italian Clean |

> **테스트 주의**: 앰프 간 Preamp 타입 차이는 **Preamp Gain 노브**를 높여야 청취로 구분 가능.
> PowerAmp의 Drive 노브는 모든 앰프에서 동일한 PowerAmp 모듈을 구동하므로 앰프 간 차이가 없다.

### 추가 구현 후보 타입 (Post-MVP 앰프 모델 추가 시)

| 타입 | 포화 특성 | 고조파 특성 | 대표 앰프 | 비고 |
|------|---------|-----------|---------|-----|
| **MOSFET** | 부드러운 소프트클리핑, Class D보다 따뜻함 | 튜브에 가깝지만 더 클린, 미세한 짝수 고조파 | Hartke LH500/LH1000, 일부 GK 모델 | 현재 미구현 타입 중 가장 독자적 캐릭터 |
| **BJT (Bipolar Junction Transistor)** | 딱딱한 하드클리핑 경향, 트랜지스터 특유의 클리핑 | 홀수 고조파 강조 → 공격적이고 날카로운 드라이브 | Peavey Mark 시리즈, 입문/중급 앰프 다수 | 가장 흔한 솔리드스테이트 타입 |
| **Op-Amp (연산증폭기)** | 높은 헤드룸, Class D와 유사하지만 능동 EQ 회로와 결합 | 헤드룸 내에서는 선형, 클리핑 시 딱딱한 특성 | Aguilar Tone Hammer, MXR Bass DI+, EBS MicroBass | 페달보드 프리앰프/DI 박스에 많이 사용 |

### 우선순위 추천 (Post-MVP 앰프 추가 순서)

1. **MOSFET** — JFET/Tube와 명확히 구분되는 독자적 캐릭터. Hartke 계열 앰프 모델 추가 시 사용 추천
2. **BJT** — 가장 흔한 솔리드스테이트이지만 현재 JFET으로 어느 정도 커버됨. 낮은 우선순위
3. **Op-Amp** — Aguilar Tone Hammer 모델 추가 시 사용. AGS(Aguilar Gain Stage) 회로 특성과 함께 구현

---

## 9. 참고 — 톤스택 타입 종류

베이스 앰프에서 사용되는 톤스택(EQ) 회로 토폴로지 정리. 향후 앰프 모델 추가 시 참고.

### 현재 구현된 타입 (Phase 2)

| 타입 | 구현 방식 | 컨트롤 특성 | 대표 앰프 | 사용 모델 |
|------|---------|-----------|---------|---------|
| **Fender TMB** | 수동 RC 네트워크 전달 함수 이산화 (Yeh 2006) | Bass/Mid/Treble 세 컨트롤이 서로 상호작용. 중간값에서 자연스러운 미드 스쿱. 독립 필터로 구현 금지 | Fender Bassman 5F6-A, Fender Bassman 100 | Tweed Bass |
| **James** | 독립 셸빙 바이쿼드 2개 + 미드 피킹 1개 | Bass/Treble 완전 독립 (서로 영향 없음). 셸빙 특성으로 자연스러운 곡선 | Orange AD200, Hiwatt 계열 | British Stack |
| **Active Baxandall** | 피킹/셸빙 바이쿼드 조합 (능동 회로) | 능동 증폭으로 더 큰 부스트/컷 범위. 중간값에서 비교적 평탄 | Ampeg SVT (미드 포지션 스위치 포함) | American Vintage |
| **BaxandallGrunt** | Active Baxandall + Grunt(저중역 드라이브) + Attack(고역 드라이브) 피킹 필터 추가 | Baxandall 기반에 드라이브 캐릭터 필터 2개 추가. 모던 메탈 성격 강화 | Darkglass B3K/B7K | Modern Micro |
| **Markbass 4-band + VPF/VLE** | 4개 독립 바이쿼드(40/360/800/10kHz) + VPF 합성 필터 + VLE 로우패스 | 4밴드 완전 독립. VPF는 3개 필터 동시 적용(35Hz 부스트 + 380Hz 노치 + 10kHz 부스트). VLE는 가변 로우패스 | Markbass Little Mark III | Italian Clean |

### 추가 구현 후보 타입 (Post-MVP 앰프 모델 추가 시)

| 타입 | 구현 방식 | 컨트롤 특성 | 대표 앰프 | 비고 |
|------|---------|-----------|---------|-----|
| **Marshall 스타일 (Passive TMB 변형)** | Fender TMB와 동일한 RC 네트워크 이산화, 단 컴포넌트 값이 달라 중간값에서 더 깊은 미드 스쿱 발생 | Fender TMB보다 미드 스쿱이 강함. 기타 앰프에서 유래했으나 일부 베이스 앰프에도 적용 | Marshall Super Bass, 일부 빈티지 베이스 앰프 | Fender TMB 코드에 컴포넌트 값만 변경하면 구현 가능 |
| **세미 파라메트릭 EQ** | Bass/Treble 셸빙 고정 + Mid는 Freq(스위프) + Level 두 컨트롤 | 미드 주파수를 사용자가 직접 선택 가능. 픽업 공명 대역 정밀 조정에 유용 | GK 800RB/1001RB, Mesa Subway D-800, Eden WT 시리즈 | GK 계열 앰프 모델 추가 시 사용. Freq 노브 = 바이쿼드 피킹 필터의 중심 주파수를 실시간 변경 |
| **Aguilar OBP-3 스타일** | 저역 셸빙(40Hz) + 스위프 미드 피킹(400~800Hz) + 고역 셸빙(6.5kHz 또는 10kHz 전환) | 온보드 베이스 프리앰프에서 가장 인기 있는 3밴드. 미드 스위프 범위가 넓어 다양한 장르 대응 | Aguilar OBP-3 (베이스 내장 프리앰프), Aguilar Tone Hammer | Aguilar 앰프/페달 모델 추가 시 사용. Post-MVP 확장 앰프 중 Aguilar Tone Hammer에 해당 |
| **GK 4밴드 독립 능동 EQ** | 4개 피킹/셸빙 바이쿼드 완전 독립 (60Hz / 250Hz / 1kHz / 4kHz) | 4밴드 모두 완전 독립, 상호작용 없음. 각 밴드 ±15dB. 정밀하고 예측 가능한 음색 조정 | Gallien-Krueger 800RB, 1001RB-II | GK 계열 앰프 모델 추가 시 사용. 현재 Italian Clean과 구조 유사하나 주파수/범위 다름 |
| **Passive 단일 Tone 컨트롤** | 가변 커패시터 기반 고역 컷 (단일 RC 로우패스) | 노브 하나로 고역만 컷. 부스트 불가. 빈티지 앰프에 많음 | Ampeg B-15, 일부 소형 빈티지 앰프 | 구현 매우 간단. B-15 스튜디오 빈티지 모델 추가 시 사용 가능 |

### 톤스택 선택 가이드 (Post-MVP 앰프 추가 시)

| 장르 / 캐릭터 | 추천 톤스택 |
|-------------|-----------|
| 클래식 록 / 빈티지 | Fender TMB 또는 Marshall 변형 |
| 재즈 / 스튜디오 | Passive 단일 Tone 또는 Aguilar OBP-3 |
| 모던 메탈 / 슬랩 | Semi-parametric 또는 BaxandallGrunt |
| 펑크 / R&B | Aguilar OBP-3 또는 GK 4밴드 |
| 하이파이 / 투명한 클린 | GK 4밴드 독립 또는 Markbass 4-band |

---

## 10. 참고 — Post-MVP 앰프 모델 선정 분석

PRD.md에 기재된 Post-MVP 4종 앰프를 프리앰프 타입 및 톤스택 타입 다양성 기준으로 평가.

### 회로 분류표

| 앰프 | 프리앰프 타입 | 톤스택 타입 | 프리앰프 신규 여부 | 톤스택 신규 여부 |
|------|------------|-----------|----------------|--------------|
| **Ampeg B-15** | Tube (12AX7) | Passive 단일 Tone 컨트롤 | 🔴 중복 (MVP에 이미 3종) | 🟡 신규이나 매우 단순 |
| **GK 800RB** | MOSFET | GK 4밴드 독립 능동 EQ | 🟢 신규 | 🟢 신규 |
| **Mesa Subway D-800** | Class D | 세미 파라메트릭 EQ | 🔴 중복 (Italian Clean과 동일) | 🟢 신규 |
| **Aguilar Tone Hammer** | Op-Amp (메인 경로) + JFET AGS 드라이브 스테이지 | Aguilar OBP-3 (스위프 미드) | 🟢 신규 | 🟢 신규 |

> **Aguilar Tone Hammer 분류 주의**: PRD.md에 "JFET + AGS 회로"로 기재되어 있으나, 이는 AGS(Aguilar Gain Stage) 드라이브 스테이지에 초점을 맞춘 표현임. 메인 프리앰프 경로는 Op-Amp 기반이므로 프리앰프 타입 분류 기준으로는 **Op-Amp**이 정확하다.

### 모델별 평가

**GK 800RB — 가장 잘 선정된 모델**
프리앰프(MOSFET)와 톤스택(GK 4밴드) 모두 MVP에 없는 신규 타입. 회로 다양성 측면에서 가장 기여도 높음.

**Aguilar Tone Hammer — 잘 선정됨**
톤스택(OBP-3 스위프 미드)은 신규. 프리앰프도 Op-Amp으로 신규 타입. AGS 드라이브 스테이지가 JFET 특성을 추가하여 Modern Micro(Darkglass B3K)와 다른 독자적 드라이브 성격을 가짐. GK 800RB와 함께 MVP에 없는 프리앰프 타입(MOSFET, Op-Amp)을 모두 커버.

**Mesa Subway D-800 — 아쉬운 선택**
세미 파라메트릭 톤스택은 신규라 가치 있음. 단 프리앰프가 Class D로 Italian Clean(Markbass)과 동일 카테고리. 두 앰프 모두 클린/하이파이 성격이라 사용자가 차이를 느끼기 어려울 수 있음.

**Ampeg B-15 — 가장 약한 선택 (단, 감성적 가치는 별도)**
프리앰프(Tube 12AX7)가 MVP 3종과 중복. Passive 단일 Tone 컨트롤은 신규이나 고역 컷 하나뿐으로 너무 단순. 차별점이 Voicing/IR에서만 오므로 회로 구조 다양성 기여가 적음.

단, B-15는 1960~70년대 모타운·재즈·R&B 레코딩의 상징적인 스튜디오 앰프로, 회로 구조 다양성과 무관하게 수요가 있는 모델이다. James Jamerson(모타운), Paul McCartney(Beatles 초기) 등 전설적인 베이스 톤이 이 앰프에서 나왔다. 따라서 B-15는 "회로 다양성" 관점이 아닌 "스튜디오 빈티지 톤 재현" 관점에서 추가 가치가 있으며, 우선순위는 낮더라도 제거보다는 후순위로 유지하는 것이 적절하다.

### MVP + Post-MVP 전체 프리앰프 타입 커버리지

| 프리앰프 타입 | MVP 커버 | Post-MVP 커버 | 담당 모델 |
|------------|---------|-------------|---------|
| Tube (12AX7) | ✅ | (중복) | American Vintage / Tweed Bass / British Stack / B-15 |
| JFET | ✅ | (중복) | Modern Micro |
| Class D | ✅ | (중복) | Italian Clean / Mesa D-800 |
| MOSFET | ❌ | ✅ | GK 800RB |
| Op-Amp | ❌ | ✅ | Aguilar Tone Hammer |
| BJT | ❌ | ❌ | 미커버 |

BJT는 대중적이나 현재 JFET/Tube로 어느 정도 커버 가능하여 우선순위 낮음.

### 추가 검토 후보 (현재 Post-MVP 명단에 없는 모델)

현재 Post-MVP 4종 중 Mesa D-800(Class D 중복)이나 B-15(Tube 중복) 자리에 아래 모델을 고려할 수 있음.

| 앰프 | 프리앰프 | 톤스택 | 추천 이유 |
|-----|---------|------|---------|
| **Eden WT-800** | Op-Amp | 세미 파라메트릭 4밴드 | Op-Amp 신규 + 톤스택 신규. Mesa D-800 대체 후보. 재즈/스튜디오 장르 커버 |
| **Hartke LH1000** | MOSFET | GK형 4밴드 | MOSFET 신규 (GK 800RB와 같은 타입이나 캐릭터 다름). GK와 중복 가능성 있어 낮은 우선순위 |

> 단, Eden과 Hartke 추가는 선택적. 현재 Post-MVP 4종(GK + Aguilar)이 MOSFET과 Op-Amp을 이미 커버하므로, 실제 추가 필요성은 장르/사용자 수요 기준으로 판단할 것.

---

## 11. 참고 — BJT 타입 베이스 앰프

현재 MVP 및 Post-MVP 명단에 BJT 타입 앰프가 없어 미커버 상태인 타입. 향후 추가 검토 시 참고.

### 대표 BJT 베이스 앰프

| 앰프 | 시대 | 특징 | 사용 아티스트 |
|------|------|------|------------|
| **Acoustic 360/370** | 1960s~70s | 최초의 본격 베이스 전용 서브우퍼 시스템. 독특한 Variamp EQ(가변 주파수 피킹 회로). BJT 앰프 중 가장 개성 있는 톤 | Jaco Pastorius, Larry Graham, John Paul Jones |
| **Peavey Mark III/IV/VI** | 1970s~80s | 미국 중소도시 밴드의 표준 장비. 저렴하고 튼튼해서 대중적으로 보급됨. 클린하고 무색무취한 톤 | 수많은 아마추어/세미프로 베이시스트 |
| **Sunn Coliseum Bass** | 1970s | Sunn의 솔리드스테이트 라인. 강력한 출력, 클린 헤드룸 | 당시 헤비록 베이시스트들 |
| **Ampeg SS 시리즈** | 1970s~80s | Ampeg가 튜브 외에 출시한 솔리드스테이트 라인. 튜브 SVT의 저렴한 대안으로 포지셔닝 | — |

### BJT 타입의 음색 특성

- **클리핑 전**: 매우 클린하고 투명. Op-Amp과 유사
- **클리핑 시**: 홀수 고조파 강조, 딱딱하고 날카로운 드라이브. 튜브나 MOSFET처럼 자연스럽게 포화되지 않고 갑자기 찌그러지는 경향
- 이 때문에 "차갑다", "임상적이다"는 평가를 받는 경우가 많음

**예외 — Acoustic 360**: BJT임에도 불구하고 독특한 Variamp EQ 회로와 폴디드 혼 캐비닛 덕분에 매우 개성 있는 톤으로 사랑받음. Jaco Pastorius의 Weather Report 시절 톤이 바로 이 앰프에서 나왔음.

### Post-MVP 추가 후보로서의 BJT

순수 BJT 앰프를 모델로 추가한다면 **Acoustic 360**이 가장 적합:

- 독자적인 Variamp EQ — 세미 파라메트릭의 전신 개념으로 톤스택도 차별화 가능
- 전설적인 아티스트 연관성 (Jaco Pastorius)
- 현재 어떤 MVP/Post-MVP 모델도 커버하지 않는 1960~70년대 재즈·펑크 베이스 톤
- 회로 타입 기준으로도 BJT 유일 모델 → 프리앰프 타입 커버리지 완성

---

## 12. 참고 — 앰프 모델별 장르 적합성

### MVP 5종

| 앰프 모델 | 원본 참고 | 주력 장르 | 보조 장르 | 대표 아티스트 |
|---------|---------|---------|---------|------------|
| **American Vintage** | Ampeg SVT | 클래식 록, 하드록, 펑크 | 헤비메탈, R&B, 모타운 | Geddy Lee (Rush), Cliff Burton (Metallica), John Entwistle (The Who) |
| **Tweed Bass** | Fender Bassman | 컨트리, 블루스, 빈티지 록 | 재즈, R&B, 50~60년대 팝 | 수많은 50~60년대 스튜디오/라이브 베이시스트 |
| **British Stack** | Orange AD200 | 스토너 록, 둠 메탈, 헤비록 | 프로그레시브 록, 슬러지 메탈 | 많은 헤비록/둠 메탈 베이시스트 |
| **Modern Micro** | Darkglass B3K | 모던 메탈, 프로그레시브 메탈 | 테크니컬 메탈, 데스 메탈, 모던 록 | Adam Nolly Getgood, 현대 메탈 세션 베이시스트 다수 |
| **Italian Clean** | Markbass Little Mark III | 재즈, 펑크, 슬랩 | 팝, 네오소울, 현대 R&B | 많은 재즈·팝 세션 베이시스트 |

**MVP 장르 커버리지 요약:**
- 빈티지 록/클래식 록 ✅ (American Vintage, Tweed Bass)
- 헤비/메탈 ✅ (British Stack, Modern Micro)
- 재즈/펑크/클린 ✅ (Italian Clean)
- 스튜디오 R&B/소울 🟡 (American Vintage로 부분 커버, 전용 모델 없음)
- 가스펠/네오소울 🟡 (Italian Clean으로 부분 커버)

---

### Post-MVP 4종

| 앰프 모델 | 원본 참고 | 주력 장르 | 보조 장르 | 대표 아티스트 |
|---------|---------|---------|---------|------------|
| **Ampeg B-15** | Ampeg B-15N | 모타운, 소울, 스튜디오 R&B | 재즈, 60~70년대 팝 | James Jamerson (Motown), Paul McCartney (Beatles 초기) |
| **GK 800RB** | Gallien-Krueger 800RB | 슬랩, 펑크, 재즈 퓨전 | 컨트리, 팝, 세션 올라운더 | Flea (RHCP 일부), 다수의 세션 베이시스트 |
| **Mesa Subway D-800** | Mesa Boogie Subway D-800 | 재즈, 블루스, 소울 | 팝, 아메리카나, 세션 올라운더 | 다양한 장르 세션 뮤지션 |
| **Aguilar Tone Hammer** | Aguilar Tone Hammer 500 | 가스펠, R&B, 네오소울 | 펑크, 재즈, 소울 | 가스펠·R&B 베이시스트 다수 (Anthony Crawford 등) |

**Post-MVP가 채우는 빈틈:**
- 스튜디오 빈티지 R&B/소울 ✅ B-15 (MVP에 없던 장르)
- 타이트한 슬랩/펑크 ✅ GK 800RB (Italian Clean보다 더 타이트한 클린)
- 가스펠/네오소울 ✅ Aguilar Tone Hammer (MVP에 없던 장르)
- 재즈·블루스 정밀 EQ ✅ Mesa D-800 (세미 파라메트릭으로 세밀한 주파수 조각)

---

### 전체 장르 → 앰프 매핑표

| 장르 | 1순위 | 2순위 |
|------|------|------|
| 클래식 록 / 하드록 | American Vintage | Tweed Bass |
| 컨트리 / 블루스 | Tweed Bass | GK 800RB |
| 스토너 록 / 둠 메탈 | British Stack | American Vintage |
| 모던 메탈 / 프로그레시브 메탈 | Modern Micro | British Stack |
| 재즈 | Italian Clean | Ampeg B-15 / Mesa D-800 |
| 펑크 / 슬랩 | GK 800RB | Italian Clean |
| 모타운 / 스튜디오 R&B | Ampeg B-15 | American Vintage |
| 가스펠 / 네오소울 | Aguilar Tone Hammer | Italian Clean |
| 소울 / 블루스 | Mesa D-800 | Ampeg B-15 |
| 팝 / 세션 올라운더 | Italian Clean | GK 800RB |
