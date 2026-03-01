/* ==============================================================================
   THD Analyzer VST Plugin Implementation
   ============================================================================== */

#include "THDAnalyzerPlugin.h"

THDAnalyzerPlugin::THDAnalyzerPlugin()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                    #if ! JucePlugin_IsMidiEffect
                     #if ! JucePlugin_IsSynth
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                     #endif
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                    #endif
                      )
#endif
{
    channels = {
        { 0, "KICK", juce::Colours::red },
        { 1, "SNARE", juce::Colours::orange },
        { 2, "BASS", juce::Colours::yellow },
        { 3, "GTR L", juce::Colours::green },
        { 4, "GTR R", juce::Colours::cyan },
        { 5, "KEYS", juce::Colours::blue },
        { 6, "VOX", juce::Colours::purple },
        { 7, "FX BUS", juce::Colours::pink }
    };
}

THDAnalyzerPlugin::~THDAnalyzerPlugin() = default;

void THDAnalyzerPlugin::setChannelId (int id)
{
    if (juce::isPositiveAndBelow (id, static_cast<int> (channels.size())))
    {
        channelId = id;
        channels[0].channelId = id;
    }
}

void THDAnalyzerPlugin::sendTHDDataToMaster (const FFTAnalyzer::AnalysisResult& analysis, float peakLevel)
{
    if (pluginMode != PluginMode::ChannelStrip)
        return;

    THDDataMessage msg;
    msg.channelId = channelId;
    msg.thd = analysis.thd;
    msg.thdN = analysis.thdN;
    msg.level = analysis.level;
    msg.peakLevel = peakLevel;

    for (size_t i = 0; i < msg.harmonics.size(); ++i)
        msg.harmonics[i] = i < analysis.harmonics.size() ? analysis.harmonics[i] : 0.0f;

    juce::MidiMessage midiMsg;
    msg.toMidiBytes (midiMsg);
    midiOutputBuffer.addEvent (midiMsg, 0);
}

void THDAnalyzerPlugin::receiveTHDData (const juce::MidiMessage& midi)
{
    if (pluginMode != PluginMode::MasterBrain)
        return;

    THDDataMessage msg;
    if (! msg.fromMidiBytes (midi))
        return;

    if (! juce::isPositiveAndBelow (msg.channelId, static_cast<int> (channels.size())))
        return;

    auto& channel = channels[static_cast<size_t> (msg.channelId)];
    channel.thd = msg.thd;
    channel.thdN = msg.thdN;
    channel.level = msg.level;
    channel.peakLevel = msg.peakLevel;

    for (size_t i = 0; i < channel.harmonics.size() && i < msg.harmonics.size(); ++i)
        channel.harmonics[i] = msg.harmonics[i];
}

void THDAnalyzerPlugin::prepareToPlay (double, int)
{
    std::fill (analysisFifo.begin(), analysisFifo.end(), 0.0f);
    fifoWritePosition = 0;
    fifoFilled = false;
    midiOutputBuffer.clear();

    for (auto& channel : channels)
    {
        channel.thd = 0.0;
        channel.thdN = 0.0;
        channel.level = 0.0;
        channel.peakLevel = 0.0;
        std::fill (channel.harmonics.begin(), channel.harmonics.end(), 0.0);
    }
}

void THDAnalyzerPlugin::releaseResources()
{
    midiOutputBuffer.clear();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool THDAnalyzerPlugin::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    const auto inputSet = layouts.getMainInputChannelSet();
    const auto outputSet = layouts.getMainOutputChannelSet();

    if (inputSet.isDisabled() || outputSet.isDisabled())
        return false;

    return inputSet == outputSet;
   #endif
}
#endif

void THDAnalyzerPlugin::pushSamplesToAnalysisFifo (const std::vector<float>& monoBuffer)
{
    for (const auto sample : monoBuffer)
    {
        analysisFifo[static_cast<size_t> (fifoWritePosition)] = sample;
        fifoWritePosition = (fifoWritePosition + 1) % FFTAnalyzer::fftSize;

        if (fifoWritePosition == 0)
            fifoFilled = true;
    }
}

void THDAnalyzerPlugin::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const auto numSamples = buffer.getNumSamples();
    std::vector<float> monoBuffer (static_cast<size_t> (numSamples), 0.0f);

    float peakLevel = 0.0f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* channelData = buffer.getReadPointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            monoBuffer[static_cast<size_t> (i)] += channelData[i];
            peakLevel = juce::jmax (peakLevel, std::abs (channelData[i]));
        }
    }

    if (totalNumInputChannels > 0)
    {
        const auto invChannels = 1.0f / static_cast<float> (totalNumInputChannels);
        for (auto& sample : monoBuffer)
            sample *= invChannels;
    }

    pushSamplesToAnalysisFifo (monoBuffer);

    if (fifoFilled)
    {
        std::array<float, FFTAnalyzer::fftSize> orderedSamples {};

        for (int i = 0; i < FFTAnalyzer::fftSize; ++i)
        {
            const int index = (fifoWritePosition + i) % FFTAnalyzer::fftSize;
            orderedSamples[static_cast<size_t> (i)] = analysisFifo[static_cast<size_t> (index)];
        }

        lastAnalysis = fftAnalyzer.analyze (orderedSamples.data(), FFTAnalyzer::fftSize, static_cast<float> (getSampleRate()));
    }

    if (pluginMode == PluginMode::ChannelStrip)
    {
        sendTHDDataToMaster (lastAnalysis, peakLevel);

        midiMessages.clear();
        midiMessages.addEvents (midiOutputBuffer, 0, numSamples, 0);
        midiOutputBuffer.clear();
    }
    else if (pluginMode == PluginMode::MasterBrain)
    {
        for (const auto metadata : midiMessages)
            receiveTHDData (metadata.getMessage());
    }
}

bool THDAnalyzerPlugin::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* THDAnalyzerPlugin::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

const juce::String THDAnalyzerPlugin::getName() const
{
    return JucePlugin_Name;
}

bool THDAnalyzerPlugin::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool THDAnalyzerPlugin::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool THDAnalyzerPlugin::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double THDAnalyzerPlugin::getTailLengthSeconds() const
{
    return 0.0;
}

int THDAnalyzerPlugin::getNumPrograms()
{
    return 1;
}

int THDAnalyzerPlugin::getCurrentProgram()
{
    return 0;
}

void THDAnalyzerPlugin::setCurrentProgram (int)
{
}

const juce::String THDAnalyzerPlugin::getProgramName (int)
{
    return {};
}

void THDAnalyzerPlugin::changeProgramName (int, const juce::String&)
{
}

void THDAnalyzerPlugin::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("THDAnalyzerSettings");
    xml.setAttribute ("pluginMode", static_cast<int> (pluginMode));
    xml.setAttribute ("channelId", channelId);

    for (const auto& channel : channels)
    {
        auto* channelXml = xml.createNewChildElement ("channel");
        channelXml->setAttribute ("id", channel.channelId);
        channelXml->setAttribute ("muted", channel.muted);
        channelXml->setAttribute ("soloed", channel.soloed);
    }

    copyXmlToBinary (xml, destData);
}

void THDAnalyzerPlugin::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr)
        return;

    pluginMode = static_cast<PluginMode> (xml->getIntAttribute ("pluginMode", static_cast<int> (PluginMode::ChannelStrip)));
    setChannelId (xml->getIntAttribute ("channelId", 0));

    for (auto* channelXml : xml->getChildIterator())
    {
        const auto id = channelXml->getIntAttribute ("id");
        if (! juce::isPositiveAndBelow (id, static_cast<int> (channels.size())))
            continue;

        channels[static_cast<size_t> (id)].muted = channelXml->getBoolAttribute ("muted", false);
        channels[static_cast<size_t> (id)].soloed = channelXml->getBoolAttribute ("soloed", false);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new THDAnalyzerPlugin();
}
