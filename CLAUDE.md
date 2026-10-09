# Noterator

A JUCE notation app for macOS (Apple silicon only; Windows later) whose music
mostly comes from the family's generators: Good Idea (the main one, drums
included), Midi Suggester and Midi Variator under Generate, Starting Blocks as
its own Blocks toolbox of chords, arpeggios, runs and intervals (0018, 0024),
and Midi Catalogue loaded but not listed (0017). The score is MIDI;
the notation is engraved from it every time it is drawn.

**Names:** in the app the user sees **Generate Notes** (Good Idea), **Suggest
Notes** (Midi Suggester) and **Vary Notes** (Midi Variator) (0025). Code, ids
and these docs keep the engines' own names; talk to the user in the app's.

The user is not a developer. Explain in plain words, show screenshots of what
changed, and make sure every change reaches them as a downloadable Mac app
(the CI's `.dmg`).

## Where the reasons are

- **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** - the shape of the app and
  every big decision on one page. Start here.
- **[`docs/decisions/`](docs/decisions/README.md)** - one record per choice
  someone could reasonably make the other way. Referred to by number:
  **(0006)** is `docs/decisions/0006-instruments-carry-their-context.md`. Not
  edited to stay true: a reversed decision keeps its text and gains a pointer.
- **[`docs/sessions/`](docs/sessions/README.md)** - one log per working
  session: the route, the mistakes, what looked broken and was not.

Write to all three when something changes. A rule here without its reason
gets undone.

## Shape of it

| | |
| --- | --- |
| `Source/Core/` | The music. **No JUCE in here, ever** - it is what makes it testable in seconds. |
| `Source/Core/Score.*` | Parts of notes in ticks (960 a quarter), meters and keys by bar, tempos by tick. |
| `Source/Core/Engrave.*` | Notes in, a laid-out page out, in staff spaces. Decides; draws nothing. |
| `Source/Core/Edit.*` | Every change the editor can make, as a function. |
| `Source/Core/Instruments.*` | The one table of what each instrument is (0006). |
| `Source/Core/Templates.*` | The ensembles a new score starts from, in score order (0022). |
| `Source/Core/Spelling.*`, `ScaleModel.h` | Keys, spelling, signatures. `ScaleModel.h` is ScaleView's, **unchanged**. |
| `Source/Core/Detect.*` | Chords and keys along the score (0014). |
| `Source/Core/AutoCC.*` | AutoCC's curves, computed for a whole part (0013). |
| `Source/Core/Perform.*`, `MidiFile.*`, `ScoreFile.*` | The score as MIDI events (channels in banks, 0023), as a .mid, as a .noterator (JSON). |
| `Source/Core/Xml.*`, `MusicXml.*` | MusicXML in and out with our own small XML reader/writer (0021). |
| `Source/Engines/LuaEngine.*` | The embedded Lua host. Speaks only to the adapters. No JUCE. |
| `Source/Engines/Generators.*` | A generator's context, fitting to an instrument, placing a result (0011). |
| `Engines/<app>/` | The family's engines, **copied unchanged** (0003), embedded at build time. |
| `Engines/adapters/` | The only Lua written here: one adapter per engine, protocol in `common.lua`. |
| `Source/App/Controller.*` | Owns the score, undo, selection, caret; every window piece asks it. |
| `Source/App/ScoreView.*` | The page: galley, lanes, sticky names, caret, mouse. |
| `Source/App/ScoreRenderer.*` | The ink: a layout into Bravura glyphs. Shared with `tools/RenderScore.cpp`. |
| `Source/App/AudioEngine.*`, `Exporter.*` | Playback through a rack of synths, one per 16 channels (0023), previews, MIDI input; MIDI, MusicXML and WAV export (0007). |
| `Source/App/GeneratorPanel.*`, `BlocksPanel.*`, `SettingsList.*` | The Generate tab (0017), the Blocks toolbox (0018, 0024), and the settings menus both draw from an adapter. |
| `Source/App/Panels.*`, `MainComponent.*`, `Theme.*` | Toolbar, Parts and Score tabs, status line, keys and menus (New is grouped by template), colours. |
| `Tests/Test*.cpp` | Core tests (65), no JUCE. `Tests/TestApp.cpp` is the JUCE-side test (8). |
| `tools/` | `RenderScore.cpp` (PNG renderer), `try_generators.lua`, `sync_engines.sh`. |

## Working in it

```sh
# core and its tests: seconds
cmake -B build-core -G Ninja -DNOTERATOR_BUILD_APP=OFF && cmake --build build-core && ./build-core/NoteratorTests
# the adapters, outside the app
lua5.4 tools/try_generators.lua
# everything (downloads JUCE 8.0.15 on first configure)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/NoteratorAppTests_artefacts/Release/NoteratorAppTests
./build/NoteratorRender_artefacts/Release/NoteratorRender demo out.png 12 light
```

- **Look at a render after any engraving change** (0010). `demo` exercises
  most of the engraver; `generated` drops Catalogue lines into a string trio.
  Crop and enlarge (`convert big.png -crop ...`) to see detail.
- **Drive the real app headless** for anything in the window: start `Xvfb :99`,
  run `build/Noterator_artefacts/Release/Noterator` with `DISPLAY=:99`, click
  and type with `xdotool`, screenshot with `import -window root`. Popup menus
  open with the *current* item over the box, so screenshot a menu before
  clicking into it. There is no sound device in the container.
- **The Mac app is built and tested only on CI** (0012). After pushing, check
  the run (GitHub MCP `actions_list` / `get_job_logs`). The Mac test log
  should say "rendering through Apple General MIDI (built into macOS)".
- **Commit and push early.** A session's container can restart.
- When fixing a bug, have the test fail on the old code first.
- Build with no warnings: JUCE's recommended flags are strict (`-Wswitch-enum`
  wants every enum case, `-Wfloat-equal`, sign conversions).

## Rules that are easy to break

- **The notation is never stored** (0002). If something needs remembering
  about a note, it is a field on `Note`, not a property of the page.
- **Never edit `Engines/<app>/*.lua`.** Fix it in that app's repository and copy
  it across (`tools/sync_engines.sh`), so the copies never drift. Record the
  commit in `Engines/VENDORED.md`. Adapters are fair game.
- **`ScaleModel.h` is the ScaleView plugin's file, and ScaleView Pro is the
  reference.** A chord-naming change is made and measured in ScaleView Pro
  (`ScaleView-for-Reaper/reascripts/ScaleView Pro.lua`), ported to the
  plugin's `Source/ScaleModel.h` with its output diffed against Pro, and
  copied here unchanged. Never the other way round.
- **The audio thread does not allocate or lock** beyond try-locks: the
  sequence is swapped in whole (`std::atomic_store`), messages from the window
  queue under a lock the audio thread only tries.
- **An Audio Unit is created on the message thread** - including for audio
  export, which makes its synth before starting its thread.
- **Pointers into `score.parts` die when a part is added.** Hold ids.
- **A result never overwrites the music it came from** (0011): Suggester's
  lines go into parts silent in those bars, Variator's after the selection.
  A line with chords never goes to a one-note instrument. The exception is
  bars the user chose (0019): Good Idea fills them, Variator replaces them.
- **Where a result lands is decided in `Controller::place`**, in this order:
  chosen bars (0019), a block at the caret (0018), the selection (0011), the
  caret's bar. Change it there, not in the panels.
- **Selecting notes clears the chosen bars** (`select`, `selectAll`,
  `selectNext`, moving, pasting). Code that sets `selection` directly must
  decide whether `range` still holds.
- **The core reads and writes MusicXML itself** (0021, `Xml.*`): no JUCE XML
  in `Source/Core`. Only `.mxl` unzipping is in the app.
- **The engraver must stay near-linear.** Anything per element per measure
  goes through the by-measure buckets; the 200-bar test holds it under a second.
- **General MIDI gets CC7 and CC11 only** from AutoCC; a .mid gets all four (0008).
- **A part's channel is bank x 16 + channel** (0023). Never assume 0-15: files
  take `% 16`, the audio thread routes by `Sequence::Event::bank`, and
  channels 15 and 16 of bank 0 belong to the keyboard and previews. A new
  synth for a bank is made on the message thread, before it is needed.
- Letters in shortcuts arrive in either case: compare them upper-cased.
- **No references into temporaries in tests**: `f().front().x` inside
  `CHECK_EQ` dangles. It passed with GCC and failed on the Mac.
- Instrument ids are stored in files: never rename one.

## Colour

The house scheme (Good Idea's `docs/COLOUR.md`), in `Theme.*` (0015): a dark
cool-grey ground, light grey controls with **dark ink on every button and
tab**, one yellow (`#FFF200`) for what is on - a chosen button, the
selection, the sounding note, the caret in note input. Every grey has
R < G < B. Red `#D2483F` is warnings only. The page is set in ink on its own
paper: black on white by default, white on dark with **Dark page** (0020,
remembered between launches); on white the accent is the yellow shaded down.

## Engraving conventions

Starting Blocks Notation's, ported and grown (see `Engrave.h`): a note is
written as its share of the bar where it was gated short; a duration with a
symbol of its own is written as that symbol; rests show the beat; the key
signature is scored; stems measure from the far head; beams lean a quarter of
the interval, at most a space and a bit; accidentals last to the bar, carry
across a tie and pack into columns; a crossed head's side is decided once,
and a unison of two spellings sits side by side. New here: every staff
shares one set of columns per bar (0004); a sustained low note under moving
music goes to the lower voice; triplets are found per beat by picking the
coarsest grid that fits; a drum hit is written to the next hit, at most a beat.

## Where it stands

Working, and tested by the user on their Mac; since then MusicXML in and out,
chosen bars, the Blocks toolbox, and templates up to full orchestra and big
band. Not built yet, roughly in the order the
user is likely to want them: a page view; dynamics, articulations and slurs;
drawable CC lanes; VST3/CLAP instruments and SoundFonts; real-time
recording; Windows. Known rough edges are in the latest
session log's "Not done yet".
