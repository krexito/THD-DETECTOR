#include "THDAnalyzerPluginEditor.h"

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

class THDAnalyzerPluginEditor::CanvasPlaceholder final : public juce::Component
{
public:
    explicit CanvasPlaceholder (juce::String labelToUse)
        : label (std::move (labelToUse))
    {
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (ColorPalette::surfaceB.withAlpha (0.85f));
        g.fillRoundedRectangle (bounds, 8.0f);

        g.setColour (ColorPalette::borderA);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawLine (bounds.getX() + 8.0f, bounds.getY() + 8.0f, bounds.getRight() - 8.0f, bounds.getBottom() - 8.0f, 1.0f);
        g.drawLine (bounds.getRight() - 8.0f, bounds.getY() + 8.0f, bounds.getX() + 8.0f, bounds.getBottom() - 8.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (makeMonoFont (9.0f));
        g.drawText (label, getLocalBounds(), juce::Justification::centred);
    }

private:
    juce::String label;
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

class THDAnalyzerPluginEditor::ChannelCard final : public juce::Component,
                                                    private juce::Timer
{
public:
    explicit ChannelCard (UIChannelModel channel, bool isMuted, bool isSoloed)
        : model (std::move (channel)), waveform ("WAVEFORM"), removeButton ("x")
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
        removeButton.setTooltip ("Remove channel (UI placeholder)");
        addAndMakeVisible (removeButton);

        muteButton.setToggleState (isMuted, juce::dontSendNotification);
        soloButton.setToggleState (isSoloed, juce::dontSendNotification);

        setInterceptsMouseClicks (true, true);
        startTimerHz (12);
        refreshFromModel();
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

    void refreshFromModel()
    {
        const auto statusColour = statusColourForThd (model.thdN);
        thdLabel.setText (juce::String (model.thdN, 2) + "% THD+N", juce::dontSendNotification);
        thdLabel.setColour (juce::Label::textColourId, statusColour);
        badge.setBadge (statusTextForThd (model.thdN), statusColour);
        vuMeter.setLevel (juce::jlimit (0.0f, 1.0f, model.thdN * 0.4f + 0.25f));
    }

    void timerCallback() override
    {
        hoverMix = juce::jlimit (0.35f, 1.0f, hoverMix + (hovered ? 0.08f : -0.08f));
        removeButton.setAlpha (hoverMix - 0.2f);

        // TODO: Replace random placeholder updates with live metering from DSP analysis data.
        model.thdN = juce::jlimit (0.01f, 3.2f, model.thdN + juce::Random::getSystemRandom().nextFloat() * 0.12f - 0.06f);
        refreshFromModel();
        repaint();
    }

    UIChannelModel model;
    CanvasPlaceholder waveform;
    Badge badge;
    VUMeter vuMeter;
    juce::Label thdLabel;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::TextButton removeButton;
    bool hovered = false;
    float hoverMix = 0.35f;
};

class THDAnalyzerPluginEditor::HeaderBar final : public juce::Component,
                                                  private juce::Timer
{
public:
    HeaderBar()
    {
        addButton.setButtonText ("+ ADD CHANNEL");
        addButton.setColour (juce::TextButton::buttonColourId, ColorPalette::accentBlue.withAlpha (0.9f));
        addButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        addButton.setTooltip ("UI-only action. TODO: wire to real channel creation/routing.");
        addAndMakeVisible (addButton);
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
        pulse = 0.45f + 0.55f * std::sin (phase);
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
    float phase = 0.0f;
    float pulse = 1.0f;
};

THDAnalyzerPluginEditor::THDAnalyzerPluginEditor (THDAnalyzerPlugin& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (1120, 760);

    headerBar = std::make_unique<HeaderBar>();
    addAndMakeVisible (*headerBar);

    channelViewport.setScrollBarsShown (false, true);
    channelViewport.setViewedComponent (&channelViewportContent, false);
    addAndMakeVisible (channelViewport);

    const std::array<UIChannelModel, 8> defaultChannels {{
        { "KICK", juce::Colour::fromString ("fff97316"), 0.42f },
        { "SNARE", juce::Colour::fromString ("ff60a5fa"), 0.38f },
        { "BASS", juce::Colour::fromString ("ffa78bfa"), 0.64f },
        { "GTR L", juce::Colour::fromString ("ff34d399"), 0.29f },
        { "GTR R", juce::Colour::fromString ("ff2dd4bf"), 0.34f },
        { "KEYS", juce::Colour::fromString ("fffbbf24"), 0.57f },
        { "VOX", juce::Colour::fromString ("fff472b6"), 1.21f },
        { "FX BUS", juce::Colour::fromString ("ff94a3b8"), 0.81f }
    }};

    auto& valueTreeState = processor.getValueTreeState();

    for (size_t i = 0; i < defaultChannels.size(); ++i)
    {
        const auto& channel = defaultChannels[i];
        const auto muteParamId = THDAnalyzerPlugin::channelMutedParamId (static_cast<int> (i));
        const auto soloParamId = THDAnalyzerPlugin::channelSoloedParamId (static_cast<int> (i));

        const auto* mutedParam = valueTreeState.getRawParameterValue (muteParamId);
        const auto* soloedParam = valueTreeState.getRawParameterValue (soloParamId);

        auto card = std::make_unique<ChannelCard> (
            channel,
            mutedParam != nullptr ? mutedParam->load() >= 0.5f : false,
            soloedParam != nullptr ? soloedParam->load() >= 0.5f : false);

        muteAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            valueTreeState,
            muteParamId,
            card->getMuteButton());

        soloAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            valueTreeState,
            soloParamId,
            card->getSoloButton());

        channelViewportContent.addAndMakeVisible (*card);
        channelCards.push_back (std::move (card));

        auto row = std::make_unique<ProgressBarRow> (channel.name, channel.color, juce::jlimit (0.15f, 0.95f, channel.thdN / 1.6f));
        addAndMakeVisible (*row);
        progressRows.push_back (std::move (row));
    }

    masterGaugePlaceholder = std::make_unique<CanvasPlaceholder> ("CIRCULAR GAUGE 160x110");
    harmonicPlaceholder = std::make_unique<CanvasPlaceholder> ("HARMONIC SPECTRUM H2-H8");
    historyPlaceholder = std::make_unique<CanvasPlaceholder> ("THD HISTORY TIMELINE");

    addAndMakeVisible (*masterGaugePlaceholder);
    addAndMakeVisible (*harmonicPlaceholder);
    addAndMakeVisible (*historyPlaceholder);

    startTimerHz (20);
}

THDAnalyzerPluginEditor::~THDAnalyzerPluginEditor() = default;

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

    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.setFont (makeMonoFont (8.5f));
    g.drawFittedText ("AVG THD 0.42%  |  MASTER 0.61%  |  PEAK -2.1dB", 28, 328, getWidth() - 56, 16, juce::Justification::centredLeft, 1);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawFittedText ("FLOOR -82.0dB  |  THD+N 0.84%", 28, 344, getWidth() - 56, 16, juce::Justification::centredLeft, 1);
}

void THDAnalyzerPluginEditor::resized()
{
    headerBar->setBounds (0, 0, getWidth(), 50);

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

    masterGaugePlaceholder->setBounds (36, 368, 160, 110);

    auto statsX = 208;
    auto y = 368;
    constexpr int rowHeight = 20;
    for (size_t i = 0; i < progressRows.size(); ++i)
        progressRows[i]->setBounds (statsX, y + static_cast<int> (i) * rowHeight, 280, rowHeight);

    harmonicPlaceholder->setBounds (500, 368, getWidth() - 536, 110);
    historyPlaceholder->setBounds (36, 492, getWidth() - 72, getHeight() - 520);
}

void THDAnalyzerPluginEditor::timerCallback()
{
    repaint();
}
