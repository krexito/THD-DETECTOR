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
// THDAnalyzerPlugin Processor
//==============================================================================
class THDAnalyzerPlugin  : public AudioProcessor
{
public:
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
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (THDAnalyzerPlugin)
};
