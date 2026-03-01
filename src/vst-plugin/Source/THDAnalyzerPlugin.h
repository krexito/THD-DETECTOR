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
#include <array>
#include <vector>
#include <cmath>
#include <cstring>

//==============================================================================
// FFT Analyzer Class - Ported from TypeScript implementation
//==============================================================================
class FFTAnalyzer
{
public:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;

    FFTAnalyzer()
        : fft (fftOrder)
    {
        windowBuffer.resize (fftSize, 0.0f);
        fftData.resize (fftSize * 2, 0.0f);

        for (int i = 0; i < fftSize; ++i)
            windowBuffer[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * static_cast<float> (i) / static_cast<float> (fftSize - 1)));
    }

    struct AnalysisResult
    {
        float fundamentalFrequency = 0.0f;
        float thd = 0.0f;
        float thdN = 0.0f;
        float level = 0.0f;
        std::vector<float> harmonics = std::vector<float> (7, 0.0f); // H2-H8
        float noiseFloor = 0.0f;
    };

    AnalysisResult analyze (const float* input, int numSamples, float sampleRate)
    {
        AnalysisResult result;

        if (input == nullptr || numSamples < fftSize || sampleRate <= 0.0f)
            return result;

        std::fill (fftData.begin(), fftData.end(), 0.0f);

        for (int i = 0; i < fftSize; ++i)
            fftData[i] = input[i] * windowBuffer[i];

        fft.performRealOnlyForwardTransform (fftData.data());

        std::vector<float> magnitude (fftSize / 2, 0.0f);
        for (int i = 0; i < fftSize / 2; ++i)
        {
            const float real = fftData[i * 2];
            const float imag = fftData[(i * 2) + 1];
            magnitude[i] = std::sqrt ((real * real) + (imag * imag));
        }

        const int minBin = juce::jlimit (1, (fftSize / 2) - 1, static_cast<int> ((20.0f * static_cast<float> (fftSize)) / sampleRate));
        const int maxBin = juce::jlimit (minBin, (fftSize / 2) - 1, static_cast<int> ((2000.0f * static_cast<float> (fftSize)) / sampleRate));

        float maxMag = 0.0f;
        int fundamentalBin = 0;

        for (int i = minBin; i <= maxBin; ++i)
        {
            if (magnitude[i] > maxMag)
            {
                maxMag = magnitude[i];
                fundamentalBin = i;
            }
        }

        result.fundamentalFrequency = static_cast<float> (fundamentalBin) * sampleRate / static_cast<float> (fftSize);

        float sumSquares = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            sumSquares += input[i] * input[i];

        result.level = std::sqrt (sumSquares / static_cast<float> (numSamples));

        if (result.fundamentalFrequency <= 0.0f || result.level <= 0.0001f || maxMag <= 0.0f)
            return result;

        float harmonicSum = 0.0f;

        for (int harmonic = 2; harmonic <= 8; ++harmonic)
        {
            const int harmonicBin = static_cast<int> (static_cast<float> (harmonic) * result.fundamentalFrequency * static_cast<float> (fftSize) / sampleRate);
            if (harmonicBin >= 1 && harmonicBin < fftSize / 2)
            {
                const float harmonicMag = magnitude[harmonicBin];
                result.harmonics[static_cast<size_t> (harmonic - 2)] = harmonicMag;
                harmonicSum += harmonicMag * harmonicMag;
            }
        }

        const float harmonicLevel = std::sqrt (harmonicSum);
        result.thd = (harmonicLevel / maxMag) * 100.0f;

        float noiseSum = 0.0f;
        int noiseBins = 0;

        for (int i = minBin; i < fftSize / 2; ++i)
        {
            bool isHarmonicRegion = false;

            for (int harmonic = 1; harmonic <= 8; ++harmonic)
            {
                const int harmonicBin = static_cast<int> (static_cast<float> (harmonic) * result.fundamentalFrequency * static_cast<float> (fftSize) / sampleRate);
                if (std::abs (i - harmonicBin) < 10)
                {
                    isHarmonicRegion = true;
                    break;
                }
            }

            if (! isHarmonicRegion)
            {
                noiseSum += magnitude[i] * magnitude[i];
                ++noiseBins;
            }
        }

        const float noiseLevel = noiseBins > 0 ? std::sqrt (noiseSum / static_cast<float> (noiseBins)) : 0.0f;
        result.thdN = ((harmonicLevel + noiseLevel) / maxMag) * 100.0f;
        result.noiseFloor = noiseLevel;

        return result;
    }

private:
    juce::dsp::FFT fft;
    std::vector<float> windowBuffer;
    std::vector<float> fftData;
};

struct ChannelData
{
    int channelId = 0;
    juce::String channelName;
    double thd = 0.0;
    double thdN = 0.0;
    double level = 0.0;
    double peakLevel = 0.0;
    std::vector<double> harmonics = std::vector<double> (7, 0.0);
    bool muted = false;
    bool soloed = false;
    juce::Colour channelColor;

    ChannelData() = default;

    ChannelData (int id, juce::String name, juce::Colour color)
        : channelId (id), channelName (std::move (name)), channelColor (color)
    {
    }
};

enum class PluginMode
{
    ChannelStrip,
    MasterBrain
};

struct THDDataMessage
{
    int channelId = 0;
    float thd = 0.0f;
    float thdN = 0.0f;
    float level = 0.0f;
    float peakLevel = 0.0f;
    std::array<float, 7> harmonics {};

    void toMidiBytes (juce::MidiMessage& midi) const
    {
        std::vector<uint8_t> sysex;
        sysex.reserve (49);

        sysex.push_back (0xF0);
        sysex.push_back (0x7D);
        sysex.push_back (0x01);
        sysex.push_back (static_cast<uint8_t> (channelId));

        const auto appendFloat = [&sysex] (float value)
        {
            std::array<uint8_t, sizeof (float)> bytes {};
            std::memcpy (bytes.data(), &value, sizeof (float));
            sysex.insert (sysex.end(), bytes.begin(), bytes.end());
        };

        appendFloat (thd);
        appendFloat (thdN);
        appendFloat (level);
        appendFloat (peakLevel);

        for (const auto harmonic : harmonics)
            appendFloat (harmonic);

        sysex.push_back (0xF7);
        midi = juce::MidiMessage (sysex.data(), static_cast<int> (sysex.size()), 0.0);
    }

    bool fromMidiBytes (const juce::MidiMessage& midi)
    {
        if (! midi.isSysEx())
            return false;

        const auto* data = midi.getSysExData();
        const int size = midi.getSysExDataSize();

        constexpr int payloadSize = 1 + (4 * 4) + (7 * 4);
        if (data == nullptr || size < payloadSize + 2)
            return false;

        if (data[0] != 0x7D || data[1] != 0x01)
            return false;

        int pos = 2;
        channelId = data[pos++];

        const auto readFloat = [&data, size, &pos] (float& destination)
        {
            if (pos + static_cast<int> (sizeof (float)) > size)
                return false;

            std::memcpy (&destination, data + pos, sizeof (float));
            pos += static_cast<int> (sizeof (float));
            return true;
        };

        if (! readFloat (thd) || ! readFloat (thdN) || ! readFloat (level) || ! readFloat (peakLevel))
            return false;

        for (auto& harmonic : harmonics)
        {
            if (! readFloat (harmonic))
                return false;
        }

        return true;
    }
};

class THDAnalyzerPlugin : public juce::AudioProcessor
{
public:
    THDAnalyzerPlugin();
    ~THDAnalyzerPlugin() override;

    void setPluginMode (PluginMode mode) { pluginMode = mode; }
    PluginMode getPluginMode() const { return pluginMode; }

    void setChannelId (int id);
    int getChannelId() const { return channelId; }

    void sendTHDDataToMaster (const FFTAnalyzer::AnalysisResult& analysis, float peakLevel);
    void receiveTHDData (const juce::MidiMessage& midi);

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    void pushSamplesToAnalysisFifo (const std::vector<float>& monoBuffer);

    FFTAnalyzer fftAnalyzer;
    FFTAnalyzer::AnalysisResult lastAnalysis;

    PluginMode pluginMode = PluginMode::ChannelStrip;
    int channelId = 0;
    juce::MidiBuffer midiOutputBuffer;

    std::array<float, FFTAnalyzer::fftSize> analysisFifo {};
    int fifoWritePosition = 0;
    bool fifoFilled = false;

    std::vector<ChannelData> channels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (THDAnalyzerPlugin)
};
