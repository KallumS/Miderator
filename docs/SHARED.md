# What Miderator shares with Noterator

Miderator is Noterator with a piano roll (decision 0026). These files are
Noterator's, **byte for byte**, and are never edited here: a fix is made in
Noterator and copied across.

```sh
tools/sync_from_noterator.sh ../Noterator --check   # which differ
tools/sync_from_noterator.sh ../Noterator           # copy them
```

Then build, run every test, render (`MideratorRender demo out.png`), and
update the commit below.

| Noterator commit | Synced |
| --- | --- |
| `1cb4015` | 2026-10-09 (the copy Miderator started from) |
| `270dfb4` | 2026-10-09: `Follow.h` and `TestFollow.cpp` (0033). Merged into Noterator's main |
| `5291bdc` | 2026-10-09: smooth following and `SmoothClock` in `Follow.h`, `Layout::playheadX` in `Engrave.*`, their tests (0034). Merged into Noterator's main |
| `c6f4c8d` | 2026-10-09: `fitToPolyphony` in `Generators.*` and its tests (0036). On Noterator's branch `ccr-ac8da7d9-3sbl5x` |
| `4c3f3ba` | 2026-10-09: `Orchestrate.*` (new, listed in the sync script and `CMakeLists.txt`), `insertIntoRange` handing several parts to it, `TestOrchestrate.cpp` (0040). On Noterator's branch `ccr-ac8da7d9-3sbl5x` |
| `42ba147` | 2026-10-09: `insertWhole` and no added parts in `Generators.*` and `Orchestrate.*`, the Generate tab's "Into" line, their tests (0041). On Noterator's branch `ccr-ac8da7d9-3sbl5x` |
| `b824612` | 2026-10-09: `isSingleLine`, `fitToSpan` filling once, a tune and second voice split in `Orchestrate.*`, the Generate tab's "Into" line, their tests (0042). On Noterator's branch `ccr-ac8da7d9-3sbl5x` |
| `271bbc7` | 2026-10-09: `addAudition`, every note of a result on a piano for an audition, and its test (0043). On Noterator's branch `ccr-ac8da7d9-3sbl5x` |
| `cba16c5` | 2026-10-09: Blocks chords keep their root (`ChordRoot`, `nameFromRoot`, `markChordRoot`), the adapter's root, and their tests (0046). On Noterator's branch `ccr-ac8da7d9-3sbl5x` |
| `5fae0da` | 2026-10-10: `ScaleModel.h` from ScaleView `b34d6f7` - Starting Blocks' chords decide which roots a chord can have (ScaleView Pro `df4ea43`). On Noterator's branch `ccr-ac8da7d9-3sbl5x` |
| `58b10f1` | 2026-10-10: Midi Suggester `40c14bc` and Midi Variator `417ed44` vendored - their chord readers take ScaleView Pro's Blocks dictionary too. On Noterator's branch `ccr-ac8da7d9-3sbl5x` |

## Shared

- `Source/Core/` - everything except `Roll.*`
- `Source/Engines/`
- `Engines/` - the engines and their adapters (and `VENDORED.md`, which says
  where the engines came from)
- `Source/App/` - `Exporter.*`, `GeneratorPanel.*`, `SettingsList.*`,
  `AudioEngine.h`, `BlocksPanel.h`
- `Tests/` - `Check.h` and every core test but `TestRoll.cpp`
- `cmake/EmbedLua.cmake`, `tools/try_generators.lua`, `tools/sync_engines.sh`
- `ThirdParty/lua/`

## Miderator's own

Everything else: `Source/Core/Roll.*`, the controller, the tracks, the piano
roll, the workspace and timeline, the toolbar and panels, the theme, the
window, `BlocksPanel.cpp` (its preview), `AudioEngine.cpp` (one name differs),
`Tests/TestRoll.cpp`, `Tests/TestApp.cpp`, `tools/RenderRoll.cpp`, the build,
the CI, the icon and these docs.

## Worth knowing

- A project is the same JSON in both apps: `.miderator` and `.noterator`
  both open in either.
- Two strings in shared files still say Noterator: the project format's own
  name (`"format": "noterator"`, which must stay so the apps read each
  other's files) and the `<software>` line in exported MusicXML.
