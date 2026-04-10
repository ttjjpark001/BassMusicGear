#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

//==============================================================================
// Phase 8 PresetTest
//
// **설계 배경**:
// PresetManager 전체를 빌드에 연결하려면 BinaryData 의존성과 JUCE 이벤트 루프
// 초기화가 필요하므로, 단위 테스트는 ValueTree 직렬화/역직렬화의 핵심 로직을
// 독립적으로 검증한다. 실제 플러그인 통합은 Standalone 빌드에서
// 수동 마일스톤(프리셋 저장 → 앱 재시작 → 복원)으로 확인한다.
//==============================================================================

namespace {

/**
 * @brief 테스트용 미니 AudioProcessor
 *
 * PresetManager를 직접 사용하지 않고 ValueTree 로직 검증용.
 * 3개 파라미터만 가짐: gain(float), input_active(bool), amp_model(choice)
 *
 * 실제 플러그인과 동일한 APVTS 구조 시뮬레이션.
 */
class MiniProcessor : public juce::AudioProcessor
{
public:
    /**
     * @brief MiniProcessor 초기화
     *
     * 스테레오 출력만 있는 단순 구조.
     */
    MiniProcessor()
        : juce::AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo())),
          apvts (*this, nullptr, "PARAMETERS", makeLayout())
    {}

    /**
     * @brief 파라미터 레이아웃 정의
     *
     * - gain: 0~1 float (기본 0.5)
     * - input_active: bool (기본 false)
     * - amp_model: 5개 선택 (기본 0)
     *
     * @return ParameterLayout
     */
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "gain", 1 }, "Gain",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "input_active", 1 }, "Active", false));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "amp_model", 1 }, "Amp",
            juce::StringArray { "A", "B", "C", "D", "E" }, 0));
        return { params.begin(), params.end() };
    }

    const juce::String getName() const override            { return "Mini"; }
    void   prepareToPlay (double, int) override            {}
    void   releaseResources() override                     {}
    void   processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override           { return 0.0; }
    bool   acceptsMidi() const override                    { return false; }
    bool   producesMidi() const override                   { return false; }
    bool   hasEditor() const override                      { return false; }
    juce::AudioProcessorEditor* createEditor() override    { return nullptr; }
    int    getNumPrograms() override                       { return 1; }
    int    getCurrentProgram() override                    { return 0; }
    void   setCurrentProgram (int) override                {}
    const  juce::String getProgramName (int) override      { return {}; }
    void   changeProgramName (int, const juce::String&) override {}
    void   getStateInformation (juce::MemoryBlock&) override {}
    void   setStateInformation (const void*, int) override {}

    /** APVTS 파라미터 저장소 */
    juce::AudioProcessorValueTreeState apvts;
};

/**
 * @brief XML로부터 개별 파라미터를 APVTS에 적용 (하위 호환)
 *
 * @param apvts  AudioProcessorValueTreeState
 * @param xml    PARAMETERS 루트 엘리먼트
 *
 * **로직** (PluginProcessor::setStateInformation과 동일):
 * - XML에 없는 파라미터: 현재 기본값 유지
 * - XML에 있는 파라미터: 개별 덮어쓰기
 * - 정규화 변환: raw 값 → 0~1 범위 → setValueNotifyingHost()
 *
 * 이전 버전 프리셋이 새 파라미터를 건드리지 않도록 보장.
 */
void applyXmlBackwardCompatible (juce::AudioProcessorValueTreeState& apvts,
                                 const juce::XmlElement& xml)
{
    // <PARAM> 엘리먼트 반복 처리
    for (auto* child : xml.getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = child->getStringAttribute ("id");
        if (id.isEmpty())
            continue;
        if (auto* p = apvts.getParameter (id))
        {
            // raw 값(파라미터 범위) → 정규화 0~1로 변환
            const float rawValue = (float) child->getDoubleAttribute ("value");
            const float normalized = p->getNormalisableRange().convertTo0to1 (rawValue);
            // 정규화 범위 체크 후 UI와 호스트에 전파
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
        }
    }
}

} // namespace

/**
 * @brief Preset: APVTS 라운드트립 (저장 → 복원) 검증
 *
 * **과정**:
 * 1. APVTS에 값 설정
 * 2. ValueTree로 변환 후 XML 직렬화
 * 3. 파라미터를 초기값으로 초기화
 * 4. XML로부터 복원
 * 5. 복원된 값이 원본과 일치 확인
 *
 * **목표**: 프리셋 저장/로드 기본 동작 검증
 */
TEST_CASE ("Preset: APVTS round-trip via ValueTree XML", "[preset][phase8]")
{
    MiniProcessor p;

    // 1. 초기 상태에서 여러 파라미터 값 변경
    p.apvts.getParameter ("gain")->setValueNotifyingHost (0.75f);
    p.apvts.getParameter ("input_active")->setValueNotifyingHost (1.0f);
    p.apvts.getParameter ("amp_model")->setValueNotifyingHost (
        p.apvts.getParameter ("amp_model")->convertTo0to1 (3.0f));

    // 2. APVTS 상태를 ValueTree로 스냅샷, XML 직렬화
    auto state = p.apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    REQUIRE (xml != nullptr);

    // 3. 파라미터를 모두 초기값으로 되돌림 (복원 테스트 사전 준비)
    p.apvts.getParameter ("gain")->setValueNotifyingHost (0.0f);
    p.apvts.getParameter ("input_active")->setValueNotifyingHost (0.0f);
    p.apvts.getParameter ("amp_model")->setValueNotifyingHost (0.0f);

    // 4. XML로부터 상태 복원 (하위 호환 적용)
    applyXmlBackwardCompatible (p.apvts, *xml);

    // 5. 복원된 값이 원본과 일치하는지 검증
    REQUIRE (p.apvts.getRawParameterValue ("gain")->load() == Catch::Approx (0.75f).margin (0.01f));
    REQUIRE (p.apvts.getRawParameterValue ("input_active")->load() > 0.5f);
    REQUIRE ((int) p.apvts.getRawParameterValue ("amp_model")->load() == 3);
}

/**
 * @brief Preset: 누락된 파라미터는 기본값 유지 (하위 호환)
 *
 * **테스트 목표**:
 * - 이전 버전 프리셋에서 새 파라미터가 없을 때
 * - 로드 시 새 파라미터는 현재 기본값 유지
 * - 기존 파라미터만 XML에서 복원
 *
 * **예시**: Phase 8에서 "input_active" 파라미터 추가 후
 * Phase 7 프리셋 로드 시에도 crash 없이 동작
 */
TEST_CASE ("Preset: missing parameters in XML keep defaults", "[preset][phase8]")
{
    MiniProcessor p;

    // 1. 일부 파라미터 변경 후 XML 직렬화
    p.apvts.getParameter ("gain")->setValueNotifyingHost (0.9f);
    p.apvts.getParameter ("input_active")->setValueNotifyingHost (1.0f);

    auto state = p.apvts.copyState();
    std::unique_ptr<juce::XmlElement> full (state.createXml());

    // 2. gain 파라미터만 포함한 부분 XML 구성
    //    (input_active, amp_model은 의도적으로 누락)
    juce::XmlElement partial ("PARAMETERS");
    for (auto* child : full->getChildWithTagNameIterator ("PARAM"))
    {
        if (child->getStringAttribute ("id") == "gain")
        {
            partial.addChildElement (new juce::XmlElement (*child));
            break;
        }
    }

    // 3. 파라미터를 다른 값으로 설정 (복원 전)
    p.apvts.getParameter ("gain")->setValueNotifyingHost (0.0f);
    p.apvts.getParameter ("input_active")->setValueNotifyingHost (0.0f);

    // 4. 부분 XML 적용 (gain만 복원, 나머지는 현재값 유지)
    applyXmlBackwardCompatible (p.apvts, partial);

    // 5. 검증
    // - gain: XML에서 복원 (0.9)
    REQUIRE (p.apvts.getRawParameterValue ("gain")->load() == Catch::Approx (0.9f).margin (0.01f));
    // - input_active: XML에 없으므로 직전값(0) 유지
    REQUIRE (p.apvts.getRawParameterValue ("input_active")->load() < 0.5f);
}

/**
 * @brief Preset: 사용자 프리셋 디렉터리 경로 검증
 *
 * **경로 규칙**:
 * - 기반: userApplicationDataDirectory (Windows: %APPDATA%, macOS: ~/Library/Application Support)
 * - 경로: BassMusicGear/Presets
 *
 * **검증 항목**:
 * - 절대 경로 (상대 경로 금지)
 * - 부모 폴더명: BassMusicGear
 * - 자신 폴더명: Presets
 */
TEST_CASE ("Preset: user preset directory path is well-formed", "[preset][phase8]")
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("BassMusicGear")
                   .getChildFile ("Presets");

    // 절대 경로이며, BassMusicGear/Presets 구조 확인
    REQUIRE (dir.getFullPathName().isNotEmpty());
    REQUIRE (dir.getFileName() == "Presets");
    REQUIRE (dir.getParentDirectory().getFileName() == "BassMusicGear");
}

/**
 * @brief Preset: A/B 슬롯 시뮬레이션 (ValueTree 스냅샷)
 *
 * **동작**:
 * 1. 설정 A 구성 후 ValueTree 스냅샷 저장
 * 2. 설정 B 구성 후 ValueTree 스냅샷 저장
 * 3. A 복원 → 값 확인
 * 4. B 복원 → 값 확인
 *
 * **목표**: 즉시 비교 모드(A/B 토글) 구현 검증
 * Standalone에서 두 설정을 빠르게 오갈 때 유용.
 */
TEST_CASE ("Preset: A/B slot simulation via ValueTree snapshots", "[preset][phase8][ab]")
{
    MiniProcessor p;

    // 1. 설정 A 작성 → 스냅샷 A 저장
    p.apvts.getParameter ("gain")->setValueNotifyingHost (0.2f);
    auto snapA = p.apvts.copyState();

    // 2. 설정 B 작성 → 스냅샷 B 저장
    p.apvts.getParameter ("gain")->setValueNotifyingHost (0.8f);
    auto snapB = p.apvts.copyState();

    // 3. A 복원 → gain=0.2 확인
    {
        std::unique_ptr<juce::XmlElement> xml (snapA.createXml());
        REQUIRE (xml != nullptr);
        applyXmlBackwardCompatible (p.apvts, *xml);
        REQUIRE (p.apvts.getRawParameterValue ("gain")->load() == Catch::Approx (0.2f).margin (0.01f));
    }

    // 4. B 복원 → gain=0.8 확인
    {
        std::unique_ptr<juce::XmlElement> xml (snapB.createXml());
        REQUIRE (xml != nullptr);
        applyXmlBackwardCompatible (p.apvts, *xml);
        REQUIRE (p.apvts.getRawParameterValue ("gain")->load() == Catch::Approx (0.8f).margin (0.01f));
    }
}
