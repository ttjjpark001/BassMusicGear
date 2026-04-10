#include "PresetManager.h"
#include "BinaryData.h"

//==============================================================================
/**
 * @brief PresetManager 초기화
 *
 * @param apvts  플러그인 프로세서의 AudioProcessorValueTreeState 참조
 *
 * 역할:
 * - 15종 팩토리 프리셋을 BinaryData에서 로드하여 내부 목록 등록
 * - 사용자 프리셋 저장 디렉터리가 없으면 생성
 *
 * @note [메인 스레드] 플러그인 로드 시 한 번만 호출된다.
 */
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts)
{
    registerFactoryPresets();

    // 사용자 프리셋 디렉터리가 없으면 생성한다.
    // Standalone 모드에서 프리셋 저장 시 경로 보장.
    auto dir = getUserPresetDirectory();
    if (! dir.exists())
        dir.createDirectory();
}

//==============================================================================
/**
 * @brief 15종 팩토리 프리셋을 BinaryData에서 로드하여 목록 등록
 *
 * 5개 앰프 모델 × 3가지 톤 (Clean/Driven/Heavy) = 15개 프리셋.
 * CMakeLists.txt에서 JUCE BinaryData로 컴파일된 XML 데이터를 참조하여
 * 메모리상 포인터와 크기를 이용해 프리셋 메타데이터를 구성한다.
 */
void PresetManager::registerFactoryPresets()
{
    // 5종 앰프 모델 × 3가지 톤 스타일 = 15개 팩토리 프리셋
    // 각 항목: 표시명 | BinaryData XML 포인터 | XML 크기
    factoryPresets = {
        { "AV - Clean",  BinaryData::Factory_AmericanVintage_Clean_xml,
                         BinaryData::Factory_AmericanVintage_Clean_xmlSize },
        { "AV - Driven", BinaryData::Factory_AmericanVintage_Driven_xml,
                         BinaryData::Factory_AmericanVintage_Driven_xmlSize },
        { "AV - Heavy",  BinaryData::Factory_AmericanVintage_Heavy_xml,
                         BinaryData::Factory_AmericanVintage_Heavy_xmlSize },

        { "Tweed - Clean",  BinaryData::Factory_TweedBass_Clean_xml,
                            BinaryData::Factory_TweedBass_Clean_xmlSize },
        { "Tweed - Driven", BinaryData::Factory_TweedBass_Driven_xml,
                            BinaryData::Factory_TweedBass_Driven_xmlSize },
        { "Tweed - Heavy",  BinaryData::Factory_TweedBass_Heavy_xml,
                            BinaryData::Factory_TweedBass_Heavy_xmlSize },

        { "British - Clean",  BinaryData::Factory_BritishStack_Clean_xml,
                              BinaryData::Factory_BritishStack_Clean_xmlSize },
        { "British - Driven", BinaryData::Factory_BritishStack_Driven_xml,
                              BinaryData::Factory_BritishStack_Driven_xmlSize },
        { "British - Heavy",  BinaryData::Factory_BritishStack_Heavy_xml,
                              BinaryData::Factory_BritishStack_Heavy_xmlSize },

        { "Modern - Clean",  BinaryData::Factory_ModernMicro_Clean_xml,
                             BinaryData::Factory_ModernMicro_Clean_xmlSize },
        { "Modern - Driven", BinaryData::Factory_ModernMicro_Driven_xml,
                             BinaryData::Factory_ModernMicro_Driven_xmlSize },
        { "Modern - Heavy",  BinaryData::Factory_ModernMicro_Heavy_xml,
                             BinaryData::Factory_ModernMicro_Heavy_xmlSize },

        { "Italian - Clean",  BinaryData::Factory_ItalianClean_Clean_xml,
                              BinaryData::Factory_ItalianClean_Clean_xmlSize },
        { "Italian - Driven", BinaryData::Factory_ItalianClean_Driven_xml,
                              BinaryData::Factory_ItalianClean_Driven_xmlSize },
        { "Italian - Heavy",  BinaryData::Factory_ItalianClean_Heavy_xml,
                              BinaryData::Factory_ItalianClean_Heavy_xmlSize },
    };
}

//==============================================================================
juce::String PresetManager::getFactoryPresetName (int index) const
{
    if (index < 0 || index >= (int) factoryPresets.size())
        return {};
    return factoryPresets[(size_t) index].name;
}

void PresetManager::loadFactoryPreset (int index)
{
    if (index < 0 || index >= (int) factoryPresets.size())
        return;

    const auto& p = factoryPresets[(size_t) index];
    juce::String xmlString = juce::String::fromUTF8 (p.xmlData, p.xmlSize);
    applyXmlString (xmlString);
}

//==============================================================================
juce::File PresetManager::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("BassMusicGear")
               .getChildFile ("Presets");
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    auto dir = getUserPresetDirectory();
    if (! dir.isDirectory())
        return names;

    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.bmg");
    for (const auto& f : files)
        names.add (f.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    if (name.isEmpty())
        return false;

    auto dir = getUserPresetDirectory();
    if (! dir.exists())
        dir.createDirectory();

    auto file = dir.getChildFile (name + ".bmg");
    return exportPresetToFile (file);
}

bool PresetManager::loadUserPreset (const juce::String& name)
{
    auto file = getUserPresetDirectory().getChildFile (name + ".bmg");
    if (! file.existsAsFile())
        return false;
    return importPresetFromFile (file);
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    auto file = getUserPresetDirectory().getChildFile (name + ".bmg");
    if (! file.existsAsFile())
        return false;
    return file.deleteFile();
}

//==============================================================================
bool PresetManager::exportPresetToFile (const juce::File& file)
{
    auto xmlString = stateToXmlString();
    if (xmlString.isEmpty())
        return false;
    return file.replaceWithText (xmlString);
}

bool PresetManager::importPresetFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;
    return applyXmlString (file.loadFileAsString());
}

//==============================================================================
/**
 * @brief APVTS 상태 전체를 XML 문자열로 직렬화
 *
 * @return  XML 형식 문자열 (파싱 실패 시 빈 문자열)
 *
 * ValueTree::createXml()이 각 파라미터를 <PARAM> 엘리먼트로 변환한다.
 * 변환된 XML은 파일 저장 또는 export에 사용된다.
 */
juce::String PresetManager::stateToXmlString() const
{
    auto state = apvtsRef.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr)
        return {};
    return xml->toString();
}

/**
 * @brief XML 문자열을 파싱하여 APVTS 상태에 적용 (하위 호환 모드)
 *
 * @param xmlString  PARAMETERS 루트 엘리먼트를 포함한 XML 문자열
 * @return           파싱 및 적용 성공 여부
 *
 * **하위 호환 전략**:
 * - XML에 없는 파라미터는 현재 값 유지 (이전 버전 프리셋이 새로운 파라미터를 건드리지 않음)
 * - XML에 존재하는 파라미터만 덮어씀
 * - 정규화 변환: 저장된 raw 값 → 0~1 범위 → setValueNotifyingHost()
 * - 음수 범위 파라미터(dB) 및 choice 파라미터 모두 처리
 *
 * @note [메인 스레드] 프리셋 로드 시 호출. UI Attachment가 자동 갱신된다.
 */
bool PresetManager::applyXmlString (const juce::String& xmlString)
{
    auto xml = juce::XmlDocument::parse (xmlString);
    if (xml == nullptr)
        return false;

    const auto stateType = apvtsRef.state.getType();
    if (! xml->hasTagName (stateType))
        return false;

    // 하위 호환: 저장된 XML에 없는 파라미터는 현재 값(기본값)을 유지하고,
    // XML에 존재하는 파라미터만 새로운 값으로 덮어쓴다.
    // 개별 파라미터 단위로 적용하므로 미존재 ID는 무시된다.
    for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = child->getStringAttribute ("id");
        if (id.isEmpty())
            continue;

        if (auto* param = apvtsRef.getParameter (id))
        {
            // raw 값 (파라미터 범위: -60~12 dB 등)을 정규화 0~1로 변환
            const float rawValue = (float) child->getDoubleAttribute ("value");
            const float normalized = param->getNormalisableRange().convertTo0to1 (rawValue);
            // 정규화 범위 체크 후 UI와 호스트에 전파
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
        }
    }
    return true;
}
