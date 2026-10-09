# Miderator's architecture, and the decisions behind it

One page for the whole shape of the app and every big decision in it. Each
decision has its own record in [`decisions/`](decisions/README.md), with the
reasoning and what it costs; this page is the map.

Miderator is Noterator with a piano roll (0026): everything under `Source/Core`
but `Roll.*`, the engines and the generator bridge are Noterator's files,
shared byte for byte ([`SHARED.md`](SHARED.md)). 0001-0025 are Noterator's
decisions; 0026 on are Miderator's.

## The shape

```
                 Engines/<app>/*.lua  (the family's engines, unchanged)
                 Engines/adapters/*.lua  (one adapter each)
                          |
                          v
  Source/Engines/   LuaEngine  ---  Generators (context in, results placed)
                          |
                          v
  Source/Core/      Score  <-- Edit, Roll (every change)     Instruments (one table), Templates
   (no JUCE)          |                                        |
                      +--> Roll::check --> what each instrument cannot play
                      +--> Detect  --> chord and key lanes   <-+
                      +--> Perform --> MIDI events <-- AutoCC
                      +--> MidiFile, ScoreFile, MusicXml (via Engrave, unseen)
                          |
                          v
  Source/App/       Controller (owns the score, undo, selection, caret, grid, tools)
   (JUCE)             |-- Workspace: ArrangeView (tracks) over PianoRollView, one Timeline
                      |-- Toolbar; Generate, Blocks, Parts panels; status line
                      |-- AudioEngine (a rack of Apple GM Audio Units / built-in synths)
                      `-- Exporter (MIDI, WAV, MusicXML)
```

- **The score is MIDI.** Parts of notes in ticks (960 a quarter), sounding
  pitch. A piano roll draws exactly that.
- **The view is derived.** Nothing about the tracks or the roll is stored;
  the grid, the tool and the zoom are choices in the window.
- **The music core has no JUCE** (`Source/Core`, `Source/Engines`), so 86 tests
  build and run in seconds. `MideratorAppTests` covers the JUCE side.
- **One controller.** Every window piece reads the `Controller` and asks it for
  changes; it keeps undo, re-checks the instruments, re-detects and re-sends
  to playback. A drag is shown by the view and asked for once.

## The decisions

### What the app is
| | |
| --- | --- |
| [0026](decisions/0026-miderator-is-noterator-with-a-piano-roll.md) | Noterator with a piano roll, in its own repository; the music code is Noterator's files, shared byte for byte. |
| [0001](decisions/0001-a-juce-app-not-a-reascript.md) | A standalone JUCE 8 app, not a ReaScript: it owns its window, files and audio, and can host instruments. |
| [0012](decisions/0012-apple-silicon-only-built-by-ci.md) | Apple silicon only; GitHub Actions builds, tests and packages the Mac app, because nothing here can. |

### The music
| | |
| --- | --- |
| [0002](decisions/0002-the-score-is-midi-shaped.md) | The score is MIDI; notation is worked out from it every time. Generators' output goes in unconverted. |
| [0005](decisions/0005-edits-are-functions-undo-is-a-copy.md) | Every edit is a pure function; undo keeps a copy of the score. Note input overwrites. |
| [0006](decisions/0006-instruments-carry-their-context.md) | One instrument table (Midi Catalogue's numbers plus clefs, transposition, sound, AutoCC shape) read by everything. |
| [0022](decisions/0022-templates-in-score-order.md) | Templates (sections, full orchestra, big band) are data in the core, in score order, numbered where repeated. |

### The generators
| | |
| --- | --- |
| [0003](decisions/0003-run-the-lua-engines-unchanged.md) | The five Lua engines run unchanged through embedded Lua 5.4, each behind a small adapter. |
| [0011](decisions/0011-selection-results-go-beside-or-after.md) | Results made from a selection go beside it (Suggester) or after it (Variator), never over it. |
| [0017](decisions/0017-catalogue-leaves-generate.md) | Generate lists Good Idea, Suggester and Variator; the Catalogue stays loaded but unlisted. |
| [0018](decisions/0018-starting-blocks-is-a-toolbox.md) | Starting Blocks is a Blocks tab: kind, degree buttons, preview; a block goes at the caret and the caret moves on. |
| [0024](decisions/0024-blocks-without-drums-and-bass.md) | Blocks offers chords, arpeggios, runs and intervals; drums and bass are Good Idea's. |
| [0025](decisions/0025-generators-named-for-what-they-do.md) | In the app: Generate Notes (Good Idea), Suggest Notes (Suggester), Vary Notes (Variator); code keeps the engines' names. |
| [0019](decisions/0019-bars-can-be-chosen-and-filled.md) | Bars chosen by dragging; Good Idea fills them exactly, tune on top, bass below, chords between; Variator replaces them. |

### The screen
| | |
| --- | --- |
| [0027](decisions/0027-tracks-above-a-piano-roll-below.md) | The tracks above, one part's piano roll below, sharing one proportional timeline. Replaces 0004 here. |
| [0028](decisions/0028-notes-drawn-with-the-mouse-on-a-grid.md) | Select, drag, stretch, copy, draw with the mouse on a grid counted from each bar line; step input from a MIDI keyboard. Replaces 0016 here. |
| [0030](decisions/0030-a-velocity-lane-and-quantise.md) | A velocity lane under the roll; quantise and duplicate. |
| [0032](decisions/0032-warnings-read-from-the-notes.md) | What an instrument cannot play is read from the notes, shown in the roll and the tracks. |
| [0033](decisions/0033-follow-the-playhead-switchable.md) | While it plays, the view follows the playhead; Follow (F) switches it, remembered. Shared with Noterator. |
| [0036](decisions/0036-one-note-at-a-time-for-one-note-instruments.md) | Every generated line is fitted to the part it lands in: no more notes at once than the instrument plays - the top line kept, or the bottom for a bass. Blocks go in as they are. Shared with Noterator. |
| [0037](decisions/0037-a-file-menu-button.md) | New, Open, Save and Export under one File button. Both apps. |
| [0038](decisions/0038-the-score-tab-moves-into-file.md) | The Score tab's settings move into the File menu; the tab is gone. Both apps. |
| [0039](decisions/0039-undo-sound-input-and-look-into-file.md) | Undo, Redo, Sound, the input mode and the look move into the File menu. Both apps. |
| [0040](decisions/0040-generated-music-orchestrated-across-chosen-parts.md) | Generated music shared across the chosen parts by instrument: each section the whole harmony, tune on top, bass below doubled an octave down, chord between, voice-led (`Orchestrate.*`). Both apps. |
| [0041](decisions/0041-where-generated-music-goes.md) | Nothing chosen: shared across every part; bars of one part: all of it there; bars of several: shared across them. No part is ever added. Both apps. |
| [0042](decisions/0042-a-single-line-to-one-part-and-a-span-filled-once.md) | A single line (melody, motif) goes to one part, never shared; chosen bars are filled once, cut or left empty, never repeated. Both apps. |
| [0043](decisions/0043-an-audition-plays-every-note-on-a-piano.md) | An audition is the result itself: every note on a piano, drums on a kit, heard where it would go - nothing fitted. |
| [0035](decisions/0035-space-plays-from-the-start.md) | Space plays from bar 1, Shift+Space and Play from the caret; |◀ and ▶| (Home, End) go to the start and the end of the music. Both apps. |
| [0034](decisions/0034-follow-scrolls-smoothly.md) | Following scrolls smoothly by default, the playhead a third of the way across, on a steady clock read once a frame for both views; turning pages is a choice in the Play menu. |
| [0010](decisions/0010-verify-by-rendering.md) | Drawing changes are checked by rendering them to PNG (`MideratorRender`). |
| [0014](decisions/0014-chords-and-keys-read-from-the-score.md) | Chord lane: ScaleView's names on beat-by-beat segments, only real harmony named. Key lane: Suggester's finder, the signature breaking ties. |
| [0015](decisions/0015-the-house-scheme-and-a-dark-page.md) | The family's colour scheme. |
| [0031](decisions/0031-dark-by-default.md) | Dark by default, as a DAW is; Light on request, remembered. Replaces 0020 here. |

Noterator's page decisions - one set of columns in a galley (0004), Bravura
(0009), a light page (0020) - are kept for the record; only 0004's columns
still matter, inside MusicXML export.

### Input and sound
| | |
| --- | --- |
| [0007](decisions/0007-one-performance-and-the-macs-own-orchestra.md) | One performance feeds playback, WAV and .mid; sound from Apple's General MIDI Audio Unit, a built-in synth elsewhere. |
| [0008](decisions/0008-general-midi-gets-two-cc-lanes.md) | General MIDI gets AutoCC's CC7 and CC11; a .mid gets all four lanes. |
| [0023](decisions/0023-a-synth-per-sixteen-channels.md) | A synth per sixteen channels (up to four), so every part of a big score has a channel and a sound of its own. |
| [0013](decisions/0013-autocc-computed-for-a-whole-part.md) | AutoCC's presets and envelope, ported and computed once per part. |

### Files
| | |
| --- | --- |
| [0021](decisions/0021-musicxml-in-and-out.md) | MusicXML import (.musicxml, .xml, .mxl) and export, in the core with its own XML parser; export written from the layout. |
| [0029](decisions/0029-the-engraver-stays-for-musicxml.md) | The engraver stays, unseen, only to write MusicXML; the music font goes. |

## Borrowed from the family, unchanged

| | |
| --- | --- |
| `Engines/good-idea`, `midi-catalogue`, `midi-suggester`, `midi-variator`, `starting-blocks` | The engines, at the commits in [`Engines/VENDORED.md`](../Engines/VENDORED.md) |
| `Source/Core/ScaleModel.h` | ScaleView's scales, spelling and chord naming |
| `Detect.cpp` key finder | Midi Suggester's `detectKey`, weights as they stand |
| `AutoCC.cpp` | AutoCC.jsfx's presets and envelope |
| `Engrave.cpp` rules | Starting Blocks Notation's engraver, ported and grown (for MusicXML) |
| Everything in [`SHARED.md`](SHARED.md) | Noterator, at the commit recorded there |
| `Theme.*` | The house colour scheme |

## Where it goes next

Roughly in order: drawable CC lanes under the roll, seeded by AutoCC;
articulations and articulation switching for sample libraries; VST3 and CLAP
instruments per part and SoundFonts through the Mac's synth; real-time MIDI
recording; Windows.
