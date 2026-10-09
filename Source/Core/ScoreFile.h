/*
    ScoreFile - a score saved as a .noterator file, which is JSON.

    Plain text on purpose: a project can be read, diffed and repaired by hand,
    and a version of the app that does not know a field ignores it. Keys and
    scales are stored by name ("F#", "Dorian"), not by index, so reordering a
    table can never repoint a saved choice (ScaleView's rule).
*/

#pragma once

#include "Score.h"

#include <string>

namespace nt
{

std::string saveScore (const Score& score);

struct LoadResult
{
    bool ok = false;
    std::string error;
    Score score;
};

LoadResult loadScore (const std::string& text);

} // namespace nt
