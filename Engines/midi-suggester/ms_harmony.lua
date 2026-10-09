--[[ Midi Suggester - chords for a melody.

     Pure Lua: no reaper., no ImGui.

     The melody is cut into slots, one per chord change, and every chord the
     key offers is scored against every slot for how well it fits the notes
     over it. A progression is then a path through the slots, and its score
     is how well each chord fits plus how well each chord leads to the next.
     The best few paths that differ enough from each other are the
     suggestions.

     How well a chord leads to the next is common-practice harmonic syntax:
     tonic, subdominant and dominant, cycling T -> S -> D -> T, a phrase
     that starts on the tonic and ends on a cadence. That is the model Open
     Music Theory teaches ("Harmonic syntax - the idealized phrase", "Harmonic
     functions"), with the pop schemata it lists - I V vi IV and the rest -
     all scoring well under it, because each of their moves is one it rewards.

     Every number that makes a musical judgement is a named constant at the
     top, so the judgement can be read in one place and tuned in one place.
]]

local M = {}

------------------------------------------------------------------------------
-- Choices offered
------------------------------------------------------------------------------

-- How often the chord changes. Bars, not beats: a harmony moving every beat
-- under a melody is a reharmonisation exercise rather than a suggestion.
M.RHYTHMS = {
  { name = "2 a bar",   bars = 0.5 },
  { name = "1 a bar",   bars = 1 },
  { name = "1 / 2 bars", bars = 2 },
  -- Auto: the melody decides. It is read in half bars, a change on the bar
  -- line is free and one on the half bar has to be earned (CHANGE_HALF), and
  -- a chord may hold for as long as the tune lets it - so chords come out
  -- half a bar, a bar, two bars long, wherever the melody puts them. Last in
  -- the list so a saved choice of the other three still means what it meant.
  { name = "Auto",      auto = true },
}

-- How far past plain triads the palette reaches. Each includes the one
-- before it.
M.COLOURS = { "Triads", "Sevenths", "Colourful" }

M.SUGGESTIONS = 6

------------------------------------------------------------------------------
-- The judgement
------------------------------------------------------------------------------

-- How much a melody note counts, by where it falls. A note on the chord
-- change is heard against the chord most of all; one tied over from before
-- the change least.
M.ACCENT_CHANGE = 2.0
M.ACCENT_BEAT   = 1.4
M.ACCENT_OFF    = 1.0
M.ACCENT_TIED   = 0.8
-- And the tune's last note most of all: it is where the cadence is heard, so
-- a chord that clashes with it is the one clash nobody will miss.
M.ACCENT_FINAL  = 2.0

-- What a melody note is worth against a chord, per unit of weight.
M.TONE_CHORD      =  1.0    -- a chord tone
M.TONE_SEVENTH    =  0.9    -- the seventh of a seventh chord: a chord tone, just
M.TONE_PASSING    = -0.25   -- a scale note that is not in the chord
M.TONE_ACCENTED   = -0.6    -- the same, struck on the change: an appoggiatura
M.TONE_AVOID      = -0.25   -- extra, a semitone over a chord tone: F over E
M.TONE_CHROMATIC  = -0.8    -- a note from outside the key and the chord

-- What reaching further costs, before the melody earns it back.
--
-- The primary triads, I IV and V, harmonise almost any diatonic tune, and
-- a harmonisation built from them is the one a musician reaches for first.
-- vi and ii are the next commonest; iii is rare; the diminished vii, as a
-- chord in its own right, rarer still. Without these costs every chord that
-- happened to hold the melody note scored alike, and Twinkle came out as
-- I iii ii V vii iii V I.
--
-- Which chords are rare depends on the mode. In a major key iii is the rare
-- one; in a minor key III, VI and VII are major chords, as common as any -
-- the relative major and the chords either side of it. The diminished triad
-- is rare in both, and is charged for being diminished rather than for the
-- degree it sits on: this table was once indexed by degree alone, written
-- for major, and in A minor it charged G major (VII) the diminished
-- chord's price while B diminished (ii) went nearly free. The minor tune
-- at a chord a bar came out Amin Bdim Emin Amin.
M.COST_DEGREE       = { [0] = 0, 0.04, 0.10, 0, 0, 0.04, 0.04 }   -- a major key
M.COST_DEGREE_MINOR = { [0] = 0, 0.04, 0.04, 0, 0, 0.04, 0.04 }   -- a minor key
M.COST_DIMINISHED   = 0.26   -- a diminished triad, on any degree
M.COST_SEVENTH    = 0.06    -- waived when the melody plays the seventh
-- A minor dominant - v in natural minor - has no leading tone, and the
-- leading tone is what makes a dominant pull home (Open Music Theory,
-- "Harmonic functions": ti is the dominant's trigger). Where the melody does
-- not choose between v and V, V wins by this much.
M.COST_MINOR_DOMINANT = 0.03
M.COST_BORROWED   = 0.20
M.COST_SECONDARY  = 0.15

-- Moves between functions (T, S, D), after Open Music Theory's T-S-D-T
-- cycle: the forward moves score, D back to S is a retrogression.
M.FUNCTION_MOVE = {
  T = { T = 0.05, S = 0.30, D = 0.20 },
  S = { T = 0.10, S = 0.10, D = 0.35 },
  D = { T = 0.40, S = -0.25, D = 0.05 },
}
-- Root motion, in semitones up from one root to the next.
M.ROOT_MOVE = { [5] = 0.20, [2] = 0.05, [9] = 0.05, [8] = 0.05, [6] = -0.20 }
M.SAME_CHORD      = 0.1   -- holding a chord across a change is allowed

-- Auto timing: what changing chord on the half bar costs. It was first
-- tried a beat at a time, with a cost for each beat by how strong it is, and
-- abandoned: every change also collects a reward for moving well, so the
-- finer the grid the more changes paid for themselves, and the minor tune
-- changed chord on nine of its sixteen beats at every cost tried short of
-- forbidding it. Half bars are where tonal harmony changes anyway.
M.CHANGE_HALF = -0.25
-- A secondary dominant resolving to its target, on top of scoring as a
-- dominant going home (see M.move). It was 0.6 while the resolution was
-- wrongly scored by the target's function; once that was fixed, 0.6 put two
-- or three in every Colourful suggestion, and 0.2 leaves one or two. At 0
-- a C before F no longer splits into C7.
M.SECONDARY_HOME  =  0.2
-- And one that does not is not offered at all. It was a cost of -0.5 at
-- first, which held in the fixed rhythms by luck and gave way in Auto,
-- where a B7 in A minor went to A minor instead of E. A secondary dominant
-- exists to lead to its chord; one that does not reads as a wrong note.
M.SECONDARY_LOST  = -1e6
M.PLAGAL_POP      =  0.20   -- bVII-I and iv-I, rock's and pop's own cadences

M.START_TONIC     =  0.40   -- begin on I
M.START_T         =  0.10   -- or on another tonic-function chord
M.START_D         = -0.20
M.END_TONIC       =  0.60   -- an authentic or plagal cadence home
M.END_HALF        =  0.25   -- a half cadence, ending on V
M.END_OTHER       = -0.20

M.W_FIT, M.W_MOVE = 1.0, 0.6

M.KEEP_PER_CHORD  = 24      -- paths kept ending on each chord, per slot
M.NOISE           = 0.08    -- how far "More ideas" shakes the fit scores

------------------------------------------------------------------------------
-- The palette
------------------------------------------------------------------------------

-- The function of each diatonic degree: I, iii and vi are tonic, ii and IV
-- subdominant, V and vii dominant (Open Music Theory, "Harmonic functions").
local DEGREE_FUNCTION = { [0] = "T", "S", "T", "S", "D", "T", "D" }

local function chordOf(root, ivs)
  local has, pcs, list = {}, {}, {}
  for _, iv in ipairs(ivs) do
    has[iv % 12] = true
    pcs[(root + iv) % 12] = true
    list[#list + 1] = iv % 12
  end
  return { root = root, has = has, pcs = pcs, ivs = list, size = #list }
end

local function sig(ch)
  local l = {}
  for pc in pairs(ch.pcs) do l[#l + 1] = pc end
  table.sort(l)
  return ch.root .. ":" .. table.concat(l, ",")
end

-- Stack every other note of the scale on degree d.
local function stacked(key, T, d, count)
  local root = T.degreePc(key, d)
  local ivs = {}
  for i = 0, count - 1 do ivs[#ivs + 1] = (T.degreePc(key, d + 2 * i) - root) % 12 end
  return chordOf(root, ivs)
end

local MAJOR, MINOR, DOM7 = { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 7, 10 }

--[[  Every chord a progression may use in this key, at this colour.

      Triads: the seven built from the scale, bar an augmented one - III+ in
      harmonic minor is a chord nobody reaches for.

      A minor-ish key also gets the major V, from the harmonic minor, which
      is the chord that makes a minor key cadence at all.

      Sevenths add the seven diatonic sevenths.

      Colourful adds what pop and rock borrow from the parallel key - iv,
      bVI, bVII and bIII in a major key; IV and the Neapolitan bII in a minor
      one - and the secondary dominants: V7 of ii, IV, V and vi. ]]
function M.palette(key, T, colour)
  colour = colour or 1
  local out, seen = {}, {}
  local function add(ch, fields)
    local s = sig(ch)
    if seen[s] then return end
    seen[s] = true
    for k, v in pairs(fields) do ch[k] = v end
    ch.numeral = ch.numeral or T.numeral(key, ch.root, ch.has)
    local pitches = {}
    for _, iv in ipairs(ch.ivs) do pitches[#pitches + 1] = 48 + ch.root + iv end
    ch.symbol = T.nameChord(pitches, key)
    -- A root from outside the key is spelled the way its numeral says: bVII
    -- in C is Bb, not the A# that C major's sharp-leaning names would give.
    if not key.pcs[ch.root] then
      local plain = T.chordNoteName(key, ch.root)
      local numeral = T.numeral(key, ch.root, ch.has)
      local names = numeral:sub(1, 1) == "#" and T.SHARP_NAMES or T.FLAT_NAMES
      ch.symbol = names[ch.root + 1] .. ch.symbol:sub(#plain + 1)
    end
    ch.id = #out + 1
    out[#out + 1] = ch
  end

  for d = 0, 6 do
    local ch = stacked(key, T, d, 3)
    if not (ch.has[4] and ch.has[8]) then
      add(ch, { degree = d, func = DEGREE_FUNCTION[d], kind = "diatonic" })
    end
  end
  local dom = key.degreePc[4]
  if not key.pcs[(dom + 4) % 12] then
    add(chordOf(dom, MAJOR), { degree = 4, func = "D", kind = "raised" })
  end

  if colour >= 2 then
    for d = 0, 6 do
      add(stacked(key, T, d, 4), { degree = d, func = DEGREE_FUNCTION[d], kind = "diatonic" })
    end
    if not key.pcs[(dom + 4) % 12] then
      add(chordOf(dom, DOM7), { degree = 4, func = "D", kind = "raised" })
    end
  end

  if colour >= 3 then
    local t = key.tonic
    if key.majorish then
      add(chordOf((t + 5) % 12, MINOR),  { func = "S", kind = "borrowed" })   -- iv
      add(chordOf((t + 8) % 12, MAJOR),  { func = "S", kind = "borrowed" })   -- bVI
      add(chordOf((t + 10) % 12, MAJOR), { func = "S", kind = "borrowed", plagal = true }) -- bVII
      add(chordOf((t + 3) % 12, MAJOR),  { func = "T", kind = "borrowed" })   -- bIII
    else
      add(chordOf((t + 5) % 12, MAJOR),  { func = "S", kind = "borrowed" })   -- IV
      add(chordOf((t + 1) % 12, MAJOR),  { func = "S", kind = "borrowed" })   -- bII
    end
    for _, d in ipairs({ 1, 3, 4, 5 }) do
      local target = stacked(key, T, d, 3)
      -- Only a chord that can be a key can be tonicised: not a diminished one.
      if not target.has[6] then
        local ch = chordOf((target.root + 7) % 12, DOM7)
        add(ch, { func = "D", kind = "secondary", target = target.root,
                  numeral = "V7/" .. T.numeral(key, target.root, target.has) })
      end
    end
  end

  -- iv and bVII lead home in the plagal way pop does.
  for _, ch in ipairs(out) do
    if ch.kind == "borrowed" and ch.func == "S" and ch.root == (key.tonic + 5) % 12 then
      ch.plagal = true
    end
  end
  return out
end

------------------------------------------------------------------------------
-- Fitting a chord to a slot
------------------------------------------------------------------------------

--[[  The melody notes over the stretch a to b, weighted: { start, len,
      notes = { {pc, w, dur, changed} } }. A note that starts on a is heard
      on the change; one tied over from before it least. ]]
function M.span(line, a, b, pulse)
  pulse = pulse or 1
  local final = line[#line]
  local s = { start = a, len = b - a, notes = {} }
  for _, note in ipairs(line) do
    local lo, hi = math.max(a, note.start), math.min(b, note.start + note.len)
    if hi - lo > 1e-9 then
      local accent
      if note.start < a - 1e-9 then accent = M.ACCENT_TIED
      elseif math.abs(note.start - a) < 1e-6 then accent = M.ACCENT_CHANGE
      elseif math.abs((note.start / pulse) - math.floor(note.start / pulse + 0.5)) < 1e-6 then
        accent = M.ACCENT_BEAT
      else accent = M.ACCENT_OFF end
      local w = (hi - lo) * accent
      if note == final then w = w * M.ACCENT_FINAL end
      s.notes[#s.notes + 1] = { pc = note.pitch % 12, w = w,
                                dur = hi - lo, changed = accent == M.ACCENT_CHANGE }
    end
  end
  return s
end

-- The melody cut into equal slots, one per chord change.
function M.slots(line, beats, slotLen, pulse)
  local n = math.max(1, math.ceil(beats / slotLen - 1e-6))
  local out = {}
  for i = 0, n - 1 do
    local a = i * slotLen
    out[#out + 1] = M.span(line, a, math.min(beats, a + slotLen), pulse)
  end
  return out
end

-- How well a chord fits the notes over a slot, from -1 to 1. An empty slot
-- fits everything equally, and leaves the choice to the moves either side.
function M.fit(ch, slot, key)
  local total, sum, playsSeventh = 0, 0, false
  for _, n in ipairs(slot.notes) do
    local v
    if ch.pcs[n.pc] then
      local iv = (n.pc - ch.root) % 12
      if iv == 10 or iv == 11 or (iv == 9 and ch.size >= 4) then
        v, playsSeventh = M.TONE_SEVENTH, true
      else
        v = M.TONE_CHORD
      end
    else
      if key.pcs[n.pc] then
        v = n.changed and M.TONE_ACCENTED or M.TONE_PASSING
      else
        v = M.TONE_CHROMATIC
      end
      if ch.pcs[(n.pc - 1) % 12] then v = v + M.TONE_AVOID end
    end
    sum, total = sum + v * n.w, total + n.w
  end
  local f = total > 0 and sum / total or 0

  if ch.kind == "diatonic" then
    f = f - (key.majorish and M.COST_DEGREE or M.COST_DEGREE_MINOR)[ch.degree]
  end
  if ch.size == 3 and ch.has[3] and ch.has[6] then f = f - M.COST_DIMINISHED end
  if ch.func == "D" and ch.degree == 4 and ch.has[3] then f = f - M.COST_MINOR_DOMINANT end
  if ch.size >= 4 and not playsSeventh then f = f - M.COST_SEVENTH end
  if ch.kind == "borrowed" then f = f - M.COST_BORROWED end
  if ch.kind == "secondary" then f = f - M.COST_SECONDARY end
  return f
end

-- The share of the melody's sounding time spent on chord tones. This is what
-- the window shows as "fits", because it is the one number a musician can
-- check by ear: how often the tune is sitting on the chord.
function M.match(chords, slots)
  local on, all = 0, 0
  for i, s in ipairs(slots) do
    for _, n in ipairs(s.notes) do
      all = all + n.dur
      if chords[i].pcs[n.pc] then on = on + n.dur end
    end
  end
  return all > 0 and on / all or 1
end

------------------------------------------------------------------------------
-- Moving from chord to chord
------------------------------------------------------------------------------

function M.move(a, b, key)
  if a.id == b.id then return M.SAME_CHORD end
  local v = M.FUNCTION_MOVE[a.func][b.func] or 0
  if a.kind == "secondary" and b.root == a.target then
    -- A secondary dominant resolving is a dominant going to its tonic, for
    -- the moment. Scored by b's function, V7/IV into IV read as D into S -
    -- a retrogression - and cost the C7 in C C7 | F its place to Cmaj7.
    v = M.FUNCTION_MOVE.D.T
  end
  v = v + (M.ROOT_MOVE[(b.root - a.root) % 12] or 0)
  if a.kind == "secondary" then
    v = v + ((b.root == a.target) and M.SECONDARY_HOME or M.SECONDARY_LOST)
  end
  if a.plagal and b.root == key.tonic and b.func == "T" then v = v + M.PLAGAL_POP end
  return v
end

local function startScore(ch, key)
  if ch.func == "T" and ch.root == key.tonic then return M.START_TONIC end
  if ch.func == "T" then return M.START_T end
  if ch.func == "D" then return M.START_D end
  return 0
end

local function endScore(ch, key)
  if ch.kind == "secondary" then return M.SECONDARY_LOST end
  if ch.root == key.tonic and ch.func == "T" then return M.END_TONIC end
  if ch.func == "D" and ch.root == key.degreePc[4] and ch.kind ~= "secondary" then
    return M.END_HALF
  end
  return M.END_OTHER
end

------------------------------------------------------------------------------
-- The search
------------------------------------------------------------------------------

-- A small, fixed random number generator, so "More ideas" gives the same
-- ideas in the same order every time and the tests can hold it to that.
local function rng(seed)
  local s = (seed or 1) % 2147483647
  if s <= 0 then s = s + 2147483646 end
  return function()
    s = (s * 48271) % 2147483647
    return s / 2147483647
  end
end
M.rng = rng

local function differs(a, b)
  local d = 0
  for i = 1, #a do if a[i].id ~= b[i].id then d = d + 1 end end
  return d
end

--[[  The suggestions for a melody.

      opts: { key, beats, barBeats, pulse, rhythm (index into RHYTHMS),
              colour (index into COLOURS), variation (0 for the best, then
              1, 2, ... for more ideas) }

      Returns a list of suggestions, best first, each
        { chords = { {start, len, chord} ... }  -- repeats merged
          numerals, symbols (strings), score, match (0..1) } ]]
function M.suggest(line, T, opts)
  local key = opts.key
  local rhythm = M.RHYTHMS[opts.rhythm or 2]
  local pulse = opts.pulse or 1
  -- Auto reads half bars where a bar divides into two equal halves of beats
  -- (4/4, 6/8, 2/2), and whole bars where it does not (3/4, 5/8).
  local pulses = math.floor(opts.barBeats / pulse + 0.5)
  local halves = rhythm.auto and pulses % 2 == 0
  local slotLen = rhythm.auto and (halves and opts.barBeats / 2 or opts.barBeats)
                  or opts.barBeats * rhythm.bars
  local slots = M.slots(line, opts.beats, slotLen, pulse)
  local change = {}
  for i, s in ipairs(slots) do
    local onBar = math.abs(s.start / opts.barBeats - math.floor(s.start / opts.barBeats + 0.5)) < 1e-6
    change[i] = (rhythm.auto and not onBar) and M.CHANGE_HALF or 0
  end
  local pal = M.palette(key, T, opts.colour or 1)
  local random = (opts.variation or 0) > 0 and rng(opts.variation * 7919) or nil

  local fits = {}
  for i, s in ipairs(slots) do
    fits[i] = {}
    for _, ch in ipairs(pal) do
      local f = M.fit(ch, s, key)
      if random then f = f + (random() * 2 - 1) * M.NOISE end
      fits[i][ch.id] = f * M.W_FIT
    end
  end

  -- Paths, kept per the chord they end on so that a strong chord cannot
  -- crowd every other ending out of the search. Each path is its last chord
  -- and a link to the path it grew from, so growing one copies nothing.
  local paths = {}
  for _, ch in ipairs(pal) do
    paths[ch.id] = { { score = fits[1][ch.id] + startScore(ch, key), ch = ch, n = 1 } }
  end

  local moves = {}
  for _, a in ipairs(pal) do
    moves[a.id] = {}
    for _, b in ipairs(pal) do
      moves[a.id][b.id] = M.move(a, b, key) * M.W_MOVE
    end
  end
  local function better(x, y) return x.score > y.score end

  for i = 2, #slots do
    local nextPaths = {}
    for _, b in ipairs(pal) do
      -- Each chord's paths are already best first, so the best ways into b
      -- are a merge of those lists, not a sort of all of them together.
      local cands, fb, at, cc = {}, fits[i][b.id], {}, change[i]
      for _, a in ipairs(pal) do at[a.id] = 1 end
      for k = 1, M.KEEP_PER_CHORD do
        local bestA, bestScore
        for _, a in ipairs(pal) do
          local p = paths[a.id][at[a.id]]
          if p then
            local sc = p.score + moves[a.id][b.id] + (a.id ~= b.id and cc or 0)
            if not bestScore or sc > bestScore then bestA, bestScore = a, sc end
          end
        end
        if not bestA then break end
        local p = paths[bestA.id][at[bestA.id]]
        at[bestA.id] = at[bestA.id] + 1
        cands[k] = { score = bestScore + fb, ch = b, prev = p, n = i }
      end
      nextPaths[b.id] = cands
    end
    paths = nextPaths
  end

  local all = {}
  for _, ch in ipairs(pal) do
    for _, p in ipairs(paths[ch.id]) do
      local chords, q = {}, p
      while q do chords[q.n] = q.ch; q = q.prev end
      all[#all + 1] = { score = p.score + endScore(ch, key), chords = chords }
    end
  end
  table.sort(all, better)

  -- Distinct enough to be worth offering: a suggestion has to change at
  -- least a quarter of the chords of every one already chosen.
  local need = math.max(1, math.floor(#slots / 4))
  local chosen = {}
  for _, p in ipairs(all) do
    local fresh = true
    for _, q in ipairs(chosen) do
      if differs(p.chords, q.chords) < need then fresh = false; break end
    end
    if fresh then
      chosen[#chosen + 1] = p
      if #chosen >= M.SUGGESTIONS then break end
    end
  end

  local out = {}
  for _, p in ipairs(chosen) do
    local sug = { score = p.score, chords = {}, palette = pal, key = key, pulse = pulse }
    for i, ch in ipairs(p.chords) do
      local last = sug.chords[#sug.chords]
      if last and last.chord.id == ch.id then
        last.len = last.len + slots[i].len
      else
        sug.chords[#sug.chords + 1] = { start = slots[i].start, len = slots[i].len, chord = ch }
      end
    end
    M.describe(sug, line)
    out[#out + 1] = sug
  end
  return out
end

------------------------------------------------------------------------------
-- Editing one progression, chord by chord
--
-- Once a progression is chosen, any one chord can be swapped for another,
-- split in two with a passing chord in its second half, or removed and its
-- time given to a neighbour. Everything else in the progression stays put.
-- The alternatives are judged the way the search judged the chord: how it
-- fits the melody over it, and how it moves from the chord before and into
-- the chord after.
------------------------------------------------------------------------------

-- Numerals, symbols and the "fits" share, from the chords as they now stand.
function M.describe(sug, line)
  local numerals, symbols, on, all = {}, {}, 0, 0
  for _, c in ipairs(sug.chords) do
    numerals[#numerals + 1] = c.chord.numeral
    symbols[#symbols + 1] = c.chord.symbol
    for _, n in ipairs(M.span(line, c.start, c.start + c.len, sug.pulse).notes) do
      all = all + n.dur
      if c.chord.pcs[n.pc] then on = on + n.dur end
    end
  end
  sug.numerals, sug.symbols = numerals, symbols
  sug.match = all > 0 and on / all or 1
  return sug
end

--[[  Other chords for chord i, best first, each { chord, score, match }.
      A chord that could not stand there is not offered: a secondary dominant
      not followed by its target, or anything after one that is not its
      target. The chord already there is left out. ]]
M.ALTERNATIVES = 8
function M.alternatives(sug, i, line)
  local c = sug.chords[i]
  local key = sug.key
  local span = M.span(line, c.start, c.start + c.len, sug.pulse)
  local prev = sug.chords[i - 1] and sug.chords[i - 1].chord
  local nxt = sug.chords[i + 1] and sug.chords[i + 1].chord
  local out = {}
  for _, ch in ipairs(sug.palette) do
    if ch.id ~= c.chord.id then
      local score = M.fit(ch, span, key) * M.W_FIT
      score = score + (prev and M.move(prev, ch, key) * M.W_MOVE or startScore(ch, key))
      score = score + (nxt and M.move(ch, nxt, key) * M.W_MOVE or endScore(ch, key))
      if score > M.SECONDARY_LOST / 2 then
        local on, all = 0, 0
        for _, n in ipairs(span.notes) do
          all = all + n.dur
          if ch.pcs[n.pc] then on = on + n.dur end
        end
        out[#out + 1] = { chord = ch, score = score, match = all > 0 and on / all or 1 }
      end
    end
  end
  table.sort(out, function(a, b)
    if a.score ~= b.score then return a.score > b.score end
    return a.chord.id < b.chord.id
  end)
  for k = #out, M.ALTERNATIVES + 1, -1 do out[k] = nil end
  return out
end

function M.replace(sug, i, ch, line)
  sug.chords[i].chord = ch
  return M.describe(sug, line)
end

-- The shortest chord a split may leave: a beat.
function M.canSplit(sug, i)
  return sug.chords[i].len >= 2 * sug.pulse - 1e-9
end

--[[  Splits chord i in two, on the beat nearest its middle, and puts the best
      alternative in the second half - a passing chord, leading into the
      chord after. Returns the new chord's index, or nil if it is too short
      to split. ]]
function M.split(sug, i, line)
  if not M.canSplit(sug, i) then return nil end
  local c = sug.chords[i]
  local p = sug.pulse
  local half = math.floor(c.len / 2 / p + 0.5) * p
  half = math.max(p, math.min(c.len - p, half))
  table.insert(sug.chords, i + 1, { start = c.start + half, len = c.len - half, chord = c.chord })
  c.len = half
  local best = M.alternatives(sug, i + 1, line)[1]
  if best then sug.chords[i + 1].chord = best.chord end
  M.describe(sug, line)
  return i + 1
end

function M.canRemove(sug) return #sug.chords > 1 end

--[[  Removes chord i. The chord before it takes its time; the first chord
      gives its time to the one after. If that leaves the same chord twice in
      a row - removing F from C F C - the two become one held chord, because
      removing a chord is asking for fewer of them. Returns the index of the
      chord that grew, or nil if it is the only chord. ]]
function M.remove(sug, i, line)
  if not M.canRemove(sug) then return nil end
  local c = sug.chords[i]
  local grew
  if i > 1 then
    sug.chords[i - 1].len = sug.chords[i - 1].len + c.len
    grew = i - 1
  else
    sug.chords[2].start = c.start
    sug.chords[2].len = sug.chords[2].len + c.len
    grew = 1
  end
  table.remove(sug.chords, i)
  local after = sug.chords[grew + 1]
  if after and after.chord.id == sug.chords[grew].chord.id then
    sug.chords[grew].len = sug.chords[grew].len + after.len
    table.remove(sug.chords, grew + 1)
  end
  M.describe(sug, line)
  return grew
end

-- A copy deep enough to edit without touching the original, so a progression
-- can be put back the way it was suggested.
function M.copy(sug)
  local c = {}
  for k, v in pairs(sug) do c[k] = v end
  c.chords = {}
  for i, ch in ipairs(sug.chords) do c.chords[i] = { start = ch.start, len = ch.len, chord = ch.chord } end
  c.numerals, c.symbols = {}, {}
  for i, v in ipairs(sug.numerals) do c.numerals[i] = v end
  for i, v in ipairs(sug.symbols) do c.symbols[i] = v end
  return c
end

------------------------------------------------------------------------------
-- Voicing
------------------------------------------------------------------------------

M.VOICE_FLOOR   = 48   -- C3: the lowest an upper voice goes
M.VOICE_CEILING = 76   -- E5
M.BASS_LOW      = 36   -- C2 to B2 for the bass
M.VELOCITY      = 100

-- Every close-position voicing of the chord, each a sorted list of pitches,
-- with its top at or under `hi` and its bottom at or over the floor.
local function closeVoicings(ch, hi)
  local pcs = {}
  for _, iv in ipairs(ch.ivs) do pcs[#pcs + 1] = (ch.root + iv) % 12 end
  table.sort(pcs, function(a, b) return (a - ch.root) % 12 < (b - ch.root) % 12 end)
  local out = {}
  for inv = 0, #pcs - 1 do
    for base = M.VOICE_FLOOR, M.VOICE_FLOOR + 11 do
      if base % 12 == pcs[inv + 1] then
        for oct = 0, 2 do
          local v, p = {}, base + oct * 12
          for k = 0, #pcs - 1 do
            local pc = pcs[(inv + k) % #pcs + 1]
            while p % 12 ~= pc do p = p + 1 end
            v[#v + 1] = p
            p = p + 1
          end
          if v[#v] <= hi then out[#out + 1] = v end
        end
      end
    end
  end
  return out
end

local function distance(a, b)
  if #a == #b then
    local d = 0
    for i = 1, #a do d = d + math.abs(a[i] - b[i]) end
    return d
  end
  local ca, cb = 0, 0
  for _, p in ipairs(a) do ca = ca + p end
  for _, p in ipairs(b) do cb = cb + p end
  return math.abs(ca / #a - cb / #b) * math.max(#a, #b)
end

--[[  The notes of a suggestion: each chord in close position, under the
      melody where there is room, moving as little as it can from the chord
      before - which is what makes block chords sound like a part rather than
      a column of shapes. And the root in the bass, if asked for. ]]
function M.voice(sug, line, withBass)
  local notes, prev = {}, nil
  for _, c in ipairs(sug.chords) do
    local lowest
    for _, n in ipairs(line) do
      if n.start < c.start + c.len and n.start + n.len > c.start then
        lowest = math.min(lowest or 999, n.pitch)
      end
    end
    local hi = math.min(M.VOICE_CEILING, (lowest or 999) - 1)
    local options = closeVoicings(c.chord, hi)
    if #options == 0 then options = closeVoicings(c.chord, M.VOICE_CEILING) end
    local best, bestD
    for _, v in ipairs(options) do
      -- The first chord sits as high under the tune as it can, near middle C.
      local d = prev and distance(v, prev) or math.abs(v[#v] - math.min(hi, 67))
      if not best or d < bestD then best, bestD = v, d end
    end
    for _, p in ipairs(best) do
      notes[#notes + 1] = { pitch = p, start = c.start, len = c.len, vel = M.VELOCITY }
    end
    if withBass then
      local b = M.BASS_LOW + (c.chord.root - M.BASS_LOW) % 12
      notes[#notes + 1] = { pitch = b, start = c.start, len = c.len, vel = M.VELOCITY }
    end
    prev = best
  end
  return notes
end

return M
