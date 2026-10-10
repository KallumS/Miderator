# Prompt for the next session

Copy everything in the box below into a new Claude Code session. It covers
both apps, Noterator and Miderator, because most changes go into both; start
the session on either repository - it will add the other. Replace the lines
under "This session" with what you want done; if you leave them, the session
starts by merging the waiting branch and then the first roadmap item.

```text
We're continuing work on my two music apps, which share their music code:

- Noterator (KallumS/Noterator): a macOS notation app (JUCE 8, C++17, Apple
  silicon only). The score is MIDI; the notation is engraved from it.
- Miderator (KallumS/Miderator): Noterator with the notation replaced by a
  piano roll, as Cubase and Ableton do - tracks along the top, one part's
  roll below with a velocity lane. Everything underneath is Noterator's
  files, byte for byte (Miderator's docs/SHARED.md lists them).

Most of their music comes from my generators, running unchanged inside them
through embedded Lua: the Generate tab has Generate Notes (my Good Idea
engine), Suggest Notes (Midi Suggester) and Vary Notes (Midi Variator); the
Blocks tab is Starting Blocks as a toolbox of chords, arpeggios, runs and
intervals. In the code the generators keep their engine names.

What the apps already do, in short: templates up to a full orchestra and big
band through Apple's built-in General MIDI synth; generated music shared out
across the instruments by section and register, a single melody to one
instrument; chosen bars filled once; ideas auditioned on a piano before
Insert; Follow (the view scrolls with the music); Space plays from bar 1,
Shift+Space from the cursor; MIDI, MusicXML and WAV out, MIDI and MusicXML in.
Added most recently (decisions 0048-0051, both apps):
- Escape lets go of everything - notes, bars and the highlighted instrument -
  so the next idea goes to every instrument, a single melody to the top one.
- Every Generate is a step in Undo: Cmd+Z brings back the ideas a Generate
  replaced, with the same one chosen.
- Cmd-click instrument names to choose several, Shift-click for every one
  between, Cmd+A for every instrument (again: every note too); an idea goes
  to the instruments chosen, a single melody to the first chosen. Cmd-click
  bars to add them to the chosen bars; each block is filled on its own.
- The Generate tab's More button is gone: Generate again does the same.

I'm not technical: explain things in plain words, show me screenshots of what
changed, and make sure each change reaches me as the downloadable Mac app
(the .dmg from GitHub Actions) for both apps.

Before doing anything:
1. Add whichever of the two repositories this session does not have, and
   clone both side by side (Miderator's tools expect ../Noterator or
   ../noterator).
2. The latest work is on the branch claude/adoring-feynman-bihxp2 in both
   repositories. If it is not yet merged into main, open a pull request for
   it in each (Noterator first) and ask me before merging; start new work
   from that branch, or from main once it is merged.
3. Read each repository's CLAUDE.md, then docs/ARCHITECTURE.md (every
   decision on one page), Miderator's docs/SHARED.md, and the latest logs in
   docs/sessions/. Follow their rules - especially: a file Miderator shares
   with Noterator is changed in Noterator first and copied across with
   Miderator's tools/sync_from_noterator.sh (recording the commit in
   docs/SHARED.md); never edit the vendored engines in Engines/<app>/; no
   JUCE in Source/Core; a drag is one undo step; where a generated result
   lands is decided only in Controller::place; look at a render or a
   screenshot after any drawing change; commit and push early.
4. Build both and run every test (core tests, app tests,
   tools/try_generators.lua) to confirm all is green before changing
   anything. For anything with the mouse, run the apps headless under Xvfb
   with xdotool (Cmd is Ctrl there) and look at screenshots; CLAUDE.md says
   how to fake a sound card for playback.

This session:
<what I want next - for example "drawable CC lanes in Miderator",
"articulations", "VST3 instruments per part", "record from my MIDI
keyboard", or a list of things I noticed while testing>

The roadmap, in the order I'm most likely to want it. Miderator: drawable CC
lanes under the roll, seeded by AutoCC; articulations and articulation
switching for sample libraries; VST3 and CLAP instruments per part, and
SoundFonts; real-time MIDI recording; Windows. Noterator: a page view;
dynamics, articulations and slurs; then the same instruments, recording and
Windows. Known rough edges are in the latest session log's "Not done yet"
(docs/sessions/2026-10-10-choosing-parts.md in either repository).

When you finish: in each repository you changed, write the session log in
docs/sessions/ (and its line in docs/sessions/README.md), add a decision
record in docs/decisions/ (with its line in docs/decisions/README.md and
docs/ARCHITECTURE.md) for any choice someone could reasonably make the other
way - numbered after the highest in either app, which share one sequence
(0051 is the latest) - update CLAUDE.md and README.md if what the app does or
a rule changed, rewrite docs/NEXT-SESSION.md, push, check that the GitHub
Actions runs are green on both Linux and macOS (the Mac test log should say
it rendered through Apple General MIDI), and tell me where to download both
new apps.
```

## Things worth knowing before you paste it

- **Waiting to be merged:** the branch `claude/adoring-feynman-bihxp2` in
  Noterator and Miderator holds Escape (0048), Undo for generated ideas
  (0049), the instrument and bar shortcuts (0050) and no More button (0051). Its last builds passed
  on GitHub (Noterator run 65, Miderator run 42). No pull request is open for
  it yet: ask a session to open them, or open them on GitHub yourself.
- **Already merged:** the earlier branch `ccr-ac8da7d9-3sbl5x` (up to 0047)
  is in main in both apps. The same branch name also exists in
  ScaleView-for-Reaper, ScaleView, Midi-Suggester, Midi-Variator,
  Starting-Blocks and Starting-Blocks-Notation; check those were merged too,
  and that ScaleView Pro, Midi Suggester and Midi Variator got a ReaPack
  release, if REAPER users should have the new chord names.
- A fix to the generators, instruments, templates, files or playback goes
  into Noterator first, then is copied to Miderator.
