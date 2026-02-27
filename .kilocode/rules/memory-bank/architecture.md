# System Patterns: THD Analyzer Project

## Architecture Overview

```
src/
├── app/                    # Next.js App Router (Web Version)
│   ├── layout.tsx          # Root layout + metadata
│   ├── page.tsx            # Home page
│   ├── globals.css         # Tailwind imports + global styles
│   └── favicon.ico         # Site icon
├── components/             # React components
│   ├── ChannelStrip.tsx   # Channel THD measurement UI
│   └── NLSSummer.tsx      # Master Brain analyzer UI
├── lib/                   # Audio engine utilities
│   ├── fftAnalyzer.ts     # Web Audio API FFT analyzer
│   └── useAudioEngine.ts  # Real-time audio engine
└── vst-plugin/            # Native VST Plugin (JUCE)
    ├── CMakeLists.txt     # Build configuration
    ├── README.md          # VST build instructions
    └── Source/
        ├── THDAnalyzerPlugin.h  # Plugin header + FFT classes
        └── THDAnalyzerPlugin.cpp # Plugin implementation
```

## Dual Architecture: Web + VST

The project now has two parallel implementations:

### 1. Web Version (Next.js + Web Audio API)
- Real-time FFT analysis in browser
- UI components for visualization
- Test signal generation
- Accessible via web browser

### 2. VST Plugin (JUCE C++)
- Native VST3 plugin for DAWs
- Same FFT analysis logic ported to C++
- Runs inside DAW (Ableton, Logic, Pro Tools, etc.)
- Needs JUCE framework to build

## Key Design Patterns

### 1. App Router Pattern

Uses Next.js App Router with file-based routing:
```
src/app/
├── page.tsx           # Route: /
├── about/page.tsx     # Route: /about
├── blog/
│   ├── page.tsx       # Route: /blog
│   └── [slug]/page.tsx # Route: /blog/:slug
└── api/
    └── route.ts       # API Route: /api
```

### 2. Component Organization Pattern (When Expanding)

```
src/components/
├── ui/                # Reusable UI components (Button, Card, etc.)
├── layout/            # Layout components (Header, Footer)
├── sections/          # Page sections (Hero, Features, etc.)
└── forms/             # Form components
```

### 3. Server Components by Default

All components are Server Components unless marked with `"use client"`:
```tsx
// Server Component (default) - can fetch data, access DB
export default function Page() {
  return <div>Server rendered</div>;
}

// Client Component - for interactivity
"use client";
export default function Counter() {
  const [count, setCount] = useState(0);
  return <button onClick={() => setCount(c => c + 1)}>{count}</button>;
}
```

### 4. Layout Pattern

Layouts wrap pages and can be nested:
```tsx
// src/app/layout.tsx - Root layout
export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}

// src/app/dashboard/layout.tsx - Nested layout
export default function DashboardLayout({ children }: { children: React.ReactNode }) {
  return (
    <div className="flex">
      <Sidebar />
      <main>{children}</main>
    </div>
  );
}
```

## Styling Conventions

### Tailwind CSS Usage
- Utility classes directly on elements
- Component composition for repeated patterns
- Responsive: `sm:`, `md:`, `lg:`, `xl:`

### Common Patterns
```tsx
// Container
<div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">

// Responsive grid
<div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">

// Flexbox centering
<div className="flex items-center justify-center">
```

## File Naming Conventions

- Components: PascalCase (`Button.tsx`, `Header.tsx`)
- Utilities: camelCase (`utils.ts`, `helpers.ts`)
- Pages/Routes: lowercase (`page.tsx`, `layout.tsx`)
- Directories: kebab-case (`api-routes/`) or lowercase (`components/`)

## State Management

For simple needs:
- `useState` for local component state
- `useContext` for shared state
- Server Components for data fetching

For complex needs (add when necessary):
- Zustand for client state
- React Query for server state
