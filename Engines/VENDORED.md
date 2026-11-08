# The engines, and where they came from

Everything in these folders except `adapters/` is copied **unchanged** from the
app's own repository (decision 0003). Do not edit them here: fix them there,
then run `tools/sync_engines.sh` and update this table.

| Folder | Repository | Commit | Files |
| --- | --- | --- | --- |
| `good-idea/` | KallumS/Good-Idea | 12da1b0 | gi_theory.lua, gi_idea.lua |
| `midi-catalogue/` | KallumS/Midi-Catalogue | 23fdf1f | mc_theory.lua, mc_orchestra.lua, mc_catalogue.lua |
| `midi-suggester/` | KallumS/Midi-Suggester | c71d570 | ms_theory.lua, ms_read.lua, ms_harmony.lua, ms_melody.lua |
| `midi-variator/` | KallumS/Midi-Variator | 6b5df24 | mv_theory.lua, mv_vary.lua |
| `starting-blocks/` | KallumS/Starting-Blocks-Notation | b33f3b1 | sb_engine.lua |

Also vendored: `Source/Core/ScaleModel.h` from KallumS/ScaleView at `084120c`
(unchanged) - the plugin's port of ScaleView Pro's engine at
ScaleView-for-Reaper `f13f8a6` (Pro 1.3.0, which took Starting Blocks' chords
back out as a dictionary: its decision 0011, reversed), diffed against Pro
itself,
Lua 5.4.7 in `ThirdParty/lua` (without lua.c, luac.c and the test files), and
Bravura in `Resources/Fonts`.

**8 November 2026:** `ScaleModel.h`, `ms_theory.lua` and `mv_theory.lua` are
synced from Noterator `13656e5` - ScaleView Pro's reader with the Blocks
dictionary removed (ScaleView `084120c`, Midi Suggester `3d5b911`, Midi
Variator `4a858ef`, each on a branch awaiting merge). Byte-identical to
Noterator's, and 427,776 names identical to Pro 1.3.0.

