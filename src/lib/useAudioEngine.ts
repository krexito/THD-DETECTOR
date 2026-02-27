"use client";

import { useEffect, useRef } from "react";
import type { ChannelData } from "../components/ChannelStrip";

// Simulate THD based on drive, saturation, and character
export function computeTHD(
  drive: number,
  saturation: number,
  character: ChannelData["character"],
  level: number
): { thd: number; thdN: number; harmonics: number[] } {
  const driveNorm = drive / 100;
  const satNorm = saturation / 100;
  const levelNorm = level / 100;

  // Base THD from drive
  let baseTHD = driveNorm * driveNorm * 3.5;

  // Character modifiers
  const characterMult = {
    clean: 0.1,
    warm: 0.6,
    bright: 0.4,
    vintage: 1.2,
  }[character];

  baseTHD *= characterMult;
  baseTHD += satNorm * 0.8;
  baseTHD *= 0.5 + levelNorm * 0.5;

  // Add some randomness for realism
  baseTHD += (Math.random() - 0.5) * 0.02;
  baseTHD = Math.max(0.001, baseTHD);

  // Harmonic distribution based on character
  const harmonicProfiles: Record<ChannelData["character"], number[]> = {
    clean: [0.1, 0.05, 0.02, 0.01, 0.005, 0.002, 0.001],
    warm: [0.8, 0.4, 0.15, 0.06, 0.02, 0.01, 0.005],
    bright: [0.3, 0.6, 0.4, 0.2, 0.1, 0.05, 0.02],
    vintage: [0.9, 0.5, 0.3, 0.15, 0.08, 0.04, 0.02],
  };

  const profile = harmonicProfiles[character];
  const harmonics = profile.map(
    (h) =>
      h * baseTHD * (1 + (Math.random() - 0.5) * 0.1)
  );

  // THD+N adds noise floor
  const noiseFloor = 0.002 + (1 - levelNorm) * 0.01;
  const thdN = baseTHD + noiseFloor;

  return {
    thd: Math.min(baseTHD, 10),
    thdN: Math.min(thdN, 10),
    harmonics,
  };
}

// Compute master THD from all active channels
export function computeMasterTHD(
  channels: ChannelData[],
  sumMode: "analog" | "digital" | "vintage" | "transformer",
  sumDrive: number,
  sumSaturation: number
): { thd: number; thdN: number } {
  const activeChannels = channels.filter((c) => !c.muted);
  if (activeChannels.length === 0) return { thd: 0, thdN: 0 };

  // Sum THD contributions (RSS method)
  const sumTHDSquared = activeChannels.reduce(
    (acc, ch) => acc + ch.thd * ch.thd,
    0
  );
  let masterTHD = Math.sqrt(sumTHDSquared / activeChannels.length);

  // Sum mode adds its own character
  const sumModeMult = {
    analog: 1.2,
    digital: 0.3,
    vintage: 1.8,
    transformer: 1.5,
  }[sumMode];

  const sumDriveNorm = sumDrive / 100;
  const sumSatNorm = sumSaturation / 100;

  masterTHD *= sumModeMult;
  masterTHD += sumDriveNorm * sumDriveNorm * 0.5;
  masterTHD += sumSatNorm * 0.3;
  masterTHD += (Math.random() - 0.5) * 0.01;

  const noiseFloor = 0.003;
  const masterTHDN = masterTHD + noiseFloor;

  return {
    thd: Math.max(0.001, Math.min(masterTHD, 10)),
    thdN: Math.max(0.001, Math.min(masterTHDN, 10)),
  };
}

// Simulate level with movement
export function simulateLevel(
  baseLevel: number,
  time: number,
  channelId: string
): { level: number; peak: number } {
  const seed = channelId.charCodeAt(0) * 0.1;
  const wave1 = Math.sin(time * 0.8 + seed) * 15;
  const wave2 = Math.sin(time * 1.3 + seed * 2) * 8;
  const wave3 = Math.sin(time * 2.1 + seed * 3) * 4;
  const noise = (Math.random() - 0.5) * 5;

  const level = Math.max(
    0,
    Math.min(100, baseLevel + wave1 + wave2 + wave3 + noise)
  );
  const peak = Math.min(100, level + Math.random() * 5);

  return { level, peak };
}

export function useAudioEngine(
  channels: ChannelData[],
  onUpdate: (updates: Map<string, Partial<ChannelData>>) => void,
  masterLevel: number,
  onMasterUpdate: (level: number, peak: number) => void
) {
  const timeRef = useRef(0);
  const rafRef = useRef<number>(0);
  const channelsRef = useRef(channels);
  const onUpdateRef = useRef(onUpdate);
  const onMasterUpdateRef = useRef(onMasterUpdate);
  const masterLevelRef = useRef(masterLevel);

  useEffect(() => {
    channelsRef.current = channels;
  }, [channels]);

  useEffect(() => {
    onUpdateRef.current = onUpdate;
  }, [onUpdate]);

  useEffect(() => {
    onMasterUpdateRef.current = onMasterUpdate;
  }, [onMasterUpdate]);

  useEffect(() => {
    masterLevelRef.current = masterLevel;
  }, [masterLevel]);

  useEffect(() => {
    let running = true;

    function tick() {
      if (!running) return;
      timeRef.current += 0.016;
      const t = timeRef.current;

      const updates = new Map<string, Partial<ChannelData>>();

      channelsRef.current.forEach((ch) => {
        const { level, peak } = simulateLevel(ch.drive * 0.8, t, ch.id);
        const { thd, thdN, harmonics } = computeTHD(
          ch.drive,
          ch.saturation,
          ch.character,
          level
        );
        updates.set(ch.id, { level, peakLevel: peak, thd, thdN, harmonics });
      });

      onUpdateRef.current(updates);

      // Master level
      const ml = masterLevelRef.current;
      const masterWave =
        Math.sin(t * 0.5) * 10 + Math.sin(t * 1.1) * 5 + (Math.random() - 0.5) * 3;
      const newMasterLevel = Math.max(0, Math.min(100, ml + masterWave));
      const newMasterPeak = Math.min(100, newMasterLevel + Math.random() * 4);
      onMasterUpdateRef.current(newMasterLevel, newMasterPeak);

      rafRef.current = requestAnimationFrame(tick);
    }

    rafRef.current = requestAnimationFrame(tick);
    return () => {
      running = false;
      cancelAnimationFrame(rafRef.current);
    };
  }, []);
}
