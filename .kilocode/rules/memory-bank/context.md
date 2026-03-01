# Active Context: THD Analyzer Project

## Current State

**Project Status**: ✅ Active Development - VST Plugin Development

The THD Analyzer project has evolved from a web-based analyzer to include native VST plugin development. The project now has two components:

1. **Web Version**: Real-time THD analyzer using Web Audio API (complete)
2. **VST Plugin**: Native VST3 plugin using JUCE framework (in development)

## Recently Completed

- [x] Base Next.js 16 setup with App Router
- [x] TypeScript configuration with strict mode
- [x] Tailwind CSS 4 integration
- [x] ESLint configuration
- [x] Memory bank documentation
- [x] Recipe system for common features
- [x] THD Distortion Analyzer DAW plugin — v1 (with NLS Summer coloring controls)
- [x] THD Analyzer v2 — Pure measurement architecture (no signal coloring)
- [x] THD Analyzer v3 — Real FFT analysis using Web Audio API AnalyserNode
- [x] VST Plugin Communication System — MIDI-based Channel Strip ↔ Master Brain communication
- [x] Fixed FFT analyzer signal lifecycle bug: detune oscillators are now tracked/stopped per channel and detune gain follows level automation

## Current Structure

| File/Directory | Purpose | Status |
|----------------|---------|--------|
| `src/app/page.tsx` | Main DAW plugin page (8 channels + master brain) | ✅ Ready |
| `src/app/layout.tsx` | Root layout | ✅ Ready |
| `src/app/globals.css` | Global styles | ✅ Ready |
| `src/components/ChannelStrip.tsx` | Per-channel THD measurement plugin (no coloring) | ✅ Ready |
| `src/components/NLSSummer.tsx` | Master Brain plugin (mixbus analyzer) | ✅ Ready |
| `src/lib/useAudioEngine.ts` | Real-time audio engine with FFT analysis | ✅ Ready |
| `src/lib/fftAnalyzer.ts` | FFT-based THD analyzer using Web Audio API | ✅ Ready |
| `src/vst-plugin/` | Native VST3 plugin (JUCE framework) | 🚧 In Development |
| `.kilocode/` | AI context & recipes | ✅ Ready |

## Architecture (v2 — Pure Measurement)

### Design Concept
- **Channel plugins** = passive THD analyzers inserted on each channel strip
  - Measure THD, THD+N, harmonics H2–H8 from signal level
  - Display: arc gauge, THD+N readout, VU meter, status badge
  - NO drive/saturation/character controls — zero signal coloring
- **Master Brain plugin** = inserted on the mixbus
  - Receives all channel measurement data
  - Displays: master THD gauge, channel table (THD/THD+N/dominant harmonic/level), harmonic spectrum H2–H8, rolling THD history timeline, alert indicator

### VST Plugin Communication System
The VST plugins communicate using MIDI System Exclusive messages:
- **Channel Strip Mode**: Each plugin instance sends THD data via MIDI to the Master Brain
- **Master Brain Mode**: Receives MIDI data from all Channel Strip plugins
- **Protocol**: Custom SysEx (0xF0 0x7D 0x01 ...) containing channel ID, THD, THD+N, level, peak, harmonics
- **Implementation**: `THDDataMessage` struct with MIDI serialization/deserialization

### ChannelData Interface
```ts
interface ChannelData {
  id: string; name: string;
  thd: number; thdN: number;
  level: number; peakLevel: number;
  harmonics: number[];   // H2–H8
  muted: boolean; soloed: boolean;
  color: string;
}
```
(Removed: drive, saturation, character)

### Engine
- `FFTAnalyzer` class — Real FFT analysis using Web Audio API AnalyserNode
  - FFT size: 32768 bins for high resolution
  - Interpolated peak detection for precision
  - Fundamental frequency detection (20Hz-2kHz range)
  - Harmonic analysis H2-H8
  - Automatic noise floor estimation
- `measureTHD(level, channelId, time)` — fallback simulation (deprecated)
- `computeMasterTHD(channels)` — RSS aggregation, returns worstChannel
- `useAudioEngine(channels, onUpdate)` — real-time FFT analysis loop

## Current Focus

**VST Plugin Development (JUCE)**
- Created VST3 plugin project structure using JUCE framework
- Ported FFT analysis logic from TypeScript to C++
- Implemented 8-channel THD analyzer in native code
- Using JUCE DSP FFT module (8192 size)
- Next: Add plugin UI and test functionality

**Web Version**: Maintained for testing and demo purposes

## Quick Start Guide

### To add a new page:

Create a file at `src/app/[route]/page.tsx`:
```tsx
export default function NewPage() {
  return <div>New page content</div>;
}
```

### To add components:

Create `src/components/` directory and add components:
```tsx
// src/components/ui/Button.tsx
export function Button({ children }: { children: React.ReactNode }) {
  return <button className="px-4 py-2 bg-blue-600 text-white rounded">{children}</button>;
}
```

### To add a database:

Follow `.kilocode/recipes/add-database.md`

### To add API routes:

Create `src/app/api/[route]/route.ts`:
```tsx
import { NextResponse } from "next/server";

export async function GET() {
  return NextResponse.json({ message: "Hello" });
}
```

## Available Recipes

| Recipe | File | Use Case |
|--------|------|----------|
| Add Database | `.kilocode/recipes/add-database.md` | Data persistence with Drizzle + SQLite |

## Pending Improvements

- [ ] Add more recipes (auth, email, etc.)
- [ ] Add example components
- [ ] Add testing setup recipe

## Session History

| Date | Changes |
|------|---------|
| Initial | Template created with base setup |
| Session 1 | THD Analyzer v1 — NLS Summer with drive/saturation/character controls |
| Session 2 | THD Analyzer v2 — Pure measurement architecture, Master Brain plugin |
| Session 3 | THD Analyzer v3 — Real FFT analysis using Web Audio API AnalyserNode |
| Session 4 | VST Plugin Development — Started native VST3 plugin with JUCE framework |
| Session 5 | Fixed Web FFT analyzer signal cleanup bug (detune oscillator leak) and synchronized detune gain automation with channel level updates |
