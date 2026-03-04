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

// ─── THD Frequency Intensity Map ────────────────────────────────────────────
function THDFrequencyMap({
  channels,
  width = 420,
  height = 82,
  className = "w-full rounded border border-neutral-800/60",
}: {
  channels: ChannelData[];
  width?: number;
  height?: number;
  className?: string;
}) {
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

      const w = canvas.width;
      const h = canvas.height;
      const spectrumH = h - 18;
      const activeChannels = channelsRef.current.filter((ch) => !ch.muted);

      ctx.fillStyle = "#050911";
      ctx.fillRect(0, 0, w, h);

      // Grid
      ctx.strokeStyle = "#132034";
      ctx.lineWidth = 0.5;
      for (let i = 0; i <= 10; i++) {
        const x = (i / 10) * w;
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, spectrumH);
        ctx.stroke();
      }
      for (let i = 0; i <= 4; i++) {
        const y = (i / 4) * spectrumH;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      // Frequency color ramp
      const baseGrad = ctx.createLinearGradient(0, 0, w, 0);
      baseGrad.addColorStop(0, "#3b82f6");
      baseGrad.addColorStop(0.25, "#22d3ee");
      baseGrad.addColorStop(0.5, "#84cc16");
      baseGrad.addColorStop(0.7, "#facc15");
      baseGrad.addColorStop(0.85, "#fb923c");
      baseGrad.addColorStop(1, "#ef4444");

      ctx.fillStyle = baseGrad;
      ctx.globalAlpha = 0.17;
      ctx.fillRect(0, 0, w, spectrumH);
      ctx.globalAlpha = 1;

      const bins = 84;
      const binW = w / bins;
      const binEnergy = new Array<number>(bins).fill(0);

      const minF = 20;
      const maxF = 20000;
      const nyquist = maxF;

      activeChannels.forEach((ch) => {
        const levelNorm = Math.max(0.2, ch.level / 100);
        ch.harmonics.forEach((harm, hi) => {
          const harmonicN = hi + 2;
          const harmonicFreq = 100 * harmonicN; // pseudo fundamental for display map
          const xNorm = Math.log10(harmonicFreq / minF) / Math.log10(maxF / minF);
          const center = Math.max(0, Math.min(bins - 1, Math.round(xNorm * (bins - 1))));

          const intensity = Math.min(1, harm * 2.6 * levelNorm + ch.thd * 0.4);
          const spread = Math.max(1, Math.round((bins / 28) * (1 - harmonicFreq / nyquist + 0.25)));

          for (let o = -spread; o <= spread; o++) {
            const idx = center + o;
            if (idx < 0 || idx >= bins) continue;
            const falloff = 1 - Math.abs(o) / (spread + 1);
            binEnergy[idx] = Math.min(1, binEnergy[idx] + intensity * falloff * 0.55);
          }
        });
      });

      const zeroY = spectrumH * 0.58;

      // Draw bars with color intensity by frequency and magnitude
      for (let i = 0; i < bins; i++) {
        const energy = binEnergy[i];
        if (energy <= 0.01) continue;

        const x = i * binW;
        const barH = Math.max(1, energy * (spectrumH * 0.55));
        const xPosNorm = i / (bins - 1);

        const hue = 220 - xPosNorm * 220; // blue -> red
        const sat = 90;
        const light = 45 + energy * 12;
        const color = `hsl(${hue}, ${sat}%, ${light}%)`;

        ctx.fillStyle = color;
        ctx.shadowColor = color;
        ctx.shadowBlur = 6;
        ctx.fillRect(x + 0.4, zeroY - barH, Math.max(1, binW - 1), barH);

        // reflected lower energy
        ctx.globalAlpha = 0.24;
        ctx.fillRect(x + 0.4, zeroY, Math.max(1, binW - 1), barH * 0.95);
        ctx.globalAlpha = 1;
      }
      ctx.shadowBlur = 0;

      // Overlay smooth contour line
      if (activeChannels.length > 0) {
        ctx.beginPath();
        binEnergy.forEach((energy, i) => {
          const x = i * binW + binW * 0.5;
          const y = zeroY - energy * (spectrumH * 0.55);
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        });
        ctx.strokeStyle = "#e2e8f070";
        ctx.lineWidth = 1.5;
        ctx.stroke();
      }

      // Center zero line
      ctx.strokeStyle = "#94a3b866";
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, zeroY);
      ctx.lineTo(w, zeroY);
      ctx.stroke();

      // Bottom legend
      ctx.fillStyle = "#6b7280";
      ctx.font = "6px monospace";
      ctx.textAlign = "left";
      ctx.fillText("20Hz", 2, h - 3);
      ctx.textAlign = "center";
      ctx.fillText("200Hz", w * 0.27, h - 3);
      ctx.fillText("2kHz", w * 0.57, h - 3);
      ctx.fillText("20kHz", w * 0.92, h - 3);
      ctx.textAlign = "right";
      ctx.fillStyle = "#94a3b8";
      ctx.fillText("THD INTENSITY", w - 3, 7);

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
      width={width}
      height={height}
      className={className}
    />
  );
}

function VintageKnob({
  label,
  value,
  accent,
}: {
  label: string;
  value: string;
  accent: string;
}) {
  return (
    <div className="flex flex-col items-center gap-2 min-w-[110px]">
      <div
        className="w-full rounded-md px-3 py-2 text-center font-mono"
        style={{
          background: "linear-gradient(180deg, #04070d 0%, #090d16 100%)",
          border: "1px solid #0a111d",
          boxShadow: "inset 0 1px 0 #ffffff10, 0 8px 16px #00000070",
          color: accent,
        }}
      >
        <div className="text-xl leading-none">{value}</div>
      </div>
      <div
        className="w-12 h-12 rounded-full relative"
        style={{
          background: "radial-gradient(circle at 35% 30%, #73767f 0%, #31343d 48%, #161922 100%)",
          border: "1px solid #0f1724",
          boxShadow: "inset 0 1px 3px #ffffff22, 0 6px 14px #00000080",
        }}
      >
        <div className="absolute left-1/2 top-[7px] -translate-x-1/2 w-[2px] h-[14px] rounded" style={{ backgroundColor: "#e5e7eb" }} />
      </div>
      <div className="text-[10px] tracking-wider text-neutral-400">{label}</div>
    </div>
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
  const minThd = activeChannels.length > 0 ? Math.min(...activeChannels.map((c) => c.thd)) : 0;
  const masterColor = getTHDColor(masterThd);

  return (
    <div
      className="rounded-2xl border overflow-hidden"
      style={{
        background: "linear-gradient(180deg, #2d3138 0%, #171b22 44%, #0c0f15 100%)",
        borderColor: "#1b1f28",
        boxShadow: "0 24px 60px #000000aa, inset 0 1px 0 #ffffff12, inset 0 -1px 0 #000000d0",
      }}
    >
      <div
        className="px-4 md:px-6 py-3 flex items-center justify-between"
        style={{
          background: "linear-gradient(180deg, #474c54 0%, #333841 58%, #272c35 100%)",
          borderBottom: "1px solid #0f1218",
        }}
      >
        <div className="w-5 h-5 rounded-full border border-black/70 bg-gradient-to-b from-neutral-600 to-neutral-900" />
        <div className="text-neutral-200 text-4xl leading-none tracking-[0.22em]">THD</div>
        <div className="w-5 h-5 rounded-full border border-black/70 bg-gradient-to-b from-neutral-600 to-neutral-900" />
      </div>

      <div className="p-4 md:p-5 space-y-4">
        <div
          className="rounded-xl p-2 md:p-3"
          style={{
            background: "linear-gradient(180deg, #070b12 0%, #090d14 100%)",
            border: "1px solid #000000",
            boxShadow: "inset 0 0 28px #000000d0, 0 0 0 1px #1f2531",
          }}
        >
          <div className="flex items-center justify-between px-1.5 pb-2">
            <div className="text-[10px] tracking-[0.2em] text-neutral-500">THD SPECTRUM</div>
            <div className="text-[10px] font-mono" style={{ color: masterColor }}>
              MASTER {masterThd.toFixed(3)}%
            </div>
          </div>
          <THDFrequencyMap
            channels={channels}
            width={1250}
            height={460}
            className="w-full rounded-lg border border-[#1b2330]"
          />
        </div>

        <div className="grid grid-cols-2 md:grid-cols-4 gap-3 items-end">
          <VintageKnob label="THD" value={avgThd.toFixed(2)} accent="#22d3ee" />
          <div className="flex flex-col items-center gap-2">
            <div
              className="w-full min-w-[170px] rounded-md px-4 py-3 text-center font-mono"
              style={{
                background: "linear-gradient(180deg, #020406 0%, #0a0f18 100%)",
                border: "1px solid #060b11",
                boxShadow: "inset 0 1px 0 #ffffff10, 0 8px 18px #00000080",
                color: "#22d3ee",
              }}
            >
              <div className="text-5xl leading-none">{masterThd.toFixed(1)}%</div>
            </div>
            <div className="text-[11px] tracking-[0.18em] text-neutral-300">OUTPUT</div>
          </div>
          <VintageKnob label="ANALYZER MODE" value={worstChannel ?? "AUTO"} accent="#c4b5fd" />
          <VintageKnob label="SCALE" value={maxThd.toFixed(1)} accent="#67e8f9" />
        </div>

        <div className="grid grid-cols-2 md:grid-cols-5 gap-2 text-[10px] font-mono">
          <div className="rounded px-2 py-1 bg-black/35 border border-neutral-900 text-neutral-300">MASTER: <span style={{ color: masterColor }}>{masterThd.toFixed(3)}%</span></div>
          <div className="rounded px-2 py-1 bg-black/35 border border-neutral-900 text-neutral-300">THD+N: <span className="text-blue-300">{masterThdN.toFixed(3)}%</span></div>
          <div className="rounded px-2 py-1 bg-black/35 border border-neutral-900 text-neutral-300">AVG: <span style={{ color: getTHDColor(avgThd) }}>{avgThd.toFixed(3)}%</span></div>
          <div className="rounded px-2 py-1 bg-black/35 border border-neutral-900 text-neutral-300">FLOOR: <span className="text-neutral-400">{minThd.toFixed(3)}%</span></div>
          <div className="rounded px-2 py-1 bg-black/35 border border-neutral-900 text-neutral-300">ACTIVE: <span className="text-emerald-300">{activeChannels.length}</span></div>
        </div>

        <details className="rounded-lg border border-neutral-900 bg-black/25 p-3">
          <summary className="cursor-pointer text-[10px] text-neutral-500 tracking-[0.2em]">ADVANCED TELEMETRY</summary>
          <div className="mt-3 grid md:grid-cols-[1fr_170px] gap-3 items-start">
            <div className="space-y-2">
              <THDTimeline channels={channels} masterThd={masterThd} />
              <HarmonicSpectrum channels={channels} />
            </div>
            <div className="space-y-2">
              <MasterGauge thd={masterThd} thdN={masterThdN} />
              <ChannelTable channels={channels} />
            </div>
          </div>
        </details>
      </div>
    </div>
  );
}
