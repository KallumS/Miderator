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

## Then: Follow scrolls smoothly (0034)

Asked for the view to move with the playhead instead of flipping pages.
Made in Noterator (its log `2026-10-09-smooth-follow.md` has the route) and
copied here: the playhead walks in to a third of the way across, then the
music moves under it; pages stay a choice in the Play menu. Here, the two
views' timers became one 60 Hz timer in `Workspace`, so the tracks and the
roll get the same playhead each frame. Playback was seen in the container
for the first time, through a silent PulseAudio sink: at the default zoom
the music moves about a pixel a frame (measured from a 60 fps recording:
mostly 1, sometimes 0 or 2 where Xvfb's frames fall), and in page mode it
turns a page twice in 14 seconds, as it should. 82 core tests.

## Then: start, end, and Space from bar 1 (0035)

Both repositories' branches had been merged into `main`; the branch was
started again from `main`. Space now plays from bar 1 and Shift+Space (and
Play) from the caret; |◀ and ▶| beside Play (Home, End) go to the start and
to the bar line after the last note. The same change was made in
Noterator - none of it touches the shared files, so nothing was synced
(`--check`: 0 differ). An app test covers the controller (13 now). Tried in
the window through the silent sound card: End showed bars 20-25 with the
caret at the end, Home went back, a click in bar 4 then Shift+Space played
from there, Space from bar 1.

## Then: one note at a time, and a File button (0036, 0037)

Chords were reaching one-note instruments from Generate Notes. Measured and
fixed in Noterator (its `2026-10-09-transport.md` has the route) and copied
here: `fitToPolyphony` in `Generators.*` fits every generated line to the
part it lands in - the top notes kept, the bottom for a bass, held notes cut
to the next, drums alone - and Blocks go in as they are. The same
`Controller::place` and File-button changes as Noterator's, made with the
same scripts. Tried in the window: a Phrase of chords (19 notes) into
Violin I went in as its top line, five single notes, and the status line
said "Violin I: top notes only" - at first running into the middle of the
status bar, so the left message now stops short of it with an ellipsis, in
both apps. 86 core tests, 14 app tests.

## Then: the Score tab into File (0038)

The user asked for the Score tab's contents to go under File too. They are
now a "This song" section in the File menu (and the Mac's File menu): title
and composer and tempo in small boxes, time signature and key as sub-menus
ticked at the caret's bar, bars, and sound; the tab and `ScorePanel` are
gone, and "use the key it hears" is `Controller::useHeardKey`, with an app
test (15 now). Made with one script in both apps. Tried every item in the
window - and the first try of Tempo did nothing but say "Grid 1 bar": the
grid range check in `menuItemSelected` took every id from 300 to 999, so
the new items at 400 were read as grid sizes past the end of the list.
Bounded to the grid's own choices; Noterator has no grid range and was
never affected. After that: tempo 132, 6/8, E Mixolydian, a title and
composer, four more bars, each shown back in the menu.
