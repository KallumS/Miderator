# 0012 - Apple silicon only, built and tested on a Mac runner

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

The work is done on Linux machines that cannot build a Mac app, and the user
is not a developer: an app that has to be compiled at home does not get used.
The first build was universal (Intel and Apple silicon); the user said the
Intel half is not wanted.

## Decision

- macOS 11 and later, **arm64 only** (`CMAKE_OSX_ARCHITECTURES`, CMake's Apple
  block and the workflow both say so).
- GitHub Actions (`.github/workflows/build.yml`) builds the app on a macOS
  runner on every push, runs every test there, ad-hoc signs the app and
  packages a `.dmg` with `docs/INSTALL-MAC.txt` inside it as "Read me first".
  The `.dmg` is the run's artifact; a `v*` tag also publishes it as a Release.
- A Linux job runs the generators through plain Lua, the core tests, compiles
  the app and renders the engraving test page.
- **`NoteratorAppTests` is the Mac test bench**: it drives the real
  `Controller`, exports MIDI and reads it back, and renders audio through each
  synth and checks it is not silent. On the Mac runner it is the only proof
  that Apple's General MIDI Audio Unit loads and sounds.

## Consequences

- Anything Apple-only (the Audio Unit code in `AudioEngine.cpp`, CMake's Apple
  settings, packaging) is checked only by that run. Read its log after
  touching them.
- The app is not signed with a Developer ID, so its first launch needs
  right-click > Open. Signing and notarising need the user's Apple Developer
  account, as secrets in the repository.
- Windows, when it comes, is another job in the same workflow.
