# The engines, and where they came from

Everything in these folders except `adapters/` is copied **unchanged** from the
app's own repository (decision 0003). Do not edit them here: fix them there,
then run `tools/sync_engines.sh` and update this table.

| Folder | Repository | Commit | Files |
| --- | --- | --- | --- |
| `good-idea/` | KallumS/Good-Idea | 4f32b6b | gi_theory.lua, gi_idea.lua |
| `midi-catalogue/` | KallumS/Midi-Catalogue | a49db5d | mc_theory.lua, mc_orchestra.lua, mc_catalogue.lua |
| `midi-suggester/` | KallumS/Midi-Suggester | 40c14bc | ms_theory.lua, ms_read.lua, ms_harmony.lua, ms_melody.lua |
| `midi-variator/` | KallumS/Midi-Variator | 417ed44 | mv_theory.lua, mv_vary.lua |
| `starting-blocks/` | KallumS/Starting-Blocks-Notation | 79b71f4 | sb_engine.lua |

Also vendored: `Source/Core/ScaleModel.h` from KallumS/ScaleView at `b34d6f7`
(unchanged) - the plugin's port of ScaleView Pro's engine at
ScaleView-for-Reaper `df4ea43` (Starting Blocks' chords decide which roots a
chord can have), diffed against Pro itself,
Lua 5.4.7 in `ThirdParty/lua` (without lua.c, luac.c and the test files), and
Bravura in `Resources/Fonts`.
