--[[ Midi Suggester, in Noterator.

     Select some music and it works out whether it is a melody or a chord
     progression, and what key it is in - its own reading, as in REAPER - then
     suggests chords to go under a melody or melodies to go over chords. ]]

local C = engine("adapters/common.lua")
local T = engine("midi-suggester/ms_theory.lua")
local R = engine("midi-suggester/ms_read.lua")
local H = engine("midi-suggester/ms_harmony.lua")
local Mel = engine("midi-suggester/ms_melody.lua")

local A = {
  id = "midi-suggester",
  name = "Suggest Notes",    -- Midi Suggester (decision 0025)
  description = "Select a melody and get chord progressions to go under it, or select chords and get melodies to go over them.",
  input = "selection",
  panel = "generate",
}

local function names(list)
  local out = {}
  for i, x in ipairs(list) do out[i] = x.name or x end
  return out
end
local function range(n) local out = {} for i = 1, n do out[i] = i end return out end

local LIST = {
  C.setting("what", "Suggest", { "Auto", "Melody", "Chords" },
    { "Whatever fits", "Chords under it", "A melody over it" },
    { hint = "Auto reads the selection: a tune gets chords, chords get a tune." }),
  C.setting("rhythm", "Chord pace", range(#H.RHYTHMS), names(H.RHYTHMS), {
    group = "Chords", when = function(st) return st.what ~= "Chords" end }),
  C.setting("colour", "Colour", range(#H.COLOURS), H.COLOURS, {
    group = "Chords", when = function(st) return st.what ~= "Chords" end }),
  C.setting("bass", "Bass line", { true, false }, { "Yes", "No" }, {
    group = "Chords", when = function(st) return st.what ~= "Chords" end }),
  C.setting("density", "Density", range(#Mel.DENSITIES), names(Mel.DENSITIES), {
    group = "Melody", when = function(st) return st.what ~= "Melody" end }),
  C.setting("register", "Register", range(#Mel.REGISTERS), names(Mel.REGISTERS), {
    group = "Melody", when = function(st) return st.what ~= "Melody" end }),
}

function A.newState()
  return { what = "Auto", rhythm = #H.RHYTHMS, colour = 1, bass = true, density = 2, register = 2 }
end
function A.settings(st, ctx) return C.describe(LIST, st, ctx) end
function A.set(st, id, index, ctx) C.set(LIST, st, id, index, ctx) end
function A.useKey() end   -- it reads the key from the music itself

function A.generate(st, ctx, seed, count)
  local sel = ctx.selection
  if not sel or #sel.notes == 0 then
    return { results = {}, message = "Select some music in the score first - a melody, or some chords." }
  end
  local barBeats = ctx.barBeats
  local forced = (st.what == "Melody" and "Melody") or (st.what == "Chords" and "Chords") or nil
  -- "Suggest a melody" means the selection is the chords, and vice versa.
  if st.what == "Melody" then forced = "Chords" elseif st.what == "Chords" then forced = "Melody" end
  local r = R.analyse(sel.notes, barBeats, T, forced)
  if not r then return { results = {}, message = "Nothing to read in the selection." } end
  r.beats = math.max(r.beats, sel.beats or 0)
  local key = T.key(r.keys[1].root, r.keys[1].scale)

  local out = {}
  if r.kind == "Melody" then
    local sugs = H.suggest(r.line, T, { key = key, beats = r.beats, barBeats = barBeats,
                                        rhythm = st.rhythm, colour = st.colour, variation = seed - 1 })
    for _, s in ipairs(sugs) do
      local parts = { { name = "Chords", notes = C.notes(H.voice(s, r.line, false)) } }
      if st.bass then
        local bass = {}
        for _, c in ipairs(s.chords) do
          bass[#bass + 1] = { start = c.start, len = c.len,
                              pitch = H.BASS_LOW + (c.chord.root - H.BASS_LOW) % 12, vel = H.VELOCITY or 100 }
        end
        parts[2] = { name = "Bass", notes = C.notes(bass) }
      end
      out[#out + 1] = { title = table.concat(s.symbols, "  "),
                        detail = ("%s  -  fits the tune %d%%"):format(table.concat(s.numerals, " "), math.floor(s.match * 100 + 0.5)),
                        beats = r.beats, parts = parts }
      if #out >= (count or 6) then break end
    end
  else
    local sugs = Mel.suggest(r.chords, T, { key = key, beats = r.beats, barBeats = barBeats,
                                            density = st.density, register = st.register, variation = seed - 1 })
    for i, s in ipairs(sugs) do
      out[#out + 1] = { title = ("Melody %d"):format(i),
                        detail = ("%d notes, %s to %s"):format(#s.notes, T.noteName(key, s.low % 12) .. (s.low // 12 - 1),
                                                               T.noteName(key, s.high % 12) .. (s.high // 12 - 1)),
                        beats = r.beats, parts = { { name = "Melody", notes = C.notes(s.notes) } } }
      if #out >= (count or 6) then break end
    end
  end
  local chordNames = {}
  if r.chords then for _, c in ipairs(r.chords) do chordNames[#chordNames + 1] = c.name end end
  local msg = ("Read the selection as %s in %s%s"):format(r.kind == "Melody" and "a melody" or "chords", key.label,
              #chordNames > 0 and (": " .. table.concat(chordNames, " ")) or "")
  return { results = out, message = msg }
end

return A
