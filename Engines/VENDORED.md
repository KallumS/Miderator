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

Also vendored: `Source/Core/ScaleModel.h` from KallumS/ScaleView at `b34d6f7`
(unchanged) - the plugin's port of ScaleView Pro's engine at
ScaleView-for-Reaper `df4ea43` (Starting Blocks' chords decide which roots a
chord can have), diffed against Pro itself,
Lua 5.4.7 in `ThirdParty/lua` (without lua.c, luac.c and the test files), and
Bravura in `Resources/Fonts`.
