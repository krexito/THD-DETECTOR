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
- [x] Plugin reset hardening — centralized startup state reset in `reset()` and invoked from `prepareToPlay` for cleaner audio initialization

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

| Session 5 | VST3 stabilization pass — fixed JUCE/CMake integration issues, repaired processor syntax/logic errors, added FIFO-based FFT windowing and safer SysEx serialization for channel/master communication |
| Session 6 | Merge-conflict cleanup pass — verified repository has no unresolved conflict markers and documented PR merge readiness status |
| Session 7 | VST plugin branding update — changed JUCE `PRODUCT_NAME` to `THD - TotalHarmonicDisplay` so hosts display the new plugin name |
| Session 8 | Audio-thread safety hardening pass — removed per-block allocations in `processBlock`, added reusable scratch buffers, and ensured Master mode clears consumed MIDI to avoid forwarding stale SysEx events |

| Session 9 | VST automation/state pass — moved host-visible settings to `AudioProcessorValueTreeState` (plugin mode, channel ID, mute/solo flags), switched state save/restore to APVTS ValueTree, and added atomic cached reads for audio-thread-safe parameter access. |
| Session 9 | Initialization and debug workflow pass — documented Visual Studio/Xcode auto-launch setup for AudioPluginHost and centralized processor state clearing via `reset()` for clean plugin starts |
| Session 10 | PR conflict-support docs pass — added step-by-step local merge conflict resolution workflow and marker sanity check to VST local build guide |
| Session 11 | PR #7 follow-up hardening — cached APVTS raw parameter pointers and listener-driven cache sync to remove per-block parameter lookups in `processBlock`, plus legacy `THDAnalyzerSettings` XML migration into current mute/solo APVTS parameters during state restore. |
| Session 11 | VST build-fix pass — replaced umbrella JUCE include with explicit `juce_audio_utils` + `juce_dsp` headers, corrected `createEditor()` to construct `GenericAudioProcessorEditor` with `*this`, and added `JUCE_VST3_CAN_REPLACE_VST2=0` compile definition in CMake. |


| Session 12 | JUCE UI implementation pass — replaced GenericAudioProcessorEditor with a custom `THDAnalyzerPluginEditor`, added THD Analyzer v2.0 native layout (sticky header, scrollable channel cards, master brain placeholders), and wired CMake to compile the new editor source files. |
| Session 13 | PR #10 follow-up fix pass — bound custom editor controls to APVTS parameters (plugin mode, channel ID, per-channel mute/solo) and removed randomized THD display updates in favor of processor analysis/channel data snapshots. |
| Session 14 | VST editor compile-fix pass — made `CanvasPlaceholder` inherit `juce::SettableTooltipClient` so tooltip updates in `timerCallback()` compile correctly with JUCE. |


| Session 15 | VST3 bus-layout compatibility pass — aligned processor bus declaration, `isBusesLayoutSupported`, and JUCE `PLUGIN_CHANNEL_CONFIGURATIONS` to stereo-only `{2,2}` to prevent host scan-time null-component channel queries. |

| Session 16 | VST3 scan-compat follow-up — tightened `isBusesLayoutSupported` to reject non-main/aux bus layouts and disabled buses, keeping runtime acceptance strictly aligned to a single stereo input/output bus. |

| Session 17 | VST GUI crash fix pass — clamped animated header pulse alpha into [0,1] before `withAlpha()` to avoid JUCE debug assertion breaks in host paint loop. |
| Session 18 | VST editor UX pass — replaced temporary canvas placeholders (waveform/gauge/harmonic/history) with live-rendered analyzer widgets driven by processor analysis snapshots. |
| Session 19 | Dynamic channel-topology pass — converted VST channel registry from fixed 8 pre-seeded channels to runtime-discovered channels (created on incoming SysEx), added stale-channel pruning, expanded Channel ID selection to 64 IDs, wired header add/remove channel buttons to real processor actions, and refreshed editor cards from live channel snapshots so Channel Strip ↔ Master Brain behavior matches dynamic DAW routing. |
| Session 20 | VST GUI callback lifetime hardening — replaced raw `this` captures in channel remove/add callbacks with `juce::Component::SafePointer` guards to avoid use-after-free during click dispatch while cards/editor are rebuilt. |
| Session 21 | Master Brain UX alignment pass — removed manual Add/Remove channel controls from the VST editor so channel cards are read-only and sourced only from auto-detected channel instances (snapshot/SysEx discovery). |
| Session 22 | Master Brain display stabilization pass — removed animated HeaderBar MEASURING pulse (static indicator to avoid visual artifacting) and added gated attack/release smoothing for displayed THD+N/history values to reduce erratic meter jumps on low-level/noisy input. |
| Session 23 | Web Audio analyzer stability fix — tracked both primary and detune oscillators per channel so `stopSignal()`/`dispose()` stop every started oscillator, preventing leaked audio nodes and improving repeated analyzer start/stop behavior. |
| Session 24 | VST Master Brain readability pass — added Fast/Legible display-speed modes with smoothing for AVG/MASTER/PEAK/FLOOR/THD+N + harmonic bars, and removed the Channel ID dropdown from the header UI to avoid the large manual channel list. |
| Session 25 | VST GUI visual-alignment pass — restyled header/channel cards/master panel to match the provided NLS Summer reference look (title treatment, status pill, 3-column master layout, richer section labels, and peak-channel footer) while preserving existing analyzer behavior and smoothing controls. |
| Session 26 | VST CPU-optimization pass — reduced per-instance audio-thread load by throttling FFT analysis to hop intervals (25% overlap), skipping analyzer runs in Master Brain mode, preallocating FFT magnitude storage, and reusing precomputed harmonic bins inside THD/THD+N computation. |
| Session 27 | VST mode/connection fix pass — made Channel Strip vs Master Brain UI visibly switch layouts, hid master-only channel list in strip mode, and added shared in-process channel telemetry fallback so Master Brain can show Channel Strip metrics even when host MIDI routing is unavailable. |

