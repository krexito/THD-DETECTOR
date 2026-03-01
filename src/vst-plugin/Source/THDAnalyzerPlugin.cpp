/* ==============================================================================
   THD Analyzer VST Plugin Implementation
   ============================================================================== */

#include "THDAnalyzerPlugin.h"
#include "THDAnalyzerPluginEditor.h"

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
    , state (*this, nullptr, "THDAnalyzerParameters", createParameterLayout())
#else
    : state (*this, nullptr, "THDAnalyzerParameters", createParameterLayout())
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

    syncCachedParametersFromState();
}

THDAnalyzerPlugin::~THDAnalyzerPlugin() = default;

juce::AudioProcessorValueTreeState::ParameterLayout THDAnalyzerPlugin::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "pluginMode", 1 },
        "Plugin Mode",
        juce::StringArray { "Channel Strip", "Master Brain" },
        static_cast<int> (PluginMode::ChannelStrip)));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "channelId", 1 },
        "Channel ID",
        0,
        7,
        0));

    for (int i = 0; i < 8; ++i)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { channelMutedParamId (i), 1 },
            "Channel " + juce::String (i + 1) + " Muted",
            false));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { channelSoloedParamId (i), 1 },
            "Channel " + juce::String (i + 1) + " Soloed",
            false));
    }

    return { params.begin(), params.end() };
}

juce::String THDAnalyzerPlugin::channelMutedParamId (int channelIndex)
{
    return "channelMuted" + juce::String (channelIndex);
}

juce::String THDAnalyzerPlugin::channelSoloedParamId (int channelIndex)
{
    return "channelSoloed" + juce::String (channelIndex);
}

void THDAnalyzerPlugin::syncCachedParametersFromState()
{
    if (const auto* modeParam = state.getRawParameterValue ("pluginMode"))
        cachedPluginMode.store (juce::jlimit (0, 1, static_cast<int> (modeParam->load())), std::memory_order_release);

    if (const auto* channelParam = state.getRawParameterValue ("channelId"))
        cachedChannelId.store (juce::jlimit (0, 7, static_cast<int> (channelParam->load())), std::memory_order_release);

    for (size_t i = 0; i < channels.size(); ++i)
    {
        if (const auto* mutedParam = state.getRawParameterValue (channelMutedParamId (static_cast<int> (i))))
            channels[i].muted = mutedParam->load() >= 0.5f;

        if (const auto* soloedParam = state.getRawParameterValue (channelSoloedParamId (static_cast<int> (i))))
            channels[i].soloed = soloedParam->load() >= 0.5f;
    }
}

void THDAnalyzerPlugin::setPluginMode (PluginMode mode)
{
    const auto normalized = state.getParameterRange ("pluginMode").convertTo0to1 (static_cast<float> (mode));
    if (auto* modeParam = state.getParameter ("pluginMode"))
    {
        modeParam->setValueNotifyingHost (normalized);
        cachedPluginMode.store (static_cast<int> (mode), std::memory_order_release);
    }
}

PluginMode THDAnalyzerPlugin::getPluginMode() const
{
    return static_cast<PluginMode> (cachedPluginMode.load (std::memory_order_acquire));
}

void THDAnalyzerPlugin::setChannelId (int id)
{
    if (juce::isPositiveAndBelow (id, static_cast<int> (channels.size())))
    {
        if (auto* channelParam = state.getParameter ("channelId"))
            channelParam->setValueNotifyingHost (state.getParameterRange ("channelId").convertTo0to1 (static_cast<float> (id)));

        cachedChannelId.store (id, std::memory_order_release);
        channels[0].channelId = id;
    }
}

int THDAnalyzerPlugin::getChannelId() const
{
    return cachedChannelId.load (std::memory_order_acquire);
}

void THDAnalyzerPlugin::sendTHDDataToMaster (const FFTAnalyzer::AnalysisResult& analysis, float peakLevel)
{
    if (getPluginMode() != PluginMode::ChannelStrip)
        return;

    THDDataMessage msg;
    msg.channelId = getChannelId();
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
    if (getPluginMode() != PluginMode::MasterBrain)
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
    reset();
}

void THDAnalyzerPlugin::reset()
{
    std::fill (analysisFifo.begin(), analysisFifo.end(), 0.0f);
    std::fill (orderedSamplesScratch.begin(), orderedSamplesScratch.end(), 0.0f);
    monoBufferScratch.clear();

    lastAnalysis = FFTAnalyzer::AnalysisResult {};
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

void THDAnalyzerPlugin::ensureScratchBuffers (int numSamples)
{
    if (numSamples <= 0)
    {
        monoBufferScratch.clear();
        return;
    }

    if (monoBufferScratch.size() != static_cast<size_t> (numSamples))
        monoBufferScratch.assign (static_cast<size_t> (numSamples), 0.0f);
    else
        std::fill (monoBufferScratch.begin(), monoBufferScratch.end(), 0.0f);
}

void THDAnalyzerPlugin::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const auto numSamples = buffer.getNumSamples();
    ensureScratchBuffers (numSamples);

    float peakLevel = 0.0f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const auto* channelData = buffer.getReadPointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            monoBufferScratch[static_cast<size_t> (i)] += channelData[i];
            peakLevel = juce::jmax (peakLevel, std::abs (channelData[i]));
        }
    }

    if (totalNumInputChannels > 0)
    {
        const auto invChannels = 1.0f / static_cast<float> (totalNumInputChannels);
        for (auto& sample : monoBufferScratch)
            sample *= invChannels;
    }

    pushSamplesToAnalysisFifo (monoBufferScratch);

    if (fifoFilled)
    {
        for (int i = 0; i < FFTAnalyzer::fftSize; ++i)
        {
            const int index = (fifoWritePosition + i) % FFTAnalyzer::fftSize;
            orderedSamplesScratch[static_cast<size_t> (i)] = analysisFifo[static_cast<size_t> (index)];
        }

        lastAnalysis = fftAnalyzer.analyze (orderedSamplesScratch.data(), FFTAnalyzer::fftSize, static_cast<float> (getSampleRate()));
    }

    syncCachedParametersFromState();

    if (getPluginMode() == PluginMode::ChannelStrip)
    {
        sendTHDDataToMaster (lastAnalysis, peakLevel);

        midiMessages.clear();
        midiMessages.addEvents (midiOutputBuffer, 0, numSamples, 0);
        midiOutputBuffer.clear();
    }
    else if (getPluginMode() == PluginMode::MasterBrain)
    {
        for (const auto metadata : midiMessages)
            receiveTHDData (metadata.getMessage());

        midiMessages.clear();
    }
}

bool THDAnalyzerPlugin::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* THDAnalyzerPlugin::createEditor()
{
    return new THDAnalyzerPluginEditor (*this);
}

const juce::AudioProcessorValueTreeState& THDAnalyzerPlugin::getValueTreeState() const noexcept
{
    return state;
}

juce::AudioProcessorValueTreeState& THDAnalyzerPlugin::getValueTreeState() noexcept
{
    return state;
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
    auto stateCopy = state.copyState();
    std::unique_ptr<juce::XmlElement> xml (stateCopy.createXml());
    copyXmlToBinary (*xml, destData);
}

void THDAnalyzerPlugin::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr)
        return;

    const auto restoredState = juce::ValueTree::fromXml (*xml);
    if (restoredState.isValid())
        state.replaceState (restoredState);

    syncCachedParametersFromState();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new THDAnalyzerPlugin();
}
