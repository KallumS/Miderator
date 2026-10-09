/*
    Templates - the ensembles a new score can start from (decision 0022).

    Each is a list of instrument ids in score order: woodwind, brass,
    percussion, keyboards and harp, voices, strings - the order every
    orchestral score is printed in - and for a big band the saxes, trumpets,
    trombones, then the rhythm section. A repeated instrument becomes
    numbered parts: Horn 1 to Horn 4.
*/

#pragma once

#include "Score.h"

#include <string>
#include <vector>

namespace nt
{

struct ScoreTemplate
{
    std::string name;
    std::string group;           // the heading it is listed under
    std::string description;     // what is in it, in a few words
    std::vector<std::string> instruments;
};

// Every template, grouped and in the order they are listed.
const std::vector<ScoreTemplate>& scoreTemplates();
const ScoreTemplate* templateByName (const std::string& name);

// An empty score with a part for each of the template's instruments, titled
// with its name ("Untitled" for Empty).
Score scoreFromTemplate (const ScoreTemplate& t);

} // namespace nt
