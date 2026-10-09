#!/bin/sh
# Copies the files Miderator shares with Noterator across from a Noterator
# checkout, so a fix made there arrives here unchanged (decision 0026).
#
#   tools/sync_from_noterator.sh ../Noterator            copy them, then show what changed
#   tools/sync_from_noterator.sh ../Noterator --check    only list the ones that differ
#
# Then build and run the tests, render (MideratorRender demo out.png), and
# record the Noterator commit in docs/SHARED.md.
#
# Only the files below are shared. Everything about the window - the tracks,
# the piano roll, the controller, the toolbar, the theme - is Miderator's own,
# and so are Source/Core/Roll.* and Tests/TestRoll.cpp.
set -e
FROM="${1:?usage: tools/sync_from_noterator.sh <Noterator checkout> [--check]}"
MODE="${2:-copy}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"

SHARED="
Source/Core/AutoCC.cpp Source/Core/AutoCC.h
Source/Core/Detect.cpp Source/Core/Detect.h
Source/Core/Edit.cpp Source/Core/Edit.h
Source/Core/Follow.h
Source/Core/Engrave.cpp Source/Core/Engrave.h
Source/Core/Instruments.cpp Source/Core/Instruments.h
Source/Core/MidiFile.cpp Source/Core/MidiFile.h
Source/Core/MusicXml.cpp Source/Core/MusicXml.h
Source/Core/Perform.cpp Source/Core/Perform.h
Source/Core/ScaleModel.h
Source/Core/Score.cpp Source/Core/Score.h
Source/Core/ScoreFile.cpp Source/Core/ScoreFile.h
Source/Core/Spelling.cpp Source/Core/Spelling.h
Source/Core/Templates.cpp Source/Core/Templates.h
Source/Core/Xml.cpp Source/Core/Xml.h
Source/Engines/EmbeddedLua.h
Source/Engines/Generators.cpp Source/Engines/Generators.h
Source/Engines/Orchestrate.cpp Source/Engines/Orchestrate.h
Source/Engines/LuaEngine.cpp Source/Engines/LuaEngine.h
Source/App/AudioEngine.h
Source/App/Exporter.cpp Source/App/Exporter.h
Source/App/GeneratorPanel.cpp Source/App/GeneratorPanel.h
Source/App/SettingsList.cpp Source/App/SettingsList.h
Source/App/BlocksPanel.h
Tests/Check.h Tests/TestMain.cpp Tests/TestScore.cpp Tests/TestEngrave.cpp Tests/TestDetect.cpp
Tests/TestMidi.cpp Tests/TestGenerators.cpp Tests/TestMusicXml.cpp Tests/TestTemplates.cpp Tests/TestFollow.cpp Tests/TestOrchestrate.cpp
cmake/EmbedLua.cmake
tools/try_generators.lua tools/sync_engines.sh
Engines/VENDORED.md
"
# The engines and their adapters, whole folders.
FOLDERS="Engines/adapters Engines/good-idea Engines/midi-catalogue Engines/midi-suggester Engines/midi-variator Engines/starting-blocks ThirdParty/lua"

differ=0
for f in $SHARED; do
  if [ ! -f "$FROM/$f" ]; then echo "  gone from Noterator: $f"; continue; fi
  if ! cmp -s "$FROM/$f" "$HERE/$f"; then
    differ=$((differ + 1))
    if [ "$MODE" = "--check" ]; then echo "  differs: $f"; else cp "$FROM/$f" "$HERE/$f"; echo "  copied: $f"; fi
  fi
done
for d in $FOLDERS; do
  for f in $(cd "$FROM" && find "$d" -type f); do
    if ! cmp -s "$FROM/$f" "$HERE/$f" 2>/dev/null; then
      differ=$((differ + 1))
      if [ "$MODE" = "--check" ]; then echo "  differs: $f"; else mkdir -p "$(dirname "$HERE/$f")"; cp "$FROM/$f" "$HERE/$f"; echo "  copied: $f"; fi
    fi
  done
done
echo "$differ shared file(s) $( [ "$MODE" = "--check" ] && echo differ || echo copied ) from $(cd "$FROM" && git rev-parse --short HEAD 2>/dev/null || echo "$FROM")"
