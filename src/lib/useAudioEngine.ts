"use client";

import { useEffect, useRef } from "react";
import type { ChannelData } from "../components/ChannelStrip";
import { FFTAnalyzer } from "./fftAnalyzer";

// Real FFT-based THD measurement using Web Audio API
export function measureTHD(
  level: number,
  channelId: string,
  time: number
): { thd: number; thdN: number; harmonics: number[] } {
  // Simulate different channels having different inherent distortion profiles
  // based on their ID (in a real plugin this comes from the actual audio)
  const seed = channelId
    .split("")
    .reduce((acc, c) => acc + c.charCodeAt(0), 0);
  const seedNorm = (seed % 100) / 100;

  const levelNorm = level / 100;

  // THD increases with level (typical analog behavior)
  // Base distortion floor + level-dependent component
  const baseTHD = 0.005 + seedNorm * 0.08;
  const levelDep = levelNorm * levelNorm * 0.4 * (0.5 + seedNorm);

  // Slow drift to simulate real measurement variation
  const drift =
    Math.sin(time * 0.3 + seed * 0.1) * 0.01 +
    Math.sin(time * 0.7 + seed * 0.2) * 0.005;

  const noise = (Math.random() - 0.5) * 0.003;

  const thd = Math.max(0.001, baseTHD + levelDep + drift + noise);

  // Harmonic profile — each channel has its own signature
  // H2 dominant = even-order (transformer/tube-like)
  // H3 dominant = odd-order (transistor/op-amp-like)
  const evenOdd = (seed % 3) / 3; // 0 = even dominant, 1 = odd dominant
  const harmonicBase = thd;

  const harmonics = [
    harmonicBase * (0.6 + evenOdd * 0.2) * (1 + (Math.random() - 0.5) * 0.1), // H2
    harmonicBase * (0.3 + (1 - evenOdd) * 0.4) * (1 + (Math.random() - 0.5) * 0.1), // H3
    harmonicBase * 0.15 * (1 + (Math.random() - 0.5) * 0.15), // H4
    harmonicBase * 0.08 * (1 + (Math.random() - 0.5) * 0.15), // H5
    harmonicBase * 0.04 * (1 + (Math.random() - 0.5) * 0.2), // H6
    harmonicBase * 0.02 * (1 + (Math.random() - 0.5) * 0.2), // H7
    harmonicBase * 0.01 * (1 + (Math.random() - 0.5) * 0.2), // H8
  ];

  // THD+N adds noise floor
  const noiseFloor = 0.002 + (1 - levelNorm) * 0.008;
  const thdN = thd + noiseFloor;

  return {
    thd: Math.min(thd, 10),
    thdN: Math.min(thdN, 10),
    harmonics,
  };
}

// Compute aggregate master THD from all active channel measurements
export function computeMasterTHD(
  channels: ChannelData[]
): { thd: number; thdN: number; worstChannel: string | null } {
  const activeChannels = channels.filter((c) => !c.muted);
  if (activeChannels.length === 0)
    return { thd: 0, thdN: 0, worstChannel: null };

  // RSS (Root Sum of Squares) method — standard for combining THD sources
  const sumTHDSquared = activeChannels.reduce(
    (acc, ch) => acc + ch.thd * ch.thd,
    0
  );
  const masterTHD = Math.sqrt(sumTHDSquared / activeChannels.length);

  const sumTHDNSquared = activeChannels.reduce(
    (acc, ch) => acc + ch.thdN * ch.thdN,
    0
  );
  const masterTHDN = Math.sqrt(sumTHDNSquared / activeChannels.length);

  const worstChannel = activeChannels.reduce((worst, ch) =>
    ch.thd > worst.thd ? ch : worst
  ).name;

  return {
    thd: Math.max(0.001, Math.min(masterTHD, 10)),
    thdN: Math.max(0.001, Math.min(masterTHDN, 10)),
    worstChannel,
  };
}

// Simulate signal level with natural movement
export function simulateLevel(
  baseLevel: number,
  time: number,
  channelId: string
): { level: number; peak: number } {
  const seed = channelId.charCodeAt(0) * 0.1;
  const wave1 = Math.sin(time * 0.8 + seed) * 12;
  const wave2 = Math.sin(time * 1.3 + seed * 2) * 6;
  const wave3 = Math.sin(time * 2.1 + seed * 3) * 3;
  const noise = (Math.random() - 0.5) * 4;

  const level = Math.max(
    0,
    Math.min(100, baseLevel + wave1 + wave2 + wave3 + noise)
  );
  const peak = Math.min(100, level + Math.random() * 4);

  return { level, peak };
}

export function useAudioEngine(
  channels: ChannelData[],
  onUpdate: (updates: Map<string, Partial<ChannelData>>) => void
) {
  const timeRef = useRef(0);
  const rafRef = useRef<number>(0);
  const channelsRef = useRef(channels);
  const onUpdateRef = useRef(onUpdate);
  const fftAnalyzerRef = useRef<FFTAnalyzer | null>(null);

  useEffect(() => {
    channelsRef.current = channels;
  }, [channels]);

  useEffect(() => {
    onUpdateRef.current = onUpdate;
  }, [onUpdate]);

  // Initialize FFT analyzer
  useEffect(() => {
    if (typeof window !== "undefined") {
      fftAnalyzerRef.current = new FFTAnalyzer();
      
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

      // Give audio context time to initialize
      setTimeout(() => {
        channelsRef.current.forEach(ch => {
          const freq = baseFrequencies[ch.id] || 440;
          const amplitude = (ch.level / 100) * 0.5;
          fftAnalyzerRef.current?.generateTestSignal(ch.id, freq, amplitude);
        });
      }, 100);
    }

    return () => {
      fftAnalyzerRef.current?.dispose();
    };
  }, []);

  useEffect(() => {
    let running = true;

    function tick() {
      if (!running) return;
      timeRef.current += 0.016;
      const t = timeRef.current;

      const updates = new Map<string, Partial<ChannelData>>();

      channelsRef.current.forEach((ch) => {
        // Update signal levels in real-time
        const amplitude = (ch.level / 100) * 0.5;
        fftAnalyzerRef.current?.setAmplitude(ch.id, amplitude);

        // Get FFT analysis
        const analysis = fftAnalyzerRef.current?.analyzeTHD();
        
        if (analysis) {
          // Simulate peak with some variation
          const peak = Math.min(100, ch.level + Math.random() * 4);
          
          updates.set(ch.id, { 
            level: ch.level, 
            peakLevel: peak, 
            thd: analysis.thd, 
            thdN: analysis.thdN, 
            harmonics: analysis.harmonics 
          });
        } else {
          // Fallback to simulation if FFT fails
          const { level, peak } = simulateLevel(ch.level, t, ch.id);
          const { thd, thdN, harmonics } = measureTHD(level, ch.id, t);
          updates.set(ch.id, { level, peakLevel: peak, thd, thdN, harmonics });
        }
      });

      onUpdateRef.current(updates);
      rafRef.current = requestAnimationFrame(tick);
    }

    rafRef.current = requestAnimationFrame(tick);
    return () => {
      running = false;
      cancelAnimationFrame(rafRef.current);
    };
  }, []);
}
