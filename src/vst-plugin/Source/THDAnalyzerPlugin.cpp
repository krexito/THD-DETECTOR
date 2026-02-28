/* ==============================================================================
   THD Analyzer VST Plugin Implementation
   ============================================================================== */

#include "THDAnalyzerPlugin.h"

//==============================================================================
// Plugin Implementation
//==============================================================================

THDAnalyzerPlugin::THDAnalyzerPlugin()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), 0)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), 0)
                     #endif
                       )
#endif
{
    audioBuffer.clear();
    bufferPosition = 0;
}

THDAnalyzerPlugin::~THDAnalyzerPlugin()
{
}

//==============================================================================
void THDAnalyzerPlugin::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Ensure we have enough buffer space
    maxBufferSize = samplesPerBlock * getTotalNumInputChannels();
    channelBuffers.resize(maxBufferSize);
    
    // Reset all channel data
    for (auto& channel : channels) {
        channel.thd = 0;
        channel.thdN = 0;
        channel.level = 0;
        channel.peakLevel = 0;
        std::fill(channel.harmonics.begin(), channel.harmonics.end(), 0.0);
    }
}

void THDAnalyzerPlugin::releaseResources()
{
    // Prepare audio buffer for analysis
    audioBuffer.resize(8192, 0.0f);

//==============================================================================
#ifndef JucePlugin_PreferredChannelConfigurations
bool THDAnalyzerPlugin::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Support mono and stereo configurations
    #if JucePlugin_IsMidiEffect
    return true;
    #else
    // This plugin supports any channel configuration (mono, stereo, 5.1, etc.)
    if (layouts.getMainInputChannels() > 0 && layouts.getMainOutputChannels() > 0)
        return true;
    
    return false;
    #endif
}
#endif

void THDAnalyzerPlugin::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    // This plugin analyzes whatever audio comes into it
    // Works on any channel configuration (stereo, 5.1, etc.)
    // Use on channel strips or master bus - same analyzer works for both
    
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumInputChannels();
    
    // Clear unused channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Mix all input channels to mono for analysis
    int numSamples = buffer.getNumSamples();
    std::vector<float> monoBuffer(numSamples, 0.0f);
    
    // Mix down to mono
    for (int channel = 0; channel < totalNumInputChannels; channel++) {
        const float* channelData = buffer.getReadPointer(channel);
        for (int i = 0; i < numSamples; i++) {
            monoBuffer[i] += channelData[i];
        }
    }
    
    // Normalize by number of channels
    if (totalNumInputChannels > 0) {
        for (int i = 0; i < numSamples; i++) {
            monoBuffer[i] /= totalNumInputChannels;
        }
    }
    
    // Perform FFT analysis on mixed audio
    double sampleRate = getSampleRate();
    lastAnalysis = fftAnalyzer.analyze(monoBuffer.data(), numSamples, sampleRate);
    
    // Handle communication based on plugin mode
    if (pluginMode == PluginMode::ChannelStrip) {
        // Channel Strip: Send THD data to Master Brain via MIDI
        sendTHDDataToMaster(lastAnalysis);
        
        // Clear MIDI input (we don't need input in Channel Strip mode)
        midiMessages.clear();
        
        // Add our MIDI output to the buffer
        midiMessages.addEvents(midiOutputBuffer, 0, midiOutputBuffer.getNumEvents(), 0);
        midiOutputBuffer.clear();
        
    } else if (pluginMode == PluginMode::MasterBrain) {
        // Master Brain: Receive THD data from Channel Strips
        for (const MidiMessageMetadata metadata : midiMessages) {
            const MidiMessage& midi = metadata.getMessage();
            if (midi.isSysEx()) {
                receiveTHDData(midi);
            }
        }
    }
}

//==============================================================================
bool THDAnalyzerPlugin::hasEditor() const
{
    return true;
}

AudioProcessorEditor* THDAnalyzerPlugin::createEditor()
{
    return new GenericAudioProcessorEditor (*this);
}

//==============================================================================
const String THDAnalyzerPlugin::getName() const
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
    return 0;
}

//==============================================================================
int THDAnalyzerPlugin::getNumPrograms()
{
    return 1;
}

int THDAnalyzerPlugin::getCurrentProgram()
{
    return 0;
}

void THDAnalyzerPlugin::setCurrentProgram (int index)
{
}

const String THDAnalyzerPlugin::getProgramName (int index)
{
    return {};
}

void THDAnalyzerPlugin::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void THDAnalyzerPlugin::getStateInformation (MemoryBlock& destData)
{
    // Save plugin state
    XmlElement xml ("THDAnalyzerSettings");
    
    for (int i = 0; i < channels.size(); i++) {
        XmlElement* channelXml = xml.createNewChildElement("channel");
        channelXml->setAttribute("id", channels[i].channelId);
        channelXml->setAttribute("muted", channels[i].muted);
        channelXml->setAttribute("soloed", channels[i].soloed);
    }
    
    copyXmlToBinary (xml, destData);
}

void THDAnalyzerPlugin::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore plugin state
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    
    if (xml != nullptr) {
        for (auto* channelXml : xml->getChildIterator()) {
            int id = channelXml->getIntAttribute("id");
            if (id >= 0 && id < channels.size()) {
                channels[id].muted = channelXml->getBoolAttribute("muted", false);
                channels[id].soloed = channelXml->getBoolAttribute("soloed", false);
            }
        }
    }
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new THDAnalyzerPlugin();
}
