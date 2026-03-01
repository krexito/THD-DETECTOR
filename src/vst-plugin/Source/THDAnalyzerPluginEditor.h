#pragma once

#include "THDAnalyzerPlugin.h"

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
    class CanvasPlaceholder;

    THDAnalyzerPlugin& processor;

    std::unique_ptr<HeaderBar> headerBar;
    juce::Viewport channelViewport;
    juce::Component channelViewportContent;
    std::vector<std::unique_ptr<ChannelCard>> channelCards;

    std::unique_ptr<CanvasPlaceholder> masterGaugePlaceholder;
    std::unique_ptr<CanvasPlaceholder> harmonicPlaceholder;
    std::unique_ptr<CanvasPlaceholder> historyPlaceholder;
    std::vector<std::unique_ptr<ProgressBarRow>> progressRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (THDAnalyzerPluginEditor)
};
