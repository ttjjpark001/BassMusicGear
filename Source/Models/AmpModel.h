#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

//==============================================================================
// 앰프 모델 열거형
//==============================================================================

/**
 * @brief 지원하는 5종 앰프 모델 식별자
 *
 * 각 모델은 톤스택, 프리앰프, 파워앰프 타입과 기본 캐비닛 IR을 조합하여
 * 서로 다른 클래식 베이스 앰프 음색을 구현한다.
 */
enum class AmpModelId
{
    AmericanVintage = 0,    // Ampeg SVT 스타일: Baxandall + Tube6550 + 8x10 SVT
    TweedBass,              // Fender Bassman 스타일: TMB + Tube6550 + 4x10 JBL
    BritishStack,           // Orange AD200 스타일: James + TubeEL34 + 2x12 British
    ModernMicro,            // Darkglass B3K 스타일: BaxandallGrunt + JFET + SolidState + 2x10 Modern
    ItalianClean,           // Markbass clean 스타일: MarkbassFourBand + ClassD + ClassD
    NumModels
};

/**
 * @brief 톤스택 토폴로지 타입
 *
 * 각 타입은 고유한 EQ 곡선과 컨트롤 상호작용을 가진다.
 * - TMB: Fender 클래식 3-band (음 상호작용 있음)
 * - Baxandall: 능동형 4-band 피킹/셸빙 (모든 대역 독립)
 * - James: 2-band 셸빙 + 1-band 미드 피킹 (British 특성)
 * - BaxandallGrunt: Baxandall + Grunt(깊이 조절) 레이어
 * - MarkbassFourBand: 4개 고정주파수 바이쿼드 + VPF/VLE 필터
 */
enum class ToneStackType
{
    TMB,                // Fender TMB 수동 RC 네트워크 (Tweed Bass)
    Baxandall,          // 능동형 Baxandall (American Vintage)
    James,              // James 토폴로지 (British Stack)
    BaxandallGrunt,     // Baxandall + Grunt/Attack (Modern Micro)
    MarkbassFourBand    // 4-band + VPF/VLE (Italian Clean)
};

/**
 * @brief 프리앰프 타입
 *
 * 서로 다른 웨이브쉐이핑 특성을 가진 이득 스테이지.
 */
enum class PreampType
{
    Tube12AX7Cascade,   // 비대칭 tanh 클리핑 (튜브 특성, 짝수 고조파 강조)
    JFETParallel,       // 평행 드라이+드라이브 블렌드 (현대식 clean 유지)
    ClassDLinear         // 선형 클래스D 게인 (깨끗한 증폭 전용)
};

/**
 * @brief 파워앰프 타입
 *
 * 최종 포화 곡선과 Sag 시뮬레이션 여부를 결정한다.
 */
enum class PowerAmpType
{
    Tube6550,           // 부드러운 튜브 포화 (high headroom)
    TubeEL34,           // 빠른 튜브 포화 (low headroom, warm 톤)
    SolidState,         // 경하드 클리핑 (현대식 tight 톤)
    ClassD              // 리니어 (최소 왜곡)
};

//==============================================================================
// 앰프 모델 데이터 구조
//==============================================================================

/**
 * @brief 단일 앰프 모델의 완전한 설정
 *
 * ID, 표시 이름, 톤스택/프리앰프/파워앰프 타입, Sag 활성화 여부,
 * 기본 캐비닛 IR, UI 테마 색상을 포함한다.
 */
struct AmpModel
{
    AmpModelId      id;
    juce::String    name;               // UI에 표시되는 모델 이름
    ToneStackType   toneStack;          // 톤 컨트롤 토폴로지
    PreampType      preamp;             // 이득 스테이지 특성
    PowerAmpType    powerAmp;           // 최종 포화 곡선
    bool            sagEnabled;         // 튜브 앰프만 true (출력 트랜스포머 새깅)
    juce::String    defaultIRName;      // BinaryData 변수명 (기본 캐비닛 IR)
    juce::Colour    themeColour;        // UI 배경/강조색
};
