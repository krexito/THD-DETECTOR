"use client";

import { useEffect, useRef } from "react";

export interface ChannelData {
  id: string;
  name: string;
  thd: number;
  thdN: number;
  level: number;
  peakLevel: number;
  harmonics: number[];
  muted: boolean;
  soloed: boolean;
  color: string;
}

interface ChannelStripProps {
  channel: ChannelData;
  onChange: (id: string, updates: Partial<ChannelData>) => void;
  isSoloing: boolean;
}

export function getTHDColor(thd: number): string {
  if (thd < 0.05) return "#22c55e";
  if (thd < 0.1) return "#84cc16";
  if (thd < 0.5) return "#eab308";
  if (thd < 1.0) return "#f97316";
  if (thd < 2.0) return "#ef4444";
  return "#dc2626";
}

export function getTHDLabel(thd: number): string {
  if (thd < 0.01) return "Ultra Clean";
  if (thd < 0.05) return "Clean";
  if (thd < 0.1) return "Very Low";
  if (thd < 0.5) return "Low";
  if (thd < 1.0) return "Moderate";
  if (thd < 2.0) return "High";
  return "Very High";
}

function VUMeter({ level, peak }: { level: number; peak: number }) {
  const segments = 20;
  return (
    <div className="flex flex-col gap-px h-28 w-3">
      {Array.from({ length: segments }, (_, i) => {
        const segLevel = ((segments - i) / segments) * 100;
        const isActive = level >= segLevel - 5;
        const isPeak = Math.abs(peak - segLevel) < 5;
        let color = "#22c55e";
        if (segLevel > 85) color = "#ef4444";
        else if (segLevel > 70) color = "#f97316";
        else if (segLevel > 55) color = "#eab308";
        return (
          <div
            key={i}
            className="flex-1 rounded-sm transition-all duration-75"
            style={{
              backgroundColor: isActive
                ? color
                : isPeak
                  ? "#ffffff40"
                  : "#1f2937",
              boxShadow: isActive ? `0 0 3px ${color}60` : "none",
            }}
          />
        );
      })}
    </div>
  );
}

function THDGauge({ thd }: { thd: number }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animFrameRef = useRef<number>(0);
  const thdRef = useRef(thd);

  useEffect(() => {
    thdRef.current = thd;
  }, [thd]);

  useEffect(() => {
    let running = true;

    function draw() {
      if (!running) return;
      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext("2d");
      if (!ctx) return;

      const thd = thdRef.current;
      const w = canvas.width;
      const h = canvas.height;
      ctx.clearRect(0, 0, w, h);

      ctx.fillStyle = "#0a0f1a";
      ctx.fillRect(0, 0, w, h);

      const cx = w / 2;
      const cy = h * 0.88;
      const radius = Math.min(w, h) * 0.72;
      const startAngle = Math.PI * 0.85;
      const sweep = Math.PI * 1.3;

      // Background arc
      ctx.beginPath();
      ctx.arc(cx, cy, radius, startAngle, startAngle + sweep, false);
      ctx.strokeStyle = "#1e293b";
      ctx.lineWidth = 5;
      ctx.lineCap = "round";
      ctx.stroke();

      // Colored zones on arc
      const zones = [
        { from: 0, to: 0.3, color: "#22c55e" },
        { from: 0.3, to: 0.5, color: "#84cc16" },
        { from: 0.5, to: 0.7, color: "#eab308" },
        { from: 0.7, to: 0.85, color: "#f97316" },
        { from: 0.85, to: 1.0, color: "#ef4444" },
      ];
      zones.forEach(({ from, to, color }) => {
        ctx.beginPath();
        ctx.arc(
          cx, cy, radius,
          startAngle + from * sweep,
          startAngle + to * sweep,
          false
        );
        ctx.strokeStyle = color + "50";
        ctx.lineWidth = 5;
        ctx.lineCap = "butt";
        ctx.stroke();
      });

      // Active arc (0–5% range)
      const thdNorm = Math.min(thd / 5, 1);
      const thdAngle = startAngle + thdNorm * sweep;
      const thdColor = getTHDColor(thd);

      if (thdNorm > 0) {
        const grad = ctx.createLinearGradient(0, cy, w, cy);
        grad.addColorStop(0, "#22c55e");
        grad.addColorStop(0.5, "#eab308");
        grad.addColorStop(1, "#ef4444");

        ctx.beginPath();
        ctx.arc(cx, cy, radius, startAngle, thdAngle, false);
        ctx.strokeStyle = grad;
        ctx.lineWidth = 5;
        ctx.lineCap = "round";
        ctx.stroke();
      }

      // Needle
      const nx = cx + Math.cos(thdAngle) * (radius - 1);
      const ny = cy + Math.sin(thdAngle) * (radius - 1);
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(nx, ny);
      ctx.strokeStyle = thdColor;
      ctx.lineWidth = 1.5;
      ctx.shadowColor = thdColor;
      ctx.shadowBlur = 5;
      ctx.stroke();
      ctx.shadowBlur = 0;

      // Center dot
      ctx.beginPath();
      ctx.arc(cx, cy, 2.5, 0, Math.PI * 2);
      ctx.fillStyle = "#94a3b8";
      ctx.fill();

      // THD value
      ctx.fillStyle = thdColor;
      ctx.font = "bold 10px monospace";
      ctx.textAlign = "center";
      ctx.fillText(`${thd.toFixed(2)}%`, cx, cy - radius * 0.35);

      ctx.fillStyle = "#475569";
      ctx.font = "7px monospace";
      ctx.fillText("THD", cx, cy - radius * 0.18);

      animFrameRef.current = requestAnimationFrame(draw);
    }

    animFrameRef.current = requestAnimationFrame(draw);
    return () => {
      running = false;
      cancelAnimationFrame(animFrameRef.current);
    };
  }, []);

  return (
    <canvas
      ref={canvasRef}
      width={96}
      height={56}
      className="w-full rounded"
    />
  );
}

export default function ChannelStrip({
  channel,
  onChange,
  isSoloing,
}: ChannelStripProps) {
  const isEffectivelyMuted = channel.muted || (isSoloing && !channel.soloed);

  return (
    <div
      className="flex flex-col rounded-lg overflow-hidden border transition-all duration-200"
      style={{
        borderColor: channel.soloed
          ? channel.color
          : isEffectivelyMuted
            ? "#374151"
            : "#1f2937",
        backgroundColor: "#0d1117",
        width: "110px",
        boxShadow: channel.soloed
          ? `0 0 12px ${channel.color}40`
          : "none",
        opacity: isEffectivelyMuted ? 0.45 : 1,
      }}
    >
      {/* Header */}
      <div
        className="px-2 py-1 flex items-center justify-between"
        style={{
          backgroundColor: channel.color + "18",
          borderBottom: `1px solid ${channel.color}35`,
        }}
      >
        <span
          className="text-[10px] font-bold truncate tracking-wider"
          style={{ color: channel.color }}
        >
          {channel.name}
        </span>
        {/* Measurement indicator */}
        <div
          className="w-1.5 h-1.5 rounded-full animate-pulse"
          style={{ backgroundColor: getTHDColor(channel.thd) }}
        />
      </div>

      {/* THD Gauge */}
      <div className="px-1.5 pt-1.5">
        <THDGauge thd={channel.thd} />
      </div>

      {/* THD+N readout */}
      <div className="px-2 py-0.5 flex justify-between items-center">
        <span className="text-[8px] text-neutral-600">THD+N</span>
        <span
          className="text-[9px] font-mono font-bold"
          style={{ color: getTHDColor(channel.thdN) }}
        >
          {channel.thdN.toFixed(3)}%
        </span>
      </div>

      {/* Status badge */}
      <div className="px-2 pb-1.5">
        <div
          className="text-center text-[7px] font-bold rounded px-1 py-0.5 tracking-wider"
          style={{
            backgroundColor: getTHDColor(channel.thd) + "18",
            color: getTHDColor(channel.thd),
            border: `1px solid ${getTHDColor(channel.thd)}35`,
          }}
        >
          {getTHDLabel(channel.thd)}
        </div>
      </div>

      {/* VU Meter */}
      <div className="px-2 pb-1.5 flex justify-center gap-2 items-end">
        <VUMeter level={channel.level} peak={channel.peakLevel} />
        <div className="flex flex-col gap-0.5 text-[7px] text-neutral-600 justify-end pb-0.5">
          <span>{channel.level.toFixed(0)}</span>
          <span className="text-neutral-700">dBFS</span>
        </div>
      </div>

      {/* Mute / Solo */}
      <div className="px-2 pb-2 flex gap-1">
        <button
          onClick={() => onChange(channel.id, { muted: !channel.muted })}
          className="flex-1 text-[8px] font-bold rounded py-0.5 transition-all"
          style={{
            backgroundColor: channel.muted ? "#ef444430" : "#1a1f2e",
            color: channel.muted ? "#ef4444" : "#4b5563",
            border: `1px solid ${channel.muted ? "#ef444450" : "#1e293b"}`,
          }}
        >
          M
        </button>
        <button
          onClick={() => onChange(channel.id, { soloed: !channel.soloed })}
          className="flex-1 text-[8px] font-bold rounded py-0.5 transition-all"
          style={{
            backgroundColor: channel.soloed ? "#eab30830" : "#1a1f2e",
            color: channel.soloed ? "#eab308" : "#4b5563",
            border: `1px solid ${channel.soloed ? "#eab30850" : "#1e293b"}`,
          }}
        >
          S
        </button>
      </div>
    </div>
  );
}
