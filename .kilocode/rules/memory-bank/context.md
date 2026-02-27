# Active Context: Next.js Starter Template

## Current State

**Template Status**: ✅ Ready for development

The template is a clean Next.js 16 starter with TypeScript and Tailwind CSS 4. It's ready for AI-assisted expansion to build any type of application.

## Recently Completed

- [x] Base Next.js 16 setup with App Router
- [x] TypeScript configuration with strict mode
- [x] Tailwind CSS 4 integration
- [x] ESLint configuration
- [x] Memory bank documentation
- [x] Recipe system for common features
- [x] THD Distortion Analyzer DAW plugin with NLS Summer master bus

## Current Structure

| File/Directory | Purpose | Status |
|----------------|---------|--------|
| `src/app/page.tsx` | Main DAW plugin page (8 channels + master) | ✅ Ready |
| `src/app/layout.tsx` | Root layout | ✅ Ready |
| `src/app/globals.css` | Global styles | ✅ Ready |
| `src/components/ChannelStrip.tsx` | Per-channel THD analyzer plugin | ✅ Ready |
| `src/components/NLSSummer.tsx` | NLS Summer master bus plugin | ✅ Ready |
| `src/lib/useAudioEngine.ts` | Real-time THD simulation engine | ✅ Ready |
| `.kilocode/` | AI context & recipes | ✅ Ready |

## Current Focus

THD Analyzer DAW plugin is live with:
- 8 default channels (KICK, SNARE, BASS, GTR L/R, KEYS, VOX, FX BUS)
- Per-channel: arc THD gauge, THD+N readout, harmonic spectrum bars, VU meter, drive/saturation/character controls, mute/solo
- NLS Summer master: spectrum analyzer, per-channel THD summary, sum mode (analog/digital/vintage/transformer), stereo VU, clip indicator
- Real-time simulation via requestAnimationFrame
- Add/remove channels dynamically

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
