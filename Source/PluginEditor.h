#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/ScribaLookAndFeel.h"
#include "gui/MeasurePage.h"
#include "gui/CorrectionPage.h"

class ScribaMonitorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ScribaMonitorEditor (ScribaMonitorProcessor&);
    ~ScribaMonitorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showPage (int index);
    void drawLogo (juce::Graphics&, juce::Rectangle<float>);
    void timerCallback() override       { repaint (pillArea); }

    ScribaMonitorProcessor& processor;
    ScribaLookAndFeel lookAndFeel;

    std::unique_ptr<MeasurePage> measurePage;      // solo standalone
    CorrectionPage correctionPage;

    juce::TextButton measureTab { "Misura" }, correctionTab { "Correzione" };
    juce::Rectangle<int> headerArea, footerArea, pillArea;
    int currentPage = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScribaMonitorEditor)
};
