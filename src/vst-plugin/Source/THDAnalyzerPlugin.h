/* ==============================================================================
   THD Analyzer VST Plugin
   A real-time THD (Total Harmonic Distortion) analyzer
   Built with JUCE framework
   
   Two plugins:
   1. THD Channel Analyzer - for individual channel strips
   2. THD Master Brain - for master bus analysis
   ============================================================================== */

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <complex>
#include <dsp/FFT.h>

//==============================================================================
// FFT Analyzer Class - Ported from TypeScript implementation
//==============================================================================
class FFTAnalyzer {
public:
    FFTAnalyzer() : fftSize(8192), fft(13), windowBuffer(nullptr) {
        windowBuffer = new float[fftSize];
        fftData.resize(fftSize * 2);
        
        // Create Hanning window
        for (int i = 0; i < fftSize; i++) {
            windowBuffer[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (fftSize - 1)));
        }
    }
    
    ~FFTAnalyzer() {
        if (windowBuffer) delete[] windowBuffer;
    }
    
    struct AnalysisResult {
        float fundamentalFrequency;
        float thd;
        float thdN;
        float level;
        std::vector<float> harmonics; // H2-H8
        float noiseFloor;
    };
    
    AnalysisResult analyze(const float* audioBuffer, int bufferSize, float sampleRate) {
        AnalysisResult result = {0};
        result.harmonics.resize(7, 0.0f);
        
        if (bufferSize < fftSize) {
            // Zero-pad if needed
            return result;
        }
        
        // Apply window function and prepare FFT data
        for (int i = 0; i < fftSize; i++) {
            fftData[i * 2] = audioBuffer[i] * windowBuffer[i];  // real
            fftData[i * 2 + 1] = 0.0f;  // imag
        }
        
        // Perform FFT using JUCE's FFT
        fft.performRealOnlyForwardTransform(fftData.data());
        
        // Calculate magnitude spectrum
        std::vector<float> magnitude(fftSize / 2);
        for (int i = 0; i < fftSize / 2; i++) {
            float real = fftData[i * 2];
            float imag = fftData[i * 2 + 1];
            magnitude[i] = std::sqrt(real * real + imag * imag);
        }
        
        // Find fundamental frequency (20Hz - 2kHz range)
        int minBin = static_cast<int>(20.0f * fftSize / sampleRate);
        int maxBin = static_cast<int>(2000.0f * fftSize / sampleRate);
        
        float maxMag = 0;
        int fundamentalBin = 0;
        
        for (int i = minBin; i <= maxBin && i < fftSize / 2; i++) {
            if (magnitude[i] > maxMag) {
                maxMag = magnitude[i];
                fundamentalBin = i;
            }
        }
        
        result.fundamentalFrequency = fundamentalBin * sampleRate / fftSize;
        
        // Calculate RMS level
        float sumSquares = 0;
        for (int i = 0; i < bufferSize; i++) {
            sumSquares += audioBuffer[i] * audioBuffer[i];
        }
        result.level = std::sqrt(sumSquares / bufferSize);
        
        if (result.fundamentalFrequency > 0 && result.level > 0.0001f) {
            // Analyze harmonics (H2-H8)
            float harmonicSum = 0;
            
            for (int h = 2; h <= 8; h++) {
                int harmonicBin = static_cast<int>(h * result.fundamentalFrequency * fftSize / sampleRate);
                
                if (harmonicBin < fftSize / 2) {
                    float harmonicMag = magnitude[harmonicBin];
                    result.harmonics[h-2] = harmonicMag;
                    harmonicSum += harmonicMag * harmonicMag;
                }
            }
            
            // Calculate THD
            float harmonicLevel = std::sqrt(harmonicSum);
            result.thd = (harmonicLevel / maxMag) * 100.0f;
            
            // Estimate noise floor
            float noiseSum = 0;
            int noiseBins = 0;
            
            // Look at frequency regions away from fundamental and harmonics
            for (int i = minBin; i < fftSize / 2; i++) {
                bool isHarmonicRegion = false;
                for (int h = 1; h <= 8; h++) {
                    int harmonicBin = static_cast<int>(h * result.fundamentalFrequency * fftSize / sampleRate);
                    if (std::abs(i - harmonicBin) < 10) {
                        isHarmonicRegion = true;
                        break;
                    }
                }
                
                if (!isHarmonicRegion) {
                    noiseSum += magnitude[i] * magnitude[i];
                    noiseBins++;
                }
            }
            
            float noiseLevel = (noiseBins > 0) ? std::sqrt(noiseSum / noiseBins) : 0;
            
            // Calculate THD+N
            result.thdN = ((harmonicLevel + noiseLevel) / maxMag) * 100.0f;
            result.noiseFloor = noiseLevel;
        }
        
        return result;
    }
    
private:
    int fftSize;
    dsp::FFT fft;
    float* windowBuffer;
    std::vector<float> fftData;
};

//==============================================================================
// Channel Data Structure
//==============================================================================
struct ChannelData {
    int channelId;
    String channelName;
    double thd;
    double thdN;
    double level;
    double peakLevel;
    std::vector<double> harmonics;
    bool muted;
    bool soloed;
    Colour channelColor;
    
    ChannelData(int id, String name, Colour color) 
        : channelId(id), channelName(name), thd(0), thdN(0), level(0), 
          peakLevel(0), muted(false), soloed(false), channelColor(color) {
        harmonics.resize(7, 0.0);
    }
};

//==============================================================================
// Plugin Mode: Channel Strip vs Master Brain
//==============================================================================
enum class PluginMode {
    ChannelStrip,  // Sends THD data via MIDI to Master Brain
    MasterBrain   // Receives THD data from all Channel Strips
};

//==============================================================================
// THD Data Message for MIDI communication
//==============================================================================
struct THDDataMessage {
    int channelId;           // Which channel (0-7)
    float thd;               // THD percentage
    float thdN;              // THD+N percentage
    float level;             // Signal level (0-1)
    float peakLevel;         // Peak level (0-1)
    float harmonics[7];      // H2-H8
    
    THDDataMessage() : channelId(0), thd(0), thdN(0), level(0), peakLevel(0) {
        for(int i = 0; i < 7; i++) harmonics[i] = 0;
    }
    
    // Convert to MIDI message bytes
    void toMidiBytes(MidiMessage& midi) const {
        // Use MIDI System Exclusive for custom data
        std::vector<uint8> sysex;
        sysex.push_back(0xF0);  // Start SysEx
        sysex.push_back(0x7D);  // Manufacturer ID (non-commercial)
        sysex.push_back(0x01);  // THD Analyzer ID
        
        // Channel ID
        sysex.push_back(static_cast<uint8_t>(channelId));
        
        // THD (4 bytes float)
        uint8* thdBytes = (uint8*)&thd;
        for(int i = 0; i < 4; i++) sysex.push_back(thdBytes[i]);
        
        // THD+N (4 bytes float)
        uint8* thdNBytes = (uint8*)&thdN;
        for(int i = 0; i < 4; i++) sysex.push_back(thdNBytes[i]);
        
        // Level (4 bytes float)
        uint8* levelBytes = (uint8*)&level;
        for(int i = 0; i < 4; i++) sysex.push_back(levelBytes[i]);
        
        // Peak Level (4 bytes float)
        uint8* peakBytes = (uint8*)&peakLevel;
        for(int i = 0; i < 4; i++) sysex.push_back(peakBytes[i]);
        
        // Harmonics H2-H8 (7 * 4 = 28 bytes)
        for(int h = 0; h < 7; h++) {
            uint8* harmBytes = (uint8*)&harmonics[h];
            for(int i = 0; i < 4; i++) sysex.push_back(harmBytes[i]);
        }
        
        sysex.push_back(0xF7);  // End SysEx
        
        midi = MidiMessage(sysex.data(), sysex.size(), 0);
    }
    
    // Parse from MIDI message
    bool fromMidiBytes(const MidiMessage& midi) {
        if (!midi.isSysEx()) return false;
        
        const uint8* data = midi.getSysExData();
        int size = midi.getSysExDataSize();
        
        if (size < 49) return false;  // Minimum size check
        if (data[0] != 0x7D || data[1] != 0x01) return false;
        
        int pos = 2;
        channelId = data[pos++];
        
        // Parse THD
        memcpy(&thd, &data[pos], 4);
        pos += 4;
        
        // Parse THD+N
        memcpy(&thdN, &data[pos], 4);
        pos += 4;
        
        // Parse Level
        memcpy(&level, &data[pos], 4);
        pos += 4;
        
        // Parse Peak Level
        memcpy(&peakLevel, &data[pos], 4);
        pos += 4;
        
        // Parse Harmonics
        for(int h = 0; h < 7; h++) {
            memcpy(&harmonics[h], &data[pos], 4);
            pos += 4;
        }
        
        return true;
    }
};
class THDAnalyzerPlugin  : public AudioProcessor
{
public:
    //==============================================================================
    // Communication methods for Channel Strip ↔ Master Brain
    //==============================================================================
    
    // Set plugin mode (Channel Strip or Master Brain)
    void setPluginMode(PluginMode mode) { pluginMode = mode; }
    PluginMode getPluginMode() const { return pluginMode; }
    
    // Set channel ID for this channel strip (0-7)
    void setChannelId(int id) { 
        if (id >= 0 && id < 8) {
            channelId = id; 
            channels[0].channelId = id;
        }
    }
    int getChannelId() const { return channelId; }
    
    // For Channel Strip: Send THD data via MIDI
    void sendTHDDataToMaster(const FFTAnalyzer::AnalysisResult& analysis) {
        if (pluginMode != PluginMode::ChannelStrip) return;
        
        THDDataMessage msg;
        msg.channelId = channelId;
        msg.thd = analysis.thd;
        msg.thdN = analysis.thdN;
        msg.level = analysis.level;
        msg.peakLevel = analysis.level * 1.5f;  // Approximate peak
        
        for(int i = 0; i < 7; i++) {
            msg.harmonics[i] = (i < analysis.harmonics.size()) ? 
                               static_cast<float>(analysis.harmonics[i]) : 0.0f;
        }
        
        // Convert to MIDI and store for output
        MidiMessage midiMsg;
        msg.toMidiBytes(midiMsg);
        midiOutputBuffer.addEvent(midiMsg, 0);
    }
    
    // For Master Brain: Receive THD data from channel strips
    void receiveTHDData(const MidiMessage& midi) {
        if (pluginMode != PluginMode::MasterBrain) return;
        
        THDDataMessage msg;
        if (msg.fromMidiBytes(midi)) {
            // Update channel data for this channel
            if (msg.channelId >= 0 && msg.channelId < 8) {
                channels[msg.channelId].thd = msg.thd;
                channels[msg.channelId].thdN = msg.thdN;
                channels[msg.channelId].level = msg.level;
                channels[msg.channelId].peakLevel = msg.peakLevel;
                
                for(int i = 0; i < 7; i++) {
                    if (i < channels[msg.channelId].harmonics.size()) {
                        channels[msg.channelId].harmonics[i] = msg.harmonics[i];
                    }
                }
            }
        }
    }
    
    // Get MIDI output for DAW to receive
    MidiBuffer& getMidiOutput() { return midiOutputBuffer; }
    
    //==============================================================================
    THDAnalyzerPlugin();
    ~THDAnalyzerPlugin() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Flexible analyzer - works on whatever channel configuration it's inserted on
    // Use on channel strips for per-channel analysis, or on master for mix analysis
    FFTAnalyzer fftAnalyzer;
    
    // Buffer for audio analysis
    std::vector<float> audioBuffer;
    int bufferPosition;

    // Analysis results
    FFTAnalyzer::AnalysisResult lastAnalysis;
    
    // Communication system variables
    PluginMode pluginMode = PluginMode::ChannelStrip;
    int channelId = 0;
    MidiBuffer midiOutputBuffer;
    
    // Channel data for Master Brain mode
    std::vector<ChannelData> channels;
    int maxBufferSize;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (THDAnalyzerPlugin)
};
