#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ResponseGraph.h"

class ScribaMonitorProcessor;

// Pagina "Correzione": è il plugin vero e proprio (e una scheda dello standalone).
class CorrectionPage : public juce::Component, private juce::Timer
{
public:
    explicit CorrectionPage (ScribaMonitorProcessor&);
    ~CorrectionPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override       { if (isVisible()) refreshProfileList(); }

    void refreshProfileList();

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String title;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void updateGraph();
    void syncButtons();
    void setChoiceParam (const char* id, int index);
    int getChoiceParam (const char* id) const;
    void chooseProfileFile();
    void chooseCustomTarget();

    ScribaMonitorProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    juce::ComboBox profileBox;
    juce::TextButton loadProfileButton, bypassButton;
    juce::Array<juce::File> profileFiles;

    juce::OwnedArray<juce::TextButton> viewButtons, targetButtons, phaseButtons, emulationButtons;
    juce::TextButton compareButton { "Confronta con correzione pura" };

    std::array<Knob, 5> knobs;
    juce::Slider maxBoost;
    std::unique_ptr<SliderAttachment> maxBoostAttachment;
    juce::ComboBox rangeBox;
    std::unique_ptr<ComboAttachment> rangeAttachment;

    ResponseGraph graph;
    std::unique_ptr<juce::FileChooser> chooser;

    int viewChannel = 2;            // 0 = L, 1 = R, 2 = L+R
    int lastDesignVersion = -1;

    juce::Rectangle<int> topBar, graphPanel, targetPanel, knobsPanel, filterPanel, emulationPanel;
    juce::Rectangle<int> legendArea, filterInfoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorrectionPage)
};
