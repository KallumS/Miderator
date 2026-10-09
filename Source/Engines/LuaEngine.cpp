#include "LuaEngine.h"

#include "EmbeddedLua.h"
#include "Instruments.h"

#include <cmath>
#include <cstring>

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace nt
{

namespace
{
const char* cacheKey = "noterator.engines";

const embedded::File* findFile (const std::string& name)
{
    for (const auto& f : embedded::files())
        if (name == f.name) return &f;
    return nullptr;
}

// engine("good-idea/gi_idea.lua"): runs an embedded file once and hands back
// what it returned, the way the scripts' own `dofile(HERE .. name)` does.
int luaEngine (lua_State* L)
{
    const char* name = luaL_checkstring (L, 1);
    lua_getfield (L, LUA_REGISTRYINDEX, cacheKey);
    lua_getfield (L, -1, name);
    if (! lua_isnil (L, -1)) return 1;
    lua_pop (L, 1);

    const auto* file = findFile (name);
    if (file == nullptr) return luaL_error (L, "no engine file called %s", name);
    const std::string chunk = std::string ("@") + name;
    if (luaL_loadbufferx (L, file->data, file->size, chunk.c_str(), "t") != LUA_OK) return lua_error (L);
    lua_call (L, 0, 1);
    if (lua_isnil (L, -1)) { lua_pop (L, 1); lua_pushboolean (L, 1); }
    lua_pushvalue (L, -1);
    lua_setfield (L, -3, name);
    return 1;
}

int traceback (lua_State* L)
{
    const char* msg = lua_tostring (L, 1);
    luaL_traceback (L, L, msg != nullptr ? msg : "(error object is not a string)", 1);
    return 1;
}

std::string stringField (lua_State* L, int index, const char* key)
{
    lua_getfield (L, index, key);
    std::string out;
    if (lua_type (L, -1) == LUA_TSTRING || lua_type (L, -1) == LUA_TNUMBER) out = lua_tostring (L, -1);
    lua_pop (L, 1);
    return out;
}

double numberField (lua_State* L, int index, const char* key, double fallback)
{
    lua_getfield (L, index, key);
    const double v = lua_isnumber (L, -1) ? lua_tonumber (L, -1) : fallback;
    lua_pop (L, 1);
    return v;
}

bool boolField (lua_State* L, int index, const char* key)
{
    lua_getfield (L, index, key);
    const bool v = lua_toboolean (L, -1) != 0;
    lua_pop (L, 1);
    return v;
}

std::vector<std::string> stringList (lua_State* L, int index, const char* key)
{
    std::vector<std::string> out;
    lua_getfield (L, index, key);
    if (lua_istable (L, -1))
    {
        const auto n = static_cast<int> (luaL_len (L, -1));
        for (int i = 1; i <= n; ++i)
        {
            lua_rawgeti (L, -1, i);
            const char* s = lua_tostring (L, -1);
            out.emplace_back (s != nullptr ? s : "");
            lua_pop (L, 1);
        }
    }
    lua_pop (L, 1);
    return out;
}

Tick toTicks (double quarters) { return static_cast<Tick> (std::llround (quarters * static_cast<double> (PPQ))); }
double toQuarters (Tick t) { return static_cast<double> (t) / static_cast<double> (PPQ); }

void pushNotes (lua_State* L, const std::vector<Note>& notes)
{
    lua_createtable (L, static_cast<int> (notes.size()), 0);
    int i = 1;
    for (const auto& n : notes)
    {
        lua_createtable (L, 0, 4);
        lua_pushnumber (L, toQuarters (n.start)); lua_setfield (L, -2, "start");
        lua_pushnumber (L, toQuarters (n.length)); lua_setfield (L, -2, "len");
        lua_pushinteger (L, n.pitch); lua_setfield (L, -2, "pitch");
        lua_pushinteger (L, n.velocity); lua_setfield (L, -2, "vel");
        lua_rawseti (L, -2, i++);
    }
}

std::vector<Note> readNotes (lua_State* L, int index)
{
    std::vector<Note> out;
    lua_getfield (L, index, "notes");
    if (lua_istable (L, -1))
    {
        const auto n = static_cast<int> (luaL_len (L, -1));
        for (int i = 1; i <= n; ++i)
        {
            lua_rawgeti (L, -1, i);
            const int t = lua_gettop (L);
            Note note;
            note.start = toTicks (numberField (L, t, "start", 0));
            note.length = std::max<Tick> (1, toTicks (numberField (L, t, "len", 1)));
            note.pitch = static_cast<int> (numberField (L, t, "pitch", 60));
            note.velocity = static_cast<int> (numberField (L, t, "vel", 100));
            if (note.pitch >= 0 && note.pitch <= 127 && note.start >= 0) out.push_back (note);
            lua_pop (L, 1);
        }
    }
    lua_pop (L, 1);
    return out;
}
} // namespace

LuaEngine::LuaEngine()
{
    L = luaL_newstate();
    // The engines need tables, strings and maths. No io, os, package or debug:
    // a generator has no business with files or the shell.
    const luaL_Reg libs[] = {
        { LUA_GNAME, luaopen_base }, { LUA_TABLIBNAME, luaopen_table }, { LUA_STRLIBNAME, luaopen_string },
        { LUA_MATHLIBNAME, luaopen_math }, { LUA_UTF8LIBNAME, luaopen_utf8 }, { LUA_COLIBNAME, luaopen_coroutine },
    };
    for (const auto& lib : libs)
    {
        luaL_requiref (L, lib.name, lib.func, 1);
        lua_pop (L, 1);
    }
    // Nothing reads files either.
    lua_pushnil (L); lua_setglobal (L, "dofile");
    lua_pushnil (L); lua_setglobal (L, "loadfile");

    lua_newtable (L);
    lua_setfield (L, LUA_REGISTRYINDEX, cacheKey);
    lua_pushcfunction (L, luaEngine);
    lua_setglobal (L, "engine");

    lua_pushcfunction (L, traceback);
    lua_getglobal (L, "engine");
    lua_pushstring (L, "adapters/index.lua");
    if (lua_pcall (L, 1, 1, -3) != LUA_OK)
    {
        loadError = popError();
        lua_pop (L, 1);
        return;
    }
    const auto count = static_cast<int> (luaL_len (L, -1));
    for (int i = 1; i <= count; ++i)
    {
        lua_rawgeti (L, -1, i);
        const std::string path = lua_tostring (L, -1) != nullptr ? lua_tostring (L, -1) : "";
        lua_pop (L, 1);

        lua_pushcfunction (L, traceback);
        lua_getglobal (L, "engine");
        lua_pushstring (L, path.c_str());
        if (lua_pcall (L, 1, 1, -3) != LUA_OK)
        {
            loadError += path + ": " + popError() + "\n";
            lua_pop (L, 1);
            continue;
        }
        lua_remove (L, -2);   // the traceback handler
        const int a = lua_gettop (L);
        GeneratorInfo info;
        info.id = stringField (L, a, "id");
        info.name = stringField (L, a, "name");
        info.description = stringField (L, a, "description");
        info.needsSelection = stringField (L, a, "input") == "selection";
        info.panel = stringField (L, a, "panel");
        adapterRefs[info.id] = luaL_ref (L, LUA_REGISTRYINDEX);
        infos.push_back (info);
        reset (info.id);
    }
    lua_pop (L, 2);   // the list and the handler
}

LuaEngine::~LuaEngine()
{
    if (L != nullptr) lua_close (L);
}

std::string LuaEngine::popError()
{
    std::string msg = lua_tostring (L, -1) != nullptr ? lua_tostring (L, -1) : "unknown error";
    lua_pop (L, 1);
    return msg;
}

// Leaves [handler, function, state] on the stack.
bool LuaEngine::pushAdapterFunction (const std::string& generator, const char* name)
{
    auto a = adapterRefs.find (generator);
    auto s = stateRefs.find (generator);
    if (a == adapterRefs.end() || s == stateRefs.end()) return false;
    lua_pushcfunction (L, traceback);
    lua_rawgeti (L, LUA_REGISTRYINDEX, a->second);
    lua_getfield (L, -1, name);
    lua_remove (L, -2);
    if (! lua_isfunction (L, -1)) { lua_pop (L, 2); return false; }
    lua_rawgeti (L, LUA_REGISTRYINDEX, s->second);
    return true;
}

void LuaEngine::pushContext (const GeneratorContext& ctx)
{
    const std::string instrumentId = ctx.instrument.empty() ? std::string ("pno") : ctx.instrument;
    const auto& inst = instrumentById (instrumentId);
    lua_createtable (L, 0, 14);
    lua_pushinteger (L, ctx.root + 1); lua_setfield (L, -2, "root");
    lua_pushinteger (L, ctx.scale + 1); lua_setfield (L, -2, "scale");
    lua_pushinteger (L, ctx.num); lua_setfield (L, -2, "num");
    lua_pushinteger (L, ctx.den); lua_setfield (L, -2, "den");
    lua_pushnumber (L, ctx.num * 4.0 / ctx.den); lua_setfield (L, -2, "barBeats");
    Meter m { 0, ctx.num, ctx.den };
    lua_pushnumber (L, toQuarters (m.beatTicks())); lua_setfield (L, -2, "pulse");
    lua_pushnumber (L, ctx.bpm); lua_setfield (L, -2, "bpm");
    lua_pushstring (L, inst.id.c_str()); lua_setfield (L, -2, "inst");
    lua_pushstring (L, inst.catalogueId.c_str()); lua_setfield (L, -2, "catalogueId");
    lua_pushinteger (L, ctx.bars); lua_setfield (L, -2, "bars");
    lua_pushinteger (L, ctx.rangeBars); lua_setfield (L, -2, "rangeBars");
    lua_createtable (L, 0, 3);
    pushNotes (L, ctx.selection);
    lua_setfield (L, -2, "notes");
    lua_pushnumber (L, toQuarters (ctx.selectionLength)); lua_setfield (L, -2, "beats");
    lua_pushboolean (L, ctx.selectionIsDrums ? 1 : 0); lua_setfield (L, -2, "drums");
    lua_setfield (L, -2, "selection");
}

void LuaEngine::reset (const std::string& generator)
{
    auto a = adapterRefs.find (generator);
    if (a == adapterRefs.end()) return;
    lua_pushcfunction (L, traceback);
    lua_rawgeti (L, LUA_REGISTRYINDEX, a->second);
    lua_getfield (L, -1, "newState");
    lua_remove (L, -2);
    if (lua_pcall (L, 0, 1, -2) != LUA_OK)
    {
        loadError += generator + ": " + popError() + "\n";
        lua_pop (L, 1);
        return;
    }
    lua_remove (L, -2);
    auto s = stateRefs.find (generator);
    if (s != stateRefs.end()) luaL_unref (L, LUA_REGISTRYINDEX, s->second);
    stateRefs[generator] = luaL_ref (L, LUA_REGISTRYINDEX);
}

std::vector<GeneratorSetting> LuaEngine::settings (const std::string& generator, const GeneratorContext& ctx)
{
    std::vector<GeneratorSetting> out;
    if (! pushAdapterFunction (generator, "settings")) return out;
    pushContext (ctx);
    if (lua_pcall (L, 2, 1, -4) != LUA_OK)
    {
        loadError = generator + ": " + popError();
        lua_pop (L, 1);
        return out;
    }
    const int list = lua_gettop (L);
    const auto n = static_cast<int> (luaL_len (L, list));
    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti (L, list, i);
        const int t = lua_gettop (L);
        GeneratorSetting s;
        s.id = stringField (L, t, "id");
        s.label = stringField (L, t, "label");
        s.hint = stringField (L, t, "hint");
        s.group = stringField (L, t, "group");
        s.names = stringList (L, t, "names");
        s.hints = stringList (L, t, "hints");
        s.index = static_cast<int> (numberField (L, t, "index", 1)) - 1;
        if (! s.names.empty()) out.push_back (s);
        lua_pop (L, 1);
    }
    lua_pop (L, 2);
    return out;
}

void LuaEngine::set (const std::string& generator, const std::string& settingId, int index, const GeneratorContext& ctx)
{
    if (! pushAdapterFunction (generator, "set")) return;
    lua_pushstring (L, settingId.c_str());
    lua_pushinteger (L, index + 1);
    pushContext (ctx);
    if (lua_pcall (L, 4, 0, -6) != LUA_OK) loadError = generator + ": " + popError();
    lua_pop (L, 1);
}

void LuaEngine::useKey (const std::string& generator, int root, int scale)
{
    if (! pushAdapterFunction (generator, "useKey")) return;
    lua_pushinteger (L, root + 1);
    lua_pushinteger (L, scale + 1);
    if (lua_pcall (L, 3, 0, -5) != LUA_OK) loadError = generator + ": " + popError();
    lua_pop (L, 1);
}

GeneratorOutput LuaEngine::generate (const std::string& generator, const GeneratorContext& ctx, int seed, int count)
{
    GeneratorOutput out;
    if (! pushAdapterFunction (generator, "generate"))
    {
        out.error = "There is no generator called " + generator + ".";
        return out;
    }
    pushContext (ctx);
    lua_pushinteger (L, std::max (1, seed));
    lua_pushinteger (L, std::max (1, count));
    if (lua_pcall (L, 4, 1, -6) != LUA_OK)
    {
        out.error = popError();
        lua_pop (L, 1);
        return out;
    }
    const int res = lua_gettop (L);
    out.message = stringField (L, res, "message");
    lua_getfield (L, res, "results");
    const int list = lua_gettop (L);
    const auto n = lua_istable (L, list) ? static_cast<int> (luaL_len (L, list)) : 0;
    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti (L, list, i);
        const int r = lua_gettop (L);
        GeneratedResult result;
        result.title = stringField (L, r, "title");
        result.detail = stringField (L, r, "detail");
        result.length = toTicks (numberField (L, r, "beats", 0));
        lua_getfield (L, r, "parts");
        const int parts = lua_gettop (L);
        const auto pn = lua_istable (L, parts) ? static_cast<int> (luaL_len (L, parts)) : 0;
        for (int k = 1; k <= pn; ++k)
        {
            lua_rawgeti (L, parts, k);
            const int p = lua_gettop (L);
            GeneratedPart part;
            part.name = stringField (L, p, "name");
            part.instrument = stringField (L, p, "instrument");
            part.drums = boolField (L, p, "drums");
            part.notes = readNotes (L, p);
            if (! part.notes.empty()) result.parts.push_back (std::move (part));
            lua_pop (L, 1);
        }
        lua_pop (L, 1);
        Tick end = 0;
        for (const auto& part : result.parts)
            for (const auto& note : part.notes) end = std::max (end, note.end());
        if (result.length <= 0) result.length = end;
        if (! result.parts.empty()) out.results.push_back (std::move (result));
        lua_pop (L, 1);
    }
    lua_pop (L, 3);   // results, the returned table, the handler
    out.ok = true;
    return out;
}

} // namespace nt
