#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * @brief 프리셋 관리자 — APVTS ValueTree 직렬화/역직렬화 및 파일 I/O
 *
 * 역할:
 * 1. 팩토리 프리셋 15종을 BinaryData에서 로드
 * 2. 사용자 프리셋을 .bmg 파일로 저장/로드
 *    (경로: userApplicationDataDirectory/BassMusicGear/Presets/*.bmg)
 * 3. Export/Import를 통한 임의 경로 파일 I/O
 *
 * 모든 파일 I/O는 메인 스레드에서만 호출된다.
 */
class PresetManager
{
public:
    /** 팩토리 프리셋 한 개의 메타데이터 */
    struct FactoryPreset
    {
        juce::String name;         // 표시명
        const char*  xmlData;      // BinaryData 포인터
        int          xmlSize;      // BinaryData 크기
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager() = default;

    //==========================================================================
    // 팩토리 프리셋

    /**
     * @brief 등록된 팩토리 프리셋 개수
     *
     * @return  전체 팩토리 프리셋 개수 (15종)
     */
    int  getNumFactoryPresets() const { return (int) factoryPresets.size(); }

    /**
     * @brief 팩토리 프리셋 이름 반환
     *
     * @param index  프리셋 인덱스 (0~14)
     * @return       프리셋 표시명 (예: "AV - Clean", "Tweed - Driven")
     */
    juce::String getFactoryPresetName (int index) const;

    /**
     * @brief 팩토리 프리셋 로드 — 선택한 프리셋의 APVTS 상태를 현재 상태로 적용
     *
     * @param index  프리셋 인덱스 (0~14)
     * @note         [메인 스레드] 선택한 프리셋의 모든 파라미터를 APVTS에 복원한다.
     *               없는 파라미터는 현재 값을 유지한다 (하위 호환).
     */
    void loadFactoryPreset (int index);

    //==========================================================================
    // 사용자 프리셋

    /**
     * @brief 사용자 프리셋 저장소 디렉터리
     *
     * 경로: `userApplicationDataDirectory/BassMusicGear/Presets`
     * 없으면 생성되며, 이곳의 .bmg 파일이 사용자 프리셋 목록에 나타난다.
     *
     * @return  사용자 프리셋 저장소 폴더
     */
    static juce::File getUserPresetDirectory();

    /**
     * @brief 사용자 프리셋 디렉터리의 .bmg 파일 이름 목록 (확장자 제외)
     *
     * 알파벳 순 정렬되며, 파일이 없으면 빈 배열 반환.
     *
     * @return  프리셋 이름 배열
     */
    juce::StringArray getUserPresetNames() const;

    /**
     * @brief 현재 APVTS 상태를 사용자 프리셋으로 저장
     *
     * @param name  프리셋 이름 (확장자 없음, 예: "MyBassPreset")
     * @return      저장 성공 여부 (True: 저장됨, False: 실패)
     * @note        [메인 스레드] 파일 I/O이므로 블로킹될 수 있다.
     *              디렉터리가 없으면 자동 생성한다.
     */
    bool saveUserPreset (const juce::String& name);

    /**
     * @brief 사용자 프리셋 로드 — 저장된 .bmg 파일을 읽어 APVTS 상태 복원
     *
     * @param name  프리셋 이름 (확장자 없음)
     * @return      로드 성공 여부 (True: 복원됨, False: 파일 없음 또는 파싱 실패)
     * @note        [메인 스레드] 없는 파라미터는 현재 값을 유지한다 (하위 호환).
     */
    bool loadUserPreset (const juce::String& name);

    /**
     * @brief 사용자 프리셋 삭제
     *
     * @param name  프리셋 이름 (확장자 없음)
     * @return      삭제 성공 여부 (True: 삭제됨, False: 파일 없음)
     * @note        [메인 스레드] 파일 시스템에서 .bmg 파일을 제거한다.
     */
    bool deleteUserPreset (const juce::String& name);

    //==========================================================================
    // Export / Import (임의 경로)

    /**
     * @brief 현재 APVTS 상태를 임의 경로의 파일로 내보낸다
     *
     * 사용자가 선택한 경로에 XML 형식으로 프리셋을 저장한다.
     * 파일 확장자는 .bmg 또는 .xml 권장.
     *
     * @param file  내보낼 대상 파일
     * @return      저장 성공 여부
     */
    bool exportPresetToFile (const juce::File& file);

    /**
     * @brief 임의 경로의 파일을 읽어 APVTS 상태 복원
     *
     * .bmg (XML) 또는 호환 형식의 파일을 받아들인다.
     *
     * @param file  가져올 파일
     * @return      로드 성공 여부 (True: 복원됨, False: 파일 없음 또는 파싱 실패)
     */
    bool importPresetFromFile (const juce::File& file);

private:
    /** 팩토리 프리셋 목록을 등록한다 (BinaryData 참조) */
    void registerFactoryPresets();

    /** ValueTree를 XML 문자열로 직렬화 */
    juce::String stateToXmlString() const;

    /** XML 문자열로부터 APVTS 상태 복원 (하위 호환: 없는 파라미터는 기본값 유지) */
    bool applyXmlString (const juce::String& xmlString);

    juce::AudioProcessorValueTreeState& apvtsRef;
    std::vector<FactoryPreset>          factoryPresets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
