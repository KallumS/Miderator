#!/bin/sh
# Copies the family's Lua engines into Engines/, unchanged, from clones that sit
# beside this repository (../Good-Idea, ../Midi-Catalogue, ...). Then run
#   lua5.4 tools/try_generators.lua && ./build-core/NoteratorTests
# and record the commits in Engines/VENDORED.md.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
SRC=${1:-$HERE/..}
copy() { # repo folder files...
  repo=$1; dest=$2; shift 2
  mkdir -p "$HERE/Engines/$dest"
  for f in "$@"; do cp "$SRC/$repo/reascripts/$f" "$HERE/Engines/$dest/"; done
  echo "$dest <- $repo @ $(cd "$SRC/$repo" && git rev-parse --short HEAD)"
}
copy Good-Idea good-idea gi_theory.lua gi_idea.lua
copy Midi-Catalogue midi-catalogue mc_theory.lua mc_orchestra.lua mc_catalogue.lua
copy Midi-Suggester midi-suggester ms_theory.lua ms_read.lua ms_harmony.lua ms_melody.lua
copy Midi-Variator midi-variator mv_theory.lua mv_vary.lua
copy Starting-Blocks-Notation starting-blocks sb_engine.lua
