# 0003 - Run the family's Lua engines unchanged, through embedded Lua

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

Good Idea, Midi Catalogue, Midi Suggester, Midi Variator and Starting Blocks
are tens of thousands of lines of Lua, each tested in its own repository and
tuned weight by weight against real music. Every one keeps its music in files
that never touch REAPER (`gi_idea.lua`, `mc_catalogue.lua`, ...), with REAPER
confined to a `_place.lua`.

## Decision

Embed Lua 5.4 (vendored in `ThirdParty/lua`) and run the engine files as they
are, copied unchanged into `Engines/<app>/`. Each gets a small adapter in
`Engines/adapters/` that gives every engine the same shape: settings out as
lists, results out as parts of notes. The engine files are compiled into the
binary.

## Why

- Porting would mean re-deriving every weight and re-proving every test, and
  the ports would drift from the originals the day after.
- The engines were written pure precisely so they could be run without REAPER.
  This is that.
- A fix upstream comes across by copying a file (`tools/sync_engines.sh`).

## Consequences

- **The files in `Engines/` other than `adapters/` are not edited here.** A
  change belongs in the engine's own repository first.
- The Lua state has no `io`, `os`, `package` or `debug`, and no `dofile`: a
  generator has no business with files.
- Settings the window shows are whatever the adapter says, so a setting added
  to an engine appears in the panel without C++ changing.
