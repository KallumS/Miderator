# Decisions

One file per decision that would otherwise have to be re-argued from scratch:
what the situation was, what was chosen, and what it costs. They are dated and
numbered and **they are not edited to stay true** - a reversed decision keeps
its text and gains a line at the top pointing at the one that replaced it.

`CLAUDE.md` is the working guide; these are the reasons underneath it. The code
refers to them by number: "(decision 0006)".

0001-0025 are Noterator's, from before Miderator was copied from it (0026);
they still hold for everything Miderator shares with it, and the ones the
piano roll changed say so at the top. 0026 on are Miderator's own.

| | | |
| --- | --- | --- |
| [0001](0001-a-juce-app-not-a-reascript.md) | A desktop app in JUCE, not a ReaScript | Accepted |
| [0002](0002-the-score-is-midi-shaped.md) | The score is MIDI-shaped; notation is worked out from it | Accepted |
| [0003](0003-run-the-lua-engines-unchanged.md) | Run the family's Lua engines unchanged, through embedded Lua | Accepted |
| [0004](0004-one-set-of-columns-in-a-galley.md) | One set of columns for every staff, in one long system | Noterator's screen; here 0027 |
| [0005](0005-edits-are-functions-undo-is-a-copy.md) | Every edit is a function in Edit.h; undo keeps the score from before | Accepted |
| [0006](0006-instruments-carry-their-context.md) | Instruments carry their context, and everything reads it | Accepted |
| [0007](0007-one-performance-and-the-macs-own-orchestra.md) | One performance for playback and export, through the Mac's own General MIDI set | Accepted |
| [0008](0008-general-midi-gets-two-cc-lanes.md) | General MIDI playback gets AutoCC's CC7 and CC11 only; files get all four | Accepted |
| [0009](0009-bravura-not-drawn-glyphs.md) | Bravura for the symbols, where Starting Blocks Notation drew its own | Noterator only (0029) |
| [0010](0010-verify-by-rendering.md) | Verify the engraving by rendering it | Accepted |
| [0011](0011-selection-results-go-beside-or-after.md) | Results made from a selection go beside it or after it, never over it | Accepted |
| [0012](0012-apple-silicon-only-built-by-ci.md) | Apple silicon only, built and tested on a Mac runner | Accepted |
| [0013](0013-autocc-computed-for-a-whole-part.md) | AutoCC is computed for a whole part, not performed live | Accepted |
| [0014](0014-chords-and-keys-read-from-the-score.md) | Chord and key lanes: ScaleView names, Suggester's key finder, and only real harmony named | Accepted |
| [0015](0015-the-house-scheme-and-a-dark-page.md) | The house colour scheme, a dark page by default and a light page on request | Default replaced by 0020 |
| [0016](0016-step-input-with-the-keys-people-know.md) | Step-time input, with the keys notation users already know | Noterator's; here 0028 |
| [0017](0017-catalogue-leaves-generate.md) | Midi Catalogue leaves the Generate tab; Good Idea leads it | Accepted |
| [0018](0018-starting-blocks-is-a-toolbox.md) | Starting Blocks is a toolbox of its own, not a generator | Accepted; narrowed by 0024 |
| [0019](0019-bars-can-be-chosen-and-filled.md) | Bars can be chosen on the page, and generated music fills them | Accepted |
| [0020](0020-light-page-by-default.md) | A light page by default; dark on request | Noterator's; here 0031 |
| [0021](0021-musicxml-in-and-out.md) | MusicXML in and out, from the core, with a parser of our own | Accepted |
| [0022](0022-templates-in-score-order.md) | Templates for sections and big ensembles, in the core, in score order | Accepted |
| [0023](0023-a-synth-per-sixteen-channels.md) | A synth for every sixteen channels, so every part has its own | Accepted |
| [0024](0024-blocks-without-drums-and-bass.md) | The Blocks toolbox offers chords, arpeggios, runs and intervals only | Accepted |
| [0025](0025-generators-named-for-what-they-do.md) | In the app, the generators are named for what they do | Accepted |
| [0026](0026-miderator-is-noterator-with-a-piano-roll.md) | Miderator is Noterator with a piano roll, in a repository of its own, sharing its music code | Accepted |
| [0027](0027-tracks-above-a-piano-roll-below.md) | The tracks above, a piano roll below, on one timeline | Accepted |
| [0028](0028-notes-drawn-with-the-mouse-on-a-grid.md) | Notes are drawn, moved and stretched with the mouse, on a grid | Accepted |
| [0029](0029-the-engraver-stays-for-musicxml.md) | The engraver stays, unseen, for MusicXML; the music font goes | Accepted |
| [0030](0030-a-velocity-lane-and-quantise.md) | A velocity lane under the roll, and quantise | Accepted |
| [0031](0031-dark-by-default.md) | Dark by default, light on request | Accepted |
| [0032](0032-warnings-read-from-the-notes.md) | What an instrument cannot play is read from the notes | Accepted |
| [0033](0033-follow-the-playhead-switchable.md) | The view follows the playhead, a page at a time, and can be switched off (both apps) | Accepted; default changed by 0034 |
| [0034](0034-follow-scrolls-smoothly.md) | Following the playhead scrolls smoothly, by default (both apps) | Accepted |
| [0035](0035-space-plays-from-the-start.md) | Space plays from bar 1, Shift+Space from the caret; buttons to the start and the end (both apps) | Accepted |
| [0036](0036-one-note-at-a-time-for-one-note-instruments.md) | Generated music gives an instrument no more notes at once than it plays (both apps) | Accepted |
| [0037](0037-a-file-menu-button.md) | New, Open, Save and Export under one File button (both apps) | Accepted |
| [0038](0038-the-score-tab-moves-into-file.md) | The Score tab's settings move into the File menu (both apps) | Accepted |
| [0039](0039-undo-sound-input-and-look-into-file.md) | Undo, Redo, Sound, the input mode and the look move into the File menu (both apps) | Accepted |
| [0040](0040-generated-music-orchestrated-across-chosen-parts.md) | Generated music is orchestrated across the chosen parts (both apps) | Accepted |
| [0041](0041-where-generated-music-goes.md) | Where generated music goes: every part, one part, or the parts chosen (both apps) | Accepted |
| [0042](0042-a-single-line-to-one-part-and-a-span-filled-once.md) | A single line goes to one part; chosen bars are filled once (both apps) | Accepted |
| [0043](0043-an-audition-plays-every-note-on-a-piano.md) | An audition plays every note, on a piano | Accepted |
| [0044](0044-a-name-click-lets-go-of-bars-chosen-elsewhere.md) | Clicking a part's name lets go of bars chosen elsewhere | Accepted |
| [0045](0045-built-on-the-users-own-mac.md) | The Mac app can be built on the user's own Mac | Accepted |
| [0046](0046-blocks-chords-keep-their-root.md) | Blocks chords keep the root they were made on | Accepted |
