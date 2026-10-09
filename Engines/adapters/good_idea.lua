--[[ Good Idea, in Noterator.

     Every setting is Good Idea's own (gi_idea.lua's SETTINGS), shown the way
     its window shows them, with "Any" where the engine can decide. One result
     is one idea number: asking for six gives six neighbouring ideas, and the
     same number with the same settings always gives the same idea back. ]]

local C = engine("adapters/common.lua")
local T = engine("good-idea/gi_theory.lua")
local I = engine("good-idea/gi_idea.lua").init(T)

local A = {
  id = "good-idea",
  name = "Generate Notes",   -- Good Idea, named for what it does in the app (decision 0025)
  description = "A motif, a phrase, eight to sixteen bars of melody, chords and bass, or a drum groove - calculated from the rules that make music sound like music.",
  input = "none",
  panel = "generate",
}

local LIST = {}
for _, s in ipairs(I.SETTINGS) do
  if not s.hidden then
    local values, names, hints = {}, {}, {}
    if s.any then
      values[1], names[1], hints[1] = "Any", "Any", "Let each idea decide."
    end
    for _, v in ipairs(s.values) do
      values[#values + 1] = v
      names[#names + 1] = I.valueName(s, v)
      hints[#hints + 1] = (s.hints and s.hints[v]) or ""
    end
    LIST[#LIST + 1] = C.setting(s.id, s.label, values, names, {
      hints = hints, group = s.step,
      when = function(st) return I.shows(s, st) end,
    })
  end
end

function A.newState() return I.newState() end
function A.settings(st, ctx) return C.describe(LIST, st, ctx) end
function A.set(st, id, index, ctx)
  C.set(LIST, st, id, index, ctx)
  I.clampState(st)
end

function A.useKey(st, root, scale)
  st.root, st.scale = root, scale
  I.clampState(st)
end

-- With bars selected to generate into, a length left on Any becomes the one
-- that fits them best: the longest that fits, else the shortest there is. The
-- app then repeats or trims the idea to the selection exactly.
local BARS_SETTING = { Motif = "motifBars", Phrase = "phraseBars", Measure = "measureBars", Drums = "drumBars" }

local function fitBars(st, ctx)
  local id = BARS_SETTING[st.kind]
  if not id or not ctx.rangeBars or ctx.rangeBars <= 0 or st[id] ~= "Any" then return nil end
  local best, shortest
  for _, v in ipairs(I.BY_ID[id].values) do
    if v <= ctx.rangeBars and (not best or v > best) then best = v end
    if not shortest or v < shortest then shortest = v end
  end
  st[id] = best or shortest
  return id
end

function A.generate(st, ctx, seed, count)
  local fitted = fitBars(st, ctx)
  local ok, out = pcall(A.make, st, ctx, seed, count)
  if fitted then st[fitted] = "Any" end
  if not ok then error(out, 0) end
  return out
end

function A.make(st, ctx, seed, count)
  local meter = I.meter(ctx.num, ctx.den)
  local out = {}
  for k = 0, (count or 6) - 1 do
    local s = (seed - 1 + k) % I.MAX_SEED + 1
    local idea = I.make(st, meter, s)
    local parts = {}
    for _, p in ipairs(idea.block.parts) do
      parts[#parts + 1] = { name = p.name, drums = p.drums and true or false, notes = C.notes(p.notes) }
    end
    local detail = idea.summary or ""
    if idea.chords and idea.chords ~= "" then detail = detail .. "\n" .. idea.chords end
    -- "Good Idea 63797 - Phrase (melody), 4 bars, C Major": the engine's
    -- name and seed are the REAPER file name; the list wants the rest.
    local title = idea.block.name:gsub("^Good Idea %d+ %- ", "")
    out[#out + 1] = { title = title, detail = detail,
                      beats = idea.block.beats, parts = parts }
  end
  return { results = out }
end

return A
