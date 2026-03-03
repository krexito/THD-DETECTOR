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
    void setData (const std::vector<float>& harmonicsToUse, float fundamentalFrequencyToUse)
    {
        harmonics = harmonicsToUse;
        fundamentalFrequency = juce::jmax (0.0f, fundamentalFrequencyToUse);
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
            const auto harmonicNumber = i + 2;
            const auto isEvenHarmonic = (harmonicNumber % 2) == 0;
            const auto colour = isEvenHarmonic ? ColorPalette::accentBlue : ColorPalette::mediumHigh;
            g.setColour (colour.withAlpha (0.85f));
            g.fillRoundedRectangle (bar, 2.5f);

            g.setColour (juce::Colours::white.withAlpha (0.65f));
            g.setFont (makeMonoFont (8.0f));
            g.drawText ("H" + juce::String (i + 2),
                        juce::Rectangle<int> (static_cast<int> (bar.getX()), static_cast<int> (plotArea.getBottom()) - 12,
                                              static_cast<int> (bar.getWidth()), 12),
                        juce::Justification::centred);
        }

        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (makeMonoFont (8.0f));
        g.drawText ("F0 " + juce::String (fundamentalFrequency, 1) + " Hz", getLocalBounds().removeFromTop (18).reduced (8, 0), juce::Justification::centredRight);
    }

private:
    std::vector<float> harmonics = std::vector<float> (7, 0.0f);
    float fundamentalFrequency = 0.0f;
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
        targetLevel = juce::jlimit (0.0f, 1.0f, value);

        const auto attack = 0.25f;
        const auto release = 0.08f;
        const auto smoothing = targetLevel > level ? attack : release;
        level += (targetLevel - level) * smoothing;
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
    float targetLevel = 0.0f;
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
    ChannelCard (THDAnalyzerPlugin& processorToUse, UIChannelModel channel)
        : processor (processorToUse), model (std::move (channel))
    {
        addAndMakeVisible (waveform);
        addAndMakeVisible (badge);
        addAndMakeVisible (vuMeter);

        thdLabel.setJustificationType (juce::Justification::centred);
        thdLabel.setFont (makeMonoFont (10.0f, true));
        addAndMakeVisible (thdLabel);

        configureButton (muteButton, "M");
        configureButton (soloButton, "S");

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
        g.setColour (ColorPalette::surfaceA.withAlpha (0.96f));
        g.fillRoundedRectangle (bounds, 10.0f);

        auto topGlow = bounds.removeFromTop (2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack,
                                                 topGlow.getX(), topGlow.getCentreY(),
                                                 model.color.withAlpha (0.7f),
                                                 topGlow.getCentreX(), topGlow.getCentreY(),
                                                 true));
        g.fillRoundedRectangle (topGlow, 1.0f);

        auto header = bounds.removeFromTop (20.0f);
        g.setColour (model.color.withAlpha (0.12f));
        g.fillRoundedRectangle (header, 8.0f);

        g.setColour (model.color.withAlpha (0.9f));
        g.setFont (makeMonoFont (8.0f, true));
        g.drawText (model.name, header.toNearestInt().reduced (6, 0), juce::Justification::centredLeft);

        auto idTag = header.removeFromRight (22.0f).reduced (2.0f, 2.0f);
        g.setColour (model.color.withAlpha (0.18f));
        g.fillRoundedRectangle (idTag, 4.0f);
        g.setColour (model.color.withAlpha (0.35f));
        g.drawRoundedRectangle (idTag.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (model.color.withAlpha (0.95f));
        g.setFont (makeMonoFont (7.0f, true));
        g.drawText (juce::String (model.channelId + 1).paddedLeft ('0', 2), idTag.toNearestInt(), juce::Justification::centred);

        g.setColour (ColorPalette::borderA.withAlpha (hoverMix));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (7);
        area.removeFromTop (18);

        waveform.setBounds (area.removeFromTop (50));
        area.removeFromTop (4);
        thdLabel.setBounds (area.removeFromTop (15));
        badge.setBounds (area.removeFromTop (14).withTrimmedLeft (10).withTrimmedRight (10));
        area.removeFromTop (3);

        vuMeter.setBounds (area.removeFromTop (18));
        area.removeFromTop (3);

        auto buttonRow = area.removeFromTop (18);
        muteButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (1));
        soloButton.setBounds (buttonRow.reduced (1));

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
    WaveformMiniDisplay waveform;
    Badge badge;
    VUMeter vuMeter;
    juce::Label thdLabel;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    bool hovered = false;
    float hoverMix = 0.35f;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
};

class THDAnalyzerPluginEditor::HeaderBar final : public juce::Component
{
public:
    HeaderBar() = default;

    void resized() override {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient grad (ColorPalette::surfaceA, 0.0f, 0.0f, ColorPalette::backgroundTop, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (bounds);

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (0.0f, bounds.getBottom() - 4.0f, bounds.getWidth(), 4.0f));
        g.setColour (ColorPalette::borderB.withAlpha (0.95f));
        g.drawLine (0.0f, bounds.getBottom() - 0.5f, bounds.getWidth(), bounds.getBottom() - 0.5f, 1.0f);

        drawWindowDots (g);

        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (66.0f, 14.0f, 1.0f, 22.0f);

        auto title = juce::Rectangle<int> (78, 10, getWidth() - 320, 28);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (makeMonoFont (10.0f, true));
        g.drawText ("THD ANALYZER", title.removeFromLeft (116), juce::Justification::centredLeft);

        g.setColour (ColorPalette::mediumHigh.withAlpha (0.9f));
        g.drawText ("///", title.removeFromLeft (26), juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.58f));
        g.setFont (makeMonoFont (8.0f));
        g.drawText ("v2.0 -- MEASUREMENT EDITION", title, juce::Justification::centredLeft);

        auto statusArea = juce::Rectangle<float> (static_cast<float> (getWidth() - 176), 10.0f, 158.0f, 28.0f);
        g.setColour (ColorPalette::surfaceB.withAlpha (0.95f));
        g.fillRoundedRectangle (statusArea, 6.0f);
        g.setColour (ColorPalette::borderA.withAlpha (0.9f));
        g.drawRoundedRectangle (statusArea.reduced (0.5f), 6.0f, 1.0f);

        const auto dotBounds = juce::Rectangle<float> (statusArea.getX() + 10.0f,
                                                       statusArea.getCentreY() - 3.0f,
                                                       6.0f,
                                                       6.0f);
        g.setColour (ColorPalette::clean.withAlpha (0.95f));
        g.fillEllipse (dotBounds);

        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.setFont (makeMonoFont (8.0f, true));
        g.drawText ("MEASURING", statusArea.toNearestInt().withTrimmedLeft (24), juce::Justification::centredLeft);
    }

private:
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

};

void THDAnalyzerPluginEditor::configureModeControls()
{
    pluginModeLabel.setText ("MODE", juce::dontSendNotification);
    pluginModeLabel.setFont (makeMonoFont (8.0f, true));
    pluginModeLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (pluginModeLabel);

    pluginModeCombo.addItem ("Channel", 1);
    pluginModeCombo.addItem ("Master Brain", 2);
    pluginModeCombo.setTooltip ("Bound to processor Plugin Mode parameter");
    pluginModeCombo.setColour (juce::ComboBox::backgroundColourId, ColorPalette::surfaceA.brighter (0.35f));
    pluginModeCombo.setColour (juce::ComboBox::outlineColourId, ColorPalette::borderA.brighter (0.2f));
    pluginModeCombo.setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.92f));
    pluginModeCombo.onChange = [this]
    {
        updateControlVisibility();
    };
    addAndMakeVisible (pluginModeCombo);

    displayModeLabel.setText ("DISPLAY", juce::dontSendNotification);
    displayModeLabel.setFont (makeMonoFont (8.0f, true));
    displayModeLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (displayModeLabel);

    displayModeCombo.addItem ("Fast", static_cast<int> (DisplaySpeed::fast));
    displayModeCombo.addItem ("Legible", static_cast<int> (DisplaySpeed::legible));
    displayModeCombo.setSelectedId (static_cast<int> (DisplaySpeed::legible), juce::dontSendNotification);
    displayModeCombo.setTooltip ("Display response speed for metrics and harmonics");
    displayModeCombo.setColour (juce::ComboBox::backgroundColourId, ColorPalette::surfaceA.brighter (0.35f));
    displayModeCombo.setColour (juce::ComboBox::outlineColourId, ColorPalette::borderA.brighter (0.2f));
    displayModeCombo.setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.92f));
    displayModeCombo.onChange = [this]
    {
        const auto selected = displayModeCombo.getSelectedId();
        displaySpeed = selected == static_cast<int> (DisplaySpeed::fast) ? DisplaySpeed::fast : DisplaySpeed::legible;
    };
    addAndMakeVisible (displayModeCombo);

    auto& state = processor.getValueTreeState();
    pluginModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "pluginMode", pluginModeCombo);

    updateControlVisibility();
}

void THDAnalyzerPluginEditor::updateControlVisibility()
{
    const auto isMasterMode = pluginModeCombo.getSelectedId() == 2;
    displayModeLabel.setVisible (isMasterMode);
    displayModeCombo.setVisible (isMasterMode);

    channelViewport.setVisible (isMasterMode);

    for (auto& row : progressRows)
        row->setVisible (isMasterMode);

    resized();
    repaint();
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

        auto card = std::make_unique<ChannelCard> (processor, uiModel);

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
    headerBar = std::make_unique<HeaderBar>();
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

    const auto channels = processor.getChannelsSnapshot();
    const auto isMasterMode = pluginModeCombo.getSelectedId() == 2;

    if (isMasterMode)
    {
        auto channelSection = juce::Rectangle<int> (16, 74, getWidth() - 32, 208);
        g.setColour (ColorPalette::surfaceC.withAlpha (0.88f));
        g.fillRoundedRectangle (channelSection.toFloat(), 10.0f);
        g.setColour (ColorPalette::borderA.withAlpha (0.95f));
        g.drawRoundedRectangle (channelSection.toFloat(), 10.0f, 1.0f);

        auto channelHeader = channelSection.removeFromTop (24).reduced (12, 0);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (makeMonoFont (9.0f, true));
        g.drawText ("MASTER BRAIN CHANNEL INPUTS", channelHeader.removeFromLeft (340), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (makeMonoFont (8.0f));
        g.drawText (juce::String (channels.size()) + " channels active", channelHeader, juce::Justification::centredRight);
    }
    else
    {
        auto channelSection = juce::Rectangle<int> (16, 74, getWidth() - 32, 84);
        g.setColour (ColorPalette::surfaceC.withAlpha (0.88f));
        g.fillRoundedRectangle (channelSection.toFloat(), 10.0f);
        g.setColour (ColorPalette::borderA.withAlpha (0.95f));
        g.drawRoundedRectangle (channelSection.toFloat(), 10.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (makeMonoFont (10.0f, true));
        g.drawText ("CHANNEL STRIP MODE - LOCAL ANALYZER", channelSection.reduced (14, 10), juce::Justification::centredLeft);
    }

    auto masterArea = isMasterMode
        ? juce::Rectangle<int> (16, 294, getWidth() - 32, getHeight() - 310)
        : juce::Rectangle<int> (16, 170, getWidth() - 32, getHeight() - 186);
    g.setColour (ColorPalette::surfaceA.withAlpha (0.92f));
    g.fillRoundedRectangle (masterArea.toFloat(), 14.0f);
    g.setColour (ColorPalette::borderC.withAlpha (0.95f));
    g.drawRoundedRectangle (masterArea.toFloat().reduced (0.5f), 14.0f, 1.2f);

    g.setColour (juce::Colours::black.withAlpha (0.36f));
    g.drawRoundedRectangle (masterArea.toFloat().expanded (1.0f), 14.0f, 3.0f);

    auto masterHeader = masterArea.removeFromTop (56);
    juce::ColourGradient masterHeaderGrad (ColorPalette::surfaceA.brighter (0.15f), static_cast<float> (masterHeader.getX()), static_cast<float> (masterHeader.getY()),
                                           ColorPalette::surfaceC, static_cast<float> (masterHeader.getX()), static_cast<float> (masterHeader.getBottom()), false);
    g.setGradientFill (masterHeaderGrad);
    g.fillRoundedRectangle (masterHeader.toFloat(), 12.0f);

    auto leftHeader = masterHeader.reduced (12, 8);
    auto rightHeader = leftHeader.removeFromRight (160);

    auto masterTitle = leftHeader.removeFromTop (18);
    g.setColour (ColorPalette::low.withAlpha (0.9f));
    g.fillEllipse (static_cast<float> (masterTitle.getX()), static_cast<float> (masterTitle.getY() + 4), 7.0f, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (makeMonoFont (10.0f, true));
    g.drawText (isMasterMode ? "THD MASTER ANALYZER" : "THD CHANNEL ANALYZER", masterTitle.withTrimmedLeft (14), juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.68f));
    g.setFont (makeMonoFont (8.0f));
    g.drawText ("AVG " + juce::String (smoothedAverageThd, 2) + "%", leftHeader.removeFromLeft (110), juce::Justification::centredLeft);
    g.drawText ("PEAK " + juce::String (smoothedPeak, 2), leftHeader.removeFromLeft (100), juce::Justification::centredLeft);
    g.drawText ("FLOOR " + juce::String (smoothedNoiseFloor, 5), leftHeader.removeFromLeft (130), juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (makeMonoFont (8.0f, true));
    g.drawText ("STATUS", rightHeader.removeFromTop (10), juce::Justification::centredRight);
    g.setColour (ColorPalette::low.withAlpha (0.92f));
    g.drawText ("MONITORING", rightHeader, juce::Justification::centredRight);

    auto sectionLabelsY = 362;
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (makeMonoFont (8.0f, true));
    g.drawText ("MASTER THD", 36, sectionLabelsY, 180, 14, juce::Justification::centredLeft);
    g.drawText (isMasterMode ? "CHANNEL MEASUREMENTS" : "LOCAL CHANNEL METRICS", 400, sectionLabelsY, 220, 14, juce::Justification::centredLeft);
    g.drawText ("HARMONIC SPECTRUM", 764, sectionLabelsY, 220, 14, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.82f));
    g.setFont (makeMonoFont (9.0f));
    g.drawText ("MASTER " + juce::String (smoothedMasterThd, 2) + "%", 36, 514, 164, 14, juce::Justification::centredLeft);
    g.setColour (ColorPalette::clean.withAlpha (0.9f));
    g.drawText ("THD+N " + juce::String (smoothedMasterThdN, 2) + "%", 36, 532, 164, 14, juce::Justification::centredLeft);

    const auto peakChannel = std::max_element (channels.begin(), channels.end(), [] (const ChannelData& a, const ChannelData& b)
    {
        return a.thd < b.thd;
    });

    if (isMasterMode)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (makeMonoFont (8.0f, true));
        g.drawText ("PEAK CHANNEL", 764, getHeight() - 52, 116, 12, juce::Justification::centredLeft);

        if (peakChannel != channels.end())
        {
            g.setColour (peakChannel->channelColor.withAlpha (0.95f));
            g.setFont (makeMonoFont (9.0f, true));
            g.drawText (peakChannel->channelName.isNotEmpty() ? peakChannel->channelName : ("CH " + juce::String (peakChannel->channelId + 1)),
                        882, getHeight() - 53, 100, 14, juce::Justification::centredLeft);
            g.setColour (ColorPalette::mediumHigh.withAlpha (0.92f));
            g.setFont (makeMonoFont (8.5f));
            g.drawText (juce::String (peakChannel->thd, 2) + "%", 984, getHeight() - 53, 64, 14, juce::Justification::centredLeft);
        }
    }
}

void THDAnalyzerPluginEditor::resized()
{
    if (headerBar == nullptr || masterGaugeDisplay == nullptr || harmonicSpectrumDisplay == nullptr || historyTimelineDisplay == nullptr)
        return;

    headerBar->setBounds (0, 0, getWidth(), 50);

    pluginModeLabel.setBounds (24, 58, 70, 16);
    pluginModeCombo.setBounds (24, 74, 148, 24);
    displayModeLabel.setBounds (184, 58, 70, 16);
    displayModeCombo.setBounds (184, 74, 120, 24);

    const auto isMasterMode = pluginModeCombo.getSelectedId() == 2;

    channelViewport.setBounds (24, 104, getWidth() - 48, 164);
    constexpr int cardWidth = 112;
    constexpr int cardGap = 10;
    constexpr int cardHeight = 152;

    int x = 0;
    for (auto& card : channelCards)
    {
        card->setBounds (x, 0, cardWidth, cardHeight);
        x += cardWidth + cardGap;
    }

    channelViewportContent.setSize (x + 8, cardHeight);

    const int contentLeft = 36;
    const int contentTop = isMasterMode ? 382 : 212;
    const int contentWidth = getWidth() - 72;
    const int columnGap = 18;
    const int columnWidth = (contentWidth - (columnGap * 2)) / 3;

    masterGaugeDisplay->setBounds (contentLeft, contentTop, columnWidth, 122);

    auto statsX = contentLeft + columnWidth + columnGap;
    auto y = contentTop;
    constexpr int rowHeight = 17;
    for (size_t i = 0; i < progressRows.size(); ++i)
        progressRows[i]->setBounds (statsX, y + static_cast<int> (i) * rowHeight, columnWidth, rowHeight);

    const int rightX = statsX + columnWidth + columnGap;
    harmonicSpectrumDisplay->setBounds (rightX, contentTop, columnWidth, 122);
    historyTimelineDisplay->setBounds (rightX, contentTop + 142, columnWidth, 80);
}

void THDAnalyzerPluginEditor::timerCallback()
{
    if (! processor.isEditorDataReady())
        return;

    if (masterGaugeDisplay == nullptr || harmonicSpectrumDisplay == nullptr || historyTimelineDisplay == nullptr)
        return;

    const auto isMasterMode = pluginModeCombo.getSelectedId() == 2;
    const auto snapshotChannels = processor.getChannelsSnapshot();
    if (isMasterMode && channelCards.size() != snapshotChannels.size())
        rebuildChannelCards();

    if (isMasterMode)
    {
        for (auto& card : channelCards)
            card->refreshFromProcessor();
    }

    auto analysis = processor.getLastAnalysisResult();
    FFTAnalyzer::AnalysisResult queuedAnalysis;
    if (processor.popLatestAnalysisResultForEditor (queuedAnalysis))
        analysis = queuedAnalysis;

    double thdSum = 0.0;
    float maxPeak = 0.0f;
    for (const auto& channel : snapshotChannels)
    {
        thdSum += channel.thd;
        maxPeak = juce::jmax (maxPeak, static_cast<float> (channel.peakLevel));
    }

    const auto averageThd = snapshotChannels.empty() ? 0.0f : static_cast<float> (thdSum / static_cast<double> (snapshotChannels.size()));
    const auto measuredThd = juce::jlimit (0.0f, 100.0f, analysis.thd);
    const auto measuredThdN = juce::jlimit (0.0f, 100.0f, analysis.thdN);
    const auto measuredFloor = juce::jlimit (0.0f, 1.0f, analysis.noiseFloor);
    const auto gatedTargetThdN = analysis.level < 0.001f ? 0.0f : measuredThdN;

    const bool fastMode = displaySpeed == DisplaySpeed::fast;
    const auto attack = fastMode ? 0.38f : 0.12f;
    const auto release = fastMode ? 0.24f : 0.06f;

    const auto smoothing = gatedTargetThdN > smoothedMasterThdN ? attack : release;
    smoothedMasterThdN += (gatedTargetThdN - smoothedMasterThdN) * smoothing;
    smoothedAverageThd += (averageThd - smoothedAverageThd) * (fastMode ? 0.34f : 0.11f);
    smoothedMasterThd += (measuredThd - smoothedMasterThd) * (fastMode ? 0.34f : 0.11f);
    smoothedPeak += (maxPeak - smoothedPeak) * (fastMode ? 0.42f : 0.12f);
    smoothedNoiseFloor += (measuredFloor - smoothedNoiseFloor) * (fastMode ? 0.32f : 0.10f);

    if (smoothedHarmonics.size() != analysis.harmonics.size())
        smoothedHarmonics.assign (analysis.harmonics.size(), 0.0f);

    const auto harmonicSmoothing = fastMode ? 0.42f : 0.12f;
    for (size_t i = 0; i < smoothedHarmonics.size(); ++i)
    {
        const auto target = juce::jlimit (0.0f, 1.0f, analysis.harmonics[i]);
        smoothedHarmonics[i] += (target - smoothedHarmonics[i]) * harmonicSmoothing;
    }

    masterGaugeDisplay->setValue (smoothedMasterThdN);
    masterGaugeDisplay->setTooltip ("THD+N " + juce::String (smoothedMasterThdN, 2) + "% (smoothed)");

    harmonicSpectrumDisplay->setData (smoothedHarmonics, analysis.fundamentalFrequency);
    harmonicSpectrumDisplay->setTooltip ("Fundamental " + juce::String (analysis.fundamentalFrequency, 1) + " Hz");

    historyTimelineDisplay->pushValue (smoothedMasterThdN);
    historyTimelineDisplay->setTooltip ("Noise floor " + juce::String (smoothedNoiseFloor, 5));

    repaint();
}
