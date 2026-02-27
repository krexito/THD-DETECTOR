"use client";

import { useState, useCallback, useRef, useEffect } from "react";
import ChannelStrip, { type ChannelData } from "../components/ChannelStrip";
import NLSSummer from "../components/NLSSummer";
import { useAudioEngine, computeMasterTHD } from "../lib/useAudioEngine";

const INITIAL_CHANNELS: ChannelData[] = [
  {
    id: "ch1",
    name: "KICK",
    thd: 0.12,
    thdN: 0.14,
    level: 75,
    peakLevel: 80,
    harmonics: [0.08, 0.04, 0.02, 0.01, 0.005, 0.002, 0.001],
    saturation: 20,
    drive: 35,
    character: "warm",
    muted: false,
    soloed: false,
    color: "#f97316",
  },
  {
    id: "ch2",
    name: "SNARE",
    thd: 0.08,
    thdN: 0.09,
    level: 65,
    peakLevel: 70,
    harmonics: [0.05, 0.08, 0.06, 0.03, 0.01, 0.005, 0.002],
    saturation: 15,
    drive: 25,
    character: "bright",
    muted: false,
    soloed: false,
    color: "#60a5fa",
  },
  {
    id: "ch3",
    name: "BASS",
    thd: 0.35,
    thdN: 0.38,
    level: 80,
    peakLevel: 85,
    harmonics: [0.25, 0.12, 0.05, 0.02, 0.01, 0.005, 0.002],
    saturation: 40,
    drive: 55,
    character: "vintage",
    muted: false,
    soloed: false,
    color: "#a78bfa",
  },
  {
    id: "ch4",
    name: "GTR L",
    thd: 0.22,
    thdN: 0.25,
    level: 60,
    peakLevel: 65,
    harmonics: [0.15, 0.1, 0.07, 0.04, 0.02, 0.01, 0.005],
    saturation: 30,
    drive: 45,
    character: "warm",
    muted: false,
    soloed: false,
    color: "#34d399",
  },
  {
    id: "ch5",
    name: "GTR R",
    thd: 0.19,
    thdN: 0.22,
    level: 58,
    peakLevel: 63,
    harmonics: [0.12, 0.09, 0.06, 0.03, 0.015, 0.008, 0.004],
    saturation: 28,
    drive: 42,
    character: "warm",
    muted: false,
    soloed: false,
    color: "#2dd4bf",
  },
  {
    id: "ch6",
    name: "KEYS",
    thd: 0.05,
    thdN: 0.06,
    level: 55,
    peakLevel: 58,
    harmonics: [0.03, 0.02, 0.01, 0.005, 0.002, 0.001, 0.0005],
    saturation: 10,
    drive: 15,
    character: "clean",
    muted: false,
    soloed: false,
    color: "#fbbf24",
  },
  {
    id: "ch7",
    name: "VOX",
    thd: 0.07,
    thdN: 0.09,
    level: 70,
    peakLevel: 75,
    harmonics: [0.05, 0.03, 0.015, 0.007, 0.003, 0.001, 0.0005],
    saturation: 12,
    drive: 20,
    character: "warm",
    muted: false,
    soloed: false,
    color: "#f472b6",
  },
  {
    id: "ch8",
    name: "FX BUS",
    thd: 0.03,
    thdN: 0.04,
    level: 45,
    peakLevel: 50,
    harmonics: [0.02, 0.01, 0.005, 0.002, 0.001, 0.0005, 0.0002],
    saturation: 5,
    drive: 10,
    character: "clean",
    muted: false,
    soloed: false,
    color: "#94a3b8",
  },
];

export default function Home() {
  const [channels, setChannels] = useState<ChannelData[]>(INITIAL_CHANNELS);
  const [sumMode, setSumMode] = useState<
    "analog" | "digital" | "vintage" | "transformer"
  >("analog");
  const [sumDrive, setSumDrive] = useState(30);
  const [sumSaturation, setSumSaturation] = useState(20);
  const [masterLevel, setMasterLevel] = useState(72);
  const [masterPeak, setMasterPeak] = useState(78);
  const [masterThd, setMasterThd] = useState(0.18);
  const [masterThdN, setMasterThdN] = useState(0.21);

  const channelsRef = useRef(channels);
  useEffect(() => {
    channelsRef.current = channels;
  });

  const handleChannelUpdate = useCallback(
    (updates: Map<string, Partial<ChannelData>>) => {
      setChannels((prev) => {
        const next = prev.map((ch) => {
          const upd = updates.get(ch.id);
          return upd ? { ...ch, ...upd } : ch;
        });

        // Recompute master THD
        const { thd, thdN } = computeMasterTHD(
          next,
          sumMode,
          sumDrive,
          sumSaturation
        );
        setMasterThd(thd);
        setMasterThdN(thdN);

        return next;
      });
    },
    [sumMode, sumDrive, sumSaturation]
  );

  const handleMasterUpdate = useCallback((level: number, peak: number) => {
    setMasterLevel(level);
    setMasterPeak(peak);
  }, []);

  useAudioEngine(channels, handleChannelUpdate, masterLevel, handleMasterUpdate);

  const handleChannelChange = useCallback(
    (id: string, updates: Partial<ChannelData>) => {
      setChannels((prev) =>
        prev.map((ch) => (ch.id === id ? { ...ch, ...updates } : ch))
      );
    },
    []
  );

  const isSoloing = channels.some((c) => c.soloed);

  const addChannel = () => {
    const colors = [
      "#f97316", "#60a5fa", "#a78bfa", "#34d399",
      "#fbbf24", "#f472b6", "#38bdf8", "#fb923c",
    ];
    const newCh: ChannelData = {
      id: `ch${Date.now()}`,
      name: `CH ${channels.length + 1}`,
      thd: 0.05,
      thdN: 0.06,
      level: 60,
      peakLevel: 65,
      harmonics: [0.03, 0.02, 0.01, 0.005, 0.002, 0.001, 0.0005],
      saturation: 10,
      drive: 20,
      character: "clean",
      muted: false,
      soloed: false,
      color: colors[channels.length % colors.length],
    };
    setChannels((prev) => [...prev, newCh]);
  };

  const removeChannel = (id: string) => {
    setChannels((prev) => prev.filter((c) => c.id !== id));
  };

  return (
    <main
      className="min-h-screen"
      style={{
        background: "linear-gradient(180deg, #060b14 0%, #0a0f1a 100%)",
        fontFamily: "var(--font-geist-mono)",
      }}
    >
      {/* Top bar */}
      <div
        className="sticky top-0 z-10 px-6 py-2 flex items-center justify-between"
        style={{
          background: "linear-gradient(180deg, #0d1117 0%, #060b14 100%)",
          borderBottom: "1px solid #1e293b",
          boxShadow: "0 4px 20px #00000060",
        }}
      >
        <div className="flex items-center gap-4">
          <div className="flex gap-1.5">
            <div className="w-3 h-3 rounded-full bg-red-500" />
            <div className="w-3 h-3 rounded-full bg-yellow-500" />
            <div className="w-3 h-3 rounded-full bg-green-500" />
          </div>
          <div className="flex items-center gap-2">
            <span className="text-xs font-bold text-white tracking-widest">
              THD ANALYZER
            </span>
            <span className="text-[9px] text-neutral-500 tracking-wider">
              v1.0 — NLS SUMMER EDITION
            </span>
          </div>
        </div>

        <div className="flex items-center gap-3">
          <div className="flex items-center gap-1.5 text-[9px] text-neutral-500">
            <div className="w-1.5 h-1.5 rounded-full bg-green-500 animate-pulse" />
            LIVE
          </div>
          <button
            onClick={addChannel}
            className="text-[9px] font-bold px-3 py-1 rounded transition-all"
            style={{
              backgroundColor: "#1a1f2e",
              color: "#60a5fa",
              border: "1px solid #1e3a5f",
            }}
          >
            + ADD CHANNEL
          </button>
        </div>
      </div>

      <div className="p-6 flex flex-col gap-6">
        {/* Channel strips */}
        <div>
          <div className="text-[8px] text-neutral-600 mb-3 tracking-widest px-1">
            CHANNEL STRIPS — THD DISTORTION ANALYZER
          </div>
          <div className="flex gap-3 overflow-x-auto pb-2">
            {channels.map((ch) => (
              <div key={ch.id} className="relative flex-shrink-0 group">
                <ChannelStrip
                  channel={ch}
                  onChange={handleChannelChange}
                  isSoloing={isSoloing}
                />
                {channels.length > 1 && (
                  <button
                    onClick={() => removeChannel(ch.id)}
                    className="absolute -top-1 -right-1 w-4 h-4 rounded-full text-[8px] font-bold opacity-0 group-hover:opacity-100 transition-opacity flex items-center justify-center"
                    style={{
                      backgroundColor: "#ef4444",
                      color: "white",
                    }}
                  >
                    ×
                  </button>
                )}
              </div>
            ))}
          </div>
        </div>

        {/* NLS Summer Master */}
        <div>
          <div className="text-[8px] text-neutral-600 mb-3 tracking-widest px-1">
            MASTER BUS — NLS SUMMER
          </div>
          <NLSSummer
            channels={channels}
            masterThd={masterThd}
            masterThdN={masterThdN}
            masterLevel={masterLevel}
            masterPeak={masterPeak}
            sumMode={sumMode}
            sumDrive={sumDrive}
            sumSaturation={sumSaturation}
            onSumModeChange={setSumMode}
            onSumDriveChange={setSumDrive}
            onSumSaturationChange={setSumSaturation}
          />
        </div>

        {/* Footer info */}
        <div className="text-[8px] text-neutral-700 text-center tracking-widest pb-4">
          THD ANALYZER — TOTAL HARMONIC DISTORTION MEASUREMENT PLUGIN
          <br />
          CHANNELS CONNECT TO NLS SUMMER MASTER BUS FOR ANALOG SUMMING SIMULATION
        </div>
      </div>
    </main>
  );
}
