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
    , state (*this, nullptr, "THDAnalyzerParameters", createParameterLayout())
#else
    : state (*this, nullptr, "THDAnalyzerParameters", createParameterLayout())
#endif
{
    cacheParameterPointers();

    state.addParameterListener ("pluginMode", this);
    state.addParameterListener ("channelId", this);

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

    for (size_t i = 0; i < channels.size(); ++i)
    {
        state.addParameterListener (channelMutedParamId (static_cast<int> (i)), this);
        state.addParameterListener (channelSoloedParamId (static_cast<int> (i)), this);
    }

    syncCachedParametersFromState();
}

THDAnalyzerPlugin::~THDAnalyzerPlugin()
{
    state.removeParameterListener ("pluginMode", this);
    state.removeParameterListener ("channelId", this);

    for (size_t i = 0; i < channels.size(); ++i)
    {
        state.removeParameterListener (channelMutedParamId (static_cast<int> (i)), this);
        state.removeParameterListener (channelSoloedParamId (static_cast<int> (i)), this);
    }
}

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

void THDAnalyzerPlugin::cacheParameterPointers()
{
    pluginModeParamValue = state.getRawParameterValue ("pluginMode");
    channelIdParamValue = state.getRawParameterValue ("channelId");

    for (size_t i = 0; i < channelMutedParamValues.size(); ++i)
    {
        channelMutedParamValues[i] = state.getRawParameterValue (channelMutedParamId (static_cast<int> (i)));
        channelSoloedParamValues[i] = state.getRawParameterValue (channelSoloedParamId (static_cast<int> (i)));
    }
}

void THDAnalyzerPlugin::parameterChanged (const juce::String&, float)
{
    syncCachedParametersFromState();
}

void THDAnalyzerPlugin::syncCachedParametersFromState()
{
    if (pluginModeParamValue != nullptr)
        cachedPluginMode.store (juce::jlimit (0, 1, static_cast<int> (pluginModeParamValue->load())), std::memory_order_release);

    if (channelIdParamValue != nullptr)
        cachedChannelId.store (juce::jlimit (0, 7, static_cast<int> (channelIdParamValue->load())), std::memory_order_release);

    for (size_t i = 0; i < channels.size(); ++i)
    {
        if (channelMutedParamValues[i] != nullptr)
            channels[i].muted = channelMutedParamValues[i]->load() >= 0.5f;

        if (channelSoloedParamValues[i] != nullptr)
            channels[i].soloed = channelSoloedParamValues[i]->load() >= 0.5f;
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
    return new juce::GenericAudioProcessorEditor (state);
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

bool THDAnalyzerPlugin::restoreLegacyStateIfNeeded (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("THDAnalyzerSettings"))
        return false;

    const auto modeText = xml.getStringAttribute ("pluginMode", {}).trim().toLowerCase();
    if (modeText == "masterbrain")
        setPluginMode (PluginMode::MasterBrain);
    else if (modeText == "channelstrip")
        setPluginMode (PluginMode::ChannelStrip);

    const auto legacyChannelId = juce::jlimit (0, 7, xml.getIntAttribute ("channelId", getChannelId()));
    setChannelId (legacyChannelId);

    forEachXmlChildElementWithTagName (xml, channelXml, "channel")
    {
        const int channelIndex = channelXml->getIntAttribute ("id", -1);
        if (! juce::isPositiveAndBelow (channelIndex, static_cast<int> (channels.size())))
            continue;

        if (auto* mutedParam = state.getParameter (channelMutedParamId (channelIndex)))
            mutedParam->setValueNotifyingHost (channelXml->getBoolAttribute ("muted", false) ? 1.0f : 0.0f);

        if (auto* soloedParam = state.getParameter (channelSoloedParamId (channelIndex)))
            soloedParam->setValueNotifyingHost (channelXml->getBoolAttribute ("soloed", false) ? 1.0f : 0.0f);
    }

    syncCachedParametersFromState();
    return true;
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

    if (restoreLegacyStateIfNeeded (*xml))
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
