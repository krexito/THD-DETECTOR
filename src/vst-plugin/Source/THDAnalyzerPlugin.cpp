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
                       .withInput  ("Input",  AudioChannelSet::stereo(), 8)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), 8)
                     #endif
                       )
#endif
{
    initializeChannels();
    maxBufferSize = 0;
}

THDAnalyzerPlugin::~THDAnalyzerPlugin()
{
}

void THDAnalyzerPlugin::initializeChannels()
{
    // 8 default channels with colors
    channels.push_back(ChannelData(0, "KICK",  Colours::red));
    channels.push_back(ChannelData(1, "SNARE", Colours::orange));
    channels.push_back(ChannelData(2, "BASS", Colours::yellow));
    channels.push_back(ChannelData(3, "GTR L", Colours::green));
    channels.push_back(ChannelData(4, "GTR R", Colours::cyan));
    channels.push_back(ChannelData(5, "KEYS", Colours::blue));
    channels.push_back(ChannelData(6, "VOX", Colours::purple));
    channels.push_back(ChannelData(7, "FX BUS", Colours::magenta));
    
    // Create FFT analyzer for each channel
    for (int i = 0; i < 8; i++) {
        fftAnalyzers.push_back(FFTAnalyzer());
    }
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
    // Release any allocated resources
    channelBuffers.clear();
}

//==============================================================================
#ifndef JucePlugin_PreferredChannelConfigurations
bool THDAnalyzerPlugin::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Support mono and stereo configurations
    #if JucePlugin_IsMidiEffect
    return true;
    #else
    // This plugin supports 8 channel input/output
    if (layouts.getMainInputChannels() == 8 && layouts.getMainOutputChannels() == 8)
        return true;
    
    // Also support stereo
    if (layouts.getMainInputChannels() == 2 && layouts.getMainOutputChannels() == 2)
        return true;
    
    return false;
    #endif
}
#endif

void THDAnalyzerPlugin::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    // For this analyzer, we don't modify the audio - we just analyze it
    // But we need to copy to our buffer for analysis
    
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumInputChannels();
    
    // Clear unused channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Analyze each channel
    int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < totalNumInputChannels && channel < 8; channel++) {
        const float* channelData = buffer.getReadPointer(channel);
        
        // Get current sample rate
        double sampleRate = getSampleRate();
        
        // Perform FFT analysis
        if (channel < fftAnalyzers.size()) {
            auto result = fftAnalyzers[channel].analyze(channelData, numSamples, sampleRate);
            
            // Update channel data
            channels[channel].thd = result.thd;
            channels[channel].thdN = result.thdN;
            channels[channel].level = result.level;
            
            // Calculate peak level
            float peak = 0;
            for (int i = 0; i < numSamples; i++) {
                peak = std::max(peak, std::abs(channelData[i]));
            }
            channels[channel].peakLevel = peak;
            
            // Update harmonics
            if (result.harmonics.size() >= 7) {
                for (int h = 0; h < 7; h++) {
                    channels[channel].harmonics[h] = result.harmonics[h];
                }
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
