# Prompt for the next session

Copy everything in the box below into a new Claude Code session on the
Miderator repository. Replace the line under "This session" with what you
want done; if you leave it, the session starts on the first roadmap item.

```text
We're continuing work on Miderator (KallumS/Miderator): a macOS app (JUCE 8,
C++17, Apple silicon only) that shows music on a piano roll, as Cubase and
Ableton do. It is my notation app Noterator (KallumS/Noterator) with the
notation replaced by a piano roll; everything underneath is shared with it.
Most of its music comes from my generators, which run unchanged inside it
through embedded Lua:
- Generate tab: Generate Notes (my Good Idea engine - motifs, phrases,
  measures and drum grooves), Suggest Notes (Midi Suggester), Vary Notes
  (Midi Variator). In the code they keep their engine names.
- Blocks tab: Starting Blocks as a toolbox of chords, arpeggios, runs and
  intervals, placed at the caret one after another.
- A Blocks chord keeps the name it was made with in the Chords lane, even
  where its notes are another chord's too (C E G A: I6 or vi7 inverted);
  each chord offers the inversions it has - a triad two, a seventh three,
  up to six for a thirteenth.
- Tracks along the top (choose bars there and Generate Notes fills them),
  one part's piano roll below with a velocity lane; select, drag, stretch,
  Alt-copy, draw, quantise, step input from a MIDI keyboard; dark by
  default, with a Light option.
- Follow: while it plays, the view scrolls smoothly with the music, or turns
  a page at a time (Play menu).
- Space plays from bar 1, Shift+Space and Play from the caret; buttons
  beside Play go to the start and the end (Home, End).
- Templates up to a full orchestra and big band through Apple's built-in
  General MIDI synth; MIDI, MusicXML and WAV out; MIDI and MusicXML in.

I'm not technical: explain things in plain words, show me screenshots of what
changed, and make sure each change reaches me as the downloadable Mac app from
GitHub Actions.

Before doing anything:
1. If the branch ccr-ac8da7d9-3sbl5x (the latest work) is not yet merged
   into main, start your branch from it; if it is, start from main.
   Noterator's main (or its branch of the same name) holds the matching
   shared code (docs/SHARED.md).
2. Read CLAUDE.md, then docs/ARCHITECTURE.md (every decision on one page),
   docs/SHARED.md, and the latest log in docs/sessions/. Follow their rules -
   especially: the files shared with Noterator are never edited here (fix
   them in Noterator, copy with tools/sync_from_noterator.sh), never edit the
   vendored engines in Engines/<app>/, no JUCE in Source/Core, a drag is one
   undo step, where a generated result lands is decided only in
   Controller::place, look at a render after any drawing change, and commit
   and push early.
3. Build and run the tests (core, app tests, tools/try_generators.lua), and
   render the demo (MideratorRender demo out.png), to confirm everything is
   green before changing anything. For anything with the mouse, run the app
   headless under Xvfb and look at screenshots; for playback, CLAUDE.md says
   how to fake a sound card so the playhead moves.

This session:
<what I want next - for example: "drawable CC lanes", "articulations",
"VST3 instruments per part", "record from my MIDI keyboard", or a list of
things I noticed while testing>

The roadmap, in the order I'm most likely to want it: drawable CC lanes
under the roll, seeded by AutoCC; articulations and articulation switching
for sample libraries; VST3 and CLAP instruments per part and SoundFonts
through the Mac's General MIDI synth; real-time MIDI recording; Windows.
Known rough edges are in the latest session log's "Not done yet".

When you finish: write the session log in docs/sessions/ (and its line in
docs/sessions/README.md), add a decision record in docs/decisions/ (with its
line in docs/decisions/README.md and docs/ARCHITECTURE.md) for any choice
someone could reasonably make the other way - numbered after the highest in
either Miderator or Noterator, which share one sequence - update CLAUDE.md and README.md
if what the app does or a rule changed, rewrite docs/NEXT-SESSION.md, push,
check that the GitHub Actions run is green on both Linux and macOS (the Mac
test log should say it rendered through Apple General MIDI), and tell me
where to download the new app.
```

## Things worth knowing before you paste it

- A fix to the generators, the instruments, templates, files or playback
  belongs in Noterator first; the session will ask for that repository if it
  needs it, and copy the change across.
- The latest work is on the branch `ccr-ac8da7d9-3sbl5x`, not yet merged,
  in eight repositories: Noterator, Miderator, ScaleView-for-Reaper (ScaleView
  Pro takes Starting Blocks as its chord dictionary), ScaleView (the plugin),
  Midi-Suggester, Midi-Variator (the same chord reader), Starting-Blocks and
  Starting-Blocks-Notation (each chord's own inversions). Merge them all; to
  put them on the main branches, ask a session to open the pull requests.
- ScaleView Pro, Midi Suggester and Midi Variator then need a new ReaPack
  release for REAPER users to get the new chord names; nothing is published
  yet.

