# 0001 - A desktop app in JUCE, not a ReaScript

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

Every app in the family so far is a ReaScript or a JSFX living inside REAPER,
except ScaleView, which is a JUCE plugin. Noterator is asked to be a notation
program in its own right - the thing Dorico, MuseScore, Sibelius and Notion are
- on macOS first and Windows later, with VST3 and CLAP instruments to come.

## Decision

A standalone JUCE 8 application, C++17, built with CMake, JUCE pinned to the
same tag ScaleView builds with (8.0.15). Apple silicon only (arm64,
macOS 11 and later): no Intel build is wanted.

## Why

- A notation program owns its window, its transport, its files and its audio.
  None of that can be borrowed from REAPER without becoming a REAPER script.
- JUCE is already the family's tool for native code (ScaleView), it builds the
  same source for macOS and Windows, and it hosts VST3 and Audio Unit
  instruments out of the box. CLAP hosting is the one part it does not do, and
  it can be added beside it.
- Hosting is what makes "plays through a real orchestra" possible from day one:
  Apple's General MIDI Audio Unit is on every Mac (0007).

## Consequences

- The musical core stays free of JUCE (`Source/Core`), as ScaleView's
  `ScaleModel.h` is, so it builds and tests in seconds without the framework.
- Nothing here can be built for macOS on the Linux machines the work is done
  on. GitHub Actions builds the Mac app on every push (`.github/workflows`), and
  the Linux build compiles the same app to catch everything but Apple-only code.
- JUCE's licence: free under its Starter terms for a project of this size, as
  for ScaleView. Revisit before selling anything.
