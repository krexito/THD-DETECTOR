/* ==============================================================================
   THD Analyzer VST Plugin Implementation
   ============================================================================== */

#include "THDAnalyzerPlugin.h"
#include "THDAnalyzerPluginEditor.h"
#include <algorithm>

std::array<THDAnalyzerPlugin::SharedChannelState, THDAnalyzerPlugin::maxDynamicChannels> THDAnalyzerPlugin::sharedChannelStates {};
juce::SpinLock THDAnalyzerPlugin::sharedChannelStatesLock;
std::atomic<uint32_t> THDAnalyzerPlugin::nextInstanceId { 1 };

THDAnalyzerPlugin::THDAnalyzerPlugin()
    : AudioProcessor (BusesProperties()
                    #if ! JucePlugin_IsMidiEffect
                     #if ! JucePlugin_IsSynth
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                     #endif
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                    #endif
                      )
    , state (*this, nullptr, "THDAnalyzerParameters", createParameterLayout())
    , instanceId (nextInstanceId.fetch_add (1, std::memory_order_relaxed))
{
    cacheParameterPointers();

    state.addParameterListener ("pluginMode", this);
    state.addParameterListener ("channelId", this);

    channels.clear();

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
        juce::StringArray { "Channel", "Master Brain" },
        static_cast<int> (PluginMode::ChannelStrip)));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "channelId", 1 },
        "Channel ID",
        0,
        maxDynamicChannels - 1,
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
        cachedChannelId.store (juce::jlimit (0, maxDynamicChannels - 1, static_cast<int> (channelIdParamValue->load())), std::memory_order_release);

    const juce::SpinLock::ScopedLockType lock (analysisDataLock);
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
    if (juce::isPositiveAndBelow (id, maxDynamicChannels))
    {
        if (auto* channelParam = state.getParameter ("channelId"))
            channelParam->setValueNotifyingHost (state.getParameterRange ("channelId").convertTo0to1 (static_cast<float> (id)));

        cachedChannelId.store (id, std::memory_order_release);
        ensureChannelExists (id);
    }
}

int THDAnalyzerPlugin::getChannelId() const
{
    return cachedChannelId.load (std::memory_order_acquire);
}

bool THDAnalyzerPlugin::isEditorDataReady() const noexcept
{
    return editorDataReady.load (std::memory_order_acquire);
}

FFTAnalyzer::AnalysisResult THDAnalyzerPlugin::getLastAnalysisResult() const
{
    const juce::SpinLock::ScopedLockType lock (analysisDataLock);
    return lastAnalysis;
}

std::vector<ChannelData> THDAnalyzerPlugin::getChannelsSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock (analysisDataLock);
    return channels;
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


void THDAnalyzerPlugin::publishSharedChannelData (const FFTAnalyzer::AnalysisResult& analysis, float peakLevel)
{
    if (getPluginMode() != PluginMode::ChannelStrip)
        return;

    const auto channelId = getChannelId();
    if (! juce::isPositiveAndBelow (channelId, maxDynamicChannels))
        return;

    const juce::SpinLock::ScopedLockType lock (sharedChannelStatesLock);
    auto& shared = sharedChannelStates[static_cast<size_t> (channelId)];
    shared.thd = analysis.thd;
    shared.thdN = analysis.thdN;
    shared.level = analysis.level;
    shared.peakLevel = peakLevel;
    for (size_t i = 0; i < shared.harmonics.size(); ++i)
        shared.harmonics[i] = i < analysis.harmonics.size() ? analysis.harmonics[i] : 0.0f;

    ++shared.sequence;
    shared.lastPublishMs = juce::Time::getMillisecondCounterHiRes();
    shared.publisherInstanceId = instanceId;
    shared.active = true;
}

void THDAnalyzerPlugin::ingestSharedChannelData()
{
    if (getPluginMode() != PluginMode::MasterBrain)
        return;

    const juce::SpinLock::ScopedLockType sharedLock (sharedChannelStatesLock);

    for (int channelId = 0; channelId < maxDynamicChannels; ++channelId)
    {
        auto& shared = sharedChannelStates[static_cast<size_t> (channelId)];
        if (! shared.active || shared.sequence == 0)
            continue;

        const auto ageMs = juce::Time::getMillisecondCounterHiRes() - shared.lastPublishMs;
        if (ageMs > (channelStaleTimeoutSeconds * 1000.0))
        {
            shared.active = false;
            continue;
        }

        if (shared.publisherInstanceId == instanceId)
            continue;

        if (shared.sequence == consumedSharedSequences[static_cast<size_t> (channelId)])
            continue;

        consumedSharedSequences[static_cast<size_t> (channelId)] = shared.sequence;

        ensureChannelExists (channelId);

        const juce::SpinLock::ScopedLockType lock (analysisDataLock);
        auto it = std::find_if (channels.begin(), channels.end(), [channelId] (const ChannelData& c)
        {
            return c.channelId == channelId;
        });

        if (it == channels.end())
            continue;

        auto& channel = *it;
        channel.thd = shared.thd;
        channel.thdN = shared.thdN;
        channel.level = shared.level;
        channel.peakLevel = shared.peakLevel;
        channel.active = true;
        channel.lastUpdateSeconds = internalClockSeconds;

        for (size_t i = 0; i < channel.harmonics.size() && i < shared.harmonics.size(); ++i)
            channel.harmonics[i] = shared.harmonics[i];
    }
}

void THDAnalyzerPlugin::receiveTHDData (const juce::MidiMessage& midi)
{
    if (getPluginMode() != PluginMode::MasterBrain)
        return;

    THDDataMessage msg;
    if (! msg.fromMidiBytes (midi))
        return;

    if (msg.channelId < 0 || msg.channelId >= maxDynamicChannels)
        return;

    ensureChannelExists (msg.channelId);

    const juce::SpinLock::ScopedLockType lock (analysisDataLock);
    auto it = std::find_if (channels.begin(), channels.end(), [&msg] (const ChannelData& c)
    {
        return c.channelId == msg.channelId;
    });

    if (it == channels.end())
        return;

    auto& channel = *it;
    channel.thd = msg.thd;
    channel.thdN = msg.thdN;
    channel.level = msg.level;
    channel.peakLevel = msg.peakLevel;
    channel.active = true;
    channel.lastUpdateSeconds = internalClockSeconds;

    for (size_t i = 0; i < channel.harmonics.size() && i < msg.harmonics.size(); ++i)
        channel.harmonics[i] = msg.harmonics[i];
}

void THDAnalyzerPlugin::prepareToPlay (double, int)
{
    editorDataReady.store (false, std::memory_order_release);
    reset();
    editorDataReady.store (true, std::memory_order_release);
}

void THDAnalyzerPlugin::reset()
{
    std::fill (analysisFifo.begin(), analysisFifo.end(), 0.0f);
    std::fill (orderedSamplesScratch.begin(), orderedSamplesScratch.end(), 0.0f);
    monoBufferScratch.clear();

    {
        const juce::SpinLock::ScopedLockType lock (analysisDataLock);
        lastAnalysis = FFTAnalyzer::AnalysisResult {};
        realtimeAnalysisCache = FFTAnalyzer::AnalysisResult {};
    }
    fifoWritePosition = 0;
    fifoFilled = false;
    analysisSamplesSinceLastRun = 0;
    internalClockSeconds = 0.0;
    midiOutputBuffer.clear();
    consumedSharedSequences.fill (0);


    {
        const juce::SpinLock::ScopedLockType lock (analysisDataLock);
        for (auto& channel : channels)
        {
            channel.thd = 0.0;
            channel.thdN = 0.0;
            channel.level = 0.0;
            channel.peakLevel = 0.0;
            channel.active = false;
            channel.lastUpdateSeconds = 0.0;
            std::fill (channel.harmonics.begin(), channel.harmonics.end(), 0.0);
        }
    }
}

void THDAnalyzerPlugin::releaseResources()
{
    editorDataReady.store (false, std::memory_order_release);
    midiOutputBuffer.clear();
}

bool THDAnalyzerPlugin::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    if (layouts.inputBuses.size() != 1 || layouts.outputBuses.size() != 1)
        return false;

    const auto inputSet = layouts.getMainInputChannelSet();
    const auto outputSet = layouts.getMainOutputChannelSet();

    if (inputSet.isDisabled() || outputSet.isDisabled())
        return false;

    return inputSet == juce::AudioChannelSet::stereo()
        && outputSet == juce::AudioChannelSet::stereo();
   #endif
}

void THDAnalyzerPlugin::pushSamplesToAnalysisFifo (const std::vector<float>& monoBuffer)
{
    if (monoBuffer.empty())
        return;

    size_t srcOffset = 0;
    size_t samplesRemaining = monoBuffer.size();

    while (samplesRemaining > 0)
    {
        const auto writePos = static_cast<size_t> (fifoWritePosition);
        const auto contiguousSpace = static_cast<size_t> (FFTAnalyzer::fftSize) - writePos;
        const auto chunkSize = std::min (samplesRemaining, contiguousSpace);

        std::copy_n (monoBuffer.data() + srcOffset,
                     chunkSize,
                     analysisFifo.begin() + static_cast<int> (writePos));

        srcOffset += chunkSize;
        samplesRemaining -= chunkSize;
        fifoWritePosition = static_cast<int> ((writePos + chunkSize) % static_cast<size_t> (FFTAnalyzer::fftSize));

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

    if (const auto sr = getSampleRate(); sr > 0.0)
        internalClockSeconds += static_cast<double> (numSamples) / sr;

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

    const auto pluginMode = getPluginMode();
    const bool shouldAnalyzeAudio = pluginMode == PluginMode::ChannelStrip;
    if (shouldAnalyzeAudio)
        analysisSamplesSinceLastRun += numSamples;

    if (shouldAnalyzeAudio && fifoFilled && analysisSamplesSinceLastRun >= analysisHopSize)
    {
        analysisSamplesSinceLastRun = 0;

        for (int i = 0; i < FFTAnalyzer::fftSize; ++i)
        {
            const int index = (fifoWritePosition + i) % FFTAnalyzer::fftSize;
            orderedSamplesScratch[static_cast<size_t> (i)] = analysisFifo[static_cast<size_t> (index)];
        }

        const auto analysis = fftAnalyzer.analyze (orderedSamplesScratch.data(), FFTAnalyzer::fftSize, static_cast<float> (getSampleRate()));
        realtimeAnalysisCache = analysis;
        {
            const juce::SpinLock::ScopedLockType lock (analysisDataLock);
            lastAnalysis = analysis;
        }
    }

    if (pluginMode == PluginMode::ChannelStrip)
    {
        sendTHDDataToMaster (realtimeAnalysisCache, peakLevel);
        publishSharedChannelData (realtimeAnalysisCache, peakLevel);

        midiMessages.clear();
        midiMessages.addEvents (midiOutputBuffer, 0, numSamples, 0);
        midiOutputBuffer.clear();
    }
    else if (pluginMode == PluginMode::MasterBrain)
    {
        for (const auto metadata : midiMessages)
            receiveTHDData (metadata.getMessage());

        ingestSharedChannelData();
        pruneStaleChannels();
        midiMessages.clear();
    }
}

bool THDAnalyzerPlugin::hasEditor() const
{
    return true;
}


void THDAnalyzerPlugin::removeChannel (int id)
{
    const juce::SpinLock::ScopedLockType lock (analysisDataLock);
    channels.erase (std::remove_if (channels.begin(), channels.end(), [id] (const ChannelData& c)
    {
        return c.channelId == id;
    }), channels.end());
}

juce::Colour THDAnalyzerPlugin::colorForChannelId (int channelId)
{
    static const std::array<juce::Colour, 10> palette {
        juce::Colour::fromString ("fff97316"),
        juce::Colour::fromString ("ff60a5fa"),
        juce::Colour::fromString ("ffa78bfa"),
        juce::Colour::fromString ("ff34d399"),
        juce::Colour::fromString ("ff2dd4bf"),
        juce::Colour::fromString ("fffbbf24"),
        juce::Colour::fromString ("fff472b6"),
        juce::Colour::fromString ("ff94a3b8"),
        juce::Colour::fromString ("ff38bdf8"),
        juce::Colour::fromString ("fffb923c")
    };

    const auto index = static_cast<size_t> (juce::jmax (0, channelId)) % palette.size();
    return palette[index];
}

juce::String THDAnalyzerPlugin::defaultChannelNameForId (int channelId)
{
    return "CH " + juce::String (channelId + 1);
}

void THDAnalyzerPlugin::ensureChannelExists (int channelId)
{
    if (channelId < 0 || channelId >= maxDynamicChannels)
        return;

    const juce::SpinLock::ScopedLockType lock (analysisDataLock);

    const auto it = std::find_if (channels.begin(), channels.end(), [channelId] (const ChannelData& c)
    {
        return c.channelId == channelId;
    });

    if (it != channels.end())
        return;

    ChannelData channel (channelId, defaultChannelNameForId (channelId), colorForChannelId (channelId));
    channel.active = true;
    channel.lastUpdateSeconds = internalClockSeconds;
    channels.push_back (std::move (channel));

    std::sort (channels.begin(), channels.end(), [] (const ChannelData& a, const ChannelData& b)
    {
        return a.channelId < b.channelId;
    });
}

void THDAnalyzerPlugin::pruneStaleChannels()
{
    const juce::SpinLock::ScopedLockType lock (analysisDataLock);
    channels.erase (std::remove_if (channels.begin(), channels.end(), [this] (const ChannelData& c)
    {
        if (! c.active)
            return false;

        return (internalClockSeconds - c.lastUpdateSeconds) > channelStaleTimeoutSeconds;
    }), channels.end());
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

bool THDAnalyzerPlugin::restoreLegacyStateIfNeeded (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("THDAnalyzerSettings"))
        return false;

    const auto modeText = xml.getStringAttribute ("pluginMode", {}).trim().toLowerCase();
    if (modeText == "masterbrain")
        setPluginMode (PluginMode::MasterBrain);
    else if (modeText == "channelstrip")
        setPluginMode (PluginMode::ChannelStrip);

    const auto legacyChannelId = juce::jlimit (0, maxDynamicChannels - 1, xml.getIntAttribute ("channelId", getChannelId()));
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
