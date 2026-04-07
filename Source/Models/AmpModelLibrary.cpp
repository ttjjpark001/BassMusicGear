#include "AmpModelLibrary.h"

/**
 * 6종 앰프 모델 정적 레지스트리
 *
 * 각 모델은 다음을 조합하여 클래식 베이스 앰프의 음색 프로필을 재현한다:
 * - ToneStackType: 사용자 조작 가능한 EQ 컨트롤 (TMB/Baxandall/James/VPF-VLE 등)
 * - PreampType: 게인 스테이징 웨이브쉐이핑 특성 (Tube vs JFET vs Class D)
 * - PowerAmpType: 최종 포화 곡선 및 Sag 시뮬레이션 (Tube vs SolidState vs Class D)
 * - voicingBands[3]: 고정된 Voicing 필터 (사용자 조작 불가)
 * - defaultIRName: 기본 캐비닛 IR (BinaryData 변수명)
 * - themeColour: UI 시각적 피드백
 *
 * 앰프 모델 전환(UI 콤보박스):
 * 1. UI에서 모델 선택 → PluginProcessor가 ampVoicing.setModel(id) 호출
 * 2. AmpVoicing::setModel() → AmpModelLibrary::getModel(id) 읽음
 * 3. voicingBands의 모든 필터 계수 재계산 → atomic으로 오디오 스레드에 전달
 * 4. 다음 processBlock()에서 새 Voicing 필터 적용 시작
 */
const std::array<AmpModel, AmpModelLibrary::numModels> AmpModelLibrary::models = {{
    {
        AmpModelId::AmericanVintage,
        "American Vintage",
        ToneStackType::Baxandall,           // Ampeg SVT 스타일: 능동형 4밴드 EQ
        PreampType::Tube12AX7Cascade,       // 튜브 비대칭 클리핑 (따뜻한 색감)
        PowerAmpType::Tube6550,             // 부드러운 튜브 포화 (높은 헤드룸)
        true,                               // Sag 활성화: 출력 트랜스포머 전압 새깅
        "ir_8x10_svt_wav",                  // 8x10 SVT 캐비닛 IR
        juce::Colour (0xffff8800),          // UI 테마: 주황색 (Ampeg 브랜드)
        // Voicing: Ampeg SVT 고정 특성 (사용자 조작 불가)
        // — 저역 셸빙 80Hz +3dB: 따뜻한 하단부 강조 (풍성한 저역)
        // — 피킹 300Hz +2dB Q=0.8: 미드레인지 프레젠스 (근육감)
        // — 피킹 1500Hz -2dB Q=1.2: 어퍼-미드 스쿱 (중간음 경감)
        {{ { 80.0f, 3.0f, 0.707f, FilterType::LowShelf },
           { 300.0f, 2.0f, 0.8f, FilterType::Peak },
           { 1500.0f, -2.0f, 1.2f, FilterType::Peak } }}
    },
    {
        AmpModelId::TweedBass,
        "Tweed Bass",
        ToneStackType::TMB,                 // Fender Bassman 원조: TMB 상호작용형 3밴드 RC 네트워크
        PreampType::Tube12AX7Cascade,       // 튜브 비대칭 클리핑
        PowerAmpType::Tube6550,             // 부드러운 튜브 포화
        true,                               // Sag 활성화
        "ir_4x10_jbl_wav",                  // 4x10 JBL 스피커 (Bassman 5F6-A 원조)
        juce::Colour (0xfff5e6c8),          // UI 테마: 크림색 (빈티지 톤)
        // Voicing: Fender Bassman 5F6-A 고정 특성
        // — 저역 셸빙 60Hz +2dB: 깊은 베이스 따뜻함 (저음 강조)
        // — 피킹 600Hz -3dB Q=0.7: 미드 스쿱 (뭉툭한 미드레인지)
        // — 고역 셸빙 5000Hz -2dB: 고역 롤오프 (빈티지 부드러움)
        {{ { 60.0f, 2.0f, 0.707f, FilterType::LowShelf },
           { 600.0f, -3.0f, 0.7f, FilterType::Peak },
           { 5000.0f, -2.0f, 0.707f, FilterType::HighShelf } }}
    },
    {
        AmpModelId::BritishStack,
        "British Stack",
        ToneStackType::James,               // Orange AD200 스타일: 2밴드 셸빙 + 미드 피킹
        PreampType::Tube12AX7Cascade,       // 튜브 비대칭 클리핑
        PowerAmpType::TubeEL34,             // 빠른 튜브 포화 (낮은 헤드룸, 따뜻한 톤)
        true,                               // Sag 활성화
        "ir_2x12_british_wav",              // 2x12 British 스택 캐비닛
        juce::Colour (0xffff6600),          // UI 테마: 진한 주황색 (Orange 브랜드)
        // Voicing: Orange AD200 고정 특성 (공격적/tight)
        // — 고역통과 60Hz Q=0.7: Tight 저음 (서브베이스 제거, 타이트한 어택)
        // — 피킹 500Hz +3dB Q=1.0: 공격적 미드레인지 (협대역 그로울)
        // — 피킹 2000Hz +2dB Q=1.5: 어퍼-미드 바이트 (선명한 톤)
        {{ { 60.0f, 0.0f, 0.7f, FilterType::HighPass },
           { 500.0f, 3.0f, 1.0f, FilterType::Peak },
           { 2000.0f, 2.0f, 1.5f, FilterType::Peak } }}
    },
    {
        AmpModelId::ModernMicro,
        "Modern Micro",
        ToneStackType::BaxandallGrunt,      // Darkglass B3K 스타일: Baxandall + Grunt 깊이
        PreampType::JFETParallel,           // JFET 평행 드라이/드라이브 혼합 (색감 유지)
        PowerAmpType::SolidState,           // 경하드 클리핑 (현대식 tight/aggressive)
        false,                              // Sag 미사용 (솔리드스테이트 특성)
        "ir_2x10_modern_wav",               // 2x10 Modern 캐비닛
        juce::Colour (0xff00cc66),          // UI 테마: 초록색 (현대식 이미지)
        // Voicing: Darkglass B3K 고정 특성 (극도로 공격적)
        // — 고역통과 80Hz Q=0.7: Tight 저음 (서브베이스 제거)
        // — 피킹 900Hz +2dB Q=1.2: Growl 주파수 (저음 그라인드)
        // — 피킹 3000Hz +4dB Q=1.5: 공격적 클래리티/그라인드 (선명한 어택)
        {{ { 80.0f, 0.0f, 0.7f, FilterType::HighPass },
           { 900.0f, 2.0f, 1.2f, FilterType::Peak },
           { 3000.0f, 4.0f, 1.5f, FilterType::Peak } }}
    },
    {
        AmpModelId::ItalianClean,
        "Italian Clean",
        ToneStackType::MarkbassFourBand,    // Markbass Little Mark III: 4개 고정주파수 밴드 + VPF/VLE
        PreampType::ClassDLinear,           // ClassD 선형 증폭 (깨끗함)
        PowerAmpType::ClassD,               // ClassD 최소 왜곡
        false,                              // Sag 미사용
        "ir_1x15_vintage_wav",              // 임시 IR (중립적 선택)
        juce::Colour (0xff3399ff),          // UI 테마: 파란색 (모던 클린)
        // Voicing: Markbass Little Mark III 고정 특성 (의도적으로 플랫)
        // — V1 Flat: 처리 없음 (기울기 중립)
        // — V2 피킹 6000Hz +1.5dB Q=1.5: 미세한 고역 클래리티 (선명도)
        // — V3 Flat: 처리 없음
        // (Voicing이 최소이므로 VPF/VLE 필터가 음색의 주요 차별점)
        {{ { 1000.0f, 0.0f, 1.0f, FilterType::Flat },
           { 6000.0f, 1.5f, 1.5f, FilterType::Peak },
           { 1000.0f, 0.0f, 1.0f, FilterType::Flat } }}
    },
    {
        AmpModelId::OriginPure,
        "Origin Pure",
        ToneStackType::Baxandall,           // 능동형 Baxandall (American Vintage와 공유)
        PreampType::SolidStateLinear,       // 솔리드스테이트 선형 증폭 (투명함)
        PowerAmpType::ClassD,               // ClassD 최소 왜곡
        false,                              // Sag 미사용
        "ir_1x15_vintage_wav",              // 임시 IR (가장 중립적 선택)
        juce::Colour (0xffc0c0c8),          // UI 테마: 실버 (투명성 상징)
        // Voicing: 완전히 평탄 (가장 투명한 음색)
        // 모든 밴드: gainDb=0, FilterType::Flat
        // (톤스택 Baxandall을 통해서만 음색 조절 가능, Voicing은 최소 간섭)
        {{ { 1000.0f, 0.0f, 1.0f, FilterType::Flat },
           { 1000.0f, 0.0f, 1.0f, FilterType::Flat },
           { 1000.0f, 0.0f, 1.0f, FilterType::Flat } }}
    }
}};

/**
 * @brief 모델 ID로 앰프 모델 정보(voicingBands 포함)를 조회한다.
 *
 * @param id  앰프 모델 ID (AmpModelId enum)
 * @return    해당 모델의 완전한 설정 const 참조 (voicingBands 배열 포함)
 * @note [오디오/메인 스레드 모두 안전] 읽기 전용, 메모리 할당 없음.
 *       AmpVoicing::computeCoefficients()에서 호출되어 voicingBands를 읽음.
 */
const AmpModel& AmpModelLibrary::getModel (AmpModelId id)
{
    return models[static_cast<size_t> (id)];
}

/**
 * @brief 정수 인덱스(0~5)로 앰프 모델 정보를 조회한다.
 *
 * @param index  모델 인덱스 (0 = AmericanVintage, 1 = TweedBass, ..., 5 = OriginPure)
 * @return       해당 모델의 완전한 설정 const 참조
 * @note         범위 외 인덱스는 jassert(debug) 또는 UB(release) 유발
 */
const AmpModel& AmpModelLibrary::getModel (int index)
{
    jassert (index >= 0 && index < numModels);
    return models[static_cast<size_t> (index)];
}

/**
 * @brief 모든 앰프 모델의 이름을 UI 콤보박스용으로 반환한다.
 *
 * @return StringArray: ["American Vintage", "Tweed Bass", "British Stack", "Modern Micro",
 *                       "Italian Clean", "Origin Pure"]
 *         순서는 AmpModelId enum 순서와 정확히 일치 (모델 선택 시 인덱스=ID)
 */
juce::StringArray AmpModelLibrary::getModelNames()
{
    juce::StringArray names;
    for (const auto& m : models)
        names.add (m.name);
    return names;
}
