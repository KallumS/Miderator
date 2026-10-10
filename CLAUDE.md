# Miderator

A JUCE app for macOS (Apple silicon only; Windows later) that shows music on
a piano roll, as Cubase and Ableton do, and whose music mostly comes from
the family's generators: Good Idea (the main one, drums included), Midi
Suggester and Midi Variator under Generate, Starting Blocks as its own Blocks
toolbox (0018, 0024), and Midi Catalogue loaded but not listed (0017).

**Miderator is Noterator with a piano roll (0026).** It was copied from
KallumS/Noterator and only what was about notation was replaced. The music
underneath - score, edits, instruments, templates, detection, AutoCC,
playback, files, MusicXML, generators - is **Noterator's files, byte for
byte**, listed in [`docs/SHARED.md`](docs/SHARED.md). Fix those in Noterator
and copy them across with `tools/sync_from_noterator.sh`.

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
  **(0006)** is `docs/decisions/0006-instruments-carry-their-context.md`.
  0001-0025 came from Noterator; 0026 on are Miderator's. Not edited to stay
  true: a reversed decision keeps its text and gains a pointer.
- **[`docs/sessions/`](docs/sessions/README.md)** - one log per working
  session: the route, the mistakes, what looked broken and was not.

Write to all three when something changes. A rule here without its reason
gets undone.

## Shape of it

| | |
| --- | --- |
| `Source/Core/` | The music. **No JUCE in here, ever** - it is what makes it testable in seconds. Shared with Noterator, except `Roll.*`. |
| `Source/Core/Score.*` | Parts of notes in ticks (960 a quarter), meters and keys by bar, tempos by tick. |
| `Source/Core/Roll.*` | **Miderator's own.** The grid, drawing, moving, stretching, quantising and velocities as functions, and each instrument's warnings read from the notes (0028, 0030, 0032). |
| `Source/Core/Follow.h` | How the view follows the playhead - scrolling along or a page at a time - and the steady clock that keeps it smooth (0033, 0034). Shared. |
| `Source/Core/Edit.*` | Every change Noterator's editor can make, as a function; Miderator uses most of them. |
| `Source/Core/Engrave.*` | Notes in, a laid-out page out. Never shown here: it only writes MusicXML (0029). |
| `Source/Core/Instruments.*` | The one table of what each instrument is (0006). |
| `Source/Core/Templates.*` | The ensembles a new song starts from, in score order (0022). |
| `Source/Core/Spelling.*`, `ScaleModel.h` | Keys, spelling, signatures. `ScaleModel.h` is ScaleView's, **unchanged**. |
| `Source/Core/Detect.*` | Chords and keys along the score (0014). |
| `Source/Core/AutoCC.*` | AutoCC's curves, computed for a whole part (0013). |
| `Source/Core/Perform.*`, `MidiFile.*`, `ScoreFile.*` | The score as MIDI events (channels in banks, 0023), as a .mid, as a project (JSON; `.miderator` and `.noterator` are the same format). |
| `Source/Core/Xml.*`, `MusicXml.*` | MusicXML in and out with our own small XML reader/writer (0021). |
| `Source/Engines/LuaEngine.*` | The embedded Lua host. Speaks only to the adapters. No JUCE. |
| `Source/Engines/Generators.*` | A generator's context, fitting to an instrument, placing a result (0011). |
| `Source/Engines/Orchestrate.*` | A result shared across chosen parts by instrument: sections, tune, bass and its octave, voice-led inner parts (0040). Shared. |
| `Engines/<app>/` | The family's engines, **copied unchanged** (0003), embedded at build time. |
| `Engines/adapters/` | The only Lua written for the apps: one adapter per engine, protocol in `common.lua`. Shared with Noterator. |
| `Source/App/Controller.*` | Owns the score, undo, selection, caret, grid and tools; every window piece asks it. |
| `Source/App/Workspace.*` | The tracks above, the roll below, the divider and the shared scroll bar (0027); reads the playhead once a frame for both views and follows it (0034). |
| `Source/App/Timeline.*` | The time axis both share, and the drawing along it (grid, ruler, small notes). |
| `Source/App/ArrangeView.*` | The tracks: names, mute/solo, notes small, the Scale and Chords lanes, choosing bars (0019). |
| `Source/App/PianoRollView.*` | One part's piano roll: keys, range bracket, notes, the mouse rules (0028), the velocity lane (0030). |
| `Source/App/AudioEngine.*`, `Exporter.*` | Playback through a rack of synths, one per 16 channels (0023), previews, MIDI input; MIDI, MusicXML and WAV export (0007). |
| `Source/App/GeneratorPanel.*`, `BlocksPanel.*`, `SettingsList.*` | The Generate tab (0017), the Blocks toolbox (0018, 0024) with a small piano-roll preview, and the settings menus both draw from an adapter. |
| `Source/App/Panels.*`, `MainComponent.*`, `Theme.*` | Toolbar, Parts tab, status line, keys and menus (File holds New, Open, Save, Export, Undo, the song's settings, Sound, Step input and Light, 0037-0039), colours (both looks in `theme::rollColours`). |
| `Tests/Test*.cpp` | Core tests (100), no JUCE; `TestRoll.cpp` is Miderator's own. `Tests/TestApp.cpp` is the JUCE-side test (22). |
| `tools/` | `RenderRoll.cpp` (MideratorRender, PNGs of the real views), `try_generators.lua`, `sync_engines.sh`, `sync_from_noterator.sh`. `build-mac.command`, at the top, builds the app on the user's Mac (0045). |

## Working in it

```sh
# core and its tests: seconds
cmake -B build-core -G Ninja -DMIDERATOR_BUILD_APP=OFF && cmake --build build-core && ./build-core/MideratorTests
# the adapters, outside the app
lua5.4 tools/try_generators.lua
# everything (downloads JUCE 8.0.15 on first configure). Debug builds in a
# couple of minutes; Release links with LTO and takes much longer.
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build
./build/MideratorAppTests_artefacts/Debug/MideratorAppTests
./build/MideratorRender_artefacts/Debug/MideratorRender demo out.png [light] [part number]
# is the shared code still Noterator's?
tools/sync_from_noterator.sh ../Noterator --check
```

- **Look at a render after any drawing change** (0010). `MideratorRender demo`
  fills a quartet with a Good Idea measure and draws a few notes the violin
  cannot play, through the app's own `Workspace`; any .mid, MusicXML or
  project file works too.
- **Drive the real app headless** for anything with the mouse: start `Xvfb :99`,
  run the app with `DISPLAY=:99`, click and drag with `xdotool` (`mousedown`,
  several `mousemove`s, `mouseup` - one jump is not a drag), screenshot with
  `import -window root`. A 1/16 note at the default zoom is 7 pixels wide:
  aim at its first pixels to move it, its last to stretch it. Popup menus
  open with the *current* item over the box. There is no sound device in the
  container, but one can be faked for playback: `apt-get install pulseaudio
  libasound2-plugins`, start `pulseaudio -D --exit-idle-time=-1 -n
  --load="module-null-sink sink_name=silent" --load=module-native-protocol-unix`,
  and put `pcm.!default { type pulse }` in `~/.asoundrc`; the playhead then
  moves in real time, silently. Record with ffmpeg's `x11grab` to judge
  motion, from a `RelWithDebInfo` build (Debug draws too slowly to tell).
  Don't `pkill -f` a name that is in your own command line.
- **The Mac app is built and tested on CI** (0012); the user can also build
  it on their own Mac with `build-mac.command` (0045), which runs no tests. After pushing, check
  the run (GitHub MCP `actions_list` / `get_job_logs`). The Mac test log
  should say "rendering through Apple General MIDI (built into macOS)".
- **Commit and push early.** A session's container can restart.
- When fixing a bug, have the test fail on the old code first.
- Build with no warnings: JUCE's recommended flags are strict (`-Wswitch-enum`
  wants every enum case, `-Wfloat-equal`, sign conversions).

## Rules that are easy to break

- **Never edit a shared file here** (0026, `docs/SHARED.md`): fix it in
  Noterator, copy it with `tools/sync_from_noterator.sh`, record the commit.
  Anything the piano roll needs from the music goes in `Roll.*`.
- **Nothing about the view is stored** (0002). If something needs remembering
  about a note, it is a field on `Note`, made in Noterator.
- **A drag is one undo step.** The views show a drag (`dragBy`,
  `velocityEdits`...) and ask the controller once, on mouse up. Never edit the
  score on every mouse move.
- **The tracks and the roll share one time axis** (`Timeline`): both keep
  `Timeline::left` at the left and `Timeline::right` at the right, so bars
  line up. A new view along time does the same.
- **Grid lines count from each bar line** (`roll::snap`), never from tick 0.
- **Never edit `Engines/<app>/*.lua`.** Fix it in that app's repository and copy
  it across (`tools/sync_engines.sh`), in Noterator first.
- **The audio thread does not allocate or lock** beyond try-locks: the
  sequence is swapped in whole (`std::atomic_store`), messages from the window
  queue under a lock the audio thread only tries.
- **An Audio Unit is created on the message thread** - including for audio
  export, which makes its synth before starting its thread.
- **Pointers into `score.parts` die when a part is added.** Hold ids.
- **A result never overwrites the music it came from** (0011), except in bars
  the user chose (0019). **Where a result lands is decided in
  `Controller::place`**: chosen bars (one part takes all of it, several share
  it), a block at the caret, the selection, and with nothing chosen every
  part from the caret's bar (0041) - or only the parts chosen by name
  (0050); a single line goes to one part (`lineTarget`: the first chosen),
  and chosen bars are filled once, never repeated (0042), block by block (0050).
  No part is ever added for a result.
- **Every generated line is fitted to the part it lands in** (0036,
  `fitToPolyphony`, shared): no more notes at once than the instrument
  plays. New ways of placing a result keep `InsertOptions::fitPolyphony`
  on; only Blocks turn it off. An audition is never fitted: every note on
  a piano, drums on a kit (0043, `addAudition`).
- **Who plays what across chosen parts is decided in `orchestrate`** (0040,
  shared): by section and by each instrument's best register, never by
  score order.
- **Selecting notes clears the chosen bars** (`select`, `selectAll`, dragging,
  pasting, drawing). Code that sets `selection` directly must decide whether
  `range` still holds.
- **A Blocks chord's name comes from its recorded root** (0046,
  `Score::chordRoots`, `nameFromRoot`): the lane reads notes only where
  nothing recorded how they were made. New data about a span of the score
  goes on `Score`, is saved by `ScoreFile` and moved by `insertBars` and
  `deleteBars`, or it drifts.
- **General MIDI gets CC7 and CC11 only** from AutoCC; a .mid gets all four (0008).
- **A part's channel is bank x 16 + channel** (0023). Never assume 0-15.
- **Menu ids come in ranges** (`MainComponent.cpp`): every range check names
  its own end, or it swallows the next range's items (0038).
- Letters in shortcuts arrive in either case: compare them upper-cased.
- **No references into temporaries in tests**: `f().front().x` inside
  `CHECK_EQ` dangles. It passed with GCC and failed on the Mac.
- Instrument ids are stored in files: never rename one.

## Colour

The house scheme (Good Idea's `docs/COLOUR.md`), in `Theme.*` (0015): a dark
cool-grey ground, light grey controls with **dark ink on every button and
tab**, one yellow (`#FFF200`) for what is on - a chosen button, selected
notes, chosen bars, the caret in step input. Every grey has R < G < B. Red
`#D2483F` is warnings only. The tracks and the roll are dark by default and
light with **Light** (0031, remembered); the chrome stays dark.

## Where it stands

First version, built 2026-10-09: tracks and piano roll, every Noterator
feature but notation, velocity lane, quantise, step input, light and dark,
and Follow - the view scrolls smoothly with the music as it plays, or turns
a page at a time (0033, 0034) - merged into `main` in both repositories,
not yet tried by the user on their Mac. Then Space from bar 1, Shift+Space
from the caret, and buttons to the start and the end (0035), merged into `main` in both, with generated music fitted to what each
instrument can play (0036), a File button (0037) that also holds what
was the Score tab, Undo, Sound, Step input and Light (0038, 0039), and
generated music orchestrated across the chosen parts, or every part
with nothing chosen, a single line to one part (0040-0042); then auditions that
play every note on a piano (0043), a name click that lets go of bars chosen
elsewhere (0044), `build-mac.command` (0045), Blocks chords named in the
Chords lane from their own root (0046) and each chord offering the
inversions it has (0047), since merged into `main`; then Escape letting
go of everything, the caret's part too, so an idea goes to every part
(0048), and Undo bringing back the ideas a Generate replaced (0049), and several parts
chosen with Cmd, Shift and Cmd+A, and blocks of bars with Cmd (0050), no More button (0051), and Vary Notes on several parts going
after the music, across them (0052), on
the branch `claude/adoring-feynman-bihxp2`. Not built yet, roughly in the order
the user is likely to want them: drawable CC lanes; articulations; VST3/CLAP
instruments and SoundFonts; real-time recording; Windows. Known rough edges
are in the latest session log's "Not done yet".
