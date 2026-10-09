--[[ Runs every generator adapter outside the app, the way the app calls them,
     and prints what each makes. No app, no JUCE:

       lua5.4 tools/try_generators.lua            every generator
       lua5.4 tools/try_generators.lua good-idea  one of them

     `engine` here reads the files from Engines/; in the app the same names
     are read from the copies embedded in the binary. ]]

local HERE = (arg and arg[0] or ""):match("^(.*)[/\\]") or "."
local ROOT = HERE .. "/../Engines/"
local cache = {}
function engine(name)
  if cache[name] == nil then
    local chunk = assert(loadfile(ROOT .. name))
    cache[name] = chunk() or false
  end
  return cache[name]
end

local only = arg[1]
local ctx = {
  root = 1, scale = 1, num = 4, den = 4, barBeats = 4, bpm = 110,
  inst = "vla", catalogueId = "vla", bars = 4,
  selection = {
    beats = 8, drums = false,
    notes = {   -- Twinkle, the first two bars and a bit
      { start = 0, len = 1, pitch = 60, vel = 100 }, { start = 1, len = 1, pitch = 60, vel = 100 },
      { start = 2, len = 1, pitch = 67, vel = 100 }, { start = 3, len = 1, pitch = 67, vel = 100 },
      { start = 4, len = 1, pitch = 69, vel = 100 }, { start = 5, len = 1, pitch = 69, vel = 100 },
      { start = 6, len = 2, pitch = 67, vel = 100 },
    },
  },
}

local failures = 0
for _, path in ipairs(engine("adapters/index.lua")) do
  local A = engine(path)
  if not only or only == A.id then
    local st = A.newState()
    A.useKey(st, 1, 1)
    local settings = A.settings(st, ctx)
    print(("== %s (%d settings, input %s)"):format(A.name, #settings, A.input))
    for _, s in ipairs(settings) do
      print(("   %-16s %s"):format(s.label, s.names[s.index] or "?"))
    end
    -- Every value of every setting can be chosen without an error.
    for _, s in ipairs(settings) do
      for i = 1, #s.names do
        local copy = A.newState()
        local ok, err = pcall(A.set, copy, s.id, i, ctx)
        if not ok then failures = failures + 1; print("   FAIL set " .. s.id .. " " .. i .. ": " .. tostring(err)) end
      end
    end
    local ok, res = pcall(A.generate, st, ctx, 1, 3)
    if not ok then
      failures = failures + 1
      print("   FAIL generate: " .. tostring(res))
    else
      if res.message then print("   " .. res.message) end
      for _, r in ipairs(res.results) do
        local count = 0
        for _, p in ipairs(r.parts) do count = count + #p.notes end
        print(("   * %s  [%g beats, %d parts, %d notes]"):format(r.title, r.beats, #r.parts, count))
        if count == 0 then failures = failures + 1; print("     FAIL: no notes") end
      end
      if #res.results == 0 then failures = failures + 1; print("   FAIL: no results") end
    end
  end
end
if failures > 0 then
  print(failures .. " failure(s)")
  os.exit(1)
end
print("all generators ran")
