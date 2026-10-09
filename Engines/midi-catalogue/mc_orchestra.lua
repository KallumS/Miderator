--[[ Midi Catalogue - the orchestra.

     Pure data plus a few questions asked of it. No REAPER, no ImGui.

     What makes a line idiomatic for an instrument, as far as the catalogue
     needs to know:

       low, high    the practical orchestral range, as sounding MIDI pitches
       sweet        the register where it sounds like itself; the catalogue
                    writes inside this and only leaves it for a reason
       role         which voice of four-part harmony it naturally takes:
                    S, A, T or B (Violins I sing soprano, Violins II
                    mezzo-soprano, and so on down)
       poly         how many notes it can sound at once. A string SECTION
                    plays one line - divisi is an orchestration decision, and
                    that is what the ensembles below are for
       fast         the shortest time between two notes it plays cleanly, in
                    seconds, so a 1/16 run that is fine at 90 bpm is hidden
                    at 160 bpm
       leap         the widest melodic leap, in semitones, it takes happily in
                    passing
       breath       wind and brass: phrases leave room to breathe
       pitches      "td" for an instrument tuned to a few notes before it plays
                    - the timpani get the tonic and the dominant
       avoid        categories or types it does not do, each with the reason
                    the window shows

     Ranges are the ones standard orchestration texts give (Rimsky-Korsakov,
     Adler, Piston). Where Orchestration Helper states one it is used as it
     stands: the timpani's E2 to G#3 and the harp's C-flat 1 to F-sharp 7. The
     character notes behind `sweet` are Orchestration Helper's: the flute
     dull low and brilliant high, the oboe wild and rough at the bottom, the
     E string thinning as it climbs.
]]

local M = {}

M.FAMILIES = { "Strings", "Woodwind", "Brass", "Percussion", "Keys & Harp" }

local TWO_MALLETS = {
  Quartal = "A quartal chord here is three or four notes at once; with two mallets this instrument plays two",
  Cluster = "A cluster here is three or four notes at once; with two mallets this instrument plays two",
  Pedal   = "A pedal under moving chords needs three notes at once; with two mallets this instrument plays two",
}

M.INSTRUMENTS = {
  -- Strings
  { id = "vln1", name = "Violin I",  family = "Strings", low = 55, high = 100,
    sweet = { 67, 91 }, role = "S", poly = 1, fast = 0.09, leap = 12 },
  { id = "vln2", name = "Violin II", family = "Strings", low = 55, high = 96,
    sweet = { 60, 84 }, role = "A", poly = 1, fast = 0.09, leap = 12 },
  { id = "vla",  name = "Viola",     family = "Strings", low = 48, high = 88,
    sweet = { 52, 76 }, role = "T", poly = 1, fast = 0.1, leap = 12 },
  { id = "vc",   name = "Cello",     family = "Strings", low = 36, high = 81,
    sweet = { 40, 67 }, role = "B", poly = 1, fast = 0.1, leap = 12 },
  { id = "cb",   name = "Double Bass", family = "Strings", low = 28, high = 60,
    sweet = { 28, 50 }, role = "B", poly = 1, fast = 0.18, leap = 9 },

  -- Woodwind
  { id = "picc", name = "Piccolo",   family = "Woodwind", low = 74, high = 108,
    sweet = { 79, 100 }, role = "S", poly = 1, fast = 0.09, leap = 12, breath = true },
  { id = "fl",   name = "Flute",     family = "Woodwind", low = 60, high = 96,
    sweet = { 67, 91 }, role = "S", poly = 1, fast = 0.09, leap = 12, breath = true },
  { id = "ob",   name = "Oboe",      family = "Woodwind", low = 58, high = 91,
    sweet = { 62, 84 }, role = "S", poly = 1, fast = 0.11, leap = 10, breath = true },
  { id = "eh",   name = "English Horn", family = "Woodwind", low = 52, high = 81,
    sweet = { 55, 74 }, role = "A", poly = 1, fast = 0.12, leap = 10, breath = true },
  { id = "cl",   name = "Clarinet",  family = "Woodwind", low = 50, high = 91,
    sweet = { 55, 84 }, role = "A", poly = 1, fast = 0.09, leap = 12, breath = true },
  { id = "bcl",  name = "Bass Clarinet", family = "Woodwind", low = 38, high = 74,
    sweet = { 40, 65 }, role = "B", poly = 1, fast = 0.12, leap = 12, breath = true },
  { id = "bsn",  name = "Bassoon",   family = "Woodwind", low = 34, high = 76,
    sweet = { 38, 67 }, role = "B", poly = 1, fast = 0.11, leap = 12, breath = true },
  { id = "cbsn", name = "Contrabassoon", family = "Woodwind", low = 22, high = 53,
    sweet = { 26, 48 }, role = "B", poly = 1, fast = 0.25, leap = 9, breath = true },

  -- Brass
  { id = "hn",   name = "Horn",      family = "Brass", low = 35, high = 77,
    sweet = { 48, 72 }, role = "A", poly = 1, fast = 0.14, leap = 9, breath = true },
  { id = "tpt",  name = "Trumpet",   family = "Brass", low = 52, high = 82,
    sweet = { 60, 79 }, role = "S", poly = 1, fast = 0.1, leap = 9, breath = true },
  { id = "tbn",  name = "Trombone",  family = "Brass", low = 40, high = 72,
    sweet = { 45, 67 }, role = "T", poly = 1, fast = 0.17, leap = 7, breath = true },
  { id = "btbn", name = "Bass Trombone", family = "Brass", low = 34, high = 65,
    sweet = { 36, 60 }, role = "B", poly = 1, fast = 0.2, leap = 7, breath = true },
  { id = "tuba", name = "Tuba",      family = "Brass", low = 28, high = 58,
    sweet = { 31, 53 }, role = "B", poly = 1, fast = 0.2, leap = 7, breath = true },

  -- Percussion. Only the pitched ones: a drum kit is Starting Blocks' job.
  { id = "timp", name = "Timpani",   family = "Percussion", low = 40, high = 56,
    sweet = { 40, 56 }, role = "B", poly = 2, fast = 0.09, leap = 12, pitches = "td",
    avoid = {
      Melody  = "Timpani are tuned before they play, to a few notes, so a melody is not theirs to play",
      Harmony = "Timpani are tuned to the tonic and the dominant; they hold the bass of the harmony, not its voices",
      Arpeggio = "An arpeggio needs more notes than the timpani are tuned to",
      Alberti  = "An Alberti figure needs three notes; the timpani have two",
      ["Broken chord"] = "A broken chord needs more notes than the timpani are tuned to",
    } },
  { id = "glock", name = "Glockenspiel", family = "Percussion", low = 79, high = 108,
    sweet = { 79, 103 }, role = "S", poly = 2, fast = 0.13, leap = 12,
    avoid = TWO_MALLETS },
  { id = "xyl",  name = "Xylophone", family = "Percussion", low = 65, high = 108,
    sweet = { 67, 100 }, role = "S", poly = 2, fast = 0.09, leap = 12,
    avoid = TWO_MALLETS },
  { id = "mar",  name = "Marimba",   family = "Percussion", low = 45, high = 96,
    sweet = { 48, 88 }, role = "A", poly = 4, fast = 0.09, leap = 12 },

  -- Keys and harp
  { id = "harp", name = "Harp",      family = "Keys & Harp", low = 23, high = 102,
    sweet = { 36, 84 }, role = "A", poly = 4, fast = 0.11, leap = 12 },
  { id = "cel",  name = "Celesta",   family = "Keys & Harp", low = 60, high = 108,
    sweet = { 67, 96 }, role = "S", poly = 4, fast = 0.09, leap = 12 },
  { id = "pno",  name = "Piano",     family = "Keys & Harp", low = 21, high = 108,
    sweet = { 43, 84 }, role = "A", poly = 6, fast = 0.07, leap = 12 },
}

-- Sections, for spreading harmony across real parts. Each part takes one
-- voice (1 is the bass), and `octave` moves it after voicing - the double
-- bass doubles the cellos an octave down, as Rimsky-Korsakov has it.
--
-- The four horns follow the convention Orchestration Helper records: horns 1
-- and 3 take the high parts, 2 and 4 the low, 1 the highest and 4 the lowest.
M.ENSEMBLES = {
  { id = "strings", name = "Strings", parts = {
      { inst = "vln1", voice = 4, name = "Violin I" },
      { inst = "vln2", voice = 3, name = "Violin II" },
      { inst = "vla",  voice = 2, name = "Viola" },
      { inst = "vc",   voice = 1, name = "Cello" },
      { inst = "cb",   voice = 1, name = "Double Bass", octave = -1 },
  } },
  { id = "winds", name = "Woodwinds", parts = {
      { inst = "fl",  voice = 4, name = "Flute" },
      { inst = "ob",  voice = 3, name = "Oboe" },
      { inst = "cl",  voice = 2, name = "Clarinet" },
      { inst = "bsn", voice = 1, name = "Bassoon" },
  } },
  { id = "brass", name = "Brass", parts = {
      { inst = "tpt",  voice = 4, name = "Trumpet" },
      { inst = "hn",   voice = 3, name = "Horn" },
      { inst = "tbn",  voice = 2, name = "Trombone" },
      { inst = "tuba", voice = 1, name = "Tuba" },
  } },
  -- A part may carry its own `sweet`, where one instrument plays several
  -- parts at different heights: the high horns are uncomfortable on the
  -- bottom notes and the low horns on the top ones (Orchestration Helper,
  -- after Berlioz), so each pair gets its own half of the compass.
  { id = "horns", name = "Four Horns", parts = {
      { inst = "hn", voice = 4, name = "Horn 1", sweet = { 60, 77 } },
      { inst = "hn", voice = 3, name = "Horn 3", sweet = { 55, 72 } },
      { inst = "hn", voice = 2, name = "Horn 2", sweet = { 46, 67 } },
      { inst = "hn", voice = 1, name = "Horn 4", sweet = { 38, 62 } },
  } },
}

function M.byId(id)
  for _, inst in ipairs(M.INSTRUMENTS) do if inst.id == id then return inst end end
  return nil
end

function M.ensembleById(id)
  for _, e in ipairs(M.ENSEMBLES) do if e.id == id then return e end end
  return nil
end

M.REGISTERS = { "Low", "Middle", "High" }

-- Where in the sweet register the material sits. Each band is two thirds of
-- it, overlapping, so Middle is not a thin slice and Low still sounds like
-- the instrument rather than like its bottom note.
function M.band(inst, register)
  local lo, hi = inst.sweet[1], inst.sweet[2]
  local span = hi - lo
  local a, b
  if register == "Low" then a, b = lo, lo + span * 2 / 3
  elseif register == "High" then a, b = lo + span / 3, hi
  else a, b = lo + span / 6, hi - span / 6 end
  a, b = math.floor(a + 0.5), math.floor(b + 0.5)
  return a, b, (a + b) / 2
end

-- The four voice ranges of plain four-part writing, bass first, then moved as
-- a block so the voice this instrument takes lands where the instrument is.
-- A single melodic instrument asked for harmony plays its own voice of a
-- chorale built this way; the other three are what the rest of the band
-- would play.
M.SATB = { { 40, 60 }, { 48, 67 }, { 55, 74 }, { 60, 79 } }
M.ROLE_VOICE = { B = 1, T = 2, A = 3, S = 4 }

function M.satbFor(inst, register)
  local _, _, centre = M.band(inst, register)
  local v = M.SATB[M.ROLE_VOICE[inst.role] or 3]
  local shift = math.floor(centre - (v[1] + v[2]) / 2 + 0.5)
  local out = {}
  for i, r in ipairs(M.SATB) do out[i] = { r[1] + shift, r[2] + shift } end
  return out, M.ROLE_VOICE[inst.role] or 3
end

-- Voice ranges for an instrument that plays the chords itself, `nv` voices of
-- them. The bass has the bottom of the band to itself and the upper voices
-- share the rest, overlapping it a little so close position is possible.
function M.chordRanges(inst, register, nv)
  local lo, hi = M.band(inst, register)
  if hi - lo < 14 then
    -- A narrow instrument stacks its voices in what it has.
    local out = {}
    for v = 1, nv do out[v] = { inst.low, inst.high } end
    return out
  end
  local out = { { lo, lo + 12 } }
  for v = 2, nv do out[v] = { lo + 3, hi } end
  return out
end

-- Where a part of a section sits: its own `sweet` if it has one, else its
-- instrument's.
function M.partBand(part, register)
  local inst = M.byId(part.inst)
  if part.sweet then return M.band({ sweet = part.sweet }, register) end
  return M.band(inst, register)
end

-- Why this instrument does not do something, or nil if it does.
function M.avoids(inst, category, type_)
  local a = inst.avoid
  if not a then return nil end
  return a[type_] or a[category]
end

-- The shortest time between notes it manages, as beats at this tempo.
function M.fastBeats(inst, bpm)
  return inst.fast * (bpm or 120) / 60
end

return M
