--[[ Midi Catalogue - the catalogue.

     Pure Lua: no REAPER, no ImGui. Everything here is a pure function of a
     context (key, chords, instrument, register) and a set of choices, so
     tests/test_catalogue.lua can run all of it and check what comes out.

     Nothing in the catalogue is stored and nothing is random. Each type below
     is a small set of musical choices - a figure, a rhythm, a voicing - and
     its catalogue is every combination of them, generated for the key, the
     chords and the instrument in front of it, then filtered by the rules a
     player or a harmony teacher would apply. What is left is what the window
     lists. What was filtered out is counted, with the reason, so the list
     never quietly shrinks.

     Times are in quarter notes (beats) from the start of the block. A note is
     { start, len, pitch, accent, voice, ct } - `ct` records whether it was a
     chord tone when it was made, which is what lets a transformation keep it
     on the harmony afterwards.
]]

local M = {}
local T, O

function M.init(theory, orchestra)
  T, O = theory, orchestra
  return M
end

local EPS = 1e-6

M.MAX_NOTES = 4096
M.VELOCITY  = 100    -- everything leaves at 100
M.ACCENT    = 115    -- unless accents are asked for, when accented notes rise above it

M.CATEGORIES = {
  { name = "Rhythm", types = { "Ostinato", "Pulse", "Syncopation", "Gallop", "Tresillo",
      "Hemiola", "Polyrhythm", "Arpeggio", "Repeated note", "Alberti", "Broken chord" } },
  { name = "Melody", types = { "Ascending", "Descending", "Arch", "Wave", "Sequence",
      "Call/response", "Repetition", "Development" } },
  { name = "Harmony", types = { "Triadic", "Quartal", "Cluster", "Pedal", "Arpeggiated",
      "Contrary motion" } },
}

M.TRANSFORMS = { "Original", "Inversion", "Retrograde", "Retrograde inversion",
                 "Augmentation", "Diminution" }
M.REPEATS    = { 1, 2, 4 }
M.DENSITIES  = { "Any", "Sparse", "Medium", "Busy" }
M.VELOCITIES = { "Flat 100", "Accents" }
M.LENGTHS    = { 1, 2, 4, 8 }

-- Onsets per beat that separate the density bands. A held chord is sparse,
-- a quarter-note pulse medium, anything moving in sixteenths busy.
M.SPARSE_BELOW = 1
M.BUSY_FROM    = 2.5

function M.categoryOf(typeName)
  for _, c in ipairs(M.CATEGORIES) do
    for _, t in ipairs(c.types) do if t == typeName then return c.name end end
  end
  return nil
end

------------------------------------------------------------------------------
-- Context
------------------------------------------------------------------------------

-- Everything the generators need to know, from the choices the window holds.
-- `o` is plain: root and scale indices, the chain of chords (or, for short, a
-- progression id and "triad" or "seventh"), bars, the project's bar length
-- and tempo, an instrument id, a register and optionally an ensemble id.
--
-- Each chord gets the scale as it sounds under it (T.chordKey), and every
-- generator turns scale steps into notes through the slot's key, so a figure
-- written as 1-3-5 plays a borrowed chord's own third.
function M.context(o)
  local key  = T.key(o.root or 1, o.scale or 1)
  local chain = o.chain
  if type(chain) == "string" then chain = T.parseChain(chain, key) end
  if not chain or #chain == 0 then
    local prog = T.progressionById(o.prog or "I")
    if not T.progressionFits(key, prog) then prog = T.PROGRESSIONS[1] end
    chain = T.presetChain(prog, o.colour == "seventh")
  end
  local ctx = {
    key = key, chain = chain, n = T.scaleLen(key),
    bars = o.bars or 4, barBeats = o.barBeats or 4, bpm = o.bpm or 120,
    inst = O.byId(o.inst or "pno") or O.byId("pno"),
    register = o.register or "Middle",
    ensemble = o.ensemble and O.ensembleById(o.ensemble) or nil,
    cache = {},
  }
  ctx.L = ctx.bars * ctx.barBeats
  local k = #chain
  ctx.slots = {}
  for i, link in ipairs(chain) do
    local chord = T.chord(key, link.degree, T.chainSpec(link))
    chord.key = T.chordKey(key, chord)
    ctx.slots[i] = { i = i, start = (i - 1) * ctx.L / k, len = ctx.L / k, degree = link.degree,
                     chord = chord, key = chord.key }
  end
  ctx.lo, ctx.hi, ctx.centre = O.band(ctx.inst, ctx.register)
  if ctx.inst.pitches == "td" then ctx.drums = M.drums(ctx) end
  return ctx
end

-- The slot, and so the chord, sounding at a time. Past the end of the block
-- it wraps, so a repeat reads the chords from the top again.
function M.slotAt(ctx, t)
  local k = #ctx.slots
  local x = t % ctx.L
  local i = math.floor(x / (ctx.L / k) + EPS) + 1
  if i > k then i = k end
  if i < 1 then i = 1 end
  return ctx.slots[i]
end

-- The scale as it sounds at a time: the key, bent to the chord there.
local function keyAt(ctx, t) return M.slotAt(ctx, t).key end
M.keyAt = keyAt

------------------------------------------------------------------------------
-- Settings
--
-- One plain table holds everything the window has chosen. Choices from lists
-- that differ between contexts - a progression, an instrument, an entry - are
-- kept by name, not by position, so changing one thing does not slide another
-- along its list.
------------------------------------------------------------------------------

function M.newState()
  return {
    root = 1, scale = 1,                 -- C major
    chain = "0:d:Triad,4:d:Triad,5:d:Triad,3:d:Triad", bars = 4,   -- I-V-vi-IV
    inst = "pno", section = "",          -- a section id, or "" for the instrument
    register = "Middle",
    cat = "Rhythm", type = "Ostinato", entry = "",
    transform = "Original", repeats = 1, velocity = "Flat 100", density = "Any",
  }
end

local function oneOf(v, list, fallback)
  for _, x in ipairs(list) do if x == v then return v end end
  return fallback
end

-- Settings can come from a saved project written by an older version, whose
-- tables were different. Everything is put back inside what exists now.
function M.clampState(st)
  local function pin(v, lo, hi, fallback)
    v = tonumber(v) or fallback
    return math.max(lo, math.min(math.floor(v), hi))
  end
  st.root  = pin(st.root, 1, #T.ROOTS, 1)
  st.scale = pin(st.scale, 1, #T.SCALES, 1)
  local key = T.key(st.root, st.scale)
  -- The chain keeps every link this scale still has; if none survive, it
  -- starts again from the first progression the scale can play.
  local chain = T.parseChain(st.chain, key)
  if #chain == 0 then
    local fallback = T.progressionById("I-V-vi-IV")
    if not T.progressionFits(key, fallback) then fallback = T.progressionsFor(key)[1] end
    chain = T.presetChain(fallback)
  end
  st.chain = T.chainString(chain)
  local bars, best = tonumber(st.bars) or 4, nil
  for _, b in ipairs(M.LENGTHS) do
    if not best or math.abs(b - bars) < math.abs(best - bars) then best = b end
  end
  st.bars = best
  if not O.byId(st.inst) then st.inst = "pno" end
  if st.section ~= "" and not O.ensembleById(st.section) then st.section = "" end
  st.register = oneOf(st.register, O.REGISTERS, "Middle")
  local catNames = {}
  for _, c in ipairs(M.CATEGORIES) do catNames[#catNames + 1] = c.name end
  st.cat = oneOf(st.cat, catNames, "Rhythm")
  if st.section ~= "" then st.cat = "Harmony" end
  local types
  for _, c in ipairs(M.CATEGORIES) do if c.name == st.cat then types = c.types end end
  st.type = oneOf(st.type, types, types[1])
  st.entry = tostring(st.entry or "")
  st.transform = oneOf(st.transform, M.TRANSFORMS, "Original")
  local reps = tonumber(st.repeats) or 1
  st.repeats = oneOf(reps, M.REPEATS, 1)
  st.velocity = oneOf(st.velocity, M.VELOCITIES, "Flat 100")
  st.density = oneOf(st.density, M.DENSITIES, "Any")
  return st
end

-- The context the settings describe, in a project with this bar and tempo.
function M.contextFor(st, barBeats, bpm)
  return M.context({
    root = st.root, scale = st.scale, chain = st.chain, bars = st.bars,
    barBeats = barBeats, bpm = bpm, inst = st.inst, register = st.register,
    ensemble = (st.section ~= "" and st.cat == "Harmony") and st.section or nil,
  })
end

------------------------------------------------------------------------------
-- Pitch helpers
------------------------------------------------------------------------------

-- A figure is written the way players say it: 1-5-8-5 is root, fifth,
-- octave, fifth. The numbers are scale steps above the chord's root, so in a
-- pentatonic scale "5" is still the chord's fifth (two thirds up). A trailing
-- comma is the octave below: 7, is the note under the root.
function M.figSteps(tok, n)
  local num, below = tostring(tok):match("^(%d+)(,?)$")
  num = tonumber(num)
  local s = (num <= 7) and (num - 1) or (n + num - 8)
  if below == "," then s = s - n end
  return s
end

local function parseFig(s)
  local out = {}
  for tok in s:gmatch("[^-]+") do out[#out + 1] = tok end
  return out
end

-- The root position for a chord: within a window around `anchor`, and of
-- those the one nearest where the last chord's root was. A bass line moves
-- the short way, and never drifts off up the keyboard over a progression.
function M.rootNear(ctx, degree, anchor, prev)
  local best, bestD
  for k = -2, 12 do
    local pos = degree + k * ctx.n
    local p = T.pitch(ctx.key, pos)
    if p >= anchor - 7 and p <= anchor + 7 then
      local d = math.abs(p - (prev or anchor))
      if not bestD or d < bestD then best, bestD = pos, d end
    end
  end
  return best or degree + 4 * ctx.n
end

-- The chord-tone position nearest `pos`, searching outwards; `lean` says
-- which side wins a tie.
function M.nearestChordPos(ctx, chord, pos, lean)
  for d = 0, 2 * ctx.n do
    local first, second = pos + d, pos - d
    if (lean or 1) < 0 then first, second = second, first end
    local k = chord.key or ctx.key
    if T.hasPc(chord, T.pc(k, first)) then return first end
    if T.hasPc(chord, T.pc(k, second)) then return second end
  end
  return pos
end

-- The timpani are tuned to the tonic and the dominant, the dominant below:
-- the classic pair. Every figure on them alternates the two.
function M.drums(ctx)
  local inst = ctx.inst
  local tonicPc = T.rootPc(ctx.key)
  local fifthPc = (tonicPc + 7) % 12
  local best
  for p = inst.low, inst.high do
    if p % 12 == tonicPc and (not best or math.abs(p - 50) < math.abs(best - 50)) then best = p end
  end
  local tonic = best or inst.low
  local dom
  for p = tonic - 1, inst.low, -1 do
    if p % 12 == fifthPc then dom = p; break end
  end
  if not dom then
    for p = tonic + 1, inst.high do if p % 12 == fifthPc then dom = p; break end end
  end
  return { tonic = tonic, dominant = dom or tonic, tonicPc = tonicPc, fifthPc = fifthPc }
end

-- Which drum this chord wants: the tonic if the chord has it, else the
-- dominant, else neither - the timpani rest through a chord that has
-- neither note rather than play against it.
local function slotDrum(ctx, slot)
  local d = ctx.drums
  if T.hasPc(slot.chord, d.tonicPc) then return d.tonic, d.dominant end
  if T.hasPc(slot.chord, d.fifthPc) then return d.dominant, d.tonic end
  return nil
end

-- One token of a figure, on a chord whose root is at `rootPos`.
local function realise(ctx, slot, rootPos, tok)
  if ctx.drums then
    local main, other = slotDrum(ctx, slot)
    if not main then return nil end
    return (M.figSteps(tok, ctx.n) % ctx.n == 0) and main or other
  end
  return T.pitch(slot.key, rootPos + M.figSteps(tok, ctx.n))
end

-- Where a figure's root goes so the figure sits in the middle of the band.
local function anchorFor(ctx, fig)
  local lo, hi = 0, 0
  for _, tok in ipairs(fig) do
    local s = M.figSteps(tok, ctx.n)
    lo, hi = math.min(lo, s), math.max(hi, s)
  end
  return ctx.centre - (lo + hi) / 2 * 12 / ctx.n
end

-- Chord roots for every slot, for a figure.
local function slotRoots(ctx, fig)
  local out, prev = {}, nil
  local anchor = anchorFor(ctx, fig)
  for i, slot in ipairs(ctx.slots) do
    out[i] = M.rootNear(ctx, slot.degree, anchor, prev)
    prev = T.pitch(ctx.key, out[i])
  end
  return out
end

------------------------------------------------------------------------------
-- Voicings shared by everything that plays the chords
------------------------------------------------------------------------------

-- T.voiceLead, remembered for the life of a context: a catalogue asks for the
-- same progression voiced the same way once per rhythm it offers.
local function lead(ctx, chords, ranges, opts)
  local k = {}
  for _, ch in ipairs(chords) do k[#k + 1] = ch.degree .. ch.kind .. table.concat(ch.pcs, ".") end
  for _, r in ipairs(ranges) do k[#k + 1] = r[1] .. "-" .. r[2] end
  local names = {}
  for name in pairs(opts or {}) do names[#names + 1] = name end
  table.sort(names)
  for _, name in ipairs(names) do
    local v = opts[name]
    if type(v) == "table" then
      local ks = {}
      for x in pairs(v) do ks[#ks + 1] = tostring(x) end
      table.sort(ks)
      v = table.concat(ks, ".")
    end
    k[#k + 1] = name .. "=" .. tostring(v)
  end
  local sig = table.concat(k, "|")
  ctx.cache.lead = ctx.cache.lead or {}
  local hit = ctx.cache.lead[sig]
  if hit == nil then
    hit = T.voiceLead(ctx.key, chords, ranges, opts) or false
    ctx.cache.lead[sig] = hit
  end
  return hit or nil
end

-- The progression voiced for this instrument. A melodic instrument gets its
-- own voice of a four-part chorale; a chordal one gets the chords themselves,
-- in its register. Cached per context, because many entries ask.
local function instrumentVoicings(ctx)
  if ctx.cache.voicings ~= nil then return ctx.cache.voicings, ctx.cache.take end
  local inst = ctx.inst
  local chords = {}
  for i, s in ipairs(ctx.slots) do chords[i] = s.chord end
  local v, take
  if inst.poly == 1 then
    local ranges
    ranges, take = O.satbFor(inst, ctx.register)
    v = lead(ctx, chords, ranges, {})
  else
    local nv = math.min(4, inst.poly)
    v = lead(ctx, chords, O.chordRanges(inst, ctx.register, nv), {})
  end
  ctx.cache.voicings, ctx.cache.take = v or false, take
  return ctx.cache.voicings, take
end

-- The pitches a rhythm plays on a slot: one for a melodic instrument (its own
-- voice), the whole chord for a chordal one.
local function voicePitches(ctx, slot)
  if ctx.drums then
    local main = slotDrum(ctx, slot)
    return main and { main } or {}
  end
  local v, take = instrumentVoicings(ctx)
  if not v then return {} end
  if take then return { v[slot.i][take] } end
  return v[slot.i]
end

------------------------------------------------------------------------------
-- Building notes
------------------------------------------------------------------------------

local function add(list, start, len, pitch, accent, voice)
  if not pitch then return end
  list[#list + 1] = { start = start, len = math.max(len, 0.03), pitch = math.floor(pitch),
                      accent = accent or false, voice = voice }
end

-- A rhythm cell tiled from s to e, counting cycles rather than accumulating
-- time so the last hit cannot drift. Hits are { at, len, accent }.
local function tile(cell, s, e)
  local out, k = {}, 0
  while true do
    local base = s + k * cell.cycle
    if base >= e - EPS then break end
    for h, hit in ipairs(cell.hits) do
      local t = base + hit[1]
      if t < e - EPS then
        out[#out + 1] = { t = t, len = math.min(hit[2], e - t), acc = hit[3] or false,
                          hit = h, cyc = k }
      end
    end
    k = k + 1
  end
  return out
end

local function even(step, accentEvery)
  return { cycle = step, hits = { { 0, step, true } }, every = accentEvery }
end

-- Rates, by the names players use.
local RATES = {
  ["1/2"] = 2, ["1/4"] = 1, ["1/8"] = 0.5, ["1/8T"] = 1 / 3, ["1/16"] = 0.25,
}

-- A note on a bar line or on the first beat of a chord.
local function onDownbeat(ctx, t)
  local b = t / ctx.barBeats
  if math.abs(b - math.floor(b + 0.5)) < EPS then return true end
  for _, s in ipairs(ctx.slots) do
    if math.abs((t % ctx.L) - s.start) < EPS then return true end
  end
  return false
end

------------------------------------------------------------------------------
-- The pitch plans a rhythm can be played on
------------------------------------------------------------------------------

-- "fig": a figure, cycling one token per onset (or per hit of the cell).
-- "voice": the harmony itself - the chords on a chordal instrument, this
-- instrument's own smoothly-led voice on a melodic one.
local function planLabel(ctx, plan)
  if plan.kind == "voice" then
    if ctx.drums then return "tonic & dominant" end
    return (ctx.inst.poly > 1) and "chords" or "voice-led"
  end
  return plan.label
end

local PLAN_ROOT  = { kind = "fig", fig = { "1" }, label = "root" }
local PLAN_158   = { kind = "fig", fig = { "1", "5", "8" }, label = "1-5-8" }
local PLAN_18    = { kind = "fig", fig = { "1", "8" }, label = "root & octave" }
local PLAN_VOICE = { kind = "voice" }

-- Plays hits (from tile) on a slot with a plan. `idx` picks the figure token:
-- by onset within the slot unless the plan says by hit within the cell.
local function playHits(ctx, notes, slot, rootPos, hits, plan, gate)
  for i, h in ipairs(hits) do
    local len = h.len * (gate or 0.85)
    if plan.kind == "voice" then
      for v, p in ipairs(voicePitches(ctx, slot)) do add(notes, h.t, len, p, h.acc, v) end
    else
      local k = plan.byHit and h.hit or i
      local tok = plan.fig[((k - 1) % #plan.fig) + 1]
      add(notes, h.t, len, realise(ctx, slot, rootPos, tok), h.acc)
    end
  end
end

-- The common shape of most rhythm types: a cell tiled through each chord,
-- restarting on each, played on a plan.
--
-- A cell longer than a chord - the two-bar clave under one chord a bar -
-- cannot restart on each chord without losing its second half, so it runs
-- straight through the block instead and each note takes the chord under it.
local function rhythmOnSlots(ctx, cell, plan, gate)
  local notes = {}
  local fig = (plan.kind == "fig") and plan.fig or { "1" }
  local roots = slotRoots(ctx, fig)
  if cell.cycle > ctx.slots[1].len + EPS then
    local hits = tile(cell, 0, ctx.L)
    for i, h in ipairs(hits) do
      local slot = M.slotAt(ctx, h.t)
      h.len = math.min(h.len, slot.start + slot.len - h.t)
      local one = { h }
      if plan.kind == "fig" and not plan.byHit then
        -- Keep the figure's count running through the block.
        one = { { t = h.t, len = h.len, acc = h.acc, hit = i, cyc = h.cyc } }
        playHits(ctx, notes, slot, roots[slot.i], one, { kind = "fig", fig = plan.fig, byHit = true }, gate)
      else
        playHits(ctx, notes, slot, roots[slot.i], one, plan, gate)
      end
    end
    return notes
  end
  for i, slot in ipairs(ctx.slots) do
    local hits = tile(cell, slot.start, slot.start + slot.len)
    if cell.every then
      for _, h in ipairs(hits) do h.acc = onDownbeat(ctx, h.t) or (h.cyc % cell.every == 0 and h.hit == 1) end
    end
    playHits(ctx, notes, slot, roots[i], hits, plan, gate)
  end
  return notes
end

------------------------------------------------------------------------------
-- The types
--
-- Each has `params(ctx)`, the list of combinations, and `build(ctx, p)`.
-- A param has an `id` (stable across keys and instruments, so a choice
-- survives changing them), a short `label` for its button and a `hint`
-- sentence for its tooltip.
------------------------------------------------------------------------------

local TYPES = {}
M.TYPES = TYPES

-- Every combination of the lists given, as a list of tables.
local function cross(lists)
  local out = { {} }
  for _, l in ipairs(lists) do
    local nxt = {}
    for _, partial in ipairs(out) do
      for _, item in ipairs(l.values) do
        local c = {}
        for k, v in pairs(partial) do c[k] = v end
        c[l.name] = item
        nxt[#nxt + 1] = c
      end
    end
    out = nxt
  end
  return out
end

-- The plans a rhythm type offers: the figures it was given, then the
-- harmony itself.
local function plans(...)
  local out = { ... }
  out[#out + 1] = PLAN_VOICE
  return out
end

--------------------------------------------------------------------- Rhythm

local OSTINATO_FIGS = { "1-5-8-5", "1-8", "1-3-5-3", "1-2-3-5", "1-2-1-7,",
                        "1-5-6-5", "1-3-5-6-5-3" }

TYPES.Ostinato = {
  params = function(ctx)
    local out = {}
    for _, f in ipairs(OSTINATO_FIGS) do
      for _, r in ipairs({ "1/4", "1/8", "1/16" }) do
        out[#out + 1] = { id = f .. "@" .. r, label = f .. "  " .. r, fig = parseFig(f), rate = r,
          hint = ("The figure %s in %s notes, over and over, starting again on each chord.")
                 :format(f, r) }
      end
    end
    return out
  end,
  build = function(ctx, p)
    local plan = { kind = "fig", fig = p.fig }
    local cell = even(RATES[p.rate])
    local notes = {}
    local roots = slotRoots(ctx, p.fig)
    for i, slot in ipairs(ctx.slots) do
      local hits = tile(cell, slot.start, slot.start + slot.len)
      for k, h in ipairs(hits) do h.acc = ((k - 1) % #p.fig == 0) end
      playHits(ctx, notes, slot, roots[i], hits, plan, 0.85)
    end
    return notes
  end,
}

TYPES.Pulse = {
  params = function(ctx)
    local out = {}
    for _, pl in ipairs(plans(PLAN_ROOT, PLAN_18)) do
      for _, r in ipairs({ "1/2", "1/4", "1/8", "1/16" }) do
        local lbl = planLabel(ctx, pl)
        out[#out + 1] = { id = (pl.label or "voice") .. "@" .. r, label = lbl .. "  " .. r,
          plan = pl, rate = r,
          hint = ("A steady pulse in %s notes on the %s. The floor of a texture."):format(r, lbl) }
      end
    end
    return out
  end,
  build = function(ctx, p)
    return rhythmOnSlots(ctx, even(RATES[p.rate], p.rate == "1/16" and 4 or 2), p.plan,
                         RATES[p.rate] >= 2 and 0.95 or 0.85)
  end,
}

-- Cells are { cycle, hits = { { at, len, accent } } } in beats.
local SYNC_CELLS = {
  { id = "offbeats",   label = "Off-beats",  cycle = 1, hits = { { 0.5, 0.5, true } },
    hint = "Every note on the 'and', none on the beat." },
  { id = "charleston", label = "Charleston", cycle = 4, hits = { { 0, 1.5, true }, { 1.5, 0.5, true } },
    hint = "A dotted quarter and an eighth pushed onto the 'and' of two." },
  { id = "anticip",    label = "Anticipation", cycle = 4, hits = { { 0, 1.5, true }, { 1.5, 2, true } },
    anticipate = true,
    hint = "Each new chord arrives half a beat early, on the 'and' of four, and holds over the bar line." },
  { id = "clave32",    label = "Son clave 3-2", cycle = 8,
    hits = { { 0, 1.5, true }, { 1.5, 1.5, true }, { 3, 1, true }, { 5, 1, true }, { 6, 1, true } },
    hint = "The son clave, the three side first: 3+3+4 then 2+2 in eighths." },
  { id = "clave23",    label = "Son clave 2-3", cycle = 8,
    hits = { { 1, 1, true }, { 2, 2, true }, { 4, 1.5, true }, { 5.5, 1.5, true }, { 7, 1, true } },
    hint = "The son clave, the two side first." },
}

TYPES.Syncopation = {
  params = function(ctx)
    local out = {}
    for _, cell in ipairs(SYNC_CELLS) do
      for _, pl in ipairs(plans(PLAN_ROOT, PLAN_158)) do
        local lbl = planLabel(ctx, pl)
        out[#out + 1] = { id = cell.id .. "@" .. (pl.label or "voice"), label = cell.label .. "  " .. lbl,
          cell = cell, plan = pl, hint = cell.hint .. " Played on the " .. lbl .. "." }
      end
    end
    return out
  end,
  build = function(ctx, p)
    local cell = p.cell
    if not cell.anticipate then return rhythmOnSlots(ctx, cell, p.plan, 0.9) end

    -- The anticipation: the last half beat before each chord change plays
    -- the NEXT chord and ties over, and that chord's own downbeat is not
    -- struck again.
    local notes = {}
    local fig = (p.plan.kind == "fig") and p.plan.fig or { "1" }
    local roots = slotRoots(ctx, fig)
    local k = #ctx.slots
    for i, slot in ipairs(ctx.slots) do
      local e = slot.start + slot.len
      local hits = tile(cell, slot.start, e - 0.5)
      if i > 1 and hits[1] and math.abs(hits[1].t - slot.start) < EPS then table.remove(hits, 1) end
      playHits(ctx, notes, slot, roots[i], hits, p.plan, 0.9)
      if slot.len >= 1 then
        local nxt = ctx.slots[i % k + 1]
        -- Held over the bar line, but never into the next push.
        local hold = (i < k) and (0.5 + math.min(1, nxt.len - 0.5)) or 0.5
        local push = { { t = e - 0.5, len = hold, acc = true, hit = 1, cyc = 0 } }
        playHits(ctx, notes, nxt, roots[i % k + 1], push, p.plan, 0.95)
      end
    end
    return notes
  end,
}

local GALLOP_CELLS = {
  { id = "gallop",  label = "Gallop", cycle = 1,
    hits = { { 0, 0.5, true }, { 0.5, 0.25 }, { 0.75, 0.25 } },
    hint = "Long-short-short: an eighth and two sixteenths, the William Tell rhythm." },
  { id = "reverse", label = "Reverse gallop", cycle = 1,
    hits = { { 0, 0.25, true }, { 0.25, 0.25 }, { 0.5, 0.5 } },
    hint = "Short-short-long: two sixteenths and an eighth." },
  { id = "triplet", label = "Triplet gallop", cycle = 1,
    hits = { { 0, 2 / 3, true }, { 2 / 3, 1 / 3 } },
    hint = "Long-short in triplets: the lilt of a jig or a shuffle." },
  { id = "slow",    label = "Slow gallop", cycle = 2,
    hits = { { 0, 1, true }, { 1, 0.5 }, { 1.5, 0.5 } },
    hint = "A quarter and two eighths: the gallop at half speed." },
}

TYPES.Gallop = {
  params = function(ctx)
    local out = {}
    local ANSWER = { kind = "fig", fig = { "1", "5", "5" }, byHit = true, label = "fifth answering" }
    for _, cell in ipairs(GALLOP_CELLS) do
      for _, pl in ipairs(plans(PLAN_ROOT, ANSWER)) do
        local lbl = planLabel(ctx, pl)
        out[#out + 1] = { id = cell.id .. "@" .. (pl.label or "voice"), label = cell.label .. "  " .. lbl,
          cell = cell, plan = pl, hint = cell.hint .. " On the " .. lbl .. "." }
      end
    end
    return out
  end,
  build = function(ctx, p) return rhythmOnSlots(ctx, p.cell, p.plan, 0.85) end,
}

local TRESILLO_CELLS = {
  { id = "332",   label = "3+3+2", cycle = 4,
    hits = { { 0, 1.5, true }, { 1.5, 1.5, true }, { 3, 1, true } },
    hint = "Eight eighths grouped 3+3+2: the tresillo." },
  { id = "332s",  label = "3+3+2 in 16ths", cycle = 2,
    hits = { { 0, 0.75, true }, { 0.75, 0.75, true }, { 1.5, 0.5, true } },
    hint = "The tresillo at double speed, twice a bar." },
  { id = "333322", label = "3+3+3+3+2+2", cycle = 8,
    hits = { { 0, 1.5, true }, { 1.5, 1.5, true }, { 3, 1.5, true }, { 4.5, 1.5, true },
             { 6, 1, true }, { 7, 1, true } },
    hint = "The long form over two bars: four groups of three eighths, then two of two." },
  { id = "cinquillo", label = "Cinquillo", cycle = 2,
    hits = { { 0, 0.5, true }, { 0.5, 0.25 }, { 0.75, 0.5, true }, { 1.25, 0.25 }, { 1.5, 0.5, true } },
    hint = "The tresillo with two notes filled in: long-short-long-short-long." },
}

TYPES.Tresillo = {
  params = function(ctx)
    local out = {}
    local BASS = { kind = "fig", fig = { "1", "5", "8" }, byHit = true, label = "1-5-8" }
    for _, cell in ipairs(TRESILLO_CELLS) do
      for _, pl in ipairs(plans(PLAN_ROOT, BASS)) do
        local lbl = planLabel(ctx, pl)
        out[#out + 1] = { id = cell.id .. "@" .. (pl.label or "voice"), label = cell.label .. "  " .. lbl,
          cell = cell, plan = pl, hint = cell.hint .. " On the " .. lbl .. "." }
      end
    end
    return out
  end,
  build = function(ctx, p) return rhythmOnSlots(ctx, p.cell, p.plan, 0.85) end,
}

-- A hemiola groups against the metre: threes against a duple bar, twos
-- against a triple one. It runs straight through the bar lines, which is the
-- point of it, so it is laid out over the whole block rather than per chord.
TYPES.Hemiola = {
  params = function(ctx)
    local triple = (ctx.barBeats % 3 == 0)
    local out = {}
    local sizes = triple
      and { { unit = 1, group = 2, label = "2 over 3 in quarters" }, { unit = 0.5, group = 2, label = "2 over 3 in eighths" } }
      or  { { unit = 0.5, group = 3, label = "3 over 2 in eighths" }, { unit = 0.25, group = 3, label = "3 over 2 in 16ths" } }
    local P135 = { kind = "fig", fig = { "1", "3", "5" }, label = "1-3-5" }
    for _, sz in ipairs(sizes) do
      for _, fill in ipairs({ "groups", "every" }) do
        for _, pl in ipairs(plans(PLAN_ROOT, P135)) do
          local lbl = planLabel(ctx, pl)
          local how = (fill == "groups") and "one note a group" or "accents on the groups"
          out[#out + 1] = { id = sz.label .. "@" .. fill .. "@" .. (pl.label or "voice"),
            label = sz.label .. ", " .. how .. "  " .. lbl, size = sz, fill = fill, plan = pl,
            hint = ("Notes grouped in %ss across the bar line (%s), on the %s.")
                   :format(sz.group == 3 and "three" or "two", how, lbl) }
        end
      end
    end
    return out
  end,
  build = function(ctx, p)
    local notes = {}
    local g = p.size.unit * p.size.group
    local fig = (p.plan.kind == "fig") and p.plan.fig or { "1" }
    local roots = slotRoots(ctx, fig)
    local count = math.floor(ctx.L / p.size.unit + EPS)
    local stepEvery = (p.fill == "groups") and p.size.group or 1
    for i = 0, count - 1, stepEvery do
      local t = i * p.size.unit
      local group = math.floor(i / p.size.group)
      local first = (i % p.size.group == 0)
      local slot = M.slotAt(ctx, t)
      local len = ((p.fill == "groups") and g or p.size.unit)
      len = math.min(len, ctx.L - t)
      local hit = { { t = t, len = len, acc = first, hit = 1, cyc = 0 } }
      if p.plan.kind == "fig" then
        local tok = p.plan.fig[(group % #p.plan.fig) + 1]
        add(notes, t, len * 0.85, realise(ctx, slot, roots[slot.i], tok), first)
      else
        playHits(ctx, notes, slot, roots[slot.i], hit, p.plan, 0.85)
      end
    end
    return notes
  end,
}

local RATIOS = { { 3, 2 }, { 2, 3 }, { 4, 3 }, { 3, 4 }, { 5, 4 } }

TYPES.Polyrhythm = {
  params = function(ctx)
    local out = {}
    local spans = { { beats = 1, label = "a beat" }, { beats = ctx.barBeats / 2, label = "half a bar" },
                    { beats = ctx.barBeats, label = "a bar" } }
    for _, r in ipairs(RATIOS) do
      for _, sp in ipairs(spans) do
        out[#out + 1] = { id = r[1] .. ":" .. r[2] .. "@" .. sp.label,
          label = ("%d against %d, %s"):format(r[1], r[2], sp.label), a = r[1], b = r[2], span = sp.beats,
          hint = ("%d evenly spaced notes on the fifth against %d on the root, every %s. "):format(r[1], r[2], sp.label)
            .. (ctx.inst.poly > 1 and "Both layers sound together." or
                "One instrument plays the rhythm the two layers make together.") }
      end
    end
    return out
  end,
  build = function(ctx, p)
    local notes = {}
    local roots = slotRoots(ctx, { "1", "5" })
    local windows = math.floor(ctx.L / p.span + EPS)
    local mono = (ctx.inst.poly == 1)
    local events = {}
    for w = 0, windows - 1 do
      local base = w * p.span
      for i = 0, p.a - 1 do
        events[#events + 1] = { t = base + i * p.span / p.a, layer = 2, step = p.span / p.a, first = (i == 0) }
      end
      for j = 0, p.b - 1 do
        events[#events + 1] = { t = base + j * p.span / p.b, layer = 1, step = p.span / p.b, first = (j == 0) }
      end
    end
    table.sort(events, function(x, y)
      if math.abs(x.t - y.t) > EPS then return x.t < y.t end
      return x.layer < y.layer
    end)
    if mono then
      -- Where the layers meet, the lower one sounds: the composite rhythm is
      -- what one player can play.
      local merged = {}
      for _, e in ipairs(events) do
        local last = merged[#merged]
        if not (last and math.abs(last.t - e.t) < EPS) then merged[#merged + 1] = e end
      end
      for i, e in ipairs(merged) do
        local nextT = merged[i + 1] and merged[i + 1].t or math.min(ctx.L, e.t + e.step)
        local slot = M.slotAt(ctx, e.t)
        local tok = (e.layer == 1) and "1" or "5"
        add(notes, e.t, (nextT - e.t) * 0.85, realise(ctx, slot, roots[slot.i], tok), e.first)
      end
    else
      for _, e in ipairs(events) do
        local slot = M.slotAt(ctx, e.t)
        local tok = (e.layer == 1) and "1" or "5"
        add(notes, e.t, e.step * 0.85, realise(ctx, slot, roots[slot.i], tok), e.first, e.layer)
      end
    end
    return notes
  end,
}

local ARP_DIRS = { "Up", "Down", "Up-down", "Down-up" }

-- The chord's tones from a starting position upwards, `octaves` octaves of
-- them, top included.
local function chordPool(ctx, chord, startPos, octaves)
  local pool = {}
  for pos = startPos, startPos + ctx.n * octaves do
    if T.hasPc(chord, T.pc(chord.key or ctx.key, pos)) then pool[#pool + 1] = pos end
  end
  return pool
end

local function direct(pool, dir)
  local out = {}
  if dir == "Up" or dir == "Up-down" then for i = 1, #pool do out[#out + 1] = pool[i] end end
  if dir == "Down" or dir == "Down-up" then for i = #pool, 1, -1 do out[#out + 1] = pool[i] end end
  if dir == "Up-down" then for i = #pool - 1, 2, -1 do out[#out + 1] = pool[i] end end
  if dir == "Down-up" then for i = 2, #pool - 1 do out[#out + 1] = pool[i] end end
  return out
end

TYPES.Arpeggio = {
  params = function(ctx)
    local out = {}
    for _, d in ipairs(ARP_DIRS) do
      for _, r in ipairs({ "1/8", "1/8T", "1/16" }) do
        for _, oc in ipairs({ 1, 2 }) do
          out[#out + 1] = { id = d .. "@" .. r .. "@" .. oc,
            label = ("%s  %s  %d oct"):format(d, r, oc), dir = d, rate = r, octaves = oc,
            hint = ("The chord broken %s over %d octave%s in %s notes. Each chord starts from the "
              .. "chord tone nearest where the last one started, so the arpeggios lead into each other.")
              :format(d:lower(), oc, oc > 1 and "s" or "", r) }
        end
      end
    end
    return out
  end,
  build = function(ctx, p)
    local notes = {}
    local step = RATES[p.rate]
    local startPitch = ctx.centre - 6 * p.octaves
    local prev
    for _, slot in ipairs(ctx.slots) do
      -- The first chord from its root; each later one from the chord tone
      -- nearest where the last one started, which is an inversion as often
      -- as not, and is what keeps the hand in one place.
      local s0
      if prev then s0 = M.nearestChordPos(ctx, slot.chord, T.nearestPos(ctx.key, prev), 1)
      else s0 = M.rootNear(ctx, slot.degree, startPitch) end
      prev = T.pitch(slot.key, s0)
      local seq = direct(chordPool(ctx, slot.chord, s0, p.octaves), p.dir)
      local count = math.floor(slot.len / step + EPS)
      for i = 0, count - 1 do
        local pos = seq[(i % #seq) + 1]
        add(notes, slot.start + i * step, step * 0.9, T.pitch(slot.key, pos), i % #seq == 0)
      end
    end
    return notes
  end,
}

TYPES["Repeated note"] = {
  params = function(ctx)
    local out = {}
    local P5 = { kind = "fig", fig = { "5" }, label = "fifth" }
    for _, pl in ipairs(plans(PLAN_ROOT, P5)) do
      for _, r in ipairs({ "1/8", "1/8T", "1/16" }) do
        local lbl = (pl.kind == "voice") and ((ctx.inst.poly > 1) and "top voice" or "voice-led") or pl.label
        if ctx.drums and pl.kind == "voice" then lbl = "tonic & dominant" end
        out[#out + 1] = { id = (pl.label or "voice") .. "@" .. r, label = lbl .. "  " .. r, plan = pl, rate = r,
          hint = ("One note struck again and again in %s notes, on the %s. "):format(r, lbl)
            .. (pl.kind == "voice" and "It holds a common tone where the chords share one, and moves by the smallest step where they do not." or "") }
      end
    end
    return out
  end,
  build = function(ctx, p)
    local notes = {}
    local step = RATES[p.rate]
    local roots = slotRoots(ctx, { "1", "5" })
    for _, slot in ipairs(ctx.slots) do
      local pitch
      if p.plan.kind == "voice" then
        local v = voicePitches(ctx, slot)
        pitch = v[#v]
      else
        pitch = realise(ctx, slot, roots[slot.i], p.plan.fig[1])
      end
      local count = math.floor(slot.len / step + EPS)
      for i = 0, count - 1 do
        local t = slot.start + i * step
        add(notes, t, step * 0.8, pitch, math.abs(t - math.floor(t + 0.5)) < EPS)
      end
    end
    return notes
  end,
}

-- A three-note voicing per chord, close position: root position, or the
-- nearest inversion to the last, which is how a left hand actually plays it.
local function closeTriads(ctx, voiceLed)
  local key = "close" .. tostring(voiceLed)
  if ctx.cache[key] ~= nil then return ctx.cache[key] end
  local chords = {}
  for i, s in ipairs(ctx.slots) do
    local ch = s.chord
    -- Three notes of each chord: root, third and fifth, or for a chord with a
    -- seventh, root, third and seventh - the three a hand keeps when it has
    -- only three fingers to spare.
    local pick = (#ch.pcs >= 4) and { 1, 2, 4 } or { 1, 2, 3 }
    local pcs = {}
    for _, m in ipairs(pick) do if ch.pcs[m] then pcs[#pcs + 1] = ch.pcs[m] end end
    local ess = {}
    for e = 1, #pcs do ess[e] = e end
    chords[i] = { degree = ch.degree, kind = "close", pcs = pcs, root = ch.root, essential = ess }
  end
  local lo = ctx.lo
  local ranges = { { lo, lo + 12 }, { lo + 2, lo + 17 }, { lo + 4, lo + 21 } }
  local out
  if voiceLed then
    out = lead(ctx, chords, ranges, { bassRoot = false, close = true })
  else
    out = {}
    for i, s in ipairs(ctx.slots) do
      local r = T.pitch(ctx.key, M.rootNear(ctx, s.degree, lo + 5))
      local v = { r }
      local pcs = chords[i].pcs
      for m = 2, 3 do
        -- A two-note chord (a power chord) takes its root again on top.
        local want = pcs[((m - 1) % #pcs) + 1]
        local p = v[m - 1] + 1
        while p % 12 ~= want do p = p + 1 end
        v[m] = p
      end
      out[i] = v
    end
  end
  ctx.cache[key] = out or false
  return ctx.cache[key]
end

TYPES.Alberti = {
  params = function(ctx)
    local out = {}
    for _, ord in ipairs({ { "low-high-mid-high", { 1, 3, 2, 3 } }, { "low-mid-high-mid", { 1, 2, 3, 2 } } }) do
      for _, vl in ipairs({ false, true }) do
        for _, r in ipairs({ "1/8", "1/16" }) do
          out[#out + 1] = { id = ord[1] .. "@" .. tostring(vl) .. "@" .. r,
            label = ("%s  %s  %s"):format(ord[1], vl and "voice-led" or "root pos", r),
            order = ord[2], voiceLed = vl, rate = r,
            hint = ("A close three-note chord broken %s in %s notes, %s.")
              :format(ord[1], r, vl and "each chord in the inversion nearest the last, so the hand hardly moves"
                                    or "every chord in root position") }
        end
      end
    end
    return out
  end,
  build = function(ctx, p)
    local v = closeTriads(ctx, p.voiceLed)
    if not v then return {} end
    local notes = {}
    local step = RATES[p.rate]
    for _, slot in ipairs(ctx.slots) do
      local count = math.floor(slot.len / step + EPS)
      for i = 0, count - 1 do
        local k = p.order[(i % 4) + 1]
        add(notes, slot.start + i * step, step * 0.9, v[slot.i][k], i % 4 == 0)
      end
    end
    return notes
  end,
}

local BROKEN = {
  { id = "rolling", label = "Rolling 1-5-8-10-8-5", fig = "1-5-8-10-8-5",
    hint = "Up through root, fifth, octave and tenth and back: the open, rolling left hand." },
  { id = "spread",  label = "Spread 1-5-10", fig = "1-5-10",
    hint = "Root, fifth and the tenth above: an open voicing broken, three notes to a group." },
  { id = "picking", label = "Picking 1-5-8-5-10-5-8-5", fig = "1-5-8-5-10-5-8-5",
    hint = "The fingerpicking pattern: the root, then the fifth between every higher note." },
  { id = "oompah",  label = "Oom-pah", fig = "1-5", oompah = true,
    hint = "The bass on the beat and the chord off it." },
}

TYPES["Broken chord"] = {
  params = function(ctx)
    local out = {}
    for _, b in ipairs(BROKEN) do
      for _, r in ipairs({ "1/8", "1/16" }) do
        out[#out + 1] = { id = b.id .. "@" .. r, label = b.label .. "  " .. r, b = b, rate = r,
          hint = b.hint .. (b.oompah and (" Oom and pah are each a " .. (r == "1/8" and "quarter" or "eighth") .. ".")
                              or (" In " .. r .. " notes.")) }
      end
    end
    return out
  end,
  build = function(ctx, p)
    if p.b.oompah then
      local unit = RATES[p.rate] * 2
      local cell = { cycle = unit * 2, hits = { { 0, unit, true }, { unit, unit } } }
      if ctx.inst.poly > 1 and not ctx.drums then
        -- The bass alone, then the chord's upper voices together.
        local notes = {}
        local v = instrumentVoicings(ctx)
        if not v then return {} end
        for _, slot in ipairs(ctx.slots) do
          for _, h in ipairs(tile(cell, slot.start, slot.start + slot.len)) do
            local ch = v[slot.i]
            if h.hit == 1 then add(notes, h.t, h.len * 0.8, ch[1], true, 1)
            else for k = 2, #ch do add(notes, h.t, h.len * 0.6, ch[k], false, k) end end
          end
        end
        return notes
      end
      return rhythmOnSlots(ctx, cell, { kind = "fig", fig = { "1", "5" }, byHit = true }, 0.8)
    end
    local fig = parseFig(p.b.fig)
    local cell = even(RATES[p.rate])
    local notes = {}
    local roots = slotRoots(ctx, fig)
    for i, slot in ipairs(ctx.slots) do
      local hits = tile(cell, slot.start, slot.start + slot.len)
      for k, h in ipairs(hits) do h.acc = ((k - 1) % #fig == 0) end
      playHits(ctx, notes, slot, roots[i], hits, { kind = "fig", fig = fig }, 0.9)
    end
    return notes
  end,
}

--------------------------------------------------------------------- Melody

-- Melody rhythms, as a cell of durations repeated through the phrase. The
-- phrase always ends on a long note: the last bar held, or the second half of
-- a one-bar phrase.
local MEL_RHYTHMS = {
  { id = "q",   label = "quarters",         cell = { 1 } },
  { id = "e",   label = "eighths",          cell = { 0.5, 0.5 } },
  { id = "ls",  label = "long-short",       cell = { 1.5, 0.5 } },
  { id = "ssl", label = "short-short-long", cell = { 0.5, 0.5, 1 } },
  { id = "syn", label = "syncopated",       cell = { 0.5, 1, 0.5 } },
}

-- Short motifs for the types built from one: scale steps from the first note
-- and durations in beats, one bar of 4/4 each.
local MOTIFS = {
  { id = "climb", label = "Climb",        steps = { 0, 1, 2, 4 },    durs = { 1, 1, 1, 1 },
    hint = "Three steps up and a skip: 1-2-3-5." },
  { id = "turn",  label = "Turn",         steps = { 0, 1, 0, -1, 0 }, durs = { 0.5, 0.5, 0.5, 0.5, 2 },
    hint = "Around a note and back to it." },
  { id = "leap",  label = "Leap & fill",  steps = { 0, 4, 3, 2 },    durs = { 1, 1, 1, 1 },
    hint = "A leap up a fifth, then steps back down into the gap it left." },
  { id = "dotted", label = "Dotted rise", steps = { 0, 1, 2 },       durs = { 1.5, 0.5, 2 },
    hint = "A dotted step up, landing on a long note." },
  { id = "fall",  label = "Falling third", steps = { 2, 1, 0 },      durs = { 1, 1, 2 },
    hint = "Down by step from the third: the sigh." },
  { id = "zigzag", label = "Zig-zag",     steps = { 0, 2, 1, 3, 2 }, durs = { 0.5, 0.5, 0.5, 0.5, 2 },
    hint = "Up a third, back a step, up a third again." },
}
M.MOTIFS = MOTIFS

local function isStrong(ctx, t, len)
  if onDownbeat(ctx, t) then return true end
  local onBeat = math.abs(t - math.floor(t + 0.5)) < EPS
  return onBeat and len >= 2 - EPS
end

local function chordOf(ctx, nt) return M.slotAt(ctx, nt.start).chord end
local function isCT(ctx, nt) return T.hasPc(chordOf(ctx, nt), T.pc(keyAt(ctx, nt.start), nt.pos)) end
local function pitchOf(ctx, nt) return T.pitch(keyAt(ctx, nt.start), nt.pos) end

-- The rules every melody here passes through, whatever made it. Notes carry
-- positions (`pos`); these put strong notes on the chord, keep weak ones
-- honest, end the phrase properly and turn positions into pitches.
--
--   - A note on a downbeat, on a chord change, or held two beats or more is a
--     chord tone.
--   - A note off the chord has to be reached or left by step: a passing or
--     neighbour note. One leapt to and leapt from moves onto the chord.
--   - The last note is a chord tone, and a closing phrase ends on the root
--     when there is one within a fifth.
--   - In a seven-note scale there are no augmented seconds (the gap in
--     harmonic minor) and no tritone leaps: the weaker note of the pair gives
--     way.
local function snapStrong(ctx, notes, from)
  for i = from or 1, #notes do
    local nt = notes[i]
    nt.strong = isStrong(ctx, nt.start, nt.len)
    if nt.strong and not isCT(ctx, nt) then
      local nxt = notes[i + 1]
      local lean = nxt and nxt.pos and (nxt.pos >= nt.pos and 1 or -1) or -1
      nt.pos = M.nearestChordPos(ctx, chordOf(ctx, nt), nt.pos, lean)
    end
  end
end

local function repairLeapt(ctx, notes, from)
  for i = math.max(from or 1, 2), #notes - 1 do
    local a, b, c = notes[i - 1], notes[i], notes[i + 1]
    if not isCT(ctx, b) and math.abs(b.pos - a.pos) > 1 and math.abs(c.pos - b.pos) > 1 then
      b.pos = M.nearestChordPos(ctx, chordOf(ctx, b), b.pos, c.pos >= b.pos and 1 or -1)
    end
  end
end

local function closeLine(ctx, notes, closing)
  local last = notes[#notes]
  if not last then return end
  local ch = chordOf(ctx, last)
  if closing then
    -- Home is worth a leap: the root is looked for up to a fifth away
    -- before the line settles for another chord tone.
    local best
    for d = 0, 4 do
      for _, s in ipairs({ last.pos - d, last.pos + d }) do
        if not best and T.pc(ch.key or ctx.key, s) == ch.root then best = s end
      end
    end
    last.pos = best or M.nearestChordPos(ctx, ch, last.pos, -1)
  elseif not isCT(ctx, last) then
    last.pos = M.nearestChordPos(ctx, ch, last.pos, 1)
  end
end

-- An augmented second or a tritone between two notes, given as positions
-- and the pitches they sound as.
local function badInterval(ctx, a, b, pa, pb)
  if ctx.n ~= 7 then return false end
  local semis = math.abs(pb - pa)
  return (math.abs(b - a) == 1 and semis == 3) or semis == 6
end

local function repairIntervals(ctx, notes, from, closing)
  if ctx.n ~= 7 then return end
  local last = notes[#notes]
  for _ = 1, 3 do
    for i = math.max(from or 1, 2), #notes do
      local a, b = notes[i - 1], notes[i]
      if badInterval(ctx, a.pos, b.pos, pitchOf(ctx, a), pitchOf(ctx, b)) then
        -- The weaker of the two gives way: it takes the other's pitch for an
        -- augmented second, or steps towards it for a tritone.
        local weak = (b.strong and not a.strong) and a or b
        local other = (weak == a) and b or a
        if weak == last and closing then weak, other = other, weak end
        -- A strong note is on the chord for a reason; if both are strong
        -- the interval stands.
        if not weak.strong and not weak.fixedCadence then
          if math.abs(b.pos - a.pos) == 1 then weak.pos = other.pos
          else weak.pos = weak.pos + ((other.pos > weak.pos) and 1 or -1) end
        end
      end
    end
  end
end

local function toPitches(ctx, notes)
  for _, nt in ipairs(notes) do nt.pitch = pitchOf(ctx, nt) end
  return notes
end

local function finishMelody(ctx, notes, closing, fromIndex)
  snapStrong(ctx, notes, fromIndex)
  repairLeapt(ctx, notes, fromIndex)
  closeLine(ctx, notes, closing)
  repairIntervals(ctx, notes, fromIndex, closing)
  return toPitches(ctx, notes)
end

-- Fills the notes between fixed ones. Each gap is solved on its own, for the
-- cheapest path between the two fixed notes: steps rather than repeats,
-- passing and neighbour notes reached and left by step, chord tones preferred
-- on the beat, going the way the line is going, and no back-and-forth
-- wobbling - a neighbour note is an ornament, three in a row is a trill.
--
-- The path is searched over pairs of notes (this one and the one before), so
-- a change of direction and a return to the note before last can be seen and
-- priced.
local function sgn(x) if x > 0 then return 1 elseif x < 0 then return -1 end return 0 end

local function fillLine(ctx, notes)
  local fixed = {}
  for i, nt in ipairs(notes) do if nt.fixed then fixed[#fixed + 1] = i end end
  local seven = (ctx.n == 7)
  for f = 1, #fixed - 1 do
    local i, j = fixed[f], fixed[f + 1]
    if j - i > 1 then
      local A, B = notes[i].pos, notes[j].pos
      local k = j - i - 1
      local lo, hi = math.min(A, B) - 3, math.max(A, B) + 3
      local way = sgn(B - A)
      local before = (i > 1 and notes[i - 1].pos) or A

      -- Everything a step asks about a position, worked out once per gap.
      -- Pitches are per note, because the scale bends with the chord: P[m] is
      -- the scale under note m, from the first fixed note (0) to the second
      -- (k + 1).
      local P, CT, beat = {}, {}, {}
      for m = 0, k + 1 do
        local key = keyAt(ctx, notes[i + m].start)
        local row = {}
        for pos = lo - 3, hi + 3 do row[pos] = T.pitch(key, pos) end
        P[m] = row
      end
      for m = 1, k do
        local nt = notes[i + m]
        local ch = chordOf(ctx, nt)
        CT[m] = {}
        for pos = lo, hi do CT[m][pos] = T.hasPc(ch, P[m][pos] % 12) end
        beat[m] = math.abs(nt.start - math.floor(nt.start + 0.5)) < EPS
      end

      -- What moving from position a (note m - 1) to b (note m) costs. Steps
      -- are free, a skip of a third nearly so; leaps cost more the wider they
      -- are, and a note struck again costs something, because a line that
      -- keeps repeating itself has stopped being a line.
      local function move(a, b, m)
        local d = math.abs(b - a)
        local c
        if d == 0 then c = 2.5 elseif d == 1 then c = 0 elseif d == 2 then c = 0.8
        elseif d == 3 then c = 2 elseif d == 4 then c = 3 else c = 3 + (d - 4) * 1.5 end
        if seven then
          local semis = math.abs(P[m][b] - P[m - 1][a])
          if (d == 1 and semis == 3) or semis == 6 then c = c + 6 end
        end
        return c
      end
      -- The cost of arriving at `b` (note m) from `a`, with `pp` before that.
      local function step(pp, a, b, bCT, aCT, onBeat, m)
        local c = move(a, b, m)
        local d, back = sgn(b - a), sgn(a - pp)
        if d ~= 0 and back ~= 0 and d ~= back then c = c + 0.6 end
        if b == pp and a ~= b then c = c + 1.0 end
        if way ~= 0 and d == -way then c = c + 0.4 end
        local leap = math.abs(b - a) > 1
        if leap and not bCT then c = c + 3 end
        if leap and not aCT then c = c + 3 end
        if not bCT and onBeat then c = c + 0.7 end
        if m <= k then c = c + 0.25 * math.abs(b - (A + (B - A) * m / (k + 1))) end
        return c
      end

      -- cost[m][a][b]: the cheapest way to have note m-1 at a and note m at b.
      local cost, from = {}, {}
      cost[1], from[1] = { [A] = {} }, { [A] = {} }
      for b = lo, hi do cost[1][A][b] = step(before, A, b, CT[1][b], true, beat[1], 1) end
      for m = 2, k do
        local cm, fm = {}, {}
        local prev = cost[m - 1]
        for a = lo, hi do
          local aCT = CT[m - 1][a]
          local ca, fa = {}, {}
          for b = lo, hi do
            local best, bestPP
            for pp, row in pairs(prev) do
              local c0 = row[a]
              if c0 then
                local c = c0 + step(pp, a, b, CT[m][b], aCT, beat[m], m)
                if not best or c < best - EPS or (math.abs(c - best) <= EPS and pp < bestPP) then
                  best, bestPP = c, pp
                end
              end
            end
            ca[b], fa[b] = best, bestPP
          end
          cm[a], fm[a] = ca, fa
        end
        cost[m], from[m] = cm, fm
      end
      -- Into the second fixed note, which is on the chord by construction.
      local best, bestA, bestB
      local As = {}
      for a in pairs(cost[k]) do As[#As + 1] = a end
      table.sort(As)
      for _, a in ipairs(As) do
        for b = lo, hi do
          local c0 = cost[k][a][b]
          if c0 then
            local c = c0 + step(a, b, B, true, CT[k][b], false, k + 1)
            if not best or c < best - EPS then best, bestA, bestB = c, a, b end
          end
        end
      end
      for m = k, 1, -1 do
        notes[i + m].pos = bestB
        local pp = (m > 1) and from[m][bestA][bestB] or nil
        bestB, bestA = bestA, pp
      end
    end
  end
end

-- Onsets for a melody rhythm through a phrase, with the long last note.
local function melodyOnsets(ctx, cell)
  local L, bar = ctx.L, ctx.barBeats
  local tailStart = (L >= 2 * bar) and (L - bar) or (L / 2)
  local out, t, i = {}, 0, 0
  while t < tailStart - EPS do
    local d = cell[(i % #cell) + 1]
    out[#out + 1] = { start = t, len = math.min(d, tailStart - t) }
    t = t + d
    i = i + 1
  end
  out[#out + 1] = { start = tailStart, len = L - tailStart }
  return out
end

local function toNotes(list, gate)
  local out = {}
  for i, nt in ipairs(list) do
    local nextStart = list[i + 1] and list[i + 1].start or (nt.start + nt.len)
    local len = math.min(nt.len, nextStart - nt.start)
    add(out, nt.start, (i == #list) and (nt.len - 0.02) or len * (gate or 0.95), nt.pitch, nt.strong)
  end
  return out
end

-- Where a melody starts: the chosen tone of the first chord, near the middle
-- of the band.
local function melodyStart(ctx, tok, rise)
  local slot = ctx.slots[1]
  local rootPos = M.rootNear(ctx, slot.degree, ctx.centre - rise * 6 / ctx.n)
  return rootPos + M.figSteps(tok, ctx.n)
end

-- Realises notes[from..] as a line with a shape: f(u) for u from 0 to 1 is
-- how far along the span (in scale steps) the line is at that point. The
-- shape is carried by the strong notes - downbeats, chord changes and half
-- bars - each placed on the contour and onto a chord tone that does not go
-- back against it: a rising line's skeleton never steps down, a falling
-- one's never up. The notes between are filled in (fillLine).
local function shapeLine(ctx, notes, from, startPos, f, span, closing)
  local first, last = notes[from], notes[#notes]
  local t0, tail = first.start, last.start
  local half = (ctx.barBeats % 2 == 0) and ctx.barBeats / 2 or ctx.barBeats
  local prevSkel = (from > 1) and notes[from - 1].pos or nil
  for idx = from, #notes do
    local nt = notes[idx]
    nt.strong = isStrong(ctx, nt.start, nt.len)
    local q = nt.start / half
    local structural = math.abs(q - math.floor(q + 0.5)) < EPS
    if nt.strong or structural or idx == from or idx == #notes then
      local u = (tail > t0) and ((nt.start - t0) / (tail - t0)) or 0
      local target = startPos + f(u) * span
      local slope = f(math.min(1, u + 0.05)) - f(math.max(0, u - 0.05))
      local ch, best, bestC = chordOf(ctx, nt), nil, nil
      for pos = math.floor(target) - 3, math.ceil(target) + 3 do
        if T.hasPc(ch, T.pc(ch.key or ctx.key, pos)) then
          local cc = math.abs(pos - target)
          if prevSkel then
            if (slope > 0.01 and pos < prevSkel) or (slope < -0.01 and pos > prevSkel) then cc = cc + 4 end
            if pos == prevSkel then cc = cc + 0.6 end
          end
          if not bestC or cc < bestC - EPS then best, bestC = pos, cc end
        end
      end
      nt.pos = best or M.nearestChordPos(ctx, ch, math.floor(target + 0.5), 1)
      prevSkel = nt.pos
      nt.fixed = true
    end
  end
  closeLine(ctx, notes, closing)
  fillLine(ctx, notes)
  repairIntervals(ctx, notes, from, closing)
  return toPitches(ctx, notes)
end

local CONTOURS = {
  Ascending  = { f = function(u) return u end, starts = { "1", "5" }, closing = false,
                 say = "climbs steadily" },
  Descending = { f = function(u) return -u end, starts = { "5", "8" }, closing = true,
                 say = "falls steadily" },
  Arch       = { f = function(u) if u < 0.6 then return u / 0.6 end return (1 - u) / 0.4 end,
                 starts = { "1", "5" }, closing = true, say = "rises to one high point and comes back down" },
  Wave       = { f = function(u) return 0.5 - 0.5 * math.cos(2 * math.pi * 2 * u) end,
                 starts = { "1", "5" }, closing = true, say = "rises and falls twice", small = true },
}

local function contourType(name)
  local c = CONTOURS[name]
  return {
    params = function(ctx)
      local out = {}
      local spans = c.small and { { 2, "a third" }, { 4, "a fifth" } } or { { 4, "a fifth" }, { ctx.n, "an octave" } }
      for _, sp in ipairs(spans) do
        for _, r in ipairs(MEL_RHYTHMS) do
          for _, st in ipairs(c.starts) do
            out[#out + 1] = { id = sp[2] .. "@" .. r.id .. "@" .. st,
              label = ("%s, %s, from %s"):format(sp[2], r.label, st), span = sp[1], rhythm = r, start = st,
              hint = ("A line that %s over %s, in %s, starting on the %s of the first chord. "
                .. "Downbeats land on chord tones and it %s."):format(c.say, sp[2], r.label,
                  ({ ["1"] = "root", ["5"] = "fifth", ["8"] = "octave" })[st],
                  c.closing and "ends on the root" or "ends on a chord tone, open") }
          end
        end
      end
      return out
    end,
    build = function(ctx, p)
      local onsets = melodyOnsets(ctx, p.rhythm.cell)
      local startPos = melodyStart(ctx, p.start, (c.f(1) >= 0 and 1 or -1) * p.span)
      shapeLine(ctx, onsets, 1, startPos, c.f, p.span, c.closing)
      return toNotes(onsets)
    end,
  }
end

TYPES.Ascending  = contourType("Ascending")
TYPES.Descending = contourType("Descending")
TYPES.Arch       = contourType("Arch")
TYPES.Wave       = contourType("Wave")

-- A motif laid into [s, e) from a starting position, truncated at e.
local function state(motif, s, e, base, scale)
  local out, t = {}, s
  scale = scale or 1
  for i, st in ipairs(motif.steps) do
    if t >= e - EPS then break end
    local d = motif.durs[i] * scale
    out[#out + 1] = { start = t, len = math.min(d, e - t), pos = base + st }
    t = t + d
  end
  return out
end

-- Segments of a phrase: one a bar, or two halves of a one-bar phrase.
local function segments(ctx, want)
  local count = want or ((ctx.bars >= 2) and ctx.bars or 2)
  local len = ctx.L / count
  local out = {}
  for i = 0, count - 1 do out[#out + 1] = { s = i * len, e = (i + 1) * len } end
  return out
end

-- The chord-tone position of a segment's chord nearest `pos`.
local function fitBase(ctx, s, pos)
  return M.nearestChordPos(ctx, M.slotAt(ctx, s).chord, pos, 1)
end

-- Where a restatement of a motif goes: as near `base` as it can be while its
-- strong notes land on the chord under them. The whole statement moves, so
-- the motif keeps its shape - the sequence bends, the motif does not.
local function fitStatement(ctx, motif, s, e, base, scale)
  local best, bestC
  for _, d in ipairs({ 0, 1, -1, 2, -2, 3, -3 }) do
    local c = math.abs(d) * 0.9
    for _, nt in ipairs((function() local l = {}; local t = s
      for i, st in ipairs(motif.steps) do
        if t >= e - EPS then break end
        local dur = motif.durs[i] * (scale or 1)
        l[#l + 1] = { start = t, len = math.min(dur, e - t), pos = base + d + st }
        t = t + dur
      end
      return l end)()) do
      if isStrong(ctx, nt.start, nt.len) and not isCT(ctx, nt) then c = c + 3 end
    end
    if not bestC or c < bestC - EPS then best, bestC = base + d, c end
  end
  return best
end

-- A cadence into [s, e): the chord tone above the target on the beat, then
-- the target held. The target is the chord's root, or for an open (half)
-- cadence its fifth. Both notes are chord tones, so nothing needs bending.
local function cadence(ctx, s, e, fromPos, open)
  local slot = M.slotAt(ctx, s)
  local R = M.rootNear(ctx, slot.degree, T.pitch(ctx.key, fromPos))
  local target = R
  if open then target = M.nearestChordPos(ctx, slot.chord, R + 4, 1) end
  if e - s < 1 + EPS then return { { start = s, len = e - s, pos = target } } end
  local approach = M.nearestChordPos(ctx, slot.chord, target + 1, 1)
  local d1 = math.min(1, (e - s) / 2)
  return { { start = s, len = d1, pos = approach }, { start = s + d1, len = e - s - d1, pos = target } }
end

local function concat(lists)
  local out = {}
  for _, l in ipairs(lists) do for _, n in ipairs(l) do out[#out + 1] = n end end
  return out
end

-- The last note of a phrase holds to its end.
local function holdLast(ctx, list, e)
  local last = list[#list]
  if last then last.len = (e or ctx.L) - last.start end
  return list
end

local function motifParams(extra, describe)
  return function(ctx)
    local out = {}
    for _, m in ipairs(MOTIFS) do
      for _, x in ipairs(extra) do
        out[#out + 1] = { id = m.id .. "@" .. x.id, label = m.label .. ", " .. x.label, motif = m, how = x.id,
          hint = describe(m, x) }
      end
    end
    return out
  end
end

local function anchorPos(ctx)
  return fitBase(ctx, 0, T.nearestPos(ctx.key, math.floor(ctx.centre - 3)))
end

TYPES.Sequence = {
  params = motifParams({ { id = "down", label = "down a step" }, { id = "up", label = "up a step" },
                         { id = "chords", label = "with the chords" } },
    function(m, x) return ("The motif \"%s\" (%s) stated once a bar, each time %s. Where a "
      .. "downbeat would clash with the chord it bends onto it."):format(m.label, m.hint:lower(),
        x.id == "chords" and "moved to follow the chords' roots" or ("moved " .. x.label)) end),
  build = function(ctx, p)
    local segs = segments(ctx)
    local base0 = anchorPos(ctx)
    local d0 = ctx.slots[1].degree
    local all = {}
    for k, sg in ipairs(segs) do
      local base
      if p.how == "down" then base = base0 - (k - 1)
      elseif p.how == "up" then base = base0 + (k - 1)
      else
        local d = M.slotAt(ctx, sg.s).degree - d0
        if d > ctx.n / 2 then d = d - ctx.n end
        if d < -ctx.n / 2 then d = d + ctx.n end
        base = base0 + d
      end
      if k > 1 then base = fitStatement(ctx, p.motif, sg.s, sg.e, base) end
      all[#all + 1] = state(p.motif, sg.s, sg.e, base)
    end
    local notes = holdLast(ctx, concat(all))
    return toNotes(finishMelody(ctx, notes, true))
  end,
}

TYPES["Call/response"] = {
  params = motifParams({ { id = "falls", label = "answer falls home" }, { id = "inverted", label = "answer inverted" },
                         { id = "echo", label = "echo a third lower" } },
    function(m, x)
      local ans = ({ falls = "an answer that walks back down to the root",
                     inverted = "the call turned upside down, landing on the root",
                     echo = "the call again a third lower, landing on the root" })[x.id]
      return ("A call built on \"%s\" that stops on an open note, then %s."):format(m.label, ans)
    end),
  build = function(ctx, p)
    local half = ctx.L / 2
    local base = anchorPos(ctx)
    local call = {}
    local segLen = math.min(4, half)
    local reps = math.max(1, math.floor(half / segLen + EPS))
    for r = 0, reps - 1 do
      local b = (r == 0) and base or fitStatement(ctx, p.motif, r * segLen, (r + 1) * segLen, base + r)
      local part = state(p.motif, r * segLen, (r + 1) * segLen, b)
      for _, n in ipairs(part) do call[#call + 1] = n end
    end
    holdLast(ctx, call, half)
    finishMelody(ctx, call, false)
    -- An open ending: the call stops on the fifth or the third, not the root.
    local lastCall = call[#call]
    local ch = M.slotAt(ctx, lastCall.start).chord
    if T.pc(ch.key or ctx.key, lastCall.pos) == ch.root then
      local up = M.nearestChordPos(ctx, ch, lastCall.pos + 1, 1)
      local down = M.nearestChordPos(ctx, ch, lastCall.pos - 1, -1)
      lastCall.pos = (math.abs(up - lastCall.pos) <= math.abs(down - lastCall.pos)) and up or down
      lastCall.pitch = pitchOf(ctx, lastCall)
    end

    local resp = {}
    local a = call[1].pos
    local endPos = lastCall.pos
    for _, n in ipairs(call) do
      local pos
      if p.how == "inverted" then pos = 2 * a - n.pos
      elseif p.how == "echo" then pos = n.pos - 2 end
      resp[#resp + 1] = { start = n.start + half, len = n.len, pos = pos }
    end
    local notes = {}
    for _, n in ipairs(call) do n.fixed = true; notes[#notes + 1] = n end
    local from = #notes + 1
    for _, n in ipairs(resp) do notes[#notes + 1] = n end
    if p.how == "falls" then
      -- Down by step from just under where the call stopped, to the root.
      local lastSlot = M.slotAt(ctx, resp[#resp].start)
      local rootPos = M.rootNear(ctx, lastSlot.degree, T.pitch(ctx.key, endPos) - 5)
      if rootPos >= endPos then rootPos = rootPos - ctx.n end
      local startPos = endPos - 1
      local span = math.max(1, startPos - rootPos)
      shapeLine(ctx, notes, from, startPos, function(u) return -u end, span, true)
      return toNotes(notes)
    end
    return toNotes(finishMelody(ctx, notes, true, from))
  end,
}

TYPES.Repetition = {
  params = motifParams({ { id = "fitted", label = "three times, then a cadence" },
                         { id = "literal", label = "unchanged every bar" } },
    function(m, x)
      if x.id == "fitted" then
        return ("The motif \"%s\" stated in every bar but the last, each time moved to the nearest "
          .. "chord tone of that bar's chord, then a cadence onto the root."):format(m.label)
      end
      return ("The motif \"%s\" in every bar, note for note. Only offered where it fits every chord "
        .. "as it stands."):format(m.label)
    end),
  build = function(ctx, p)
    local segs = segments(ctx)
    local base = anchorPos(ctx)
    local all = {}
    for k, sg in ipairs(segs) do
      if p.how == "fitted" and k == #segs and k > 1 then
        -- The cadence: onto the root from the chord tone above it.
        all[#all + 1] = cadence(ctx, sg.s, sg.e, base + p.motif.steps[1], false)
      else
        if p.how == "fitted" and k > 1 then base = fitStatement(ctx, p.motif, sg.s, sg.e, base) end
        all[#all + 1] = state(p.motif, sg.s, sg.e, base)
      end
    end
    local notes = holdLast(ctx, concat(all))
    if p.how == "literal" then
      -- Unchanged means unchanged: if it clashes on a downbeat, it is not
      -- offered, rather than bent into something else.
      for i, n in ipairs(notes) do
        if (onDownbeat(ctx, n.start) or i == #notes)
           and not T.hasPc(M.slotAt(ctx, n.start).chord, T.pc(keyAt(ctx, n.start), n.pos)) then
          return nil, "clashes with the chords"
        end
        n.pitch = pitchOf(ctx, n)
        n.strong = onDownbeat(ctx, n.start)
      end
      return toNotes(notes)
    end
    return toNotes(finishMelody(ctx, notes, true))
  end,
}

TYPES.Development = {
  params = motifParams({ { id = "sentence", label = "sentence" }, { id = "inversion", label = "inversion" },
                         { id = "augment", label = "augmentation" } },
    function(m, x)
      local how = ({
        sentence  = "stated, restated on the next chord, broken into fragments that climb, then brought to a cadence",
        inversion = "stated, answered upside down, stated again, then brought to a cadence",
        augment   = "stated, then played at half speed across two bars, then brought to a cadence",
      })[x.id]
      return ("The motif \"%s\" %s. Four bars of development in miniature."):format(m.label, how)
    end),
  build = function(ctx, p)
    local groups = math.max(1, math.floor(ctx.bars / 4 + EPS))
    local segs = segments(ctx, 4 * groups)
    local base0 = anchorPos(ctx)
    local all = {}
    local m = p.motif
    for g = 0, groups - 1 do
      local s1, s2, s3, s4 = segs[g * 4 + 1], segs[g * 4 + 2], segs[g * 4 + 3], segs[g * 4 + 4]
      local last = (g == groups - 1)
      all[#all + 1] = state(m, s1.s, s1.e, base0)
      if p.how == "sentence" then
        all[#all + 1] = state(m, s2.s, s2.e, fitStatement(ctx, m, s2.s, s2.e, base0))
        -- Fragmentation: the first half of the motif, twice, a step higher each time.
        local frag = { steps = {}, durs = {} }
        local acc = 0
        for i, d in ipairs(m.durs) do
          if acc >= 2 - EPS then break end
          frag.steps[i], frag.durs[i] = m.steps[i], math.min(d, 2 - acc)
          acc = acc + d
        end
        local mid = (s3.s + s3.e) / 2
        local f1 = fitStatement(ctx, frag, s3.s, mid, base0 + 1, (mid - s3.s) / 2)
        all[#all + 1] = state(frag, s3.s, mid, f1, (mid - s3.s) / 2)
        all[#all + 1] = state(frag, mid, s3.e, fitStatement(ctx, frag, mid, s3.e, f1 + 1, (s3.e - mid) / 2), (s3.e - mid) / 2)
      elseif p.how == "inversion" then
        local inv = { steps = {}, durs = m.durs }
        for i, s in ipairs(m.steps) do inv.steps[i] = -s end
        all[#all + 1] = state(inv, s2.s, s2.e, fitStatement(ctx, inv, s2.s, s2.e, base0 + 2))
        all[#all + 1] = state(m, s3.s, s3.e, fitStatement(ctx, m, s3.s, s3.e, base0))
      else
        all[#all + 1] = state(m, s2.s, s3.e, fitStatement(ctx, m, s2.s, s3.e, base0, 2), 2)
      end
      -- The cadence bar: home at the end, or before the last group the
      -- fifth - a half cadence that asks for more.
      all[#all + 1] = cadence(ctx, s4.s, s4.e, base0 + m.steps[1], not last)
    end
    local notes = holdLast(ctx, concat(all))
    return toNotes(finishMelody(ctx, notes, true))
  end,
}

-------------------------------------------------------------------- Harmony

-- Comping rhythms for chords: { cycle, hits } like the rhythm cells, plus
-- "held", which is one strike a chord with common tones tied over.
local COMPS = {
  held    = { label = "held" },
  half    = { label = "half notes",  cycle = 2, hits = { { 0, 1.9, true } } },
  quarter = { label = "quarters",    cycle = 1, hits = { { 0, 0.9, true } } },
  stabs   = { label = "off-beat stabs", cycle = 1, hits = { { 0.5, 0.25, true } } },
  charleston = { label = "Charleston", cycle = 4, hits = { { 0, 1, true }, { 1.5, 0.5, true } } },
}

-- The voices of a harmony block, from voicings (one per slot) and a comp.
-- Held voices tie a common tone over into the next chord instead of striking
-- it again, which is what keeping a common tone sounds like.
local function comp(ctx, slots, voicings, compId, skipVoice)
  local nv = #voicings[1]
  local voices = {}
  for v = 1, nv do voices[v] = {} end
  local c = COMPS[compId]
  for i, slot in ipairs(slots) do
    local V = voicings[i]
    for v = 1, nv do
      if v ~= skipVoice then
        if compId == "held" then
          local list = voices[v]
          local prev = list[#list]
          if prev and prev.pitch == V[v] and math.abs(prev.start + prev.full - slot.start) < EPS then
            prev.full = prev.full + slot.len
            prev.len = prev.full * 0.98
          else
            local n = { start = slot.start, len = slot.len * 0.98, full = slot.len, pitch = V[v],
                        accent = onDownbeat(ctx, slot.start), voice = v }
            list[#list + 1] = n
          end
        else
          -- Accented on the downbeats, so accents give a comp its pulse.
          for _, h in ipairs(tile(c, slot.start, slot.start + slot.len)) do
            add(voices[v], h.t, h.len, V[v], onDownbeat(ctx, h.t), v)
          end
        end
      end
    end
  end
  return voices
end

-- How the voices of a harmony type are laid out for this instrument.
--   mono      a melodic instrument: a four-part chorale, and it plays its own voice
--   poly      a chordal instrument: the chords, in its register
--   ensemble  a section: one voice to each part
-- `uniform` gives every voice the whole band, for clusters, whose voices
-- have to sit a step apart and cannot be kept in four separate ranges.
local function harmonySetup(ctx, p, uniform)
  local inst = ctx.inst
  if ctx.ensemble then
    local ranges = {}
    for v = 1, 4 do
      for _, part in ipairs(ctx.ensemble.parts) do
        if part.voice == v and not part.octave then
          local pi = O.byId(part.inst)
          if uniform then ranges[v] = { pi.low, pi.high }
          else local a, b = O.partBand(part, ctx.register); ranges[v] = { a, b } end
          break
        end
      end
    end
    return { mode = "ensemble", nv = 4, ranges = ranges }
  end
  if inst.poly == 1 then
    local ranges, take = O.satbFor(inst, ctx.register)
    if uniform then
      -- The shared band, widened by a fifth each way (inside the instrument):
      -- four stacked fourths span more than the sweet register alone.
      ranges = {}
      for v = 1, 4 do ranges[v] = { math.max(inst.low, ctx.lo - 7), math.min(inst.high, ctx.hi + 7) } end
    end
    return { mode = "mono", nv = 4, ranges = ranges, take = take }
  end
  local nv = math.min(p.voices or 4, inst.poly)
  local ranges
  if uniform then
    ranges = {}
    for v = 1, nv do ranges[v] = { ctx.lo, ctx.hi } end
  else
    ranges = O.chordRanges(inst, ctx.register, nv)
  end
  return { mode = "poly", nv = nv, ranges = ranges }
end
M._harmonySetup = harmonySetup

-- The chords of a list of slots: the ones chosen, or with `kind` a shape the
-- type builds for itself (stacked fourths, a cluster) on each chord's degree,
-- from the scale as it sounds under that chord.
local function slotChords(ctx, slots, kind)
  local out = {}
  for i, s in ipairs(slots) do
    if kind then
      local ch = T.chord(s.key, s.degree, kind)
      ch.key = s.key
      out[i] = ch
    else
      out[i] = s.chord
    end
  end
  return out
end

-- Harmony params: the voice count only matters to an instrument that plays
-- the chords itself, so only there is it a choice.
local function harmonyParams(ctx, dims, describe)
  local lists = {}
  if not ctx.ensemble and ctx.inst.poly > 1 then
    local vs = {}
    for _, n in ipairs({ 3, 4 }) do if n <= ctx.inst.poly then vs[#vs + 1] = n end end
    if #vs == 0 then vs = { ctx.inst.poly } end
    lists[#lists + 1] = { name = "voices", values = vs }
  end
  for _, d in ipairs(dims) do lists[#lists + 1] = d end
  local out = {}
  for _, c in ipairs(cross(lists)) do
    local idParts, labelParts = {}, {}
    if c.voices then idParts[#idParts + 1] = c.voices .. "v"; labelParts[#labelParts + 1] = c.voices .. " voices" end
    for _, d in ipairs(dims) do
      local v = c[d.name]
      idParts[#idParts + 1] = v.id
      labelParts[#labelParts + 1] = v.label
    end
    c.id = table.concat(idParts, "@")
    c.label = table.concat(labelParts, ", ")
    c.hint = describe(c)
    out[#out + 1] = c
  end
  return out
end

local function compDim(ids)
  local vals = {}
  for _, id in ipairs(ids) do vals[#vals + 1] = { id = id, label = COMPS[id].label } end
  return { name = "comp", values = vals }
end

local function voicesText(ctx, c)
  if ctx.ensemble then return "spread one voice to each part of the " .. ctx.ensemble.name:lower() end
  if ctx.inst.poly == 1 then return "the " .. ctx.inst.name .. "'s own voice of a four-part chorale" end
  return (c.voices or 4) .. " voices"
end

TYPES.Triadic = {
  harmony = true,
  params = function(ctx)
    return harmonyParams(ctx, { compDim({ "held", "half", "quarter", "stabs", "charleston" }) }, function(c)
      return ("Chords stacked in thirds, %s, voice-led: common tones held, every voice moving "
        .. "as little as it can, no parallel fifths or octaves. Played %s."):format(voicesText(ctx, c), c.comp.label)
    end)
  end,
  build = function(ctx, p, hv)
    local v = lead(ctx, slotChords(ctx, ctx.slots), hv.ranges, { bassRoot = true })
    if not v then return nil, "no voicing fits" end
    return comp(ctx, ctx.slots, v, p.comp.id)
  end,
}

-- Stacked fourths sit close, like clusters, so their voices share the band
-- rather than each keeping to a range of its own.
TYPES.Quartal = {
  harmony = true, uniform = true,
  params = function(ctx)
    return harmonyParams(ctx, {
      { name = "motion", values = { { id = "planing", label = "planing" }, { id = "led", label = "voice-led" } } },
      compDim({ "held", "half", "stabs" }),
    }, function(c)
      return ("Chords stacked in fourths from the scale, %s, %s. Played %s.")
        :format(voicesText(ctx, c),
          c.motion.id == "planing" and "the whole shape sliding up and down the scale in parallel"
            or "each voice moving to the nearest note of the next stack", c.comp.label)
    end)
  end,
  build = function(ctx, p, hv)
    local kind = (hv.nv >= 4) and "quartal4" or "quartal"
    local chords = slotChords(ctx, ctx.slots, kind)
    local opts = { bassRoot = true, adjacent = { [5] = true, [6] = true, [7] = true }, noParallels = false }
    local v
    if p.motion.id == "planing" then
      local first = lead(ctx, { chords[1] }, hv.ranges, opts)
      if not first then return nil, "no voicing fits" end
      v = T.plane(ctx.key, first[1], chords)
      for _, V in ipairs(v) do
        for k, pch in ipairs(V) do
          if pch < hv.ranges[k][1] - 5 or pch > hv.ranges[k][2] + 5 then return nil, "the planing runs out of range" end
        end
      end
    else
      v = lead(ctx, chords, hv.ranges, opts)
      if not v then return nil, "no voicing fits" end
    end
    return comp(ctx, ctx.slots, v, p.comp.id)
  end,
}

TYPES.Cluster = {
  harmony = true, uniform = true,
  params = function(ctx)
    return harmonyParams(ctx, { compDim({ "held", "half", "quarter" }) }, function(c)
      return ("Neighbouring scale notes sounded together, %s, each cluster moving to the nearest "
        .. "one on the next chord. Played %s."):format(voicesText(ctx, c), c.comp.label)
    end)
  end,
  build = function(ctx, p, hv)
    local kind = (hv.nv >= 4) and "cluster4" or "cluster"
    local opts = { bassRoot = false, adjacent = { [1] = true, [2] = true, [3] = true }, noParallels = false }
    local v = lead(ctx, slotChords(ctx, ctx.slots, kind), hv.ranges, opts)
    if not v then return nil, "no voicing fits" end
    return comp(ctx, ctx.slots, v, p.comp.id)
  end,
}

TYPES.Pedal = {
  harmony = true,
  params = function(ctx)
    return harmonyParams(ctx, {
      { name = "note", values = { { id = "tonic", label = "tonic pedal" }, { id = "dominant", label = "dominant pedal" } } },
      { name = "where", values = { { id = "bass", label = "in the bass" }, { id = "top", label = "on top" } } },
      { name = "pulse", values = { { id = "held", label = "held" }, { id = "8ths", label = "repeated 1/8" },
                                   { id = "4ths", label = "repeated 1/4" } } },
    }, function(c)
      local part = ""
      if not ctx.ensemble and ctx.inst.poly == 1 then
        part = " The " .. ctx.inst.name .. " plays whichever line of it is its own: the pedal, "
          .. "or its voice of the chords moving against it."
      end
      return ("One note held %s - the %s - while the chords change %s it, voice-led. The pedal is %s.%s")
        :format(c.where.id == "bass" and "in the bass" or "on top", c.note.id,
                c.where.id == "bass" and "above" or "under", c.pulse.label, part)
    end)
  end,
  build = function(ctx, p, hv)
    local nv = hv.nv
    local pc = T.rootPc(ctx.key)
    if p.note.id == "dominant" then pc = (pc + 7) % 12 end
    local idx = (p.where.id == "bass") and 1 or nv
    local r = hv.ranges[idx]
    local mid = (p.where.id == "bass") and (r[1] + 4) or (r[2] - 4)
    local pedal
    for q = r[1] - 6, r[2] + 6 do
      if q % 12 == pc and (not pedal or math.abs(q - mid) < math.abs(pedal - mid)) then pedal = q end
    end
    local opts = { bassRoot = true }
    if p.where.id == "bass" then opts.fixedBass = pedal else opts.fixedTop = pedal end
    local v = lead(ctx, slotChords(ctx, ctx.slots), hv.ranges, opts)
    if not v then return nil, "no voicing fits over the pedal" end
    local voices = comp(ctx, ctx.slots, v, "held", idx)
    local line = {}
    if p.pulse.id == "held" then
      add(line, 0, ctx.L * 0.99, pedal, true, idx)
    else
      local step = (p.pulse.id == "8ths") and 0.5 or 1
      local count = math.floor(ctx.L / step + EPS)
      for i = 0, count - 1 do
        local t = i * step
        add(line, t, step * 0.8, pedal, math.abs(t - math.floor(t + 0.5)) < EPS and (t % 2 < EPS), idx)
      end
    end
    voices[idx] = line
    return voices
  end,
}

local ARPEGGIATED = {
  { id = "rolled",  label = "rolled", hint = "Each chord rolled from the bottom up, a thirty-second apart, and held." },
  { id = "stagger8", label = "staggered 1/8", step = 0.5, stagger = true,
    hint = "The voices come in one by one from the bottom, an eighth apart, and hold." },
  { id = "stagger4", label = "staggered 1/4", step = 1, stagger = true,
    hint = "The voices come in one by one from the bottom, a quarter apart, and hold." },
  { id = "rise8",   label = "rising 1/8",  step = 0.5, pattern = "rise",
    hint = "The voice-led chord broken from the bottom voice to the top, over and over, in eighths." },
  { id = "rise16",  label = "rising 1/16", step = 0.25, pattern = "rise",
    hint = "The voice-led chord broken from the bottom voice to the top, over and over, in sixteenths." },
  { id = "wave8",   label = "rise & fall 1/8", step = 0.5, pattern = "wave",
    hint = "Up through the voices and back down, in eighths." },
  { id = "wave16",  label = "rise & fall 1/16", step = 0.25, pattern = "wave",
    hint = "Up through the voices and back down, in sixteenths." },
}

TYPES.Arpeggiated = {
  harmony = true,
  params = function(ctx)
    local vals = {}
    for _, a in ipairs(ARPEGGIATED) do vals[#vals + 1] = a end
    return harmonyParams(ctx, { { name = "arp", values = vals } }, function(c)
      return c.arp.hint .. " The chords are " .. voicesText(ctx, c) .. ", voice-led."
    end)
  end,
  build = function(ctx, p, hv)
    local v = lead(ctx, slotChords(ctx, ctx.slots), hv.ranges, { bassRoot = true })
    if not v then return nil, "no voicing fits" end
    local nv = hv.nv
    local voices = {}
    for k = 1, nv do voices[k] = {} end
    local a = p.arp
    for i, slot in ipairs(ctx.slots) do
      local V = v[i]
      local e = slot.start + slot.len
      if a.id == "rolled" or a.stagger then
        local gap = a.stagger and a.step or 0.125
        for k = 1, nv do
          local t = slot.start + (k - 1) * gap
          if t < e - EPS then add(voices[k], t, (e - t) * 0.97, V[k], k == 1, k) end
        end
      else
        local order = {}
        for k = 1, nv do order[#order + 1] = k end
        if a.pattern == "wave" then for k = nv - 1, 2, -1 do order[#order + 1] = k end end
        local count = math.floor(slot.len / a.step + EPS)
        for j = 0, count - 1 do
          local k = order[(j % #order) + 1]
          add(voices[k], slot.start + j * a.step, a.step * 0.9, V[k], j % #order == 0, k)
        end
      end
    end
    return voices
  end,
}

TYPES["Contrary motion"] = {
  harmony = true,
  params = function(ctx)
    return harmonyParams(ctx, {
      { name = "dir", values = { { id = "up", label = "top rises, bass falls" }, { id = "down", label = "top falls, bass rises" } } },
      compDim({ "held", "half", "quarter" }),
    }, function(c)
      return ("The outer voices move in opposite directions - the %s - using inversions where they "
        .. "need them, %s. Played %s."):format(c.dir.label, voicesText(ctx, c), c.comp.label)
    end)
  end,
  build = function(ctx, p, hv)
    -- A single chord has nowhere to move, so the block steps through it
    -- in four, the outer voices opening or closing through its inversions.
    local slots = {}
    local per = math.max(1, math.ceil(4 / #ctx.slots))
    for _, s in ipairs(ctx.slots) do
      for j = 0, per - 1 do
        slots[#slots + 1] = { i = #slots + 1, start = s.start + j * s.len / per, len = s.len / per,
                              degree = s.degree, chord = s.chord, key = s.key }
      end
    end
    local opts = { bassRoot = false, contrary = (p.dir.id == "up") and 1 or -1 }
    local v = lead(ctx, slotChords(ctx, slots), hv.ranges, opts)
    if not v then return nil, "no voicing fits" end
    return comp(ctx, slots, v, p.comp.id)
  end,
}

------------------------------------------------------------------------------
-- Checks
--
-- What a player would refuse. Each returns a reason, or nil.
------------------------------------------------------------------------------

local function sortNotes(list)
  table.sort(list, function(a, b)
    if math.abs(a.start - b.start) > EPS then return a.start < b.start end
    return a.pitch < b.pitch
  end)
  return list
end

-- A melodic instrument plays one note at a time: nothing overlaps, and two
-- notes at once become the higher.
local function monophonic(list)
  sortNotes(list)
  local out = {}
  for _, n in ipairs(list) do
    local prev = out[#out]
    if prev and math.abs(prev.start - n.start) < EPS then
      if n.pitch > prev.pitch then out[#out] = n end
    else
      if prev and prev.start + prev.len > n.start - EPS then prev.len = math.max(0.03, n.start - prev.start - 0.01) end
      out[#out + 1] = n
    end
  end
  return out
end

-- Wind and brass breathe. Every two bars, a note sounding across the bar line
-- stops a sixteenth short of it and, if it was held across and still fits the
-- chord, comes back in after; a short note in the way of the breath is left
-- out.
function M.breathe(ctx, list, L)
  L = L or ctx.L
  local phrase = 2 * ctx.barBeats
  local k = 1
  while k * phrase < L - EPS do
    local B = k * phrase
    local out = {}
    for _, n in ipairs(list) do
      local e = n.start + n.len
      if n.start < B - EPS and e > B - 0.25 + EPS then
        local cut = B - 0.25
        if cut - n.start >= 0.125 - EPS then
          out[#out + 1] = { start = n.start, len = cut - n.start, pitch = n.pitch, accent = n.accent,
                            voice = n.voice, ct = n.ct }
        end
        -- The note comes back after the breath only if it belongs to the
        -- chord there; otherwise the breath simply lasts until the next note.
        if e > B + 0.25 + EPS and T.hasPc(M.slotAt(ctx, B).chord, n.pitch % 12) then
          out[#out + 1] = { start = B, len = e - B, pitch = n.pitch, accent = n.accent, voice = n.voice, ct = n.ct }
        end
      else
        out[#out + 1] = n
      end
    end
    list = out
    k = k + 1
  end
  return sortNotes(list)
end

-- Shifts a part by whole octaves into its instrument, as near the middle of
-- the band as it will go. Nil if no octave holds it all. `keep` leaves a part
-- that already fits where it is - a voice of an ensemble, or a transformed
-- entry, has its register for a reason.
local function fitPart(list, inst, centre, keep)
  if #list == 0 then return list end
  local lo, hi, sum = 999, -1, 0
  for _, n in ipairs(list) do lo, hi, sum = math.min(lo, n.pitch), math.max(hi, n.pitch), sum + n.pitch end
  if keep and lo >= inst.low and hi <= inst.high then return list end
  local mean = sum / #list
  -- Shifts are tried nearest first, and one has to be clearly better to win:
  -- material that sits as well where it was written stays there.
  local best, bestD
  for _, k in ipairs({ 0, -1, 1, -2, 2, -3, 3, -4, 4 }) do
    local s = 12 * k
    if lo + s >= inst.low and hi + s <= inst.high then
      local d = math.abs(mean + s - centre)
      if not bestD or d < bestD - 1 then best, bestD = s, d end
    end
  end
  if not best then return nil end
  if best ~= 0 then for _, n in ipairs(list) do n.pitch = n.pitch + best end end
  return list
end

-- The shortest gap between two onsets in the same voice.
local function shortestGap(list)
  local byVoice = {}
  for _, n in ipairs(list) do
    local v = n.voice or 0
    byVoice[v] = byVoice[v] or {}
    local t = byVoice[v]
    t[#t + 1] = n.start
  end
  local best = math.huge
  for _, times in pairs(byVoice) do
    table.sort(times)
    for i = 2, #times do
      local d = times[i] - times[i - 1]
      if d > EPS and d < best then best = d end
    end
  end
  return best
end
M.shortestGap = shortestGap

local function tooQuick(ctx, inst, list)
  return shortestGap(list) < M.fastBeats(ctx, inst) - 1e-4
end

function M.fastBeats(ctx, inst) return O.fastBeats(inst, ctx.bpm) end

-- The widest leap between two notes less than a beat apart.
local function widestQuickLeap(list)
  local w = 0
  for i = 2, #list do
    if list[i].start - list[i - 1].start < 1 - EPS then
      w = math.max(w, math.abs(list[i].pitch - list[i - 1].pitch))
    end
  end
  return w
end
M.widestQuickLeap = widestQuickLeap

------------------------------------------------------------------------------
-- Making an entry
------------------------------------------------------------------------------

local function markChordTones(ctx, list)
  for _, n in ipairs(list) do
    n.ct = T.hasPc(M.slotAt(ctx, n.start).chord, n.pitch % 12)
  end
end

-- One entry: the parts it makes, or nil and why not.
-- A part is { name, inst, notes }.
function M.make(ctx, typeName, p)
  local typ = TYPES[typeName]
  local inst = ctx.inst
  local parts = {}

  if typ.harmony then
    local hv = harmonySetup(ctx, p, typ.uniform)
    local voices, why = typ.build(ctx, p, hv)
    if not voices then return nil, why end
    if hv.mode == "ensemble" then
      for _, part in ipairs(ctx.ensemble.parts) do
        local pi = O.byId(part.inst)
        local list = {}
        for _, n in ipairs(voices[part.voice] or {}) do
          list[#list + 1] = { start = n.start, len = n.len, pitch = n.pitch + 12 * (part.octave or 0),
                              accent = n.accent }
        end
        local _, _, c = O.partBand(part, ctx.register)
        list = fitPart(monophonic(list), pi, c + 12 * (part.octave or 0), true)
        if not list then return nil, "goes outside the " .. part.name .. "'s range" end
        parts[#parts + 1] = { name = part.name, inst = pi, notes = list }
      end
    elseif hv.mode == "mono" then
      local list = {}
      for _, n in ipairs(voices[hv.take] or {}) do list[#list + 1] = n end
      parts[1] = { name = inst.name, inst = inst, notes = monophonic(list) }
    else
      local list = {}
      for _, vl in ipairs(voices) do for _, n in ipairs(vl) do list[#list + 1] = n end end
      parts[1] = { name = inst.name, inst = inst, notes = sortNotes(list) }
    end
  else
    local list, why = typ.build(ctx, p)
    if not list then return nil, why end
    if inst.poly == 1 then list = monophonic(list) else sortNotes(list) end
    local fitted = fitPart(list, inst, ctx.centre)
    if not fitted then return nil, "goes outside the " .. inst.name .. "'s range" end
    parts[1] = { name = inst.name, inst = inst, notes = fitted }
  end

  local total = 0
  for _, part in ipairs(parts) do
    local pi = part.inst
    if #part.notes == 0 then return nil, "has nothing to play on these chords" end
    for _, n in ipairs(part.notes) do
      if n.pitch < pi.low or n.pitch > pi.high then
        return nil, "goes outside the " .. part.name .. "'s range"
      end
    end
    if pi.breath then part.notes = M.breathe(ctx, part.notes) end
    if tooQuick(ctx, pi, part.notes) then
      return nil, ("too quick for the %s at %d bpm"):format(part.name, math.floor(ctx.bpm + 0.5))
    end
    if pi.poly == 1 and widestQuickLeap(part.notes) > pi.leap then
      return nil, "leaps wider than the " .. part.name .. " takes in passing"
    end
    markChordTones(ctx, part.notes)
    total = total + #part.notes
  end
  if total > M.MAX_NOTES then return nil, "more notes than a block holds" end
  return parts
end

-- How many notes start in each beat, counting a chord as one.
function M.density(parts, beats)
  local starts = {}
  for _, part in ipairs(parts) do
    for _, n in ipairs(part.notes) do starts[math.floor(n.start * 1000 + 0.5)] = true end
  end
  local c = 0
  for _ in pairs(starts) do c = c + 1 end
  return c / math.max(beats, EPS)
end

function M.densityBand(d)
  if d < M.SPARSE_BELOW then return "Sparse" end
  if d >= M.BUSY_FROM then return "Busy" end
  return "Medium"
end

local function signature(parts)
  local out = {}
  for _, part in ipairs(parts) do
    for _, n in ipairs(part.notes) do
      out[#out + 1] = ("%s:%.3f:%.3f:%d"):format(part.name, n.start, n.len, n.pitch)
    end
  end
  return table.concat(out, ";")
end

-- The catalogue for one type: every combination, made and checked. Entries
-- that come out identical to an earlier one are the same entry and are
-- listed once. Returns { entries, hidden }, where hidden is a list of
-- { reason, count }, most common first.
function M.catalogue(ctx, typeName)
  local typ = TYPES[typeName]
  local out = { entries = {}, hidden = {}, avoided = nil }
  if not typ then return out end
  if not ctx.ensemble then
    local why = O.avoids(ctx.inst, M.categoryOf(typeName), typeName)
    if why then out.avoided = why; return out end
  end
  local reasons, seen = {}, {}
  for _, p in ipairs(typ.params(ctx)) do
    local parts, why = M.make(ctx, typeName, p)
    if parts then
      local sig = signature(parts)
      if not seen[sig] then
        seen[sig] = true
        local d = M.density(parts, ctx.L)
        out.entries[#out.entries + 1] = { id = p.id, label = p.label, hint = p.hint, parts = parts,
                                          density = d, band = M.densityBand(d), type = typeName }
      end
    else
      reasons[why] = (reasons[why] or 0) + 1
    end
  end
  for r, c in pairs(reasons) do out.hidden[#out.hidden + 1] = { reason = r, count = c } end
  table.sort(out.hidden, function(a, b)
    if a.count ~= b.count then return a.count > b.count end
    return a.reason < b.reason
  end)
  return out
end

------------------------------------------------------------------------------
-- Shaping an entry
------------------------------------------------------------------------------

local function copyParts(parts)
  local out = {}
  for i, part in ipairs(parts) do
    local notes = {}
    for k, n in ipairs(part.notes) do
      notes[k] = { start = n.start, len = n.len, pitch = n.pitch, accent = n.accent, voice = n.voice, ct = n.ct }
    end
    out[i] = { name = part.name, inst = part.inst, notes = notes }
  end
  return out
end

-- The transformations keep the harmony: a note that was a chord tone lands
-- on a chord tone of whatever chord is under it afterwards. Inversion mirrors
-- the music diatonically about its first note (or, for more than one voice,
-- about the middle of it); retrograde plays it backwards. Augmentation and
-- diminution stretch the chords with it, so nothing needs refitting: it is
-- the same music at half or double speed.
--
-- Returns the parts, how many beats they now last, and a warning or nil.
function M.transform(ctx, parts, kind)
  parts = copyParts(parts)
  local L = ctx.L
  if kind == "Original" or not kind then return parts, L end

  if kind == "Augmentation" or kind == "Diminution" then
    local f = (kind == "Augmentation") and 2 or 0.5
    for _, part in ipairs(parts) do
      for _, n in ipairs(part.notes) do n.start, n.len = n.start * f, n.len * f end
    end
    L = L * f
  else
    local invert = (kind == "Inversion" or kind == "Retrograde inversion")
    local retro  = (kind == "Retrograde" or kind == "Retrograde inversion")
    local axis
    if invert then
      local all = {}
      for _, part in ipairs(parts) do for _, n in ipairs(part.notes) do all[#all + 1] = n end end
      if #parts == 1 and parts[1].inst.poly == 1 and #all > 0 then
        axis = T.nearestPos(keyAt(ctx, all[1].start), all[1].pitch)
      else
        local sum = 0
        for _, n in ipairs(all) do sum = sum + T.nearestPos(keyAt(ctx, n.start), n.pitch) end
        axis = math.floor(sum / math.max(1, #all) + 0.5)
      end
    end
    for _, part in ipairs(parts) do
      for _, n in ipairs(part.notes) do
        n.origPitch = n.pitch
        -- A position is read in the scale under the note where it was, and
        -- turned back into a pitch in the scale under it where it lands.
        local pos = T.nearestPos(keyAt(ctx, n.start), n.pitch)
        if invert then pos = 2 * axis - pos end
        if retro then n.start = L - n.start - n.len end
        if n.ct then
          local lean = invert and -1 or 1
          pos = M.nearestChordPos(ctx, M.slotAt(ctx, n.start).chord, pos, lean)
        end
        n.pitch = T.pitch(keyAt(ctx, n.start), pos)
        if ctx.drums then
          -- On the timpani an inversion swaps the two drums, and whatever
          -- lands under a chord that lacks it takes that chord's own drum.
          local d = ctx.drums
          local orig = (math.abs(n.pitch - d.tonic) <= math.abs(n.pitch - d.dominant)) and d.tonic or d.dominant
          local cand = orig
          if invert then cand = (n.origPitch == d.tonic) and d.dominant or d.tonic end
          local main = slotDrum(ctx, M.slotAt(ctx, n.start))
          if n.ct and main and not T.hasPc(M.slotAt(ctx, n.start).chord, cand % 12) then cand = main end
          n.pitch = cand
        end
      end
      -- Two voices mirrored onto the same note at the same time are one note.
      local seen, kept = {}, {}
      for _, n in ipairs(sortNotes(part.notes)) do
        local k = ("%.4f:%d"):format(n.start, n.pitch)
        if not seen[k] then seen[k] = true; kept[#kept + 1] = n end
      end
      part.notes = kept
      local _, _, c = O.band(part.inst, ctx.register)
      part.notes = fitPart(part.notes, part.inst, c, true) or part.notes
      if part.inst.poly == 1 then part.notes = monophonic(part.notes) end
    end
  end

  local warning
  for _, part in ipairs(parts) do
    if tooQuick(ctx, part.inst, part.notes) then
      warning = ("Faster than the %s plays cleanly at %d bpm"):format(part.name, math.floor(ctx.bpm + 0.5))
    end
  end
  return parts, L, warning
end

-- Plays the parts `times` times over.
function M.repeatParts(parts, beats, times)
  if times <= 1 then return parts end
  local out = {}
  for i, part in ipairs(parts) do
    local notes = {}
    for k = 0, times - 1 do
      for _, n in ipairs(part.notes) do
        notes[#notes + 1] = { start = n.start + k * beats, len = n.len, pitch = n.pitch,
                              accent = n.accent, voice = n.voice, ct = n.ct }
      end
    end
    out[i] = { name = part.name, inst = part.inst, notes = notes }
  end
  return out
end

-- Velocity: 100 for everything, unless accents are asked for.
function M.applyVelocity(parts, mode)
  for _, part in ipairs(parts) do
    for _, n in ipairs(part.notes) do
      n.vel = (mode == "Accents" and n.accent) and M.ACCENT or M.VELOCITY
    end
  end
  return parts
end

------------------------------------------------------------------------------
-- The block
------------------------------------------------------------------------------

function M.blockName(ctx, typeName, entry, transform, times)
  local who = ctx.ensemble and ctx.ensemble.name or ctx.inst.name
  local name = ("%s %s %s %s %s %s"):format(T.ROOTS[ctx.key.root].name, T.SCALES[ctx.key.scale].name,
    T.chainName(ctx.key, ctx.chain, true), who, typeName, entry.label)
  if transform and transform ~= "Original" then name = name .. " " .. transform:lower() end
  if times and times > 1 then name = name .. " x" .. times end
  return (name:gsub("%s+", " "))
end

-- The finished thing: an entry shaped and ready to go into the project.
-- `sel` is { transform, repeats, velocity }.
function M.block(ctx, typeName, entry, sel)
  sel = sel or {}
  local parts, beats, warning = M.transform(ctx, entry.parts, sel.transform)
  local times = sel.repeats or 1
  parts = M.repeatParts(parts, beats, times)
  M.applyVelocity(parts, sel.velocity)
  local notes = {}
  for pi, part in ipairs(parts) do
    for _, n in ipairs(part.notes) do
      notes[#notes + 1] = { start = n.start, len = n.len, pitch = n.pitch, vel = n.vel, part = pi }
    end
  end
  sortNotes(notes)
  return {
    name = M.blockName(ctx, typeName, entry, sel.transform, times),
    beats = beats * times,
    parts = parts,
    notes = notes,
    warning = warning,
  }
end

return M
