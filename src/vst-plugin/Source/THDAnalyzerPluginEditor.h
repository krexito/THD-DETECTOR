#pragma once

#include "THDAnalyzerPlugin.h"
#include <array>

class THDAnalyzerPluginEditor final : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit THDAnalyzerPluginEditor (THDAnalyzerPlugin&);
    ~THDAnalyzerPluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    class HeaderBar;
    class ChannelCard;
    class ProgressBarRow;
    class WaveformMiniDisplay;
    class MasterGaugeDisplay;
    class HarmonicSpectrumDisplay;
    class HistoryTimelineDisplay;

    void configureModeControls();

    THDAnalyzerPlugin& processor;

    std::unique_ptr<HeaderBar> headerBar;
    juce::Component channelViewportContent;
    juce::Viewport channelViewport;
    std::vector<std::unique_ptr<ChannelCard>> channelCards;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 8> muteAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 8> soloAttachments;

    juce::ComboBox pluginModeCombo;
    juce::ComboBox channelIdCombo;
    juce::Label pluginModeLabel;
    juce::Label channelIdLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pluginModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelIdAttachment;


    std::unique_ptr<MasterGaugeDisplay> masterGaugeDisplay;
    std::unique_ptr<HarmonicSpectrumDisplay> harmonicSpectrumDisplay;
    std::unique_ptr<HistoryTimelineDisplay> historyTimelineDisplay;
    std::vector<std::unique_ptr<ProgressBarRow>> progressRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (THDAnalyzerPluginEditor)
};
