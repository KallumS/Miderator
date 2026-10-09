--[[ Good Idea - the ideas.

     Pure Lua: nothing here touches REAPER or ImGui. `M.make(st, meter, seed)`
     takes the settings, the project's metre and an idea number, and returns
     the idea - a block of MIDI notes in parts. Nothing else goes in, so the
     same three always give the same idea
     (docs/decisions/0002-an-idea-is-a-number.md).

     How an idea is made, top to bottom:

       1. resolve   every setting left on "Any" is rolled, each from its own
                    draw of the dice
       2. plan      the idea is cut into units - a basic idea, its repeat, a
                    sequence of it, an answer, a fragment, a cadence - from a
                    template for the kind and length (Motif, Phrase) or the
                    form (Measure)
       3. harmony   each unit gets its chords: a walk through the
                    tonic-subdominant-dominant table (gi_theory) ending in
                    the unit's cadence. A repeat reuses its source's chords,
                    a sequence shifts them, an answer changes the ending
       4. melody    each new unit gets a rhythm (the strongest beats, or a
                    Euclidean spread for syncopation) and a line (a weighted
                    walk pulled toward a contour, chord tones on the beat,
                    leaps filled by a step back). Repeats copy, sequences
                    shift, answers change the ending, fragments cut and fall
       5. parts     the chords voiced and played in a pattern; for a Measure
                    a bass line and a drum kit as well
       6. block     notes in quarter notes, a part per instrument

     Everything is worked out in sixteenth-note steps and scale positions, and
     only turned into quarter notes and MIDI pitches at the very end.
]]

local M = {}
local T

function M.init(theory)
  T = theory
  M.buildSettings()
  return M
end

M.MAX_SEED = 99999
M.ACCENT = 115

------------------------------------------------------------------------------
-- The dice
--
-- Midi Variator's generator (Park and Miller's), so a seed always gives the
-- same numbers on any Lua. Each part of the idea draws from its own stream,
-- seeded from the idea number and the stream's name, so changing one setting
-- changes only what depends on it: a different chord style leaves the melody
-- alone, a different key moves the same tune into it.
------------------------------------------------------------------------------

function M.random(seed)
  local s = math.floor(math.abs(seed or 1)) % 2147483646 + 1
  local function nextr()
    s = s * 48271 % 2147483647
    return (s - 1) / 2147483646
  end
  -- The first few numbers from neighbouring seeds are close together; let
  -- them go.
  for _ = 1, 4 do nextr() end
  return nextr
end

local STREAMS = { pick = 1, plan = 2, harmony = 3, rhythm = 4, melody = 5,
                  chords = 6, bass = 7, drums = 8, borrow = 9, push = 10,
                  pull = 11, kit = 12, colour = 13, invert = 14, applied = 15, schema = 16,
                  tension = 17, sixnine = 18, ghost = 19, chroma = 20, passing = 21, commontone = 22 }

function M.stream(seed, name)
  local salt = STREAMS[name] or 0
  return M.random((math.floor(seed or 1) * 7919 + salt * 104729 + 12345) % 2147483646)
end

local function between(rnd, lo, hi) return lo + (hi - lo) * rnd() end
local function coin(rnd, p) return rnd() < (p or 0.5) end
local function pickOne(rnd, list) return list[math.floor(rnd() * #list) + 1] end
local function round(x) return math.floor(x + 0.5) end

-- One of `items` in proportion to `weights` (a list, or nil for even), from
-- one number already drawn.
local function pickAt(x, items, weights)
  local total = 0
  for i = 1, #items do total = total + (weights and weights[i] or 1) end
  local at = x * total
  for i = 1, #items do
    at = at - (weights and weights[i] or 1)
    if at < 0 then return items[i] end
  end
  return items[#items]
end

local function weighted(rnd, items, weights) return pickAt(rnd(), items, weights) end

------------------------------------------------------------------------------
-- Metre
--
-- A bar is counted in sixteenths. The beat is what the time signature's
-- bottom number says, except in 6/8, 9/8 and 12/8, where it is the dotted
-- quarter - that is how those are felt and played.
------------------------------------------------------------------------------

function M.meter(num, den)
  num = math.max(1, math.floor(tonumber(num) or 4))
  den = math.max(1, math.floor(tonumber(den) or 4))
  local bar = math.max(4, round(num * 16 / den))
  local beat
  if den == 8 and num % 3 == 0 and num >= 6 then beat = 6
  else beat = math.max(1, math.floor(16 / den)) end
  if bar % beat ~= 0 then beat = 1 end
  local beats = bar // beat
  -- The half-bar beat (beat 3 of 4/4) is stronger than the others.
  local mid = (beats % 2 == 0 and beats >= 4) and bar // 2 or nil
  return { num = num, den = den, bar = bar, beat = beat, beats = beats, mid = mid,
           barBeats = bar / 4 }
end

-- How strong a step is: 3 the downbeat, 2.5 the half bar, 2 a beat, 1 an
-- eighth, 0 a sixteenth.
function M.strength(meter, step)
  local s = step % meter.bar
  if s == 0 then return 3 end
  if meter.mid and s == meter.mid then return 2.5 end
  if s % meter.beat == 0 then return 2 end
  if s % 2 == 0 then return 1 end
  return 0
end

local function snap(meter, x) return round(x / meter.beat) * meter.beat end

------------------------------------------------------------------------------
-- The settings
--
-- One list, and everything reads it: the window draws a row of buttons per
-- setting, the state is clamped and saved from it, resolve rolls the ones
-- left on "Any", and the UI test clicks every value of every one. A setting
-- shows only `when` it means something for what is chosen (no dead controls).
------------------------------------------------------------------------------

M.KINDS = { "Motif", "Phrase", "Measure", "Drums" }

local function notDrums(st) return st.kind ~= "Drums" end
local function hasMelody(st)
  return st.kind ~= "Drums" and (st.kind ~= "Phrase" or st.content ~= "Chords")
end
local function hasChords(st)
  return st.kind == "Measure" or (st.kind == "Phrase" and st.content ~= "Melody")
end
M.hasMelody, M.hasChords = hasMelody, hasChords

local function barsName(v) return v .. (v == 1 and " bar" or " bars") end

function M.buildSettings()
  local rootIdx, scaleIdx = {}, {}
  for i = 1, #T.ROOTS do rootIdx[i] = i end
  for i = 1, #T.SCALES do scaleIdx[i] = i end

  M.SETTINGS = {
    { id = "kind", label = "Make a", step = "Idea", values = M.KINDS, default = "Motif",
      hints = {
        Motif = "A short melodic hook, 1 to 4 bars, built from one small cell repeated and varied.",
        Phrase = "A 1 to 4 bar phrase: a melody, a chord pattern, or both in one clip, with a proper ending.",
        Measure = "8 to 16 bars of music: melody, chords and bass, laid out in a form.",
        Drums = "1 to 16 bars of drums on General MIDI notes: a groove, varied, with fills where you ask for them.",
      } },
    { id = "motifBars", label = "Bars", step = "Idea", values = { 1, 2, 3, 4 }, any = true,
      default = "Any", name = barsName, weights = { 1, 3, 1, 2 },
      when = function(st) return st.kind == "Motif" end },
    { id = "phraseBars", label = "Bars", step = "Idea", values = { 1, 2, 3, 4 }, any = true,
      default = "Any", name = barsName, weights = { 1, 3, 1, 3 },
      when = function(st) return st.kind == "Phrase" end },
    { id = "measureBars", label = "Bars", step = "Idea", values = { 8, 12, 16 }, any = true,
      default = "Any", name = barsName, weights = { 3, 1, 2 },
      when = function(st) return st.kind == "Measure" end },
    { id = "content", label = "Content", step = "Idea", values = { "Melody", "Chords", "Both" },
      any = true, default = "Any", weights = { 1, 1, 1.3 },
      when = function(st) return st.kind == "Phrase" end,
      hints = {
        Melody = "A tune on its own.",
        Chords = "A chord pattern on its own, with the root in the bass.",
        Both = "A tune over a chord pattern, together in one clip (tune on channel 1, chords on 2).",
      } },

    { id = "root", label = "Key", step = "Key", values = rootIdx, any = true, default = 1,
      name = function(v) return T.ROOTS[v].name end, when = notDrums,
      -- Any rolls the twelve common spellings, not C# and Db both.
      anyValues = { 1, 3, 4, 6, 7, 8, 9, 11, 13, 14, 16, 17 } },
    { id = "scale", label = "Scale", step = "Key", values = scaleIdx, any = true, default = 1,
      name = function(v) return T.SCALES[v].name end, when = notDrums,
      -- Any rolls the scales a tune is usually written in; the colour scales
      -- (blues, whole tone, diminished) are there to be chosen.
      anyValues = { 1, 2, 5, 8, 3, 6, 7, 10, 11 },
      anyWeights = { 3, 3, 1.5, 1.2, 1, 0.8, 0.8, 1, 1 } },

    { id = "pace", label = "Pace", step = "Feel", values = { "Calm", "Flowing", "Busy" },
      any = true, default = "Any", weights = { 1, 1.5, 1 },
      hints = {
        Calm = "Few notes: halves, quarters and the odd eighth.",
        Flowing = "Eighth notes moving.",
        Busy = "Sixteenths.",
      } },
    { id = "groove", label = "Groove", step = "Feel",
      values = { "Straight", "Syncopated", "Tresillo", "Habanera", "Clave", "3+3+3+3+2+2" },
      any = true, default = "Any",
      -- (Any rolls the two 1.0 had; the named rhythms, 1.11, are there to
      -- choose.)
      anyValues = { "Straight", "Syncopated" },
      hints = {
        Straight = "Notes on the strongest beats first: on the beat, then the half beat.",
        Syncopated = "Notes spread evenly over the bar (a Euclidean rhythm) and turned so they fall off the beat - the tresillo, the cinquillo.",
        Tresillo = "3+3+2 eighths: 1, the 'and' of 2, 4 - the Cuban tresillo, the commonest syncopation in pop. Pulsing chords, a pulsing bass and the kick play it; the tune is syncopated. (In 4/4; syncopated elsewhere.)",
        Habanera = "The habanera: 1, the 'and' of 2, 3, 4 - the tresillo with beat 3 filled in. Pulsing chords, a pulsing bass and the kick play it. (In 4/4; syncopated elsewhere.)",
        Clave = "The son clave, 3-2, in a bar of sixteenths: 1, the 'a' of 1, the 'and' of 2, the 'and' of 3, 4. Pulsing chords, a pulsing bass and the kick play it. (In 4/4; syncopated elsewhere.)",
        ["3+3+3+3+2+2"] = "Sixteenths grouped 3+3+3+3+2+2 ('Shape of You'): the pop off-beat that spans the bar. Pulsing chords, a pulsing bass and the kick play it. (In 4/4; syncopated elsewhere.)",
      } },

    { id = "contour", label = "Contour", step = "Melody",
      values = { "Arch", "Rise", "Fall", "Wave", "Valley" }, any = true, default = "Any",
      weights = { 1.6, 1, 1, 1, 0.8 }, when = hasMelody,
      hints = {
        Arch = "Up to a high point about two thirds through (the golden section), then down.",
        Rise = "Climbing all the way.",
        Fall = "Starting high and coming down.",
        Wave = "Up and down, and up and down.",
        Valley = "Down to a low point two thirds through, then back up.",
      } },
    { id = "register", label = "Register", step = "Melody", values = { "Low", "Middle", "High" },
      any = true, default = "Middle", weights = { 1, 2, 1 }, when = hasMelody,
      hints = {
        Low = "Around G3.",
        Middle = "Around G4.",
        High = "Around E5.",
      } },

    { id = "colour", label = "Colour", step = "Chords", values = T.COLOURS, any = true,
      default = "Any", weights = { 1.5, 1, 1 }, when = hasChords,
      hints = {
        Triads = "Three-note chords: C, Dm, G.",
        Sevenths = "Every chord with its seventh: Cmaj7, Dm7, G7.",
        Mixed = "Sevenths where they pull (ii, V), added ninths on the others: Cadd9, Dm7, G7.",
      } },
    { id = "chordPace", label = "Chord pace", step = "Chords",
      values = { "Slow", "One a bar", "1.5 a bar", "Two a bar", "4 a bar" }, any = true, default = "Any",
      -- Shown as numbers (0.5, 1, 1.5, 2, 4 a bar); kept by their 1.0 names
      -- so saved settings still load. Any rolls the three 1.0 had, with
      -- 1.0's weights, so a 1.0 idea number still rolls the same pace; 1.5
      -- and 4 a bar are there to choose.
      name = function(v)
        return ({ Slow = "0.5 a bar", ["One a bar"] = "1 a bar", ["Two a bar"] = "2 a bar" })[v] or v
      end,
      anyValues = { "Slow", "One a bar", "Two a bar" }, anyWeights = { 0.7, 1.6, 0.8 },
      when = hasChords,
      hints = {
        Slow = "Half a chord a bar: a chord every two bars.",
        ["One a bar"] = "A chord a bar.",
        ["1.5 a bar"] = "Three chords every two bars: in 4/4, three beats, three beats, two - the 3+3+2 that pushes a progression along. (Chosen, not rolled by Any.)",
        ["Two a bar"] = "Two chords a bar: one every half bar.",
        ["4 a bar"] = "A chord on every beat - four a bar in 4/4, three in 3/4. (Chosen, not rolled by Any.)",
      } },
    { id = "chordStyle", label = "Style", step = "Chords", values = { "Block", "Pulse", "Broken", "Pedal", "Offbeat", "Fill" },
      any = true, default = "Any", weights = { 1, 1.2, 1, 1, 1, 1 }, when = hasChords,
      -- (Any rolls the three 1.0 had; the 1.11 styles are there to choose.)
      anyValues = { "Block", "Pulse", "Broken" }, anyWeights = { 1, 1.2, 1 },
      hints = {
        Block = "Held chords, struck again at each bar line.",
        Pulse = "The chord struck in rhythm: on the beat, or syncopated, or in the groove's named rhythm.",
        Broken = "One note at a time: up, up and down, Alberti, rolling.",
        Pedal = "Each chord struck once and held until the next - a pad, an orchestra's 'sustain pedal' under the tune.",
        Offbeat = "Short chords on the off-beats only - the reggae skank, the 'pah' of oom-pah (afterbeats).",
        Fill = "The chords answer the tune: struck where it holds a note or rests, quiet while it moves - and always where the chord changes, if nowhere else.",
      } },

    { id = "form", label = "Form", step = "Arrangement",
      values = { "Period", "Sentence", "Song", "Loop", "Hybrid 1", "Hybrid 2", "Hybrid 3", "Hybrid 4",
                 "Ternary", "Extended" }, any = true, default = "Any",
      -- (1.13: not shown; every idea rolls one, each about one time in ten.
      -- 0025.)
      hidden = true,
      when = function(st) return st.kind == "Measure" end,
      hints = {
        ["Hybrid 1"] = "Antecedent + continuation (Caplin's first hybrid): an idea and a contrasting idea to a half close, then breaking it up and speeding to a full close. (Chosen, not rolled by Any.)",
        ["Hybrid 2"] = "Antecedent + cadential: an idea and a contrasting idea to a half close, then one long cadential phrase home. (Chosen, not rolled by Any.)",
        ["Hybrid 3"] = "Compound basic idea + continuation: an idea and a contrasting one with no cadence between, then breaking it up to a full close. (Chosen, not rolled by Any.)",
        ["Hybrid 4"] = "Compound basic idea + consequent: an idea and a contrasting one, then both again, the second time to a full close. (Chosen, not rolled by Any.)",
        Ternary = "A B A (the small ternary): a theme closed in the key, a contrasting middle standing on the dominant, the theme again to finish. (Chosen, not rolled by Any.)",
        Extended = "A sentence stretched by a deceptive cadence: it reaches V and goes to vi instead of home, so the end is played 'one more time' to a full close. (Chosen, not rolled by Any.)",
        Period = "A question and its answer: the same opening twice, ending open and then closed.",
        Sentence = "An idea, the idea again on another chord, then breaking it up and speeding to the cadence.",
        Song = "A A B A: a tune, the tune again, something different, the tune to finish.",
        Loop = "One progression round and round, the tune varied over it - a groove to build on.",
      } },
    { id = "bass", label = "Bass", step = "Arrangement", values = { "Held", "Pulse", "Moving" },
      any = true, default = "Any",
      when = function(st) return st.kind == "Measure" end,
      hints = {
        Held = "The root, held under each chord.",
        Pulse = "The root, in the rhythm a kick drum would play: on 1 and 3, or syncopated.",
        Moving = "On the beat: the root, then the fifth or the octave, and a step into the next chord.",
      } },
    -- Retired: in 1.2 a Measure always had drums; since 1.3 it has none -
    -- drums are the Drums kind. It stays in the list, never shown and only
    -- ever Off, because the list is the order the dice are drawn in.
    { id = "drums", label = "Drums", step = "Arrangement", values = { "Off" },
      default = "Off", retired = true, when = function() return false end },
    { id = "layout", label = "Layout", step = "Out", values = { "Tracks", "One item" },
      default = "Tracks", when = function(st) return st.kind == "Measure" end,
      hints = {
        Tracks = "A new track for each part - Melody, Chords, Bass - under the selected track.",
        ["One item"] = "Every part in one item on the selected track, each on its own MIDI channel (1, 2, 3).",
      } },

    { id = "velocity", label = "Velocity", step = "Out", values = { "Flat", "Accents", "Shaped" },
      default = "Shaped",
      hints = {
        Flat = "Every note at 100.",
        Accents = "Every note at 100, and the downbeats and the start of each idea at " .. M.ACCENT .. ".",
        Shaped = "As a player would: the downbeat loudest, the beats a little softer, the off-beats softer still; the chords under the tune, the inner notes of a chord under its top, the bass just under the tune.",
      } },

    -- Added in 1.1. They come last in this list because the list is also
    -- the order the dice are drawn in: added anywhere else, they would have
    -- changed what every 1.0 idea number rolled.
    { id = "figures", label = "Figures", step = "Feel",
      values = { "Plain", "Dotted", "Triplets", "Mixed" }, any = true, default = "Any",
      weights = { 2, 1, 1, 1 },
      hints = {
        Plain = "Straight eighths and sixteenths.",
        Dotted = "Now and then a pair of notes becomes long-short: a dotted eighth and a sixteenth, a dotted quarter and an eighth - in the tune, the chords and a moving bass.",
        Triplets = "Now and then a beat becomes three: eighth-note triplets, or three quarter notes across two beats. The drums shuffle and broken chords roll in threes.",
        Mixed = "Now and then dotted, now and then triplets.",
      } },
    { id = "push", label = "Push", step = "Feel", values = { "None", "Some", "Lots" },
      any = true, default = "Any", weights = { 1.5, 1.5, 1 }, when = notDrums,
      hints = {
        None = "Every chord arrives on the beat.",
        Some = "Some chords arrive an eighth early - on the 'and' before the beat - and the tune and the bass come with them.",
        Lots = "Most chords arrive an eighth early: a pushed, syncopated feel.",
      } },
    { id = "borrowed", label = "Borrowed", step = "Key", values = { "Off", "Rare", "Common" }, default = "Rare",
      when = function(st) return st.kind ~= "Drums" and (st.scale == "Any" or #T.SCALES[st.scale].iv == 7) end,
      hints = {
        Off = "Every chord from the scale.",
        Rare = "About one idea in four borrows one chord from another scale on the same key note - a minor iv or a bVI in a major key, a major IV in a minor one. And now and then, before a close's V, the ii or IV becomes a chromatic chord: the Neapolitan (Db/F in C) or an augmented sixth (Ab7 in C: Italian, French, or German, which goes through the I6/4). The window says which chord, and where it is from. Seven-note scales only.",
        Common = "About two ideas in three borrow a chord, and a longer one (eight chords or more) sometimes two; about half the closes that can take a chromatic chord do.",
      } },

    -- Added in 1.2, last for the same reason.
    { id = "pull", label = "Pull", step = "Feel", values = { "None", "Some", "Lots" },
      any = true, default = "Any", weights = { 1.5, 1.5, 1 }, when = hasChords,
      hints = {
        None = "Every chord is played on the beat.",
        Some = "Some chords are played an eighth late, laid back behind the beat; the tune stays on it, and the bass either stays with the tune or lies back with the chords (the idea decides).",
        Lots = "Most chords lie back an eighth: a lazy, behind-the-beat feel.",
      } },
    { id = "drumBars", label = "Bars", step = "Idea", values = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 },
      any = true, default = "Any", name = function(v) return tostring(v) end,
      weights = { 1, 3, 0.3, 3, 0.2, 0.3, 0.2, 2, 0.2, 0.2, 0.2, 0.6, 0.2, 0.2, 0.2, 1 },
      when = function(st) return st.kind == "Drums" end },
    { id = "beat", label = "Beat", step = "Drums",
      values = { "Backbeat", "Half-time", "Four on the floor", "Breakbeat", "Reggaeton" }, any = true, default = "Any",
      weights = { 2, 1, 1, 1, 1 }, when = function(st) return st.kind == "Drums" end,
      hints = {
        Backbeat = "Kick on and around 1 and 3, snare on 2 and 4.",
        ["Half-time"] = "The snare on 3 only: twice as slow, twice as heavy.",
        ["Four on the floor"] = "A kick on every beat, a clap on 2 and 4, open hats on the off-beats.",
        Breakbeat = "A broken kick, the snare on 2 and 4 with one knocked off it, sixteenths on the hats. (In 4/4; a backbeat elsewhere.)",
        Reggaeton = "The dembow: a kick on every beat, the snare on the 'a' of 1, the 'and' of 2, the 'a' of 3 and the 'and' of 4 - the tresillo, twice. (In 4/4; a backbeat elsewhere.)",
      } },
    { id = "fills", label = "Fills", step = "Drums",
      values = { "None", "At the end", "Every 4 bars", "Every 2 bars" }, any = true, default = "Any",
      weights = { 0.6, 2, 2, 1 }, when = function(st) return st.kind == "Drums" end,
      hints = {
        None = "The groove all the way.",
        ["At the end"] = "A fill in the last bar, leading back to the top - and a crash when it gets there.",
        ["Every 4 bars"] = "A fill at the end of every fourth bar, and of the last.",
        ["Every 2 bars"] = "A fill at the end of every second bar.",
      } },
    { id = "cymbal", label = "Cymbal", step = "Drums", values = { "Hats", "Ride" }, any = true,
      default = "Any", weights = { 3, 1 }, when = function(st) return st.kind == "Drums" end,
      hints = {
        Hats = "Time kept on the hi-hats (42), opening (46) now and then.",
        Ride = "Time kept on the ride (51), the bell (53) on the beat, the hi-hat pedal (44) on the backbeat.",
      } },

    -- Added in 1.5, last for the same reason. Off, Close and Off are the
    -- 1.4 sound, and draw nothing from the dice.
    { id = "flavours", label = "Flavours", step = "Chords", values = { "Off", "Rare", "Common" }, default = "Rare",
      when = function(st)
        return hasChords(st) and (st.scale == "Any" or #T.SCALES[st.scale].iv == 7)
      end,
      hints = {
        Off = "Every chord in the Colour chosen, and nothing else.",
        Rare = "Now and then a chord takes another colour: a sus4 or sus2, an added 2nd or 9th, a 6th (or a 6/9) - and, where the chords have sevenths, a 7sus4, a 9th, or the diminished chord on its third (G7 becomes Bm7b5). Never the first chord or the cadence. Seven-note scales only.",
        Common = "The same colours, on about half the chords that can take one.",
      } },
    { id = "voicing", label = "Voicing", step = "Chords", values = T.VOICINGS, any = true, default = "Close",
      -- (Any rolls the seven 1.5 had; Power, 1.12, and Drop 4, 1.14, are there
      -- to choose.)
      anyValues = { "Close", "Open", "Drop 2", "Drop 3", "Drop 2 & 4", "Shell", "Rootless" },
      anyWeights = { 3, 1, 1, 1, 1, 1, 1 },
      weights = { 3, 1, 1, 1, 1, 1, 1, 1, 1 }, when = hasChords,
      hints = {
        Close = "Every note once, inside an octave, each chord nearest the one before.",
        Open = "Spread wide: the root, the fifth, then the third an octave up and the rest above it.",
        ["Drop 2"] = "Four notes in close position with the second from the top dropped an octave - the guitarist's and arranger's favourite.",
        ["Drop 3"] = "Four notes with the third from the top dropped an octave: a wide gap at the bottom.",
        ["Drop 2 & 4"] = "Four notes with the second and the fourth from the top dropped an octave: wide, like a big band's saxes.",
        ["Drop 4"] = "Four notes in close position with the lowest dropped an octave: three close notes over a gap. (Chosen, not rolled by Any.)",
        Shell = "The root, the third and the seventh - the notes that say what the chord is, and nothing else.",
        Rootless = "No root - the bass has it: the third, fifth, seventh and ninth, the jazz pianist's left hand.",
        Power = "The root, the fifth and the root an octave up - no third: the rock guitarist's power chord (C5). A chord with no perfect fifth plays its root in octaves. (Chosen, not rolled by Any.)",
      } },
    { id = "inversions", label = "Inversions", step = "Chords", values = { "Off", "Rare", "Common" }, default = "Rare",
      when = hasChords,
      hints = {
        Off = "Every chord with its root in the bass.",
        Rare = "Now and then a chord's third, fifth or seventh in the bass, where it makes the bass move by step - C G/B Am, a passing chord, or the I6/4 before the cadence. The bass plays it.",
        Common = "The same, on more than half the chords where an inversion does its job.",
      } },

    -- Added in 1.7, last for the same reason. Free is 1.6's part-writing,
    -- note for note, and draws nothing from the dice.
    { id = "partWriting", label = "Part-writing", step = "Chords", values = { "By the book", "Free" },
      -- (1.13: not shown - always by the book; Free is kept for the tests,
      -- which compare the two. 0025.)
      default = "By the book", when = hasChords, hidden = true,
      hints = {
        ["By the book"] = "As the harmony and orchestration books have it: an inverted chord does not double its bass note (G/B plays no B above the bass), a seventh falls a step into the next chord, a half close with Mixed is a plain V, the chords sit just under the tune, the bass no more than an octave and a fifth below the chords and never in among them, and no parallel fifths or octaves between the tune and the bass.",
        Free = "As Good Idea did before 1.7: every chord note in every chord, the chords under the whole tune's lowest note, the bass where it falls.",
      } },

    -- Added in 1.8, last for the same reason. Off draws nothing.
    { id = "applied", label = "Applied", step = "Key", values = { "Off", "Rare", "Common" }, default = "Rare",
      when = function(st) return st.kind ~= "Drums" and (st.scale == "Any" or #T.SCALES[st.scale].iv == 7) end,
      hints = {
        Off = "No chord borrowed from another key.",
        Rare = "Now and then the chord before a major or minor chord becomes that chord's own dominant - its V (or V7), or its leading-tone chord - borrowed from the key the next chord is home in: D7 before G in C major (V7/V), E before Am (V/vi). The most common chromatic chord there is. And where the bass climbs a tone (F to G), now and then a passing diminished seventh on the note between (F F#dim7 G); and, but with Triads, a held I or V coloured by the common-tone diminished seventh (C D#dim7/C C), its root held. The window says which, and where. Seven-note scales only.",
        Common = "The same, on more of the chords that can take one, and in most ideas.",
      } },

    -- Added in 1.9, last for the same reason. Walk draws nothing new.
    { id = "progression", label = "Progression", step = "Chords",
      values = { "Walk", "Any named", "Doo-wop", "Singer-songwriter", "Puff", "Pachelbel", "Lament",
                 "Circle", "Double plagal", "Galant", "Blues", "Do-Re-Mi", "Romanesca", "Fonte", "Monte",
                 "Aprile", "Pastorella", "Ponte" },
      -- (1.13: not shown; every idea walks or plays a named progression that
      -- suits its key, half and half. 0025.)
      any = true, default = "Any", anyValues = { "Walk", "Any named" }, anyWeights = { 1, 1 },
      hidden = true, when = hasChords,
      hints = {
        Walk = "Each chord drawn from the one before, by how strongly it leads there (tonic, subdominant, dominant) - the way Good Idea has always worked. A named progression fills the chords in order, going round, and a passage that closes still ends on its cadence, and a repeated passage carries the progression on. It is heard best in a Loop, which plays nothing else.",
        ["Any named"] = "One of the named progressions that suits the key, chosen by the idea number.",
        ["Doo-wop"] = "I vi IV V - the '50s doo-wop progression (Open Music Theory). Major keys.",
        ["Singer-songwriter"] = "vi IV I V in a major key, i VI III VII in a minor one - never quite sure which is home.",
        Puff = "I iii IV I - the 'Puff' opening, the bass climbing do mi fa. Major keys.",
        Pachelbel = "I V6 vi iii6 IV I6 IV V - Pachelbel's canon, the bass stepping down do ti la sol fa mi. Major keys.",
        Lament = "i VII VI V - the lament (the Andalusian cadence), the bass falling do te le sol. Minor keys.",
        Circle = "Round the circle of fifths: I IV vii iii vi ii V I, or in minor i iv VII III VI ii V i ('I Will Survive').",
        ["Double plagal"] = "I bVII IV I - two plagal steps home (the coda of 'Hey Jude'). Major keys.",
        Galant = "The galant schemata: a Meyer (I V4/3 V6/5 I, the bass do re ti do, the tune do ti fa mi) then a Prinner (IV I6 vii6 I, the bass fa mi re do, the tune la sol fa mi) - Gjerdingen's stock phrases. In minor, i V4/3 V6/5 i, iv i6 vii°6 i.",
        Blues = "The 12-bar blues - I I I I IV IV I I V IV I I - and its 8- and 16-bar cousins, a chord a bar. Measures only.",
        ["Do-Re-Mi"] = "I V6/5 I - the bass do ti do under a tune rising do re mi: the galant Do-Re-Mi.",
        Romanesca = "I V6 vi I6 - the bass do ti la mi: the galant Romanesca. In minor, i v6 VI i6, the bass do te le me.",
        Fonte = "V7/ii ii V7 I - a pair stepped down: the galant Fonte ('fountain'). In minor, V7/iv iv V7/III III: the minor key's ii cannot be a key, so it falls from iv to III.",
        Monte = "V7/IV IV V7/V V - a pair stepped up: the galant Monte ('mountain'). In minor, V7/iv iv V7/V V.",
        Aprile = "I V4/3 V6/5 I under a tune do ti re do: the galant Aprile.",
        Pastorella = "I V7 I under a tune mi re fa mi, the V7 holding two notes of it: the galant Pastorella.",
        Ponte = "The tonic, then V held (V7 with Sevenths or Mixed) - standing on the dominant: the galant Ponte ('bridge').",
      } },

    -- Added in 1.10, last for the same reason. Off draws nothing, and the
    -- tension notes have dice of their own.
    { id = "tension", label = "Tension", step = "Melody", values = { "Off", "Rare", "Common" },
      default = "Rare", when = hasMelody,
      hints = {
        Off = "Every note on the beat is a note of the chord, as Good Idea's tunes were before 1.10.",
        Rare = "Now and then the tune leans on the beat and falls a step to the chord: a suspension (the note before held over the chord change, then falling), an appoggiatura (leapt up to, a step above the chord's note), or, at a close, the last note arriving an eighth early (an anticipation).",
        Common = "The same, on more of the beats where one can go.",
      } },
    { id = "secondVoice", label = "Second voice", step = "Melody", values = { "Off", "Thirds", "Sixths" },
      default = "Off", when = hasMelody,
      hints = {
        Off = "The tune alone.",
        Thirds = "A second part a third under the tune, moving with it - a fourth or a sixth under where a third would not be a note of the chord on the beat. Its own channel (or track).",
        Sixths = "A second part a sixth under the tune, moving with it - a third or a fourth under where a sixth would not be a note of the chord on the beat. Its own channel (or track).",
      } },

    -- Added in 1.12, last for the same reason. None draws nothing.
    { id = "keyChange", label = "Key change", step = "Arrangement",
      values = { "None", "Step up", "Half step up", "Truck driver" }, default = "None",
      when = function(st) return st.kind == "Measure" end,
      hints = {
        None = "One key all the way.",
        ["Step up"] = "The last section a whole tone higher - the pop key change for a last chorus. Tune, chords and bass all go up.",
        ["Half step up"] = "The last section a semitone higher.",
        ["Truck driver"] = "A whole tone up, with the new key's V7 squeezed in before it (C ... A7 | D) - the 'truck-driver' gear change - into the new key's tonic: the last section that starts on it, or one made to start on it. In scales other than the seven-note ones, a plain step up.",
      } },

    -- Added in 1.14, last for the same reason. Off draws nothing; the
    -- ghost notes have dice of their own.
    { id = "ghosts", label = "Ghost notes", step = "Drums", values = { "Off", "Rare", "Common" },
      default = "Rare", when = function(st) return st.kind == "Drums" end,
      hints = {
        Off = "Only the groove's own snare.",
        Rare = "Now and then a ghost note: the snare tapped very quietly on a sixteenth, mostly just before or after the backbeat - the funk and R&B drummer's in-between notes. The same in every bar, quiet at any Velocity.",
        Common = "The same ghost notes, and more of them.",
      } },
    -- (1.14: not shown - the engine decides, half and half, whether the
    -- bass lies back with pulled chords or holds the beat with the tune.)
    { id = "bassPull", label = "Bass pull", step = "Arrangement", values = { "On the beat", "With the chords" },
      any = true, default = "Any", hidden = true,
      when = function(st) return st.kind == "Measure" end,
      hints = {
        ["On the beat"] = "Where the chords are pulled, the bass stays on the beat with the tune.",
        ["With the chords"] = "Where the chords are pulled, the bass lies back with them: the new note an eighth late, the one before held to meet it.",
      } },
  }
  M.BY_ID = {}
  for _, s in ipairs(M.SETTINGS) do M.BY_ID[s.id] = s end
end

function M.valueName(s, v)
  if v == "Any" then return "Any" end
  return s.name and s.name(v) or tostring(v)
end

-- (A `hidden` setting, 1.13, is never shown but still rolled: the engine
-- decides it.)
function M.shows(s, st) return not s.hidden and (not s.when or s.when(st)) end

------------------------------------------------------------------------------
-- State
------------------------------------------------------------------------------

function M.newState()
  local st = {}
  for _, s in ipairs(M.SETTINGS) do st[s.id] = s.default end
  st.seed = 1
  st.autoplay = 0
  st.swing = 0
  st.follow = 1            -- 1.17: play along when REAPER plays
  st.exportTo = "Project"  -- 1.17: where Export writes (gi_place's exportDir)
  return st
end

-- Puts every field back inside what exists. Saved settings come from
-- anywhere, so nothing in them is trusted.
function M.clampState(st)
  for _, s in ipairs(M.SETTINGS) do
    local v = st[s.id]
    local good = (v == "Any" and s.any) or false
    for _, x in ipairs(s.values) do if x == v then good = true end end
    if not good then st[s.id] = s.default end
  end
  local seed = tonumber(st.seed)
  if seed and seed >= 1 and seed <= M.MAX_SEED then st.seed = math.floor(seed) else st.seed = 1 end
  st.autoplay = (tonumber(st.autoplay) == 1) and 1 or 0
  local swing = tonumber(st.swing)
  st.swing = (swing and swing >= 0 and swing <= 100) and math.floor(swing) or 0
  st.follow = (tonumber(st.follow) == 0) and 0 or 1
  if st.exportTo ~= "REAPER" and st.exportTo ~= "Folder" then st.exportTo = "Project" end
  return st
end

------------------------------------------------------------------------------
-- 1. Resolve
------------------------------------------------------------------------------

-- Every setting takes one draw from the "pick" stream whether it is on Any
-- or not, so fixing one setting never changes what another rolls. That is
-- what makes "Keep" (turning every Any into what this idea rolled) give
-- exactly the same idea back.
function M.resolve(st, seed)
  local rnd = M.stream(seed, "pick")
  local r = { rolled = {} }
  for _, s in ipairs(M.SETTINGS) do
    local x = rnd()
    local v = st[s.id]
    if v == "Any" and s.any then
      v = pickAt(x, s.anyValues or s.values, s.anyWeights or (not s.anyValues and s.weights) or nil)
      r.rolled[s.id] = true
    end
    r[s.id] = v
  end
  if r.kind == "Motif" then r.bars = r.motifBars
  elseif r.kind == "Phrase" then r.bars = r.phraseBars
  elseif r.kind == "Drums" then r.bars = r.drumBars
  else r.bars = r.measureBars end
  if r.kind == "Motif" then r.content = "Melody"
  elseif r.kind == "Measure" then r.content = "All"
  elseif r.kind == "Drums" then r.content = "Melody"; r.drumsOnly = true end
  r.melody = r.content ~= "Chords" and not r.drumsOnly
  r.chords = r.content ~= "Melody"
  return r
end

-- The bars setting the kind uses.
function M.barsSetting(kind)
  return (kind == "Motif" and "motifBars") or (kind == "Phrase" and "phraseBars")
      or (kind == "Drums" and "drumBars") or "measureBars"
end

------------------------------------------------------------------------------
-- 2. The plan
--
-- A unit is written letter:bars[:cadence]. A letter's first appearance is new
-- material; `a` again repeats it, `a'` answers it (the same start, a new
-- ending), `a~` sequences it (the whole unit moved up or down, chords and
-- all). `f` fragments the basic idea - its first half, falling - and `c` is a
-- new cadential unit. The cadence `X` is the idea's ending, drawn for the
-- kind. These are the shapes Open Music Theory gives for the sentence and the
-- period, and the A A B A of a song, at the size of the idea.
------------------------------------------------------------------------------

M.PLANS = {
  Motif = {
    [1] = { "a:0.5 a~:0.5:X", "a:0.5:open a':0.5:X", "a:1:X" },
    [2] = { "a:1:open a':1:X", "a:1 a~:1:X", "a:1:open b:1:X" },
    [3] = { "a:1 a~:1 c:1:X", "a:1:open b:1 a':1:X" },
    [4] = { "a:1 a~:1 f:1 c:1:X", "a:2:open a':2:X", "a:1:open b:1 a:1:open b':1:X" },
  },
  Phrase = {
    [1] = { "a:1:X" },
    [2] = { "a:2:X", "a:1:HC a':1:X" },
    [3] = { "a:2 c:1:X", "a:1 a~:1 c:1:X" },
    [4] = { "a:4:X", "a:2:HC a':2:X", "a:1 a~:1 f:1 c:1:X" },
  },
}

M.FORMS = {
  Period   = { [8] = "a:4:HC a':4:PAC",
               [12] = "a:4:HC a':4:IAC b:4:PAC",
               [16] = "a:4:IAC b:4:HC a:4:IAC c:4:PAC" },
  Sentence = { [8] = "a:2 a~:2 f:2 c:2:PAC",
               [12] = "a:2 a~:2 f:4 c:4:PAC",
               [16] = "a:4 a~:4 f:4 c:4:PAC" },
  Song     = { [8] = "a:2:IAC a:2:PAC b:2:HC a:2:PAC",
               [12] = "a:4:IAC a:4:PAC b:4:PAC",
               [16] = "a:4:IAC a:4:PAC b:4:HC a:4:PAC" },
  Loop     = { [8] = "a:4:open a':4:open",
               [12] = "a:4:open a':4:open a:4:open",
               [16] = "a:4:open a':4:open a:4:open a'':4:open" },
  -- Added in 1.8 (Caplin's hybrid themes and small ternary, and a sentence
  -- stretched by a deceptive cadence; Open Music Theory, "Hybrid themes",
  -- "The Small Ternary", "Internal Expansions").
  ["Hybrid 1"] = { [8] = "a:2 b:2:HC f:1 f:1 c:2:PAC",
                   [12] = "a:3 b:3:HC f:2 f:1 c:3:PAC",
                   [16] = "a:4 b:4:HC f:4 c:4:PAC" },
  ["Hybrid 2"] = { [8] = "a:2 b:2:HC c:4:PAC",
                   [12] = "a:3 b:3:HC c:6:PAC",
                   [16] = "a:4 b:4:HC c:8:PAC" },
  ["Hybrid 3"] = { [8] = "a:2 b:2 f:1 f:1 c:2:PAC",
                   [12] = "a:3 b:3 f:2 f:1 c:3:PAC",
                   [16] = "a:4 b:4 f:4 c:4:PAC" },
  ["Hybrid 4"] = { [8] = "a:2 b:2 a:2 b':2:PAC",
                   [12] = "a:3 b:3 a:3 b':3:PAC",
                   [16] = "a:4 b:4 a:4 b':4:PAC" },
  Ternary  = { [8] = "a:2:HC a':2:PAC b:2:HC a':2:PAC",
               [12] = "a:4:PAC b:4:HC a:4:PAC",
               [16] = "a:4:HC a':4:PAC b:4:HC a':4:PAC" },
  Extended = { [8] = "a:2 a~:2 c:2:DC c:2:PAC",
               [12] = "a:2 a~:2 f:2 c:2:DC f:2 c:2:PAC",
               [16] = "a:4 a~:4 f:2 c:2:DC f:2 c:2:PAC" },
}

-- How an idea that is not a Measure ends. A motif is a hook, so mostly it
-- leaves the door open and loops; a phrase mostly closes.
local ENDINGS = {
  Motif  = { { "open", "PAC", "IAC" }, { 3, 2, 1 } },
  Phrase = { { "PAC", "HC", "IAC", "open" }, { 3, 1.5, 1, 1.5 } },
}

function M.parsePlan(text, meter, finalCad)
  local units, firstOf = {}, {}
  local cum = 0
  for tok in text:gmatch("%S+") do
    local name, bars, cad = tok:match("^([^:]+):([%d%.]+):?(%a*)$")
    local letter, mark = name:match("^(%a)(.*)$")
    bars = tonumber(bars)
    local start = snap(meter, cum * meter.bar)
    cum = cum + bars
    local stop = snap(meter, cum * meter.bar)
    local u = { letter = letter, mark = mark, bars = bars, start = start, len = stop - start,
                cad = (cad == "" and "none") or (cad == "X" and finalCad) or cad }
    if letter == "f" then u.kind, u.of = "frag", 1
    elseif letter == "c" then u.kind = "cad"
    elseif not firstOf[letter] then u.kind = "new"; firstOf[letter] = #units + 1
    elseif mark == "~" then u.kind, u.of = "seq", firstOf[letter]
    elseif mark:find("'") then u.kind, u.of = "answer", firstOf[letter]
    else u.kind, u.of = "repeat", firstOf[letter] end
    -- A repeat that ends differently from its source is an answer.
    if u.kind == "repeat" and units[u.of].cad ~= u.cad then u.kind = "answer" end
    if u.len > 0 then units[#units + 1] = u end
  end
  return units, snap(meter, cum * meter.bar)
end

function M.plan(r, meter, rnd)
  local text, finalCad
  if r.kind == "Measure" then
    text = M.FORMS[r.form][r.bars]
    finalCad = "PAC"
  else
    local e = ENDINGS[r.kind]
    finalCad = weighted(rnd, e[1], e[2])
    text = pickOne(rnd, M.PLANS[r.kind][r.bars])
  end
  local units, total = M.parsePlan(text, meter, finalCad)
  -- (1.13) Extended's stretch is a deceptive close or, half the time, an
  -- evaded one (EC): the full close never comes, and the passage goes round
  -- "one more time".
  if r.kind == "Measure" and r.form == "Extended" and rnd() < 0.5 then
    for _, u in ipairs(units) do if u.cad == "DC" then u.cad = "EC" end end
  end
  -- A sequence moves by a step up, a step down, or to the dominant (up a
  -- fifth) - the second statement of a sentence's basic idea.
  for _, u in ipairs(units) do
    if u.kind == "seq" then u.shift = weighted(rnd, { 1, -1, 4 }, { 2, 1.5, 2 }) end
  end
  local shape = {}
  for _, u in ipairs(units) do shape[#shape + 1] = u.letter .. u.mark end
  return { units = units, total = total, text = text, ending = units[#units].cad,
           shape = table.concat(shape, " ") }
end

------------------------------------------------------------------------------
-- 3. Harmony
------------------------------------------------------------------------------

local RATE = { Slow = 0.5, ["One a bar"] = 1, ["1.5 a bar"] = 1.5, ["Two a bar"] = 2, ["4 a bar"] = 4 }

-- Where an answer stops copying its source: half way, on a beat, and
-- always before the end (a unit one beat long copies nothing).
function M.cutFor(meter, src, u)
  local half = math.min(src.len, u.len) / 2
  local cut = math.floor(half / meter.beat + 0.5) * meter.beat
  return math.max(0, math.min(cut, u.len - meter.beat))
end

-- How many chords a stretch of `len` steps gets.
local function countFor(meter, len, r, kind, cad, first)
  local bars = len / meter.bar
  local rate = RATE[r.chordPace] or 1
  -- A continuation speeds the harmony up (Open Music Theory, the sentence).
  -- (Never slower than the pace chosen: at four a bar it is already quick.)
  if kind == "frag" then rate = math.max(rate, math.min(2, rate * 2)) end
  local n = math.max(1, round(bars * rate))
  local beats = len // meter.beat
  -- An ending needs two chords: one to lead to it, and the one it lands on.
  -- A half close is one chord, the dominant.
  local need = (cad == "HC" and 1) or ((cad ~= "none" or kind == "cad") and 2) or 1
  if beats >= 2 then n = math.max(n, need) end
  -- The opening unit starts on the tonic, so it needs a chord before its
  -- ending too - as long as the chords still fall evenly on the beats, line
  -- up with the bars, and come no faster than two a bar.
  if first and cad ~= "none" then
    local tail = (cad == "PAC" or cad == "IAC" or cad == "DC" or cad == "EC") and 2 or 1
    if n <= tail then
      for m = tail + 1, math.max(tail + 1, round(2 * bars)) do
        local per = beats // m
        if beats % m == 0 and (per % meter.beats == 0 or meter.beats % per == 0) then n = m; break end
      end
    end
  end
  return math.max(1, math.min(n, beats))
end

------------------------------------------------------------------------------
-- Named progressions (1.9; docs/decisions/0021-named-progressions.md)
--
-- The stock progressions of Open Music Theory's pop/rock pages, and two of
-- Gjerdingen's galant schemata. Each chord is a degree of the key, with the
-- scale it comes from when not the key's own (`from`, an index of
-- T.SCALES on the same key note) and the scale degree in its bass when it
-- is inverted (`bass`). They fill the chords a unit walks to, in order and
-- going round; a unit that closes still ends on its cadence.
------------------------------------------------------------------------------

M.PROGRESSIONS = {
  ["Doo-wop"] = { major = { { 0 }, { 5 }, { 3 }, { 4 } } },
  ["Singer-songwriter"] = { major = { { 5 }, { 3 }, { 0 }, { 4 } },
                            minor = { { 0 }, { 5, from = 2 }, { 2, from = 2 }, { 6, from = 2 } } },
  Puff = { major = { { 0 }, { 2 }, { 3 }, { 0 } } },
  Pachelbel = { major = { { 0 }, { 4, bass = 6 }, { 5 }, { 2, bass = 4 }, { 3 }, { 0, bass = 2 }, { 3 }, { 4 } } },
  Lament = { minor = { { 0 }, { 6, from = 2 }, { 5, from = 2 }, { 4, from = 3 } } },
  Circle = { major = { { 0 }, { 3 }, { 6 }, { 2 }, { 5 }, { 1 }, { 4 }, { 0 } },
             minor = { { 0 }, { 3 }, { 6, from = 2 }, { 2, from = 2 }, { 5, from = 2 }, { 1 }, { 4, from = 3 }, { 0 } } },
  ["Double plagal"] = { major = { { 0 }, { 6, from = 8 }, { 3 }, { 0 } } },
  -- (1.16) `sing`: the scale degree the schema's tune has on that stage
  -- (Open Music Theory, "Galant schemata - summary"): the Meyer do ti fa mi,
  -- the Prinner la sol fa mi. A list, for a chord that holds two stages.
  Galant = { major = { { 0, sing = 0 }, { 4, bass = 1, sing = 6 }, { 4, bass = 6, sing = 3 }, { 0, sing = 2 },
                       { 3, sing = 5 }, { 0, bass = 2, sing = 4 }, { 6, bass = 1, sing = 3 }, { 0, sing = 2 } },
             -- (1.14) In minor, "converted directly" (Open Music Theory):
             -- the V and the vii°6 from the harmonic minor, the iv from the
             -- natural.
             minor = { { 0, sing = 0 }, { 4, from = 3, bass = 1, sing = 6 }, { 4, from = 3, bass = 6, sing = 3 },
                       { 0, sing = 2 }, { 3, from = 2, sing = 5 }, { 0, bass = 2, sing = 4 },
                       { 6, from = 3, bass = 1, sing = 3 }, { 0, sing = 2 } } },
  -- (1.13) More galant schemata (Open Music Theory, "Galant schemata"): the
  -- Do-Re-Mi (I V6/5 I, the bass do ti do), the Romanesca (I V6 vi I6, the
  -- bass do ti la mi), the Fonte (V7/ii ii V7 I, a step down) and the Monte
  -- (V7/IV IV V7/V V, a step up). `appliedTo`: the chord is that degree's own
  -- dominant, from the home scale bent (I.appliedKey).
  ["Do-Re-Mi"] = { major = { { 0, sing = 0 }, { 4, bass = 6, sing = 1 }, { 0, sing = 2 } },
                   minor = { { 0, sing = 0 }, { 4, from = 3, bass = 6, sing = 1 }, { 0, sing = 2 } } },
  -- (1.14) And in minor. The Romanesca's bass falls do te le me - the
  -- natural minor's v6, as a falling line in minor takes it (the raised ti
  -- falling to le would be an augmented second). The Monte: V7/iv iv V7/V V.
  -- The Fonte cannot go from ii to i - the minor key's ii is diminished, and
  -- cannot be made a key of its own - so it tonicises iv, then III a step
  -- below (Fm, then Eb, in C minor): minor, then major, a step down, as the
  -- major key's ii to I.
  Romanesca = { major = { { 0 }, { 4, bass = 6 }, { 5 }, { 0, bass = 2 } },
                minor = { { 0 }, { 4, from = 2, bass = 6 }, { 5, from = 2 }, { 0, bass = 2 } } },
  Fonte = { major = { { 5, appliedTo = 1 }, { 1 }, { 4 }, { 0 } },
            minor = { { 0, appliedTo = 3 }, { 3, from = 2 }, { 6, from = 2, appliedTo = 2 }, { 2, from = 2 } } },
  Monte = { major = { { 0, appliedTo = 3 }, { 3 }, { 1, appliedTo = 4 }, { 4 } },
            minor = { { 0, appliedTo = 3 }, { 3, from = 2 }, { 1, appliedTo = 4 }, { 4, from = 3 } } },
  -- (1.16) The last three of Open Music Theory's schemata. The Aprile has
  -- the Meyer's chords and bass (do re ti do) under the tune do ti re do;
  -- the Pastorella I V7 V7 I (the bass do sol sol do) under mi re fa mi -
  -- one V7 holding two stages; the Ponte "holds onto" the dominant and "adds
  -- a seventh" ("Galant Schemata - continuation patterns"): V held, entered
  -- from the tonic so an idea on it opens at home. (The seventh is the
  -- Colour's: V7 with Sevenths or Mixed, V with Triads.)
  Aprile = { major = { { 0, sing = 0 }, { 4, bass = 1, sing = 6 }, { 4, bass = 6, sing = 1 }, { 0, sing = 0 } },
             minor = { { 0, sing = 0 }, { 4, from = 3, bass = 1, sing = 6 }, { 4, from = 3, bass = 6, sing = 1 },
                       { 0, sing = 0 } } },
  Pastorella = { major = { { 0, sing = 2 }, { 4, sing = { 1, 3 } }, { 0, sing = 2 } },
                 minor = { { 0, sing = 2 }, { 4, from = 3, sing = { 1, 3 } }, { 0, sing = 2 } } },
  Ponte = { major = { { 0 }, { 4 }, { 4 }, { 4 } },
            minor = { { 0 }, { 4, from = 3 }, { 4, from = 3 }, { 4, from = 3 } } },
  -- (A chord a bar, by the bar: not filled in order like the rest.)
  Blues = { blues = { [8] = { 0, 4, 3, 3, 0, 4, 0, 0 },
                      [12] = { 0, 0, 0, 0, 3, 3, 0, 0, 4, 3, 0, 0 },
                      [16] = { 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 4, 3, 0, 0 } } },
}
M.PROGRESSION_ORDER = { "Doo-wop", "Singer-songwriter", "Puff", "Pachelbel", "Lament", "Circle",
                        "Double plagal", "Galant", "Blues", "Do-Re-Mi", "Romanesca", "Fonte", "Monte",
                        "Aprile", "Pastorella", "Ponte" }

-- The progression an idea plays - its chords for this key's mode - or nil
-- (Walk, or one that does not suit the key, the kind or the length; `why`
-- says which).
function M.schemaFor(r, key, rnd)
  local name = r.progression
  if not name or name == "Walk" then return nil end
  local seven = T.scaleLen(key) == 7
  local q = T.degreeQuality(key, 0)
  local mode = (q == "major" and "major") or (q == "minor" and "minor") or nil
  local function fits(n)
    local p = M.PROGRESSIONS[n]
    if not seven then return false end
    if p.blues then
      -- (Only where I, IV and V are major or minor chords: not Lydian's
      -- diminished IV, nor Locrian's I.)
      for _, d in ipairs({ 0, 3, 4 }) do
        local q = T.degreeQuality(key, d)
        if q ~= "major" and q ~= "minor" then return false end
      end
      return r.kind == "Measure" and p.blues[r.bars] ~= nil
    end
    return mode ~= nil and p[mode] ~= nil
  end
  if name == "Any named" then
    local ok = {}
    for _, n in ipairs(M.PROGRESSION_ORDER) do if fits(n) then ok[#ok + 1] = n end end
    local x = rnd()
    if #ok == 0 then return nil, "no named progression suits this key" end
    name = ok[math.floor(x * #ok) + 1]
  elseif not fits(name) then
    local p = M.PROGRESSIONS[name]
    if not seven then return nil, name .. " needs a seven-note scale" end
    if p.blues then return nil, "the blues needs a Measure" end
    return nil, name .. " needs a " .. (p.major and "major" or "minor") .. " key"
  end
  local p = M.PROGRESSIONS[name]
  return { name = name, chords = p[mode], blues = p.blues and p.blues[r.bars], at = 0 }
end

-- `n` chords from the progression, going on from where it got to, the last
-- of them its cadence's (as `T.progression` would end).
local function schemaDegrees(sch, key, n, cad, rnd)
  local tail = {}
  if cad ~= "none" and cad ~= "open" then
    local need = (cad == "HC") and 1 or 2
    tail = T.progression(key, math.min(need, n), { cadence = cad }, rnd)
  end
  local degs, specs = {}, {}
  for i = 1, n - #tail do
    local c = sch.chords[sch.at % #sch.chords + 1]
    sch.at = sch.at + 1
    degs[i], specs[i] = c[1], c
  end
  for _, d in ipairs(tail) do degs[#degs + 1] = d end
  return degs, specs
end

-- `n` stretches of a span, as even as the beats allow.
local function evenSlots(meter, from, len, n)
  local out = {}
  for i = 1, n do
    local s = from + snap(meter, len * (i - 1) / n)
    local e = (i == n) and (from + len) or (from + snap(meter, len * i / n))
    if e > s then out[#out + 1] = { s = s, e = e } end
  end
  return out
end

local function withDegrees(slots, degrees, specs)
  for i, sl in ipairs(slots) do
    sl.degree = degrees[i] or degrees[#degrees]
    sl.spec = specs and specs[i] or nil
  end
  return slots
end

-- A unit's chords, as { s, e, degree } from the unit's start.
--
--   - a repeat, or an answer or sequence to the same ending, plays its
--     source's chords (a sequence moved by its step);
--   - an answer (or a repeat or sequence to a different ending) plays its
--     source's chords for the first half, by time, then walks to its own
--     ending - so the tune's first half fits it exactly as before;
--   - anything new walks from the chord after the last one.
local function unitChords(u, units, key, r, meter, rnd, prevLast, sch)
  -- The blues: a chord a bar, by where the bar is in the idea.
  if sch and sch.blues then
    local out = {}
    local bars = math.max(1, u.len // meter.bar)
    for i, sl in ipairs(evenSlots(meter, 0, u.len, bars)) do
      local bar = (u.start + sl.s) // meter.bar
      sl.degree = sch.blues[bar % #sch.blues + 1]
      sl.spec = { sl.degree, blues = true }
      out[i] = sl
    end
    return out
  end
  local src = (u.kind == "repeat" or u.kind == "answer" or u.kind == "seq") and units[u.of] or nil
  local shift = (u.kind == "seq") and u.shift or 0
  -- (A copied chord remembers the one it was copied from, `orig`, so a
  -- flavour or an inversion comes round with it.)
  local function moved(sl, e)
    return { s = sl.s, e = math.min(e or sl.e, sl.e), degree = T.normDegree(key, sl.degree + shift),
             orig = sl.orig or sl, spec = (shift == 0) and sl.spec or nil }
  end
  -- A named progression goes on under a repeat (the tune copied, fitted to
  -- the chords now under it) until it has come round to where the repeat's
  -- source began - then the repeat copies it: Pachelbel's eight chords over
  -- two four-bar statements of a Loop, then round again.
  if sch and not sch.blues and src and shift == 0 and src.schemaAt
     and sch.at % #sch.chords ~= src.schemaAt % #sch.chords then
    -- (1.14: and where an earlier passage of the same length and close
    -- began at this point of the progression, it plays that one's chords -
    -- the second time round a Loop is the first time again, flavours and
    -- all.)
    local twin
    for _, w in ipairs(units) do
      if w == u then break end
      if w.rel and w.schemaAt and w.schemaAt % #sch.chords == sch.at % #sch.chords
         and w.len == u.len and w.cad == u.cad then twin = w end
    end
    src = twin
  end
  if src and src.len == u.len and src.cad == u.cad and (u.kind ~= "seq" or u.cad == "none") then
    local out = {}
    for _, sl in ipairs(src.rel) do out[#out + 1] = moved(sl) end
    if sch and src.schemaAt and shift == 0 then
      u.schemaAt, u.schemaUsed = sch.at, src.schemaUsed
      sch.at = sch.at + src.schemaUsed
    end
    return out
  end
  local need = (u.cad == "PAC" or u.cad == "IAC" or u.cad == "DC" or u.cad == "EC") and 2 or ((u.cad ~= "none") and 1 or 0)
  local cut = src and M.cutFor(meter, src, u)
  -- (When the second half is too short for the ending's chords - half a bar
  -- of 4/4 has two beats, a full close needs a chord on each, and so on -
  -- the answer's chords are all new, and its tune is fitted to them.)
  if src and cut > 0 and (u.len - cut) // meter.beat >= need then
    local out = {}
    for _, sl in ipairs(src.rel) do
      if sl.s < cut then out[#out + 1] = moved(sl, cut) end
    end
    local rest = u.len - cut
    local n = countFor(meter, rest, r, "rest", u.cad, false)
    local degs, specs
    if sch then degs, specs = schemaDegrees(sch, key, n, u.cad, rnd)
    else
      degs = T.progression(key, n, { start = T.nextDegree(key, out[#out].degree, rnd),
                                     cadence = u.cad, loopTo = 0 }, rnd)
    end
    for _, sl in ipairs(withDegrees(evenSlots(meter, cut, rest, n), degs, specs)) do out[#out + 1] = sl end
    return out
  end
  local n = countFor(meter, u.len, r, u.kind, u.cad, u == units[1])
  if sch then
    u.schemaAt = sch.at
    local degs, specs = schemaDegrees(sch, key, n, u.cad, rnd)
    u.schemaUsed = sch.at - u.schemaAt
    return withDegrees(evenSlots(meter, 0, u.len, n), degs, specs)
  end
  local start = prevLast and T.nextDegree(key, prevLast, rnd) or 0
  local degs = T.progression(key, n, { start = start, cadence = u.cad, loopTo = 0 }, rnd)
  return withDegrees(evenSlots(meter, 0, u.len, n), degs)
end

-- Each unit's chords, and one timeline of { s, e, degree, chord } for the
-- whole idea, in steps. The same chord twice running (where one unit ends on
-- the chord the next begins with) is one chord, held.
function M.harmony(plan, key, r, meter, rnd, colour, sch, breakAt, tonicAt)
  local timeline = {}
  local prevLast
  for _, u in ipairs(plan.units) do
    u.rel = unitChords(u, plan.units, key, r, meter, rnd, prevLast, sch)
    -- (1.14) A section the truck driver needs on the tonic starts on it -
    -- its first chord a new one, not a copy.
    if tonicAt and u.start == tonicAt and u.rel[1] and u.rel[1].degree ~= 0 then
      local f = u.rel[1]
      u.rel[1] = { s = f.s, e = f.e, degree = 0 }
    end
    u.degrees, u.slots = {}, {}
    for _, rs in ipairs(u.rel) do
      u.degrees[#u.degrees + 1] = rs.degree
      local last = timeline[#timeline]
      local sl
      local spec = rs.spec
      local k = (spec and spec.from) and T.key(key.root, spec.from) or key
      -- (A named applied chord: the home scale bent to make it, a V7 but
      -- with Triads.)
      local applied = spec and spec.appliedTo and M.appliedKey(key, spec.appliedTo, "V")
      if applied then k = applied end
      -- (Not where a named progression moves the bass under the same chord:
      -- the Meyer's V4/3 to V6/5.)
      if last and last.degree == rs.degree and last.e == u.start + rs.s
         and (last.spec and last.spec.bass) == (spec and spec.bass) and u.start + rs.s ~= breakAt then
        last.e = u.start + rs.e
        sl = last
      else
        sl = { s = u.start + rs.s, e = u.start + rs.e, beat = u.start + rs.s, degree = rs.degree, key = k,
               chord = T.chord(k, rs.degree, (applied and colour ~= "Triads") and "Sevenths" or colour),
               origin = rs.orig or rs, spec = spec }
        if applied then sl.applied = { kind = "V", named = true } end
        -- A named progression's inverted chord: the scale degree it names
        -- in the bass (Pachelbel's V6, the Prinner's I6).
        if spec and spec.bass then
          local bpc = T.pc(k, spec.bass)
          for idx, pc in ipairs(sl.chord.pcs) do
            if pc == bpc and idx > 1 then
              sl.bassPc, sl.bassPos = pc, sl.chord.pos[idx]
              sl.inversion = ({ ["3"] = 1, ["5"] = 2, ["7"] = 3 })[T.roleOf(sl.chord, pc)]
            end
          end
        end
        timeline[#timeline + 1] = sl
      end
      if u.slots[#u.slots] ~= sl then u.slots[#u.slots + 1] = sl end
      -- (An evaded close, 1.13: its tonic is I6 - the bass does not arrive
      -- on do either.)
      if u.cad == "EC" and rs == u.rel[#u.rel] and sl.degree == 0 and not sl.bassPc then
        local third = T.pc(sl.key or key, 2)
        if sl.chord.has[third] and third ~= sl.chord.rootPc then
          sl.bassPc, sl.bassPos, sl.inversion = third, 2, 1
          sl.spec = sl.spec or { 0, bass = 2 }
          sl.evaded = true
        end
      end
    end
    prevLast = u.degrees[#u.degrees]
  end
  -- (A named applied chord - the Fonte's V7/ii - must lead to its chord: a
  -- close that cuts in leaves it the plain chord of its degree.)
  for i, sl in ipairs(timeline) do
    local nx = timeline[i + 1]
    if sl.applied and sl.applied.named and not (nx and nx.degree == sl.spec.appliedTo) then
      sl.key, sl.applied = key, nil
      sl.chord = T.chord(key, sl.degree, colour)
    end
  end
  return timeline
end

function M.chordAt(timeline, step)
  for i = #timeline, 1, -1 do
    if timeline[i].s <= step then return timeline[i] end
  end
  return timeline[1]
end

-- The scale sounding at a step: the key's own, or under a borrowed chord the
-- scale it was borrowed from.
function M.keyAt(ctx, step)
  return M.chordAt(ctx.timeline, step).key or ctx.key
end

------------------------------------------------------------------------------
-- Borrowed chords (docs/decisions/0011-borrowed-chords-rarely-and-named.md)
--
-- Modal mixture: a chord on the same degree, taken from another scale on
-- the same key note - Fm (iv) or Ab (bVI) in C major, from C minor; F (IV)
-- in C minor, from C Dorian. The degree, and so the walk, is unchanged; only
-- the chord's notes are. Under it everything - the tune, the bass's steps -
-- reads that scale, the way a player bends to a borrowed chord.
--
-- Rare by design: about one idea in four, one chord, never the first or the
-- last two (the opening tonic and the cadence stay the key's own), never a
-- diminished or augmented chord. Seven-note scales only, borrowing from
-- seven-note scales, so the scale positions line up note for note.
------------------------------------------------------------------------------

M.BORROW_CHANCE = { Rare = 0.25, Common = 0.65 }
-- With Common, a second borrowed chord in an idea of eight chords or more,
-- this often, never next to the first.
M.BORROW_AGAIN = 0.5

-- Where to borrow from, by what the home key is: the parallel minor or major
-- most of all (the commonest mixture), the modes a step from it next.
local BORROW_FROM = {
  major = { { 2, 3 }, { 8, 1.5 }, { 5, 1 }, { 3, 0.7 }, { 6, 0.4 }, { 7, 0.4 } },
  minor = { { 1, 3 }, { 5, 2 }, { 3, 1.5 }, { 6, 0.8 }, { 8, 0.6 }, { 7, 0.3 } },
}

local function sameNotes(a, b)
  if #a.pcs ~= #b.pcs then return false end
  for _, pc in ipairs(a.pcs) do if not b.has[pc] then return false end end
  return true
end

function M.borrow(timeline, key, r, rnd, colour)
  local chance = M.BORROW_CHANCE[r.borrowed]
  if not chance or T.scaleLen(key) ~= 7 or #timeline < 4 then return {} end
  if rnd() >= chance then return {} end
  local tonic = T.degreeQuality(key, 0)
  local from = BORROW_FROM[(tonic == "major") and "major" or "minor"]
  local inKey = {}
  for d = 0, 6 do inKey[T.pc(key, d)] = true end
  local cands, weights = {}, {}
  for i = 2, #timeline - 2 do
    local sl = timeline[i]
    -- (A named progression's chords are its own; a chord after a key
    -- change, 1.12, is in another key.)
    for _, f in ipairs((sl.spec or sl.moved or sl.raised) and {} or from) do
      if T.SCALES[f[1]].iv ~= T.SCALES[key.scale].iv then
        local other = T.key(key.root, f[1])
        local ch = T.chord(other, sl.degree, colour)
        local q = ch.quality
        local outside = false
        for _, pc in ipairs(ch.pcs) do if not inKey[pc] then outside = true end end
        if (q == "major" or q == "minor") and outside and not sameNotes(ch, sl.chord) then
          cands[#cands + 1] = { slot = sl, key = other, chord = ch }
          weights[#weights + 1] = f[2]
        end
      end
    end
  end
  if #cands == 0 then return {} end
  local function take(c)
    c.slot.chord, c.slot.key = c.chord, c.key
    -- Named against the home key: Ab in C major is bVI, Fm is iv.
    local shift = (c.chord.rootPc - T.pc(key, c.slot.degree)) % 12
    local numeral = ((shift == 11) and "b" or (shift == 1) and "#" or "") ..
                    T.degreeNumeral(c.key, c.slot.degree)
    c.slot.borrowed = { name = c.chord.name, numeral = numeral, from = M.keyName(c.key) }
  end
  local c = weighted(rnd, cands, weights)
  take(c)
  local out = { c.slot }
  if r.borrowed == "Common" and #timeline >= 8 and rnd() < M.BORROW_AGAIN then
    local at = {}
    for i, sl in ipairs(timeline) do at[sl] = i end
    local more, mw = {}, {}
    for i, d in ipairs(cands) do
      if math.abs(at[d.slot] - at[c.slot]) > 1 then more[#more + 1] = d; mw[#mw + 1] = weights[i] end
    end
    if #more > 0 then
      local d = weighted(rnd, more, mw)
      take(d)
      out[2] = d.slot
      if at[d.slot] < at[c.slot] then out = { d.slot, c.slot } end
    end
  end
  return out
end

------------------------------------------------------------------------------
-- Push: chords that arrive an eighth early
--
-- A chord change on a beat moves back an eighth, onto the "and" before it
-- (some, or most, by the Push setting). The chord before is cut short to
-- make room, so the chords still follow each other with no gap. The tune's
-- note on that beat comes early with it (`M.melody`), and so does the bass.
-- Only where the beat is a quarter or longer, and only
-- where the chord before is long enough to give up an eighth.
------------------------------------------------------------------------------

local PUSH = { None = 0, Some = 0.3, Lots = 0.65 }
M.PULL_CHANCE = { None = 0, Some = 0.3, Lots = 0.65 }

function M.push(timeline, meter, r, rnd)
  local p = PUSH[r.push] or 0
  if p == 0 or meter.beat < 4 then return end
  for i = 2, #timeline do
    local sl, prev = timeline[i], timeline[i - 1]
    if sl.s % meter.beat == 0 and prev.e - prev.s >= meter.beat + 2 and rnd() < p then
      sl.s, prev.e = sl.s - 2, prev.e - 2
      sl.pushed = true
    end
  end
end

------------------------------------------------------------------------------
-- Pull: chords that arrive an eighth late
-- (docs/decisions/0014-pull-the-chords-lie-back.md)
--
-- The opposite of a push, and only for the chords part: the comping lies
-- back behind the beat while the tune, the bass and the drums stay on it.
-- So it is done to a copy of the timeline that only the chords part plays
-- from (`idea.chordTimeline`); the harmony everything else hears is the one
-- on the beat. The chord before is held an eighth longer to meet it. A
-- pushed chord is not also pulled, and a chord must be long enough to give
-- up an eighth at its start.
------------------------------------------------------------------------------

function M.pull(timeline, meter, r, rnd)
  local out = {}
  for i, sl in ipairs(timeline) do
    local c = {}
    for k, v in pairs(sl) do c[k] = v end
    out[i] = c
  end
  local p = M.PULL_CHANCE[r.pull] or 0
  if p == 0 or meter.beat < 4 then return out end
  for i = 2, #out do
    local sl, prev = out[i], out[i - 1]
    if not sl.pushed and sl.s % meter.beat == 0 and sl.e - sl.s >= meter.beat + 2 and rnd() < p then
      sl.s, prev.e = sl.s + 2, prev.e + 2
      sl.pulled = true
    end
  end
  return out
end

------------------------------------------------------------------------------
-- Flavours and inversions (docs/decisions/0018-flavours-voicings-and-inversions.md)
--
-- Both touch single chords, never the first chord or a cadence's chords
-- (the last chord of every unit that closes, and the chord leading to a full
-- or imperfect close): the idea opens on the tonic and its cadences stay
-- the textbook's. Each draws from a stream of its own, and nothing when off.
------------------------------------------------------------------------------

-- The slots that stay as they are: the first, the last, and each cadence's.
local function cadenceSlots(plan, timeline)
  local keep = { [timeline[1]] = true, [timeline[#timeline]] = true }
  for _, u in ipairs(plan.units) do
    if u.cad ~= "none" and u.slots and #u.slots > 0 then
      keep[u.slots[#u.slots]] = true
      if (u.cad == "PAC" or u.cad == "IAC" or u.cad == "DC" or u.cad == "EC") and #u.slots > 1 then keep[u.slots[#u.slots - 1]] = true end
    end
  end
  return keep
end

------------------------------------------------------------------------------
-- A key change (1.12; docs/decisions/0024-...)
--
-- The pop key change for a last section (Open Music Theory, "Modulation";
-- Hutchinson, ch. 21): from the start of the last unit that begins at or
-- after half way, on a bar line, with two bars or more to go, every chord is
-- the same degree of the key a step (or a semitone) up, so the tune - which
-- is positions in the scale - goes up with it. The truck driver puts the new
-- key's V (V7 with Sevenths or Mixed) in the second half of the chord before
-- (or in its place, if it is short).
------------------------------------------------------------------------------

M.KEY_CHANGE = { ["Step up"] = 2, ["Half step up"] = 1, ["Truck driver"] = 2 }

-- `tonicAt(step)` (optional): whether the tonic sounds there. The truck
-- driver wants a section that starts on it, so it takes the latest that
-- does, if one does.
-- (1.14) `canTonic(u)` (optional): whether a section could be made to start
-- on the tonic. Where none starts on it, the latest that could is taken,
-- and the second value says so: the harmony then starts it there.
function M.changeAt(plan, meter, tonicAt, canTonic)
  local best, home, made
  for _, u in ipairs(plan.units) do
    if u.start > 0 and u.start * 2 >= plan.total and u.start % meter.bar == 0
       and plan.total - u.start >= 2 * meter.bar then
      best = u.start
      if tonicAt and tonicAt(u.start) then home = u.start end
      if canTonic and canTonic(u) then made = u.start end
    end
  end
  if not best then
    local last = plan.units[#plan.units]
    if last.start > 0 then best = last.start end
  end
  if home then return home end
  if made then return made, true end
  return best
end

function M.keyChange(plan, timeline, key, r, meter, colour, at)
  local semis = M.KEY_CHANGE[r.keyChange]
  if not semis or not at then return nil end
  local newKey = T.transpose(key, semis)
  -- (A named applied chord just before the change - its chord now in the
  -- new key - is plain again.)
  for i, sl in ipairs(timeline) do
    local nx = timeline[i + 1]
    if sl.s < at and nx and nx.s >= at and sl.applied and sl.applied.named then
      sl.key, sl.applied = key, nil
      sl.chord = T.chord(key, sl.degree, colour)
    end
  end
  for _, sl in ipairs(timeline) do
    if sl.s >= at then
      local was = sl.chord
      sl.key = T.transpose(sl.key or key, semis)
      sl.chord = T.chord(sl.key, sl.degree, colour)
      if sl.bassPc then sl.bassPc = (sl.bassPc + semis) % 12 end
      sl.moved = semis
    end
  end
  local truck
  if r.keyChange == "Truck driver" and T.scaleLen(key) == 7 then
    -- The new key's V: major, from harmonic minor in a minor key.
    local K = newKey
    if T.degreeQuality(K, 4) ~= "major" then
      local HARM
      for i, sc in ipairs(T.SCALES) do if sc.name == "Harm Minor" then HARM = i end end
      K = HARM and T.transpose(T.key(key.root, HARM), semis) or nil
      if K and T.degreeQuality(K, 4) ~= "major" then K = nil end
    end
    local idx
    for i, sl in ipairs(timeline) do if sl.s < at then idx = i end end
    local sl = idx and timeline[idx]
    -- (Only where the new key's tonic arrives at the change: the V is there
    -- "to prepare that tonic arrival" (Open Music Theory). Elsewhere it is
    -- a plain step up.)
    local arrive = idx and timeline[idx + 1]
    if K and sl and idx > 1 and arrive and arrive.degree == 0 then
      local V = { degree = 4, key = K, chord = T.chord(K, 4, (colour == "Triads") and "Triads" or "Sevenths"),
                  spec = { 4, truck = true }, truck = true }
      local cut = sl.s + snap(meter, (sl.e - sl.s) / 2)
      if sl.e - sl.s >= 2 * meter.beat and cut > sl.s and cut < sl.e then
        -- (The V is the gear change, not part of the passage before: that
        -- passage's close stays its own - 1.13.)
        V.s, V.e, V.beat = cut, sl.e, cut
        sl.e = cut
        table.insert(timeline, idx + 1, V)
      else
        V.s, V.e, V.beat = sl.s, sl.e, sl.beat
        timeline[idx] = V
        for _, u in ipairs(plan.units) do
          for j, x in ipairs(u.slots or {}) do if x == sl then u.slots[j] = V end end
        end
      end
      V.origin = V
      truck = V
      -- (A named applied chord the gear change cuts off from its chord - a
      -- Monte's V7/V before the new key's V - is plain again, 1.14.)
      for i, x in ipairs(timeline) do
        local nx = timeline[i + 1]
        if x.applied and x.applied.named and nx == V then
          x.key, x.applied = key, nil
          x.chord = T.chord(key, x.degree, colour)
        end
      end
    end
  end
  return { at = at, semis = semis, key = newKey, truck = truck }
end

------------------------------------------------------------------------------
-- The minor key's V (1.13)
--
-- In a minor key the dominant is major, its third the raised seventh - "v
-- ... rare; V from the harmonic minor scale" (Hutchinson, ch. 7, Figure
-- 7.3.1). So every V in a key on the Minor scale is the harmonic minor's,
-- and the tune bends with it (B natural in C minor). The modes keep their
-- own v: Aeolian, Dorian and Phrygian are what they are. A named
-- progression's chords are as written.
------------------------------------------------------------------------------

local MINOR, HARM_MINOR
function M.raiseDominant(timeline, key, colour)
  if not MINOR then
    for i, sc in ipairs(T.SCALES) do
      if sc.name == "Minor" then MINOR = i elseif sc.name == "Harm Minor" then HARM_MINOR = i end
    end
  end
  for _, sl in ipairs(timeline) do
    local k = sl.key or key
    if sl.degree == 4 and k.scale == MINOR and not k.iv and not sl.spec then
      sl.key = { root = k.root, scale = HARM_MINOR, lift = k.lift }
      sl.chord = T.chord(sl.key, 4, colour)
      if sl.bassPc then sl.bassPc = nil; sl.bassPos = nil; sl.inversion = nil end
      sl.raised = true
    end
  end
end

------------------------------------------------------------------------------
-- Applied chords (1.8; docs/decisions/0020-applied-chords-cadences-and-forms.md)
--
-- The chord before a major or minor chord becomes that chord's own dominant
-- (V, or V7 with Sevenths or Mixed) or its leading-tone chord (viio, viio7):
-- "a chromatically altered chord that also functions as a dominant chord in
-- the key of the chord that follows it" (Open Music Theory, "Applied
-- chords"; Hutchinson, chs. 17-18). It is made from the home scale with the
-- notes it needs bent a semitone - V/V in C is the C scale with F#, V/vi the
-- C scale with G# - so the tune keeps its positions and, while the chord
-- sounds, bends with it (`I.keyAt`). Never the tonic's (that is just V),
-- never the first chord or a cadence's, never a borrowed or flavoured one,
-- never two running; a copied chord does as its original did, where the
-- chord after it is the same.
------------------------------------------------------------------------------

M.APPLIED_CHANCE = { Rare = 0.15, Common = 0.4 }
M.APPLIED_LEADING = 0.25   -- how often it is the leading-tone chord, not V

-- The home scale with the notes of the dominant (`kind` "V") or the
-- leading-tone chord ("vii") of degree `x` bent to fit, and that chord's
-- degree; nil if it needs more than a semitone's bend, or bends nothing.
function M.appliedKey(key, x, kind)
  local n = T.scaleLen(key)
  if n ~= 7 then return nil end
  local iv = {}
  for i, v in ipairs(T.ivOf(key)) do iv[i] = v end
  local target = T.pc(key, x)
  local tonic = T.rootPc(key)
  -- Each chord note: its degree, and its distance above the target's root.
  local spec = (kind == "V") and { { x + 4, 7 }, { x + 6, 11 }, { x + 1, 2 }, { x + 3, 5 } }
                              or { { x + 6, 11 }, { x + 1, 2 }, { x + 3, 5 }, { x + 5, 8 } }
  local bent = false
  for _, sp in ipairs(spec) do
    local d = sp[1] % 7
    local want = (target + sp[2] - tonic) % 12
    if iv[d + 1] ~= want then
      local diff = (want - iv[d + 1] + 6) % 12 - 6
      if math.abs(diff) ~= 1 then return nil end
      iv[d + 1] = want
      bent = true
    end
  end
  for i = 2, n do if iv[i] <= iv[i - 1] then return nil end end
  if not bent then return nil end
  return { root = key.root, scale = key.scale, iv = iv }, spec[1][1] % 7
end

function M.applied(timeline, plan, key, r, rnd, colour)
  local chance = M.APPLIED_CHANCE[r.applied]
  if not chance or T.scaleLen(key) ~= 7 or #timeline < 3 then return {} end
  local keep = cadenceSlots(plan, timeline)
  local decided = {}
  local out = {}
  local lastDone
  -- (Only where the chord after is the same every time the passage comes
  -- round - a repeat, a Loop, an answer's first half - so it does too.)
  local nextOf, same = {}, {}
  for i, sl in ipairs(timeline) do
    local o = sl.origin or sl
    local nx = timeline[i + 1] and timeline[i + 1].degree or -1
    if nextOf[o] == nil then nextOf[o], same[o] = nx, true
    elseif nextOf[o] ~= nx then same[o] = false end
  end
  -- (The first chord and the last are never changed: nor are their copies.)
  for _, sl in ipairs({ timeline[1], timeline[#timeline] }) do decided[sl.origin or sl] = false end
  for i = 2, #timeline - 1 do
    local sl, before, after = timeline[i], timeline[i - 1], timeline[i + 1]
    local was = sl.origin and decided[sl.origin]
    local x1, x2 = rnd(), rnd()
    local x = after.degree
    local q = T.degreeQuality(key, x)
    local can = not keep[sl] and not sl.borrowed and not after.borrowed and lastDone ~= i - 1 and not sl.spec
                and not sl.moved and not after.moved and not after.truck and not sl.raised and not after.applied
                and same[sl.origin or sl]
                and x ~= 0 and (q == "major" or q == "minor")
    local go, kind
    if was ~= nil then
      go = was ~= false and was.target == x
      kind = go and was.kind
    else
      go = x1 < chance
      kind = (x2 < M.APPLIED_LEADING) and "vii" or "V"
    end
    local done = false
    if go and can then
      local akey, deg = M.appliedKey(key, x, kind)
      local ch = akey and T.chord(akey, deg, (colour == "Triads") and "Triads" or "Sevenths")
      -- (A chord with no bent note in it - V/IV as a plain triad is I - is
      -- not an applied chord.)
      local bent = false
      if ch then
        local home = {}
        for d = 0, 6 do home[T.pc(key, d)] = true end
        for _, pc in ipairs(ch.pcs) do if not home[pc] then bent = true end end
      end
      if bent and deg ~= before.degree then
        sl.chord, sl.key, sl.degree = ch, akey, deg
        local numeral = (kind == "V" and "V" or "viio") .. ((#ch.pcs > 3) and "7" or "") .. "/" ..
                        T.degreeNumeral(key, x)
        sl.applied = { name = ch.name, numeral = numeral, to = after.chord.name, kind = kind, target = x }
        out[#out + 1] = sl
        lastDone, done = i, true
      end
    end
    if sl.origin and was == nil then decided[sl.origin] = done and { kind = kind, target = x } or false end
  end
  return out
end

------------------------------------------------------------------------------
-- Chromatic chords (1.15; docs/decisions/0027-...)
--
-- The chromatic pre-dominants: "the most common chromatically altered
-- subdominant chords (aside from the applied dominant of V) are the
-- Neapolitan chord and the various augmented-sixth chords" (Open Music
-- Theory, "Chromatically altered subdominant chords"). At a close, the ii or
-- IV before its V may become one (with Borrowed on; dice of their own):
--
--   N6    the Neapolitan: the major chord on the flat 2nd (ra fa le), with
--         fa in the bass - Db/F in C. "Being a chromatically altered ii
--         chord, the Neapolitan has pre-dominant harmonic function"
--         (Hutchinson, 20.1). From the Phrygian on the same key note.
--   It+6  le do fi: Ab C F# in C (Ab7 without its fifth, on a lead sheet).
--   Fr+6  le do re fi: Ab C D F# (Ab7(b5)).
--   Ger+6 le do me fi: Ab C Eb F# (Ab7). "Almost always ... followed by a
--         cadential 6/4" (Open Music Theory): it takes the first half of
--         the V for the I6/4, or is not chosen.
--
-- Le in the bass; le and fi move out to sol (Hutchinson, 21.1). The tune
-- under one uses the home scale with those notes bent. Only before a major
-- V (one with a leading tone), never on a named progression's chord, a
-- borrowed or applied one, or after a key change. A copy does what its
-- original did.
------------------------------------------------------------------------------

M.CHROMATIC_CHANCE = { Rare = 0.35, Common = 0.8 }
-- (1.16) The Swiss sixth: the German's sound with ri for me - "the Swiss
-- chord tends to appear in major keys, with ri proceeding to mi and do
-- carrying over into the cadential 6/4" (Open Music Theory). In a major key
-- it shares the German's place, half and half; in minor, the German alone
-- ("almost always used in minor").
M.CHROMATIC_WEIGHT = { N6 = 2, ["It+6"] = 1, ["Fr+6"] = 1, ["Ger+6"] = 1, ["Sw+6"] = 0.5 }
local CHROMATIC_KINDS = { "N6", "It+6", "Fr+6", "Ger+6", "Sw+6" }

local function scaleNamed(name)
  for i, sc in ipairs(T.SCALES) do if sc.name == name then return i end end
end

-- The home scale with fa raised and la lowered (and mi lowered, for the
-- German sixth): the notes the tune hears under an augmented sixth. Nil if
-- the scale cannot take it.
local function aug6Key(key, kind)
  local iv = {}
  for i, v in ipairs(T.ivOf(key)) do iv[i] = v end
  iv[4], iv[6] = 6, 8
  if kind == "Ger+6" then iv[3] = 3 end
  if kind == "Sw+6" then iv[2] = 3 end
  for i = 2, 7 do if iv[i] <= iv[i - 1] then return nil end end
  return { root = key.root, scale = key.scale, iv = iv }
end

-- The augmented sixth chord, named as a lead sheet names it (Hutchinson,
-- 21.4): the dominant seventh on le it sounds like.
local function aug6Chord(k6, kind)
  local pos = { 5, 7 }
  if kind == "Fr+6" or kind == "Sw+6" then pos[#pos + 1] = 8 elseif kind == "Ger+6" then pos[#pos + 1] = 9 end
  pos[#pos + 1] = 10
  local ch = { degree = 5, pos = pos, pcs = {}, has = {}, colour = "Sevenths", quality = "major",
               aug6 = kind, numeral = kind }
  for i, p in ipairs(pos) do
    ch.pcs[i] = T.pc(k6, p)
    ch.has[ch.pcs[i]] = true
  end
  ch.rootPc = ch.pcs[1]
  ch.name = T.noteName(k6, 5) .. ({ ["It+6"] = "7(no5)", ["Fr+6"] = "7(b5)", ["Ger+6"] = "7", ["Sw+6"] = "7" })[kind]
  return ch
end

function M.chromatic(timeline, plan, key, r, rnd, meter)
  local chance = M.CHROMATIC_CHANCE[r.borrowed]
  if not chance or T.scaleLen(key) ~= 7 then return {} end
  local PHRYG = scaleNamed("Phrygian")
  local out, decided = {}, {}
  -- (A copy of an earlier chord - a Period's answer opening as its question
  -- did - does only what its original did, and its original did nothing if
  -- it was not before a V.)
  local first = {}
  for i, sl in ipairs(timeline) do
    if sl.origin and not first[sl.origin] then first[sl.origin] = sl end
  end
  for _, u in ipairs(plan.units) do
    -- (Two draws for every close, taken or not: Common keeps Rare's.)
    local closes = u.cad == "PAC" or u.cad == "IAC" or u.cad == "DC" or u.cad == "EC" or u.cad == "HC"
    local x1, x2 = 1, 0
    if closes then x1, x2 = rnd(), rnd() end
    local V = closes and u.slots and u.slots[#u.slots - ((u.cad == "HC") and 0 or 1)]
    local at
    for i, sl in ipairs(timeline) do if sl == V then at = i end end
    local pre = at and timeline[at - 1]
    -- (Where the tonic comes straight before the V, its second half may
    -- take the chromatic chord: i N6 V, as both books show it.)
    local split
    if pre and pre.degree == 0 then
      split = pre.s + snap(meter, (pre.e - pre.s) / 2)
      if not (pre.e - pre.s >= 2 * meter.beat and split > pre.s and split < pre.e) then split = nil end
    end
    -- (Not a chord an applied chord leads to: it must stay its target.)
    local led = pre and not split and timeline[at - 2] and timeline[at - 2].applied
    if pre and not led and at - 1 > 1 and V.degree == 4 and V.chord.quality == "major" and not V.spec and not V.moved
       and not V.applied and not V.borrowed and pre.e == V.s
       and (pre.degree == 1 or pre.degree == 3 or split) and not pre.spec and not pre.borrowed and not pre.applied
       and not pre.moved and not pre.truck and not pre.flavour and not pre.bassPc and (pre.key or key) == key then
      local origin = pre.origin
      local was = origin and decided[origin]
      if was == nil and origin and first[origin] ~= pre then was = false end
      local kinds, weights = {}, {}
      -- (Not the same root as the chord before - VI then the augmented
      -- sixth on le, ii then the Neapolitan's: the chord would seem to
      -- follow itself.)
      local before = not split and timeline[at - 2]
      for _, k in ipairs(CHROMATIC_KINDS) do
        local fits = (k == "N6" and PHRYG ~= nil) or (k ~= "N6" and aug6Key(key, k) ~= nil)
        -- (With Triads, three notes: the Neapolitan and the Italian sixth.)
        if r.colour == "Triads" and (k == "Fr+6" or k == "Ger+6" or k == "Sw+6") then fits = false end
        if before and before.degree == ((k == "N6") and 1 or 5) then fits = false end
        -- (The German sixth needs room for the I6/4: half of a V two beats
        -- long or more, on a beat.)
        -- (Not at a half close, whose V is its last chord: the tune comes
        -- home on it, not on the six-four.)
        if k == "Ger+6" or k == "Sw+6" then
          local cut = V.s + snap(meter, (V.e - V.s) / 2)
          fits = fits and V.e - V.s >= 2 * meter.beat and cut > V.s and cut < V.e and u.cad ~= "HC"
        end
        -- (The Swiss only in major keys: in minor, ri would be me, which
        -- `aug6Key` refuses - the scale would not climb.)
        local w = M.CHROMATIC_WEIGHT[k]
        if k == "Ger+6" and T.degreeQuality(key, 0) == "major" then w = 0.5 end
        if fits and (was == nil or was == k) then kinds[#kinds + 1] = k; weights[#weights + 1] = w end
      end
      local kind
      if was ~= nil then kind = was and kinds[1] or nil
      elseif x1 < chance and #kinds > 0 then kind = pickAt(x2, kinds, weights) end
      if kind and split then
        -- The tonic keeps the first half; the second is the new chord's.
        local tonic = {}
        for kk, v in pairs(pre) do tonic[kk] = v end
        tonic.e = split
        pre.s, pre.beat = split, split
        pre.origin = { of = pre.origin }
        table.insert(timeline, at - 1, tonic)
        for _, w in ipairs(plan.units) do
          for j, x in ipairs(w.slots or {}) do
            if x == pre then w.slots[j] = tonic; table.insert(w.slots, j + 1, pre); break end
          end
        end
        at = at + 1
      end
      if kind == "N6" then
        local k = T.key(key.root, PHRYG)
        pre.key, pre.degree = k, 1
        pre.chord = T.chord(k, 1, "Triads")
        pre.bassPc, pre.bassPos, pre.inversion = T.pc(k, 3), 3, 1
        pre.spec = { 1, from = PHRYG, bass = 3, chromatic = true }
      elseif kind then
        local k = aug6Key(key, kind)
        pre.key, pre.degree = k, 5
        pre.chord = aug6Chord(k, kind)
        pre.bassPc, pre.bassPos, pre.inversion = nil, nil, nil
        pre.spec = { 5, chromatic = true }
        if kind == "Ger+6" or kind == "Sw+6" then
          -- The cadential six-four: the tonic over sol, then V.
          local cut = V.s + snap(meter, (V.e - V.s) / 2)
          local six = { s = V.s, e = cut, beat = V.beat or V.s, degree = 0, key = key,
                        chord = T.chord(key, 0, "Triads"), spec = { 0, bass = 4, cadential = true } }
          six.bassPc, six.bassPos, six.inversion = T.pc(key, 4), 4, 2
          six.origin = six
          V.s, V.beat = cut, cut
          table.insert(timeline, at, six)
          for j, x in ipairs(u.slots) do
            if x == V then table.insert(u.slots, j, six); break end
          end
        end
      end
      if kind then
        -- (The chord before keeps its root in the bass: an inversion there
        -- would be judged against the chord that was.)
        local before = timeline[at - 2]
        if before then before.rootHeld = true end
        pre.chromatic = { kind = kind, name = pre.chord.name ..
                          (pre.bassPos and ("/" .. T.noteName(pre.key, pre.bassPos)) or ""),
                          numeral = kind }
        out[#out + 1] = pre
      end
      if origin and was == nil then decided[origin] = kind or false end
    end
  end
  return out
end

------------------------------------------------------------------------------
-- The passing diminished seventh (1.15)
--
-- Where the bass rises a whole tone from one chord to the next - IV to V,
-- I to ii, V to vi - the second half of the first chord may become the
-- diminished seventh on the note between: F F#dim7 G, the bass climbing by
-- semitones. It is the next chord's own leading-tone seventh (viio7/V), a
-- passing chord ("a passing chord ... will fill in the third with stepwise
-- motion" - Open Music Theory, "Harmonic syntax - prolongation"; Hutchinson's
-- I6 ii6 viio7/V I6/4). With Applied on; dice of their own. Only where the
-- next chord is major or minor and in root position, the chord split is two
-- beats or more and not a close's, and the next chord is the same each time
-- round.
------------------------------------------------------------------------------

M.PASSING_CHANCE = { Rare = 0.25, Common = 0.6 }

function M.passing(timeline, plan, key, r, rnd, meter)
  local chance = M.PASSING_CHANCE[r.applied]
  if not chance or T.scaleLen(key) ~= 7 or #timeline < 2 then return {} end
  local keep = cadenceSlots(plan, timeline)
  local home = {}
  for d = 0, 6 do home[T.pc(key, d)] = true end
  local nextOf, same = {}, {}
  for i, sl in ipairs(timeline) do
    local o = sl.origin or sl
    local nx = timeline[i + 1] and timeline[i + 1].degree or -1
    if nextOf[o] == nil then nextOf[o], same[o] = nx, true
    elseif nextOf[o] ~= nx then same[o] = false end
  end
  local out, decided = {}, {}
  local i = 1
  while i < #timeline do
    local sl, nx = timeline[i], timeline[i + 1]
    local x = rnd()
    local was = sl.origin and decided[sl.origin]
    local q = T.degreeQuality(key, nx.degree)
    local cut = sl.s + snap(meter, (sl.e - sl.s) / 2)
    local can = not keep[sl] and not sl.spec and not sl.borrowed and not sl.applied and not sl.moved
                and not sl.chromatic and not nx.spec and not nx.borrowed and not nx.applied and not nx.moved
                and not nx.truck and not nx.chromatic and ((nx.key or key) == key or nx.raised)
                and (q == "major" or q == "minor") and not nx.bassPc
                and (T.pc(key, nx.degree) - M.bassPcOf(sl)) % 12 == 2
                and sl.e - sl.s >= 2 * meter.beat and cut > sl.s and cut < sl.e and cut % meter.beat == 0
                and same[sl.origin or sl]
    local go
    if was ~= nil then go = was == nx.degree else go = x < chance end
    local done = false
    if go and can then
      local akey, deg = M.appliedKey(key, nx.degree, "vii")
      -- (With Triads, the diminished triad: three notes, as the Colour says.)
      local triads = r.colour == "Triads"
      local ch = akey and T.chord(akey, deg, triads and "Triads" or "Sevenths")
      local full = ch and #ch.pcs == (triads and 3 or 4)
      if full then
        for k = 2, #ch.pcs do if (ch.pcs[k] - ch.pcs[k - 1]) % 12 ~= 3 then full = false end end
      end
      local bent = false
      if full then for _, pc in ipairs(ch.pcs) do if not home[pc] then bent = true end end end
      if full and bent then
        local P = { s = cut, e = sl.e, beat = cut, degree = deg, key = akey, chord = ch,
                    spec = { deg, passing = true } }
        P.origin = P
        P.applied = { name = ch.name, numeral = (triads and "viio/" or "viio7/") .. T.degreeNumeral(key, nx.degree), kind = "vii",
                      target = nx.degree, passing = true, to = nx.chord.name }
        sl.e = cut
        sl.rootHeld, nx.rootHeld = true, true
        table.insert(timeline, i + 1, P)
        for _, u in ipairs(plan.units) do
          for j, y in ipairs(u.slots or {}) do
            if y == sl then table.insert(u.slots, j + 1, P); break end
          end
        end
        out[#out + 1] = P
        done = true
        i = i + 1
      end
    end
    if sl.origin and was == nil then decided[sl.origin] = done and nx.degree or false end
    i = i + 1
  end
  return out
end

------------------------------------------------------------------------------
-- The common-tone diminished seventh (1.16)
--
-- A diminished seventh that "progresses to a major triad or dominant
-- seventh chords whose root is the same as one of the notes of the o7
-- chord" - #ii°7 to I, #vi°7 to V - an embellishment: "the remaining three
-- notes of the diminished seventh chord resolve to the nearest chord tone,
-- but the diminished 7th of the chord remains unresolved as a common tone".
-- So a held I or V, three beats or more and not a close's, may be split I -
-- #ii°7 - I (V - #vi°7 - V): a neighbour chord, its common tone - the
-- chord's root - held in the bass. With Applied on, its own dice; not with
-- Triads (it is a seventh chord); a copy does what its original did.
------------------------------------------------------------------------------

M.COMMON_TONE_CHANCE = { Rare = 0.4, Common = 0.8 }

-- The scale the tune hears under it: the chord's own with its second, fourth
-- and sixth bent to a minor third, an augmented fourth and a major sixth
-- above the root. Nil where that takes more than a semitone.
local function commonToneKey(k, x)
  local iv = {}
  for i, v in ipairs(T.ivOf(k)) do iv[i] = v end
  local root = T.pc(k, x)
  local tonic = T.rootPc(k)
  for _, sp in ipairs({ { x + 1, 3 }, { x + 3, 6 }, { x + 5, 9 } }) do
    local d = sp[1] % 7
    local want = (root + sp[2] - tonic) % 12
    if iv[d + 1] ~= want then
      local diff = (want - iv[d + 1] + 6) % 12 - 6
      if math.abs(diff) ~= 1 then return nil end
      iv[d + 1] = want
    end
  end
  for i = 2, 7 do if iv[i] <= iv[i - 1] then return nil end end
  return { root = k.root, scale = k.scale, iv = iv, lift = k.lift }
end

function M.commonTone(timeline, plan, key, r, rnd, meter)
  local chance = M.COMMON_TONE_CHANCE[r.applied]
  if not chance or T.scaleLen(key) ~= 7 or r.colour == "Triads" then return {} end
  local keep = cadenceSlots(plan, timeline)
  local out, decided = {}, {}
  local first = {}
  for _, sl in ipairs(timeline) do
    if sl.origin and not first[sl.origin] then first[sl.origin] = sl end
  end
  local i = 1
  while i <= #timeline do
    local sl = timeline[i]
    local x = rnd()
    local origin = sl.origin
    local was = origin and decided[origin]
    if was == nil and origin and first[origin] ~= sl then was = false end
    -- (Counted from its beat: a chord pushed in early is split on the beats.)
    local from = sl.beat or sl.s
    local beats = (sl.e - from) // meter.beat
    local k = sl.key or key
    local can = (sl.degree == 0 or sl.degree == 4) and sl.chord.quality == "major" and not sl.bassPc
                and not keep[sl] and not sl.spec and not sl.borrowed and not sl.applied and not sl.chromatic
                and not sl.moved and not sl.truck and not sl.flavour
                and from % meter.beat == 0 and (sl.e - from) % meter.beat == 0 and beats >= 3
    local go
    if was ~= nil then go = was == true else go = x < chance end
    local done = false
    if go and can then
      local ck = commonToneKey(k, sl.degree)
      local ch = ck and T.chord(ck, sl.degree + 1, "Sevenths")
      local full = ch and #ch.pcs == 4 and ch.has[sl.chord.rootPc]
      if full then for j = 2, 4 do if (ch.pcs[j] - ch.pcs[j - 1]) % 12 ~= 3 then full = false end end end
      if full then
        local ctLen = math.max(1, beats // 4) * meter.beat
        local after = math.max(1, beats // 4) * meter.beat
        local a = sl.e - after - ctLen
        local C = { s = a, e = a + ctLen, beat = a, degree = (sl.degree + 1) % 7, key = ck, chord = ch,
                    spec = { (sl.degree + 1) % 7, commonTone = true } }
        C.bassPc, C.bassPos, C.inversion = sl.chord.rootPc, sl.degree, 3
        C.origin = C
        C.commonTone = { name = ch.name .. "/" .. T.noteName(ck, sl.degree),
                         numeral = (sl.degree == 0) and "#iio7" or "#vio7", over = sl.chord.name }
        local back = {}
        for kk, v in pairs(sl) do back[kk] = v end
        back.s, back.beat = a + ctLen, a + ctLen
        sl.e = a
        sl.rootHeld, back.rootHeld = true, true
        table.insert(timeline, i + 1, C)
        table.insert(timeline, i + 2, back)
        for _, u in ipairs(plan.units) do
          for j, y in ipairs(u.slots or {}) do
            if y == sl then table.insert(u.slots, j + 1, C); table.insert(u.slots, j + 2, back); break end
          end
        end
        out[#out + 1] = C
        done = true
        i = i + 2
      end
    end
    if origin and was == nil then decided[origin] = done end
    i = i + 1
  end
  return out
end

-- Now and then a chord takes another colour (`T.flavourChord`): about one
-- chord in five that may. (With Mixed until 1.14; now with every colour -
-- the diminished chord a third up, a seventh chord, not with Triads.)
M.FLAVOUR_CHANCE = { Rare = 0.2, Common = 0.5 }
local FLAVOUR_WEIGHT = { sus4 = 3, sus2 = 2, add2 = 1.5, add9 = 1.5, ["9"] = 2, ["6"] = 2, dim = 1.5 }

-- A chord copied from another (a repeat, an answer's first half, a Loop
-- going round) takes the flavour its original took, where it can, and
-- draws nothing: the music that comes round again sounds the same.
-- (1.12) A 6 becomes a 6/9 - the whole-tone ninth on top - half the time
-- the ninth is there, on dice of its own, so the flavours drawn before are
-- drawn as they were.
M.SIXNINE_CHANCE = 0.5

function M.flavour(timeline, plan, key, r, rnd, sixRnd)
  local chance = M.FLAVOUR_CHANCE[r.flavours]
  if not chance or T.scaleLen(key) ~= 7 then return end
  local keep = cadenceSlots(plan, timeline)
  local decided = {}
  -- (Nor a chord whose copy is a cadence's, where the copy could not follow
  -- it: a Period's answer opens as its question did.)
  local blocked = {}
  for _, sl in ipairs(timeline) do if keep[sl] and sl.origin then blocked[sl.origin] = true end end
  for i, sl in ipairs(timeline) do
    local was = sl.origin and decided[sl.origin]
    local go
    if was ~= nil then go = was ~= false
    else go = not keep[sl] and not sl.borrowed and not (sl.origin and blocked[sl.origin]) and rnd() < chance end
    -- (Nor the chord an applied chord leads to: it must stay the chord it
    -- is the dominant of.)
    local target = timeline[i - 1] and timeline[i - 1].applied
    -- (Nor the truck driver's V: the gear change is a dominant seventh. Nor
    -- a chromatic chord or the six-four a German sixth goes to, nor a chord
    -- whose root a passing or chromatic chord needs where it is, 1.15.)
    if go and not keep[sl] and not sl.borrowed and not sl.applied and not target and not sl.truck
       and not sl.chromatic and not (sl.spec and sl.spec.cadential) and not sl.rootHeld and not sl.commonTone then
      local seventh = false
      for _, pc in ipairs(sl.chord.pcs) do if T.roleOf(sl.chord, pc) == "7" then seventh = true end end
      local before, after = timeline[i - 1], timeline[i + 1]
      local cands, weights = {}, {}
      for _, f in ipairs(T.FLAVOURS) do
        local ch = T.flavourChord(sl.key or key, sl.degree, f, seventh)
        if f == "dim" and r.colour == "Triads" then ch = nil end
        -- (Not one that drops a named progression's bass note: no sus4 over
        -- Pachelbel's G/B.)
        if ch and sl.bassPc then
          local has = false
          for _, pc in ipairs(ch.pcs) do if pc == sl.bassPc then has = true end end
          if not has then ch = nil end
        end
        -- (Not one that sounds like the chord either side of it. An add9 has
        -- the notes and the name of Mixed's own; it changes the voicing,
        -- putting its ninth on top.)
        -- (Nor one whose bass is the chord before's, on the same degree - a
        -- Romanesca's I6 then I as D#m7b5 over D#, 1.16: the same chord on
        -- the same bass twice.)
        if ch and before and before.degree == sl.degree and M.bassPcOf(before) == (sl.bassPc or ch.rootPc) then ch = nil end
        if ch and after and after.degree == sl.degree and M.bassPcOf(after) == (sl.bassPc or ch.rootPc) then ch = nil end
        if ch and (ch.name ~= sl.chord.name or f == "add9") and not (before and before.chord.name == ch.name)
           and not (after and after.chord.name == ch.name) and (was == nil or was == f or (was == "6/9" and f == "6")) then
          cands[#cands + 1] = ch
          weights[#weights + 1] = FLAVOUR_WEIGHT[f]
        end
      end
      if #cands > 0 then
        sl.chord = (was ~= nil) and cands[1] or weighted(rnd, cands, weights)
        sl.flavour = sl.chord.flavour
        if sl.flavour == "6" then
          local up = T.flavourChord(sl.key or key, sl.degree, "6/9", false)
          local go
          if was ~= nil then go = was == "6/9" else go = sixRnd and sixRnd() < M.SIXNINE_CHANCE end
          if up and go and not (sl.bassPc and not up.has[sl.bassPc]) then
            sl.chord, sl.flavour = up, "6/9"
          end
        end
      end
    end
    if sl.origin and was == nil then decided[sl.origin] = sl.flavour or false end
  end
end

-- The note in the bass under a chord: its root, or the note it is inverted on.
local function bassPcOf(sl) return sl.bassPc or sl.chord.rootPc end
M.bassPcOf = bassPcOf

local function byStep(a, b)
  local d = (a - b) % 12
  return d == 1 or d == 2 or d == 10 or d == 11
end

-- Inversions where they do a job (Open Music Theory, "Harmonic syntax -
-- prolongation"): the bass moving by step.
--
--   first   the third in the bass, where the bass steps into it or out of
--           it - C G/B Am, F C/E Dm - likeliest when it does both (a
--           passing chord)
--   second  the fifth in the bass only where a 6/4 belongs: over a held
--           bass (I IV6/4 I), passing between two steps (I V6/4 I6), or
--           the tonic's fifth before the dominant at a cadence (I6/4 V)
--   third   the seventh in the bass, only where it can fall a step into
--           the next chord, which then takes that note in its bass
--           (V4/2 I6)
--
-- About one chord in four that could be inverted is (more than half with
-- Common); never two running (but for the chord a third inversion resolves
-- to), never a flavoured one.
--
-- Checked against the textbooks' rules for six-fours (Open Music Theory;
-- Puget Sound's Music Theory for the 21st-Century Classroom): a cadential
-- 6/4 comes at a cadence, right before its V, on a stronger beat; a passing
-- 6/4 walks the bass through three notes one way and a pedal 6/4 holds it,
-- both on a weaker beat, between two chords of the same function. And a
-- diminished triad is most at home in first inversion (vii6): in root
-- position its fifth is a tritone over the bass. So a diminished triad is
-- inverted three times in four where its bass steps, and never to a 6/4.
M.INVERT_CHANCE = { Rare = 0.25, Common = 0.6 }
M.DIM_FIRST = 0.75

-- How strong a beat is, with the odd bars of a pair stronger than the even
-- (so a chord a bar still has strong and weak places).
local function weightAt(meter, step)
  local w = M.strength(meter, step)
  if step % meter.bar == 0 and (step // meter.bar) % 2 == 0 then w = w + 0.5 end
  return w
end
M.weightAt = weightAt
local function beatAt(sl) return sl.beat or sl.s end

-- As with flavours, a chord copied from another is inverted as its original
-- was, where the bass around it still allows it, and draws nothing.
function M.invert(timeline, plan, key, r, rnd, meter)
  local chance = M.INVERT_CHANCE[r.inversions]
  if not chance or #timeline < 3 then return end
  local keep = cadenceSlots(plan, timeline)
  local decided = {}
  local i = 2
  while i <= #timeline - 1 do
    local sl, before, after = timeline[i], timeline[i - 1], timeline[i + 1]
    local step = 1
    local was = sl.origin and decided[sl.origin]
    local dimTriad = sl.chord.quality == "diminished" and #sl.chord.pcs == 3 and not sl.flavour
    local go
    if was ~= nil then go = was ~= false
    else
      local x = rnd()
      go = x < chance or (dimTriad and x < M.DIM_FIRST)
    end
    if sl.inversion then go = false end
    -- (Nor either side of a passing diminished seventh, 1.15: the bass must
    -- climb by semitones through it.)
    if go and not keep[sl] and not sl.flavour and not sl.applied and not sl.spec and not sl.rootHeld then
      local ch = sl.chord
      local pb, nb = bassPcOf(before), after.chord.rootPc
      local k = sl.key or key
      local sameFunction = T.functionOf(k, before.degree) == T.functionOf(k, after.degree)
      local weaker = weightAt(meter, beatAt(sl)) < weightAt(meter, beatAt(before))
      local opts, weights = {}, {}
      for idx, pc in ipairs(ch.pcs) do
        local role = T.roleOf(ch, pc)
        if role == "3" and (byStep(pb, pc) or byStep(pc, nb)) then
          opts[#opts + 1] = { idx = idx, inv = 1 }
          weights[#weights + 1] = (byStep(pb, pc) and byStep(pc, nb)) and 3 or 1
        elseif role == "5" and not dimTriad then
          local up = (pc - pb) % 12
          local on = (nb - pc) % 12
          local oneWay = (up >= 1 and up <= 2 and on >= 1 and on <= 2) or (up >= 10 and on >= 10)
          local pedal = pc == pb and pc == nb and sameFunction and weaker
          local passing = oneWay and sameFunction and weaker
          local cadential = sl.degree == 0 and keep[after] and T.rootAbove(k, after.degree) == 7
                            and weightAt(meter, beatAt(sl)) > weightAt(meter, beatAt(after))
          if pedal or passing or cadential then
            opts[#opts + 1] = { idx = idx, inv = 2 }
            weights[#weights + 1] = 1
          end
        elseif role == "7" and not keep[after] and not after.flavour and not after.rootHeld then
          -- (Onto the next chord's root or third - V4/2 to I6 - never its
          -- seventh, which would want resolving in turn.)
          for _, t in ipairs(after.chord.pcs) do
            local d = (pc - t) % 12
            local role = T.roleOf(after.chord, t)
            if (d == 1 or d == 2) and (role == "R" or role == "3") then
              opts[#opts + 1] = { idx = idx, inv = 3, to = t }
              weights[#weights + 1] = 1
              break
            end
          end
        end
      end
      -- (Never the same chord on the same bass as the one beside it - a
      -- named V6/5 next to it, say: 1.13.)
      do
        local keepO, keepW = {}, {}
        for k, o in ipairs(opts) do
          local b = ch.pcs[o.idx]
          local twin = (before and before.degree == sl.degree and bassPcOf(before) == b)
                       or (after and after.degree == sl.degree and bassPcOf(after) == b)
          if not twin then keepO[#keepO + 1] = o; keepW[#keepW + 1] = weights[k] end
        end
        opts, weights = keepO, keepW
      end
      if was ~= nil then
        local same = {}
        for _, o in ipairs(opts) do if o.inv == was then same[1] = o; break end end
        opts, weights = same, { 1 }
      end
      if #opts > 0 then
        local o = (was ~= nil) and opts[1] or weighted(rnd, opts, weights)
        sl.bassPc, sl.bassPos, sl.inversion = ch.pcs[o.idx], ch.pos[o.idx], o.inv
        if o.to then
          for idx, t in ipairs(after.chord.pcs) do
            if t == o.to and t ~= after.chord.rootPc then
              after.bassPc, after.bassPos = t, after.chord.pos[idx]
              after.inversion = ({ ["3"] = 1, ["5"] = 2, ["7"] = 3 })[T.roleOf(after.chord, t)]
            end
          end
        end
        step = 2
      end
    end
    if sl.origin and was == nil then decided[sl.origin] = sl.inversion or false end
    i = i + step
  end
end

------------------------------------------------------------------------------
-- 4. Rhythm
--
-- Two kinds of maths, one for each groove:
--
--   - Straight takes the k strongest steps of the bar (the metric hierarchy:
--     the downbeat, the half bar, the beats, then the half beats).
--   - Syncopated spreads k notes as evenly as they will go over the bar (a
--     Euclidean rhythm - Toussaint showed most of the world's rhythms are
--     these: 3 in 8 is the tresillo, 5 in 8 the cinquillo) and turns it so
--     the notes fall off the beat, keeping the first on the downbeat.
--
-- Pace decides the grid (eighths or sixteenths) and how full it is.
------------------------------------------------------------------------------

-- k onsets spread over n steps as evenly as they go (Bresenham's line; the
-- same patterns as Bjorklund's algorithm, up to rotation). Starts on 0.
function M.euclid(k, n)
  local out = {}
  if n <= 0 then return out end
  k = math.max(0, math.min(n, k))
  for i = 0, n - 1 do
    if (i * k) % n < k then out[#out + 1] = i end
  end
  return out
end

local function meanStrength(meter, base, unit, slots)
  local s = 0
  for _, o in ipairs(slots) do s = s + M.strength(meter, base + o * unit) end
  return s / math.max(1, #slots)
end

-- The k strongest of `slots` grid points, ties broken by the dice.
local function strongest(meter, base, unit, slots, k, rnd)
  local order = {}
  for i = 0, slots - 1 do order[#order + 1] = { i = i, w = M.strength(meter, base + i * unit) + rnd() * 0.5 } end
  table.sort(order, function(a, b) return a.w > b.w end)
  local out = {}
  for j = 1, math.min(k, #order) do out[j] = order[j].i end
  table.sort(out)
  if out[1] ~= 0 then
    -- The downbeat always speaks.
    out[#out] = nil
    table.insert(out, 1, 0)
    table.sort(out)
  end
  return out
end

-- A Euclidean rhythm turned to start on one of its own notes, the turn
-- chosen to sit off the beat.
local function syncopated(meter, base, unit, slots, k, rnd)
  local pat = M.euclid(k, slots)
  local turns = {}
  for _, o in ipairs(pat) do
    local t = {}
    for _, x in ipairs(pat) do t[#t + 1] = (x - o) % slots end
    table.sort(t)
    turns[#turns + 1] = { t = t, s = meanStrength(meter, base, unit, t) }
  end
  -- Ties are left to the sort as 1.0 left them: the list is never longer
  -- than a bar's sixteen steps, and Lua only varies its sort past a hundred
  -- items, so the same turns always come out in the same order.
  table.sort(turns, function(a, b) return a.s < b.s end)
  -- The most off-beat turn, or the one after it.
  return turns[(#turns > 1 and coin(rnd, 0.35)) and 2 or 1].t
end

M.PACE = {
  Calm    = { unit = 2, lo = 0.2,  hi = 0.4,  halves = false },
  Flowing = { unit = 2, lo = 0.45, hi = 0.75, halves = true },
  Busy    = { unit = 1, lo = 0.4,  hi = 0.65, halves = true },
}

------------------------------------------------------------------------------
-- Figures: dotted and triplet rhythms
--
-- Laid over a rhythm after it is made, a beat (or a pair of beats) at a
-- time, by chance:
--
--   - two eighths in a beat become a dotted eighth and a sixteenth;
--     two quarters in two beats, a dotted quarter and an eighth;
--   - a beat with two or more notes becomes an eighth-note triplet; two
--     quarters in two beats, a quarter-note triplet;
--   - in the tune, a quarter note on the beat (with a note on the beat
--     after) becomes a dotted eighth and a sixteenth, or an eighth-note
--     triplet; a half note, a dotted quarter and an eighth, or a
--     quarter-note triplet. A tune at an easy pace is mostly quarters and
--     halves, which the shapes above never touch, so without this the
--     chords took the figures and the tune hardly did (since 1.4).
--
-- Triplet notes fall between the sixteenths, so their steps are fractions
-- (a third of a beat is 4/3 of a step). Only in metres whose beat is a
-- quarter note: 6/8 and 12/8 are already in threes, and 7/8 has no beats to
-- divide. With Plain nothing is drawn, so a 1.0 idea is unchanged.
------------------------------------------------------------------------------

local FIGURES = {
  Dotted   = { dot = 0.5,  tri = 0,    qdot = 0.45, qtri = 0 },
  Triplets = { dot = 0,    tri = 0.45, qdot = 0,    qtri = 0.4 },
  Mixed    = { dot = 0.25, tri = 0.2,  qdot = 0.22, qtri = 0.18 },
}

-- `onsets` are steps from the start of a cell `len` long that begins
-- `base` steps into the bar. `tune`: a melody's rhythm, whose quarter
-- notes take figures too.
function M.figure(meter, base, len, onsets, figures, rnd, tune)
  local F = FIGURES[figures]
  if not F or meter.beat ~= 4 then return onsets end
  local function within(a, b)
    local o = {}
    for _, x in ipairs(onsets) do if x >= a and x < b then o[#o + 1] = x end end
    return o
  end
  local function has(x)
    for _, y in ipairs(onsets) do if y == x then return true end end
    return false
  end
  local b = (4 - base % 4) % 4
  local out = within(0, b)
  local function add(...) for _, x in ipairs({ ... }) do out[#out + 1] = x end end
  while b < len do
    local step = 4
    local pair = (b + 8 <= len) and within(b, b + 8) or {}
    if #pair == 2 and pair[1] == b and pair[2] == b + 4 then
      local x = rnd()
      if x < F.tri / 2 then add(b, b + 8 / 3, b + 16 / 3); step = 8
      elseif x < F.tri / 2 + F.dot then add(b, b + 6); step = 8 end
    elseif tune and #pair == 1 and pair[1] == b and b + 8 < len and has(b + 8) then
      -- A half note in the tune.
      local x = rnd()
      if x < F.qtri then add(b, b + 8 / 3, b + 16 / 3)
      elseif x < F.qtri + F.qdot then add(b, b + 6)
      else add(b) end
      step = 8
    end
    if step == 4 then
      local beat = within(b, math.min(len, b + 4))
      if b + 4 <= len and #beat >= 2 and beat[1] == b then
        local x = rnd()
        if x < F.tri then add(b, b + 4 / 3, b + 8 / 3)
        elseif #beat == 2 and beat[2] == b + 2 and x < F.tri + F.dot then add(b, b + 3)
        else add(table.unpack(beat)) end
      elseif tune and b + 4 < len and #beat == 1 and beat[1] == b and has(b + 4) then
        local x = rnd()
        if x < F.qtri then add(b, b + 4 / 3, b + 8 / 3)
        elseif x < F.qtri + F.qdot then add(b, b + 3)
        else add(b) end
      else
        add(table.unpack(beat))
      end
    end
    b = b + step
  end
  table.sort(out)
  return out
end

-- The onsets of a cell `len` steps long starting at `base` in the bar, in
-- steps from the start of the cell.
-- The named rhythms (1.11; docs/decisions/0023-chord-styles-and-named-rhythms.md): steps in a bar of 4/4,
-- as Toussaint writes them over sixteen pulses. Elsewhere they play as
-- Syncopated.
M.RHYTHMS = {
  Tresillo = { 0, 6, 12 },
  Habanera = { 0, 6, 8, 12 },
  Clave = { 0, 3, 6, 10, 12 },
  ["3+3+3+3+2+2"] = { 0, 3, 6, 9, 12, 14 },
}
-- The named rhythm a groove plays in this metre, or nil.
function M.rhythmOf(meter, groove)
  local t = M.RHYTHMS[groove]
  if t and meter.bar == 16 and meter.beat == 4 then return t end
end
-- What a named rhythm is to the tune (and elsewhere): syncopated.
local function plainGroove(groove)
  return M.RHYTHMS[groove] and "Syncopated" or groove
end
M.plainGroove = plainGroove

function M.cell(meter, base, len, pace, groove, rnd, figures)
  groove = plainGroove(groove)
  local P = M.PACE[pace] or M.PACE.Flowing
  local pieces = { { 0, len } }
  -- Busy and straight rhythms are made a half bar at a time, for variety;
  -- a syncopation needs the whole bar to spread over (3 in 8 is a tresillo,
  -- 3 in 4 twice is not).
  if P.halves and (groove ~= "Syncopated" or pace == "Busy") and len >= 2 * meter.beat then
    local h = snap(meter, len / 2)
    if h > 0 and h < len then pieces = { { 0, h }, { h, len } } end
  end
  local out = {}
  for _, pc in ipairs(pieces) do
    local slots = (pc[2] - pc[1]) // P.unit
    if slots >= 1 then
      local k = math.max(1, math.min(slots, round(slots * between(rnd, P.lo, P.hi))))
      local pat
      if groove == "Syncopated" and k > 1 and k < slots then
        pat = syncopated(meter, base + pc[1], P.unit, slots, k, rnd)
      else
        pat = strongest(meter, base + pc[1], P.unit, slots, k, rnd)
      end
      for _, o in ipairs(pat) do out[#out + 1] = pc[1] + o * P.unit end
    end
  end
  return M.figure(meter, base, len, out, figures, rnd, true)
end

-- The rhythm of a whole unit: a cell a bar, the first bar's cell often
-- coming back, so the unit has a rhythm of its own. A unit that closes holds
-- its last note from the half bar.
function M.unitRhythm(u, meter, r, rnd)
  local out = {}
  local cellLen = math.min(meter.bar, u.len)
  local first
  local at = 0
  while at < u.len do
    local len = math.min(cellLen, u.len - at)
    local cell
    if first and len == cellLen and coin(rnd, r.pace == "Busy" and 0.5 or 0.6) then cell = first
    else cell = M.cell(meter, (u.start + at) % meter.bar, len, r.pace, r.groove, rnd, r.figures) end
    first = first or cell
    for _, o in ipairs(cell) do out[#out + 1] = at + o end
    at = at + len
  end
  if u.cad ~= "none" and u.cad ~= "open" then
    -- The last note lands half way through the last bar, or where the last
    -- chord arrives if that is later: the tune comes home with the harmony.
    local region = math.min(meter.bar, u.len)
    local from = u.len - region
    local h = from + snap(meter, region / 2)
    if h >= u.len then h = from end
    if u.rel and u.rel[#u.rel].s > h and u.rel[#u.rel].s < u.len then h = u.rel[#u.rel].s end
    local kept = {}
    for _, o in ipairs(out) do if o < h then kept[#kept + 1] = o end end
    kept[#kept + 1] = h
    out = kept
  end
  table.sort(out)
  -- An ending is a note to land on, not the middle of a triplet: a unit
  -- with a cadence (open ones too) ends on a step of its own.
  if u.cad ~= "none" then
    while #out > 1 and M.offGrid(out[#out]) do out[#out] = nil end
  end
  return out
end

------------------------------------------------------------------------------
-- 5. Melody
--
-- A walk through scale positions. Each next note is drawn from the notes up
-- to a sixth either side, weighted:
--
--   - by size: a step is most likely, a third about half as likely, leaps
--     rare (the shape of real melodies: mostly steps);
--   - toward the contour, a bell curve around where the contour is now;
--   - on the beat, only chord tones;
--   - after a leap of a fourth or more, only a step or a third back the
--     other way (the gap is filled);
--   - a note off the chord moves on by step (a passing or neighbour note);
--   - no tritone leaps, no augmented seconds, no note three times running,
--     and a step back to the note before last only now and then (once is a
--     neighbour note; again and again is a trill).
--
-- A unit that ends on a cadence ends on its goal - the tonic for a full
-- close, the third or fifth for an imperfect one, a note of the dominant for
-- a half close - and the note before it is pulled to a step away.
------------------------------------------------------------------------------

local IV_WEIGHT = { [0] = 0.15, [1] = 1, [2] = 0.6, [3] = 0.22, [4] = 0.18, [5] = 0.07 }
local PULL = 2.3   -- how far from the contour a note wanders, in scale steps

local CONTOURS = {
  Arch = function(t)
    local g = 0.618
    if t < g then return 0.2 + 0.8 * math.sin(math.pi / 2 * t / g) end
    return 0.15 + 0.85 * math.cos(math.pi / 2 * (t - g) / (1 - g))
  end,
  Valley = function(t)
    local g = 0.618
    if t < g then return 0.8 - 0.8 * math.sin(math.pi / 2 * t / g) end
    return 0.85 * math.sin(math.pi / 2 * (t - g) / (1 - g))
  end,
  Rise = function(t) return 0.1 + 0.8 * t end,
  Fall = function(t) return 0.9 - 0.8 * t end,
  Wave = function(t) return 0.5 + 0.4 * math.sin(2 * math.pi * 1.5 * t) end,
}
M.CONTOURS = CONTOURS

local REGISTER = { Low = 55, Middle = 67, High = 76 }

-- The positions a tune may use: about a ninth, centred on the fifth above
-- the tonic nearest the register. Counted from the tonic rather than from a
-- pitch, so the same idea in another key is the same tune, moved.
function M.melodyRange(key, register)
  local n = T.scaleLen(key)
  local want = (REGISTER[register] or 67) - 7
  local tonic = math.floor((want - T.rootPc(key)) / 12 + 0.5) * n
  local centre = tonic + round(n * 4 / 7)
  local span = n + 2
  local lo = centre - span // 2
  return lo, lo + span
end

-- `key` everywhere below is the scale sounding at that moment
-- (`M.keyAt`): the key's own, or a borrowed chord's.
local function nearestOn(ctx, key, ch, target, avoid)
  local best
  for p = ctx.lo, ctx.hi do
    if T.onChord(key, ch, p) and p ~= avoid then
      if not best or math.abs(p - target) < math.abs(best - target) then best = p end
    end
  end
  return best or math.max(ctx.lo, math.min(ctx.hi, round(target)))
end

-- (1.13) Do three pitches outline a consonant triad - major or minor, in
-- any position and spacing (E C G is C major)? "No consecutive leaps in the
-- same direction" but where they do (Open Music Theory, "Composing a cantus
-- firmus").
function M.outlinesTriad(a, b, c)
  local pcs, seen = {}, {}
  for _, x in ipairs({ a, b, c }) do
    local pc = x % 12
    if not seen[pc] then seen[pc] = true; pcs[#pcs + 1] = pc end
  end
  if #pcs ~= 3 then return false end
  for _, root in ipairs(pcs) do
    for _, third in ipairs({ 3, 4 }) do
      if seen[(root + third) % 12] and seen[(root + 7) % 12] then return true end
    end
  end
  return false
end

-- Three notes running (each { pos, pitch, first }): two leaps the same way
-- that do not outline a triad? Not across a statement's start.
function M.leapsBad(x, y, z)
  if not (x and y and z) or y.first or z.first then return false end
  local i1, i2 = y.pos - x.pos, z.pos - y.pos
  return math.abs(i1) >= 2 and math.abs(i2) >= 2 and i1 * i2 > 0 and not M.outlinesTriad(x.pitch, y.pitch, z.pitch)
end

local function choose(ctx, key, prev, prevIv, prevNct, reps, target, ch, strong, rnd, goal, before)
  for relax = 0, 2 do
    local cands, ws = {}, {}
    for iv = -5, 5 do
      local p = prev + iv
      if p >= ctx.lo and p <= ctx.hi then
        local w = IV_WEIGHT[math.abs(iv)]
        local on = T.onChord(key, ch, p)
        if strong and not on then w = 0 end
        if relax < 2 and prevIv and math.abs(prevIv) >= 3 and (iv * prevIv >= 0 or math.abs(iv) > 2) then w = 0 end
        -- (A leap after a leap the same way only if the three outline a triad.)
        if relax < 2 and prevIv and before and math.abs(prevIv) >= 2 and math.abs(iv) >= 2 and iv * prevIv > 0
           and not M.outlinesTriad(T.pitch(key, before), T.pitch(key, prev), T.pitch(key, p)) then w = 0 end
        if relax < 1 and prevNct and math.abs(iv) ~= 1 then w = 0 end
        if iv == 0 and reps >= 1 then w = 0 end
        if math.abs(iv) >= 2 and not on then w = w * 0.15 end
        if before and p == before and math.abs(iv) <= 2 then w = w * 0.25 end
        local semis = math.abs(T.pitch(key, p) - T.pitch(key, prev))
        if semis == 6 then w = 0 end
        if math.abs(iv) == 1 and semis == 3 then w = 0 end
        local d = p - target
        w = w * math.exp(-(d * d) / (2 * PULL * PULL))
        if goal then
          if math.abs(T.pitch(key, p) - T.pitch(key, goal)) == 6 then w = 0 end
          local g = math.abs(p - goal)
          w = w * ((g == 1 and 4) or (g == 2 and 1.5) or (g == 0 and 0.3) or 0.2)
        end
        if w > 0 then cands[#cands + 1] = p; ws[#ws + 1] = w end
      end
    end
    if #cands > 0 then return weighted(rnd, cands, ws) end
  end
  return nearestOn(ctx, key, ch, target)
end

-- Where a closing unit lands: its last note, at `step`.
local function goalFor(ctx, u, prev, target, step)
  local key = M.keyAt(ctx, step)
  local n = T.scaleLen(key)
  local ch = M.chordAt(ctx.timeline, step).chord
  -- (The tonic of the key sounding there: after a key change, 1.12, the new
  -- one's.)
  local tonic = T.chord(key, 0, "Triads")
  local ok
  -- (A close whose chord is not the tonic - the blues, which plays its own
  -- changes bar by bar - lands on a note of the chord it has.)
  local home = ch.rootPc == tonic.rootPc
  if u.cad == "PAC" and home then ok = function(p) return p % n == 0 end
  elseif u.cad == "IAC" and home then ok = function(p) return T.onChord(key, tonic, p) and p % n ~= 0 end
  elseif u.cad == "HC" or u.cad == "PAC" or u.cad == "IAC" then ok = function(p) return T.onChord(key, ch, p) end
  elseif u.cad == "EC" then
    -- An evaded close (1.13): the leading note does not resolve - the tune
    -- leaps up instead, to a note of the chord that is not do.
    local best, bestCost
    for p = ctx.lo - 2, ctx.hi + 2 do
      if T.onChord(key, ch, p) and p % n ~= 0 then
        local up = T.pitch(key, p) - T.pitch(key, prev)
        local cost = math.abs(p - target) * 0.4 + ((up >= 5 and up <= 9) and 0 or (up > 9 and up <= 12) and 3 or 40)
        if up ~= 6 and (not bestCost or cost < bestCost) then best, bestCost = p, cost end
      end
    end
    return best
  elseif u.cad == "DC" then
    -- The tune lands where the tonic was due - do, which vi also has - and
    -- the harmony goes elsewhere under it.
    ok = function(p) return T.onChord(key, ch, p) end
    local okBase = ok
    local best, bestCost
    for p = ctx.lo - 2, ctx.hi + 2 do
      if okBase(p) then
        local cost = math.abs(p - prev) + 0.4 * math.abs(p - target) + ((p % n == 0) and 0 or 3)
        if not bestCost or cost < bestCost then best, bestCost = p, cost end
      end
    end
    return best
  elseif u.cad == "open" then
    -- Back toward where the idea started, without landing on it, so it
    -- loops.
    target = ctx.firstPos or target
    ok = function(p) return T.onChord(key, ch, p) and p ~= ctx.firstPos end
  else return nil end
  local best, bestCost
  for p = ctx.lo - 2, ctx.hi + 2 do
    if ok(p) then
      local cost = math.abs(p - prev) + 0.4 * math.abs(p - target)
      if not bestCost or cost < bestCost then best, bestCost = p, cost end
    end
  end
  return best
end

-- New notes for a unit at the given onsets (steps from the unit's start).
-- (1.16) A named schema's tune: the stage a step falls in, if its chord
-- names a degree to sing there (`spec.sing`, a degree or a list of two for
-- a chord holding two stages), as { at = where the stage starts, degree }.
local function stageAt(ctx, sl, step)
  local sing = sl.spec and sl.spec.sing
  if not sing then return nil end
  if type(sing) == "number" then return { at = sl.s, degree = sing, key = sl } end
  local mid = sl.s + snap(ctx.meter, (sl.e - sl.s) / 2)
  if step < mid or mid <= sl.s then return { at = sl.s, degree = sing[1], key = sl } end
  return { at = mid, degree = sing[2], key = sl, second = true }
end

-- The note a stage sings: that degree, a note of the chord, the nearest to
-- the note before (within a fifth, no tritone, no augmented second, not a
-- third time), or nil.
local function singNote(ctx, key, ch, prev, reps, degree, target)
  local want = T.pc(key, degree)
  local best, bestCost
  for p = ctx.lo, ctx.hi do
    if T.pc(key, p) == want and T.onChord(key, ch, p) then
      local ok = true
      if prev then
        local iv = p - prev
        local semis = math.abs(T.pitch(key, p) - T.pitch(key, prev))
        if math.abs(iv) > 4 or semis == 6 or (math.abs(iv) == 1 and semis == 3) or (iv == 0 and reps >= 1) then ok = false end
      end
      if ok then
        local cost = (prev and math.abs(p - prev) or 0) * 10 + math.abs(p - target)
        if not bestCost or cost < bestCost then best, bestCost = p, cost end
      end
    end
  end
  return best
end

local function walkUnit(ctx, u, onsets, rnd, state)
  local out = {}
  state.sung = state.sung or {}
  local reps = state.reps or 0
  local closing = u.cad ~= "none"
  for i, o in ipairs(onsets) do
    local step = u.start + o
    local sl = M.chordAt(ctx.timeline, step)
    local ch = sl.chord
    local key = sl.key or ctx.key
    local strong = M.strength(ctx.meter, step) >= 2 or i == 1
    local target = ctx.target(step)
    local p
    -- (1.16) The first note of a schema's stage sings its degree.
    local stage = stageAt(ctx, sl, step)
    local sing
    if stage and step >= stage.at and not (closing and i >= #onsets - 1) then
      local id = tostring(sl) .. (stage.second and "b" or "a")
      if not state.sung[id] then
        state.sung[id] = true
        sing = singNote(ctx, key, ch, state.prev, reps, stage.degree, target)
      end
    end
    if sing then
      p = sing
      if not state.prev then ctx.firstPos = p end
    elseif not state.prev then
      p = nearestOn(ctx, key, ch, target)
      ctx.firstPos = p
    elseif closing and i == #onsets then
      p = goalFor(ctx, u, state.prev, target, step) or choose(ctx, key, state.prev, state.iv, state.nct, reps, target, ch, true, rnd)
    elseif closing and i == #onsets - 1 then
      local goal = goalFor(ctx, u, state.prev, target, u.start + onsets[#onsets])
      p = choose(ctx, key, state.prev, state.iv, state.nct, reps, goal or target, ch, strong, rnd, goal, state.before)
    else
      p = choose(ctx, key, state.prev, state.iv, state.nct, reps, target, ch, strong, rnd, nil, state.before)
    end
    state.before = state.prev
    if state.prev then
      state.iv = p - state.prev
      reps = (p == state.prev) and reps + 1 or 0
      state.reps = reps
    end
    state.prev = p
    state.nct = not T.onChord(key, ch, p)
    out[#out + 1] = { at = o, pos = p }
  end
  return out
end

-- Notes copied from another unit, moved to this one's start, shifted by
-- `shift` scale steps.
local function copyNotes(src, from, to, shift)
  local out = {}
  for _, nt in ipairs(src.notes) do
    if nt.at >= from and nt.at < to then out[#out + 1] = { at = nt.at - from, pos = nt.pos + (shift or 0) } end
  end
  return out
end

-- A statement moved onto new chords keeps its shape: the notes on the beat
-- that are not on the chord now go to the nearest chord tone, the rest stay.
-- With `ending`, the last note counts as on the beat too: it is the
-- unit's ending (an exact repeat of a unit that closes).
local function fit(ctx, u, notes, ending)
  local sung = {}
  for i, nt in ipairs(notes) do
    local step = u.start + nt.at
    -- (1.16) A copied tune sings a schema's stage too: its first note there
    -- moves to the stage's degree, the nearest within a fifth - but for a
    -- close's last two notes, which go where the close goes.
    local sl = M.chordAt(ctx.timeline, step)
    local stage = stageAt(ctx, sl, step)
    if stage and step >= stage.at and not (ending and u.cad ~= "none" and i >= #notes - 1) then
      local id = tostring(sl) .. (stage.second and "b" or "a")
      if not sung[id] then
        sung[id] = true
        local key = M.keyAt(ctx, step)
        local want = T.pc(key, stage.degree)
        if T.pc(key, nt.pos) ~= want then
          for d = 1, 4 do
            local hit
            for _, q in ipairs({ nt.pos - d, nt.pos + d }) do
              if not hit and q >= ctx.lo and q <= ctx.hi and T.pc(key, q) == want and T.onChord(key, sl.chord, q) then hit = q end
            end
            if hit then nt.pos = hit; break end
          end
        end
      end
    end
    if M.strength(ctx.meter, step) >= 2 or (ending and i == #notes and u.cad ~= "none") then
      local ch = M.chordAt(ctx.timeline, step).chord
      local key = M.keyAt(ctx, step)
      if not T.onChord(key, ch, nt.pos) then
        -- (Within an octave and more either way; a chord always has a note
        -- there in its own scale, but a search must end.)
        local up, down = nt.pos + 1, nt.pos - 1
        while not T.onChord(key, ch, up) and up < nt.pos + 15 do up = up + 1 end
        while not T.onChord(key, ch, down) and down > nt.pos - 15 do down = down - 1 end
        if T.onChord(key, ch, up) or T.onChord(key, ch, down) then
          nt.pos = (T.onChord(key, ch, up) and (not T.onChord(key, ch, down) or up - nt.pos <= nt.pos - down)) and up or down
        end
      end
    end
  end
  return notes
end

-- The shift (of `shift` or an octave either side of it) that keeps a
-- statement in range and nearest the note before.
local function bestShift(ctx, src, from, to, shift, prev)
  local n = T.scaleLen(ctx.key)
  local best, bestCost
  for _, s in ipairs({ shift, shift - n, shift + n }) do
    local notes = copyNotes(src, from, to, s)
    if #notes > 0 then
      -- Out of range costs by how far out, so if every choice is out, the
      -- least out wins.
      local over = 0
      for _, nt in ipairs(notes) do
        over = math.max(over, ctx.lo - 1 - nt.pos, nt.pos - ctx.hi - 1)
      end
      local cost = over * 100 + (prev and math.abs(notes[1].pos - prev) or 0)
      if not bestCost or cost < bestCost then best, bestCost = s, cost end
    end
  end
  return best or shift
end

function M.melody(plan, ctx, r, rnd)
  local state = {}
  local units = plan.units
  for _, u in ipairs(units) do
    local src = u.of and units[u.of]
    local notes
    local same = src and src.len == u.len and src.cad == u.cad
    if u.kind == "repeat" and same then
      -- (Fitted too: the chords are the same degrees, but a borrowed chord
      -- or a push may have come to one pass and not the other.)
      notes = fit(ctx, u, copyNotes(src, 0, u.len, 0), true)
    elseif u.kind == "seq" and same and u.cad == "none" then
      local s = bestShift(ctx, src, 0, u.len, u.shift, state.prev)
      notes = fit(ctx, u, copyNotes(src, 0, u.len, s))
    elseif src and u.kind ~= "frag" then
      -- The source's first half (moved, for a sequence), then a new second
      -- half to this unit's own ending. An answer to the same ending (a loop
      -- going round again) keeps the chords but still answers in the tune.
      local cut = M.cutFor(ctx.meter, src, u)
      local s = (u.kind == "seq") and bestShift(ctx, src, 0, cut, u.shift, state.prev) or 0
      notes = fit(ctx, u, copyNotes(src, 0, cut, s))
      if #notes > 0 then
        state.prev = notes[#notes].pos
        state.iv = #notes > 1 and (notes[#notes].pos - notes[#notes - 1].pos) or nil
        state.nct = false
      end
      local rh = M.unitRhythm(u, ctx.meter, r, ctx.rhythmRnd)
      local rest = {}
      for _, o in ipairs(rh) do if o >= cut then rest[#rest + 1] = o - cut end end
      if #rest == 0 then rest = { 0 } end
      local tail = { start = u.start + cut, len = u.len - cut, cad = u.cad, slots = u.slots }
      for _, nt in ipairs(walkUnit(ctx, tail, rest, rnd, state)) do
        notes[#notes + 1] = { at = nt.at + cut, pos = nt.pos }
      end
    elseif u.kind == "frag" then
      -- The basic idea's first half, again and again, each a step lower.
      local basic = units[1]
      local half = snap(ctx.meter, basic.len / 2)
      if half <= 0 then half = basic.len end
      notes = {}
      local at, k = 0, 0
      while at < u.len do
        local s = bestShift(ctx, basic, 0, half, -k, state.prev)
        for j, nt in ipairs(copyNotes(basic, 0, math.min(half, u.len - at), s)) do
          -- Each fragment is a statement of its own.
          notes[#notes + 1] = { at = nt.at + at, pos = nt.pos, fresh = (j == 1) }
        end
        if #notes > 0 then state.prev = notes[#notes].pos end
        at, k = at + half, k + 1
      end
      notes = fit(ctx, u, notes)
    else
      notes = walkUnit(ctx, u, M.unitRhythm(u, ctx.meter, r, ctx.rhythmRnd), rnd, state)
    end
    u.notes = notes
    if #notes > 0 then
      state.prev = notes[#notes].pos
      state.iv = #notes > 1 and (notes[#notes].pos - notes[#notes - 1].pos) or state.iv
      local at = u.start + notes[#notes].at
      state.nct = not T.onChord(M.keyAt(ctx, at), M.chordAt(ctx.timeline, at).chord, notes[#notes].pos)
    end
  end

  -- Into the timeline: each note lasts until the next (never longer than a
  -- half note, unless it is the last of its unit, which is held).
  local all = {}
  -- (Where each note came from: an exact repeat's notes are its source's,
  -- so a tension decided once comes round again.)
  local origin = {}
  for ui, u in ipairs(units) do
    local src = u.of and units[u.of]
    origin[ui] = (u.kind == "repeat" and src and src.len == u.len and src.cad == u.cad) and origin[u.of] or ui
  end
  for ui, u in ipairs(units) do
    for i, nt in ipairs(u.notes) do
      all[#all + 1] = { order = #all, step = u.start + nt.at, pos = nt.pos, last = (i == #u.notes),
                        from = origin[ui] .. ":" .. nt.at,
                        closes = (i == #u.notes) and u.cad ~= "none" and u.cad,
                        unitEnd = u.start + u.len, first = (i == 1) or nt.fresh or false }
    end
  end
  table.sort(all, function(a, b)
    if a.step ~= b.step then return a.step < b.step end
    return a.order < b.order
  end)
  -- Two notes on one step (a copied cell meeting the next unit): keep the later.
  local notes = {}
  for i, nt in ipairs(all) do
    if not all[i + 1] or all[i + 1].step ~= nt.step then notes[#notes + 1] = nt end
  end
  notes = M.wholeTriplets(notes)
  M.untangle(ctx, notes)
  -- A pushed chord takes the tune's note on its beat with it, an eighth
  -- early - as long as nothing else is sounding in that eighth, and the
  -- note does not begin a triplet (which would leave the other two behind).
  for _, sl in ipairs(ctx.timeline) do
    if sl.pushed then
      for i, nt in ipairs(notes) do
        local before, after = notes[i - 1], notes[i + 1]
        if nt.step == sl.s + 2 and i > 1 and before.step < sl.s and not (after and M.offGrid(after.step)) then
          nt.step, nt.pushed = sl.s, true
        end
      end
    end
  end
  if ctx.bassPcAt then M.noParallels(ctx, notes, ctx.bassPcAt) end
  notes = M.tension(ctx, notes, r, ctx.tensionRnd, plan.total)
  for i, nt in ipairs(notes) do
    local nextStep = notes[i + 1] and notes[i + 1].step or plan.total
    local len = nextStep - nt.step
    if not nt.last and not nt.held then len = math.min(len, 8) end
    nt.len = math.max(1, len)
    nt.pitch = T.pitch(M.keyAt(ctx, nt.step), nt.pos)
    nt.accent = nt.first or nt.pushed or nt.step % ctx.meter.bar == 0
  end
  -- (What each dissonance falls to: the note after it.)
  for i, nt in ipairs(notes) do
    if nt.dissonance and notes[i + 1] then
      nt.dissonance.res = notes[i + 1].pitch % 12
      nt.dissonance.root = M.chordAt(ctx.timeline, nt.dissonance.s).chord.rootPc
    end
  end
  return notes
end

------------------------------------------------------------------------------
-- Tension (1.10; docs/decisions/0022-tension-and-a-second-voice.md)
--
-- The accented non-chord tones the books teach
-- (Hutchinson, ch. 10; Open Music Theory, "Embellishing tones"):
--
--   - a suspension: the note before a chord change, a step above a note of
--     the new chord, is held over the change and falls to it - prepared,
--     dissonant on the beat, resolved down a step (4-3, 9-8, 7-6);
--   - an appoggiatura: on the beat, a note a step above the chord's note,
--     leapt up to, falling to it;
--   - an anticipation: a close's last note arriving an eighth early, over
--     the chord before, and struck again on the beat.
--
-- The note on the beat must last a quarter (in 4/4) or more, so the
-- dissonance can have its eighth (or quarter) and the chord's note the
-- rest; the chord must stay put until then. Never a minor ninth against the
-- bass, nor a note that falls onto the bass's own note but for the root
-- (the 9-8). Never on a pushed note, in a triplet, or on a close's goal
-- (but for the anticipation, which is the goal early). An exact repeat does
-- what its source did, and draws nothing.
------------------------------------------------------------------------------

-- (The suspension needs the tune to step down over a chord change, which
-- is rarer than a leap up to a beat, so it is taken more often when it can be.)
M.TENSION_CHANCE = { Rare = 0.1, Common = 0.25 }
M.SUSPEND_CHANCE = { Rare = 0.35, Common = 0.7 }
M.ANTICIPATE_CHANCE = { Rare = 0.25, Common = 0.5 }

function M.tension(ctx, notes, r, rnd, total)
  local chance = M.TENSION_CHANCE[r.tension]
  if not chance or not rnd then return notes end
  local meter = ctx.meter
  local function P(step, pos) return T.pitch(M.keyAt(ctx, step), pos) end
  local function chordOf(step) return M.chordAt(ctx.timeline, step) end
  -- (By the book, the bass the tune keeps clear of; free, the chord's root,
  -- so an inversion still leaves the tune alone.)
  local function bassAt(step)
    if ctx.bassPcAt then return ctx.bassPcAt(step) end
    local sl = chordOf(step)
    return sl and sl.chord.rootPc
  end
  -- The dissonance at `step` (pitch `d`) falling to `res` (a note of the
  -- chord there, pitch `rp`): not a minor ninth on the bass, and not onto
  -- the bass's own note unless it is the root.
  local function againstBass(step, d, rp)
    local b = bassAt(step)
    if not b then return true end
    local sl = chordOf(step)
    if (d - b) % 12 == 1 then return false end
    if rp % 12 == b and b ~= sl.chord.rootPc then return false end
    return true
  end
  local decided = {}
  local out = {}
  local i = 1
  while i <= #notes do
    local a, b, c = out[#out], notes[i], notes[i + 1]
    local nextStep = c and c.step or total
    local len = nextStep - b.step
    local strong = not M.offGrid(b.step) and M.strength(meter, b.step) >= 2
    local kind
    if a and strong and not b.pushed and not M.offGrid(a.step) and not (c and M.offGrid(c.step)) then
      local was = decided[b.from]
      local x = (was == nil) and rnd() or nil
      local sl = chordOf(b.step)
      local delay = (len >= 8) and 4 or 2
      local still = len >= 4 and chordOf(b.step + delay) == sl
      local key = M.keyAt(ctx, b.step)
      local onIt = T.onChord(key, sl.chord, b.pos)
      local goal = b.closes == "PAC" or b.closes == "IAC" or b.closes == "DC" or b.closes == "EC"
      local pa, pb = P(a.step, a.pos), P(b.step, b.pos)
      -- By the book, no parallels with the bass from the notes in their new
      -- places: a suspension does not hide parallel octaves (C held over
      -- into Bb and falling to it, the bass C to Bb), nor a pulled chord's
      -- bass under a late resolution.
      local late = { step = b.step + delay, pos = b.pos }
      local function clear(list)
        if not ctx.bassPcAt then return true end
        for k = 2, #list do
          if list[k - 1] and list[k] and M.parallel(ctx, ctx.bassPcAt, list[k - 1], nil, list[k], nil) then return false end
        end
        return true
      end
      -- An anticipation: a close's goal, an eighth early.
      local antic = b.closes and b.last and pa ~= pb and not (c and P(c.step, c.pos) == pb)
                    and b.step - 2 > a.step and P(b.step - 2, b.pos) == pb
                    and not M.offGrid(b.step - 2) and M.strength(meter, b.step - 2) < 2
                    and clear({ a, { step = b.step - 2, pos = b.pos } })
                    and not M.leapsBad(out[#out - 1] and { pos = out[#out - 1].pos, pitch = P(out[#out - 1].step, out[#out - 1].pos), first = out[#out - 1].first },
                                       { pos = a.pos, pitch = pa, first = a.first }, { pos = b.pos, pitch = pb })
      -- A suspension: the note before, a step above, held over a change.
      local before = chordOf(a.step)
      local sus = not goal and still and onIt and before ~= sl and a.pos == b.pos + 1
                  and P(b.step, a.pos) == pa and not T.onChord(key, sl.chord, a.pos)
                  and T.onChord(M.keyAt(ctx, a.step), before.chord, a.pos)
                  and againstBass(b.step, pa, pb) and againstBass(b.step + delay - 1e-3, pa, pb) and againstBass(b.step + delay, pa, pb)
                  and clear({ a, late, c })
      -- An appoggiatura: leapt up to, a step above the chord's note.
      local app = b.pos + 1
      local pApp = P(b.step, app)
      local appo = not goal and still and onIt and a.pos <= app - 2 and app <= ctx.hi + 1
                   and not T.onChord(key, sl.chord, app) and math.abs(pApp - pa) ~= 6
                   and math.abs(pApp - pa) <= 12 and againstBass(b.step, pApp, pb)
                   and againstBass(b.step + delay - 1e-3, pApp, pb) and againstBass(b.step + delay, pApp, pb)
                   and clear({ a, { step = b.step, pos = app }, late, c })
                   and not M.leapsBad(out[#out - 1] and { pos = out[#out - 1].pos, pitch = P(out[#out - 1].step, out[#out - 1].pos), first = out[#out - 1].first },
                                      { pos = a.pos, pitch = pa, first = a.first }, { pos = app, pitch = pApp })
      if was ~= nil then
        kind = (was == "anticipation" and antic and was) or (was == "suspension" and sus and was)
               or (was == "appoggiatura" and appo and was) or nil
      elseif antic and x < M.ANTICIPATE_CHANCE[r.tension] then kind = "anticipation"
      elseif sus and x < M.SUSPEND_CHANCE[r.tension] then kind = "suspension"
      elseif appo and x < chance then kind = "appoggiatura" end
      if was == nil then decided[b.from] = kind or false end
      if kind == "anticipation" then
        out[#out + 1] = { step = b.step - 2, pos = b.pos, tension = "anticipation", from = b.from .. "^" }
      elseif kind == "suspension" then
        -- (The note before rings on over the change; the chord's note comes
        -- in late.)
        a.held, a.tension = true, "suspension"
        a.dissonance = { s = b.step, e = b.step + delay }
        b.step, b.resolves = b.step + delay, true
      elseif kind == "appoggiatura" then
        out[#out + 1] = { step = b.step, pos = app, tension = "appoggiatura", first = b.first, from = b.from .. "^",
                          dissonance = { s = b.step, e = b.step + delay } }
        b.first = false
        b.step, b.resolves = b.step + delay, true
      end
    elseif decided[b.from] == nil then
      -- (Nothing can lean here - the idea's first note, say - so nothing
      -- does when it comes round again either.)
      decided[b.from] = false
    end
    out[#out + 1] = b
    i = i + 1
  end
  return out
end

------------------------------------------------------------------------------
-- The second voice (1.10): the tune harmonised a third or a sixth under -
-- "melodies in octaves, double octaves, in thirds and sixths"
-- (Rimsky-Korsakov), "parallel doubling in thirds or sixths, adjusted to
-- the harmony" (Belkin). Each note of the tune gets one under it, in the
-- scale sounding then: on the beat a note of the chord (the third or sixth
-- if it is one, else a fourth, else the nearest note of the chord a third
-- to a sixth under, a fifth at the last), off the beat the interval as it
-- comes. Under a tension note it moves with the tune.
------------------------------------------------------------------------------

M.SECOND = { Thirds = { -2, -3, -5 }, Sixths = { -5, -2, -3 } }
local SWEET = { [3] = true, [4] = true, [5] = true, [8] = true, [9] = true }

function M.secondVoice(ctx, melody, r)
  local order = M.SECOND[r.secondVoice]
  if not order then return nil end
  local out = {}
  for _, nt in ipairs(melody) do
    local key = M.keyAt(ctx, nt.step)
    local ch = M.chordAt(ctx.timeline, nt.step).chord
    local strong = not M.offGrid(nt.step) and M.strength(ctx.meter, nt.step) >= 2
    local free = not strong or nt.tension
    local function gap(p) return nt.pitch - T.pitch(key, p) end
    local pick
    for _, d in ipairs(order) do
      local p = nt.pos + d
      if SWEET[gap(p)] and (free or T.onChord(key, ch, p)) then pick = p; break end
    end
    if not pick then
      for _, want in ipairs({ SWEET, { [7] = true } }) do
        for p = nt.pos - 1, nt.pos - 6, -1 do
          if not pick and want[gap(p)] and T.onChord(key, ch, p) then pick = p end
        end
      end
    end
    -- (Where no note of the chord is consonant under it - fi over an
    -- Italian sixth has only the augmented sixth and the tritone, 1.15 - a
    -- consonant note of the scale.)
    if not pick then
      for p = nt.pos - 1, nt.pos - 6, -1 do
        if not pick and SWEET[gap(p)] then pick = p end
      end
    end
    pick = pick or (nt.pos + order[1])
    out[#out + 1] = { step = nt.step, len = nt.len, pos = pick, pitch = T.pitch(key, pick), accent = nt.accent }
  end
  return out
end

-- Is a step between the sixteenths (part of a triplet)?
function M.offGrid(step) return math.abs(step - math.floor(step + 0.5)) > 1e-6 end

-- Every triplet in the tune whole. Copying half a statement, holding a
-- closing note, or cutting at a cadence can take the end off a triplet
-- that began before it; what is left is not a triplet but a stumble. So a
-- note between the sixteenths stays only if it is one of a whole
-- eighth-note triplet (three in a beat) or quarter-note triplet (three in
-- two beats) starting on a beat; otherwise it goes, and the note before it
-- simply lasts longer.
function M.wholeTriplets(notes)
  local at = {}
  local function key(x) return ("%.4f"):format(x) end
  for _, n in ipairs(notes) do at[key(n.step)] = true end
  local function has(x) return at[key(x)] end
  local out = {}
  for _, n in ipairs(notes) do
    local keep = not M.offGrid(n.step)
    if not keep then
      for _, span in ipairs({ 4 / 3, 8 / 3 }) do
        for k = 1, 2 do
          local g = n.step - k * span
          if not M.offGrid(g / 4) and has(g) and has(g + span) and has(g + 2 * span) then keep = true end
        end
      end
    end
    if keep then out[#out + 1] = n end
  end
  return out
end

-- The last pass over the whole tune. Copying a statement onto new chords
-- can land two or three of its notes on the same chord tone, and a copy can
-- start a tritone, or more than an octave, from where the tune was. So:
--
--   - a note that would be the third the same in a row moves;
--   - a note a tritone from the one before moves - or, if it cannot (or it
--     is the last note, which stays put), the one before it does;
--   - a leap of more than an octave inside a statement is brought an
--     octave closer.
--
-- A note on the beat moves to another chord tone, one off the beat to the
-- note a step away - whichever is nearest, in range, and makes no tritone
-- with either neighbour.
function M.untangle(ctx, notes)
  local n = T.scaleLen(ctx.key)
  -- A note's pitch at a position, read in the scale sounding at its step.
  local function P(note, pos) return T.pitch(M.keyAt(ctx, note.step), pos or note.pos) end
  local function apart(x, px, y, py) return math.abs(P(x, px) - P(y, py)) end
  local NEAR, WIDE = { 1, -1, 2, -2, 3, -3, 4, -4 }, { 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6 }
  -- `wide` looks further, and a little outside the range, when nothing
  -- near will do.
  local function move(i, strict, wide)
    local a, b, c = notes[i - 1], notes[i], notes[i + 1]
    if not b then return false end
    local ch = M.chordAt(ctx.timeline, b.step).chord
    local key = M.keyAt(ctx, b.step)
    local strong = M.strength(ctx.meter, b.step) >= 2
    local slack = wide and 3 or 1
    for _, d in ipairs(wide and WIDE or NEAR) do
      local p = b.pos + d
      local inRange = p >= ctx.lo - slack and p <= ctx.hi + slack
      local fits = (strong and T.onChord(key, ch, p)) or (not strong and (math.abs(d) == 1 or wide))
      local clash = strict and ((a and apart(a, nil, b, p) == 6) or (c and apart(b, p, c, nil) == 6))
      -- A repeat is fine; three of a kind is not.
      local z, y = notes[i - 2], notes[i + 2]
      local same = function(x) return x and apart(x, nil, b, p) == 0 end
      local triple = (same(a) and same(z)) or (same(a) and same(c)) or (same(c) and same(y))
      -- (Nor two leaps the same way that outline no triad.)
      local function at(x, pos) return x and { pos = pos or x.pos, pitch = P(x, pos), first = x.first } end
      local nb = at(b, p)
      local leaps = strict and (M.leapsBad(at(z), at(a), nb) or M.leapsBad(at(a), nb, at(c)) or M.leapsBad(nb, at(c), at(y)))
      -- (Nor a leap wider than an octave inside a statement.)
      local wide12 = (a and not b.first and apart(a, nil, b, p) > 12) or (c and not c.first and apart(b, p, c, nil) > 12)
      if fits and inRange and not clash and not triple and not leaps and not wide12 and p ~= b.pos then
        b.pos = p
        return true
      end
    end
    return false
  end
  local function tryMove(i, allowTritone)
    return move(i, true, false) or move(i, true, true) or (allowTritone and move(i, false, true))
  end
  -- Twice over, so a note moved to mend one thing is checked again.
  for _ = 1, 2 do
    for i = 2, #notes do
      local a, b, c = notes[i - 1], notes[i], notes[i + 1]
      if c and P(a) == P(b) and P(b) == P(c) then
        -- (In the diminished and whole-tone scales every way out may be a
        -- tritone; a tritone is better there than a note three times.)
        tryMove(i, true)
      end
      if apart(a, nil, b, nil) == 6 then
        if i == #notes or not tryMove(i, false) then tryMove(i - 1, false) end
      end
      if not b.first and apart(a, nil, b, nil) > 12 then
        local p = b.pos + ((a.pos > b.pos) and n or -n)
        if apart(a, nil, b, p) ~= 6 and not (c and apart(b, p, c, nil) == 6)
           and p >= ctx.lo - 3 and p <= ctx.hi + 3 then b.pos = p end
      end
      -- Two leaps the same way that outline no triad (after the octave fix,
      -- which can make one): the middle note moves, or the last, or the first.
      local function at(x) return x and { pos = x.pos, pitch = P(x), first = x.first } end
      if c and M.leapsBad(at(a), at(b), at(c)) then
        -- (Not a close's goal, nor the idea's last note: they stay.)
        local function free(k)
          local x = notes[k]
          return x and k < #notes and not (x.closes == "PAC" or x.closes == "IAC" or x.closes == "DC" or x.closes == "EC")
        end
        if not ((free(i) and tryMove(i, false)) or (free(i + 1) and tryMove(i + 1, false)))
           and not a.first and free(i - 1) then tryMove(i - 1, false) end
      end
    end
  end
end

-- By the book (1.7): no parallel fifths or octaves between the tune and
-- the bass - the outer voices, where the books are strictest (Hutchinson,
-- ch. 26; Open Music Theory, first-species counterpoint and basso
-- continuo). Two notes running in which both the tune and the bass move,
-- and stand a fifth (or an octave, or a unison) apart both times, contrary
-- motion included. `bassPcAt(step)` is the bass sounding at a step.
--
-- Of the two, the later note moves (a full or imperfect close's last note
-- stays; the idea's first note moves only as a last resort); failing that, the earlier one. A note on
-- the beat moves to another chord tone, one off it a step - whichever is
-- nearest and keeps every rule `untangle` keeps.
function M.parallel(ctx, bassPcAt, a, ap, b, bp)
  local ba, bb = bassPcAt(a.step), bassPcAt(b.step)
  if not ba or not bb or ba == bb then return false end
  local pa, pb = T.pitch(M.keyAt(ctx, a.step), ap or a.pos), T.pitch(M.keyAt(ctx, b.step), bp or b.pos)
  if pa == pb then return false end
  local ia, ib = (pa - ba) % 12, (pb - bb) % 12
  return ia == ib and (ia == 0 or ia == 7)
end

function M.noParallels(ctx, notes, bassPcAt)
  local function P(note, pos) return T.pitch(M.keyAt(ctx, note.step), pos or note.pos) end
  local function try(i, wide, first, iac)
    local a, b, c = notes[i - 1], notes[i], notes[i + 1]
    -- (A full or imperfect close's last note is its goal and stays; a half
    -- close's or an open ending's may move to another note of its chord.
    -- As a last resort, `iac`, an imperfect close's goal may move between
    -- the tonic's third and fifth: the same close. 1.13.)
    local goal = b and (b.closes == "PAC" or b.closes == "IAC" or b.closes == "DC" or b.closes == "EC")
    if b and iac then goal = b.closes ~= "IAC" end
    if not b or goal or (i == 1 and not first) or (i == #notes and not b.closes) then return false end
    local ch = M.chordAt(ctx.timeline, b.step).chord
    local key = M.keyAt(ctx, b.step)
    local strong = M.strength(ctx.meter, b.step) >= 2 or b.closes
    -- (`wide` looks further, and a little further outside the range, when
    -- nothing near will do.)
    local slack = wide and 3 or 1
    for _, d in ipairs(wide and { 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6 } or { 1, -1, 2, -2, 3, -3, 4, -4 }) do
      local p = b.pos + d
      local fits = (strong and T.onChord(key, ch, p)) or (not strong and (math.abs(d) == 1 or wide))
      if iac then fits = T.onChord(key, T.chord(key, 0, "Triads"), p) and p % T.scaleLen(key) ~= 0 end
      local good = fits and p >= ctx.lo - slack and p <= ctx.hi + slack
      if good then
        local pp = P(b, p)
        -- (Not ipairs: with no note before, `a` is nil and ipairs would stop.)
        for k = 1, 2 do
          local x = (k == 1) and a or c
          if x then
            local gap = math.abs(P(x) - pp)
            if gap == 6 or (gap > 12 and not (x == c and c.first) and not (x == a and b.first)) then good = false end
          end
        end
        local z, y = notes[i - 2], notes[i + 2]
        if (z and a and P(z) == P(a) and P(a) == pp) or (c and y and P(c) == pp and P(y) == pp)
           or (a and c and P(a) == pp and P(c) == pp) then good = false end
        if good and a and M.parallel(ctx, bassPcAt, a, nil, b, p) then good = false end
        -- (Nor two leaps the same way that outline no triad, 1.13.)
        if good then
          local function at(x, pos) return x and { pos = pos or x.pos, pitch = P(x, pos), first = x.first } end
          local nb = at(b, p)
          if M.leapsBad(at(z), at(a), nb) or M.leapsBad(at(a), nb, at(c)) or M.leapsBad(nb, at(c), at(y)) then good = false end
        end
        if good and c and M.parallel(ctx, bassPcAt, b, p, c, nil) then good = false end
      end
      if good then b.pos = p; return true end
    end
    return false
  end
  for _ = 1, 2 do
    for i = 2, #notes do
      if M.parallel(ctx, bassPcAt, notes[i - 1], nil, notes[i], nil) then
        if not (try(i) or try(i - 1) or try(i, true) or try(i - 1, true)) and i > 3 then
          -- (Boxed in - the way out would be a third note the same: the
          -- note before that moves first, then this one.)
          local keep = notes[i - 2].pos
          if (try(i - 2) or try(i - 2, true)) and not (try(i - 1) or try(i - 1, true)) then notes[i - 2].pos = keep end
          if M.parallel(ctx, bassPcAt, notes[i - 1], nil, notes[i], nil) then try(i, true, nil, true) end
        elseif i == 2 and M.parallel(ctx, bassPcAt, notes[1], nil, notes[2], nil) then
          -- (At the very start, the idea's first note may move as a last resort.)
          try(1, true, true)
        end
        if M.parallel(ctx, bassPcAt, notes[i - 1], nil, notes[i], nil) and notes[i + 1] then
          -- (Boxed in from the other side - the note before is a close's
          -- goal, and every way out would be a tritone with the note after:
          -- the note after moves, then this one.)
          local keep = notes[i + 1].pos
          if not ((try(i + 1) or try(i + 1, true)) and (try(i) or try(i, true))) then notes[i + 1].pos = keep end
        end
      end
    end
  end
end

------------------------------------------------------------------------------
-- 6. Chords, bass and drums
------------------------------------------------------------------------------

local ARPEGGIOS = {
  { name = "up",          order = { 1, 2, 3, 4 } },
  { name = "up and down", order = { 1, 2, 3, 4, 3, 2 } },
  { name = "Alberti",     order = { 1, 3, 2, 3 } },
  { name = "rolling",     order = { 1, 2, 3, 2 } },
}

-- Bar lines inside [s, e), and s itself.
local function barStarts(meter, s, e)
  local out = { s }
  local b = (s // meter.bar + 1) * meter.bar
  while b < e do out[#out + 1] = b; b = b + meter.bar end
  return out
end

-- A pushed chord is played as if it began on its beat, with its first
-- stroke moved back onto the push: so it is struck on the "and", and not
-- struck again an eighth later on the beat.
-- Every chord keeps the beat it belongs to (`sl.beat`), even when it is
-- pushed an eighth early or pulled an eighth late. A chord's strokes are
-- laid out on the grid from its beat, then its first stroke moves to where
-- the chord really arrives (`onShift`): onto the push, so it is not struck
-- again on the beat, or back to the pull, so nothing is struck before it.
local function beatOf(sl) return sl.beat or sl.s end
local function gridStart(sl) return beatOf(sl) end
local function onShift(sl, onsets)
  local beat = beatOf(sl)
  if sl.s == beat then return onsets end
  if sl.s < beat then
    if onsets[1] == beat then onsets[1] = sl.s else table.insert(onsets, 1, sl.s) end
    return onsets
  end
  -- Pulled: nothing before it, and nothing crowding in less than a
  -- sixteenth after it (a triplet grid can put a stroke two thirds of a
  -- sixteenth behind).
  local out = { sl.s }
  for _, o in ipairs(onsets) do if o >= sl.s + 1 then out[#out + 1] = o end end
  return out
end
local onPush = onShift

-- The onsets of a pulse inside [s, e), with dotted and triplet figures laid
-- over it bar by bar.
local function pulseOnsets(meter, s, e, pace, groove, rnd, pattern, figures)
  -- A named rhythm: its steps in each bar, and where the chord comes.
  local named = M.rhythmOf(meter, groove)
  if named then
    local out, seen = { s }, { [s] = true }
    for _, b in ipairs(barStarts(meter, s, e)) do
      local bar = (b // meter.bar) * meter.bar
      for _, t in ipairs(named) do
        local st = bar + t
        if st >= s and st < e and not seen[st] then seen[st] = true; out[#out + 1] = st end
      end
    end
    table.sort(out)
    return out
  end
  groove = plainGroove(groove)
  -- Straight is on the beat (eighths when busy); syncopated is spread over
  -- the eighths.
  local unit = (pace == "Busy" or groove == "Syncopated") and 2 or meter.beat
  if pace == "Calm" then unit = meter.beat end
  local out = {}
  for _, b in ipairs(barStarts(meter, s, e)) do
    local stop = math.min(e, (b // meter.bar + 1) * meter.bar)
    local slots = (stop - b) // unit
    local pat
    if groove == "Syncopated" and slots >= 4 then
      pattern[slots] = pattern[slots] or syncopated(meter, b % meter.bar, unit, slots,
        math.max(2, round(slots * 3 / 8)), rnd)
      pat = pattern[slots]
    else
      pat = {}
      for i = 0, slots - 1 do pat[#pat + 1] = i end
    end
    local rel = {}
    for i, o in ipairs(pat) do rel[i] = o * unit end
    for _, o in ipairs(M.figure(meter, b % meter.bar, stop - b, rel, figures, rnd)) do out[#out + 1] = b + o end
  end
  return out
end

local function addNote(list, step, len, pitch, accent)
  if pitch and pitch >= 0 and pitch <= 127 and len > 0 then
    list[#list + 1] = { step = step, len = len, pitch = pitch, accent = accent or false }
  end
end

-- A held (Block) chord takes the figures as stabs: now and then struck
-- again a dotted quarter in (the Charleston, 1 and the "and" of 2), or
-- three times across the first two beats (a quarter-note triplet).
local function blockFigures(meter, onsets, to, figures, rnd)
  local F = FIGURES[figures]
  if not F or meter.beat ~= 4 then return onsets end
  local out = {}
  for i, o in ipairs(onsets) do
    out[#out + 1] = o
    local stop = onsets[i + 1] or to
    if o % 4 == 0 and stop - o >= 8 then
      local x = rnd()
      if x < F.tri then out[#out + 1] = o + 8 / 3; out[#out + 1] = o + 16 / 3
      elseif x < F.tri + F.dot then out[#out + 1] = o + 6 end
    end
  end
  return out
end

------------------------------------------------------------------------------
-- Part-writing by the book (1.7; docs/decisions/0019-part-writing-by-the-book.md)
------------------------------------------------------------------------------

-- An inverted chord does not double its bass note above it (Hutchinson,
-- ch. 26; Rimsky-Korsakov, ch. III). In close position a chord of four
-- notes or more simply leaves it out; otherwise the note takes the root's
-- place, or the fifth's - the nearest one free, so a voicing keeps its
-- number of notes (a rootless voicing takes the fifth or the ninth, never
-- the root). A diminished triad in first inversion doubles
-- its bass, as the books say: it is the one exception.
function M.undouble(v, ch, bassPc, style)
  local others = 0
  for _, p in ipairs(v) do if p % 12 ~= bassPc then others = others + 1 end end
  if others == #v then return v end
  local out, used = {}, {}
  for _, p in ipairs(v) do if p % 12 ~= bassPc then out[#out + 1] = p; used[p] = true end end
  if others >= 3 and (style == "Close" or not style) then return out end
  local fifth, nine
  for _, pc in ipairs(ch.pcs) do
    local role = T.roleOf(ch, pc)
    if role == "5" and pc ~= bassPc then fifth = pc end
    if role == "9" and pc ~= bassPc then nine = pc end
  end
  nine = nine or ch.nine
  local want = (style == "Rootless") and { fifth, nine } or { ch.rootPc ~= bassPc and ch.rootPc or nil, fifth }
  local top = v[#v]
  for _, p in ipairs(v) do
    if p % 12 == bassPc then
      -- (The top note's place goes to a note at or above it, so a spread
      -- voicing keeps its span.)
      local function cost(q) return math.abs(q - p) + ((p == top and q < p) and 12 or 0) end
      local best
      for k = 1, 2 do
        local pc = want[k]
        if pc then
          for q = p - 12, p + 12 do
            -- (Never under the chords' floor, G2, that the voicings keep:
            -- the bass must have room under it.)
            if q % 12 == pc and not used[q] and q >= math.min(43, p) and (not best or cost(q) < cost(best)) then best = q end
          end
        end
        if best then break end
      end
      if best then out[#out + 1] = best; used[best] = true end
    end
  end
  table.sort(out)
  -- (A two-note chord - a pentatonic scale's - keeps what it has.)
  if #out < 2 then return v end
  return out
end

-- A seventh falls a step into the next chord (Hutchinson, ch. 27; Open Music
-- Theory, "Tendency tones"): if the chord before had its seventh at `p7`,
-- and this chord has a note a semitone or a tone below it, the voicing that
-- puts that note there is wanted - unless this chord keeps the seventh's
-- note, which may then be held. Returns the wanted pitch, or nil.
function M.seventhTarget(ch, prevV, prevCh)
  if not prevV or not prevCh then return nil end
  local s7
  for _, pc in ipairs(prevCh.pcs) do if T.roleOf(prevCh, pc) == "7" then s7 = pc end end
  if not s7 or ch.has[s7] then return nil end
  local p7
  for _, p in ipairs(prevV) do if p % 12 == s7 then p7 = p end end
  if not p7 then return nil end
  -- (An augmented sixth's "seventh" is fi, which rises to sol: 1.15.)
  if prevCh.aug6 then return ch.has[(p7 + 1) % 12] and p7 + 1 or nil end
  for _, t in ipairs({ p7 - 1, p7 - 2 }) do
    if ch.has[t % 12] then return t end
  end
end

-- The lowest note of the tune sounding in [s, e), or nil.
local function tuneLowIn(tune, s, e)
  local low
  for _, n in ipairs(tune) do
    if n.step < e and n.step + n.len > s then low = math.min(low or 127, n.pitch) end
  end
  return low
end

-- Fill (1.11): the chords answer the tune - struck on the eighths where it
-- has held a note a beat or more, or is resting, a beat apart at most, and
-- held until it moves again; struck where the chord comes only if nowhere
-- else (or if the tune is silent there).
function M.fillOnsets(meter, sl, tune, to)
  local starts, sounding = {}, {}
  for _, n in ipairs(tune) do starts[#starts + 1] = n end
  local function lastBefore(st)
    local best
    for _, n in ipairs(tune) do if n.step <= st + 1e-9 then best = n end end
    return best
  end
  local function nextAfter(st)
    for _, n in ipairs(tune) do if n.step > st + 1e-9 then return n.step end end
  end
  local out, stops = {}, {}
  local last = -99
  for st = math.ceil(sl.s / 2) * 2, (to or sl.e) - 1, 2 do
    local n = lastBefore(st)
    local moving = n and math.abs(n.step - st) < 1e-9
    local rest = not n or n.step + n.len <= st + 1e-9
    local held = n and not moving and st - n.step >= meter.beat
    if not moving and (rest or held) and st - last >= meter.beat then
      out[#out + 1] = st
      stops[#stops + 1] = nextAfter(st) or sl.e
      last = st
    end
  end
  if #out == 0 or out[1] > sl.s then
    -- (The chord is heard where it comes, if the tune is not answering then.)
    if #out == 0 then
      table.insert(out, 1, sl.s)
      table.insert(stops, 1, nextAfter(sl.s) or sl.e)
    end
  end
  out.stops = stops
  return out
end

function M.chordsPart(ctx, timeline, r, rnd, win)
  local notes = {}
  local meter = ctx.meter
  local prev, prevBass, prevCh
  local arp = pickOne(rnd, ARPEGGIOS)
  local pattern = {}
  local lows = {}
  local book = win.book
  local lastHi = win.hi
  for idx, sl in ipairs(timeline) do
    local lo, hi = win.lo, win.hi
    if book and win.tune then
      -- By the book the chords sit just under the tune notes over them -
      -- reaching a step or two into the lowest, if they must - not under
      -- the whole tune's lowest note.
      -- (Over the chord's span on the beat: a pull does not move it.)
      local span = win.beatTl and win.beatTl[idx] or sl
      local low = tuneLowIn(win.tune, span.s, span.e)
      hi = low and math.max(57, math.min(76, low + 2)) or lastHi
      lastHi = hi
      lo = math.max(43, hi - ((r.voicing == "Close") and 12 or 24))
    end
    -- A spread voicing may reach down to G2 for its root.
    -- A voicing that will not fit under the tune may reach an octave over
    -- the top before it gives up and plays close position.
    local want = book and M.seventhTarget(sl.chord, prev, prevCh) or nil
    -- (A seventh's step down matters more than where the tune has gone:
    -- the window stretches to take it - a little over the top, if it must.)
    if want then
      if want < lo and want >= 43 then lo = want end
      if want > hi and want <= hi + 3 then hi = want end
    end
    local v = T.voiceAs(sl.chord, r.voicing, prev, lo, hi, 43, want)
    if #v == 0 then v = T.voiceAs(sl.chord, r.voicing, nil, lo, hi + 12, 43, want) end
    if #v == 0 then v = T.voiceAs(sl.chord, "Close", nil, lo, hi + 12, nil, want) end
    if book then
      local dimTriad = sl.chord.quality == "diminished" and #sl.chord.pcs == 3
      -- (Not a six-four: "when a triad is in second inversion, double the
      -- fifth (the bass note)" - Hutchinson, 26.9 and 26.12. 1.13.)
      -- (Nor the Neapolitan: "double the bass (the third)" - Hutchinson,
      -- 29.3; Open Music Theory likewise. 1.15.)
      local n6 = sl.chromatic and sl.chromatic.kind == "N6"
      if sl.inversion and sl.inversion ~= 2 and not dimTriad and not n6 then v = M.undouble(v, sl.chord, sl.bassPc, r.voicing) end
    end
    prev, prevCh = v, sl.chord
    lows[idx] = v[1]
    local onsets, perNote = {}, false
    -- The grid runs from this chord's beat to the next chord's beat (or to
    -- where this one ends, if the next is pushed in front of it).
    local from = gridStart(sl)
    local nextSl = timeline[idx + 1]
    local to = nextSl and math.min(sl.e, beatOf(nextSl)) or sl.e
    local short
    if r.chordStyle == "Pulse" then
      onsets = onShift(sl, pulseOnsets(meter, from, to, r.pace, r.groove, rnd, pattern, r.figures))
    elseif r.chordStyle == "Pedal" then
      -- (Struck once, where the chord comes, and held.)
      onsets = { sl.s }
    elseif r.chordStyle == "Offbeat" then
      -- The eighths between the beats, from where the chord comes to where
      -- it goes; short.
      -- (Struck no later than the next chord's beat, as every style is: a
      -- pulled chord only holds into the eighth after it.)
      local first = math.ceil(sl.s / 2) * 2
      for st = first, to - 1, 2 do
        if M.strength(meter, st) < 2 then onsets[#onsets + 1] = st end
      end
      if #onsets == 0 then onsets = { sl.s } end
      short = 1
    elseif r.chordStyle == "Fill" and win.melody then
      onsets = M.fillOnsets(meter, sl, win.melody, to)
    elseif r.chordStyle == "Broken" then
      local unit = (r.pace == "Calm") and meter.beat or (r.pace == "Busy" and 1 or 2)
      -- In triplets, an arpeggio rolls in eighth-note triplets.
      if r.pace ~= "Calm" and meter.beat == 4 and (r.figures == "Triplets" or
         (r.figures == "Mixed" and coin(rnd, 0.4))) then unit = 4 / 3 end
      for st = from, to - 1e-6, unit do onsets[#onsets + 1] = st end
      if unit ~= 4 / 3 then
        -- Otherwise the figures fall on the arpeggio: long-short pairs, or
        -- a beat in three.
        local rel = {}
        for i, o in ipairs(onsets) do rel[i] = o - from end
        onsets = {}
        for _, o in ipairs(M.figure(meter, from % meter.bar, to - from, rel, r.figures, rnd)) do
          onsets[#onsets + 1] = from + o
        end
      end
      onsets = onShift(sl, onsets)
      perNote = true
    else
      onsets = onShift(sl, blockFigures(meter, barStarts(meter, from, to), to, r.figures, rnd))
    end
    for i, o in ipairs(onsets) do
      local stop = onsets[i + 1] or sl.e
      if short then stop = math.min(stop, o + short) end
      if type(onsets.stops) == "table" and onsets.stops[i] then stop = math.min(stop, onsets.stops[i]) end
      local accent = o % meter.bar == 0
      if perNote then
        local k = arp.order[(i - 1) % #arp.order + 1]
        local pitch = (k <= #v) and v[k] or (v[(k - 1) % #v + 1] + 12 * ((k - 1) // #v))
        addNote(notes, o, stop - o, pitch, accent)
      else
        for _, p in ipairs(v) do addNote(notes, o, stop - o, p, accent) end
      end
    end
    if win.bass then
      local b = T.bassNote(bassPcOf(sl), prevBass, 36, 47)
      -- (Under any voicing but Close - which is as it always was - the bass
      -- goes under the chord's lowest note, down to E1 if it has to.)
      if r.voicing ~= "Close" and v[1] and b >= v[1] then b = T.bassNote(bassPcOf(sl), prevBass, 28, v[1] - 1) end
      -- By the book: under the chord, and no more than an octave and a fifth
      -- under it.
      if book and v[1] then b = T.bassNote(bassPcOf(sl), prevBass, math.max(28, v[1] - 19), v[1] - 1) end
      prevBass = b
      local bars = onShift(sl, barStarts(meter, gridStart(sl), to))
      for i, o in ipairs(bars) do addNote(notes, o, (bars[i + 1] or sl.e) - o, b, o % meter.bar == 0) end
    end
  end
  return notes, arp.name, lows
end

-- By the book, a Measure's bass is put in the octave that keeps it under
-- the chords and no more than an octave and a fifth under them (Rimsky-
-- Korsakov: "rarely more than an octave"; Belkin: no hole in the middle),
-- inside the bass's range, each note the octave nearest the one before.
-- Only octaves move: the notes are the same.
-- By the book, while a suspension or an appoggiatura sounds, the chords do
-- not play the note it falls to: "with the exception of 9-8, the pitch
-- class of the resolution tone should never sound in another voice
-- simultaneous with the suspended tone" (Open Music Theory, "Embellishing
-- tones"; the appoggiatura is treated alike) - the tune brings it. So not
-- when it falls to the chord's root (the 9-8). A chord struck then
-- leaves it out (but keeps two notes, and a Phrase's own bass).
function M.clearResolutions(list, tune, keepBass)
  local spans = {}
  for _, nt in ipairs(tune or {}) do
    local d = nt.dissonance
    if d and d.res and d.res ~= d.root then spans[#spans + 1] = d end
  end
  if #spans == 0 then return list end
  local at, low = {}, {}
  for _, n in ipairs(list) do
    at[n.step] = (at[n.step] or 0) + 1
    low[n.step] = math.min(low[n.step] or 999, n.pitch)
  end
  local out = {}
  for _, n in ipairs(list) do
    local drop = false
    for _, d in ipairs(spans) do
      if n.step < d.e and n.step + n.len > d.s and n.pitch % 12 == d.res then drop = true end
    end
    if drop and (at[n.step] <= 2 or (keepBass and n.pitch == low[n.step])) then drop = false end
    if drop then at[n.step] = at[n.step] - 1 else out[#out + 1] = n end
  end
  return out
end

function M.spaceBass(bass, timeline, lows)
  local function slotOf(step)
    local idx = 1
    for i, sl in ipairs(timeline) do if step >= sl.s then idx = i end end
    return idx
  end
  -- The octave of `n` nearest `ref` in [lo, hi], or nil.
  local function pick(n, ref, lo, hi)
    local best
    for q = n.pitch - 36, n.pitch + 36, 12 do
      if q >= lo and q <= hi and (not best or math.abs(q - ref) < math.abs(best - ref)) then best = q end
    end
    return best
  end
  local orig, starts = {}, {}
  for i, n in ipairs(bass) do
    orig[i] = n.pitch
    local sl = timeline[slotOf(n.step)]
    starts[i] = math.abs(n.step - sl.s) < 1e-9
  end
  -- First the note each chord stands on, the octave nearest where the
  -- line was going (the same interval from the last one as it had)...
  local prevI
  for i, n in ipairs(bass) do
    local low = lows[slotOf(n.step)]
    if starts[i] and low then
      local ref = prevI and (bass[prevI].pitch + orig[i] - orig[prevI]) or n.pitch
      -- (And where the note leading into it can still step into it from
      -- under the chord before.)
      local before = bass[i - 1]
      local lowBefore = before and not starts[i - 1] and lows[slotOf(before.step)]
      local best, bestCost
      -- (A walking bass's step into the chord matters more than the gap,
      -- which matters more than where the line was going.)
      for wi, w in ipairs({ { math.max(28, low - 19), math.min(55, low - 1) }, { 28, math.min(55, low - 1) } }) do
        for q = n.pitch - 36, n.pitch + 36, 12 do
          if q >= w[1] and q <= w[2] then
            local cost = math.abs(q - ref) + (wi == 2 and 50 or 0)
            if lowBefore then
              local into = q + orig[i - 1] - orig[i]
              if into < 28 or into > math.min(55, lowBefore - 1) then cost = cost + 100 end
            end
            if not bestCost or cost < bestCost then best, bestCost = q, cost end
          end
        end
      end
      n.pitch = best or n.pitch
      prevI = i
    end
  end
  -- ...then the notes between, each as far from the next chord's note as it
  -- was (so a walking bass still steps into it), or from the note before.
  for i, n in ipairs(bass) do
    local low = lows[slotOf(n.step)]
    if not starts[i] and low then
      local nx = bass[i + 1]
      local ref
      if nx and starts[i + 1] then ref = nx.pitch + orig[i] - orig[i + 1]
      elseif i > 1 then ref = bass[i - 1].pitch + orig[i] - orig[i - 1]
      else ref = n.pitch end
      local q = pick(n, ref, math.max(28, low - 19), math.min(55, low - 1))
      if not q or math.abs(q - ref) > 2 then q = pick(n, ref, 28, math.min(55, low - 1)) or q end
      n.pitch = q or n.pitch
    end
  end
  return bass
end

-- (1.14) The bass lying back with the chords: where a chord is pulled, the
-- bass's note on its beat comes an eighth late, with the chord, and the
-- note before is held to meet it. (Anything the bass played inside that
-- eighth goes: it changes where the chords do.) `chordTl` is the pulled
-- copy of `timeline`, slot for slot.
function M.pullBass(list, timeline, chordTl)
  local eps = 1e-9
  for i, csl in ipairs(chordTl) do
    if csl.pulled then
      local at, to = timeline[i].s, csl.s
      local pitch, onTo
      for k = #list, 1, -1 do
        local n = list[k]
        if math.abs(n.step - at) < eps then pitch = n.pitch end
        if math.abs(n.step - to) < eps then onTo = true end
        if n.step > at - eps and n.step < to - eps then table.remove(list, k) end
      end
      if pitch and not onTo then
        local nextAt = csl.e
        for _, n in ipairs(list) do if n.step > to + eps and n.step < nextAt then nextAt = n.step end end
        list[#list + 1] = { step = to, len = nextAt - to, pitch = pitch, accent = false }
      end
      for _, p in ipairs(list) do
        if p.step < at - eps and math.abs(p.step + p.len - at) < eps then p.len = to - p.step end
      end
      table.sort(list, function(a, b) if a.step ~= b.step then return a.step < b.step end return a.pitch < b.pitch end)
    end
  end
end

-- The kick drum's places in a bar, which the bass can follow.
function M.kickPattern(meter, r, rnd)
  -- (A named rhythm, 1.11, is the kick's pattern, as it is; drawn nothing.)
  local named = M.rhythmOf(meter, r.groove)
  if named then
    local out = {}
    for i, t in ipairs(named) do out[i] = t end
    return out
  end
  local slots = meter.bar // 2
  local k = ({ Calm = 1, Flowing = 2, Busy = 3 })[r.pace] or 2
  k = math.max(1, round(k * slots / 8))
  if plainGroove(r.groove) == "Syncopated" then
    local kk = math.max(2, round(slots * ({ Calm = 0.38, Flowing = 0.38, Busy = 0.62 })[r.pace]))
    local pat = syncopated(meter, 0, 2, slots, math.min(kk, slots - 1), rnd)
    local out = {}
    for i, o in ipairs(pat) do out[i] = o * 2 end
    return out
  end
  local pat = strongest(meter, 0, 2, slots, k, rnd)
  local out = {}
  for i, o in ipairs(pat) do out[i] = o * 2 end
  return out
end

-- The tune's pitch sounding at a step, or nil.
local function tuneAt(tune, step)
  for _, n in ipairs(tune) do
    if n.step <= step + 1e-9 and n.step + n.len > step + 1e-9 then return n.pitch end
  end
end

function M.bassPart(ctx, timeline, r, rnd, kick, tune)
  local notes = {}
  local meter = ctx.meter
  local prev
  local prevStep, prevP
  -- Parallel fifths or octaves with the tune, from the bass note before to `q` at `st`.
  local function parallelAt(st, q, fromStep, fromP)
    if not (tune and fromStep) then return false end
    local tA, tB = tuneAt(tune, fromStep), tuneAt(tune, st)
    if not (tA and tB) or tA == tB or q % 12 == fromP % 12 then return false end
    local ia, ib = (tA - fromP) % 12, (tB - q) % 12
    return ia == ib and (ib == 0 or ib == 7)
  end
  for idx, sl in ipairs(timeline) do
    -- (The root, or the note an inverted chord stands on.)
    local root = T.bassNote(bassPcOf(sl), prev, 31, 50)
    prev = root
    local nextSl = timeline[idx + 1]
    local key = sl.key or ctx.key
    if r.bass == "Pulse" then
      local on = {}
      for _, b in ipairs(barStarts(meter, sl.s, sl.e)) do
        local barStart = (b // meter.bar) * meter.bar
        for _, k in ipairs(kick) do
          local st = barStart + k
          -- (A pushed chord's bass comes on the push, not again on the beat.)
          if st >= sl.s and st < sl.e and not (sl.pushed and st == sl.s + 2) then on[#on + 1] = st end
        end
      end
      local seen, list = {}, {}
      table.insert(on, 1, sl.s)
      for _, st in ipairs(on) do if not seen[st] then seen[st] = true; list[#list + 1] = st end end
      table.sort(list)
      for i, st in ipairs(list) do
        addNote(notes, st, math.min((list[i + 1] or sl.e) - st, 8), root, st % meter.bar == 0)
      end
    elseif r.bass == "Moving" then
      local unit = meter.beat
      if r.pace == "Calm" and (meter.bar // 2) % meter.beat == 0 and meter.beats % 2 == 0 then unit = meter.bar // 2 end
      local beats = {}
      local from = gridStart(sl)
      for st = from, sl.e - 1, unit do beats[#beats + 1] = st - from end
      -- Dotted and triplet figures fall on a walking bass too.
      local figured = M.figure(meter, from % meter.bar, sl.e - from, beats, r.figures, rnd)
      beats = {}
      for i, o in ipairs(figured) do beats[i] = from + o end
      beats = onPush(sl, beats)
      for i, st in ipairs(beats) do
        local p
        if i == 1 then p = root
        elseif i == #beats and nextSl then
          -- A step into the next chord's root, from the scale sounding now:
          -- the scale note a semitone or a tone from it, nearer the root
          -- being left.
          local nr = T.bassNote(bassPcOf(nextSl), root, 31, 50)
          local best, bestBad
          for q = nr - 2, nr + 2 do
            local d = math.abs(q - nr)
            if d >= 1 and T.posOf(key, q) then
              -- (By the book, one that makes no parallels with the tune,
              -- going in or coming out, is preferred.)
              local bad = (parallelAt(st, q, prevStep, prevP) or parallelAt(nextSl.s, nr, st, q)) and 1 or 0
              if not best or bad < bestBad or (bad == bestBad and math.abs(q - root) < math.abs(best - root)) then
                best, bestBad = q, bad
              end
            end
          end
          p = best or root
        else
          local fifth = T.bassNote(sl.chord.pcs[3] or sl.chord.pcs[2] or sl.chord.rootPc, root, 31, 52)
          local choices = { fifth, root + 12 <= 52 and root + 12 or root, T.bassNote(sl.chord.pcs[2] or sl.chord.rootPc, root, 31, 52) }
          local ws = { 3, 2, 1 }
          -- By the book, no passing note that makes parallel fifths or
          -- octaves with the tune (if another will do).
          if tune and prevStep then
            local any = false
            local w2 = {}
            for k, q in ipairs(choices) do
              local par = parallelAt(st, q, prevStep, prevP)
              w2[k] = par and 0 or ws[k]
              if not par then any = true end
            end
            if any then ws = w2
            else
              -- (Every one would: the bass holds its note - oblique motion.)
              choices, ws = { prevP, prevP, prevP }, ws
            end
          end
          p = weighted(rnd, choices, ws)
        end
        prevStep, prevP = st, p
        addNote(notes, st, (beats[i + 1] or sl.e) - st, p, st % meter.bar == 0)
      end
    else
      local bars = onPush(sl, barStarts(meter, gridStart(sl), sl.e))
      for i, st in ipairs(bars) do addNote(notes, st, (bars[i + 1] or sl.e) - st, root, st % meter.bar == 0) end
    end
  end
  return notes
end

M.DRUM = { kick = 36, snare = 38, hat = 42, open = 46, crash = 49, tomHi = 50, tomMid = 47, tomLo = 45,
           clap = 39, pedal = 44, ride = 51, bell = 53, tomHiMid = 48, tomFloor = 43, tomFloorLo = 41 }

-- Which beats the snare plays: the backbeat (2 and 4), or the half-time
-- beat 3 when the pace is calm.
local function backbeats(meter, pace)
  local out = {}
  if pace == "Calm" and meter.mid then return { meter.mid } end
  if meter.beats == 1 then return { meter.bar // 2 } end
  if meter.beats == 3 then return { 2 * meter.beat } end
  for b = 1, meter.beats - 1, 2 do out[#out + 1] = b * meter.beat end
  return out
end

------------------------------------------------------------------------------
-- Drums on their own (docs/decisions/0013-drums-are-a-kind-of-idea.md)
--
-- A groove a bar long, made for the beat style from the same pieces 1.0's
-- Measures had for their drums (the kick's metric or Euclidean pattern, the
-- backbeat),
-- played in pairs of bars where the second answers the first with one
-- small change; then fills where the Fills setting asks, each a beat or two
-- of snare, toms, or both (in triplets when the figures are), with a crash
-- on the downbeat it leads to - the top of the idea, for the last one, since
-- a drum idea is made to loop. General MIDI notes throughout, channel 10.
------------------------------------------------------------------------------

local TOMS = { 50, 48, 47, 45, 43, 41 }   -- high to floor

-- The bar of groove: kick, snare and cymbal steps, and what plays them.
local function drumGroove(meter, r, rnd)
  local D = M.DRUM
  local bar, beat = meter.bar, meter.beat
  local style = r.beat
  if (style == "Breakbeat" or style == "Reggaeton") and not (bar == 16 and beat == 4) then style = "Backbeat" end
  local g = { kick = {}, snare = {}, snarePitch = D.snare, open = {}, style = style }
  if style == "Four on the floor" then
    for b = 0, bar - 1, beat do g.kick[#g.kick + 1] = b end
    g.snare = backbeats(meter, "Flowing")
    g.snarePitch = D.clap
    if beat % 2 == 0 then
      for b = 0, bar - 1, beat do g.open[#g.open + 1] = b + beat // 2 end
    end
  elseif style == "Half-time" then
    g.snare = { meter.mid or (meter.beats - 1) * beat }
    local calm = {}
    for k, v in pairs(r) do calm[k] = v end
    calm.pace = "Calm"
    g.kick = M.kickPattern(meter, calm, rnd)
  elseif style == "Reggaeton" then
    -- The dembow (1.13): four on the floor under the tresillo, twice.
    for b = 0, bar - 1, beat do g.kick[#g.kick + 1] = b end
    g.snare = { 3, 6, 11, 14 }
  elseif style == "Breakbeat" then
    g.kick = pickOne(rnd, { { 0, 10 }, { 0, 2, 10 }, { 0, 6, 10 }, { 0, 10, 11 } })
    g.snare = { 4, 12 }
    -- One snare knocked off the backbeat: a sixteenth or an eighth either
    -- side of beat 4, or the "a" of 3.
    if coin(rnd, 0.75) then table.insert(g.snare, pickOne(rnd, { 7, 9, 14, 15 })) end
  else
    g.kick = M.kickPattern(meter, r, rnd)
    g.snare = backbeats(meter, "Flowing")
  end
  -- Dotted and triplet figures fall on the kick - but four on the floor is
  -- four on the floor.
  -- (Nor on a named rhythm, which is that rhythm.)
  if style ~= "Four on the floor" and style ~= "Reggaeton" and not (M.rhythmOf(meter, r.groove) and (style == "Backbeat" or style == "Half-time")) then
    g.kick = M.figure(meter, 0, bar, g.kick, r.figures, rnd)
  end
  if style ~= "Four on the floor" then
    local onSnare = {}
    for _, x in ipairs(g.snare) do onSnare[x] = true end
    local k = {}
    for _, x in ipairs(g.kick) do if not onSnare[x] then k[#k + 1] = x end end
    g.kick = k
  end
  table.sort(g.snare)

  -- Time: quarters when calm, eighths when flowing, sixteenths when busy (a
  -- breakbeat is sixteenths at any pace but calm); eighths in 6/8 rather
  -- than dotted quarters; in triplets, the shuffle.
  local unit = ({ Calm = beat, Flowing = 2, Busy = 1 })[r.pace] or 2
  if style == "Breakbeat" and r.pace ~= "Calm" then unit = 1 end
  if beat % 4 ~= 0 and r.pace == "Calm" then unit = 2 end
  g.time = {}
  if r.figures == "Triplets" and r.pace ~= "Calm" and beat == 4 then
    for b = 0, bar - 1, 4 do
      g.time[#g.time + 1] = b
      if r.pace == "Busy" then g.time[#g.time + 1] = b + 4 / 3 end
      g.time[#g.time + 1] = b + 8 / 3
    end
  else
    for t = 0, bar - 1, unit do g.time[#g.time + 1] = t end
  end
  g.timePitch = (r.cymbal == "Ride") and D.ride or D.hat
  return g
end

-- A fill over [from, to) of a bar starting at `base`.
local function drumFill(meter, r, rnd, base, from, to)
  local D = M.DRUM
  local out = {}
  local unit = (r.pace == "Calm") and 2 or 1
  local kinds = { "roll", "toms", "snare and toms" }
  if meter.beat == 4 and (r.figures == "Triplets" or r.figures == "Mixed") then kinds[#kinds + 1] = "triplets" end
  local kind = pickOne(rnd, kinds)
  local steps = {}
  if kind == "triplets" then
    for t = from, to - 1e-6, 4 / 3 do steps[#steps + 1] = t end
  else
    for t = from, to - 1, unit do steps[#steps + 1] = t end
  end
  for i, t in ipairs(steps) do
    local pitch
    if kind == "roll" then pitch = D.snare
    elseif kind == "snare and toms" then
      pitch = (i <= #steps // 2) and D.snare or TOMS[math.min(#TOMS, 1 + (i - #steps // 2 - 1) * #TOMS // math.max(1, #steps - #steps // 2))]
    else
      pitch = TOMS[math.min(#TOMS, 1 + (i - 1) * #TOMS // #steps)]
    end
    out[#out + 1] = { step = base + t, len = 1, pitch = pitch, accent = (i == 1) }
  end
  -- The kick under the fill's first note, so it lands with weight.
  out[#out + 1] = { step = base + from, len = 1, pitch = D.kick, accent = true }
  return out, kind
end

-- (1.14) Ghost notes: the snare tapped very quietly (`M.GHOST_VELOCITY`,
-- at any Velocity) on the weakest places in the bar - the sixteenths between
-- the eighths, or the middle of a triplet - most often just before or after
-- the backbeat. Chosen once for the idea, so every bar has the same; never
-- where the snare or the kick already plays, nor in a fill. One draw per
-- place, so Common keeps Rare's and adds.
M.GHOST_CHANCE = { Rare = 0.35, Common = 0.8 }
M.GHOST_AWAY = 0.35     -- a place not next to the backbeat: this much as likely
M.GHOST_VELOCITY = 38

function M.ghostSteps(meter, g, r, rnd)
  local chance = M.GHOST_CHANCE[r.ghosts]
  if not chance or not rnd then return {} end
  -- (Places by the third of a sixteenth: a triplet kick at 9 1/3 is the
  -- same place however it was reached.)
  local function at(x) return math.floor(x * 3 + 0.5) end
  local taken = {}
  for _, x in ipairs(g.kick) do taken[at(x)] = true end
  for _, x in ipairs(g.snare) do taken[at(x)] = true end
  local places = {}
  if r.figures == "Triplets" and r.pace ~= "Calm" and meter.beat == 4 then
    -- (In a shuffle, the middle note of the triplet, which the hats skip.)
    for b = 0, meter.bar - 1, 4 do places[#places + 1] = b + 4 / 3 end
  else
    for t = 1, meter.bar - 1, 2 do places[#places + 1] = t end
  end
  local out = {}
  for _, t in ipairs(places) do
    local x = rnd()
    local nearIt = false
    for _, y in ipairs(g.snare) do if math.abs(t - y) <= 2 then nearIt = true end end
    if not taken[at(t)] and x < chance * (nearIt and 1 or M.GHOST_AWAY) then out[#out + 1] = t end
  end
  return out
end

function M.drumIdea(meter, r, rnd, ghostRnd)
  local D = M.DRUM
  local bar, bars = meter.bar, r.bars
  local g = drumGroove(meter, r, rnd)
  local ghosts = M.ghostSteps(meter, g, r, ghostRnd)
  -- The answering bar: one small change, chosen once for the whole idea.
  local change = pickOne(rnd, { "pickup", "double", "open" })

  local fills = {}
  for b = 0, bars - 1 do
    local last = (b == bars - 1)
    if (r.fills == "At the end" and last) or
       (r.fills == "Every 4 bars" and ((b + 1) % 4 == 0 or last)) or
       (r.fills == "Every 2 bars" and ((b + 1) % 2 == 0 or last)) then fills[b] = true end
  end
  local crashes = {}
  for b in pairs(fills) do crashes[(b + 1) % bars] = true end

  local notes, fillKinds = {}, {}
  local total = bars * bar
  -- (An open hat or a crash near the end stops at the end.)
  local function add(step, len, pitch, accent)
    notes[#notes + 1] = { step = step, len = math.min(len, total - step), pitch = pitch, accent = accent or false }
  end
  for b = 0, bars - 1 do
    local base = b * bar
    -- A fill takes a beat - two when busy, or at the end of the idea or a
    -- four-bar phrase when flowing - never the whole bar.
    local fillBeats = (r.pace == "Busy" or (r.pace == "Flowing" and (b == bars - 1 or (b + 1) % 4 == 0))) and 2 or 1
    local fillFrom = fills[b] and math.max(meter.beat, bar - fillBeats * meter.beat) or bar
    local answer = (b % 2 == 1)
    local kick = {}
    for _, k in ipairs(g.kick) do kick[#kick + 1] = k end
    local open = {}
    for _, o in ipairs(g.open) do open[o] = true end
    if answer then
      local snareAt = {}
      for _, x in ipairs(g.snare) do snareAt[x] = true end
      -- (A pickup kick on the "and" of 4 - or on its "a", where the snare
      -- has the "and", as in the dembow.)
      -- (And an open hat only where there is a closed one to open: on the
      -- ride, or with the hats in quarters, the pickup instead.)
      local hatAt = false
      for _, t in ipairs(g.time) do if t == bar - 2 then hatAt = true end end
      local how = change
      if how == "open" and not (g.timePitch == D.hat and hatAt) then how = "pickup" end
      if how == "pickup" and not snareAt[bar - 2] then kick[#kick + 1] = bar - 2
      elseif how == "pickup" and not snareAt[bar - 1] then kick[#kick + 1] = bar - 1
      elseif how == "double" and g.snare[#g.snare] and g.snare[#g.snare] - 1 > 0 then kick[#kick + 1] = g.snare[#g.snare] - 1
      elseif how == "open" then open[bar - 2] = true end
    end
    for _, k in ipairs(kick) do if k < fillFrom then add(base + k, 1, D.kick, k == 0) end end
    for _, x in ipairs(g.snare) do if x < fillFrom then add(base + x, 1, g.snarePitch, true) end end
    local kickAt = {}
    for _, k in ipairs(kick) do kickAt[math.floor(k * 3 + 0.5)] = true end
    for _, x in ipairs(ghosts) do
      if x < fillFrom and not kickAt[math.floor(x * 3 + 0.5)] then
        add(base + x, 1, D.snare, false)
        notes[#notes].ghost = true
      end
    end
    for _, t in ipairs(g.time) do
      if t < fillFrom and not (crashes[b] and t == 0) then
        if open[t] and g.timePitch == D.hat then add(base + t, 2, D.open, false)
        else
          local bell = g.timePitch == D.ride and t % meter.beat == 0 and t == 0
          add(base + t, 1, bell and D.bell or g.timePitch, false)
        end
      end
    end
    if g.timePitch == D.ride then
      for _, x in ipairs(g.snare) do if x < fillFrom then add(base + x, 1, D.pedal, false) end end
    end
    if crashes[b] then add(base, 4, D.crash, true) end
    if fills[b] then
      local f, kind = drumFill(meter, r, rnd, base, fillFrom, bar)
      for _, n in ipairs(f) do notes[#notes + 1] = n end
      fillKinds[#fillKinds + 1] = kind
    end
  end
  table.sort(notes, function(a, c)
    if a.step ~= c.step then return a.step < c.step end
    return a.pitch < c.pitch
  end)
  local fillBars = {}
  for b = 0, bars - 1 do if fills[b] then fillBars[#fillBars + 1] = tostring(b + 1) end end
  return notes, { style = g.style, fills = fillBars, fillKinds = fillKinds, change = change, ghosts = #ghosts }
end

------------------------------------------------------------------------------
-- 7. The idea
------------------------------------------------------------------------------

function M.keyName(key)
  return T.ROOTS[key.root].name .. " " .. T.SCALES[key.scale].name
end

------------------------------------------------------------------------------
-- Swing
--
-- The last thing done to an idea, after every note is placed: each quarter
-- note's grid is stretched so its off-beat eighth lands later - at 100% two
-- thirds of the way through the beat, where a triplet would be - and the
-- sixteenths either side move with it in proportion. Beats themselves never
-- move. Triplet notes are already in threes and are left alone. Only in
-- metres whose beat is a quarter or a half note: 6/8 and 12/8 swing by
-- being in threes, and 7/8 has no quarter notes to swing.
------------------------------------------------------------------------------

function M.swings(meter) return meter.beat % 4 == 0 end

function M.swingWarp(meter, amount)
  amount = tonumber(amount) or 0
  if amount <= 0 or not M.swings(meter) then return nil end
  local d = math.min(amount, 100) / 100 / 6      -- of a beat: 1/6 is triplet swing
  return function(step)
    local q = math.floor(step / 4) * 4
    local f = (step - q) / 4
    if f <= 0.5 then f = f * (0.5 + d) / 0.5
    else f = 0.5 + d + (f - 0.5) * (0.5 - d) / 0.5 end
    return q + f * 4
  end
end

local function whole(x) return math.abs(x - math.floor(x + 0.5)) < 1e-9 end

-- Shaped velocity (1.7): by how strong the beat is - the downbeat loudest,
-- an off-beat or a triplet note softest - and by the part: the chords under
-- the tune (their inner notes under their top), the bass just under it. An
-- accented note (a push, the start of a statement) leans in a little.
M.SHAPE = { [3] = 104, [2.5] = 98, [2] = 94, [1] = 86, [0] = 80 }
M.SHAPE_PART = { Melody = 0, Chords = -10, Bass = -4, Drums = 0, ["Second voice"] = -6 }
M.SHAPE_INNER, M.SHAPE_ACCENT = -4, 6

function M.shapedVelocity(meter, step, part, accent, inner)
  local v = M.SHAPE[M.strength(meter, step)] or 80
  v = v + (M.SHAPE_PART[part] or 0) + (accent and M.SHAPE_ACCENT or 0) + (inner and M.SHAPE_INNER or 0)
  return math.max(1, math.min(127, v))
end

local function toBlockNotes(list, chan, vel, warp, meter, part)
  local out = {}
  -- (The top note of everything struck together, for the inner notes.)
  local top = {}
  if vel == "Shaped" and part == "Chords" then
    for _, n in ipairs(list) do top[n.step] = math.max(top[n.step] or 0, n.pitch) end
  end
  for _, n in ipairs(list) do
    local s, e = n.step, n.step + n.len
    if warp then
      if whole(s) then s = warp(s) end
      if whole(e) then e = warp(e) end
    end
    local v = (vel == "Accents" and n.accent) and M.ACCENT or 100
    if vel == "Shaped" then
      v = M.shapedVelocity(meter, n.step, part, n.accent, top[n.step] and n.pitch < top[n.step])
    end
    if n.ghost then v = M.GHOST_VELOCITY end
    out[#out + 1] = { start = s / 4, len = (e - s) / 4, pitch = n.pitch, chan = chan,
                      accent = n.accent, vel = v, tension = n.tension, ghost = n.ghost }
  end
  table.sort(out, function(a, b)
    if a.start ~= b.start then return a.start < b.start end
    if a.pitch ~= b.pitch then return a.pitch < b.pitch end
    return a.len < b.len
  end)
  return out
end

-- The chords as a musician reads them: bar by bar, | C G | Am F |. A chord
-- is shown in the bar its beat is in: pushed an eighth early it is marked
-- ^, pulled an eighth late _, borrowed *.
function M.chordLine(timeline, meter)
  local bars = {}
  for _, sl in ipairs(timeline) do
    local b = beatOf(sl) // meter.bar + 1
    bars[b] = bars[b] or {}
    -- An inverted chord is written over its bass note: C/E.
    local slash = sl.bassPos and ("/" .. T.noteName(sl.key, sl.bassPos)) or ""
    table.insert(bars[b], (sl.pushed and "^" or "") .. (sl.pulled and "_" or "") .. sl.chord.name .. slash ..
                          ((sl.borrowed or sl.chromatic) and "*" or "") .. ((sl.applied or sl.commonTone) and ">" or ""))
    -- A chord held over bar lines shows in each bar it sounds in, as "-".
    for x = b + 1, (sl.e - 1) // meter.bar + 1 do
      bars[x] = bars[x] or {}
      table.insert(bars[x], "-")
    end
  end
  local out = {}
  for i = 1, #bars do out[#out + 1] = table.concat(bars[i] or {}, " ") end
  return "| " .. table.concat(out, " | ") .. " |"
end

--[[ The idea for these settings, this metre and this number.

     Returns {
       block    = { name, beats, notes, parts = { { name, notes, chan } }, layout },
       r        = the settings with every Any rolled,
       key, plan, timeline, melody (steps and positions, for the tests),
       summary  = a line saying what was rolled,
       chords   = the chord line,
     }
]]
function M.make(st, meter, seed)
  seed = math.floor(tonumber(seed) or st.seed or 1)
  local r = M.resolve(st, seed)
  if r.kind == "Drums" then return M.makeDrums(st, meter, seed, r) end
  local key = T.key(r.root, r.scale)
  -- With no chords to play, the tune still walks over chords - plain triads,
  -- one a bar - so its strong notes outline a harmony. The chord settings
  -- are hidden then, and must not change it.
  if not r.chords then
    r.colour, r.chordPace, r.chordStyle = "Triads", "One a bar", "Block"
    r.flavours, r.voicing, r.inversions = "Off", "Close", "Off"
    r.partWriting = "Free"
    r.progression = "Walk"
  end
  local book = r.partWriting == "By the book"
  local plan = M.plan(r, meter, M.stream(seed, "plan"))
  local colour = r.colour
  local sch, schemaWhy = M.schemaFor(r, key, M.stream(seed, "schema"))
  -- (1.12) Where the key changes, if it does: the timeline keeps the same
  -- chord either side apart there.
  local changeAt, tonicStart
  if r.kind == "Measure" and M.KEY_CHANGE[r.keyChange] then
    -- (For the truck driver, a look at the chords first - the same dice
    -- give the same chords - to find a section that starts on the tonic.)
    local tonicAt, canTonic, made
    if r.keyChange == "Truck driver" and T.scaleLen(key) == 7 then
      local look = M.harmony(plan, key, r, meter, M.stream(seed, "harmony"), colour,
                             sch and M.schemaFor(r, key, M.stream(seed, "schema")))
      tonicAt = function(step)
        local sl = M.chordAt(look, step)
        return sl and sl.degree == 0
      end
      -- (1.14) Where none starts on the tonic, one may be made to: its first
      -- chord becomes the tonic, if that leaves its close the chords it
      -- needs (two for a full close, one for a half).
      canTonic = function(u)
        local need = (u.cad == "PAC" or u.cad == "IAC" or u.cad == "DC" or u.cad == "EC") and 2
                     or ((u.cad ~= "none" and u.cad ~= "open") and 1 or 0)
        return u.rel and #u.rel > need
      end
    end
    changeAt, made = M.changeAt(plan, meter, tonicAt, canTonic)
    if made then tonicStart = changeAt end
  end
  local timeline = M.harmony(plan, key, r, meter, M.stream(seed, "harmony"), colour, sch, changeAt, tonicStart)
  local keyChange = M.keyChange(plan, timeline, key, r, meter, colour, changeAt)
  M.raiseDominant(timeline, key, colour)
  local borrowed = M.borrow(timeline, key, r, M.stream(seed, "borrow"), colour)
  local applied = M.applied(timeline, plan, key, r, M.stream(seed, "applied"), colour)
  -- (1.15) At a close, the Neapolitan or an augmented sixth before the V
  -- (with Borrowed); a passing diminished seventh where the bass climbs a
  -- tone (with Applied).
  local chromatic = M.chromatic(timeline, plan, key, r, M.stream(seed, "chroma"), meter)
  for _, sl in ipairs(M.passing(timeline, plan, key, r, M.stream(seed, "passing"), meter)) do
    applied[#applied + 1] = sl
  end
  -- (1.16) And the common-tone diminished seventh, colouring a held I or V.
  local commonTones = M.commonTone(timeline, plan, key, r, M.stream(seed, "commontone"), meter)
  M.flavour(timeline, plan, key, r, M.stream(seed, "colour"), M.stream(seed, "sixnine"))
  -- By the book, a half close with Mixed stands on a plain V: "almost
  -- invariably a triad, rather than a seventh chord" (Open Music Theory,
  -- "Classical cadence types"). Sevenths, chosen for sevenths everywhere,
  -- keep theirs.
  if book and colour == "Mixed" then
    for _, u in ipairs(plan.units) do
      if u.cad == "HC" then
        local sl = M.chordAt(timeline, u.start + u.len - 1)
        if sl and #sl.chord.pcs > 3 and not sl.flavour and not sl.borrowed and not sl.applied then
          sl.chord = T.chord(sl.key or key, sl.degree, "Triads")
          sl.halfTriad = true
        end
      end
    end
  end
  M.push(timeline, meter, r, M.stream(seed, "push"))
  M.invert(timeline, plan, key, r, M.stream(seed, "invert"), meter)
  -- The chords part plays from its own copy, pulled late where it is; the
  -- tune, the bass and the drums play on the beat.
  local chordTl = r.chords and M.pull(timeline, meter, r, M.stream(seed, "pull")) or timeline

  local lo, hi = M.melodyRange(key, r.register)
  local ctx = { key = key, meter = meter, lo = lo, hi = hi, timeline = timeline,
                rhythmRnd = M.stream(seed, "rhythm"), tensionRnd = M.stream(seed, "tension") }
  local contour = CONTOURS[r.contour] or CONTOURS.Arch
  ctx.target = function(step)
    local t = step / math.max(1, plan.total)
    return lo + 1 + contour(t) * (hi - lo - 2)
  end

  -- By the book the tune keeps clear of parallels with the bass each chord
  -- stands on (a Measure's bass on the beat, a Phrase's under its own
  -- chords) - not with a moving bass's passing notes, which keep clear of
  -- the tune themselves: so a different bass still leaves the tune alone.
  if book and r.chords then
    local tl = (r.kind == "Measure") and timeline or chordTl
    ctx.bassPcAt = function(step)
      local sl = M.chordAt(tl, step)
      return sl and bassPcOf(sl)
    end
  end

  local parts = {}
  local melody
  if r.melody then
    melody = M.melody(plan, ctx, r, M.stream(seed, "melody"))
    parts[#parts + 1] = { name = "Melody", list = melody }
  end

  -- The second voice (1.10), under the tune; the chords go under both.
  local second = melody and M.secondVoice(ctx, melody, r)
  local front = melody
  if second then
    front = {}
    for _, n in ipairs(melody) do front[#front + 1] = n end
    for _, n in ipairs(second) do front[#front + 1] = n end
  end

  local arpName, lows
  if r.chords then
    -- The chords sit under the tune, so a tune in a low register pushes them
    -- down, but never into the mud below C3.
    local top = 69
    if front then
      local low = 127
      for _, n in ipairs(front) do low = math.min(low, n.pitch) end
      top = math.max(57, math.min(69, low - 1))
    end
    local list
    list, arpName, lows = M.chordsPart(ctx, chordTl, r, M.stream(seed, "chords"),
                                 { lo = math.max(43, top - 16), hi = top, bass = r.kind ~= "Measure",
                                   book = book, tune = front, beatTl = timeline, melody = melody })
    if book then list = M.clearResolutions(list, melody, r.kind ~= "Measure") end
    parts[#parts + 1] = { name = "Chords", list = list }
  end

  if r.kind == "Measure" then
    -- No drums in a Measure since 1.3 (drums are their own kind), but a
    -- pulsing bass still plays the pattern a kick drum would, drawn as it
    -- always was so the bass is unchanged.
    local kick = M.kickPattern(meter, r, M.stream(seed, "drums"))
    local bassList = M.bassPart(ctx, timeline, r, M.stream(seed, "bass"), kick, book and melody)
    if book then M.spaceBass(bassList, timeline, lows) end
    if r.bassPull == "With the chords" then M.pullBass(bassList, timeline, chordTl) end
    parts[#parts + 1] = { name = "Bass", list = bassList }
  end
  -- (Last, so the other parts keep their channels.)
  if second then parts[#parts + 1] = { name = "Second voice", list = second } end

  -- Channels: in one item each part has its own; on tracks of their own
  -- every part is on 1. (A drum idea, on 10, makes its own block: makeDrums.)
  local oneItem = r.kind ~= "Measure" or r.layout == "One item"
  local warp = M.swingWarp(meter, st.swing)
  local block = { parts = {}, notes = {}, beats = plan.total / 4,
                  layout = (oneItem and "one") or "tracks" }
  for i, p in ipairs(parts) do
    local chan = oneItem and (i - 1) or 0
    local notes = toBlockNotes(p.list, chan, r.velocity, warp, meter, p.name)
    block.parts[#block.parts + 1] = { name = p.name, notes = notes, chan = chan }
    for _, n in ipairs(notes) do block.notes[#block.notes + 1] = n end
  end
  -- Fully ordered: Lua's sort is not stable, and may shuffle notes that
  -- start together differently from one run to the next.
  table.sort(block.notes, function(a, b)
    if a.start ~= b.start then return a.start < b.start end
    if a.chan ~= b.chan then return a.chan < b.chan end
    if a.pitch ~= b.pitch then return a.pitch < b.pitch end
    return a.len < b.len
  end)

  local what = r.kind
  if r.kind == "Phrase" then what = what .. " (" .. r.content:lower() .. ")" end
  if r.kind == "Measure" then what = what .. " (" .. r.form:lower() .. ")" end
  block.name = ("Good Idea %d - %s, %s, %s"):format(seed, what, barsName(r.bars), M.keyName(key))

  local said = { M.keyName(key), r.pace, r.groove }
  if r.melody then said[#said + 1] = r.contour:lower() .. " contour" end
  if r.chords then
    said[#said + 1] = r.colour:lower()
    if r.voicing ~= "Close" then said[#said + 1] = r.voicing:lower() .. " voicing" end
    if sch then said[#said + 1] = sch.name:lower() .. " progression"
    elseif schemaWhy then said[#said + 1] = "walked (" .. schemaWhy .. ")" end
    said[#said + 1] = M.valueName(M.BY_ID.chordPace, r.chordPace)
    said[#said + 1] = r.chordStyle:lower() .. ((r.chordStyle == "Broken" and arpName) and (" (" .. arpName .. ")") or "")
  end
  if r.kind == "Measure" then said[#said + 1] = r.bass:lower() .. " bass" end
  if r.figures ~= "Plain" then said[#said + 1] = r.figures:lower() end
  if r.push ~= "None" then said[#said + 1] = r.push:lower() .. " push" end
  if r.chords and r.pull ~= "None" then said[#said + 1] = r.pull:lower() .. " pull" end
  if warp then said[#said + 1] = math.floor(st.swing) .. "% swing" end
  if keyChange then said[#said + 1] = r.keyChange:lower() .. " to " .. M.keyName(keyChange.key) end

  -- Each borrowed chord, said in full for the window.
  local notes = {}
  for _, sl in ipairs(borrowed) do
    local b = sl.borrowed
    b.bar = (sl.pushed and sl.s + 2 or sl.s) // meter.bar + 1
    b.text = ("%s (%s) in bar %d, from %s"):format(b.name, b.numeral, b.bar, b.from)
    notes[#notes + 1] = b
  end

  -- Each applied chord, said in full for the window.
  local appliedNotes = {}
  for _, sl in ipairs(applied) do
    local a = sl.applied
    -- (Named as the chord after it now is: flavoured or inverted, say.)
    for i, x in ipairs(timeline) do
      if x == sl and timeline[i + 1] then
        local nx = timeline[i + 1]
        a.to = nx.chord.name .. (nx.bassPos and ("/" .. T.noteName(nx.key, nx.bassPos)) or "")
      end
    end
    a.bar = (sl.pushed and sl.s + 2 or sl.s) // meter.bar + 1
    a.text = ("%s (%s) in bar %d, %s %s"):format(a.name, a.numeral, a.bar,
                                                  a.passing and "passing to" or "leading to", a.to)
    appliedNotes[#appliedNotes + 1] = a
  end

  -- (1.15) Each chromatic chord, said in full for the window.
  local chromaticNotes = {}
  local CHROMATIC_SAID = { N6 = "the Neapolitan", ["It+6"] = "an Italian augmented sixth",
                           ["Fr+6"] = "a French augmented sixth", ["Ger+6"] = "a German augmented sixth",
                           ["Sw+6"] = "a Swiss augmented sixth" }
  for _, sl in ipairs(chromatic) do
    local c = sl.chromatic
    c.bar = (sl.pushed and sl.s + 2 or sl.s) // meter.bar + 1
    c.text = ("%s (%s, %s) in bar %d, before the V"):format(c.name, c.numeral, CHROMATIC_SAID[c.kind], c.bar)
    chromaticNotes[#chromaticNotes + 1] = c
  end

  -- (1.16) Each common-tone diminished seventh, said with the applied
  -- chords.
  for _, sl in ipairs(commonTones) do
    local c = sl.commonTone
    c.bar = sl.s // meter.bar + 1
    c.text = ("%s (%s, the common-tone diminished seventh) in bar %d, colouring %s"):format(c.name, c.numeral, c.bar, c.over)
    appliedNotes[#appliedNotes + 1] = c
  end

  local cadNames = { PAC = "closes on the tonic", IAC = "closes on the third or fifth",
                     DC = "a deceptive close (V to vi)",
                     HC = "ends on the dominant (open)", open = "ends open, to loop", none = "" }
  return {
    seed = seed, r = r, key = key, plan = plan, timeline = timeline, melody = melody,
    block = block,
    summary = table.concat(said, "  /  "),
    chords = M.chordLine(chordTl, meter),
    chordTimeline = chordTl,
    shape = plan.shape,
    ending = cadNames[plan.ending] or "",
    borrowed = notes,
    applied = appliedNotes,
    chromatic = chromaticNotes,
    keyChange = keyChange and {
      at = keyChange.at, key = keyChange.key, truck = keyChange.truck,
      text = ("%s to %s at bar %d%s"):format(
        ({ ["Step up"] = "Up a whole tone", ["Half step up"] = "Up a semitone", ["Truck driver"] = "Up a whole tone" })[r.keyChange],
        M.keyName(keyChange.key), keyChange.at // meter.bar + 1,
        keyChange.truck and (", through its V (" .. keyChange.truck.chord.name .. ") - the truck driver") or ""),
    } or nil,
    schema = sch and sch.name or nil,
  }
end

-- A Drums idea: one part, on channel 10, in one item.
function M.makeDrums(st, meter, seed, r)
  local total = r.bars * meter.bar
  local list, info = M.drumIdea(meter, r, M.stream(seed, "kit"),
                                (r.ghosts ~= "Off") and M.stream(seed, "ghost") or nil)
  local notes = toBlockNotes(list, 9, r.velocity, M.swingWarp(meter, st.swing), meter, "Drums")
  local block = { parts = { { name = "Drums", notes = notes, chan = 9, drums = true } },
                  notes = {}, beats = total / 4, layout = "one" }
  for i, n in ipairs(notes) do block.notes[i] = n end
  local style = info.style:lower()
  block.name = ("Good Idea %d - Drums (%s), %s"):format(seed, style, barsName(r.bars))
  local said = { info.style, r.cymbal == "Ride" and "ride" or "hi-hats", r.pace, r.groove }
  if r.figures ~= "Plain" then said[#said + 1] = r.figures:lower() end
  if M.swingWarp(meter, st.swing) then said[#said + 1] = math.floor(st.swing) .. "% swing" end
  local fills = (#info.fills == 0) and "no fills"
                or ((#info.fills == 1 and "a fill in bar " or "fills in bars ") .. table.concat(info.fills, ", "))
  return {
    seed = seed, r = r, key = T.key(1, 1), meter = meter,
    plan = { units = {}, total = total, shape = "", ending = "" },
    timeline = {}, chordTimeline = {}, melody = nil,
    block = block, chromatic = {}, applied = {},
    summary = table.concat(said, "  /  "),
    chords = "",
    shape = "",
    ending = fills,
    borrowed = {},
    drums = info,
  }
end

-- The settings that would give this idea back with nothing left to chance:
-- every Any on screen replaced by what it rolled. A hidden setting does not
-- change the idea, and is left alone so it is still Any when it shows again.
function M.keep(st, idea)
  for id in pairs(idea.r.rolled) do
    local s = M.BY_ID[id]
    if s and st[id] == "Any" and M.shows(s, st) then st[id] = idea.r[id] end
  end
  return st
end

return M
