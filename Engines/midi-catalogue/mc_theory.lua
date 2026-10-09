--[[ Midi Catalogue - keys, chords and voice leading.

     Pure Lua. Nothing in this file touches REAPER or ImGui: the harmony is the
     part that has to be right, and here it can be run and checked by
     tests/test_theory.lua rather than only by reading.

     Two ways of naming a note run through the whole engine:

       - a MIDI pitch, 0..127;
       - a scale position, an integer counting scale notes from the key's root
         in MIDI octave -1. Position 0 is the root at pitch 0..11, position n
         (the scale's length) is the root an octave up, and so on.

     Every diatonic move is integer arithmetic on positions - a third above is
     +2, whatever the scale - and pitches only appear at the edges. Scale
     degrees are 0-based: degree 0 is the tonic.
]]

local M = {}

------------------------------------------------------------------------------
-- Keys
--
-- ScaleView for REAPER's roots and scales, unchanged, the same tables Starting
-- Blocks copies, so all three apps agree on what a scale is and what to call
-- its notes. test_theory.lua asserts they still match. Do not tidy them
-- independently.
------------------------------------------------------------------------------

local LETTER_PC  = { 0, 2, 4, 5, 7, 9, 11 }        -- C D E F G A B
local LETTERS    = { "C", "D", "E", "F", "G", "A", "B" }
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

local NUMERALS = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII" }

-- A key is the pair of indices the window picks. A chord scale (M.chordKey)
-- is a key that also carries its own intervals: the same scale with a note
-- or two bent to the chord sounding over it.
function M.key(root, scale) return { root = root or 1, scale = scale or 1 } end

local function ivOf(key) return key.iv or M.SCALES[key.scale].iv end
M.ivOf = ivOf

function M.scaleLen(key) return #ivOf(key) end

function M.rootPc(key)
  local rt = M.ROOTS[key.root]
  return (LETTER_PC[rt.letter + 1] + rt.acc + 12) % 12
end

------------------------------------------------------------------------------
-- Positions and pitches
------------------------------------------------------------------------------

-- The MIDI pitch of a scale position.
function M.pitch(key, pos)
  local iv  = ivOf(key)
  local n   = #iv
  local oct = math.floor(pos / n)
  local k   = pos - oct * n
  return M.rootPc(key) + iv[k + 1] + 12 * oct
end

-- The highest position at or below a pitch. Every pitch has one, because the
-- root is in every octave.
function M.floorPos(key, midi)
  local iv   = ivOf(key)
  local n    = #iv
  local root = M.rootPc(key)
  local oct  = math.floor((midi - root) / 12)
  local kmax = 0
  for k = 1, n do
    if root + iv[k] + 12 * oct <= midi then kmax = k - 1 end
  end
  return oct * n + kmax
end

-- The position of a pitch that is in the scale, or nil for one that is not.
function M.posOf(key, midi)
  local s = M.floorPos(key, midi)
  if M.pitch(key, s) == midi then return s end
  return nil
end

-- The nearest position to any pitch. A pitch exactly between two scale notes
-- goes the way `lean` says (+1 up, -1 down), and down when it says nothing.
function M.nearestPos(key, midi, lean)
  local lo = M.floorPos(key, midi)
  if M.pitch(key, lo) == midi then return lo end
  local dLo = midi - M.pitch(key, lo)
  local dHi = M.pitch(key, lo + 1) - midi
  if dLo < dHi then return lo end
  if dHi < dLo then return lo + 1 end
  return (lean or -1) > 0 and lo + 1 or lo
end

function M.pc(key, pos) return M.pitch(key, pos) % 12 end

-- Spelled for the key: the seventh of F# major comes out E#, not F.
function M.noteName(key, pos)
  local sc  = M.SCALES[key.scale]
  local n   = #ivOf(key)
  local oct = math.floor(pos / n)
  local k   = pos - oct * n
  local letter = (M.ROOTS[key.root].letter + sc.letters[k + 1]) % 7
  local acc = M.pitch(key, pos) % 12 - LETTER_PC[letter + 1]
  if acc >  6 then acc = acc - 12 end
  if acc < -6 then acc = acc + 12 end
  return LETTERS[letter + 1] .. (ACCIDENTAL[acc] or "?")
end

-- A pitch named with its octave, C4 being middle C (60). Out of the key, the
-- sharp spelling.
local SHARP_NAMES = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }
function M.pitchName(midi, key)
  local oct = math.floor(midi / 12) - 1
  if key then
    local s = M.posOf(key, midi)
    if s then return M.noteName(key, s) .. oct end
  end
  return SHARP_NAMES[midi % 12 + 1] .. oct
end

------------------------------------------------------------------------------
-- Degrees
------------------------------------------------------------------------------

-- Read off the scale rather than assumed, so the modes and the blues scales
-- come out right: the vii of major is diminished, the III of minor is major.
function M.degreeQuality(key, degree)
  local p  = M.pitch(key, degree)
  local r3 = M.pitch(key, degree + 2) - p
  local r5 = M.pitch(key, degree + 4) - p
  if r3 == 4 and r5 == 7 then return "major"      end
  if r3 == 3 and r5 == 7 then return "minor"      end
  if r3 == 3 and r5 == 6 then return "diminished" end
  if r3 == 4 and r5 == 8 then return "augmented"  end
  return "other"
end

function M.degreeNumeral(key, degree, ascii)
  local q = M.degreeQuality(key, degree)
  local n = NUMERALS[(degree % 8) + 1]
  if q == "minor" or q == "diminished" then n = n:lower() end
  if q == "diminished" then n = n .. (ascii and "dim" or "\u{00B0}") end
  if q == "augmented"  then n = n .. (ascii and "aug" or "+") end
  return n
end

M.DEGREE_TITLES = { "Tonic", "Supertonic", "Mediant", "Subdominant",
                    "Dominant", "Submediant", "Leading Tone" }

-- Starting Blocks' degree names. Only the seven-note scales carry them, and
-- the seventh is a leading tone only when it leans on the tonic a semitone up.
function M.degreeTitle(key, degree)
  if M.scaleLen(key) ~= 7 then return "Degree " .. (degree + 1) end
  if degree == 6 and M.pitch(key, 7) - M.pitch(key, 6) ~= 1 then return "Subtonic" end
  return M.DEGREE_TITLES[degree + 1] or ("Degree " .. (degree + 1))
end

-- The seventh degree leans on the tonic only when it sits a semitone under
-- it. Only then is it a leading tone, and only then does it want resolving
-- and not doubling.
function M.leadingTonePc(key)
  local n = M.scaleLen(key)
  if n ~= 7 then return nil end
  if M.pitch(key, 7) - M.pitch(key, 6) == 1 then return M.pc(key, 6) end
  return nil
end

------------------------------------------------------------------------------
-- Progressions
--
-- The chords under everything, as scale degrees. One chord to a slot; the
-- slots share the block's length evenly. A progression whose degrees the
-- scale does not have (a sixth degree in a pentatonic) is simply not offered.
------------------------------------------------------------------------------

M.PROGRESSIONS = {
  { id = "I",        degrees = { 0 } },
  { id = "I-V",      degrees = { 0, 4 } },
  { id = "I-IV",     degrees = { 0, 3 } },
  { id = "I-IV-V-I", degrees = { 0, 3, 4, 0 } },
  { id = "I-V-vi-IV", degrees = { 0, 4, 5, 3 } },
  { id = "I-vi-IV-V", degrees = { 0, 5, 3, 4 } },
  { id = "vi-IV-I-V", degrees = { 5, 3, 0, 4 } },
  { id = "ii-V-I",   degrees = { 1, 4, 0, 0 } },
  { id = "i-VI-III-VII", degrees = { 0, 5, 2, 6 } },
  { id = "i-VII-VI-V", degrees = { 0, 6, 5, 4 } },   -- the Andalusian cadence
  { id = "i-iv-v-i", degrees = { 0, 3, 4, 0 } },
}

-- The ids above are fixed names for saving; what the window shows is the
-- numerals this scale actually builds, so I-IV-V-I in minor reads i-iv-v-i.
function M.progressionName(key, prog)
  local out = {}
  for i, d in ipairs(prog.degrees) do out[i] = M.degreeNumeral(key, d, true) end
  return table.concat(out, "-")
end

function M.progressionFits(key, prog)
  for _, d in ipairs(prog.degrees) do
    if d >= M.scaleLen(key) then return false end
  end
  return true
end

-- The progressions this scale can play, keeping only the first of any two
-- that come out the same here (I-IV-V-I and i-iv-v-i are one thing in any
-- given scale).
function M.progressionsFor(key)
  local out, seen = {}, {}
  for _, p in ipairs(M.PROGRESSIONS) do
    if M.progressionFits(key, p) then
      local sig = table.concat(p.degrees, ",")
      if not seen[sig] then seen[sig] = true; out[#out + 1] = p end
    end
  end
  return out
end

function M.progressionById(id)
  for _, p in ipairs(M.PROGRESSIONS) do if p.id == id then return p end end
  return M.PROGRESSIONS[1]
end

------------------------------------------------------------------------------
-- Chords
--
-- Starting Blocks' chord tables, copied unchanged: one row per chord carrying
-- its own name, symbol and intervals (semitones from the chord's root), in
-- the families of Wikipedia's list of chords, plus the diatonic chords the
-- scale hands you for free. Do not tidy them independently of Starting
-- Blocks.
------------------------------------------------------------------------------

M.FAMILIES = { "Diatonic", "Triads", "6ths & 7ths", "Extended", "Altered",
               "Sus & Add", "Quartal", "Named" }

local TR, S7, EX, AL, SA, QU, NA = 2, 3, 4, 5, 6, 7, 8   -- indices into FAMILIES

M.CHORDS = {
  { sym="maj",  name="Major",                  iv={0,4,7},          fam=TR },
  { sym="m",    name="Minor",                  iv={0,3,7},          fam=TR },
  { sym="dim",  name="Diminished",             iv={0,3,6},          fam=TR },
  { sym="aug",  name="Augmented",              iv={0,4,8},          fam=TR },
  { sym="b5",   name="Flat Five",              iv={0,4,6},          fam=TR },
  { sym="5",    name="Fifth (Power)",          iv={0,7},            fam=TR },

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

-- The chords the key hands you for free, as offsets in scale steps from the
-- degree. Always in key, which is why this is the first family. `suffix` is
-- what goes after the numeral in a progression's name.
M.DIATONIC = {
  { name = "Triad", offsets = {0,2,4},           suffix = "" },
  { name = "7th",   offsets = {0,2,4,6},         suffix = "7" },
  { name = "9th",   offsets = {0,2,4,6,8},       suffix = "9" },
  { name = "11th",  offsets = {0,2,4,6,8,10},    suffix = "11" },
  { name = "13th",  offsets = {0,2,4,6,8,10,12}, suffix = "13" },
  { name = "6th",   offsets = {0,2,4,5},         suffix = "6" },
  { name = "sus2",  offsets = {0,1,4},           suffix = "sus2" },
  { name = "sus4",  offsets = {0,3,4},           suffix = "sus4" },
  { name = "5th",   offsets = {0,4},             suffix = "5" },
}

-- Shapes the harmony types build for themselves, whatever chord was chosen:
-- a quartal or cluster texture stacks its own steps on the chord's degree.
M.CHORD_KINDS = {
  triad   = { 0, 2, 4 },
  seventh = { 0, 2, 4, 6 },
  quartal = { 0, 3, 6 },
  quartal4 = { 0, 3, 6, 9 },
  cluster = { 0, 1, 2 },
  cluster4 = { 0, 1, 2, 3 },
}

function M.diatonicByName(name)
  for _, d in ipairs(M.DIATONIC) do if d.name == name then return d end end
  return nil
end

function M.chordBySym(sym)
  for _, c in ipairs(M.CHORDS) do if c.sym == sym then return c end end
  return nil
end

-- Which members a voicing may not leave out. The root and the third make a
-- chord what it is, a seventh makes it a seventh chord, and the top of an
-- extended or altered chord is its colour; a fifth can go.
local function essentialFor(n, tertian)
  if n <= 2 then local e = {}; for i = 1, n do e[i] = i end; return e end
  if n == 3 then return tertian and { 1, 2 } or { 1, 2, 3 } end
  local e = { 1, 2, 4 }
  if n >= 5 then e[#e + 1] = n end
  return e
end

-- A chord on a scale degree. `spec` is one of:
--   a kind name from M.CHORD_KINDS ("triad", "quartal4", ...), stacked from
--     the scale;
--   { fam = "d", name = "7th" }, a diatonic chord from M.DIATONIC;
--   { fam = "c", sym = "maj7" }, a chord from M.CHORDS on the degree's root,
--     whatever the scale says - which is how a borrowed or altered chord gets
--     into a progression.
-- Returns pitch classes in member order (root first), with `essential`.
function M.chord(key, degree, spec)
  local pcs, seen = {}, {}
  local function addPc(pc)
    pc = pc % 12
    if not seen[pc] then seen[pc] = true; pcs[#pcs + 1] = pc end
  end
  local kind, tertian, ivs
  if type(spec) == "table" and spec.fam == "c" then
    local c = M.chordBySym(spec.sym) or M.CHORDS[1]
    local root = M.pc(key, degree)
    for _, iv in ipairs(c.iv) do addPc(root + iv) end
    tertian = (c.iv[2] == 3 or c.iv[2] == 4)
    kind = (#pcs == 3 and tertian and c.iv[3] == 7) and "triad" or c.sym
    ivs = c.iv
  else
    local offs
    if type(spec) == "table" then
      local d = M.diatonicByName(spec.name) or M.DIATONIC[1]
      offs = d.offsets
      kind = (d.name == "Triad") and "triad" or ((d.name == "7th") and "seventh" or d.name)
      tertian = not d.name:find("sus")
    else
      offs = M.CHORD_KINDS[spec] or M.CHORD_KINDS.triad
      kind = spec or "triad"
      tertian = (kind == "triad" or kind == "seventh")
    end
    for _, o in ipairs(offs) do addPc(M.pc(key, degree + o)) end
  end
  local essential
  if kind == "triad" then essential = { 1, 2 }
  elseif kind == "seventh" then essential = { 1, 2, 4 }
  elseif type(spec) == "string" then essential = {}; for i = 1, #pcs do essential[i] = i end
  else essential = essentialFor(#pcs, tertian) end
  return { degree = degree, kind = kind, pcs = pcs, root = pcs[1], essential = essential, ivs = ivs }
end

-- The scale as it sounds under a chord. A chord can hold notes the scale does
-- not - the major third of a borrowed I in a minor key, the leading tone of a
-- V7 in natural minor, the F# of a D7 in C. Under that chord the scale bends
-- to meet it: the scale note on the same letter as the chord's note moves the
-- semitone to it, so a C major chord in C minor turns Eb into E for as long as
-- it lasts, and melodies and figures written in scale steps land on the
-- chord's own notes instead of grinding against them. A scale note that is
-- itself in the chord is never moved, and a chord note two semitones from
-- anything in the scale is left alone.
--
-- Which scale note a chord note belongs to is read off the interval, the way
-- it is spelled: a third is two steps up, a flat five or a sharp five is still
-- the fifth, a sharp eleven is the fourth step.
local STEPS_IN_OCTAVE = { [0]=0, 1, 1, 2, 2, 3, 4, 4, 4, 5, 6, 6 }
local STEPS_ABOVE     = { [0]=0, 1, 1, 1, 2, 3, 3, 4, 5, 5, 6, 6 }
function M.ivSteps(iv)
  if iv < 12 then return STEPS_IN_OCTAVE[iv] end
  return 7 + STEPS_ABOVE[(iv - 12) % 12]
end

function M.chordKey(key, chord)
  local base = ivOf(key)
  local n = #base
  if n ~= 7 or not chord.ivs then
    -- Only a chord built from outside the scale can need bending, and only a
    -- seven-note scale has a letter for every note to bend.
    return key
  end
  local iv = {}
  for i, v in ipairs(base) do iv[i] = v end
  local rootPc = M.rootPc(key)
  local changed = false
  for _, civ in ipairs(chord.ivs) do
    local pc = (M.pc(key, chord.degree) + civ) % 12
    local inScale = false
    for _, v in ipairs(iv) do if (rootPc + v) % 12 == pc then inScale = true end end
    if not inScale then
      local k = (chord.degree + M.ivSteps(civ)) % n
      local cur = (rootPc + iv[k + 1]) % 12
      local diff = (pc - cur + 12) % 12
      if (diff == 1 or diff == 11) and not M.hasPc(chord, cur) then
        local nv = iv[k + 1] + ((diff == 1) and 1 or -1)
        local below = (k > 0) and iv[k] or -1
        local above = (k < n - 1) and iv[k + 2] or 12
        if nv > below and nv < above then iv[k + 1] = nv; changed = true end
      end
    end
  end
  if not changed then return key end
  return { root = key.root, scale = key.scale, iv = iv }
end

------------------------------------------------------------------------------
-- A chain of chords
--
-- The progression is a chain the user builds: each link is a scale degree and
-- a chord on it, { degree, fam = "d" | "c", name }, where `name` is a diatonic
-- chord's name or a chord's symbol. Kept by name, never by index, so it
-- survives the tables growing. Saved as text, "0:d:Triad,4:c:7".
------------------------------------------------------------------------------

M.MAX_CHAIN = 8

function M.chainString(chain)
  local out = {}
  for i, c in ipairs(chain) do out[i] = c.degree .. ":" .. c.fam .. ":" .. c.name end
  return table.concat(out, ",")
end

-- Reads a chain back, dropping any link that no longer means anything in this
-- scale (a sixth degree in a pentatonic, a chord that is not in the tables).
function M.parseChain(s, key)
  local out = {}
  for tok in tostring(s or ""):gmatch("[^,]+") do
    local d, fam, name = tok:match("^(%-?%d+):([dc]):(.+)$")
    d = tonumber(d)
    if d and d >= 0 and d < M.scaleLen(key) and #out < M.MAX_CHAIN then
      if (fam == "d" and M.diatonicByName(name)) or (fam == "c" and M.chordBySym(name)) then
        out[#out + 1] = { degree = d, fam = fam, name = name }
      end
    end
  end
  return out
end

function M.chainSpec(link)
  if link.fam == "c" then return { fam = "c", sym = link.name } end
  return { fam = "d", name = link.name }
end

-- How a link is written in a progression: the numeral of its degree, cased
-- by the chord the scale builds there, then the chord. A diatonic chord adds
-- only its extension (V7, ii9); a chosen chord takes an upper-case numeral
-- and its own symbol (IVmaj7, IIm7, bVI is not needed - the root is always a
-- degree of the scale).
function M.linkLabel(key, link, ascii)
  if link.fam == "d" then
    local d = M.diatonicByName(link.name) or M.DIATONIC[1]
    return M.degreeNumeral(key, link.degree, ascii) .. d.suffix
  end
  local sym = link.name
  local sep = sym:match("^%u") and " " or ""
  if sym == "maj" then sym = "" end
  return NUMERALS[(link.degree % 8) + 1] .. sep .. sym
end

function M.chainName(key, chain, ascii)
  local out = {}
  for i, link in ipairs(chain) do out[i] = M.linkLabel(key, link, ascii) end
  return table.concat(out, "-")
end

-- The symbol a set of pitch classes goes by, read off the chord tables: the
-- first chord whose notes are exactly these, from this root. "" for a major
-- triad, nil for a set with no name here.
function M.symbolOf(pcs, root)
  local want = {}
  for _, pc in ipairs(pcs) do want[(pc - root) % 12] = true end
  local n = 0
  for _ in pairs(want) do n = n + 1 end
  for _, c in ipairs(M.CHORDS) do
    local have, m = {}, 0
    for _, iv in ipairs(c.iv) do
      if not have[iv % 12] then have[iv % 12] = true; m = m + 1 end
    end
    if m == n then
      local same = true
      for k in pairs(want) do if not have[k] then same = false end end
      if same then return (c.sym == "maj") and "" or c.sym end
    end
  end
  return nil
end

-- A link's chord name and its notes, spelled in the scale as it sounds under
-- that chord: "C", "C E G" for a borrowed C major in C minor; "Dm7",
-- "D F A C" for ii7 in C major.
function M.linkSpelling(key, link)
  local ch = M.chord(key, link.degree, M.chainSpec(link))
  local ck = M.chordKey(key, ch)
  local names = {}
  for _, pc in ipairs(ch.pcs) do
    local name
    for s = link.degree, link.degree + 13 do
      if M.pc(ck, s) == pc then name = M.noteName(ck, s); break end
    end
    names[#names + 1] = name or SHARP_NAMES[pc + 1]
  end
  local sym = (link.fam == "c") and ((link.name == "maj") and "" or link.name)
              or M.symbolOf(ch.pcs, ch.root) or link.name
  return names[1] .. (sym:match("^%u") and " " or "") .. sym, table.concat(names, " ")
end

-- A chain from one of the progressions, every chord diatonic: triads, or
-- sevenths.
function M.presetChain(prog, seventh)
  local out = {}
  for i, d in ipairs(prog.degrees) do
    out[i] = { degree = d, fam = "d", name = seventh and "7th" or "Triad" }
  end
  return out
end

function M.hasPc(ch, pc)
  for _, p in ipairs(ch.pcs) do if p == pc % 12 then return true end end
  return false
end

------------------------------------------------------------------------------
-- Voice leading
--
-- A voicing is a list of MIDI pitches, bass first, strictly rising. Each voice
-- has its own range, which is how an ensemble keeps every part inside its
-- instrument: the violins' voice is looked for in the violins' range.
--
-- The rules are the ones a harmony teacher marks, as costs:
--
--   - move as little as possible, and keep common tones where they are;
--   - no parallel fifths or octaves between any two voices;
--   - no doubled leading tone, and the leading tone rises to the tonic;
--   - upper voices within an octave of each other, the bass wider;
--   - no voice crossing (the voicing is strictly rising, so it cannot).
--
-- The search keeps the best few paths through the progression rather than
-- only the best single step, so an early cheap move cannot corner a later
-- chord.
------------------------------------------------------------------------------

M.BEAM = 6

local function interval(a, b) return (b - a) % 12 end

-- Every voicing of one chord that the ranges and the spacing rules allow.
--
-- opts:
--   bassRoot    false lets the bass take any chord tone (an inversion)
--   fixedBass   an exact pitch the bass holds whatever the chord: a pedal
--   fixedTop    the same, for the top voice: an inverted pedal
--   adjacent    a set of the only intervals allowed between neighbouring
--               voices - fourths and fifths for quartal harmony, seconds
--               for clusters
--   close       the whole voicing inside an octave: close position
function M.candidates(ch, ranges, opts)
  opts = opts or {}
  local nv = #ranges
  local out = {}
  local options = {}
  for v = 1, nv do
    local list = {}
    if v == 1 and opts.fixedBass then list = { opts.fixedBass }
    elseif v == nv and opts.fixedTop then list = { opts.fixedTop }
    else
      for p = ranges[v][1], ranges[v][2] do
        local okPc
        if v == 1 and opts.bassRoot ~= false then okPc = (p % 12 == ch.root)
        else okPc = M.hasPc(ch, p) end
        if okPc then list[#list + 1] = p end
      end
    end
    options[v] = list
  end

  -- The members a voice must supply. A fixed bass or top is a pedal, not a
  -- chord tone, so the others have to cover the chord between them.
  local need = {}
  for _, m in ipairs(ch.essential) do need[#need + 1] = ch.pcs[m] end

  local cur = {}
  local function covered()
    for _, pc in ipairs(need) do
      local found = false
      for v = 1, nv do
        local pedal = (v == 1 and opts.fixedBass) or (v == nv and opts.fixedTop)
        if not pedal and cur[v] % 12 == pc then found = true; break end
      end
      if not found then return false end
    end
    return true
  end

  local function walk(v)
    if #out >= 20000 then return end
    if v > nv then
      if covered() then
        local c = {}
        for i = 1, nv do c[i] = cur[i] end
        out[#out + 1] = c
      end
      return
    end
    for _, p in ipairs(options[v]) do
      local fine = true
      if v > 1 then
        local below = cur[v - 1]
        local gap = p - below
        if gap <= 0 then fine = false
        elseif v == 2 then
          -- The bass may sit wider than the upper voices, but not past a
          -- twelfth, where it stops sounding like the bottom of this chord.
          -- A pedal is allowed two octaves: it is under the chords, not in them.
          if gap > (opts.fixedBass and 24 or 19) then fine = false end
        elseif gap > ((v == nv and opts.fixedTop) and 16 or 12) then fine = false end
        if fine and opts.adjacent and not opts.adjacent[gap] then fine = false end
        if fine and opts.close and v == nv and p - cur[1] > 12 then fine = false end
      end
      if fine then
        cur[v] = p
        walk(v + 1)
      end
    end
    cur[v] = nil
  end
  walk(1)
  return out
end

-- Parallel fifths and octaves: two voices a perfect fifth or octave apart
-- that both move, the same way, to the same interval again.
function M.parallels(a, b)
  local n = 0
  for i = 1, #a - 1 do
    for j = i + 1, #a do
      local was, now = interval(a[i], a[j]), interval(b[i], b[j])
      if (was == 0 or was == 7) and was == now then
        local mi, mj = b[i] - a[i], b[j] - a[j]
        if mi ~= 0 and mj ~= 0 and (mi > 0) == (mj > 0) then n = n + 1 end
      end
    end
  end
  return n
end

local function sign(x) if x > 0 then return 1 elseif x < 0 then return -1 end return 0 end

-- What one voicing costs, after `prev` (nil for the first chord).
function M.cost(ch, v, prev, opts, ctx)
  opts = opts or {}
  local nv, c = #v, 0

  -- Doubling. The leading tone is never doubled; in a triad in four or more
  -- voices, doubling the root is the textbook choice.
  local counts = {}
  for _, p in ipairs(v) do counts[p % 12] = (counts[p % 12] or 0) + 1 end
  if ctx.lt and (counts[ctx.lt] or 0) > 1 then c = c + 20 end
  if ch.kind == "triad" and nv >= 4 and (counts[ch.root] or 0) < 2 then c = c + 2 end
  -- A fifth is the member a full chord can most afford to lose, but a chord
  -- that could have had it and did not is thinner than it needed to be.
  for _, pc in ipairs(ch.pcs) do
    if not counts[pc] then c = c + 3 end
  end

  -- An inversion, where one is allowed at all.
  if opts.bassRoot == false and not opts.fixedBass then
    if v[1] % 12 ~= ch.root then
      c = c + ((ch.pcs[3] and v[1] % 12 == ch.pcs[3]) and 6 or 3)
    end
  end

  if not prev then
    -- The first chord is judged on where it sits and how it is spaced:
    -- wider at the bottom than the top, like the harmonic series.
    local sum = 0
    for _, p in ipairs(v) do sum = sum + p end
    c = c + math.abs(sum / nv - ctx.centre) * 0.6
    if nv >= 3 then
      local lowGap, topGap = v[2] - v[1], v[nv] - v[nv - 1]
      if lowGap < topGap then c = c + 2 end
    end
    return c
  end

  -- Movement, and common tones held.
  for i = 1, nv do
    local d = math.abs(v[i] - prev[i])
    c = c + d
    if d > 7 then c = c + (d - 7) * 2 end
  end

  if opts.noParallels ~= false then c = c + 50 * M.parallels(prev, v) end

  -- The leading tone in the top voice rises to the tonic when the tonic chord
  -- follows. Inner voices are allowed to let it fall, as they are in chorales.
  if ctx.lt and ctx.tonicPc and ch.root == ctx.tonicPc and prev[nv] % 12 == ctx.lt then
    if v[nv] - prev[nv] ~= 1 then c = c + 10 end
  end

  -- Outer voices in contrary motion, when that is the point of the block.
  if opts.contrary then
    local s, b = sign(v[nv] - prev[nv]), sign(v[1] - prev[1])
    if s ~= 0 and s == b then c = c + 12
    elseif s == 0 or b == 0 then c = c + 5 end
    if s ~= opts.contrary then c = c + 6 end
  end
  return c
end

-- The whole progression voiced. `chords` is a list from M.chord; `ranges` is
-- one {lo, hi} per voice, bass first. Returns one voicing per chord, or nil
-- when some chord has no voicing inside those ranges.
function M.voiceLead(key, chords, ranges, opts)
  opts = opts or {}
  local ctx = {
    lt = M.leadingTonePc(key), tonicPc = M.rootPc(key),
    centre = 0,
  }
  for _, r in ipairs(ranges) do ctx.centre = ctx.centre + (r[1] + r[2]) / 2 end
  ctx.centre = ctx.centre / #ranges

  local beam = { { cost = 0, path = {} } }
  for i, ch in ipairs(chords) do
    local cands = M.candidates(ch, ranges, opts)
    if #cands == 0 then return nil end
    local next_ = {}
    for _, state in ipairs(beam) do
      local prev = state.path[#state.path]
      for _, v in ipairs(cands) do
        local cc = state.cost + M.cost(ch, v, prev, opts, ctx)
        next_[#next_ + 1] = { cost = cc, path = state.path, v = v }
      end
    end
    table.sort(next_, function(a, b)
      if a.cost ~= b.cost then return a.cost < b.cost end
      -- Equal costs are settled by the voicing itself, so the answer never
      -- depends on the order the sort happened to see them in.
      for k = 1, #a.v do
        if a.v[k] ~= b.v[k] then return a.v[k] < b.v[k] end
      end
      return false
    end)
    beam = {}
    local seen = {}
    for _, s in ipairs(next_) do
      local sig = table.concat(s.v, ",")
      if not seen[sig] then
        seen[sig] = true
        local path = {}
        for k, p in ipairs(s.path) do path[k] = p end
        path[#path + 1] = s.v
        beam[#beam + 1] = { cost = s.cost, path = path }
        if #beam >= M.BEAM then break end
      end
    end
    if i == 1 and opts.firstOnly then break end
  end
  return beam[1].path
end

-- Diatonic planing: every voice moves by the same number of scale steps as
-- the root does. Parallel fourths are the sound of quartal harmony rather
-- than a fault in it, so this is not voice leading and does not pretend to be.
function M.plane(key, first, chords)
  local out = { first }
  local firstPos = {}
  local k1 = chords[1].key or key
  for i, p in ipairs(first) do firstPos[i] = M.nearestPos(k1, p) end
  for c = 2, #chords do
    local shift = chords[c].degree - chords[1].degree
    local prev = out[c - 1]
    -- The same shape, moved by the root's step, in whichever octave is
    -- nearest to where the last chord was.
    local best, bestD
    for o = -1, 1 do
      local v = {}
      local kc = chords[c].key or key
      for i, s in ipairs(firstPos) do v[i] = M.pitch(kc, s + shift + o * M.scaleLen(key)) end
      local d = 0
      for i = 1, #v do d = d + math.abs(v[i] - prev[i]) end
      if not bestD or d < bestD then best, bestD = v, d end
    end
    out[c] = best
  end
  return out
end

return M
