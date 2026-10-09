# Prompt for the next session

Copy everything in the box below into a new Claude Code session on the
Noterator repository. Replace the line under "This session" with what you
want done; if you leave it, the session starts on the first roadmap item.

```text
We're continuing work on Noterator (KallumS/Noterator): a macOS notation app
(JUCE 8, C++17, Apple silicon only) whose music mostly comes from my
generators, which run unchanged inside it through embedded Lua:
- Generate tab: Generate Notes (my Good Idea engine, the main one - motifs,
  phrases, measures and drum grooves), Suggest Notes (Midi Suggester: ideas
  around chords or a tune I have), Vary Notes (Midi Variator: changes music
  already written). In the code they keep their engine names.
- Blocks tab: Starting Blocks as a toolbox of chords, arpeggios, runs and
  intervals on every degree of the key, placed at the caret one after another.
- Bars can be chosen by clicking or dragging over the page, and Generate
  Notes fills them across the chosen instruments.
- New scores start from templates up to a full orchestra and a big band; every
  instrument keeps its own sound through Apple's built-in General MIDI synth.
- Scores open and export as MIDI and MusicXML; audio exports as WAV. The page
  is black on white by default, with a Dark page option.
It works; I have tested it on my Mac.

I'm not technical: explain things in plain words, show me screenshots of what
changed, and make sure each change reaches me as the downloadable Mac app from
GitHub Actions.

Before doing anything:
1. If the work so far (branch claude/epic-hypatia-z0ya3r) is not yet in your
   branch, start your branch from it.
2. Read CLAUDE.md, then docs/ARCHITECTURE.md (every decision, 0001-0025, on
   one page), then the latest logs in docs/sessions/. Follow their rules -
   especially: never edit the vendored engines in Engines/<app>/ (adapters are
   fine), no JUCE in Source/Core, where a generated result lands is decided
   only in Controller::place, a part's MIDI channel can be above 16 (banks),
   look at a render after any engraving change, and commit and push early.
3. Build and run the tests (core, app tests, tools/try_generators.lua), and
   render the demo page, to confirm everything is green before changing
   anything. For anything in the window, run the app headless under Xvfb and
   look at screenshots.

This session:
<what I want next - for example: "a page view", "dynamics and articulations",
"drawable CC lanes", "VST3 instruments per part", "save my own templates", or
a list of things I noticed while testing>

The roadmap, in the order I'm most likely to want it: a page view (systems on
pages, title, parts); dynamics, articulations and slurs, with articulation
switching for sample libraries (and carried in MusicXML); drawable CC lanes
seeded by AutoCC; VST3 and CLAP instruments per part and SoundFonts through
the Mac's General MIDI synth; saving my own templates; real-time MIDI
recording; Windows. Known rough edges are in the latest session log's
"Not done yet".

When you finish: write the session log in docs/sessions/ (and its line in
docs/sessions/README.md), add a decision record in docs/decisions/ (with its
line in docs/decisions/README.md and docs/ARCHITECTURE.md) for any choice
someone could reasonably make the other way, update CLAUDE.md and README.md
if what the app does or a rule changed, rewrite docs/NEXT-SESSION.md, push,
check that the GitHub Actions run is green on both Linux and macOS (the Mac
test log should say it rendered through Apple General MIDI), and tell me
where to download the new app.
```

## Things worth knowing before you paste it

- The other repositories (Good-Idea, Midi-Catalogue, ScaleView and so on) are
  only needed if the session has to copy an engine update across; it will ask
  for them if so.
- If you want the app to open with a normal double-click, the session can set
  up Apple Developer ID signing - it needs your Apple Developer account, added
  to the repository as secrets.
- The work so far is all on the branch `claude/epic-hypatia-z0ya3r`. If you
  would like it on the main branch first, ask the session to open a pull
  request for you to merge.
