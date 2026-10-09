# 0007 - One performance for playback and export, through the Mac's own General MIDI set

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

Playback, audio export and MIDI export each need the score as MIDI. Built
separately, they disagree. And a notation program that is silent until the
user finds and installs sample libraries does not get used.

## Decision

`Perform.cpp` turns the score into channel events once - programs, levels,
notes, AutoCC - and playback, the WAV bounce and the .mid all read it.

Sound comes from Apple's DLSMusicDevice, the General MIDI Audio Unit every Mac
has, hosted through JUCE. Where it cannot be loaded (Linux, and Windows for
now) a small built-in synth stands in, so the app is never silent.

## Consequences

- General MIDI is a sketching sound, not a production one. Per-part VST3 and
  CLAP instruments slot in behind the same `SynthBackend`, and the DLS device
  can load any SoundFont (`kMusicDeviceProperty_SoundBankURL`) - both are on the
  roadmap.
- Sixteen channels: past fifteen pitched parts, channels are shared.
