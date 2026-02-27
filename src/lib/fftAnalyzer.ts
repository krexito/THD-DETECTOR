"use client";

import { useEffect, useRef } from "react";

// FFT Analyzer using Web Audio API AnalyserNode
export class FFTAnalyzer {
  private audioContext: AudioContext | null = null;
  private analyserNode: AnalyserNode | null = null;
  private oscillators: Map<string, OscillatorNode> = new Map();
  private gains: Map<string, GainNode> = new Map();

  constructor() {
    if (typeof window !== "undefined") {
      this.audioContext = new (window.AudioContext || (window as any).webkitAudioContext)();
      if (this.audioContext) {
        this.analyserNode = this.audioContext.createAnalyser();
        this.analyserNode.fftSize = 32768;
        this.analyserNode.smoothingTimeConstant = 0.8;
        this.analyserNode.connect(this.audioContext.destination);
      }
    }
  }

  // Generate test signal for a channel (simulating audio input)
  generateTestSignal(channelId: string, frequency: number, amplitude: number): void {
    if (!this.audioContext || !this.analyserNode) return;

    // Stop existing oscillator if any
    this.stopSignal(channelId);

    const oscillator = this.audioContext.createOscillator();
    const gainNode = this.audioContext.createGain();

    // Add harmonics to simulate real audio
    oscillator.type = "sine";
    oscillator.frequency.setValueAtTime(frequency, this.audioContext.currentTime);
    
    // Add slight detuning for more realistic signal
    const detune = this.audioContext.createOscillator();
    detune.type = "sine";
    detune.frequency.setValueAtTime(frequency * 1.002, this.audioContext.currentTime);
    
    const detuneGain = this.audioContext.createGain();
    detuneGain.gain.setValueAtTime(amplitude * 0.3, this.audioContext.currentTime);
    
    gainNode.gain.setValueAtTime(amplitude, this.audioContext.currentTime);
    
    oscillator.connect(gainNode);
    detune.connect(detuneGain);
    detuneGain.connect(gainNode);
    gainNode.connect(this.analyserNode);
    
    oscillator.start();
    detune.start();
    
    this.oscillators.set(channelId, oscillator);
    this.gains.set(channelId, gainNode);
  }

  // Set amplitude for existing signal
  setAmplitude(channelId: string, amplitude: number): void {
    const gain = this.gains.get(channelId);
    if (gain && this.audioContext) {
      gain.gain.setTargetAtTime(amplitude, this.audioContext.currentTime, 0.01);
    }
  }

  // Stop signal for a channel
  stopSignal(channelId: string): void {
    const oscillator = this.oscillators.get(channelId);
    if (oscillator) {
      oscillator.stop();
      this.oscillators.delete(channelId);
    }
    const gain = this.gains.get(channelId);
    if (gain) {
      gain.disconnect();
      this.gains.delete(channelId);
    }
  }

  // Perform FFT analysis and calculate THD
  analyzeTHD(): { 
    thd: number; 
    thdN: number; 
    harmonics: number[]; 
    spectrum: Float32Array 
  } {
    if (!this.analyserNode || !this.audioContext) {
      return this.getDefaultAnalysis();
    }

    const bufferLength = this.analyserNode.frequencyBinCount;
    const frequencyData = new Float32Array(bufferLength);
    this.analyserNode.getFloatFrequencyData(frequencyData);

    const sampleRate = this.audioContext.sampleRate;
    const binWidth = sampleRate / (bufferLength * 2);

    // Find fundamental frequency (strongest peak in lower frequencies)
    const fundamentalBin = this.findFundamentalBin(frequencyData, binWidth);
    const fundamentalFreq = fundamentalBin * binWidth;
    const fundamentalMagnitude = frequencyData[fundamentalBin];

    if (fundamentalMagnitude < -100) {
      return this.getDefaultAnalysis();
    }

    // Calculate harmonic bins
    const harmonicBins = [2, 3, 4, 5, 6, 7, 8].map(h => 
      Math.round((fundamentalFreq * h) / binWidth)
    );

    // Extract harmonic magnitudes
    const harmonics: number[] = harmonicBins.map((bin, i) => {
      if (bin >= bufferLength) return -120;
      
      // Interpolate between bins for better precision
      const magnitude = bin > 0 && bin < bufferLength - 1
        ? this.interpolatePeak(frequencyData, bin)
        : frequencyData[bin];
      
      return magnitude - fundamentalMagnitude; // Relative to fundamental
    });

    // Calculate THD (Total Harmonic Distortion)
    let harmonicPower = 0;
    harmonics.forEach(h => {
      if (h > -120) {
        harmonicPower += Math.pow(10, h / 10);
      }
    });
    
    const thd = harmonicPower > 0 
      ? Math.sqrt(harmonicPower) * 100 
      : 0;

    // Estimate noise floor (average of high frequency bins)
    const noiseStartBin = Math.round(8000 / binWidth);
    const noiseEndBin = Math.round(20000 / binWidth);
    let noiseFloor = -120;
    if (noiseStartBin < noiseEndBin && noiseEndBin < bufferLength) {
      let noiseSum = 0;
      let noiseCount = 0;
      for (let i = noiseStartBin; i < noiseEndBin; i++) {
        if (frequencyData[i] > -120) {
          noiseSum += frequencyData[i];
          noiseCount++;
        }
      }
      if (noiseCount > 0) {
        noiseFloor = noiseSum / noiseCount;
      }
    }

    const noiseRelative = noiseFloor - fundamentalMagnitude;
    const thdN = Math.sqrt(
      harmonicPower + Math.pow(10, noiseRelative / 10)
    ) * 100;

    return {
      thd: Math.min(thd, 100),
      thdN: Math.min(thdN, 100),
      harmonics: harmonics.map(h => Math.max(0, Math.min(1, (h + 60) / 60))),
      spectrum: frequencyData
    };
  }

  private findFundamentalBin(data: Float32Array, binWidth: number): number {
    // Search for fundamental in typical audio range (20Hz - 2000Hz)
    const minBin = Math.round(20 / binWidth);
    const maxBin = Math.round(2000 / binWidth);
    
    let maxMagnitude = -Infinity;
    let maxBinIndex = minBin;

    for (let i = minBin; i <= maxBin && i < data.length; i++) {
      if (data[i] > maxMagnitude) {
        maxMagnitude = data[i];
        maxBinIndex = i;
      }
    }

    // Refine by checking neighbors
    if (maxBinIndex > 0 && maxBinIndex < data.length - 1) {
      if (data[maxBinIndex - 1] > data[maxBinIndex] && data[maxBinIndex - 1] > data[maxBinIndex + 1]) {
        return maxBinIndex - 1;
      } else if (data[maxBinIndex + 1] > data[maxBinIndex] && data[maxBinIndex + 1] > data[maxBinIndex - 1]) {
        return maxBinIndex + 1;
      }
    }

    return maxBinIndex;
  }

  private interpolatePeak(data: Float32Array, bin: number): number {
    if (bin <= 0 || bin >= data.length - 1) return data[bin];
    
    const alpha = data[bin - 1];
    const beta = data[bin];
    const gamma = data[bin + 1];
    
    const denominator = alpha - 2 * beta + gamma;
    if (Math.abs(denominator) < 0.0001) return beta;
    
    const p = 0.5 * (alpha - gamma) / denominator;
    
    return beta - (1/4) * (alpha - gamma) * p;
  }

  private getDefaultAnalysis() {
    return {
      thd: 0,
      thdN: 0,
      harmonics: [0, 0, 0, 0, 0, 0, 0],
      spectrum: new Float32Array(0)
    };
  }

  // Cleanup
  dispose(): void {
    this.oscillators.forEach(osc => osc.stop());
    this.oscillators.clear();
    this.gains.forEach(gain => gain.disconnect());
    this.gains.clear();
    if (this.audioContext) {
      this.audioContext.close();
    }
  }
}

// React hook for using FFT analyzer
export function useFFTAnalyzer(
  channels: { id: string; level: number }[],
  onAnalysis: (channelId: string, analysis: { 
    thd: number; 
    thdN: number; 
    harmonics: number[] 
  }) => void
) {
  const analyzerRef = useRef<FFTAnalyzer | null>(null);
  const rafRef = useRef<number>(0);
  const onAnalysisRef = useRef(onAnalysis);

  // Keep onAnalysis ref updated
  useEffect(() => {
    onAnalysisRef.current = onAnalysis;
  }, [onAnalysis]);

  useEffect(() => {
    analyzerRef.current = new FFTAnalyzer();

    // Setup test signals for each channel
    const baseFrequencies: { [key: string]: number } = {
      'kick': 60,
      'snare': 200,
      'bass': 80,
      'gtr-l': 330,
      'gtr-r': 330,
      'keys': 440,
      'vox': 220,
      'fx-bus': 550
    };

    channels.forEach(ch => {
      const freq = baseFrequencies[ch.id] || 440;
      const amplitude = ch.level / 100 * 0.5;
      analyzerRef.current?.generateTestSignal(ch.id, freq, amplitude);
    });

    // Start analysis loop
    const analyze = () => {
      if (!analyzerRef.current) return;
      
      const analysis = analyzerRef.current.analyzeTHD();
      
      // Apply same analysis to all channels for demo (in real app, each would have own analyzer)
      channels.forEach(ch => {
        onAnalysisRef.current(ch.id, analysis);
      });

      rafRef.current = requestAnimationFrame(analyze);
    };
    
    analyze();

    return () => {
      if (rafRef.current) {
        cancelAnimationFrame(rafRef.current);
      }
      analyzerRef.current?.dispose();
    };
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []); // Only run once on mount

  // Update levels in real-time (separate effect to avoid re-running analysis setup)
  useEffect(() => {
    channels.forEach(ch => {
      const amplitude = ch.level / 100 * 0.5;
      analyzerRef.current?.setAmplitude(ch.id, amplitude);
    });
  }, [channels]);
}
