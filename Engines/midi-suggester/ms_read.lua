--[[ Midi Suggester - reading what was imported.

     Pure Lua: no reaper., no ImGui. The notes arrive as plain tables, in
     quarter notes counted from the first bar line at or before the music:

       { pitch = 60, start = 0.0, len = 1.0, vel = 100 }

     and this file decides three things about them. Is it a melody or a chord
     progression? If a melody, what is the line - the one note at a time that
     a chord has to fit? If chords, where does each chord start and stop, and
     what is it called?
]]

local M = {}

-- Notes struck within this many quarter notes of each other are one event.
-- A played chord is never exactly together, and a thirty-second at 120bpm is
-- about the spread of a human strum.
M.ONSET_TOLERANCE = 0.125

-- The share of events that have to be three or more notes before the music
-- is called chords. Low on purpose: a progression with a moving bass line
-- strikes single bass notes between its chords, and is still a progression.
M.CHORD_SHARE = 0.3

-- Or this many notes sounding at once, on average, whenever anything is
-- sounding. That is what catches an arpeggiated progression, which strikes
-- one note at a time but lets them ring into each other.
M.CHORD_DENSITY = 2.5

local function byStart(a, b)
  if a.start ~= b.start then return a.start < b.start end
  return a.pitch < b.pitch
end

local function sorted(notes)
  local out = {}
  for i, n in ipairs(notes) do out[i] = n end
  table.sort(out, byStart)
  return out
end

-- Groups notes into events: everything struck within ONSET_TOLERANCE of the
-- event's first note. Returns { { start, notes = {...} }, ... } in order.
function M.events(notes)
  local out = {}
  for _, n in ipairs(sorted(notes)) do
    local last = out[#out]
    if last and n.start - last.start <= M.ONSET_TOLERANCE then
      last.notes[#last.notes + 1] = n
    else
      out[#out + 1] = { start = n.start, notes = { n } }
    end
  end
  return out
end

-- Where the music stops, in quarter notes.
function M.finish(notes)
  local e = 0
  for _, n in ipairs(notes) do e = math.max(e, n.start + n.len) end
  return e
end

-- The average number of notes sounding, over the time anything is sounding.
function M.density(notes)
  local edges = {}
  for _, n in ipairs(notes) do
    edges[#edges + 1] = { t = n.start, d = 1 }
    edges[#edges + 1] = { t = n.start + n.len, d = -1 }
  end
  table.sort(edges, function(a, b)
    if a.t ~= b.t then return a.t < b.t end
    return a.d < b.d          -- a note ending lets go before the next begins
  end)
  local sounding, busy, weighted, last = 0, 0, 0, nil
  for _, e in ipairs(edges) do
    if last and sounding > 0 then
      busy = busy + (e.t - last)
      weighted = weighted + (e.t - last) * sounding
    end
    sounding, last = sounding + e.d, e.t
  end
  if busy <= 0 then return 0 end
  return weighted / busy
end

--[[  "Melody" or "Chords", and the two measurements that decided it, so the
      window can say why. ]]
function M.classify(notes)
  if #notes == 0 then return nil, 0, 0 end
  local events, chordal = M.events(notes), 0
  for _, ev in ipairs(events) do
    local pcs, count = {}, 0
    for _, n in ipairs(ev.notes) do
      if not pcs[n.pitch % 12] then pcs[n.pitch % 12] = true; count = count + 1 end
    end
    if count >= 3 then chordal = chordal + 1 end
  end
  local share, density = chordal / #events, M.density(notes)
  local kind = (share >= M.CHORD_SHARE or density >= M.CHORD_DENSITY) and "Chords" or "Melody"
  return kind, share, density
end

--[[  The melody: the top note of each event, held until the next event
      starts or it ends, whichever is sooner. That is what an ear follows,
      and it means a melody imported with the odd double-stop still reads as
      one line. ]]
function M.melodyLine(notes)
  local events, line = M.events(notes), {}
  for i, ev in ipairs(events) do
    local top = ev.notes[1]
    for _, n in ipairs(ev.notes) do if n.pitch > top.pitch then top = n end end
    local len = top.len
    local nxt = events[i + 1]
    if nxt then len = math.min(len, nxt.start - ev.start) end
    line[#line + 1] = { pitch = top.pitch, start = ev.start, len = len, vel = top.vel or 100 }
  end
  return line
end

------------------------------------------------------------------------------
-- Cutting a progression into chords
------------------------------------------------------------------------------

local function pcSig(pitches)
  local seen, list = {}, {}
  for _, p in ipairs(pitches) do
    if not seen[p % 12] then seen[p % 12] = true; list[#list + 1] = p % 12 end
  end
  table.sort(list)
  return table.concat(list, ","), #list
end

local function lowest(pitches)
  local lo
  for _, p in ipairs(pitches) do if not lo or p < lo then lo = p end end
  return lo
end

-- Arpeggios strike one note at a time, so there are no events to cut at.
-- They are cut by the bar instead, and a bar holding more than four pitch
-- classes is tried in halves in case the harmony changed in the middle.
local function byBar(notes, barBeats, finish)
  local segs = {}
  local function collect(a, b)
    local ps = {}
    for _, n in ipairs(notes) do
      if n.start >= a - 1e-9 and n.start < b - 1e-9 then ps[#ps + 1] = n.pitch end
    end
    return ps
  end
  local bars = math.ceil(finish / barBeats - 1e-9)
  for bar = 0, bars - 1 do
    local a, b = bar * barBeats, (bar + 1) * barBeats
    local ps = collect(a, b)
    local _, count = pcSig(ps)
    local mid = a + barBeats / 2
    local left, right = collect(a, mid), collect(mid, b)
    local _, cl = pcSig(left)
    local _, cr = pcSig(right)
    if count > 4 and cl >= 3 and cr >= 3 then
      segs[#segs + 1] = { start = a, len = mid - a, pitches = left }
      segs[#segs + 1] = { start = mid, len = b - mid, pitches = right }
    elseif #ps > 0 then
      segs[#segs + 1] = { start = a, len = b - a, pitches = ps }
    end
  end
  return segs
end

--[[  Each chord is struck: an event of two or more notes starts one, and it
      lasts until the next one starts. Single notes in between - a bass
      walking, a note of melody on top - are left out of the name. ]]
local function byEvent(notes, finish)
  local strikes = {}
  for _, ev in ipairs(M.events(notes)) do
    if #ev.notes >= 2 then strikes[#strikes + 1] = ev end
  end
  if #strikes == 0 then return nil end
  local segs = {}
  for i, ev in ipairs(strikes) do
    local stop = strikes[i + 1] and strikes[i + 1].start or finish
    local ps = {}
    for _, n in ipairs(ev.notes) do ps[#ps + 1] = n.pitch end
    segs[#segs + 1] = { start = ev.start, len = stop - ev.start, pitches = ps }
  end
  return segs
end

--[[  The chords, each { start, len, pitches, pcs (a set), bass (a pitch),
      name, root (pitch class, or nil where the notes are only an interval) }.
      The same chord struck again straight after itself - a strummed rhythm
      - is one chord held, not several. ]]
function M.chordSegments(notes, key, T, barBeats)
  local finish = M.finish(notes)
  local segs = byEvent(notes, finish) or byBar(notes, barBeats, finish)

  local merged = {}
  for _, s in ipairs(segs) do
    local prev = merged[#merged]
    local sig = pcSig(s.pitches)
    local bass = lowest(s.pitches) % 12
    if prev and prev.sig == sig and prev.bassPc == bass then
      prev.len = s.start + s.len - prev.start
    else
      s.sig, s.bassPc = sig, bass
      merged[#merged + 1] = s
    end
  end

  for _, s in ipairs(merged) do
    s.pcs = {}
    for _, p in ipairs(s.pitches) do s.pcs[p % 12] = true end
    s.bass = lowest(s.pitches)
    s.sig, s.bassPc = nil, nil
  end
  M.nameSegments(merged, key, T)
  return merged
end

-- Naming depends on the key only for spelling and for settling a genuine
-- draw, so a change of key re-names without re-cutting.
function M.nameSegments(segs, key, T)
  for _, s in ipairs(segs) do
    s.name, s.root = T.nameChord(s.pitches, key)
  end
end

------------------------------------------------------------------------------
-- All of it at once
------------------------------------------------------------------------------

--[[  Everything the window needs to know about some notes. `kind` may be
      forced ("Melody" or "Chords") when the user has said which it is;
      otherwise it is decided here.

      Returns { kind, share, density, beats, line | chords, firstPc, lastPc,
      keys } where keys is detectKey's ranking, best first. ]]
function M.analyse(notes, barBeats, T, kind)
  local r = { notes = notes, barBeats = barBeats }
  local detected
  detected, r.share, r.density = M.classify(notes)
  r.detected = detected
  r.kind = kind or detected
  if not r.kind then return nil end

  -- A whole number of bars, so what gets suggested fills the last one.
  r.beats = math.max(barBeats, math.ceil(M.finish(notes) / barBeats - 1e-6) * barBeats)

  if r.kind == "Melody" then
    r.line = M.melodyLine(notes)
    r.firstPc = r.line[1].pitch % 12
    r.lastPc = r.line[#r.line].pitch % 12
    r.keys = T.detectKey(r.line, r.firstPc, r.lastPc)
  else
    -- The key comes first because spelling a chord needs one; the chords
    -- are then cut, and their basses settle a relative major against its
    -- minor the way a melody's last note does.
    local guess = T.detectKey(notes)[1]
    local provisional = T.key(guess.root, guess.scale)
    local segs = M.chordSegments(notes, provisional, T, barBeats)
    r.firstPc = segs[1].bass % 12
    r.lastPc = segs[#segs].bass % 12
    r.keys = T.detectKey(notes, r.firstPc, r.lastPc)
    r.chords = segs
    M.nameSegments(segs, T.key(r.keys[1].root, r.keys[1].scale), T)
  end
  return r
end

return M
