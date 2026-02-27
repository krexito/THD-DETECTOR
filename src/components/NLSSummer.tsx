"use client";

import { useEffect, useRef } from "react";
import type { ChannelData } from "./ChannelStrip";
import { getTHDColor } from "./ChannelStrip";

interface MasterBrainProps {
  channels: ChannelData[];
  masterThd: number;
  masterThdN: number;
  worstChannel: string | null;
}

// ─── Harmonic Spectrum Display ───────────────────────────────────────────────
function HarmonicSpectrum({ channels }: { channels: ChannelData[] }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animRef = useRef<number>(0);
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

      const chs = channelsRef.current.filter((c) => !c.muted);
      const w = canvas.width;
      const h = canvas.height;

      ctx.fillStyle = "#060b14";
      ctx.fillRect(0, 0, w, h);

      // Grid
      ctx.strokeStyle = "#0f1929";
      ctx.lineWidth = 0.5;
      for (let i = 0; i <= 8; i++) {
        const x = (i / 8) * w;
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, h - 14);
        ctx.stroke();
      }
      for (let i = 0; i <= 4; i++) {
        const y = (i / 4) * (h - 14);
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      // dB labels on left
      const dbLabels = ["0", "-20", "-40", "-60", "-80"];
      ctx.fillStyle = "#1e3a5f";
      ctx.font = "6px monospace";
      ctx.textAlign = "left";
      dbLabels.forEach((label, i) => {
        const y = (i / 4) * (h - 14) + 6;
        ctx.fillText(label, 2, y);
      });

      // Harmonic labels (H2–H8)
      const harmonicLabels = ["H2", "H3", "H4", "H5", "H6", "H7", "H8"];
      const barGroupW = w / 7;

      harmonicLabels.forEach((label, hi) => {
        const groupX = hi * barGroupW;

        // Draw each channel's bar for this harmonic
        const barW = Math.max(2, (barGroupW - 4) / Math.max(chs.length, 1));

        chs.forEach((ch, ci) => {
          const val = ch.harmonics[hi] ?? 0;
          // Convert to dB-like scale for display
          const norm = val > 0 ? Math.min(1, val / 0.5) : 0;
          const barH = norm * (h - 14);
          const x = groupX + 2 + ci * barW;

          // Bar fill
          ctx.fillStyle = ch.color + "90";
          ctx.fillRect(x, h - 14 - barH, barW - 1, barH);

          // Bar top glow
          if (barH > 2) {
            ctx.fillStyle = ch.color;
            ctx.fillRect(x, h - 14 - barH, barW - 1, 2);
          }
        });

        // Harmonic label
        ctx.fillStyle = "#374151";
        ctx.font = "7px monospace";
        ctx.textAlign = "center";
        ctx.fillText(label, groupX + barGroupW / 2, h - 2);
      });

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
      width={420}
      height={110}
      className="w-full rounded border border-neutral-800/50"
    />
  );
}

// ─── THD Timeline (rolling history) ─────────────────────────────────────────
function THDTimeline({
  channels,
  masterThd,
}: {
  channels: ChannelData[];
  masterThd: number;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animRef = useRef<number>(0);
  const historyRef = useRef<{ master: number; channels: Map<string, number> }[]>([]);
  const channelsRef = useRef(channels);
  const masterRef = useRef(masterThd);

  useEffect(() => {
    channelsRef.current = channels;
  }, [channels]);

  useEffect(() => {
    masterRef.current = masterThd;
  }, [masterThd]);

  useEffect(() => {
    let running = true;
    let frameCount = 0;

    function draw() {
      if (!running) return;
      frameCount++;

      // Sample every 6 frames (~10 samples/sec at 60fps)
      if (frameCount % 6 === 0) {
        const chs = channelsRef.current;
        const chMap = new Map<string, number>();
        chs.forEach((ch) => chMap.set(ch.id, ch.thd));
        historyRef.current.push({ master: masterRef.current, channels: chMap });
        if (historyRef.current.length > 120) historyRef.current.shift();
      }

      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext("2d");
      if (!ctx) return;

      const w = canvas.width;
      const h = canvas.height;
      const history = historyRef.current;

      ctx.fillStyle = "#060b14";
      ctx.fillRect(0, 0, w, h);

      // Grid
      ctx.strokeStyle = "#0f1929";
      ctx.lineWidth = 0.5;
      for (let i = 0; i <= 4; i++) {
        const y = (i / 4) * h;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      if (history.length < 2) {
        animRef.current = requestAnimationFrame(draw);
        return;
      }

      const maxTHD = 2; // 2% max display range

      // Draw per-channel lines
      channelsRef.current.forEach((ch) => {
        ctx.beginPath();
        let started = false;
        history.forEach((snap, i) => {
          const val = snap.channels.get(ch.id) ?? 0;
          const x = (i / (history.length - 1)) * w;
          const y = h - (val / maxTHD) * h;
          if (!started) {
            ctx.moveTo(x, y);
            started = true;
          } else {
            ctx.lineTo(x, y);
          }
        });
        ctx.strokeStyle = ch.color + "60";
        ctx.lineWidth = 1;
        ctx.stroke();
      });

      // Draw master THD line (white, prominent)
      ctx.beginPath();
      history.forEach((snap, i) => {
        const x = (i / (history.length - 1)) * w;
        const y = h - (snap.master / maxTHD) * h;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.strokeStyle = "#ffffff90";
      ctx.lineWidth = 1.5;
      ctx.shadowColor = "#ffffff40";
      ctx.shadowBlur = 3;
      ctx.stroke();
      ctx.shadowBlur = 0;

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
      width={420}
      height={70}
      className="w-full rounded border border-neutral-800/50"
    />
  );
}

// ─── Master THD Gauge ────────────────────────────────────────────────────────
function MasterGauge({ thd, thdN }: { thd: number; thdN: number }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animRef = useRef<number>(0);
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

      ctx.fillStyle = "#060b14";
      ctx.fillRect(0, 0, w, h);

      const cx = w / 2;
      const cy = h * 0.82;
      const radius = Math.min(w, h) * 0.68;
      const startAngle = Math.PI * 0.8;
      const sweep = Math.PI * 1.4;

      // Outer ring
      ctx.beginPath();
      ctx.arc(cx, cy, radius + 4, startAngle, startAngle + sweep, false);
      ctx.strokeStyle = "#0f1929";
      ctx.lineWidth = 2;
      ctx.stroke();

      // Background arc
      ctx.beginPath();
      ctx.arc(cx, cy, radius, startAngle, startAngle + sweep, false);
      ctx.strokeStyle = "#1e293b";
      ctx.lineWidth = 10;
      ctx.lineCap = "round";
      ctx.stroke();

      // Zone arcs
      const zones = [
        { from: 0, to: 0.25, color: "#22c55e" },
        { from: 0.25, to: 0.45, color: "#84cc16" },
        { from: 0.45, to: 0.65, color: "#eab308" },
        { from: 0.65, to: 0.82, color: "#f97316" },
        { from: 0.82, to: 1.0, color: "#ef4444" },
      ];
      zones.forEach(({ from, to, color }) => {
        ctx.beginPath();
        ctx.arc(
          cx, cy, radius,
          startAngle + from * sweep,
          startAngle + to * sweep,
          false
        );
        ctx.strokeStyle = color + "40";
        ctx.lineWidth = 10;
        ctx.lineCap = "butt";
        ctx.stroke();
      });

      // Active arc
      const thdNorm = Math.min(thd / 5, 1);
      const thdAngle = startAngle + thdNorm * sweep;
      const thdColor = getTHDColor(thd);

      if (thdNorm > 0) {
        const grad = ctx.createLinearGradient(0, cy, w, cy);
        grad.addColorStop(0, "#22c55e");
        grad.addColorStop(0.4, "#eab308");
        grad.addColorStop(1, "#ef4444");

        ctx.beginPath();
        ctx.arc(cx, cy, radius, startAngle, thdAngle, false);
        ctx.strokeStyle = grad;
        ctx.lineWidth = 10;
        ctx.lineCap = "round";
        ctx.shadowColor = thdColor;
        ctx.shadowBlur = 8;
        ctx.stroke();
        ctx.shadowBlur = 0;
      }

      // Tick marks
      for (let i = 0; i <= 10; i++) {
        const angle = startAngle + (i / 10) * sweep;
        const inner = radius - 14;
        const outer = radius - 8;
        ctx.beginPath();
        ctx.moveTo(cx + Math.cos(angle) * inner, cy + Math.sin(angle) * inner);
        ctx.lineTo(cx + Math.cos(angle) * outer, cy + Math.sin(angle) * outer);
        ctx.strokeStyle = "#1e3a5f";
        ctx.lineWidth = 1;
        ctx.stroke();
      }

      // Needle
      const nx = cx + Math.cos(thdAngle) * (radius - 4);
      const ny = cy + Math.sin(thdAngle) * (radius - 4);
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(nx, ny);
      ctx.strokeStyle = thdColor;
      ctx.lineWidth = 2.5;
      ctx.shadowColor = thdColor;
      ctx.shadowBlur = 8;
      ctx.stroke();
      ctx.shadowBlur = 0;

      // Center hub
      ctx.beginPath();
      ctx.arc(cx, cy, 5, 0, Math.PI * 2);
      ctx.fillStyle = "#1e293b";
      ctx.fill();
      ctx.beginPath();
      ctx.arc(cx, cy, 3, 0, Math.PI * 2);
      ctx.fillStyle = "#94a3b8";
      ctx.fill();

      // Scale labels
      const scaleLabels = ["0", "1", "2", "3", "4", "5%"];
      ctx.fillStyle = "#374151";
      ctx.font = "7px monospace";
      ctx.textAlign = "center";
      scaleLabels.forEach((label, i) => {
        const angle = startAngle + (i / (scaleLabels.length - 1)) * sweep;
        const lx = cx + Math.cos(angle) * (radius - 22);
        const ly = cy + Math.sin(angle) * (radius - 22);
        ctx.fillText(label, lx, ly);
      });

      // Center readout
      ctx.fillStyle = thdColor;
      ctx.font = "bold 16px monospace";
      ctx.textAlign = "center";
      ctx.fillText(`${thd.toFixed(3)}%`, cx, cy - radius * 0.28);

      ctx.fillStyle = "#475569";
      ctx.font = "8px monospace";
      ctx.fillText("MASTER THD", cx, cy - radius * 0.1);

      animRef.current = requestAnimationFrame(draw);
    }

    animRef.current = requestAnimationFrame(draw);
    return () => {
      running = false;
      cancelAnimationFrame(animRef.current);
    };
  }, []);

  return (
    <div className="flex flex-col items-center gap-1">
      <canvas
        ref={canvasRef}
        width={160}
        height={110}
        className="rounded"
      />
      <div className="text-[8px] text-neutral-500 font-mono">
        THD+N:{" "}
        <span style={{ color: getTHDColor(thdN) }}>{thdN.toFixed(3)}%</span>
      </div>
    </div>
  );
}

// ─── Channel THD Table ───────────────────────────────────────────────────────
function ChannelTable({ channels }: { channels: ChannelData[] }) {
  const maxThd = Math.max(...channels.map((c) => c.thd), 0.01);

  return (
    <div className="flex flex-col gap-1">
      {channels.map((ch) => {
        const color = getTHDColor(ch.thd);
        const barW = (ch.thd / maxThd) * 100;
        const h2 = ch.harmonics[0] ?? 0;
        const h3 = ch.harmonics[1] ?? 0;
        const evenOdd = h2 > h3 ? "2nd" : "3rd";

        return (
          <div
            key={ch.id}
            className="flex items-center gap-2 text-[8px] rounded px-2 py-1"
            style={{
              backgroundColor: ch.muted ? "#0a0f1a" : "#0d1520",
              border: `1px solid ${ch.muted ? "#0f1929" : ch.color + "20"}`,
              opacity: ch.muted ? 0.4 : 1,
            }}
          >
            {/* Channel name */}
            <span
              className="w-10 font-bold truncate tracking-wider"
              style={{ color: ch.color }}
            >
              {ch.name}
            </span>

            {/* THD bar */}
            <div className="flex-1 h-1.5 bg-neutral-900 rounded-full overflow-hidden">
              <div
                className="h-full rounded-full transition-all duration-300"
                style={{
                  width: `${barW}%`,
                  backgroundColor: color,
                  boxShadow: `0 0 4px ${color}50`,
                }}
              />
            </div>

            {/* THD value */}
            <span
              className="w-12 text-right font-mono font-bold"
              style={{ color }}
            >
              {ch.thd.toFixed(3)}%
            </span>

            {/* THD+N */}
            <span className="w-12 text-right font-mono text-neutral-600">
              +N {ch.thdN.toFixed(3)}
            </span>

            {/* Dominant harmonic */}
            <span
              className="w-6 text-center font-mono rounded px-0.5"
              style={{
                backgroundColor: color + "15",
                color: color,
                border: `1px solid ${color}25`,
              }}
            >
              {evenOdd}
            </span>

            {/* Level */}
            <span className="w-8 text-right font-mono text-neutral-600">
              {ch.level.toFixed(0)}
            </span>
          </div>
        );
      })}
    </div>
  );
}

// ─── Master Brain Plugin ─────────────────────────────────────────────────────
export default function MasterBrain({
  channels,
  masterThd,
  masterThdN,
  worstChannel,
}: MasterBrainProps) {
  const activeChannels = channels.filter((c) => !c.muted);
  const avgThd =
    activeChannels.length > 0
      ? activeChannels.reduce((a, c) => a + c.thd, 0) / activeChannels.length
      : 0;
  const maxThd = Math.max(...activeChannels.map((c) => c.thd), 0);
  const minThd =
    activeChannels.length > 0
      ? Math.min(...activeChannels.map((c) => c.thd))
      : 0;

  const masterColor = getTHDColor(masterThd);

  return (
    <div
      className="rounded-xl border overflow-hidden"
      style={{
        backgroundColor: "#080d16",
        borderColor: "#1a2540",
        boxShadow: `0 0 60px #00000090, 0 0 20px ${masterColor}10, inset 0 1px 0 #ffffff08`,
      }}
    >
      {/* Header */}
      <div
        className="px-4 py-2.5 flex items-center justify-between"
        style={{
          background: "linear-gradient(180deg, #0f1929 0%, #080d16 100%)",
          borderBottom: "1px solid #1a2540",
        }}
      >
        <div className="flex items-center gap-3">
          <div
            className="w-2 h-2 rounded-full animate-pulse"
            style={{ backgroundColor: masterColor }}
          />
          <span className="text-xs font-bold tracking-widest text-white">
            THD MASTER ANALYZER
          </span>
          <span className="text-[9px] text-neutral-600 tracking-wider">
            MIXBUS BRAIN
          </span>
        </div>
        <div className="flex items-center gap-3">
          <span className="text-[9px] text-neutral-600">
            {activeChannels.length}/{channels.length} ACTIVE
          </span>
          {worstChannel && (
            <div
              className="text-[8px] font-mono px-2 py-0.5 rounded"
              style={{
                backgroundColor: "#ef444415",
                color: "#ef4444",
                border: "1px solid #ef444430",
              }}
            >
              WORST: {worstChannel}
            </div>
          )}
          <div
            className="text-[9px] font-mono font-bold px-2 py-0.5 rounded"
            style={{
              backgroundColor: masterColor + "15",
              color: masterColor,
              border: `1px solid ${masterColor}30`,
            }}
          >
            {masterThd.toFixed(3)}% THD
          </div>
        </div>
      </div>

      <div className="p-4 flex gap-5">
        {/* Left column: Master gauge + stats */}
        <div className="flex flex-col gap-3 items-center" style={{ minWidth: "170px" }}>
          <MasterGauge thd={masterThd} thdN={masterThdN} />

          {/* Stats grid */}
          <div
            className="w-full rounded-lg p-2.5 grid grid-cols-2 gap-x-3 gap-y-1.5"
            style={{ backgroundColor: "#060b14", border: "1px solid #0f1929" }}
          >
            <div className="flex flex-col gap-0.5">
              <span className="text-[7px] text-neutral-600 tracking-widest">AVG THD</span>
              <span className="text-[10px] font-mono font-bold" style={{ color: getTHDColor(avgThd) }}>
                {avgThd.toFixed(3)}%
              </span>
            </div>
            <div className="flex flex-col gap-0.5">
              <span className="text-[7px] text-neutral-600 tracking-widest">MASTER</span>
              <span className="text-[10px] font-mono font-bold" style={{ color: masterColor }}>
                {masterThd.toFixed(3)}%
              </span>
            </div>
            <div className="flex flex-col gap-0.5">
              <span className="text-[7px] text-neutral-600 tracking-widest">PEAK</span>
              <span className="text-[10px] font-mono font-bold" style={{ color: getTHDColor(maxThd) }}>
                {maxThd.toFixed(3)}%
              </span>
            </div>
            <div className="flex flex-col gap-0.5">
              <span className="text-[7px] text-neutral-600 tracking-widest">FLOOR</span>
              <span className="text-[10px] font-mono font-bold text-neutral-400">
                {minThd.toFixed(3)}%
              </span>
            </div>
            <div className="flex flex-col gap-0.5 col-span-2">
              <span className="text-[7px] text-neutral-600 tracking-widest">THD+N</span>
              <span className="text-[10px] font-mono font-bold text-blue-400">
                {masterThdN.toFixed(3)}%
              </span>
            </div>
          </div>

          {/* Alert indicator */}
          <div
            className="w-full text-center text-[8px] font-bold rounded py-1 tracking-widest transition-all"
            style={{
              backgroundColor: masterThd > 1.0 ? "#ef444420" : masterThd > 0.5 ? "#f9731620" : "#22c55e15",
              color: masterThd > 1.0 ? "#ef4444" : masterThd > 0.5 ? "#f97316" : "#22c55e",
              border: `1px solid ${masterThd > 1.0 ? "#ef444440" : masterThd > 0.5 ? "#f9731640" : "#22c55e30"}`,
            }}
          >
            {masterThd > 1.0 ? "⚠ HIGH DISTORTION" : masterThd > 0.5 ? "△ MODERATE" : "✓ NOMINAL"}
          </div>
        </div>

        {/* Right column: Channel table + spectrum + timeline */}
        <div className="flex-1 flex flex-col gap-3 min-w-0">
          {/* Channel measurements table */}
          <div>
            <div className="text-[7px] text-neutral-600 mb-1.5 tracking-widest flex items-center gap-2">
              <span>CHANNEL MEASUREMENTS</span>
              <div className="flex-1 h-px bg-neutral-800" />
              <span className="text-neutral-700">THD · THD+N · DOM.HARM · LEVEL</span>
            </div>
            <ChannelTable channels={channels} />
          </div>

          {/* Harmonic spectrum */}
          <div>
            <div className="text-[7px] text-neutral-600 mb-1.5 tracking-widest flex items-center gap-2">
              <span>HARMONIC SPECTRUM (H2–H8)</span>
              <div className="flex-1 h-px bg-neutral-800" />
              <div className="flex gap-2">
                {channels.slice(0, 6).map((ch) => (
                  <div key={ch.id} className="flex items-center gap-1">
                    <div
                      className="w-1.5 h-1.5 rounded-full"
                      style={{ backgroundColor: ch.color }}
                    />
                    <span className="text-[6px] text-neutral-700">{ch.name}</span>
                  </div>
                ))}
              </div>
            </div>
            <HarmonicSpectrum channels={channels} />
          </div>

          {/* THD timeline */}
          <div>
            <div className="text-[7px] text-neutral-600 mb-1.5 tracking-widest flex items-center gap-2">
              <span>THD HISTORY (12s)</span>
              <div className="flex-1 h-px bg-neutral-800" />
              <span className="text-neutral-700">— MASTER &nbsp; — CHANNELS</span>
            </div>
            <THDTimeline channels={channels} masterThd={masterThd} />
          </div>
        </div>
      </div>
    </div>
  );
}
