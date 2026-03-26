#pragma once

#include "AmpModel.h"
#include <array>

/**
 * @brief 5종 앰프 모델 등록/조회 라이브러리
 *
 * 모든 앰프 모델 정보를 정적으로 관리한다.
 * 모델별 ToneStack 타입, Preamp 타입, PowerAmp 타입, Sag 활성 여부 등을 조회할 수 있다.
 */
class AmpModelLibrary
{
public:
    static constexpr int numModels = static_cast<int> (AmpModelId::NumModels);

    /** @brief 모델 ID로 앰프 모델 정보를 조회한다. */
    static const AmpModel& getModel (AmpModelId id);

    /** @brief 인덱스(int)로 앰프 모델 정보를 조회한다. */
    static const AmpModel& getModel (int index);

    /** @brief 모든 모델 이름을 StringArray로 반환한다 (ComboBox용). */
    static juce::StringArray getModelNames();

private:
    static const std::array<AmpModel, numModels> models;
};
