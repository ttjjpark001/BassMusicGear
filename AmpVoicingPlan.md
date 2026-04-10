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

---

## 13. 향후 계획

본 장은 **Phase 10까지 MVP를 완성한 이후** 재검토할 진짜 "향후 계획"이다. 현재 Phase 7(이월 작업 정리) 및 그 이후의 계획된 일정과는 무관하다.

앰프 음색 차별화와 실물 앰프 재현에 대해 세 가지 방향을 검토 중이며, 상호 배타적이지 않고 조합 적용도 가능하다.

- **13-0. AmpVoicing 원래 값 재테스트** — PowerAmp 포화 도입 후 강화된 Voicing 값 적정성 재검토
- **13-1. 캐리커처 방향 재설계** — 실물 앰프 캐리커처를 Voicing 과장으로 재현
- **13-2. 장르별 앰프 구현** — 앰프 모델을 음악 장르 중심으로 재구성
- **13-3. NAM 모델 로딩으로 실물 앰프 구현** — 실물 앰프 특성은 NAM 추론에 위임

---

### 13-0. PowerAmp 포화 도입 후 AmpVoicing 원래 값 재테스트

**기록일**: 2026-04-09

#### 배경

Phase 6에서 AmpVoicing을 처음 적용했을 때, 앰프 간 음색 차이가 잘 느껴지지 않아 Voicing 필터 계수를 강화하여 적용했다. 이후 Phase 7에서 PowerAmp 앰프별 포화 차별화(Tube6550/TubeEL34/SolidState/ClassD 4종 분기 + 4x 오버샘플링)가 구현되었고, 실제 청취 시 앰프 간 음색 차이가 비로소 느껴지기 시작했다.

#### 현재 상태

- **AmpVoicing**: Phase 6에서 강화된 값 적용 중
- **PowerAmp**: Phase 7에서 4종 포화 곡선 분기 구현 완료
- **청취 결과**: PowerAmp 포화가 더해지니 앰프 간 음색 차이가 명확히 느껴짐

#### 재테스트 필요성

PowerAmp 포화가 없던 상태에서는 AmpVoicing 값을 강하게 올려야 차이가 들렸다. 이제 PowerAmp 포화까지 더해진 최종 상태에서는 강화된 AmpVoicing 값이 **과도하게 느껴질 수 있다**.

**Phase 10 완료 후 다음을 테스트한다:**

1. 현재 강화된 AmpVoicing 값 → 전체 신호 체인(AmpVoicing + PowerAmp 포화 + Cabinet IR) 상태에서 청취
2. Phase 6 최초 적용 시의 원래(약한) AmpVoicing 값으로 되돌린 후 동일 조건으로 청취
3. A/B 비교 후 최적값 결정:
   - 원래 값으로도 충분히 구분된다면 → 원래 값으로 복원
   - 강화된 값이 더 자연스럽다면 → 현재 값 유지
   - 중간값이 더 좋다면 → 조정

#### 관련 파일

- `Source/DSP/AmpVoicing.cpp` — 현재 강화된 voicingBands 계수 적용
- `Source/Models/AmpModelLibrary.cpp` — 앰프별 voicingBands 배열 정의
- `AmpVoicingPlan.md` 섹션 3 — 최초 설계값 / 섹션 13-1 — 캐리커처 방향 재설계 검토값

---

### 13-1. 캐리커처 방향 재설계 (검토 중)

**배경**: 실물 앰프의 정확한 주파수 응답 측정 데이터를 확보하기 어렵고, 본 프로젝트는 실물 앰프명을 사용하지 않는 방향으로 진행 중이다. 따라서 "리얼리즘 복각"이 아닌 **"각 앰프 캐릭터의 캐리커처(과장된 시그니처)"** 방향으로 Voicing을 재설계하는 방안을 검토한다.

**목표**: 사용자가 앰프 모델을 전환하는 순간 즉시 캐릭터 차이를 인지할 수 있도록, 각 모델의 대표 특성을 과장해서 구현한다. 플랫 상태에서도 "이 앰프는 분명히 저역이 두껍다" / "이 앰프는 미드가 사납다" 같은 인상이 명확해야 한다.

#### 13-1-1. 재설계 방향 표

| 앰프 | 캐릭터 한 줄 | Voicing 방향 (검토 중) |
|------|----------|----------------------|
| **American Vintage** | 두툼한 저역 + 따뜻한 중역 | 80Hz **+9dB** shelf / 300Hz **+5dB** peak (Q 0.8) / 1.5kHz **−6dB** peak (Q 1.2) |
| **Tweed Bass** | 빈티지 미드 스쿱 + 부드러운 고역 | 60Hz **+6dB** shelf / 600Hz **−10dB** peak (Q 0.5~0.7, 넓게 파임) / 5kHz **−7dB** shelf |
| **British Stack** | 공격적 미드 포워드 + 어택 | 60Hz HP / 500Hz **+10dB** peak (Q 1.0) / 2kHz **+7dB** peak (Q 1.5) |
| **Modern Micro** | 그라인드, 타이트, 어퍼미드 날카로움 | 80Hz HP / 900Hz **+7dB** peak (Q 1.2) / 3kHz **+12dB** peak (Q 1.8~2.0, 날카롭게 튀는 느낌) |
| **Italian Clean** | 하이파이, 거의 플랫, 상단 클래리티 | 6kHz **+5dB** peak (Q 1.5) 단일 밴드 |
| **Origin Pure** | 완전 투명 레퍼런스 | Flat 유지 (변경 없음) |

#### 13-1-2. 함께 해결해야 할 이슈

**① 게인 스테이징 보상**
Voicing에서 +10dB 이상 밀면 Preamp 출력 레벨이 그만큼 커져 PowerAmp 입력이 과하게 포화될 수 있다. 해결책:
- `AmpModel`에 `voicingMakeupDb` 필드 추가 (예: British Stack −6dB, Modern Micro −8dB)
- `AmpVoicing` 체인 마지막 단에 make-up 감쇠 적용
- 또는 전 밴드의 최대 부스트를 측정해 자동 보상 (`-maxBoostDb` 적용)

**② 피크 필터 Q 캐릭터화**
- 미드 스쿱(Tweed)은 Q 0.5~0.7 → 넓게 파이는 부드러운 느낌
- 공격적 어퍼미드(Modern Micro 3kHz)는 Q 1.8~2.0 → 날카롭게 튀는 느낌
- 현재 값은 대부분 0.7~1.5로 균일한데, 재설계 시 Q도 캐릭터별로 차별화

**③ 실물 앰프명 레퍼런스 제거**
본 문서 및 `AmpModelLibrary.cpp` 주석, PRD.md 등에 남아 있는 "Ampeg SVT / Fender Bassman / Orange AD200 / Darkglass B3K / Markbass Little Mark III" 레퍼런스를 **캐릭터 설명 문구**로 교체:
- American Vintage → "클래식 튜브 스택 캐릭터"
- Tweed Bass → "빈티지 웜톤 튜브 콤보"
- British Stack → "브리티시 어그레시브 튜브 스택"
- Modern Micro → "모던 하이게인 솔리드스테이트"
- Italian Clean → "이탈리안 하이파이 Class D"

**④ Italian Clean vs Origin Pure 구분점**
둘 다 거의 플랫에 가깝다. 구분을 위해:
- Italian Clean: 6kHz 상단 클래리티 단일 부스트 + VPF/VLE로 캐릭터 표현
- Origin Pure: 완전 플랫, VPF/VLE 없음, Baxandall + Class D로 가장 투명한 레퍼런스 톤

#### 13-1-3. 진행 계획

- Phase 10 완료 후 실제 사용자 청취 피드백을 바탕으로 재검토
- 검토 시점에 다음 항목을 결정:
  1. Voicing 값 캐리커처 재설계 적용 여부 (위 표 적용)
  2. AmpVoicing make-up gain 보상 메커니즘 도입 여부
  3. 실물 앰프명 레퍼런스를 캐릭터 설명으로 교체 (주석/문서)
- 또는 13-2(장르별) / 13-3(NAM)와 비교하여 더 적합한 방향을 선택

#### 13-1-4. 리스크

- 과도한 부스트(+12dB)는 특정 대역에서 피드백/링잉 우려 → Q 값 조정으로 완화
- Make-up gain 없이 진행 시 PowerAmp 클리핑이 의도치 않게 증가할 수 있음 → 반드시 보상 메커니즘 선행 구현
- 캐리커처가 지나치게 과장되면 "EQ 프리셋처럼 들린다"는 인상 → 실제 구현 후 A/B 테스트로 밸런스 확인

---

### 13-2. 장르별 앰프 구현 (검토 중)

**배경**: 13-1의 캐리커처 방식은 "이 앰프가 어떤 실물 앰프처럼 들리는가"를 기준으로 한다. 반면 일반 사용자(특히 초·중급 베이시스트)는 "이 앰프가 어떤 실물 앰프인지"보다 **"내가 지금 연주할 장르에 어울리는가"** 를 더 직관적으로 받아들인다.

따라서 실물 앰프의 캐리커처를 구현하는 대신, 아예 앰프 모델을 **음악 장르 중심**으로 재구성하는 방안을 검토한다. 사용자가 앰프 셀렉터에서 "Rock", "Jazz", "Pop"을 고르면 해당 장르의 전형적인 베이스 톤이 즉시 나오는 것이 목표다.

#### 13-2-1. 컨셉

- 각 앰프 모델은 **장르 이름**을 가진다 (예: "Rock Amp", "Jazz Amp", "Pop Amp")
- 사용자는 실물 앰프 지식이 없어도 "지금 연주할 곡의 장르"만 알면 적절한 앰프를 선택할 수 있다
- 각 장르별 앰프는 해당 장르에서 가장 전형적으로 사용되는 톤 프로파일(Voicing + Preamp + PowerAmp + Cabinet IR 조합)로 구성
- 내부적으로는 여전히 Voicing 필터 + ToneStack + Preamp/PowerAmp 타입 조합을 사용하지만, 사용자에게 노출되는 이름과 프리셋은 장르 기반

#### 13-2-2. 장르 후보 (초안)

| 장르 | 대표 톤 방향 | 내부 구성 아이디어 |
|------|----------|----------------|
| **Rock** | 두툼한 저역 + 공격적 미드 + 약간의 드라이브 | 저역 부스트 + 500Hz peak + Tube Preamp + 미디엄 Sag |
| **Metal** | 타이트한 저역 + 날카로운 어퍼미드 그릿 | HP 80Hz + 3kHz peak + JFET/SolidState Preamp + Hard Clip |
| **Jazz** | 따뜻한 중저역 + 부드러운 고역 롤오프 | 저역 살짝 부스트 + 5kHz shelf 컷 + Tube Preamp + 최소 드라이브 |
| **Pop / Session** | 플랫에 가까운 밸런스 + 상단 클래리티 | 거의 플랫 + 6kHz 약간 부스트 + Class D + 최대 헤드룸 |
| **Funk / Slap** | 저역 + 고역 강조(U자), 미드 스쿱 | 80Hz 부스트 + 800Hz 컷 + 4kHz 부스트 + 빠른 트랜지언트 |
| **Blues / Motown** | 빈티지 웜톤, 중역 풍부함 | 저역 살짝 + 300~600Hz 부스트 + 고역 롤오프 + Tube Preamp |
| **Reggae / Dub** | 극단적 저역, 고역 거의 없음 | 60Hz 큰 부스트 + 1kHz 컷 + 3kHz 이상 큰 컷 + LPF |
| **Indie / Alternative** | 중간 톤, 약간 거친 미드 | 저역 보통 + 1kHz peak + 약간의 고조파 |

※ 6~8종 중 MVP 시점에 포함할 5~6종을 선정한다.

#### 13-2-3. 장점

- **직관성**: 사용자가 실물 앰프 지식 없이도 장르만 보고 선택 가능. "이게 뭔지 모르겠다"는 진입 장벽 제거
- **명확한 차별화**: 각 모델이 확실히 다른 톤을 내야 할 강한 동기 부여 (장르 간 구분이 모호하면 의미가 없음 → 자연스럽게 Voicing 강도가 커짐)
- **네이밍 이슈 해소**: 실물 앰프 상표권/레퍼런스 문제 완전 회피
- **프리셋 시스템과의 시너지**: 각 장르 앰프 아래에 세부 프리셋("Rock / Classic", "Rock / Modern", "Rock / Stoner") 추가 가능

#### 13-2-4. 단점 및 고민 지점

- **개성 상실 우려**: 실물 앰프 기반은 "SVT 좋아하는 사람", "Markbass 좋아하는 사람" 같은 팬덤이 있지만, "Rock Amp"는 중립적이라 브랜드 매력이 약할 수 있음
- **장르 경계 모호**: 록과 메탈, 재즈와 블루스의 경계가 모호해서 사용자가 어느 쪽을 골라야 할지 망설일 수 있음
- **주관성**: "Rock 앰프는 이렇게 들려야 한다"는 기준이 사용자마다 다름 → A/B 테스트 및 레퍼런스 트랙 수집 필요
- **상급자 진입 장벽**: 오히려 실물 앰프에 익숙한 상급 베이시스트는 "이게 어떤 앰프를 참고한 건지" 알 수 없어 불편할 수 있음

#### 13-2-5. 진행 계획

- Phase 10 완료 후 실제 사용자 피드백 및 자체 청취 테스트를 거쳐 결정
- 결정 전 레퍼런스 트랙 수집: 각 장르에서 전형적인 베이스 톤을 들려주는 녹음 10곡 정도
- 13-1 / 13-2 / 13-3 중 선택(또는 조합) 후 별도 Phase(Phase 11 이상)로 진행

---

### 13-3. NAM 모델 로딩으로 실물 앰프 구현 (검토 중)

**배경**: 13-1은 "Voicing 과장으로 캐리커처 재현", 13-2는 "장르 중심 재구성"을 제안한다. 그러나 둘 다 본질적으로 **IIR 바이쿼드 필터 기반**이라 실물 앰프의 복잡한 비선형성, 트랜지언트 특성, 고조파 분포를 완벽히 재현할 수 없다.

**NAM (Neural Amp Modeler)** 은 실물 앰프를 딥러닝 모델로 캡처한 `.nam` 파일을 오디오 추론으로 재생하는 오픈소스 기술이다. 실물 앰프의 주파수 응답을 측정할 필요 없이, 공개된 NAM 라이브러리에서 원하는 앰프 모델을 다운로드해 그대로 적용할 수 있다. 따라서 "실물 앰프 복각" 역할을 NAM에 위임하고, 내장 앰프 DSP는 장르/캐리커처 같은 다른 역할에 집중시키는 방향이다.

#### 13-3-1. 컨셉

- 사용자가 `.nam` 파일을 UI에서 로드하면 해당 앰프 모델의 **Preamp/PowerAmp 비선형 특성**이 NAM 추론으로 대체된다
- 내장 DSP 체인(Voicing, ToneStack, Cabinet 등)은 그대로 유지하며, NAM은 비선형 포화 단계만 담당
- NAM 모델 커뮤니티에는 이미 Ampeg SVT, Fender Bassman, Mesa, Markbass 등 다양한 베이스 앰프 캡처가 공유되어 있음
- 사용자가 직접 자신의 앰프를 캡처해서 로드하는 것도 가능 (NAM Capture 도구 제공 시)

> **전제: NAM은 고정 스냅샷이다.** 각 `.nam` 파일은 실물 앰프의 모든 노브가 특정 값에 고정된 상태의 캡처이므로, 로드 후 "앰프 노브를 돌려서" 음색을 바꾸는 것은 불가능하다. 사용자의 음색 조정은 NAM 뒤에 연결된 플러그인의 **ToneStack / GraphicEQ / Post-FX** 로 수행한다. 게인 변경 등 근본적인 캐릭터 전환은 같은 앰프의 다른 스냅샷 `.nam` 파일로 교체해야 한다. 자세한 내용은 **13-4. NAM 모델 고려사항** 참조.

#### 13-3-2. 구현 개요 (CLAUDE.md의 기존 NAM 섹션 참조)

```cpp
class Preamp {
    std::unique_ptr<nam::DSP> namModel;  // null이면 DSP 웨이브쉐이핑 사용
    bool namEnabled = false;

    void processBlock(AudioBuffer<float>& buffer) {
        if (namEnabled && namModel)
            namModel->process(buffer);
        else
            processWithDSP(buffer);
    }
};
```

- `Preamp`, `PowerAmp`, `Overdrive` 각각이 `NamSlot` 보유
- `.nam` 파일 로드는 백그라운드 스레드, `std::atomic<nam::DSP*>`로 오디오 스레드에 swap
- DSP ↔ NAM 전환 시 10ms 크로스페이드로 팝 노이즈 방지
- 라이브러리: `NeuralAmpModelerCore` (MIT 라이선스)

#### 13-3-3. 장점

- **진정한 리얼리즘**: 실물 앰프의 복잡한 비선형성, 트랜지언트, 고조파 분포를 IIR 근사 없이 정확히 재현
- **주파수 응답 측정 불필요**: 13-1의 근본 문제(측정 데이터 부재) 완전 해결
- **확장성**: 사용자가 무한히 많은 앰프를 추가 가능. 개발팀이 새 앰프 모델을 추가할 필요 없음
- **상표권 회피**: "Ampeg SVT"를 내장하는 것이 아니라 "사용자가 로드하는 파일"이므로 상표권 부담 없음
- **커뮤니티 자산 활용**: ToneHunt.org 등에 이미 수천 개의 NAM 캡처가 공유됨 (대부분 무료)
- **내장 앰프 역할 명확화**: 내장 6종은 "장르/캐리커처" 역할에 집중, NAM은 "실물 복각" 역할 분담

#### 13-3-4. 단점 및 고민 지점

- **노브 스냅샷 제약 (가장 중요)**: NAM 파일은 실물 앰프 노브를 특정 값에 고정한 상태의 캡처이므로, 로드 후 사용자가 앰프 노브를 돌려 음색을 바꿀 수 없다. 게인/톤 조정은 플러그인의 Post-EQ나 별도 스냅샷 교체로 해결해야 함. 내장 6종 앰프가 제공하는 "실시간 노브 조작" UX와 큰 이질감 발생. 자세한 분석 및 대안은 **13-4. NAM 모델 고려사항** 참조
- **CPU 부하**: NAM 추론은 경량화된 모델도 IIR 필터보다 수 배 무거움. 저사양 환경에서는 Bi-Amp + NAM 동시 사용 시 버벅일 수 있음
- **지연(Latency)**: 모델에 따라 수 ms의 추론 지연 → `setLatencySamples()` 정확히 보고 필수
- **파일 관리 UX**: 사용자가 `.nam` 파일을 어디에 저장하고 어떻게 선택하는지 UI 설계 필요 (파일 다이얼로그 / 라이브러리 폴더 스캔 / 즐겨찾기 등)
- **프리셋 호환성**: 프리셋이 특정 `.nam` 파일 경로를 참조하면 다른 사용자와 공유 시 경로가 깨짐 → 파일 해시 기반 참조 + 자동 검색 필요
- **품질 편차**: NAM 캡처는 품질 편차가 큼. 공식 큐레이션 세트를 내장 배포하거나, 권장 라이브러리 링크 제공 필요
- **블록 경계**: Preamp와 PowerAmp를 분리해서 캡처한 NAM 모델이 적고, 대부분 "프리앰프+파워앰프+캐비닛 전체"를 하나로 캡처함 → 블록 단위 대체 시 매칭 문제
- **라이선스**: `NeuralAmpModelerCore`는 MIT지만, 개별 `.nam` 파일의 배포 라이선스는 캡처 제작자마다 다름

#### 13-3-5. 13-1 / 13-2와의 관계 및 조합 전략

세 방향은 서로 보완적이므로 조합 적용 가능:

- **13-1 + 13-3**: 내장 앰프는 캐리커처로 운영, 사용자가 원하면 NAM으로 실물 앰프 로드 → 초보자는 내장 앰프, 고급자는 NAM
- **13-2 + 13-3**: 내장 앰프는 장르별 구성, NAM은 "실물 앰프 팬" 사용자를 위한 선택지 → 가장 균형 잡힌 구성
- **13-3 단독**: 내장 앰프를 최소 2~3종(완전 투명/장르 중립)으로 줄이고 나머지는 모두 NAM 위임 → 플러그인이 가벼워지지만 NAM 미설치 시 사용성 하락

**추천 조합 (현재 시점 의견)**: **13-2 + 13-3**
- 내장 6종을 장르별로 재구성 (Rock/Jazz/Pop/Metal/Funk/Clean)
- NAM 슬롯을 Preamp 블록에 추가해서 "실물 앰프 느낌"을 원하는 사용자는 `.nam` 파일 로드
- 프리셋은 내장 + NAM 양쪽 조합 가능

#### 13-3-6. 단계별 진행 계획

- **Phase 11**: `NeuralAmpModelerCore` 통합 + Preamp NAM 슬롯 구현 + 단일 `.nam` 파일 로드 테스트
- **Phase 12**: 파일 관리 UI (라이브러리 폴더, 즐겨찾기, 검색) + 프리셋 호환성 (해시 기반 참조)
- **Phase 13**: PowerAmp / Overdrive 블록에도 NAM 슬롯 확장
- **Phase 14**: 자체 NAM 캡처 도구 (사용자가 자기 앰프를 직접 캡처) — Post-Post-MVP

#### 13-3-7. 리스크

- **CPU 벤치마크 필수**: Phase 11 구현 전 NAM 추론 CPU 사용량을 Bi-Amp + 전체 이펙트 체인 동시 실행 조건에서 측정. 목표치 초과 시 경량 모델만 지원 또는 옵션으로 제공
- **라이선스 검토**: `.nam` 파일 배포 시 원 캡처 제작자의 라이선스 확인 필수. 내장 배포 시에는 명시적 허가 확보
- **UX 복잡도 증가**: 일반 사용자가 `.nam` 파일 개념을 이해하지 못할 수 있음 → "Advanced" 탭으로 숨기거나 튜토리얼 제공
- **NAM 생태계 의존**: NAM 프로젝트가 중단되거나 포맷이 변경되면 호환성 문제 → 주기적으로 업스트림 추적 필요

#### 13-3-8. 파라메트릭 대안 (AIDA-X / Proteus / SmartAmp)

NAM의 "노브 스냅샷 제약"(13-3-4 참조)을 근본적으로 해결하는 기술이 별도로 존재한다. NAM과 달리 **노브 값을 신경망의 conditioning 입력으로 같이 학습**시켜, 로드 후에도 노브를 연속적으로 돌릴 수 있다.

- **AIDA-X**: 노브 conditioning 지원, LV2/VST3 제공 — https://github.com/AidaDSP/aida-x
- **Proteus**: 게인 파라미터 학습, 경량 모델 — https://github.com/GuitarML/Proteus
- **SmartAmp**: 노브 컨디셔닝 기반 앰프 모델링 — https://github.com/GuitarML/SmartAmpPro

**현재 시점 판단**: NAM이 생태계/커뮤니티 규모(ToneHunt.org 등)에서 압도적이므로 13-3 초기 도입은 NAM(스냅샷 + Post-EQ) 방식으로 진행한다. 단, Phase 11 재검토 시점에 위 파라메트릭 대안들의 성숙도(커뮤니티 규모, 모델 품질, C++ 통합 난이도)를 재평가하여 전환 또는 병행 지원 여부를 결정한다. 상세 비교는 **13-4-2 ③번** 참조.

---

### 13-4. NAM 모델 고려사항

13-3의 NAM 도입을 검토할 때 반드시 함께 고민해야 할 **근본적 제약**에 대한 정리. "사용자가 NAM 앰프를 고른 뒤에도 노브를 돌릴 수 있는가?"라는 핵심 질문을 다룬다.

#### 13-4-1. NAM 캡처의 본질적 제약

NAM은 **스냅샷 캡처** 방식이다. 즉 앰프의 모든 노브(Gain, Bass, Mid, Treble, Presence, Master 등)를 **특정 값으로 고정**한 상태에서 입력 스윕 신호를 넣고 출력을 녹음해서, 그 하나의 "정적 전달 함수"를 신경망으로 학습시킨다. 따라서:

- **노브를 돌리면 그 스냅샷이 아닌 완전히 다른 회로**가 되는 것이라서, 학습된 모델로는 재현 불가능
- 노브를 바꾸려면 **새 캡처 파일이 필요**함
- ToneHunt.org 같은 커뮤니티에는 같은 앰프를 "Gain 3/Bass 5/Treble 7", "Gain 7/Bass 5/Treble 5" 등 **수십 개의 스냅샷**으로 따로 올려놓은 경우가 많음

#### 13-4-2. 실제 해결 방법 3가지

**① 캡처 전 스냅샷 고정 (가장 일반적)**
- 사용자가 `.nam` 파일을 고르는 순간, 그 파일에 캡처된 노브 세팅이 그대로 들어옴
- 플러그인 내 앰프 노브를 돌려도 **NAM 모델 내부는 변화 없음**
- 대신 NAM 뒤에 별도의 **Post-EQ(ToneStack)** 를 붙여서 사용자가 음색 조정 가능
- 실제 Neural DSP, STL Tones 같은 상용 플러그인도 이 방식: "NAM은 고정된 '사운드 스냅샷', 톤 조정은 플러그인의 EQ로"

**② 멀티 스냅샷 번들 (절충안)**
- 같은 앰프를 Gain Low/Mid/High 3~5 스냅샷으로 캡처한 세트를 제공
- 사용자가 "Gain 노브"를 돌리면 내부적으로 가장 가까운 스냅샷으로 **전환 + 크로스페이드**
- 단점: 파일 N배 용량, 노브 중간값은 여전히 근사, 두 스냅샷 사이 전환 시 음색 점프

**③ 파라메트릭 모델 (NAM과는 다른 기술)**
- **AIDA-X / Proteus / SmartAmp** 같은 프로젝트는 노브 값을 신경망의 **입력(conditioning)** 으로 같이 넣어 학습시킴
- 캡처 시 다양한 노브 조합으로 수백 번 스윕 → 노브 값을 연속적으로 변화시킬 수 있는 모델 생성
- 장점: 진짜로 "앰프 노브를 돌릴 수 있음"
- 단점: 캡처 프로세스가 훨씬 복잡, 학습 데이터 수십 배, 모델 크기/추론 비용 증가, 현재 생태계는 NAM보다 작음

#### 13-4-3. 우리 프로젝트에 주는 시사점

13-3의 원래 제안("NAM으로 실물 앰프 재현")은 **①번 방식(스냅샷 고정)** 을 전제로 한 것이다. 이게 사용자 입장에서 꽤 중요한 제약이다.

**NAM 슬롯을 도입하면 사용자가 보게 될 UX**:
- 내장 앰프 6종: 모든 노브(Gain/Bass/Mid/Treble/Master) 완전 조작 가능 ← 지금 그대로
- NAM 슬롯: `.nam` 파일 하나 = 하나의 고정된 사운드. 톤 조정은 플러그인의 **Post-EQ나 GraphicEQ로** 해야 함
- "Gain을 올리고 싶다" = 같은 앰프의 다른 게인 스냅샷 `.nam` 파일로 교체해야 함

#### 13-4-4. 설계 반영 사항 (NAM 도입 결정 시)

- **13-3-1 (컨셉)** 에 스냅샷 전제 명시: "NAM은 고정 스냅샷이며, 사용자 음색 조정은 플러그인의 Post-EQ/GraphicEQ로 수행한다"
- **13-3-4 (단점)** 에 "노브 스냅샷 제약" 항목 추가
- **파일 관리 UX**: 같은 앰프의 여러 스냅샷을 그룹으로 묶어 표시 (예: "SVT > Clean / Crunch / Driven")
- **프리셋 시스템 연동**: 프리셋이 특정 NAM 파일 + Post-EQ 조합을 하나의 단위로 저장
- **향후 대안 추적**: AIDA-X / Proteus 등 파라메트릭 기술이 성숙해지면 NAM 대신 검토. 13-4-2의 ③번 참조

#### 13-4-5. 파라메트릭 대안 참고 링크

- **AIDA-X**: https://github.com/AidaDSP/aida-x — 노브 conditioning 지원, LV2/VST3
- **Proteus**: https://github.com/GuitarML/Proteus — 게인 파라미터 학습, 경량 모델
- **SmartAmp**: https://github.com/GuitarML/SmartAmpPro — 노브 컨디셔닝 기반 앰프 모델링

MVP 및 13-3 초기 도입 시점에는 NAM(스냅샷 + Post-EQ)으로 출발하되, Phase 11 이상 검토 시점에 이 대안들의 성숙도를 재확인한다.

