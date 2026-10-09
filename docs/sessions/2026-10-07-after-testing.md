# 2026-10-07 (later) - After the first test on a Mac

The user tried the first version on their Mac and came back with five things:

1. The Midi Catalogue under Generate felt redundant - take it out, keep any
   code that helps elsewhere. Good Idea is the main generator, Midi Suggester
   the backup for ideas around chords or a melody, Midi Variator for changing
   what has been generated.
2. Starting Blocks should not be under Generate but a toolbox of its own, for
   picking chords, intervals, runs and arpeggios, as it works in REAPER.
3. Click and drag across bars to select them, and generate into that selection.
4. A light page by default; dark only if chosen.
5. MusicXML import and export.

All five are in, with decisions 0017-0021.

## The route

- **Generate's list** (0017). Each adapter now says which panel it belongs to;
  Generate shows only `panel = "generate"`. The Catalogue is still loaded:
  its instrument numbers already live in the C++ table, and the core tests and
  `NoteratorRender generated` use it, so removing it would only lose tests.
- **Light page** (0020). The default flipped, the toolbar button became
  **Dark page**, and the choice is now remembered with the zoom
  (`juce::ApplicationProperties`). 0015 keeps its text and gains a pointer.
- **MusicXML** (0021). A small XML reader/writer in the core keeps it testable
  without JUCE. Export is written from the engraver's layout - its beams,
  ties, tuplets, voices and spelled accidentals - so the file says what the
  page says. Checked outside: music21 read an export with every bar the right
  length, and a Bach chorale (BWV 66.6, from music21's corpus) imported as
  SATB in F sharp minor with its pickup in place. `.mxl` is unzipped in the
  app with `juce::ZipFile`, following `META-INF/container.xml`.
- **Choosing bars** (0019). Core first: `fitToSpan` (repeat and cut to the
  span), `spreadChords` (one chord note to each part, top first) and
  `insertIntoRange` (tune to the top part, bass to the bottom, chords between,
  drums to a kit; only receiving parts cleared). Good Idea's adapter picks the
  longest length that fits when its own is Any. Then the controller's
  `BarRange` and the page: click an empty bar, drag for more, drag along the
  lanes for every part, Shift-click to stretch, Shift-drag for the old note
  lasso. Tried in the window: bars 2-6 of a string quartet, a Good Idea phrase
  of chords, dealt out across all four instruments.
- **The Blocks toolbox** (0018). Kind buttons, a button per degree (numeral
  and note), a one-staff preview through the same engraver and renderer, Play
  and Insert. A block goes at the caret, not the bar's start, and the caret
  moves past it: arpeggios on I, IV and V then a chord on I went in one after
  another. The adapter gained Starting Blocks' chord families (Triads, 6ths &
  7ths, Extended, Altered, Sus & Add, Quartal, Named), not only the key's
  chords. The settings menus became one component for both tabs.

## What looked wrong and was not

- **A block of eight identical Ds** in the preview. The click had landed on
  **Bass** (the root repeated at eighths), not Arpeggio. Run through the
  adapter in plain Lua, the arpeggio was D F A as it should be.
- **A drag meant to end in bar 5 chose bar 6.** The pointer was past bar 6's
  barline; the range follows the bar under the pointer, as it should.
- **A four-bar phrase into five bars repeats its first bar.** That is
  `fitToSpan` doing what it says; the engine offers no five-bar phrase.

## Mistakes

- `std::llround` without `<cmath>` in a test: built with GCC here, would have
  failed elsewhere. `Parser { text }` tripped `-Wmissing-field-initializers`.
- The session's context was compacted mid-way through the bar work; the core
  half had been tested but not committed. It was committed first thing after.

## Not done yet

Everything in the first log's list, less MusicXML export, plus:

- MusicXML carries notes, ties, tuplets, keys, meters, tempo, clefs and
  instruments; not dynamics, articulations, slurs, lyrics or text (the score
  has nowhere to keep them yet). Timewise files are refused.
- The Blocks toolbox always follows the score's key; REAPER's lets you pick
  any key. A block into a one-part range is repeated to fill it.
- Suggester ignores chosen bars beyond reading their notes; it still puts its
  lines beside the source.
- Chosen bars are cleared by most edits that change the selection; undo does
  not bring a range back.
