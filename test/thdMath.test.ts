import { describe, expect, test } from "bun:test";
import {
  calculateCompensatedWindowedRms,
  calculateRms,
  computeThdFromLinearAmplitudes,
  dftBinPower,
  estimateToneRmsAtFrequency,
  foldFrequencyToNyquist,
  getCoherentGain,
  getHarmonicBins,
  getWindowRmsGain,
  goertzelPower,
} from "../src/lib/thdMath";

describe("THD math reference checks", () => {
  test("1 kHz fundamental @1.0 and 2 kHz harmonic @0.1 returns 10% (-20 dB)", () => {
    const result = computeThdFromLinearAmplitudes(1.0, [0.1]);

    expect(result.thdRatio).toBeCloseTo(0.1, 10);
    expect(result.thdPercent).toBeCloseTo(10, 10);
    expect(result.thdDb).toBeCloseTo(-20, 10);
  });

  test("harmonics above Nyquist are ignored in ignore mode and folded in fold mode", () => {
    const ignore = getHarmonicBins({
      fundamentalFrequency: 3000,
      sampleRate: 48000,
      fftSize: 4096,
      maxHarmonic: 10,
      aliasMode: "ignore",
    });

    const fold = getHarmonicBins({
      fundamentalFrequency: 3000,
      sampleRate: 48000,
      fftSize: 4096,
      maxHarmonic: 10,
      aliasMode: "fold",
    });

    expect(ignore.every((h) => h.frequency <= 24000)).toBe(true);
    expect(fold.some((h) => h.isAliased)).toBe(true);
    expect(foldFrequencyToNyquist(27000, 48000)).toBeCloseTo(21000, 10);
  });

  test("Goertzel power matches DFT bin power", () => {
    const sampleRate = 48000;
    const size = 4096;
    const targetFreq = 1000;
    const targetBin = Math.round((targetFreq * size) / sampleRate);

    const signal = new Float32Array(size);
    for (let n = 0; n < size; n += 1) {
      signal[n] = Math.sin((2 * Math.PI * targetBin * n) / size);
    }

    const goertzel = goertzelPower(signal, targetBin);
    const dft = dftBinPower(signal, targetBin);

    expect(goertzel / dft).toBeCloseTo(1, 6);
  });

  test("Hann coherent gain is near 0.5 and compensated RMS matches expected", () => {
    const sampleRate = 48000;
    const size = 4096;
    const frequency = 1000;
    const peakAmplitude = 1.0;

    const signal = new Float32Array(size);
    for (let n = 0; n < size; n += 1) {
      signal[n] = peakAmplitude * Math.sin((2 * Math.PI * frequency * n) / sampleRate);
    }

    const rms = estimateToneRmsAtFrequency(signal, sampleRate, frequency, "hann");
    expect(getCoherentGain("hann", size)).toBeCloseTo(0.5, 3);
    expect(rms).toBeCloseTo(1 / Math.sqrt(2), 2);
  });

  test("RMS vs peak consistency for pure sinusoid", () => {
    const sampleRate = 48000;
    const size = 4096;
    const frequency = 1500;
    const peakAmplitude = 0.8;

    const signal = new Float32Array(size);
    for (let n = 0; n < size; n += 1) {
      signal[n] = peakAmplitude * Math.sin((2 * Math.PI * frequency * n) / sampleRate);
    }

    const timeRms = calculateRms(signal);
    const freqRms = estimateToneRmsAtFrequency(signal, sampleRate, frequency, "hann");

    expect(timeRms).toBeCloseTo(peakAmplitude / Math.sqrt(2), 2);
    expect(freqRms).toBeCloseTo(peakAmplitude / Math.sqrt(2), 2);
  });

  test("window-compensated total RMS matches unwindowed RMS", () => {
    const sampleRate = 48000;
    const size = 4096;
    const f1 = 997;
    const f2 = 1994;

    const signal = new Float32Array(size);
    for (let n = 0; n < size; n += 1) {
      signal[n] =
        0.8 * Math.sin((2 * Math.PI * f1 * n) / sampleRate) +
        0.15 * Math.sin((2 * Math.PI * f2 * n) / sampleRate);
    }

    const unwindowed = calculateRms(signal);
    const compensated = calculateCompensatedWindowedRms(signal, "hann");

    expect(getWindowRmsGain("hann", size)).toBeCloseTo(Math.sqrt(3 / 8), 2);
    expect(compensated).toBeCloseTo(unwindowed, 2);
  });

  test("residual power clamp avoids negative sqrt domain", () => {
    const fundamentalRms = 0.25;
    const totalRms = Math.sqrt(fundamentalRms * fundamentalRms - 1e-12);
    const residual2 = Math.max(0, totalRms * totalRms - fundamentalRms * fundamentalRms);

    expect(residual2).toBe(0);
  });
});
