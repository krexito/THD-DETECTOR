import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "THD Analyzer — NLS Summer Edition",
  description: "DAW plugin for measuring Total Harmonic Distortion per channel with NLS Summer master bus",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className="antialiased">{children}</body>
    </html>
  );
}
