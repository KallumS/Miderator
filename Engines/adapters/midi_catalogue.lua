--[[ Midi Catalogue, in Noterator.

     The catalogue is written for one instrument, and in Noterator that
     instrument is the part the music is going into: a violin part gets the
     catalogue for Violin I, a viola part the viola's, so the same settings
     give a different line for each (Noterator decision 0006). A section
     (strings, woodwinds, brass, four horns) writes several parts at once,
     each on its own instrument.

     Results are the catalogue's entries for the chosen type, filtered by
     density, shaped by the transformation, repeats and velocity, as the
     Catalogue's own window does. ]]

local C = engine("adapters/common.lua")
local T = engine("midi-catalogue/mc_theory.lua")
local O = engine("midi-catalogue/mc_orchestra.lua")
local Cat = engine("midi-catalogue/mc_catalogue.lua").init(T, O)

local A = {
  id = "midi-catalogue",
  name = "Midi Catalogue",
  description = "Rhythms, melodies and harmony written for this part's instrument, in the key, over a chain of chords. Nothing the instrument cannot play is offered.",
  input = "none",
}

local MAX_RESULTS = 48

local function catNames()
  local out = {}
  for _, c in ipairs(Cat.CATEGORIES) do out[#out + 1] = c.name end
  return out
end

local function typesFor(st)
  for _, c in ipairs(Cat.CATEGORIES) do if c.name == st.cat then return c.types end end
  return Cat.CATEGORIES[1].types
end

local progIds = {}
for _, p in ipairs(T.PROGRESSIONS) do progIds[#progIds + 1] = p.id end

local sectionIds, sectionNames = { "" }, { "This part's instrument" }
for _, e in ipairs(O.ENSEMBLES) do
  sectionIds[#sectionIds + 1] = e.id
  sectionNames[#sectionNames + 1] = e.name .. " section"
end

local LIST = {
  C.setting("prog", "Chords", progIds, nil, {
    group = "Harmony",
    hint = "The chain of chords the music is written over, one chord per equal share of the length.",
    dynamic = function(st)
      local key = T.key(st.root, st.scale)
      local values, names = {}, {}
      for _, p in ipairs(T.progressionsFor(key)) do
        values[#values + 1] = p.id
        names[#names + 1] = T.progressionName(key, p)
      end
      return values, names, {}
    end }),
  C.setting("bars", "Bars", Cat.LENGTHS, nil, { group = "Harmony" }),
  C.setting("section", "Write for", sectionIds, sectionNames, {
    group = "Who",
    hint = "A section writes harmony across several parts at once, each in its own range." }),
  C.setting("register", "Register", O.REGISTERS, nil, { group = "Who",
    hint = "Where in the instrument's sweet register the music sits." }),
  C.setting("cat", "Kind", catNames(), nil, { group = "What",
    when = function(st) return st.section == "" end }),
  C.setting("type", "Type", Cat.CATEGORIES[1].types, nil, {
    group = "What",
    dynamic = function(st) local t = typesFor(st); return t, t, {} end }),
  C.setting("density", "Density", Cat.DENSITIES, nil, { group = "What" }),
  C.setting("transform", "Transform", Cat.TRANSFORMS, nil, { group = "Shape",
    hint = "Inversion and retrograde keep the harmony: a chord tone lands on a chord tone." }),
  C.setting("repeats", "Repeats", Cat.REPEATS, { "Once", "Twice", "Four times" }, { group = "Shape" }),
  C.setting("velocity", "Velocity", Cat.VELOCITIES, nil, { group = "Shape" }),
}

function A.newState()
  local st = Cat.newState()
  st.prog = "I-V-vi-IV"
  return st
end

local function sync(st, ctx)
  st.inst = (ctx and ctx.catalogueId ~= "" and O.byId(ctx.catalogueId)) and ctx.catalogueId or "pno"
  local key = T.key(st.root, st.scale)
  local prog = T.progressionById(st.prog or "I")
  if not T.progressionFits(key, prog) then prog = T.progressionsFor(key)[1]; st.prog = prog.id end
  st.chain = T.chainString(T.presetChain(prog))
  Cat.clampState(st)
end

function A.settings(st, ctx)
  sync(st, ctx)
  return C.describe(LIST, st, ctx)
end

function A.set(st, id, index, ctx)
  C.set(LIST, st, id, index, ctx)
  if id == "cat" then st.type = typesFor(st)[1] end
  if id == "section" and st.section ~= "" then st.cat = "Harmony"; st.type = typesFor(st)[1] end
  sync(st, ctx)
end

function A.useKey(st, root, scale)
  st.root, st.scale = root, scale
end

function A.generate(st, ctx, seed, count)
  sync(st, ctx)
  local c = Cat.contextFor(st, ctx.barBeats, ctx.bpm)
  local res = Cat.catalogue(c, st.type)
  if res.avoided then return { results = {}, message = res.avoided } end

  local out = {}
  for _, e in ipairs(res.entries) do
    if st.density == "Any" or e.band == st.density then
      local block = Cat.block(c, st.type, e, { transform = st.transform, repeats = st.repeats,
                                                velocity = st.velocity })
      local parts = {}
      for _, p in ipairs(block.parts) do
        parts[#parts + 1] = { name = p.name, instrument = p.inst, notes = C.notes(p.notes) }
      end
      out[#out + 1] = { title = e.label, detail = (e.hint or "") .. ((block.warning and ("\n" .. block.warning)) or ""),
                        beats = block.beats, parts = parts }
      if #out >= MAX_RESULTS then break end
    end
  end
  local msg
  if #res.hidden > 0 then
    local bits = {}
    for _, h in ipairs(res.hidden) do bits[#bits + 1] = ("%d %s"):format(h.count, h.reason) end
    msg = "Left out: " .. table.concat(bits, "; ")
  end
  return { results = out, message = msg }
end

return A
