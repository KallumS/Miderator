/*
    LuaEngine - the family's generators, run as they are.

    Good Idea, Midi Catalogue, Midi Suggester, Midi Variator and Starting
    Blocks are Lua, tested in their own repositories, and pure: none of their
    engine files touches REAPER. So rather than port tens of thousands of lines
    and their tuned weights to C++, Noterator runs the files themselves, copied
    unchanged into Engines/, through an embedded Lua 5.4 (decision 0003). Each
    has a small adapter (Engines/adapters/) that gives it the same shape as the
    others; this class speaks to the adapters and to nothing else.

    No JUCE in here. One instance owns one Lua state, which is not
    thread-safe: call it from one thread.
*/

#pragma once

#include "Score.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

struct lua_State;

namespace nt
{

struct GeneratorInfo
{
    std::string id, name, description;
    bool needsSelection = false;
    std::string panel;              // "generate", "toolbox", or empty: not shown
};

struct GeneratorSetting
{
    std::string id, label, hint, group;
    std::vector<std::string> names;
    std::vector<std::string> hints;   // one per value, may be empty
    int index = 0;                    // 0-based here
};

struct GeneratorContext
{
    int root = 0, scale = 0;          // scaleview indices, 0-based
    int num = 4, den = 4;
    double bpm = 120.0;
    std::string instrument;           // the target part's instrument id
    int bars = 4;
    int rangeBars = 0;                // bars selected to generate into, or 0
    std::vector<Note> selection;      // starts relative to the selection's first bar
    Tick selectionLength = 0;
    bool selectionIsDrums = false;
};

struct GeneratedPart
{
    std::string name;
    std::string instrument;           // what the engine wrote it for, if it said
    bool drums = false;
    std::vector<Note> notes;          // starts relative to the result
};

struct GeneratedResult
{
    std::string title, detail;
    Tick length = 0;
    std::vector<GeneratedPart> parts;
};

struct GeneratorOutput
{
    bool ok = false;
    std::string error;                // the engine failed - a bug, shown as such
    std::string message;              // what the engine wants to say
    std::vector<GeneratedResult> results;
};

class LuaEngine
{
public:
    LuaEngine();
    ~LuaEngine();
    LuaEngine (const LuaEngine&) = delete;
    LuaEngine& operator= (const LuaEngine&) = delete;

    bool ok() const { return loadError.empty(); }
    const std::string& error() const { return loadError; }

    const std::vector<GeneratorInfo>& generators() const { return infos; }

    std::vector<GeneratorSetting> settings (const std::string& generator, const GeneratorContext& ctx);
    void set (const std::string& generator, const std::string& settingId, int index, const GeneratorContext& ctx);
    void useKey (const std::string& generator, int root, int scale);
    void reset (const std::string& generator);
    GeneratorOutput generate (const std::string& generator, const GeneratorContext& ctx, int seed, int count);

private:
    lua_State* L = nullptr;
    std::string loadError;
    std::vector<GeneratorInfo> infos;
    std::map<std::string, int> adapterRefs;   // registry refs
    std::map<std::string, int> stateRefs;

    bool pushAdapterFunction (const std::string& generator, const char* name);
    void pushContext (const GeneratorContext& ctx);
    std::string popError();
};

} // namespace nt
