--[[ Midi Suggester - melodies for a chord progression.

     Pure Lua: no reaper., no ImGui.

     A melody here is built the way a songwriter builds one, in two passes.

     Rhythm first. One bar of rhythm - the motif - is drawn from cells of a
     beat or two, and the phrase is made by repeating it: A, A again with
     its end changed, A, then a cadence bar that states half the motif and
     holds a long note. Repetition is most of what makes a tune sound like a
     tune rather than a sequence of notes.

     Then pitch, one note at a time, with the rules every melody-writing text
     gives (Open Music Theory, "Embellishing tones", for the passing and
     neighbour notes):

       - a note on a strong beat is a chord tone;
       - a note off the beat may be a scale note, but only if it is reached
         by step - a passing or neighbour note - and not left hanging;
       - most moves are steps, a leap is answered by a step back the other
         way, and the line arches up and comes home;
       - the repeated bars repeat the motif's shape, in scale steps, wherever
         the chord lets them;
       - the last note is the tonic.

     Many candidates are drawn this way, each is scored against the same
     rules as a whole, and the best few that differ from each other are the
     suggestions.
]]

local M = {}

------------------------------------------------------------------------------
-- Choices offered
------------------------------------------------------------------------------

--[[  Rhythm cells: a length in beats and the notes in it, each an offset and
      a length. A cell with no notes is a rest. `w` is how often it is drawn. ]]
local Q, E, S = 1, 0.5, 0.25
M.DENSITIES = {
  { name = "Sparse", cells = {
      { len = 2, notes = { { 0, 2 } },                 w = 4 },
      { len = 1, notes = { { 0, 1 } },                 w = 4 },
      { len = 4, notes = { { 0, 4 } },                 w = 1 },
      { len = 2, notes = { { 0, 1.5 }, { 1.5, 0.5 } }, w = 2 },
      { len = 1, notes = {},                           w = 1 },
  } },
  { name = "Medium", cells = {
      { len = 1, notes = { { 0, Q } },                 w = 4 },
      { len = 1, notes = { { 0, E }, { E, E } },       w = 4 },
      { len = 1, notes = { { 0, 0.75 }, { 0.75, S } }, w = 1 },
      { len = 1, notes = { { E, E } },                 w = 1 },
      { len = 2, notes = { { 0, 1.5 }, { 1.5, E } },   w = 2 },
      { len = 2, notes = { { 0, 2 } },                 w = 2 },
  } },
  { name = "Busy", cells = {
      { len = 1, notes = { { 0, S }, { S, S }, { E, S }, { 0.75, S } }, w = 2 },
      { len = 1, notes = { { 0, E }, { E, S }, { 0.75, S } },           w = 2 },
      { len = 1, notes = { { 0, S }, { S, S }, { E, E } },              w = 2 },
      { len = 1, notes = { { 0, E }, { E, E } },                        w = 3 },
      { len = 1, notes = { { 0, Q } },                                  w = 2 },
      { len = 1, notes = { { E, E } },                                  w = 1 },
  } },
}

-- Where the tune sits: the note it arches up from and comes home to.
M.REGISTERS = {
  { name = "Low",  centre = 60 },
  { name = "Mid",  centre = 67 },
  { name = "High", centre = 74 },
}
M.RANGE_BELOW, M.RANGE_ABOVE = 7, 12

M.SUGGESTIONS = 4
M.CANDIDATES  = 48
M.VELOCITY    = 100

------------------------------------------------------------------------------
-- The judgement, note by note
------------------------------------------------------------------------------

M.STRONG_CHORD    =  3.0   -- a chord tone on a strong beat
M.STRONG_OTHER    = -4.0   -- anything else there
M.WEAK_CHORD      =  1.0   -- a chord tone off the beat
M.WEAK_STEPPED    =  0.3   -- a scale note off the beat, reached by step
M.WEAK_LEAPT      = -3.0   -- a scale note off the beat, reached by leap
M.MOVE = { [0] = -0.5, 2, 2, 1, 1, 0, 0, 0, -2, -2, -2, -2, -2 }  -- by semitones
M.MOVE_FAR        = -6.0   -- past an octave
M.RECOVER         =  1.5   -- a step back the other way after a leap
M.NOT_RECOVERED   = -1.5   -- carrying on the same way after one
M.CONTOUR         = -0.25  -- per semitone from where the arch wants to be
M.ECHO            =  2.0   -- following the motif's shape in a repeated bar
M.TEMPERATURE     =  0.6   -- how freely the next note is drawn

------------------------------------------------------------------------------
-- The judgement, whole melodies
------------------------------------------------------------------------------

M.SCORE_STRONG    = 3.0    -- per share of strong notes on chord tones
M.SCORE_STEPS     = 2.0    -- lost per unit the share of steps is off 0.65
M.STEP_SHARE      = 0.65
M.SCORE_SPAN      = 0.2    -- per semitone of span past a tenth
M.SCORE_STUCK     = 0.5    -- per note repeating two before it
M.SCORE_LEAP      = 0.3    -- per leap past a fifth

------------------------------------------------------------------------------
-- Randomness that can be repeated
------------------------------------------------------------------------------

local function rng(seed)
  local s = (seed or 1) % 2147483647
  if s <= 0 then s = s + 2147483646 end
  return function()
    s = (s * 48271) % 2147483647
    return s / 2147483647
  end
end

local function weighted(random, items, weightOf)
  local total = 0
  for _, it in ipairs(items) do total = total + weightOf(it) end
  local r = random() * total
  for _, it in ipairs(items) do
    r = r - weightOf(it)
    if r <= 0 then return it end
  end
  return items[#items]
end

------------------------------------------------------------------------------
-- Rhythm
------------------------------------------------------------------------------

-- Short fillers, so any bar length can be filled - a 5/8 bar is 2.5 beats.
local FILL = { { len = E, notes = { { 0, E } }, w = 1 }, { len = S, notes = { { 0, S } }, w = 1 } }

-- One bar of cells, as a list of { start, len } offsets within the bar.
local function drawBar(random, cells, barBeats)
  local out, t = {}, 0
  while t < barBeats - 1e-9 do
    local left = barBeats - t
    local fits = {}
    for _, c in ipairs(cells) do if c.len <= left + 1e-9 then fits[#fits + 1] = c end end
    if #fits == 0 then
      for _, c in ipairs(FILL) do if c.len <= left + 1e-9 then fits[#fits + 1] = c end end
    end
    if #fits == 0 then break end
    local c = weighted(random, fits, function(x) return x.w end)
    for _, n in ipairs(c.notes) do out[#out + 1] = { start = t + n[1], len = n[2] } end
    t = t + c.len
  end
  return out
end

-- A cadence bar: the motif up to half-way, then one note held to the end.
local function cadenceBar(motif, barBeats)
  local out, half = {}, barBeats / 2
  for _, n in ipairs(motif) do
    if n.start + 1e-9 < half then out[#out + 1] = { start = n.start, len = math.min(n.len, half - n.start) } end
  end
  out[#out + 1] = { start = half, len = barBeats - half }
  return out
end

-- The same bar with its last beat redrawn: A', the repeat that answers.
local function varied(random, cells, motif, barBeats)
  local keepTo = math.max(0, barBeats - 1)
  local out = {}
  for _, n in ipairs(motif) do
    if n.start + n.len <= keepTo + 1e-9 then out[#out + 1] = { start = n.start, len = n.len } end
  end
  local tail = drawBar(random, cells, barBeats - keepTo)
  for _, n in ipairs(tail) do out[#out + 1] = { start = keepTo + n.start, len = n.len } end
  return out
end

--[[  The rhythm of the whole melody, as events { start, len, bar, role, k }
      where role is "A", "A'", "B" or "C" and k is the event's place in its
      bar - which is how a repeated bar knows which note of the motif it is
      echoing. Phrases are four bars; a shorter tail is A then C. ]]
function M.rhythm(random, density, barBeats, beats)
  local cells = M.DENSITIES[density].cells
  local motif = drawBar(random, cells, barBeats)
  -- A motif that is all rest is no motif. Draw again; a few tries is plenty.
  for _ = 1, 8 do
    if #motif > 0 then break end
    motif = drawBar(random, cells, barBeats)
  end
  local bars = math.max(1, math.ceil(beats / barBeats - 1e-6))
  local events = {}
  local bar = 0
  while bar < bars do
    local left = bars - bar
    local plan
    if left >= 4 then
      plan = random() < 0.5 and { "A", "A'", "A", "C" } or { "A", "B", "A", "C" }
    elseif left == 3 then plan = { "A", "A'", "C" }
    elseif left == 2 then plan = { "A", "C" }
    else plan = { "C" } end
    local b = nil
    for _, role in ipairs(plan) do
      local r
      if role == "A" then r = motif
      elseif role == "A'" then r = varied(random, cells, motif, barBeats)
      elseif role == "B" then b = b or drawBar(random, cells, barBeats); r = b
      else r = cadenceBar(motif, barBeats) end
      for k, n in ipairs(r) do
        local start = bar * barBeats + n.start
        if start < beats - 1e-9 then
          events[#events + 1] = { start = start, len = math.min(n.len, beats - start),
                                  bar = bar, role = role, k = k }
        end
      end
      bar = bar + 1
    end
  end
  return events
end

------------------------------------------------------------------------------
-- Pitch
------------------------------------------------------------------------------

local function chordAt(chords, t)
  local found = chords[1]
  for _, c in ipairs(chords) do
    if c.start <= t + 1e-9 then found = c else break end
  end
  return found
end

-- The scale notes that may pass over a chord: the key's, less any that sit
-- a semitone from a chord tone borrowed from outside the key. Over E major
-- in A minor the G# is a chord tone, so G natural is no longer a passing
-- note - it would clash with it.
local function localScale(key, chord)
  local ok = {}
  for pc in pairs(key.pcs) do ok[pc] = true end
  for pc in pairs(chord.pcs) do
    if not key.pcs[pc] then
      for _, near in ipairs({ (pc + 1) % 12, (pc + 11) % 12 }) do
        if near ~= key.tonic then ok[near] = nil end
      end
      ok[pc] = true
    end
  end
  return ok
end

-- A pitch's place counted in scale steps, for echoing a shape: notes outside
-- the key take the step of the scale note below them.
local function stepOf(key, p)
  local pc, oct = p % 12, p // 12
  for back = 0, 11 do
    local d = key.degree[(pc - back) % 12]
    if d then
      local o = oct - (((pc - back) < 0) and 1 or 0)
      return o * 7 + d
    end
  end
  return oct * 7
end

-- Strong: on the bar line, on the half bar, or where the chord changes.
local function isStrong(ev, chords, barBeats)
  local inBar = ev.start - ev.bar * barBeats
  if inBar < 1e-9 or math.abs(inBar - barBeats / 2) < 1e-9 then return true end
  for _, c in ipairs(chords) do
    if math.abs(c.start - ev.start) < 1e-9 then return true end
  end
  return false
end

local function arch(t, total, centre, height)
  -- Up from the centre to a peak about two thirds of the way, and home.
  local x = t / math.max(total, 1e-9)
  local shape = x < 0.66 and (x / 0.66) or ((1 - x) / 0.34)
  return centre + height * shape
end

--[[  Pitches for a rhythm. Returns the notes, each { pitch, start, len, vel,
      strong }. ]]
function M.pitches(random, events, chords, key, barBeats, centre, beats)
  local lo, hi = centre - M.RANGE_BELOW, centre + M.RANGE_ABOVE
  local height = 4 + math.floor(random() * 5)          -- a fourth to a sixth
  local notes = {}
  local motifSteps = {}          -- k -> scale steps from the note before, in bar A
  local prev, lastMove = nil, 0
  local prevStep

  for i, ev in ipairs(events) do
    local chord = chordAt(chords, ev.start)
    local strong = isStrong(ev, chords, barBeats)
    local scale = localScale(key, chord)
    local target = arch(ev.start, beats, centre, height)
    local final = i == #events

    local options = {}
    for p = lo, hi do
      local pc = p % 12
      local isChord = chord.pcs[pc] == true
      if isChord or scale[pc] then
        local v = 0
        if final then
          -- Home: the tonic if the last chord holds it, its root if not.
          local home = chord.pcs[key.tonic] and key.tonic or (chord.root or key.tonic)
          v = (pc == home) and 10 or -100
        end
        local d = prev and math.abs(p - prev) or 0
        if strong then
          v = v + (isChord and M.STRONG_CHORD or M.STRONG_OTHER)
        elseif isChord then
          v = v + M.WEAK_CHORD
        else
          v = v + ((prev and d <= 2) and M.WEAK_STEPPED or M.WEAK_LEAPT)
        end
        if prev then
          v = v + (M.MOVE[d] or M.MOVE_FAR)
          if math.abs(lastMove) >= 5 then
            local back = (p - prev) * lastMove < 0 and d <= 2
            v = v + (back and M.RECOVER or M.NOT_RECOVERED)
          end
        end
        v = v + M.CONTOUR * math.abs(p - target)
        if (ev.role == "A" or ev.role == "A'") and ev.bar > 0 and motifSteps[ev.k] and prevStep then
          if stepOf(key, p) - prevStep == motifSteps[ev.k] then v = v + M.ECHO end
        end
        options[#options + 1] = { p = p, v = v }
      end
    end

    -- Drawn rather than taken: the best note most often, but not always.
    local best = -math.huge
    for _, o in ipairs(options) do best = math.max(best, o.v) end
    local pick = weighted(random, options, function(o)
      return math.exp((o.v - best) / M.TEMPERATURE)
    end)
    local p = pick.p

    local step = stepOf(key, p)
    if ev.bar == 0 and prevStep then motifSteps[ev.k] = step - prevStep end
    if prev then lastMove = p - prev end
    prev, prevStep = p, step
    notes[#notes + 1] = { pitch = p, start = ev.start, len = ev.len, vel = M.VELOCITY,
                          strong = strong }
  end
  return notes
end

------------------------------------------------------------------------------
-- Scoring and choosing
------------------------------------------------------------------------------

function M.score(notes, chords)
  if #notes == 0 then return -math.huge end
  local strong, strongOn, steps, moves, stuck, leaps = 0, 0, 0, 0, 0, 0
  local lo, hi = 999, -1
  for i, n in ipairs(notes) do
    lo, hi = math.min(lo, n.pitch), math.max(hi, n.pitch)
    if n.strong then
      strong = strong + 1
      if chordAt(chords, n.start).pcs[n.pitch % 12] then strongOn = strongOn + 1 end
    end
    if i > 1 then
      local d = math.abs(n.pitch - notes[i - 1].pitch)
      moves = moves + 1
      if d >= 1 and d <= 2 then steps = steps + 1 end
      if d > 7 then leaps = leaps + 1 end
      if i > 2 and n.pitch == notes[i - 1].pitch and n.pitch == notes[i - 2].pitch then
        stuck = stuck + 1
      end
    end
  end
  local s = 0
  if strong > 0 then s = s + M.SCORE_STRONG * strongOn / strong end
  if moves > 0 then s = s - M.SCORE_STEPS * math.abs(steps / moves - M.STEP_SHARE) end
  s = s - M.SCORE_SPAN * math.max(0, (hi - lo) - 16)
  s = s - M.SCORE_STUCK * stuck - M.SCORE_LEAP * leaps
  return s
end

-- The share of two melodies' notes, by start and pitch, that are not shared.
local function difference(a, b)
  local set, shared = {}, 0
  for _, n in ipairs(a) do set[n.start .. ":" .. n.pitch] = true end
  for _, n in ipairs(b) do if set[n.start .. ":" .. n.pitch] then shared = shared + 1 end end
  return 1 - shared / math.max(#a, #b, 1)
end

M.DISTINCT = 0.4   -- at least this share of notes must differ between two suggestions

--[[  The suggestions for a progression.

      chords: the reader's segments, each { start, len, pcs, root }.
      opts: { key, beats, barBeats, density, register, variation }

      Returns up to SUGGESTIONS, best first, each { notes, score, low, high }. ]]
function M.suggest(chords, T, opts)
  local key = opts.key
  -- The centre is left where the register puts it, not moved to a tonic:
  -- the three registers are closer together than an octave, so snapping
  -- them to a tonic put two of them on the same note. The last note is what
  -- comes home, and it finds the tonic nearest the centre by itself.
  local centre = M.REGISTERS[opts.register or 2].centre

  local base = 1 + (opts.variation or 0) * 1009
  local cands = {}
  for c = 1, M.CANDIDATES do
    local random = rng(base * 7919 + c * 104729)
    local events = M.rhythm(random, opts.density or 2, opts.barBeats, opts.beats)
    local notes = M.pitches(random, events, chords, key, opts.barBeats, centre, opts.beats)
    cands[#cands + 1] = { notes = notes, score = M.score(notes, chords), seed = c }
  end
  table.sort(cands, function(a, b)
    if a.score ~= b.score then return a.score > b.score end
    return a.seed < b.seed
  end)

  local out = {}
  for _, c in ipairs(cands) do
    local fresh = #c.notes > 0
    for _, o in ipairs(out) do
      if difference(c.notes, o.notes) < M.DISTINCT then fresh = false; break end
    end
    if fresh then
      local lo, hi = 999, -1
      for _, n in ipairs(c.notes) do
        lo, hi = math.min(lo, n.pitch), math.max(hi, n.pitch)
        n.strong = nil
      end
      c.low, c.high, c.seed = lo, hi, nil
      out[#out + 1] = c
      if #out >= M.SUGGESTIONS then break end
    end
  end
  return out
end

return M
