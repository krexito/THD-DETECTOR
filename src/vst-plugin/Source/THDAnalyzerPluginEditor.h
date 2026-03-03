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

    static float applyBallistics (float input, float previous, double dtSeconds, double attackTauSeconds, double releaseTauSeconds);

    class HeaderBar;
    class ChannelCard;
    class ProgressBarRow;
    class WaveformMiniDisplay;
    class MasterGaugeDisplay;
    class HarmonicSpectrumDisplay;
    class HistoryTimelineDisplay;

    void configureModeControls();
    void updateControlVisibility();
    void rebuildChannelCards();

    THDAnalyzerPlugin& processor;

    std::unique_ptr<HeaderBar> headerBar;
    juce::Component channelViewportContent;
    juce::Viewport channelViewport;
    std::vector<std::unique_ptr<ChannelCard>> channelCards;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 8> muteAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 8> soloAttachments;

    juce::ComboBox pluginModeCombo;
    juce::ComboBox displayModeCombo;
    juce::Label pluginModeLabel;
    juce::Label displayModeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pluginModeAttachment;


    std::unique_ptr<MasterGaugeDisplay> masterGaugeDisplay;
    std::unique_ptr<HarmonicSpectrumDisplay> harmonicSpectrumDisplay;
    std::unique_ptr<HistoryTimelineDisplay> historyTimelineDisplay;
    std::vector<std::unique_ptr<ProgressBarRow>> progressRows;

    enum class DisplaySpeed
    {
        fast = 1,
        legible = 2
    };

    DisplaySpeed displaySpeed = DisplaySpeed::legible;

    float smoothedMasterThdN = 0.0f;
    float smoothedAverageThd = 0.0f;
    float smoothedMasterThd = 0.0f;
    float smoothedPeak = 0.0f;
    float smoothedNoiseFloor = 0.0f;
    float lastValidMasterThd = 0.0f;
    float lastValidMasterThdN = 0.0f;
    float latestAnalysisConfidence = 0.0f;
    bool hasSeenValidAnalysis = false;
    double lastTimerCallbackMs = 0.0;
    std::vector<float> smoothedHarmonics = std::vector<float> (7, 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (THDAnalyzerPluginEditor)
};
