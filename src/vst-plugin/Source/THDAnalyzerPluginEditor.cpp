#include "THDAnalyzerPluginEditor.h"
#include <functional>
#include <algorithm>

namespace
{
struct ColorPalette
{
    static const juce::Colour backgroundTop;
    static const juce::Colour backgroundBottom;
    static const juce::Colour surfaceA;
    static const juce::Colour surfaceB;
    static const juce::Colour surfaceC;
    static const juce::Colour borderA;
    static const juce::Colour borderB;
    static const juce::Colour borderC;

    static const juce::Colour clean;
    static const juce::Colour veryLow;
    static const juce::Colour low;
    static const juce::Colour mediumHigh;
    static const juce::Colour critical;
    static const juce::Colour accentBlue;
};

const juce::Colour ColorPalette::backgroundTop = juce::Colour::fromString ("ff040810");
const juce::Colour ColorPalette::backgroundBottom = juce::Colour::fromString ("ff060b14");
const juce::Colour ColorPalette::surfaceA = juce::Colour::fromString ("ff080d16");
const juce::Colour ColorPalette::surfaceB = juce::Colour::fromString ("ff0d1117");
const juce::Colour ColorPalette::surfaceC = juce::Colour::fromString ("ff060b14");
const juce::Colour ColorPalette::borderA = juce::Colour::fromString ("ff1f2937");
const juce::Colour ColorPalette::borderB = juce::Colour::fromString ("ff0f1929");
const juce::Colour ColorPalette::borderC = juce::Colour::fromString ("ff1a2540");
const juce::Colour ColorPalette::clean = juce::Colour::fromString ("ff22c55e");
const juce::Colour ColorPalette::veryLow = juce::Colour::fromString ("ff84cc16");
const juce::Colour ColorPalette::low = juce::Colour::fromString ("ffeab308");
const juce::Colour ColorPalette::mediumHigh = juce::Colour::fromString ("fff97316");
const juce::Colour ColorPalette::critical = juce::Colour::fromString ("ffef4444");
const juce::Colour ColorPalette::accentBlue = juce::Colour::fromString ("ff60a5fa");

struct UIChannelModel
{
    int channelId = 0;
    juce::String name;
    juce::Colour color;
    float thdN = 0.0f;
};

juce::Font makeMonoFont (float size, bool bold = false)
{
    juce::Font font (size, bold ? juce::Font::bold : juce::Font::plain);
    font.setTypefaceName ("Geist Mono");

    return font;
}

juce::Colour statusColourForThd (float thd)
{
    if (thd < 0.2f)
        return ColorPalette::clean;
    if (thd < 0.5f)
        return ColorPalette::veryLow;
    if (thd < 1.0f)
        return ColorPalette::low;
    if (thd < 2.0f)
        return ColorPalette::mediumHigh;

    return ColorPalette::critical;
}

juce::String statusTextForThd (float thd)
{
    if (thd < 0.2f)
        return "CLEAN";
    if (thd < 0.5f)
        return "VERY LOW";
    if (thd < 1.0f)
        return "LOW";
    if (thd < 2.0f)
        return "MED/HIGH";

    return "CRITICAL";
}
}

class THDAnalyzerPluginEditor::WaveformMiniDisplay final : public juce::Component
{
public:
    void setLevel (float newLevel)
    {
        targetLevel = juce::jlimit (0.0f, 1.0f, newLevel);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (ColorPalette::surfaceB.withAlpha (0.9f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (ColorPalette::borderA.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        level = (level * 0.8f) + (targetLevel * 0.2f);

        juce::Path wave;
        const auto width = juce::jmax (1.0f, bounds.getWidth() - 12.0f);
        const auto centreY = bounds.getCentreY();
        const auto amplitude = 4.0f + (level * 12.0f);
        wave.startNewSubPath (bounds.getX() + 6.0f, centreY);

        constexpr int points = 40;
        for (int i = 1; i <= points; ++i)
        {
            const auto x = bounds.getX() + 6.0f + (width * static_cast<float> (i) / static_cast<float> (points));
            const auto t = (phase + static_cast<float> (i) * 0.35f);
            const auto y = centreY + amplitude * std::sin (t) * std::exp (-0.02f * static_cast<float> (i));
            wave.lineTo (x, y);
        }

        phase += 0.18f;
        g.setColour (ColorPalette::accentBlue.withAlpha (0.85f));
        g.strokePath (wave, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    float level = 0.0f;
    float targetLevel = 0.0f;
    float phase = 0.0f;
};

class THDAnalyzerPluginEditor::MasterGaugeDisplay final : public juce::Component,
                                                           public juce::SettableTooltipClient
{
public:
    void setValue (float newThdN)
    {
        thdN = juce::jmax (0.0f, newThdN);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (ColorPalette::surfaceB.withAlpha (0.85f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (ColorPalette::borderA.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        auto dialArea = bounds.reduced (12.0f, 10.0f);
        const auto radius = juce::jmin (dialArea.getWidth(), dialArea.getHeight()) * 0.42f;
        const auto centre = dialArea.getCentre();

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                             juce::MathConstants<float>::pi * 1.15f,
                             juce::MathConstants<float>::pi * 2.85f,
                             true);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.strokePath (track, juce::PathStrokeType (5.0f));

        const auto normalised = juce::jlimit (0.0f, 1.0f, thdN / 5.0f);
        juce::Path active;
        active.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                              juce::MathConstants<float>::pi * 1.15f,
                              juce::MathConstants<float>::pi * (1.15f + (1.7f * normalised)),
                              true);

        const auto statusColour = statusColourForThd (thdN);
        g.setColour (statusColour.withAlpha (0.95f));
        g.strokePath (active, juce::PathStrokeType (5.0f));

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (makeMonoFont (13.0f, true));
        g.drawText (juce::String (thdN, 2) + "%", getLocalBounds().withTrimmedTop (36), juce::Justification::centred);
    }

private:
    float thdN = 0.0f;
};

class THDAnalyzerPluginEditor::HarmonicSpectrumDisplay final : public juce::Component,
                                                                public juce::SettableTooltipClient
{
public:
    void setData (const std::vector<float>& harmonicsToUse)
    {
        harmonics = harmonicsToUse;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (ColorPalette::surfaceB.withAlpha (0.88f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (ColorPalette::borderA.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        auto plotArea = bounds.reduced (12.0f, 12.0f);
        const auto bins = juce::jmin (7, static_cast<int> (harmonics.size()));
        if (bins <= 0)
            return;

        float peak = 0.0001f;
        for (int i = 0; i < bins; ++i)
            peak = juce::jmax (peak, harmonics[static_cast<size_t> (i)]);

        const auto barWidth = (plotArea.getWidth() - (static_cast<float> (bins - 1) * 6.0f)) / static_cast<float> (bins);
        for (int i = 0; i < bins; ++i)
        {
            const auto value = juce::jlimit (0.0f, 1.0f, harmonics[static_cast<size_t> (i)] / peak);
            auto bar = juce::Rectangle<float> (plotArea.getX() + static_cast<float> (i) * (barWidth + 6.0f),
                                               plotArea.getBottom() - plotArea.getHeight() * value,
                                               barWidth,
                                               plotArea.getHeight() * value);
            const auto colour = statusColourForThd (value * 2.0f);
            g.setColour (colour.withAlpha (0.85f));
            g.fillRoundedRectangle (bar, 2.5f);

            g.setColour (juce::Colours::white.withAlpha (0.65f));
            g.setFont (makeMonoFont (8.0f));
            g.drawText ("H" + juce::String (i + 2),
                        juce::Rectangle<int> (static_cast<int> (bar.getX()), static_cast<int> (plotArea.getBottom()) - 12,
                                              static_cast<int> (bar.getWidth()), 12),
                        juce::Justification::centred);
        }
    }

private:
    std::vector<float> harmonics = std::vector<float> (7, 0.0f);
};

class THDAnalyzerPluginEditor::HistoryTimelineDisplay final : public juce::Component,
                                                               public juce::SettableTooltipClient
{
public:
    void pushValue (float value)
    {
        history.push_back (juce::jmax (0.0f, value));
        while (history.size() > 180)
            history.erase (history.begin());
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (ColorPalette::surfaceB.withAlpha (0.9f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (ColorPalette::borderA.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        auto plotArea = bounds.reduced (10.0f, 10.0f);
        if (history.size() < 2)
            return;

        float peak = 0.0001f;
        for (const auto v : history)
            peak = juce::jmax (peak, v);

        juce::Path line;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const auto norm = juce::jlimit (0.0f, 1.0f, history[i] / juce::jmax (0.5f, peak));
            const auto x = plotArea.getX() + plotArea.getWidth() * static_cast<float> (i) / static_cast<float> (history.size() - 1);
            const auto y = plotArea.getBottom() - plotArea.getHeight() * norm;
            if (i == 0)
                line.startNewSubPath (x, y);
            else
                line.lineTo (x, y);
        }

        g.setColour (ColorPalette::accentBlue.withAlpha (0.9f));
        g.strokePath (line, juce::PathStrokeType (1.8f));
    }

private:
    std::vector<float> history;
};

class Badge final : public juce::Component
{
public:
    void setBadge (juce::String textToUse, juce::Colour colourToUse)
    {
        text = std::move (textToUse);
        colour = colourToUse;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colour.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (colour.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

        g.setColour (colour.brighter (0.2f));
        g.setFont (makeMonoFont (8.0f, true));
        g.drawText (text, getLocalBounds(), juce::Justification::centred);
    }

private:
    juce::String text { "LOW" };
    juce::Colour colour { ColorPalette::low };
};

class VUMeter final : public juce::Component
{
public:
    void setLevel (float value)
    {
        level = juce::jlimit (0.0f, 1.0f, value);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        constexpr int bars = 20;
        auto area = getLocalBounds().reduced (4);
        const auto barWidth = juce::jmax (2, area.getWidth() / bars - 1);
        const auto litBars = static_cast<int> (std::round (level * static_cast<float> (bars)));

        for (int i = 0; i < bars; ++i)
        {
            auto bar = juce::Rectangle<float> (
                static_cast<float> (area.getX() + i * (barWidth + 1)),
                static_cast<float> (area.getY()),
                static_cast<float> (barWidth),
                static_cast<float> (area.getHeight()));

            const bool active = i < litBars;
            auto colour = active ? ColorPalette::clean : juce::Colours::white.withAlpha (0.1f);
            if (i > 12 && active)
                colour = ColorPalette::mediumHigh;
            if (i > 16 && active)
                colour = ColorPalette::critical;

            g.setColour (colour.withAlpha (active ? 0.95f : 0.3f));
            g.fillRoundedRectangle (bar, 1.2f);

            if (active)
            {
                g.setColour (colour.withAlpha (0.6f));
                g.drawRoundedRectangle (bar.expanded (1.2f), 2.0f, 1.0f);
            }
        }
    }

private:
    float level = 0.0f;
};

class THDAnalyzerPluginEditor::ProgressBarRow final : public juce::Component
{
public:
    ProgressBarRow (juce::String nameToUse, juce::Colour colourToUse, float valueToUse)
        : name (std::move (nameToUse)), colour (colourToUse), value (valueToUse)
    {
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        auto left = bounds.removeFromLeft (78);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (makeMonoFont (8.0f));
        g.drawText (name, left, juce::Justification::centredLeft);

        auto bar = bounds.reduced (0, 5).toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (bar, 4.0f);

        auto fill = bar;
        fill.setWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, value));
        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (fill, 4.0f);
        g.setColour (colour.withAlpha (0.35f));
        g.drawRoundedRectangle (bar, 4.0f, 1.0f);
    }

private:
    juce::String name;
    juce::Colour colour;
    float value = 0.0f;
};

class THDAnalyzerPluginEditor::ChannelCard final : public juce::Component
{
public:
    ChannelCard (THDAnalyzerPlugin& processorToUse, UIChannelModel channel, std::function<void(int)> onRemoveToUse)
        : processor (processorToUse), model (std::move (channel)), onRemove (std::move (onRemoveToUse)), removeButton ("x")
    {
        addAndMakeVisible (waveform);
        addAndMakeVisible (badge);
        addAndMakeVisible (vuMeter);

        thdLabel.setJustificationType (juce::Justification::centred);
        thdLabel.setFont (makeMonoFont (10.0f, true));
        addAndMakeVisible (thdLabel);

        configureButton (muteButton, "M");
        configureButton (soloButton, "S");

        removeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.45f));
        removeButton.setTooltip ("Remove channel");
        addAndMakeVisible (removeButton);
        removeButton.onClick = [this]
        {
            if (onRemove != nullptr)
                onRemove (model.channelId);
        };

        if (juce::isPositiveAndBelow (model.channelId, 8))
        {
            auto& state = processor.getValueTreeState();
            muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                state,
                THDAnalyzerPlugin::channelMutedParamId (model.channelId),
                muteButton);

            soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                state,
                THDAnalyzerPlugin::channelSoloedParamId (model.channelId),
                soloButton);
        }
        else
        {
            muteButton.setEnabled (false);
            soloButton.setEnabled (false);
        }

        setInterceptsMouseClicks (true, true);
        refreshFromProcessor();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (ColorPalette::surfaceA);
        g.fillRoundedRectangle (bounds, 10.0f);

        auto header = bounds.removeFromTop (18.0f);
        g.setColour (model.color.withAlpha (0.1f));
        g.fillRoundedRectangle (header, 10.0f);

        g.setColour (model.color.withAlpha (0.6f));
        g.setFont (makeMonoFont (8.0f, true));
        g.drawText (model.name, header.toNearestInt(), juce::Justification::centred);

        g.setColour (ColorPalette::borderA.withAlpha (hoverMix));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (7);
        area.removeFromTop (14);

        waveform.setBounds (area.removeFromTop (56));
        area.removeFromTop (5);
        thdLabel.setBounds (area.removeFromTop (16));
        badge.setBounds (area.removeFromTop (16).withTrimmedLeft (8).withTrimmedRight (8));
        area.removeFromTop (4);

        vuMeter.setBounds (area.removeFromTop (18));
        area.removeFromTop (4);

        auto buttonRow = area.removeFromTop (20);
        muteButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (1));
        soloButton.setBounds (buttonRow.reduced (1));

        removeButton.setBounds (getWidth() - 16, 2, 14, 14);
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true; }
    void mouseExit (const juce::MouseEvent&) override { hovered = false; }

    void refreshFromProcessor()
    {
        const auto channels = processor.getChannelsSnapshot();
        const auto it = std::find_if (channels.begin(), channels.end(), [this] (const ChannelData& c)
        {
            return c.channelId == model.channelId;
        });

        if (it == channels.end())
            return;

        const auto& channelData = *it;
        model.thdN = static_cast<float> (channelData.thdN);

        const auto statusColour = statusColourForThd (model.thdN);
        thdLabel.setText (juce::String (model.thdN, 2) + "% THD+N", juce::dontSendNotification);
        thdLabel.setColour (juce::Label::textColourId, statusColour);
        badge.setBadge (statusTextForThd (model.thdN), statusColour);
        const auto channelPeak = juce::jlimit (0.0f, 1.0f, static_cast<float> (channelData.peakLevel));
        vuMeter.setLevel (channelPeak);
        waveform.setLevel (channelPeak + juce::jlimit (0.0f, 0.2f, static_cast<float> (channelData.level) * 0.15f));

        hoverMix = juce::jlimit (0.35f, 1.0f, hoverMix + (hovered ? 0.08f : -0.08f));
        removeButton.setAlpha (hoverMix - 0.2f);
        repaint();
    }
    juce::Button& getMuteButton() noexcept { return muteButton; }
    juce::Button& getSoloButton() noexcept { return soloButton; }

private:
    void configureButton (juce::TextButton& button, const juce::String& text)
    {
        button.setButtonText (text);
        button.setColour (juce::TextButton::buttonColourId, ColorPalette::surfaceB);
        button.setColour (juce::TextButton::buttonOnColourId, model.color.withAlpha (0.25f));
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.75f));
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        button.setClickingTogglesState (true);
        button.setTooltip ("Automatable APVTS mute/solo control.");
        button.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        addAndMakeVisible (button);
    }

    THDAnalyzerPlugin& processor;
    UIChannelModel model;
    std::function<void(int)> onRemove;
    WaveformMiniDisplay waveform;
    Badge badge;
    VUMeter vuMeter;
    juce::Label thdLabel;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::TextButton removeButton;
    bool hovered = false;
    float hoverMix = 0.35f;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
};

class THDAnalyzerPluginEditor::HeaderBar final : public juce::Component,
                                                  private juce::Timer
{
public:
    explicit HeaderBar (std::function<void()> onAddChannelToUse)
        : onAddChannel (std::move (onAddChannelToUse))
    {
        addButton.setButtonText ("+ ADD CHANNEL");
        addButton.setColour (juce::TextButton::buttonColourId, ColorPalette::accentBlue.withAlpha (0.9f));
        addButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        addButton.setTooltip ("Add channel analyzer ID");
        addAndMakeVisible (addButton);
        addButton.onClick = [this]
        {
            if (onAddChannel != nullptr)
                onAddChannel();
        };
        startTimerHz (24);
    }

    void resized() override
    {
        addButton.setBounds (getWidth() - 140, 12, 128, 24);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient grad (ColorPalette::surfaceA, 0.0f, 0.0f, ColorPalette::backgroundTop, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (bounds);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (0.0f, bounds.getBottom() - 4.0f, bounds.getWidth(), 4.0f));
        g.setColour (ColorPalette::borderB);
        g.drawLine (0.0f, bounds.getBottom() - 0.5f, bounds.getWidth(), bounds.getBottom() - 0.5f, 1.0f);

        drawWindowDots (g);

        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (makeMonoFont (10.0f, true));
        g.drawText ("THD ANALYZER", getLocalBounds().withTrimmedTop (4), juce::Justification::centredTop);

        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.setFont (makeMonoFont (8.0f));
        g.drawText ("v2.0 -- MEASUREMENT EDITION", getLocalBounds().withTrimmedTop (18), juce::Justification::centredTop);

        auto statusArea = getLocalBounds().removeFromRight (250).withTrimmedLeft (8).withTrimmedTop (13);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (makeMonoFont (8.0f, true));
        g.drawText ("MEASURING", statusArea.removeFromLeft (74), juce::Justification::centredLeft);

        const auto dotBounds = statusArea.removeFromLeft (14).reduced (4).toFloat();
        g.setColour (ColorPalette::clean.withAlpha (pulse));
        g.fillEllipse (dotBounds);
    }

private:
    void timerCallback() override
    {
        phase += 0.15f;
        pulse = juce::jlimit (0.0f, 1.0f, 0.45f + 0.55f * std::sin (phase));
        repaint();
    }

    void drawWindowDots (juce::Graphics& g)
    {
        const std::array<juce::Colour, 3> dots {
            juce::Colour::fromString ("ffff5f56"),
            juce::Colour::fromString ("ffffbd2e"),
            juce::Colour::fromString ("ff27c93f")
        };

        for (size_t i = 0; i < dots.size(); ++i)
        {
            g.setColour (dots[i]);
            g.fillEllipse (14.0f + static_cast<float> (i) * 14.0f, 14.0f, 9.0f, 9.0f);
        }
    }

    juce::TextButton addButton;
    std::function<void()> onAddChannel;
    float phase = 0.0f;
    float pulse = 1.0f;
};

void THDAnalyzerPluginEditor::configureModeControls()
{
    pluginModeLabel.setText ("MODE", juce::dontSendNotification);
    pluginModeLabel.setFont (makeMonoFont (8.0f, true));
    pluginModeLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (pluginModeLabel);

    pluginModeCombo.addItem ("Channel Strip", 1);
    pluginModeCombo.addItem ("Master Brain", 2);
    pluginModeCombo.setTooltip ("Bound to processor Plugin Mode parameter");
    addAndMakeVisible (pluginModeCombo);

    channelIdLabel.setText ("CHANNEL", juce::dontSendNotification);
    channelIdLabel.setFont (makeMonoFont (8.0f, true));
    channelIdLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (channelIdLabel);

    for (int i = 0; i < THDAnalyzerPlugin::maxDynamicChannels; ++i)
        channelIdCombo.addItem ("CH " + juce::String (i + 1), i + 1);

    channelIdCombo.setTooltip ("Bound to processor Channel ID parameter");
    addAndMakeVisible (channelIdCombo);

    auto& state = processor.getValueTreeState();
    pluginModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "pluginMode", pluginModeCombo);
    channelIdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "channelId", channelIdCombo);
}


void THDAnalyzerPluginEditor::rebuildChannelCards()
{
    channelViewportContent.removeAllChildren();
    channelCards.clear();
    progressRows.clear();

    const auto channels = processor.getChannelsSnapshot();
    for (const auto& channel : channels)
    {
        UIChannelModel uiModel;
        uiModel.channelId = channel.channelId;
        uiModel.name = channel.channelName.isNotEmpty() ? channel.channelName : ("CH " + juce::String (channel.channelId + 1));
        uiModel.color = channel.channelColor;
        uiModel.thdN = static_cast<float> (channel.thdN);

        auto card = std::make_unique<ChannelCard> (processor, uiModel, [this] (int id)
        {
            processor.removeChannel (id);
            rebuildChannelCards();
            resized();
        });

        channelViewportContent.addAndMakeVisible (*card);
        channelCards.push_back (std::move (card));

        auto row = std::make_unique<ProgressBarRow> (uiModel.name, uiModel.color, juce::jlimit (0.15f, 0.95f, uiModel.thdN / 1.6f));
        addAndMakeVisible (*row);
        progressRows.push_back (std::move (row));
    }

    resized();
}

THDAnalyzerPluginEditor::THDAnalyzerPluginEditor (THDAnalyzerPlugin& p)
    : AudioProcessorEditor (&p), processor (p)
{
    headerBar = std::make_unique<HeaderBar> ([this]
    {
        processor.ensureChannelExists (processor.getChannelId());
        rebuildChannelCards();
    });
    addAndMakeVisible (*headerBar);
    configureModeControls();

    channelViewport.setScrollBarsShown (false, true);
    channelViewport.setViewedComponent (&channelViewportContent, false);
    addAndMakeVisible (channelViewport);

    rebuildChannelCards();

    masterGaugeDisplay = std::make_unique<MasterGaugeDisplay>();
    harmonicSpectrumDisplay = std::make_unique<HarmonicSpectrumDisplay>();
    historyTimelineDisplay = std::make_unique<HistoryTimelineDisplay>();

    addAndMakeVisible (*masterGaugeDisplay);
    addAndMakeVisible (*harmonicSpectrumDisplay);
    addAndMakeVisible (*historyTimelineDisplay);

    setSize (1120, 760);

    startTimerHz (20);
}

THDAnalyzerPluginEditor::~THDAnalyzerPluginEditor()
{
    stopTimer();
}

void THDAnalyzerPluginEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (ColorPalette::backgroundTop, 0.0f, 0.0f,
                                     ColorPalette::backgroundBottom, 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (background);
    g.fillRect (getLocalBounds());

    auto channelSection = juce::Rectangle<int> (16, 74, getWidth() - 32, 208);
    g.setColour (ColorPalette::surfaceC.withAlpha (0.85f));
    g.fillRoundedRectangle (channelSection.toFloat(), 10.0f);
    g.setColour (ColorPalette::borderA);
    g.drawRoundedRectangle (channelSection.toFloat(), 10.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.setFont (makeMonoFont (9.0f, true));
    g.drawText ("CHANNEL ANALYZER", channelSection.removeFromTop (24).reduced (12, 0), juce::Justification::centredLeft);

    auto masterArea = juce::Rectangle<int> (16, 294, getWidth() - 32, getHeight() - 310);
    g.setColour (ColorPalette::surfaceA.withAlpha (0.9f));
    g.fillRoundedRectangle (masterArea.toFloat(), 14.0f);
    g.setColour (ColorPalette::borderC);
    g.drawRoundedRectangle (masterArea.toFloat().reduced (0.5f), 14.0f, 1.2f);

    g.setColour (juce::Colours::black.withAlpha (0.36f));
    g.drawRoundedRectangle (masterArea.toFloat().expanded (1.0f), 14.0f, 3.0f);

    auto masterHeader = masterArea.removeFromTop (30);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (makeMonoFont (9.0f, true));
    g.drawText ("MASTER BRAIN", masterHeader.reduced (12, 0), juce::Justification::centredLeft);

    g.setColour (ColorPalette::clean.withAlpha (0.9f));
    g.fillEllipse (masterHeader.getRight() - 128.0f, static_cast<float> (masterHeader.getY() + 10), 8.0f, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (makeMonoFont (8.0f));
    g.drawText ("LOCKED", masterHeader.removeFromRight (96), juce::Justification::centredLeft);

    const auto channels = processor.getChannelsSnapshot();
    const auto analysis = processor.getLastAnalysisResult();

    double thdSum = 0.0;
    float maxPeak = 0.0f;
    for (const auto& channel : channels)
    {
        thdSum += channel.thd;
        maxPeak = juce::jmax (maxPeak, static_cast<float> (channel.peakLevel));
    }

    const auto averageThd = channels.empty() ? 0.0 : thdSum / static_cast<double> (channels.size());

    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.setFont (makeMonoFont (8.5f));
    g.drawFittedText ("AVG THD " + juce::String (averageThd, 2) + "%  |  MASTER " + juce::String (analysis.thd, 2) + "%  |  PEAK " + juce::String (maxPeak, 2),
                     28, 328, getWidth() - 56, 16, juce::Justification::centredLeft, 1);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawFittedText ("FLOOR " + juce::String (analysis.noiseFloor, 5) + "  |  THD+N " + juce::String (analysis.thdN, 2) + "%",
                     28, 344, getWidth() - 56, 16, juce::Justification::centredLeft, 1);
}

void THDAnalyzerPluginEditor::resized()
{
    if (headerBar == nullptr || masterGaugeDisplay == nullptr || harmonicSpectrumDisplay == nullptr || historyTimelineDisplay == nullptr)
        return;

    headerBar->setBounds (0, 0, getWidth(), 50);

    pluginModeLabel.setBounds (24, 58, 70, 16);
    pluginModeCombo.setBounds (24, 74, 140, 24);
    channelIdLabel.setBounds (176, 58, 70, 16);
    channelIdCombo.setBounds (176, 74, 120, 24);

    channelViewport.setBounds (24, 104, getWidth() - 48, 164);
    constexpr int cardWidth = 110;
    constexpr int cardGap = 10;
    constexpr int cardHeight = 150;

    int x = 0;
    for (auto& card : channelCards)
    {
        card->setBounds (x, 0, cardWidth, cardHeight);
        x += cardWidth + cardGap;
    }

    channelViewportContent.setSize (x + 8, cardHeight);

    masterGaugeDisplay->setBounds (36, 368, 160, 110);

    auto statsX = 208;
    auto y = 368;
    constexpr int rowHeight = 20;
    for (size_t i = 0; i < progressRows.size(); ++i)
        progressRows[i]->setBounds (statsX, y + static_cast<int> (i) * rowHeight, 280, rowHeight);

    harmonicSpectrumDisplay->setBounds (500, 368, getWidth() - 536, 110);
    historyTimelineDisplay->setBounds (36, 492, getWidth() - 72, getHeight() - 520);
}

void THDAnalyzerPluginEditor::timerCallback()
{
    if (! processor.isEditorDataReady())
        return;

    if (masterGaugeDisplay == nullptr || harmonicSpectrumDisplay == nullptr || historyTimelineDisplay == nullptr)
        return;

    const auto snapshotChannels = processor.getChannelsSnapshot();
    if (channelCards.size() != snapshotChannels.size())
        rebuildChannelCards();

    for (auto& card : channelCards)
        card->refreshFromProcessor();

    const auto analysis = processor.getLastAnalysisResult();
    masterGaugeDisplay->setValue (analysis.thdN);
    masterGaugeDisplay->setTooltip ("THD+N " + juce::String (analysis.thdN, 2) + "%");

    harmonicSpectrumDisplay->setData (analysis.harmonics);
    harmonicSpectrumDisplay->setTooltip ("Fundamental " + juce::String (analysis.fundamentalFrequency, 1) + " Hz");

    historyTimelineDisplay->pushValue (analysis.thdN);
    historyTimelineDisplay->setTooltip ("Noise floor " + juce::String (analysis.noiseFloor, 5));

    repaint();
}
