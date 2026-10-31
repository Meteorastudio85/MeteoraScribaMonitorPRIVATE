#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ResponseGraph.h"

class ScribaMonitorProcessor;
class MeasurementSession;

// Pagina "Misura" dello standalone: setup, check del livello, misura dei punti, salvataggio.
class MeasurePage : public juce::Component, private juce::Timer
{
public:
    explicit MeasurePage (ScribaMonitorProcessor&);
    ~MeasurePage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onProfileSaved;

    class PositionMap;
    class ProgressGrid;
    class LevelMeter;
    class StepsBar;

private:
    void timerCallback() override;
    void refresh();
    void chooseCalibrationFile();
    void saveProfile();

    ScribaMonitorProcessor& processor;
    MeasurementSession& session;

    juce::ComboBox micBox, calBox, pointsBox;
    juce::Slider outputLevel;
    juce::TextEditor profileName;
    juce::TextButton checkButton { "Check livello" }, measureButton, resetButton { "Ricomincia" },
                     saveButton { "Salva profilo" };
    juce::OwnedArray<juce::Label> ownedLabels;

    std::unique_ptr<PositionMap> positionMap;
    std::unique_ptr<ProgressGrid> progressGrid;
    std::unique_ptr<LevelMeter> levelMeter;
    std::unique_ptr<StepsBar> stepsBar;
    ResponseGraph graph;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String calibrationFileName { "ECM8000_generic.cal" };
    bool profileSaved = false;

    // aree dei pannelli (calcolate in resized, disegnate in paint)
    juce::Rectangle<int> setupPanel, mapPanel, progressPanel, graphPanel, levelPanel;
    juce::Rectangle<int> setupInfoArea, mapCaptionArea, progressInfoArea, levelReadoutArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeasurePage)
};
