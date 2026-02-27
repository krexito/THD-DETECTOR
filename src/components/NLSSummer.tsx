"use client";

import { useEffect, useRef } from "react";
import type { ChannelData } from "./ChannelStrip";

interface NLSSummerProps {
  channels: ChannelData[];
  masterThd: number;
  masterThdN: number;
  masterLevel: number;
  masterPeak: number;
  sumMode: "analog" | "digital" | "vintage" | "transformer";
  sumDrive: number;
  sumSaturation: number;
  onSumModeChange: (mode: "analog" | "digital" | "vintage" | "transformer") => void;
  onSumDriveChange: (val: number) => void;
  onSumSaturationChange: (val: number) => void;
}

const SUM_MODES = {
  analog: { label: "ANALOG", color: "#f97316", desc: "Warm analog summing" },
  digital: { label: "DIGITAL", color: "#60a5fa", desc: "Clean digital sum" },
  vintage: { label: "VINTAGE", color: "#a78bfa", desc: "Vintage console" },
  transformer: { label: "XFMR", color: "#34d399", desc: "Transformer coupled" },
};

function SpectrumAnalyzer({
  channels,
}: {
  channels: ChannelData[];
  masterLevel: number;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animRef = useRef<number>(0);
  const timeRef = useRef(0);
  const channelsRef = useRef(channels);

  useEffect(() => {
    channelsRef.current = channels;
  }, [channels]);

  useEffect(() => {
    let running = true;

    function draw() {
      if (!running) return;
      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext("2d");
      if (!ctx) return;
      const channels = channelsRef.current;

    const w = canvas.width;
    const h = canvas.height;
    timeRef.current += 0.05;

    ctx.fillStyle = "#0a0f1a";
    ctx.fillRect(0, 0, w, h);

    // Grid
    ctx.strokeStyle = "#1e293b";
    ctx.lineWidth = 0.5;
    for (let i = 0; i <= 8; i++) {
      const x = (i / 8) * w;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }
    for (let i = 0; i <= 4; i++) {
      const y = (i / 4) * h;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

    // Frequency labels
    const freqs = ["20", "100", "500", "1k", "5k", "10k", "20k"];
    ctx.fillStyle = "#374151";
    ctx.font = "7px monospace";
    ctx.textAlign = "center";
    freqs.forEach((f, i) => {
      ctx.fillText(f, (i / (freqs.length - 1)) * w, h - 2);
    });

    // Draw spectrum for each channel
    const activeChannels = channels.filter((c) => !c.muted);
    activeChannels.forEach((ch) => {
      const bars = 64;
      ctx.beginPath();
      for (let i = 0; i < bars; i++) {
        const x = (i / bars) * w;
        const freq = Math.pow(10, 1.3 + (i / bars) * 3);
        const baseLevel = ch.level / 100;

        // Simulate frequency response based on character
        let freqResponse = 1;
        if (ch.character === "warm") {
          freqResponse = 1 - (i / bars) * 0.4;
        } else if (ch.character === "bright") {
          freqResponse = 0.7 + (i / bars) * 0.5;
        } else if (ch.character === "vintage") {
          freqResponse = Math.sin((i / bars) * Math.PI) * 0.8 + 0.3;
        }

        // Add harmonics
        const harmonicBoost =
          ch.harmonics.reduce((acc, hv, hi) => {
            const harmonicFreq = freq * (hi + 2);
            if (harmonicFreq < 20000) return acc + hv * 0.01;
            return acc;
          }, 0);

        const noise = (Math.random() - 0.5) * 0.05;
        const wave =
          Math.sin(timeRef.current * 0.3 + i * 0.2) * 0.05 * baseLevel;
        const barH =
          (baseLevel * freqResponse + harmonicBoost + noise + wave) * h * 0.8;

        if (i === 0) ctx.moveTo(x, h - Math.max(0, barH));
        else ctx.lineTo(x, h - Math.max(0, barH));
      }
      ctx.strokeStyle = ch.color + "80";
      ctx.lineWidth = 1;
      ctx.stroke();
    });

    // Master sum line
    if (activeChannels.length > 0) {
      const bars = 64;
      ctx.beginPath();
      for (let i = 0; i < bars; i++) {
        const x = (i / bars) * w;
        let sumH = 0;
        activeChannels.forEach((ch) => {
          const baseLevel = ch.level / 100;
          const noise = (Math.random() - 0.5) * 0.02;
          const wave = Math.sin(timeRef.current * 0.3 + i * 0.2) * 0.03;
          sumH += (baseLevel + noise + wave) * 0.5;
        });
        const barH = Math.min(sumH * h * 0.8, h);
        if (i === 0) ctx.moveTo(x, h - barH);
        else ctx.lineTo(x, h - barH);
      }
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 1.5;
      ctx.shadowColor = "#ffffff40";
      ctx.shadowBlur = 3;
      ctx.stroke();
      ctx.shadowBlur = 0;
    }

      animRef.current = requestAnimationFrame(draw);
    }

    animRef.current = requestAnimationFrame(draw);
    return () => {
      running = false;
      cancelAnimationFrame(animRef.current);
    };
  }, []);

  return (
    <canvas
      ref={canvasRef}
      width={400}
      height={100}
      className="w-full rounded border border-neutral-800"
    />
  );
}

function THDSummaryBar({
  channel,
  maxThd,
}: {
  channel: ChannelData;
  maxThd: number;
}) {
  const width = maxThd > 0 ? (channel.thd / maxThd) * 100 : 0;
  const color =
    channel.thd < 0.1
      ? "#22c55e"
      : channel.thd < 0.5
        ? "#84cc16"
        : channel.thd < 1.0
          ? "#eab308"
          : channel.thd < 2.0
            ? "#f97316"
            : "#ef4444";

  return (
    <div className="flex items-center gap-2 text-[9px]">
      <span
        className="w-12 truncate font-bold"
        style={{ color: channel.color }}
      >
        {channel.name}
      </span>
      <div className="flex-1 h-2 bg-neutral-800 rounded-full overflow-hidden">
        <div
          className="h-full rounded-full transition-all duration-300"
          style={{
            width: `${width}%`,
            backgroundColor: color,
            boxShadow: `0 0 4px ${color}60`,
          }}
        />
      </div>
      <span className="w-10 text-right font-mono" style={{ color }}>
        {channel.thd.toFixed(2)}%
      </span>
    </div>
  );
}

function MasterVUMeter({
  levelL,
  levelR,
  peakL,
  peakR,
}: {
  levelL: number;
  levelR: number;
  peakL: number;
  peakR: number;
}) {
  const segments = 24;

  const renderMeter = (level: number, peak: number) => (
    <div className="flex flex-col gap-px h-40 w-4">
      {Array.from({ length: segments }, (_, i) => {
        const segLevel = ((segments - i) / segments) * 100;
        const isActive = level >= segLevel - 4;
        const isPeak = Math.abs(peak - segLevel) < 4;
        let color = "#22c55e";
        if (segLevel > 88) color = "#ef4444";
        else if (segLevel > 75) color = "#f97316";
        else if (segLevel > 60) color = "#eab308";
        return (
          <div
            key={i}
            className="flex-1 rounded-sm transition-all duration-75"
            style={{
              backgroundColor: isActive
                ? color
                : isPeak
                  ? "#ffffff50"
                  : "#1f2937",
              boxShadow: isActive ? `0 0 2px ${color}80` : "none",
            }}
          />
        );
      })}
    </div>
  );

  return (
    <div className="flex gap-1 items-end">
      <div className="flex flex-col items-center gap-0.5">
        <span className="text-[7px] text-neutral-500">L</span>
        {renderMeter(levelL, peakL)}
      </div>
      <div className="flex flex-col items-center gap-0.5">
        <span className="text-[7px] text-neutral-500">R</span>
        {renderMeter(levelR, peakR)}
      </div>
    </div>
  );
}

export default function NLSSummer({
  channels,
  masterThd,
  masterThdN,
  masterLevel,
  masterPeak,
  sumMode,
  sumDrive,
  sumSaturation,
  onSumModeChange,
  onSumDriveChange,
  onSumSaturationChange,
}: NLSSummerProps) {
  const maxThd = Math.max(...channels.map((c) => c.thd), 0.01);
  const activeChannels = channels.filter((c) => !c.muted);
  const avgThd =
    activeChannels.length > 0
      ? activeChannels.reduce((a, c) => a + c.thd, 0) / activeChannels.length
      : 0;

  const thdColor =
    masterThd < 0.1
      ? "#22c55e"
      : masterThd < 0.5
        ? "#84cc16"
        : masterThd < 1.0
          ? "#eab308"
          : masterThd < 2.0
            ? "#f97316"
            : "#ef4444";

  return (
    <div
      className="rounded-xl border border-neutral-700 overflow-hidden"
      style={{
        backgroundColor: "#0d1117",
        boxShadow: "0 0 40px #00000080, inset 0 1px 0 #ffffff10",
        minWidth: "440px",
      }}
    >
      {/* Header */}
      <div
        className="px-4 py-2 flex items-center justify-between"
        style={{
          background: "linear-gradient(180deg, #1a1f2e 0%, #0d1117 100%)",
          borderBottom: "1px solid #1e293b",
        }}
      >
        <div className="flex items-center gap-3">
          <div
            className="w-2 h-2 rounded-full animate-pulse"
            style={{ backgroundColor: SUM_MODES[sumMode].color }}
          />
          <span
            className="text-sm font-bold tracking-widest"
            style={{ color: SUM_MODES[sumMode].color }}
          >
            NLS SUMMER
          </span>
          <span className="text-[9px] text-neutral-500 tracking-wider">
            MASTER BUS
          </span>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-[9px] text-neutral-500">
            {activeChannels.length}/{channels.length} CH
          </span>
          <div
            className="text-[9px] font-mono font-bold px-2 py-0.5 rounded"
            style={{
              backgroundColor: thdColor + "20",
              color: thdColor,
              border: `1px solid ${thdColor}40`,
            }}
          >
            MASTER THD: {masterThd.toFixed(3)}%
          </div>
        </div>
      </div>

      <div className="p-4 flex gap-4">
        {/* Left: Spectrum + THD bars */}
        <div className="flex-1 flex flex-col gap-3">
          {/* Spectrum Analyzer */}
          <div>
            <div className="text-[8px] text-neutral-500 mb-1 tracking-widest">
              SPECTRUM ANALYZER
            </div>
            <SpectrumAnalyzer channels={channels} masterLevel={masterLevel} />
          </div>

          {/* THD per channel */}
          <div>
            <div className="text-[8px] text-neutral-500 mb-1.5 tracking-widest">
              THD PER CHANNEL
            </div>
            <div className="flex flex-col gap-1">
              {channels.map((ch) => (
                <THDSummaryBar key={ch.id} channel={ch} maxThd={maxThd} />
              ))}
            </div>
          </div>

          {/* Sum mode selector */}
          <div>
            <div className="text-[8px] text-neutral-500 mb-1.5 tracking-widest">
              SUM CHARACTER
            </div>
            <div className="grid grid-cols-4 gap-1">
              {(
                Object.entries(SUM_MODES) as [
                  keyof typeof SUM_MODES,
                  (typeof SUM_MODES)[keyof typeof SUM_MODES],
                ][]
              ).map(([key, val]) => (
                <button
                  key={key}
                  onClick={() => onSumModeChange(key)}
                  className="flex flex-col items-center py-1.5 px-1 rounded text-[8px] font-bold transition-all"
                  style={{
                    backgroundColor:
                      sumMode === key ? val.color + "25" : "#1a1f2e",
                    color: sumMode === key ? val.color : "#4b5563",
                    border: `1px solid ${sumMode === key ? val.color + "60" : "#1e293b"}`,
                    boxShadow:
                      sumMode === key ? `0 0 8px ${val.color}30` : "none",
                  }}
                >
                  {val.label}
                </button>
              ))}
            </div>
          </div>

          {/* Drive & Saturation */}
          <div className="grid grid-cols-2 gap-3">
            <div>
              <div className="flex justify-between text-[8px] mb-1">
                <span className="text-neutral-500 tracking-widest">DRIVE</span>
                <span className="font-mono text-orange-400">{sumDrive}</span>
              </div>
              <input
                type="range"
                min={0}
                max={100}
                value={sumDrive}
                onChange={(e) => onSumDriveChange(Number(e.target.value))}
                className="w-full h-1 cursor-pointer accent-orange-500"
              />
            </div>
            <div>
              <div className="flex justify-between text-[8px] mb-1">
                <span className="text-neutral-500 tracking-widest">SAT</span>
                <span className="font-mono text-purple-400">{sumSaturation}</span>
              </div>
              <input
                type="range"
                min={0}
                max={100}
                value={sumSaturation}
                onChange={(e) => onSumSaturationChange(Number(e.target.value))}
                className="w-full h-1 cursor-pointer accent-purple-500"
              />
            </div>
          </div>
        </div>

        {/* Right: Master meters + stats */}
        <div className="flex flex-col gap-3 items-center">
          <div className="text-[8px] text-neutral-500 tracking-widest">
            OUTPUT
          </div>
          <MasterVUMeter
            levelL={masterLevel}
            levelR={masterLevel * 0.97}
            peakL={masterPeak}
            peakR={masterPeak * 0.97}
          />

          {/* Stats */}
          <div
            className="w-full rounded-lg p-2 flex flex-col gap-1.5"
            style={{ backgroundColor: "#0a0f1a", border: "1px solid #1e293b" }}
          >
            <div className="flex justify-between text-[8px]">
              <span className="text-neutral-500">THD</span>
              <span className="font-mono" style={{ color: thdColor }}>
                {masterThd.toFixed(3)}%
              </span>
            </div>
            <div className="flex justify-between text-[8px]">
              <span className="text-neutral-500">THD+N</span>
              <span className="font-mono text-blue-400">
                {masterThdN.toFixed(3)}%
              </span>
            </div>
            <div className="flex justify-between text-[8px]">
              <span className="text-neutral-500">AVG</span>
              <span className="font-mono text-neutral-300">
                {avgThd.toFixed(3)}%
              </span>
            </div>
            <div className="flex justify-between text-[8px]">
              <span className="text-neutral-500">PEAK</span>
              <span className="font-mono text-neutral-300">
                {masterPeak.toFixed(1)} dB
              </span>
            </div>
            <div className="flex justify-between text-[8px]">
              <span className="text-neutral-500">MODE</span>
              <span
                className="font-mono font-bold"
                style={{ color: SUM_MODES[sumMode].color }}
              >
                {SUM_MODES[sumMode].label}
              </span>
            </div>
          </div>

          {/* Clip indicator */}
          <div
            className="w-full text-center text-[8px] font-bold rounded py-1 transition-all"
            style={{
              backgroundColor:
                masterPeak > 95 ? "#ef444430" : "#1a1f2e",
              color: masterPeak > 95 ? "#ef4444" : "#374151",
              border: `1px solid ${masterPeak > 95 ? "#ef444460" : "#1e293b"}`,
            }}
          >
            {masterPeak > 95 ? "⚠ CLIP" : "● SAFE"}
          </div>
        </div>
      </div>
    </div>
  );
}
