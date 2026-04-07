#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <array>

//==============================================================================
// 앰프 모델 열거형
//==============================================================================

/**
 * @brief 지원하는 6종 앰프 모델 식별자
 *
 * 각 모델은 톤스택 토폴로지, 프리앰프 타입, 파워앰프 타입, Voicing 필터, 기본 캐비닛 IR을
 * 조합하여 실제 베이스 앰프의 음색을 재현한다.
 * Voicing 필터(AmpVoicing)는 사용자가 조작할 수 없는 고정 특성이며,
 * 앰프 모델 선택 시 자동으로 적용된다.
 */
enum class AmpModelId
{
    AmericanVintage = 0,    // Ampeg SVT 스타일: Baxandall + Tube6550 + 8x10 SVT
    TweedBass,              // Fender Bassman 스타일: TMB + Tube6550 + 4x10 JBL
    BritishStack,           // Orange AD200 스타일: James + TubeEL34 + 2x12 British
    ModernMicro,            // Darkglass B3K 스타일: BaxandallGrunt + JFET + SolidState + 2x10 Modern
    ItalianClean,           // Markbass clean 스타일: MarkbassFourBand + ClassD + ClassD
    OriginPure,             // Flat voicing: SolidState + Baxandall + ClassD (투명한 사운드)
    NumModels
};

/**
 * @brief 톤스택 토폴로지 타입
 *
 * 각 타입은 고유한 EQ 곡선과 컨트롤 상호작용 특성을 정의한다.
 * - TMB: Fender Bassman 클래식 3밴드 (Bass/Mid/Treble) — RC 네트워크 상호작용 있음
 * - Baxandall: 능동형 4밴드 피킹/셸빙 — 모든 대역 독립적 (American Vintage)
 * - James: 2밴드 셸빙(Bass/Treble) + 1밴드 미드 피킹 — British Stack 특성
 * - BaxandallGrunt: Baxandall 기반 + Grunt/Attack 깊이 조절 레이어 (Modern Micro)
 * - MarkbassFourBand: 4개 고정주파수 바이쿼드 + VPF/VLE 합성 필터 (Italian Clean)
 */
enum class ToneStackType
{
    TMB,                // Fender TMB 수동 RC 네트워크 (Tweed Bass)
    Baxandall,          // 능동형 Baxandall (American Vintage, Origin Pure)
    James,              // James 토폴로지 (British Stack)
    BaxandallGrunt,     // Baxandall + Grunt/Attack (Modern Micro)
    MarkbassFourBand    // 4-band + VPF/VLE (Italian Clean)
};

/**
 * @brief 프리앰프 타입
 *
 * 서로 다른 웨이브쉐이핑 곡선을 가진 이득 스테이지.
 * 게인을 높일 때의 포화 특성이 앰프의 음색을 크게 결정한다.
 */
enum class PreampType
{
    Tube12AX7Cascade,   // 비대칭 tanh 클리핑: 양/음 반파 비대칭 (튜브 특성, 홀수 + 짝수 고조파)
    JFETParallel,       // 평행 드라이+드라이브 블렌드: 깨끗함 유지하며 색감 추가 (현대식)
    ClassDLinear,       // 선형 ClassD 증폭: 깨끗한 증폭만 (최소 왜곡)
    SolidStateLinear    // 선형 솔리드스테이트: 깨끗한 증폭 (Origin Pure 투명성)
};

/**
 * @brief 파워앰프 타입
 *
 * 최종 포화 곡선과 Sag(출력 트랜스포머 전압 새깅) 시뮬레이션 여부를 결정한다.
 * 파워앰프의 saturation은 강하게 연주할 때의 압축감을 주는 핵심 요소다.
 */
enum class PowerAmpType
{
    Tube6550,           // 부드러운 튜브 포화: 높은 헤드룸, 천천히 포화 (American Vintage)
    TubeEL34,           // 빠른 튜브 포화: 낮은 헤드룸, 따뜻한 톤 (British Stack)
    SolidState,         // 경하드 클리핑: 현대식 tight/aggressive 톤 (Modern Micro)
    ClassD              // 선형 ClassD: 최소 왜곡 (Italian Clean, Origin Pure)
};

//==============================================================================
// Voicing 필터 타입 및 데이터 구조
//==============================================================================

/**
 * @brief AmpVoicing 필터 밴드의 필터 타입
 *
 * Voicing은 사용자가 조작할 수 없는 앰프별 고정 필터이다.
 * 각 앰프의 고유한 회로 특성 및 스피커 특성을 반영한 음색을 정의한다.
 * 앰프 모델이 선택될 때 AmpVoicing::setModel()에서 자동으로 적용된다.
 */
enum class FilterType
{
    Flat,       // 처리 없음 (단위 전달 함수, 평탄응답)
    LowShelf,   // 저역 셸빙: 설정 주파수 아래의 모든 대역을 부스트/컷
    HighShelf,  // 고역 셸빙: 설정 주파수 위의 모든 대역을 부스트/컷
    Peak,       // 피킹/벨 필터: 중심주파수 주변 좁은 대역 부스트/컷 (Q로 대역폭 제어)
    HighPass    // 2차 고역통과: 서브베이스 제거 및 tight 저음 효과 (게인DB 무시)
};

/**
 * @brief Voicing 필터 밴드 한 개의 사양
 *
 * 각 앰프 모델은 최대 3개의 Voicing 밴드를 가진다.
 * 이들은 사용자가 조작하는 톤스택과 무관한, 회로 자체의 고정된 주파수 특성이다.
 * 예: Ampeg SVT = { 80Hz LowShelf +3dB, 300Hz Peak +2dB, 1500Hz Peak -2dB }
 *
 * setModel()이 호출될 때 AmpModelLibrary에서 읽어 AmpVoicing에서 처리된다.
 */
struct VoicingBand
{
    float      freq    = 1000.0f;                      // 중심/코너 주파수 (Hz)
    float      gainDb  = 0.0f;                         // 필터 이득 (dB, HighPass에서는 무시)
    float      q       = 1.0f;                         // Q 인수 (Q↑ = 좁은 대역, HighPass에서는 기울기)
    FilterType type    = FilterType::Flat;             // 필터 타입 (위의 FilterType enum 참조)
};

//==============================================================================
// 앰프 모델 데이터 구조
//==============================================================================

/**
 * @brief 단일 앰프 모델의 완전한 설정
 *
 * 각 앰프 모델은 ID, 표시명, 톤스택 토폴로지, 프리앰프 타입, 파워앰프 타입,
 * Sag 활성화 여부, 기본 캐비닛 IR, UI 테마 색상, 그리고 고정 Voicing 필터 체인을 정의한다.
 * AmpVoicing은 이 voicingBands를 읽어 매 버퍼마다 처리한다.
 */
struct AmpModel
{
    AmpModelId      id;
    juce::String    name;               // UI에 표시되는 모델 이름 (예: "American Vintage")
    ToneStackType   toneStack;          // 톤 컨트롤 토폴로지
    PreampType      preamp;             // 이득 스테이지 웨이브쉐이핑 특성
    PowerAmpType    powerAmp;           // 최종 포화 곡선
    bool            sagEnabled;         // 튜브 앰프만 true (출력 트랜스포머 전압 새깅 활성화)
    juce::String    defaultIRName;      // BinaryData 변수명 (기본 캐비닛 IR)
    juce::Colour    themeColour;        // UI 배경/강조색 (시각적 피드백)
    std::array<VoicingBand, 3> voicingBands;  // 앰프별 고정 Voicing 필터 밴드 (최대 3개 바이쿼드)
};
