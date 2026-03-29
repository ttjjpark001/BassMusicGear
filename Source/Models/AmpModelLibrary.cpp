#include "AmpModelLibrary.h"

// 5종 앰프 모델을 정적 배열로 등록한다.
// 각 모델은 고유한 톤스택/프리앰프/파워앰프 타입과 기본 IR을 조합하여
// 전혀 다른 음색 프로필을 제공한다.
const std::array<AmpModel, AmpModelLibrary::numModels> AmpModelLibrary::models = {{
    {
        AmpModelId::AmericanVintage,
        "American Vintage",
        ToneStackType::Baxandall,           // 4밴드 능동형 EQ
        PreampType::Tube12AX7Cascade,       // 튜브 비대칭 클리핑
        PowerAmpType::Tube6550,             // 부드러운 튜브 포화
        true,                               // Sag 활성화 (출력 트랜스 새깅)
        "ir_8x10_svt_wav",                  // 임시: 8x10 SVT placeholder IR (Phase 9에서 실제 IR로 교체 예정)
        juce::Colour (0xffff8800)           // UI 테마: 주황색
    },
    {
        AmpModelId::TweedBass,
        "Tweed Bass",
        ToneStackType::TMB,                 // Fender TMB 상호작용형 3밴드
        PreampType::Tube12AX7Cascade,
        PowerAmpType::Tube6550,
        true,
        "ir_4x10_jbl_wav",                  // Fender Bassman 원조 스피커 (JBL D130F 계열)
        juce::Colour (0xfff5e6c8)           // 크림색
    },
    {
        AmpModelId::BritishStack,
        "British Stack",
        ToneStackType::James,               // 2밴드 셸빙 + 미드 피킹
        PreampType::Tube12AX7Cascade,
        PowerAmpType::TubeEL34,             // 빠른 튜브 포화 (warm 톤)
        true,
        "ir_2x12_british_wav",              // 2x12 British 캐비닛
        juce::Colour (0xffff6600)           // 진한 주황색
    },
    {
        AmpModelId::ModernMicro,
        "Modern Micro",
        ToneStackType::BaxandallGrunt,      // Baxandall + Grunt 깊이 조절 (Darkglass B3K 스타일)
        PreampType::JFETParallel,           // 평행 드라이/드라이브 혼합
        PowerAmpType::SolidState,           // 경하드 클리핑 (현대식 tight)
        false,                              // Sag 미사용 (솔리드스테이트)
        "ir_2x10_modern_wav",               // 2x10 Modern 캐비닛
        juce::Colour (0xff00cc66)           // 초록색
    },
    {
        AmpModelId::ItalianClean,
        "Italian Clean",
        ToneStackType::MarkbassFourBand,    // 4개 고정주파수 밴드 + VPF/VLE
        PreampType::ClassDLinear,           // 깨끗한 선형 증폭
        PowerAmpType::ClassD,               // 최소 왜곡
        false,
        "ir_1x15_vintage_wav",               // 임시: Italian Clean 전용 IR 미확보, 1x15 Vintage로 대체 (Phase 9에서 교체 예정)
        juce::Colour (0xff3399ff)           // 파란색
    }
}};

/**
 * @brief 모델 ID로 앰프 모델 정보를 조회한다.
 *
 * @param id  AmpModelId 열거값
 * @return    해당 모델의 const 참조
 * @note      [메인 스레드] UI 초기화 및 모델 변경 시 호출된다.
 */
const AmpModel& AmpModelLibrary::getModel (AmpModelId id)
{
    return models[static_cast<size_t> (id)];
}

/**
 * @brief 정수 인덱스로 앰프 모델 정보를 조회한다.
 *
 * @param index  0 ~ (numModels-1)
 * @return       해당 모델의 const 참조
 * @note         ComboBox 콜백에서 주로 사용된다.
 */
const AmpModel& AmpModelLibrary::getModel (int index)
{
    jassert (index >= 0 && index < numModels);
    return models[static_cast<size_t> (index)];
}

/**
 * @brief 모든 앰프 모델의 이름을 리스트로 반환한다.
 *
 * @return  "American Vintage", "Tweed Bass", ... 순서의 StringArray
 * @note    ComboBox 항목 채우기 및 UI 갱신에 사용된다.
 */
juce::StringArray AmpModelLibrary::getModelNames()
{
    juce::StringArray names;
    for (const auto& m : models)
        names.add (m.name);
    return names;
}
