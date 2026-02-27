"use client";

import { useEffect, useRef, useState } from "react";

export interface ChannelData {
  id: string;
  name: string;
  thd: number;
  thdN: number;
  level: number;
  peakLevel: number;
  harmonics: number[];
  saturation: number;
  drive: number;
  character: "clean" | "warm" | "bright" | "vintage";
  muted: boolean;
  soloed: boolean;
  color: string;
}

interface ChannelStripProps {
  channel: ChannelData;
  onChange: (id: string, updates: Partial<ChannelData>) => void;
  isSoloing: boolean;
}

function getTHDColor(thd: number): string {
  if (thd < 0.1) return "#22c55e";
  if (thd < 0.5) return "#84cc16";
  if (thd < 1.0) return "#eab308";
  if (thd < 2.0) return "#f97316";
  return "#ef4444";
}

function getTHDLabel(thd: number): string {
  if (thd < 0.01) return "Ultra Clean";
  if (thd < 0.1) return "Clean";
  if (thd < 0.5) return "Warm";
  if (thd < 1.0) return "Saturated";
  if (thd < 2.0) return "Driven";
  return "Clipping";
}

function HarmonicBar({
  value,
  index,
  maxVal,
}: {
  value: number;
  index: number;
  maxVal: number;
}) {
  const colors = [
    "#60a5fa",
    "#34d399",
    "#fbbf24",
    "#f87171",
    "#a78bfa",
    "#fb923c",
    "#38bdf8",
  ];
  const height = maxVal > 0 ? (value / maxVal) * 100 : 0;
  return (
    <div className="flex flex-col items-center gap-0.5">
      <div className="w-3 h-16 bg-neutral-800 rounded-sm relative overflow-hidden flex items-end">
        <div
          className="w-full rounded-sm transition-all duration-150"
          style={{
            height: `${height}%`,
            backgroundColor: colors[index % colors.length],
            boxShadow: `0 0 4px ${colors[index % colors.length]}80`,
          }}
        />
      </div>
      <span className="text-[8px] text-neutral-500">H{index + 2}</span>
    </div>
  );
}

function VUMeter({ level, peak }: { level: number; peak: number }) {
  const segments = 20;
  return (
    <div className="flex flex-col gap-px h-32">
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

export default function ChannelStrip({
  channel,
  onChange,
  isSoloing,
}: ChannelStripProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animFrameRef = useRef<number>(0);
  const thdRef = useRef(channel.thd);
  const [isExpanded, setIsExpanded] = useState(false);

  useEffect(() => {
    thdRef.current = channel.thd;
  }, [channel.thd]);

  useEffect(() => {
    let running = true;

    function drawTHDMeter() {
      if (!running) return;
      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext("2d");
      if (!ctx) return;

      const thd = thdRef.current;
      const w = canvas.width;
      const h = canvas.height;
      ctx.clearRect(0, 0, w, h);

      // Background
      ctx.fillStyle = "#0f172a";
      ctx.fillRect(0, 0, w, h);

      // Grid lines
      ctx.strokeStyle = "#1e293b";
      ctx.lineWidth = 1;
      for (let i = 0; i <= 4; i++) {
        const y = (i / 4) * h;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      // THD arc meter
      const cx = w / 2;
      const cy = h * 0.85;
      const radius = Math.min(w, h) * 0.7;
      const startAngle = Math.PI * 0.85;
      const endAngle = Math.PI * 0.15;

      // Background arc
      ctx.beginPath();
      ctx.arc(cx, cy, radius, startAngle, endAngle + Math.PI * 2, false);
      ctx.strokeStyle = "#1e293b";
      ctx.lineWidth = 6;
      ctx.stroke();

      // THD value arc (0-5% range)
      const thdNorm = Math.min(thd / 5, 1);
      const thdAngle =
        startAngle + thdNorm * (Math.PI * 2 - (startAngle - endAngle - Math.PI * 2));
      const thdColor = getTHDColor(thd);

      const grad = ctx.createLinearGradient(0, 0, w, 0);
      grad.addColorStop(0, "#22c55e");
      grad.addColorStop(0.5, "#eab308");
      grad.addColorStop(1, "#ef4444");

      ctx.beginPath();
      ctx.arc(cx, cy, radius, startAngle, thdAngle, false);
      ctx.strokeStyle = grad;
      ctx.lineWidth = 6;
      ctx.lineCap = "round";
      ctx.stroke();

      // Needle
      const needleAngle = startAngle + thdNorm * (Math.PI * 2 - (startAngle - endAngle - Math.PI * 2));
      const nx = cx + Math.cos(needleAngle) * (radius - 2);
      const ny = cy + Math.sin(needleAngle) * (radius - 2);
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(nx, ny);
      ctx.strokeStyle = thdColor;
      ctx.lineWidth = 2;
      ctx.shadowColor = thdColor;
      ctx.shadowBlur = 6;
      ctx.stroke();
      ctx.shadowBlur = 0;

      // Center dot
      ctx.beginPath();
      ctx.arc(cx, cy, 3, 0, Math.PI * 2);
      ctx.fillStyle = "#94a3b8";
      ctx.fill();

      // THD text
      ctx.fillStyle = thdColor;
      ctx.font = "bold 11px monospace";
      ctx.textAlign = "center";
      ctx.fillText(`${thd.toFixed(2)}%`, cx, cy - radius * 0.3);

      ctx.fillStyle = "#64748b";
      ctx.font = "8px monospace";
      ctx.fillText("THD", cx, cy - radius * 0.15);

      animFrameRef.current = requestAnimationFrame(drawTHDMeter);
    }

    animFrameRef.current = requestAnimationFrame(drawTHDMeter);
    return () => {
      running = false;
      cancelAnimationFrame(animFrameRef.current);
    };
  }, []);

  const isEffectivelyMuted =
    channel.muted || (isSoloing && !channel.soloed);

  return (
    <div
      className="flex flex-col rounded-lg overflow-hidden border transition-all duration-200"
      style={{
        borderColor: channel.soloed
          ? channel.color
          : isEffectivelyMuted
            ? "#374151"
            : "#1f2937",
        backgroundColor: "#111827",
        width: isExpanded ? "160px" : "120px",
        boxShadow: channel.soloed
          ? `0 0 12px ${channel.color}40`
          : "none",
        opacity: isEffectivelyMuted ? 0.5 : 1,
      }}
    >
      {/* Header */}
      <div
        className="px-2 py-1.5 flex items-center justify-between cursor-pointer"
        style={{ backgroundColor: channel.color + "20", borderBottom: `1px solid ${channel.color}40` }}
        onClick={() => setIsExpanded(!isExpanded)}
      >
        <span
          className="text-xs font-bold truncate"
          style={{ color: channel.color }}
        >
          {channel.name}
        </span>
        <span className="text-[9px] text-neutral-500">{isExpanded ? "▲" : "▼"}</span>
      </div>

      {/* THD Canvas Meter */}
      <div className="px-2 pt-2">
        <canvas
          ref={canvasRef}
          width={isExpanded ? 136 : 96}
          height={60}
          className="w-full rounded"
        />
      </div>

      {/* THD+N */}
      <div className="px-2 py-1 flex justify-between items-center">
        <span className="text-[9px] text-neutral-500">THD+N</span>
        <span
          className="text-[10px] font-mono font-bold"
          style={{ color: getTHDColor(channel.thdN) }}
        >
          {channel.thdN.toFixed(3)}%
        </span>
      </div>

      {/* Status label */}
      <div className="px-2 pb-1">
        <div
          className="text-center text-[8px] font-bold rounded px-1 py-0.5"
          style={{
            backgroundColor: getTHDColor(channel.thd) + "20",
            color: getTHDColor(channel.thd),
            border: `1px solid ${getTHDColor(channel.thd)}40`,
          }}
        >
          {getTHDLabel(channel.thd)}
        </div>
      </div>

      {/* Harmonics display */}
      {isExpanded && (
        <div className="px-2 pb-2">
          <div className="text-[8px] text-neutral-500 mb-1">HARMONICS</div>
          <div className="flex gap-1 justify-center">
            {channel.harmonics.slice(0, 7).map((h, i) => (
              <HarmonicBar
                key={i}
                value={h}
                index={i}
                maxVal={Math.max(...channel.harmonics)}
              />
            ))}
          </div>
        </div>
      )}

      {/* VU Meter + Controls */}
      <div className="px-2 pb-2 flex gap-2">
        <VUMeter level={channel.level} peak={channel.peakLevel} />

        <div className="flex flex-col gap-2 flex-1">
          {/* Drive knob */}
          <div className="flex flex-col items-center">
            <span className="text-[8px] text-neutral-500 mb-0.5">DRIVE</span>
            <input
              type="range"
              min={0}
              max={100}
              value={channel.drive}
              onChange={(e) =>
                onChange(channel.id, { drive: Number(e.target.value) })
              }
              className="w-full h-1 accent-orange-500 cursor-pointer"
              style={{ writingMode: "vertical-lr", direction: "rtl", height: "48px", width: "12px" }}
            />
            <span className="text-[8px] font-mono text-orange-400">
              {channel.drive}
            </span>
          </div>

          {/* Character selector */}
          <div className="flex flex-col gap-0.5">
            {(["clean", "warm", "bright", "vintage"] as const).map((c) => (
              <button
                key={c}
                onClick={() => onChange(channel.id, { character: c })}
                className="text-[7px] rounded px-1 py-0.5 transition-all"
                style={{
                  backgroundColor:
                    channel.character === c ? channel.color + "40" : "#1f2937",
                  color:
                    channel.character === c ? channel.color : "#6b7280",
                  border: `1px solid ${channel.character === c ? channel.color + "60" : "#374151"}`,
                }}
              >
                {c.toUpperCase()}
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Mute / Solo */}
      <div className="px-2 pb-2 flex gap-1">
        <button
          onClick={() => onChange(channel.id, { muted: !channel.muted })}
          className="flex-1 text-[9px] font-bold rounded py-0.5 transition-all"
          style={{
            backgroundColor: channel.muted ? "#ef444440" : "#1f2937",
            color: channel.muted ? "#ef4444" : "#6b7280",
            border: `1px solid ${channel.muted ? "#ef444460" : "#374151"}`,
          }}
        >
          M
        </button>
        <button
          onClick={() => onChange(channel.id, { soloed: !channel.soloed })}
          className="flex-1 text-[9px] font-bold rounded py-0.5 transition-all"
          style={{
            backgroundColor: channel.soloed ? "#eab30840" : "#1f2937",
            color: channel.soloed ? "#eab308" : "#6b7280",
            border: `1px solid ${channel.soloed ? "#eab30860" : "#374151"}`,
          }}
        >
          S
        </button>
      </div>
    </div>
  );
}
