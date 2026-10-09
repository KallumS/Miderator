--[[ Starting Blocks - the music.

     Pure Lua. Nothing in this file touches REAPER or ImGui, which is the
     point: the generators are the part that has to be right, and here they can
     be run and checked by tests/test_engine.lua rather than only by reading.

     Scale degrees are 0-based throughout - degree 0 is the tonic - because the
     arithmetic wants them that way (degree + 2 is the third above, and the
     octave falls out of the division). Table indices are 1-based, like Lua.
]]

local M = {}

M.MAX_NOTES = 1024
M.PPQ       = 960

-- Everything leaves at one velocity. Shaping a block's dynamics is a job for
-- the MIDI editor once it is in the project, not for seven sliders here.
M.VELOCITY  = 100

------------------------------------------------------------------------------
-- Keys
--
-- These are ScaleView for REAPER's roots and scales, unchanged, so the two
-- apps agree on what a scale is and on what to call its notes. Both spellings
-- of every pitch class are here plus Cb, because C# major and Db major are the
-- same seven notes written differently and the difference is what the note
-- names come out as.
------------------------------------------------------------------------------

local LETTER_PC = { 0, 2, 4, 5, 7, 9, 11 }        -- C D E F G A B
local LETTERS   = { "C", "D", "E", "F", "G", "A", "B" }
local ACCIDENTAL = { [-2] = "bb", [-1] = "b", [0] = "", [1] = "#", [2] = "x" }

M.ROOTS = {
  { name = "C",  letter = 0, acc =  0 }, { name = "C#", letter = 0, acc =  1 },
  { name = "Db", letter = 1, acc = -1 }, { name = "D",  letter = 1, acc =  0 },
  { name = "D#", letter = 1, acc =  1 }, { name = "Eb", letter = 2, acc = -1 },
  { name = "E",  letter = 2, acc =  0 }, { name = "F",  letter = 3, acc =  0 },
  { name = "F#", letter = 3, acc =  1 }, { name = "Gb", letter = 4, acc = -1 },
  { name = "G",  letter = 4, acc =  0 }, { name = "G#", letter = 4, acc =  1 },
  { name = "Ab", letter = 5, acc = -1 }, { name = "A",  letter = 5, acc =  0 },
  { name = "A#", letter = 5, acc =  1 }, { name = "Bb", letter = 6, acc = -1 },
  { name = "B",  letter = 6, acc =  0 }, { name = "Cb", letter = 0, acc = -1 },
}

-- iv is semitones from the root; letters is how many letter-names each degree
-- sits above the root letter, which is what makes F# major spell its seventh
-- E# rather than F.
M.SCALES = {
  { name = "Major",      iv = {0,2,4,5,7,9,11},   letters = {0,1,2,3,4,5,6} },
  { name = "Minor",      iv = {0,2,3,5,7,8,10},   letters = {0,1,2,3,4,5,6} },
  { name = "Harm Minor", iv = {0,2,3,5,7,8,11},   letters = {0,1,2,3,4,5,6} },
  { name = "Ionian",     iv = {0,2,4,5,7,9,11},   letters = {0,1,2,3,4,5,6} },
  { name = "Dorian",     iv = {0,2,3,5,7,9,10},   letters = {0,1,2,3,4,5,6} },
  { name = "Phrygian",   iv = {0,1,3,5,7,8,10},   letters = {0,1,2,3,4,5,6} },
  { name = "Lydian",     iv = {0,2,4,6,7,9,11},   letters = {0,1,2,3,4,5,6} },
  { name = "Mixolydian", iv = {0,2,4,5,7,9,10},   letters = {0,1,2,3,4,5,6} },
  { name = "Aeolian",    iv = {0,2,3,5,7,8,10},   letters = {0,1,2,3,4,5,6} },
  { name = "Maj Pent",   iv = {0,2,4,7,9},        letters = {0,1,2,4,5} },
  { name = "Min Pent",   iv = {0,3,5,7,10},       letters = {0,2,3,4,6} },
  { name = "Maj Blues",  iv = {0,2,3,4,7,9},      letters = {0,1,2,2,4,5} },
  { name = "Min Blues",  iv = {0,3,5,6,7,10},     letters = {0,2,3,4,4,6} },
  { name = "Whole Tone", iv = {0,2,4,6,8,10},     letters = {0,1,2,3,4,5} },
  { name = "Dim W-H",    iv = {0,2,3,5,6,8,9,11}, letters = {0,1,2,3,4,5,5,6} },
  { name = "Dim H-W",    iv = {0,1,3,4,6,7,9,10}, letters = {0,1,2,2,3,4,5,6} },
}

M.DEGREE_TITLES = { "Tonic", "Supertonic", "Mediant", "Subdominant",
                    "Dominant", "Submediant", "Leading Tone" }

local NUMERALS = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII" }

------------------------------------------------------------------------------
-- Chords
--
-- One row per chord, carrying its own name, symbol and intervals. Under JSFX
-- these were three parallel tables and adding a chord meant editing all three
-- in step; here a chord is one thing and cannot half-exist.
--
-- Semitones are from the chord's root. The families follow Wikipedia's list of
-- chords.
------------------------------------------------------------------------------

M.FAMILIES = { "Diatonic", "Triads", "6ths & 7ths", "Extended", "Altered",
               "Sus & Add", "Quartal", "Named" }

local T, S7, EX, AL, SA, QU, NA = 2, 3, 4, 5, 6, 7, 8   -- indices into FAMILIES

M.CHORDS = {
  { sym="maj",  name="Major",                  iv={0,4,7},          fam=T },
  { sym="m",    name="Minor",                  iv={0,3,7},          fam=T },
  { sym="dim",  name="Diminished",             iv={0,3,6},          fam=T },
  { sym="aug",  name="Augmented",              iv={0,4,8},          fam=T },
  { sym="b5",   name="Flat Five",              iv={0,4,6},          fam=T },
  { sym="5",    name="Fifth (Power)",          iv={0,7},            fam=T },

  { sym="6",       name="Sixth",                    iv={0,4,7,9},     fam=S7 },
  { sym="m6",      name="Minor Sixth",              iv={0,3,7,9},     fam=S7 },
  { sym="6/9",     name="Six-Nine",                 iv={0,4,7,9,14},  fam=S7 },
  { sym="m6/9",    name="Minor Six-Nine",           iv={0,3,7,9,14},  fam=S7 },
  { sym="7",       name="Dominant Seventh",         iv={0,4,7,10},    fam=S7 },
  { sym="maj7",    name="Major Seventh",            iv={0,4,7,11},    fam=S7 },
  { sym="m7",      name="Minor Seventh",            iv={0,3,7,10},    fam=S7 },
  { sym="mMaj7",   name="Minor-Major Seventh",      iv={0,3,7,11},    fam=S7 },
  { sym="m7b5",    name="Half-Diminished Seventh",  iv={0,3,6,10},    fam=S7 },
  { sym="dim7",    name="Diminished Seventh",       iv={0,3,6,9},     fam=S7 },
  { sym="7#5",     name="Augmented Seventh",        iv={0,4,8,10},    fam=S7 },
  { sym="maj7#5",  name="Augmented Major Seventh",  iv={0,4,8,11},    fam=S7 },
  { sym="7b5",     name="Seventh Flat Five",        iv={0,4,6,10},    fam=S7 },
  { sym="dimMaj7", name="Diminished Major Seventh", iv={0,3,6,11},    fam=S7 },
  { sym="7/6",     name="Seven Six",                iv={0,4,7,9,10},  fam=S7 },

  { sym="9",     name="Ninth",               iv={0,4,7,10,14},       fam=EX },
  { sym="maj9",  name="Major Ninth",         iv={0,4,7,11,14},       fam=EX },
  { sym="m9",    name="Minor Ninth",         iv={0,3,7,10,14},       fam=EX },
  { sym="mMaj9", name="Minor-Major Ninth",   iv={0,3,7,11,14},       fam=EX },
  { sym="11",    name="Eleventh",            iv={0,4,7,10,14,17},    fam=EX },
  { sym="maj11", name="Major Eleventh",      iv={0,4,7,11,14,17},    fam=EX },
  { sym="m11",   name="Minor Eleventh",      iv={0,3,7,10,14,17},    fam=EX },
  { sym="13",    name="Thirteenth",          iv={0,4,7,10,14,17,21}, fam=EX },
  { sym="maj13", name="Major Thirteenth",    iv={0,4,7,11,14,17,21}, fam=EX },
  { sym="m13",   name="Minor Thirteenth",    iv={0,3,7,10,14,17,21}, fam=EX },

  { sym="7b9",       name="Seventh Flat Nine",             iv={0,4,7,10,13},    fam=AL },
  { sym="7#9",       name="Seventh Sharp Nine",            iv={0,4,7,10,15},    fam=AL },
  { sym="7#11",      name="Seventh Sharp Eleven",          iv={0,4,7,10,18},    fam=AL },
  { sym="7b13",      name="Seventh Flat Thirteen",         iv={0,4,7,10,20},    fam=AL },
  { sym="7#5b9",     name="Seventh Sharp Five Flat Nine",  iv={0,4,8,10,13},    fam=AL },
  { sym="7#5#9",     name="Seventh Sharp Five Sharp Nine", iv={0,4,8,10,15},    fam=AL },
  { sym="7b5b9",     name="Seventh Flat Five Flat Nine",   iv={0,4,6,10,13},    fam=AL },
  { sym="7alt",      name="Altered Dominant",              iv={0,4,8,10,13,15}, fam=AL },
  { sym="13b9",      name="Thirteenth Flat Nine",          iv={0,4,7,10,13,21}, fam=AL },
  { sym="maj7#11",   name="Major Seventh Sharp Eleven",    iv={0,4,7,11,18},    fam=AL },
  { sym="m9b5",      name="Minor Ninth Flat Five",         iv={0,3,6,10,14},    fam=AL },
  { sym="9#5",       name="Ninth Augmented Fifth",         iv={0,4,8,10,14},    fam=AL },
  { sym="9b5",       name="Ninth Flat Fifth",              iv={0,4,6,10,14},    fam=AL },
  { sym="9#11",      name="Augmented Eleventh",            iv={0,4,7,10,14,18}, fam=AL },
  { sym="maj7#5#11", name="Augmented Major Seventh Sharp Eleven", iv={0,4,8,11,18}, fam=AL },
  { sym="13b9b5",    name="Thirteenth Flat Nine Flat Five", iv={0,4,6,10,13,21}, fam=AL },

  { sym="sus2",     name="Suspended Second",             iv={0,2,7},       fam=SA },
  { sym="sus4",     name="Suspended Fourth",             iv={0,5,7},       fam=SA },
  { sym="7sus4",    name="Seventh Suspended Fourth",     iv={0,5,7,10},    fam=SA },
  { sym="9sus4",    name="Ninth Suspended Fourth",       iv={0,5,7,10,14}, fam=SA },
  { sym="maj7sus4", name="Major Seventh Suspended Fourth", iv={0,5,7,11},  fam=SA },
  { sym="add9",     name="Added Ninth",                  iv={0,4,7,14},    fam=SA },
  { sym="m(add9)",  name="Minor Added Ninth",            iv={0,3,7,14},    fam=SA },
  { sym="add4",     name="Added Fourth",                 iv={0,4,5,7},     fam=SA },
  { sym="add11",    name="Added Eleventh",               iv={0,4,7,17},    fam=SA },
  { sym="add13",    name="Added Thirteenth",             iv={0,4,7,21},    fam=SA },
  { sym="add2",     name="Added Second",                 iv={0,2,4,7},     fam=SA },
  { sym="m(add2)",  name="Minor Added Second",           iv={0,2,3,7},     fam=SA },

  { sym="Q4/3",    name="Quartal Triad",       iv={0,5,10},    fam=QU },
  { sym="Q4/4",    name="Quartal Tetrad",      iv={0,5,10,15}, fam=QU },
  { sym="Q5/3",    name="Quintal Triad",       iv={0,7,14},    fam=QU },
  { sym="WT3",     name="Whole-Tone Trichord", iv={0,2,4},     fam=QU },
  { sym="cluster", name="Chromatic Cluster",   iv={0,1,2},     fam=QU },
  { sym="dia-cl",  name="Diatonic Cluster",    iv={0,2,4,5},   fam=QU },

  -- Voiced as they stand rather than reduced to a pitch-class set: the list
  -- gives the Tristan chord as 0 3 6 10, which makes it a half-diminished
  -- seventh and indistinguishable from one.
  { sym="Mystic",    name="Mystic (Scriabin)",   iv={0,6,10,16,21,26}, fam=NA },
  { sym="Petrushka", name="Petrushka",           iv={0,4,6,7,10,13},   fam=NA },
  { sym="Tristan",   name="Tristan",             iv={0,6,10,15},       fam=NA },
  { sym="So What",   name="So What",             iv={0,5,10,15,19},    fam=NA },
  { sym="Dream",     name="Dream",               iv={0,5,6,7},         fam=NA },
  { sym="Vienna",    name="Viennese Trichord",   iv={0,1,6},           fam=NA },
  { sym="Vienna II", name="Viennese Trichord II", iv={0,6,7},          fam=NA },
  { sym="Napoleon",  name="Ode-to-Napoleon",     iv={0,1,4,5,8,9},     fam=NA },
  { sym="Elektra",   name="Elektra",             iv={0,7,9,13,16},     fam=NA },
  { sym="Farben",    name="Farben",              iv={0,8,11,16,21},    fam=NA },
  { sym="It+6",      name="Italian Sixth",       iv={0,4,10},          fam=NA },
  { sym="Fr+6",      name="French Sixth",        iv={0,4,6,10},        fam=NA },
  { sym="Ger+6",     name="German Sixth",        iv={0,4,7,10},        fam=NA },
}

-- The chords the key hands you for free, as offsets in scale degrees from the
-- one you picked. Always in key, which is why this is the default family.
M.DIATONIC = {
  { name = "Triad", offsets = {0,2,4} },
  { name = "7th",   offsets = {0,2,4,6} },
  { name = "9th",   offsets = {0,2,4,6,8} },
  { name = "11th",  offsets = {0,2,4,6,8,10} },
  { name = "13th",  offsets = {0,2,4,6,8,10,12} },
  { name = "6th",   offsets = {0,2,4,5} },
  { name = "sus2",  offsets = {0,1,4} },
  { name = "sus4",  offsets = {0,3,4} },
  { name = "5th",   offsets = {0,4} },
}

------------------------------------------------------------------------------
-- Everything else in the catalogue
------------------------------------------------------------------------------

M.RATES = {
  { name = "1/64", beats = 0.0625 }, { name = "1/32", beats = 0.125 },
  { name = "1/16", beats = 0.25 },   { name = "1/8",  beats = 0.5 },
  { name = "1/4",  beats = 1 },      { name = "1/2",  beats = 2 },
  { name = "1/1",  beats = 4 },
}
M.RATE_MODS = {
  { name = "Straight", mul = 1 },
  { name = "Triplet",  mul = 2/3 },
  { name = "Dotted",   mul = 1.5 },
}

M.DIRECTIONS = { "Up", "Down", "Up/Down", "Down/Up", "Random", "Converge", "Diverge" }
M.MAX_REPEATS = 16

-- How long a block is, as a multiple of a bar. Quarter and half bars are here
-- because a block shorter than a bar is still a block.
M.BAR_LENGTHS = {
  { name = "1/4", bars = 0.25 }, { name = "1/2", bars = 0.5 },
  { name = "1",   bars = 1 },    { name = "2",   bars = 2 },
  { name = "4",   bars = 4 },    { name = "8",   bars = 8 },
}

-- An arpeggio or a run is measured one of two ways, and the user picks which:
-- by how many passes it plays, or by filling a length the way it did before
-- repeats existed.
M.LENGTH_MODES = { "Repeats", "Bars" }

-- What a melodic cell moves by, in scale steps. Sustain moves by nothing: it
-- is one note held, which is the smallest melodic thing there is.
M.INTERVALS = {
  { name = "2nd",     steps = 1 },
  { name = "3rd",     steps = 2 },
  { name = "4th",     steps = 3 },
  { name = "5th",     steps = 4 },
  { name = "6th",     steps = 5 },
  { name = "7th",     steps = 6 },
  { name = "Octave",  octave = true },   -- however many steps this scale takes
  { name = "Sustain", hold = true },
}
M.SHAPES     = { "Single", "Return", "Fill" }
M.BASS_TONES = { "Root", "3rd", "5th", "7th" }
M.INVERSIONS = { "Root", "1st", "2nd", "3rd" }

-- A drum block is one piece of the kit hit at one rate. There are no named
-- patterns: four on the floor is a kick every 1/4, one-and-three is a kick
-- every 1/2, a backbeat is a snare from beat two every 1/2. Naming those
-- would be naming things the rates already say.
--
-- `rates` is what that piece offers, always ending at 1/1, which is a single
-- hit. `start` is where its first hit falls, in beats from the top of the bar,
-- so the snare begins on the two. An empty `rates` is a single hit and nothing
-- to choose.
M.DRUM_PIECES = {
  { name = "Kick",      note = 36, start = 0,
    rates = { "1/16", "1/8", "1/4", "1/2", "1/1" } },
  { name = "Snare",     note = 38, start = 1,
    rates = { "1/8", "1/4", "1/2", "1/1" } },
  { name = "Closed HH", note = 42, start = 0,
    rates = { "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" } },
  { name = "Open HH",   note = 46, start = 0,
    rates = { "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" } },
  { name = "Crash",     note = 49, start = 0,
    rates = { "1/16", "1/8", "1/4", "1/2", "1/1" } },
  { name = "Ride",      note = 51, start = 0,
    rates = { "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" } },
  -- The toms are a single hit until they are thought through.
  { name = "Low Tom",   note = 41, start = 0, rates = {} },
  { name = "Mid Tom",   note = 47, start = 0, rates = {} },
  { name = "High Tom",  note = 50, start = 0, rates = {} },
}

M.CATEGORIES = { "Chord", "Arpeggio", "Run", "Melody", "Bass", "Drums" }

------------------------------------------------------------------------------
-- Settings
--
-- One plain table describes a block completely. Every function below is a pure
-- function of it, so a test can build one, ask for the notes, and check them.
------------------------------------------------------------------------------

function M.newState()
  return {
    root = 1, scale = 1, degree = 0,       -- degree is 0-based: 0 is the tonic
    cat = "Chord",
    family = 1, dia = 1, chord = 1,        -- family 1 is Diatonic
    inv = 0, oct = 0,
    pattern = 1,                           -- an index into DIRECTIONS
    runDir = 1,
    rate = 4, rateMod = 1,                 -- 1/8 straight
    chop = 7,                              -- the chord is one 1/1 segment
    octaves = 1, repeats = 1, bars = 1,
    lengthMode = "Repeats",                -- how an arpeggio or run is measured
    gate = 90,
    interval = 1, melDir = 1, shape = 1,   -- a step, up, on its own
    bassTone = 1, bassOct = -1,
    drumPiece = 1, drumRate = "1/1", shuffle = 0,
    baseOct = 4,                           -- C4 is 60
    barBeats = 4,                          -- what the project says a bar is
  }
end

------------------------------------------------------------------------------
-- Theory
------------------------------------------------------------------------------

function M.scaleLen(st) return #M.SCALES[st.scale].iv end

-- The MIDI pitch of a scale degree, counting past the top of the scale into
-- the octave above and below zero into the one below.
function M.scalePitch(st, degree)
  local sc  = M.SCALES[st.scale]
  local n   = #sc.iv
  local oct = math.floor(degree / n)
  local k   = degree - oct * n
  local rt  = M.ROOTS[st.root]
  local pc  = (LETTER_PC[rt.letter + 1] + rt.acc + 120) % 12
  return pc + (st.baseOct + 1) * 12 + sc.iv[k + 1] + oct * 12
end

-- Spelled for the key: the seventh of F# major comes out E#, not F.
function M.noteName(st, degree)
  local sc  = M.SCALES[st.scale]
  local n   = #sc.iv
  local oct = math.floor(degree / n)
  local k   = degree - oct * n
  local letter = (M.ROOTS[st.root].letter + sc.letters[k + 1]) % 7
  local acc = M.scalePitch(st, degree) % 12 - LETTER_PC[letter + 1]
  if acc >  6 then acc = acc - 12 end
  if acc < -6 then acc = acc + 12 end
  return LETTERS[letter + 1] .. (ACCIDENTAL[acc] or "?")
end

-- Read off the scale rather than assumed, so the modes and the blues scales
-- come out right: the vii of major is diminished, the III of natural minor is
-- major.
function M.degreeQuality(st, degree)
  local p  = M.scalePitch(st, degree)
  local r3 = M.scalePitch(st, degree + 2) - p
  local r5 = M.scalePitch(st, degree + 4) - p
  if r3 == 4 and r5 == 7 then return "major"      end
  if r3 == 3 and r5 == 7 then return "minor"      end
  if r3 == 3 and r5 == 6 then return "diminished" end
  if r3 == 4 and r5 == 8 then return "augmented"  end
  return "other"
end

function M.degreeNumeral(st, degree, ascii)
  local q = M.degreeQuality(st, degree)
  local n = NUMERALS[(degree % 8) + 1]
  if q == "minor" or q == "diminished" then n = n:lower() end
  if q == "diminished" then n = n .. (ascii and "dim" or "\u{00B0}") end
  if q == "augmented"  then n = n .. (ascii and "aug" or "+") end
  return n
end

-- Only the seven-note scales carry these names, and the seventh is a leading
-- tone only when it really does lean on the tonic a semitone above.
function M.degreeTitle(st, degree)
  if M.scaleLen(st) ~= 7 then return "Degree " .. (degree + 1) end
  if degree == 6 and M.scalePitch(st, 7) - M.scalePitch(st, 6) ~= 1 then
    return "Subtonic"
  end
  return M.DEGREE_TITLES[degree + 1] or ("Degree " .. (degree + 1))
end

-- The chord's pitches, ascending. inv lifts that many of the lowest voices an
-- octave, one at a time.
function M.chordTones(st, degree, inv)
  local tones = {}
  if st.family == 1 then
    for _, off in ipairs(M.DIATONIC[st.dia].offsets) do
      tones[#tones + 1] = M.scalePitch(st, degree + off) + st.oct * 12
    end
  else
    local base = M.scalePitch(st, degree) + st.oct * 12
    for _, iv in ipairs(M.CHORDS[st.chord].iv) do
      tones[#tones + 1] = base + iv
    end
  end
  for _ = 1, math.min(inv or 0, #tones - 1) do
    local lifted = table.remove(tones, 1) + 12
    tones[#tones + 1] = lifted
  end
  return tones
end

function M.rateByName(name)
  for _, r in ipairs(M.RATES) do if r.name == name then return r.beats end end
  return 4
end

-- Straight, triplet or dotted. One setting, shown on every panel, applied
-- wherever that panel reads a rate: the chord's chop, the drum's spacing, and
-- the step an arpeggio, run, melody or bass line walks in.
function M.modMul(st) return M.RATE_MODS[st.rateMod].mul end

-- What to put after a rate in a block's name, so "1/8" and "1/8 triplet" are
-- not two files with the same name.
function M.modSuffix(st)
  local name = M.RATE_MODS[st.rateMod].name
  if name == "Triplet" then return "T" end
  if name == "Dotted"  then return "." end
  return ""
end

-- How often the chosen piece is hit, or nil when it only ever gets one hit.
function M.drumStep(st)
  local piece = M.DRUM_PIECES[st.drumPiece]
  if #piece.rates == 0 then return nil end
  for _, r in ipairs(piece.rates) do
    if r == st.drumRate then return M.rateByName(r) * M.modMul(st) end
  end
  -- 1/1 is always last
  return M.rateByName(piece.rates[#piece.rates]) * M.modMul(st)
end

function M.rateBeats(st)
  return M.RATES[st.rate].beats * M.modMul(st)
end

-- The chord's segment, which is its own rate rather than the shared one.
function M.chopBeats(st)
  return M.RATES[st.chop].beats * M.modMul(st)
end

-- Settings can arrive from a saved project written by an older version, or one
-- whose tables were a different size. Every index here is used to look
-- something up, so anything past the end of its table has to come back inside
-- it before the rest of the program trusts it.
function M.clampState(st)
  local function pin(v, lo, hi, fallback)
    v = tonumber(v) or fallback
    return math.max(lo, math.min(math.floor(v), hi))
  end

  st.root     = pin(st.root, 1, #M.ROOTS, 1)
  st.scale    = pin(st.scale, 1, #M.SCALES, 1)
  st.family   = pin(st.family, 1, #M.FAMILIES, 1)
  st.chord    = pin(st.chord, 1, #M.CHORDS, 1)
  st.dia      = pin(st.dia, 1, #M.DIATONIC, 1)
  st.rate     = pin(st.rate, 1, #M.RATES, 4)
  st.rateMod  = pin(st.rateMod, 1, #M.RATE_MODS, 1)
  st.runDir   = pin(st.runDir, 1, #M.DIRECTIONS, 1)
  st.pattern  = pin(st.pattern, 1, #M.DIRECTIONS, 1)
  st.interval = pin(st.interval, 1, #M.INTERVALS, 1)
  st.melDir   = pin(st.melDir, 1, 2, 1)
  st.shape    = pin(st.shape, 1, #M.SHAPES, 1)
  st.bassTone = pin(st.bassTone, 1, #M.BASS_TONES, 1)
  st.drumPiece   = pin(st.drumPiece, 1, #M.DRUM_PIECES, 1)
  st.chop        = pin(st.chop, 1, #M.RATES, #M.RATES)

  -- The drum rate is kept by name rather than by index, so switching from a
  -- kick to a snare keeps 1/8 as 1/8 instead of sliding it up the list. A name
  -- the piece does not offer falls back to a single hit.
  local known = false
  for _, r in ipairs(M.RATES) do if r.name == st.drumRate then known = true end end
  if not known then st.drumRate = "1/1" end

  -- Ranges the sliders declare. A value outside one of these is what ReaImGui
  -- refuses, so they have to agree with the UI.
  st.inv      = pin(st.inv, 0, 3, 0)
  st.oct      = pin(st.oct, -3, 3, 0)
  st.bassOct  = pin(st.bassOct, -3, 0, -1)
  st.octaves  = pin(st.octaves, 1, 4, 1)
  st.repeats  = pin(st.repeats, 1, M.MAX_REPEATS, 1)
  st.gate     = pin(st.gate, 5, 100, 90)
  st.shuffle  = pin(st.shuffle, 0, 100, 0)
  st.baseOct  = pin(st.baseOct, 0, 8, 4)
  -- Bars are picked from a list rather than typed, and the list has fractions
  -- in it, so snap to the nearest entry instead of rounding to an integer.
  -- The list ascends and the comparison is strict, so a value exactly between
  -- two entries keeps the shorter: a block that is too short is easier to
  -- notice than one that is too long.
  local bars, best = tonumber(st.bars) or 1, nil
  for _, b in ipairs(M.BAR_LENGTHS) do
    if not best or math.abs(b.bars - bars) < math.abs(best - bars) then best = b.bars end
  end
  st.bars = best

  local mode = false
  for _, m in ipairs(M.LENGTH_MODES) do if m == st.lengthMode then mode = true end end
  if not mode then st.lengthMode = M.LENGTH_MODES[1] end

  local found = false
  for _, c in ipairs(M.CATEGORIES) do if c == st.cat then found = true end end
  if not found then st.cat = M.CATEGORIES[1] end

  st.degree = pin(st.degree, 0, M.scaleLen(st) - 1, 0)
  return st
end

function M.chordLabel(st)
  if st.family == 1 then return M.DIATONIC[st.dia].name end
  return M.CHORDS[st.chord].sym
end

------------------------------------------------------------------------------
-- Generators
--
-- Each one fills c.notes and says how long the block it made is, in c.len.
-- The bar-based blocks - chord, bass, drums - are handed a length and fill it.
-- The others decide their own: a melodic cell is as long as its notes, and an
-- arpeggio or a run is as long as the number of repeats asks for.
------------------------------------------------------------------------------

local function addNote(c, start, len, pitch, vel)
  if #c.notes >= M.MAX_NOTES then c.truncated = true; return end
  if pitch < 0 or pitch > 127 then return end
  c.notes[#c.notes + 1] = {
    start = start,
    len   = math.max(len, 0.015),
    pitch = math.floor(pitch),
    vel   = math.max(1, math.min(127, math.floor(vel))),
  }
end

-- Lays a list of pitches out in playing order for one of the seven directions.
local function applyDirection(pool, dir)
  local out, m = {}, #pool
  if m == 0 then return out end
  local name = M.DIRECTIONS[dir]

  if name == "Up" then
    for i = 1, m do out[#out + 1] = pool[i] end

  elseif name == "Down" then
    for i = m, 1, -1 do out[#out + 1] = pool[i] end

  elseif name == "Up/Down" then
    for i = 1, m do out[#out + 1] = pool[i] end
    for i = m - 1, 2, -1 do out[#out + 1] = pool[i] end

  elseif name == "Down/Up" then
    for i = m, 1, -1 do out[#out + 1] = pool[i] end
    for i = 2, m - 1 do out[#out + 1] = pool[i] end

  elseif name == "Random" then
    -- A shuffle rather than free picks, so every pitch gets its turn before
    -- any of them repeats.
    for i = 1, m do out[i] = pool[i] end
    for i = m, 2, -1 do
      local j = math.random(i)
      out[i], out[j] = out[j], out[i]
    end

  elseif name == "Converge" then          -- outside in
    local lo, hi = 1, m
    while lo <= hi do
      out[#out + 1] = pool[lo]; lo = lo + 1
      if lo <= hi then out[#out + 1] = pool[hi]; hi = hi - 1 end
    end

  elseif name == "Diverge" then           -- middle out
    local lo = math.floor((m + 1) / 2)
    local hi = lo + 1
    while lo >= 1 or hi <= m do
      if lo >= 1 then out[#out + 1] = pool[lo]; lo = lo - 1 end
      if hi <= m then out[#out + 1] = pool[hi]; hi = hi + 1 end
    end
  end
  return out
end
M._applyDirection = applyDirection

-- Plays a sequence through `count` times over, one pitch per step, and sizes
-- the block to exactly that. Counting the notes rather than walking a clock
-- keeps the last one from falling a rounding error short of the end.
local function layRepeats(st, c, seq, repeats)
  if #seq == 0 then return end
  local step  = M.rateBeats(st)
  local count = repeats * #seq
  for i = 0, count - 1 do
    addNote(c, i * step, step * st.gate / 100, seq[(i % #seq) + 1], M.VELOCITY)
    if c.truncated then break end
  end
  c.len = count * step
end

-- Cycles a sequence over a length that is already decided, which is how an
-- arpeggio worked before repeats existed: it plays until the block runs out,
-- wherever in the pass that happens to fall.
local function layFill(st, c, seq)
  if #seq == 0 then return end
  local step = M.rateBeats(st)
  local n    = math.max(1, math.ceil(c.len / step - 1e-9))
  for i = 0, n - 1 do
    local at = i * step
    addNote(c, at, math.min(step * st.gate / 100, c.len - at),
            seq[(i % #seq) + 1], M.VELOCITY)
    if c.truncated then return end
  end
end

-- Whichever way this block is being measured.
local function layOut(st, c, seq)
  if st.lengthMode == "Bars" then layFill(st, c, seq)
  else layRepeats(st, c, seq, st.repeats) end
end

-- How many notes a single pass of an arpeggio or a run comes to. Worth knowing
-- on screen: it is what one repeat actually costs.
function M.passLength(st)
  if st.cat == "Arpeggio" then
    local tones = M.chordTones(st, st.degree, st.inv)
    if #tones == 0 then return 0 end
    local pool = {}
    for o = 0, st.octaves - 1 do
      for _, p in ipairs(tones) do pool[#pool + 1] = p + o * 12 end
    end
    return #applyDirection(pool, st.pattern)
  elseif st.cat == "Run" then
    local n = M.scaleLen(st) * st.octaves + 1
    local pool = {}
    for i = 1, n do pool[i] = i end
    return #applyDirection(pool, st.runDir)
  end
  return 0
end

-- Shuffle pushes every second hit later. At 100 it lands two thirds of the way
-- through the pair, which is the triplet feel a shuffle is named after;
-- anything less is on the way there.
function M.swingOffset(st, index, step)
  if index % 2 == 0 then return 0 end
  return (st.shuffle or 0) / 100 * step / 3
end

local GEN = {}

GEN.Chord = function(st, c)
  -- The block is chopped into segments and the chord struck again in each one.
  -- At 1/1 over one bar that is a single held chord, which is what it was
  -- before the chop existed.
  local tones = M.chordTones(st, st.degree, st.inv)
  local seg   = M.chopBeats(st)
  local n     = math.max(1, math.ceil(c.len / seg - 1e-9))
  for i = 0, n - 1 do
    local at  = i * seg
    local len = math.min(seg, c.len - at) * st.gate / 100
    for _, p in ipairs(tones) do addNote(c, at, len, p, M.VELOCITY) end
    if c.truncated then return end
  end
end

GEN.Arpeggio = function(st, c)
  local tones = M.chordTones(st, st.degree, st.inv)
  if #tones == 0 then return end
  local pool = {}
  for o = 0, st.octaves - 1 do
    for _, p in ipairs(tones) do pool[#pool + 1] = p + o * 12 end
  end
  layOut(st, c, applyDirection(pool, st.pattern))
end

GEN.Run = function(st, c)
  local pool = {}
  -- Inclusive of the octave above, so a one-octave run lands back on the note
  -- it started from.
  for i = 0, M.scaleLen(st) * st.octaves do
    pool[#pool + 1] = M.scalePitch(st, st.degree + i) + st.oct * 12
  end
  layOut(st, c, applyDirection(pool, st.runDir))
end

function M.isSustain(st) return M.INTERVALS[st.interval].hold == true end

-- How many scale steps the cell moves. An octave is however many this
-- particular scale takes to get there, which is five in a pentatonic.
function M.melodySteps(st)
  local iv = M.INTERVALS[st.interval]
  if iv.hold then return 0 end
  if iv.octave then return M.scaleLen(st) end
  return iv.steps
end

-- A melodic cell is however long its own notes make it.
function M.melodyBeats(st)
  if M.isSustain(st) then return M.rateBeats(st) end
  local steps = M.melodySteps(st)
  local n = (st.shape == 1 and 2) or (st.shape == 2 and 3) or (steps + 1)
  return M.rateBeats(st) * n
end

GEN.Melody = function(st, c)
  local step = M.rateBeats(st)
  local len  = step * st.gate / 100
  local a    = M.scalePitch(st, st.degree) + st.oct * 12

  -- One note, held for the rate. Nothing moves, so there is no direction and
  -- no shape to give it.
  if M.isSustain(st) then
    addNote(c, 0, len, a, M.VELOCITY)
    c.len = step
    return
  end

  -- A 2nd is one scale step, a 3rd is two, and the octave is however many
  -- steps this particular scale takes to get there.
  local steps = M.melodySteps(st)
  local dir   = (st.melDir == 1) and 1 or -1
  local b = M.scalePitch(st, st.degree + dir * steps) + st.oct * 12

  if st.shape == 1 then                      -- Single: the move
    addNote(c, 0, len, a, M.VELOCITY)
    addNote(c, step, len, b, M.VELOCITY)
  elseif st.shape == 2 then                  -- Return: there and back
    addNote(c, 0, len, a, M.VELOCITY)
    addNote(c, step, len, b, M.VELOCITY)
    addNote(c, step * 2, len, a, M.VELOCITY)
  else                                       -- Fill: every note in between
    for i = 0, steps do
      addNote(c, i * step, len,
              M.scalePitch(st, st.degree + dir * i) + st.oct * 12, M.VELOCITY)
    end
  end
  c.len = M.melodyBeats(st)
end

GEN.Bass = function(st, c)
  -- The bass reads the chord as it is stacked, so inversion is ignored: voice
  -- 1 is the root, 2 the third, 3 the fifth, 4 the seventh.
  local tones = M.chordTones(st, st.degree, 0)
  if #tones == 0 then return end
  local pitch = tones[math.min(st.bassTone, #tones)] + st.bassOct * 12
  local step  = M.rateBeats(st)
  local pos   = 0
  while pos < c.len - 1e-9 do
    addNote(c, pos, math.min(step * st.gate / 100, c.len - pos), pitch, M.VELOCITY)
    if c.truncated then return end
    pos = pos + step
  end
end

GEN.Drums = function(st, c)
  local piece = M.DRUM_PIECES[st.drumPiece]
  local step  = M.drumStep(st)

  -- The pattern belongs to a bar and repeats with it, but the block can be a
  -- fraction of one, so walk bars and drop anything past the end of the block.
  local bar = 0
  while bar * st.barBeats < c.len - 1e-9 do
    local base = bar * st.barBeats

    if not step then                          -- a single hit, nothing to space
      if base + piece.start < c.len - 1e-9 then
        addNote(c, base + piece.start, 0.1, piece.note, M.VELOCITY)
      end
    else
      local i, at = 0, piece.start
      while at < st.barBeats - 1e-9 do
        local hit = base + at + M.swingOffset(st, i, step)
        if hit < c.len - 1e-9 then
          addNote(c, hit, 0.1, piece.note, M.VELOCITY)
          if c.truncated then return end
        end
        i  = i + 1
        at = piece.start + i * step
      end
    end
    bar = bar + 1
  end
end

------------------------------------------------------------------------------
-- The block
------------------------------------------------------------------------------

-- The label for whatever st.bars currently is, as the list spells it.
function M.barsLabel(st)
  for _, b in ipairs(M.BAR_LENGTHS) do
    if math.abs(b.bars - st.bars) < 1e-9 then return b.name end
  end
  return tostring(st.bars)
end

function M.blockName(st)
  local root  = M.ROOTS[st.root].name
  local scale = M.SCALES[st.scale].name
  local where = M.degreeNumeral(st, st.degree, true)
  local rate  = M.RATES[st.rate].name .. M.modSuffix(st)
  -- How the block was measured belongs in its name: "x3" is three passes,
  -- "1/2 bar" is a length the pass was cut to fit.
  local times = ""
  if st.lengthMode == "Bars" then
    times = " " .. M.barsLabel(st) .. " bar"
  elseif st.repeats > 1 then
    times = " x" .. st.repeats
  end

  if st.cat == "Chord" then
    local chop = (st.chop < #M.RATES or st.rateMod > 1)
                 and (" " .. M.RATES[st.chop].name .. M.modSuffix(st)) or ""
    return ("%s %s %s Chord %s%s"):format(root, scale, where,
                                          M.chordLabel(st), chop)
  elseif st.cat == "Arpeggio" then
    return ("%s %s %s Arp %s %s %s%s"):format(root, scale, where,
      M.chordLabel(st), M.DIRECTIONS[st.pattern], rate, times)
  elseif st.cat == "Run" then
    return ("%s %s %s Run %s %s%s"):format(root, scale, where,
      M.DIRECTIONS[st.runDir], rate, times)
  elseif st.cat == "Melody" then
    -- A sustained note has no direction and no shape, so its name claims
    -- neither of them.
    if M.isSustain(st) then
      return ("%s %s %s Melody Sustain %s"):format(root, scale, where, rate)
    end
    return ("%s %s %s Melody %s %s %s"):format(root, scale, where,
      (st.melDir == 1) and "Up" or "Down",
      M.INTERVALS[st.interval].name, M.SHAPES[st.shape])
  elseif st.cat == "Bass" then
    return ("%s %s %s Bass %s %s"):format(root, scale, where,
      M.BASS_TONES[st.bassTone], rate)
  end
  -- A single hit has no second hit to push, so a piece with no rates never
  -- claims a shuffle it cannot have used.
  local piece = M.DRUM_PIECES[st.drumPiece]
  if #piece.rates == 0 then return "Drum " .. piece.name end
  local swing = (st.shuffle > 0) and (" shuffle " .. st.shuffle) or ""
  return ("Drum %s %s%s%s"):format(piece.name, st.drumRate, M.modSuffix(st), swing)
end

-- The whole point of the file. Returns the notes in quarter notes from the
-- start of the block, how long the block is, and what it is called.
function M.generate(st)
  -- The length a bar-based block fills. A self-sizing one replaces it.
  local c = { notes = {}, len = st.barBeats * st.bars, truncated = false }
  ;(GEN[st.cat] or GEN.Chord)(st, c)
  return {
    notes     = c.notes,
    beats     = math.max(c.len, 0.0625),
    name      = M.blockName(st),
    truncated = c.truncated,
  }
end

return M
