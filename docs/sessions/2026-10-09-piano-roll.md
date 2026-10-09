# 2026-10-09 - Miderator: Noterator on a piano roll

## Asked

A version of Noterator that shows MIDI on a piano roll, as Cubase and
Ableton do, with everything else the same. Asked before starting, the user
chose:

- **Layout:** tracks along the top with each part's notes small, and a big
  piano roll below for the track chosen (0027).
- **Writing notes:** DAW-style only - draw, move and stretch with the mouse,
  play in from a MIDI keyboard; no notation keys (0028).
- **MusicXML:** keep import and export (0029).
- **Extras:** a velocity lane, quantise, and dark by default (0030, 0031).
  Drawable CC lanes later.

## Built

- Noterator copied in unchanged as the first commit, so the second shows
  exactly what Miderator changes.
- `Roll.*` in the core: the grid (counted from each bar line), drawing,
  moving with pitch, copying, stretching either end, velocities, quantise,
  and the instruments' warnings read from the notes (0032). 8 tests.
- The window: `ArrangeView` (tracks, lanes, choosing bars), `PianoRollView`
  (keys, range bracket, notes, the mouse, velocity lane), `Workspace` and
  `Timeline` (one time axis, one scroll bar, a divider). The toolbar's note
  values and voices became Select/Draw, Grid, Triplet, Snap, Quantise, Step
  input and Light. The Blocks preview is a small piano roll.
- The controller lost note input, voices and transposed scores; it gained
  the grid, the tools, drag/stretch/copy/quantise/duplicate/velocity, and
  step input on the grid. 4 new app tests; the old ones draw notes instead
  of typing letters.
- `MideratorRender` replaces `NoteratorRender`: it builds a `Workspace` with
  no window and snapshots it, so the PNG is the app's own drawing.
- Bravura, the stave renderer and the page are gone; projects are
  `.miderator` (Noterator's format); a new icon; CI builds `Miderator-macOS`.
- `tools/sync_from_noterator.sh` and `docs/SHARED.md` (0026): the shared
  files are checked to be byte for byte Noterator's.

## The route

- **Baseline first.** Noterator built and passed its 66 core and 8 app
  tests in the container before anything changed.
- **The core before the window.** `Roll.*` and its tests passed first time;
  then the views, then the controller, then one build.
- **Rendering through the real components** caught two things straight away:
  the black keys vanished on the dark keyboard (their colour was the
  ground's, and the white part of their row was never drawn), and a Good
  Idea Motif only ever fills the top part - the demo now asks for a Measure,
  which brings chords and bass.
- **Driving the app under Xvfb** caught the third: a 1/16 note at the
  default zoom is seven pixels wide and a click on its last pixel missed it,
  starting a lasso instead of a stretch. Notes can now be caught three
  pixels either side and at least eight tall.
- Tried in the window: double-click to draw, the Draw tool with a drag, a
  note moved a beat and a tone, a short note stretched, bars 1-4 of a
  quartet chosen and filled with a Good Idea Measure, a velocity ramp
  painted, a lasso, the Blocks tab, the light look.

## What looked wrong and was not

- **The bar numbers' caret moved when a note was clicked.** It should: a
  clicked note puts the caret at its start, so Space plays from there.
- **The chosen bars stay tinted after Insert.** As in Noterator (0019), so
  another idea can go straight into them.

## Mistakes

- `pkill -f Miderator_artefacts` killed the shell running it: its own
  command line matched. Kill by `pgrep -x`.
- CI still named Noterator's targets for one push; fixed in the next commit,
  before that run could report.

## Not done yet

- Not yet tried on a Mac by the user; no sound in the container, so
  playback, the playhead and the keyboard lighting up are untested here
  (the Mac CI renders audio through Apple's synth).
- One part in the roll at a time; no editing several parts' notes together
  in the roll (they can be selected in the tracks, moved with the arrows).
- No resizing of track heights or vertical zoom of the tracks.
- Drawable CC lanes, articulations, VST3/CLAP, real-time recording, Windows.
- Noterator's own rough edges carry over: chosen bars are cleared by most
  edits and not brought back by undo; Suggester ignores chosen bars beyond
  reading their notes; templates are fixed.

## Then: Follow (0033)

Asked for an auto-scroll option in both apps. Both already turned the page
at the right edge, always; now `Follow.h`, written in Noterator and copied
here with `tools/sync_from_noterator.sh` (its first use), turns it a margin
before the edge, goes to a playhead that is off the screen, and is switched
by **Follow** beside Play, the Play menu, or **F** - on by default,
remembered. Three shared core tests (77 now). The page turning itself has
not been seen: there is no sound card in the container, so the playhead
never moves here.
