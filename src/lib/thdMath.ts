export interface THDResult {
  thdRatio: number;
  thdPercent: number;
  thdDb: number;
}

export interface HarmonicBin {
  order: number;
  frequency: number;
  aliasedFrequency: number;
  bin: number;
  isAliased: boolean;
}

export type WindowType = "rectangular" | "hann";

const windowCache = new Map<string, Float32Array>();

function getWindowCoefficients(size: number, windowType: WindowType): Float32Array {
  const key = `${windowType}:${size}`;
  const cached = windowCache.get(key);
  if (cached) return cached;

  const coeffs = new Float32Array(size);
  for (let i = 0; i < size; i += 1) {
    coeffs[i] = windowSample(i, size, windowType);
  }

  windowCache.set(key, coeffs);
  return coeffs;
}

export function computeThdFromLinearAmplitudes(
  fundamentalAmplitude: number,
  harmonicAmplitudes: number[],
): THDResult {
  if (!Number.isFinite(fundamentalAmplitude) || fundamentalAmplitude <= 0) {
    return { thdRatio: 0, thdPercent: 0, thdDb: -Infinity };
  }

  const harmonicPower = harmonicAmplitudes.reduce((acc, amplitude) => {
    if (!Number.isFinite(amplitude) || amplitude <= 0) return acc;
    return acc + amplitude * amplitude;
  }, 0);

  const thdRatio = Math.sqrt(harmonicPower) / fundamentalAmplitude;
  return {
    thdRatio,
    thdPercent: thdRatio * 100,
    thdDb: thdRatio > 0 ? 20 * Math.log10(thdRatio) : -Infinity,
  };
}

export function calculateRms(samples: Float32Array): number {
  if (samples.length === 0) return 0;
  const sumSquares = samples.reduce((acc, sample) => acc + sample * sample, 0);
  return Math.sqrt(sumSquares / samples.length);
}

export function getCoherentGain(windowType: WindowType, size: number): number {
  if (size <= 0) return 1;
  if (windowType === "rectangular") return 1;

  // Hann coherent gain approaches 0.5 for large N.
  let sum = 0;
  for (let n = 0; n < size; n += 1) {
    sum += 0.5 * (1 - Math.cos((2 * Math.PI * n) / (size - 1)));
  }
  return sum / size;
}

export function windowSample(index: number, size: number, windowType: WindowType): number {
  if (windowType === "rectangular") return 1;
  if (size <= 1) return 1;
  return 0.5 * (1 - Math.cos((2 * Math.PI * index) / (size - 1)));
}

export function estimateToneRmsAtFrequency(
  samples: Float32Array,
  sampleRate: number,
  frequency: number,
  windowType: WindowType = "hann",
): number {
  const n = samples.length;
  if (n === 0 || sampleRate <= 0 || frequency <= 0) return 0;

  const coherentGain = getCoherentGain(windowType, n);

  const window = getWindowCoefficients(n, windowType);

  let real = 0;
  let imag = 0;
  for (let i = 0; i < n; i += 1) {
    const xw = samples[i] * window[i];
    const angle = (-2 * Math.PI * frequency * i) / sampleRate;
    real += xw * Math.cos(angle);
    imag += xw * Math.sin(angle);
  }

  const magnitude = Math.sqrt(real * real + imag * imag);
  const peakAmplitude = (2 * magnitude) / (n * coherentGain);
  return peakAmplitude / Math.sqrt(2);
}

export function getWindowRmsGain(windowType: WindowType, size: number): number {
  if (size <= 0) return 1;
  if (windowType === "rectangular") return 1;

  let sumSquares = 0;
  for (let n = 0; n < size; n += 1) {
    const w = windowSample(n, size, windowType);
    sumSquares += w * w;
  }
  return Math.sqrt(sumSquares / size);
}

export function calculateCompensatedWindowedRms(
  samples: Float32Array,
  windowType: WindowType = "hann",
): number {
  if (samples.length === 0) return 0;
  const size = samples.length;
  const rmsGain = getWindowRmsGain(windowType, size);
  if (rmsGain <= 0) return 0;

  const window = getWindowCoefficients(size, windowType);

  let sumSquares = 0;
  for (let i = 0; i < size; i += 1) {
    const xw = samples[i] * window[i];
    sumSquares += xw * xw;
  }

  const windowedRms = Math.sqrt(sumSquares / size);
  return windowedRms / rmsGain;
}

export function foldFrequencyToNyquist(frequency: number, sampleRate: number): number {
  const nyquist = sampleRate / 2;
  if (frequency <= nyquist) return frequency;

  const wrapped = ((frequency % sampleRate) + sampleRate) % sampleRate;
  return wrapped > nyquist ? sampleRate - wrapped : wrapped;
}

export function getHarmonicBins(params: {
  fundamentalFrequency: number;
  sampleRate: number;
  fftSize: number;
  maxHarmonic?: number;
  aliasMode?: "ignore" | "fold";
}): HarmonicBin[] {
  const {
    fundamentalFrequency,
    sampleRate,
    fftSize,
    maxHarmonic = 9,
    aliasMode = "ignore",
  } = params;

  const binWidth = sampleRate / fftSize;
  const nyquist = sampleRate / 2;
  const bins: HarmonicBin[] = [];

  for (let order = 2; order <= maxHarmonic; order += 1) {
    const harmonicFrequency = fundamentalFrequency * order;
    if (aliasMode === "ignore" && harmonicFrequency > nyquist) {
      continue;
    }

    const aliasedFrequency =
      harmonicFrequency > nyquist
        ? foldFrequencyToNyquist(harmonicFrequency, sampleRate)
        : harmonicFrequency;

    bins.push({
      order,
      frequency: harmonicFrequency,
      aliasedFrequency,
      bin: Math.round(aliasedFrequency / binWidth),
      isAliased: harmonicFrequency > nyquist,
    });
  }

  return bins;
}

export function goertzelPower(samples: Float32Array, targetBin: number): number {
  if (samples.length === 0) return 0;

  const omega = (2 * Math.PI * targetBin) / samples.length;
  const coeff = 2 * Math.cos(omega);

  let s0 = 0;
  let s1 = 0;
  let s2 = 0;

  for (let i = 0; i < samples.length; i += 1) {
    s0 = samples[i] + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }

  return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

export function dftBinPower(samples: Float32Array, targetBin: number): number {
  const n = samples.length;
  if (n === 0) return 0;

  let real = 0;
  let imag = 0;
  for (let i = 0; i < n; i += 1) {
    const angle = (-2 * Math.PI * targetBin * i) / n;
    real += samples[i] * Math.cos(angle);
    imag += samples[i] * Math.sin(angle);
  }

  return real * real + imag * imag;
}
