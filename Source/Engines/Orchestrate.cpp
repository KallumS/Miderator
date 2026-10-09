#include "Orchestrate.h"

#include "Detect.h"
#include "Edit.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>

namespace nt
{

namespace
{
int pcOf (int pitch) { return ((pitch % 12) + 12) % 12; }

int centreOf (const Instrument& i) { return (i.sweetLow + i.sweetHigh) / 2; }

std::string lower (std::string n)
{
    std::transform (n.begin(), n.end(), n.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return n;
}

// The note of a line sounding at `t`, or the first to start within [t, end);
// -1 if neither.
int pitchAt (const std::vector<Note>& line, Tick t, Tick end)
{
    int found = -1;
    Tick latest = -1;
    for (const auto& n : line)
        if (n.start <= t && n.end() > t && n.start > latest) { found = n.pitch; latest = n.start; }
    if (found >= 0) return found;
    Tick first = end;
    for (const auto& n : line)
        if (n.start >= t && n.start < first) { found = n.pitch; first = n.start; }
    return found;
}

// An octave down (or up), each note still outside the instrument's range
// folded back in by octaves: a doubling stays a doubling.
std::vector<Note> shiftAndFold (std::vector<Note> notes, int semitones, const Instrument& inst)
{
    for (auto& n : notes)
    {
        n.pitch += semitones;
        while (n.pitch < inst.low && n.pitch + 12 <= 127) n.pitch += 12;
        while (n.pitch > inst.high && n.pitch - 12 >= 0) n.pitch -= 12;
    }
    return notes;
}

bool isContrabass (const Instrument& i) { return i.id == "cb" || i.id == "cbsn" || i.id == "tuba"; }

// Where a section may double the tune at the octave: strings and woodwind
// (Rimsky-Korsakov's Vns I and II, flutes over oboes). Brass and saxophones
// voice the chord closely instead; voices sing four real parts.
bool doublesTheTune (const std::string& family, const std::vector<const Instrument*>& section)
{
    if (family == "Strings" || family.empty()) return true;
    if (family != "Woodwind") return false;
    for (const auto* i : section)
        if (i->id.find ("sax") != std::string::npos) return false;
    return true;
}

int rootFromName (const std::string& name, int fallback)
{
    if (name.empty()) return fallback;
    static const std::map<char, int> letters { { 'C', 0 }, { 'D', 2 }, { 'E', 4 }, { 'F', 5 }, { 'G', 7 }, { 'A', 9 }, { 'B', 11 } };
    const auto it = letters.find (name[0]);
    if (it == letters.end()) return fallback;
    int pc = it->second;
    if (name.size() > 1 && name[1] == '#') ++pc;
    if (name.size() > 1 && name[1] == 'b') --pc;
    return pcOf (pc);
}

// How much the chord wants each of its notes beside the tune and the bass:
// the third and the seventh make the chord, the root anchors it, the fifth
// can go (Rimsky-Korsakov; the four-part books).
int neededRank (int pc, int root)
{
    switch (pcOf (pc - root))
    {
        case 3: case 4: return 0;
        case 10: case 11: return 1;
        case 0: return 2;
        case 7: return 4;
        default: return 3;
    }
}

// And which it doubles first: the root, the fifth, anything else, the third
// last; never the seventh while something else will do.
int doublingRank (int pc, int root)
{
    switch (pcOf (pc - root))
    {
        case 0: return 0;
        case 7: return 1;
        case 3: case 4: return 3;
        case 10: case 11: return 4;
        default: return 2;
    }
}
} // namespace

std::vector<Assignment> planOrchestra (const Score& score, const std::vector<uint32_t>& parts, const Layers& layers)
{
    std::vector<Assignment> out;
    // One-note instruments by section, in the order chosen.
    std::vector<std::string> families;
    std::map<std::string, std::vector<std::pair<uint32_t, const Instrument*>>> sections;
    bool kitTaken = false;
    for (auto id : parts)
    {
        const auto* p = score.partById (id);
        if (p == nullptr) continue;
        const auto& inst = instrumentById (p->instrument);
        Assignment a;
        a.partId = id;
        if (inst.drums)
        {
            if (layers.drums && ! kitTaken) { a.layer = Layer::drums; kitTaken = true; }
        }
        else if (inst.id == "timp") a.layer = (layers.harmony || layers.bass) ? Layer::timpani : Layer::none;
        else if (inst.poly >= 3) a.layer = layers.harmony ? Layer::chords : (layers.melody ? Layer::melody : (layers.bass ? Layer::bass : Layer::none));
        else if (inst.poly == 2) a.layer = layers.melody ? Layer::melody : Layer::none;   // glockenspiel, xylophone: the tune, up high
        else
        {
            if (sections.find (inst.family) == sections.end()) families.push_back (inst.family);
            sections[inst.family].push_back ({ id, &inst });
            continue;
        }
        out.push_back (a);
    }

    // A section of one joins the others of one, as a section of its own.
    std::vector<std::pair<std::string, std::vector<std::pair<uint32_t, const Instrument*>>>> groups;
    std::vector<std::pair<uint32_t, const Instrument*>> singles;
    for (const auto& f : families)
    {
        if (sections[f].size() == 1) singles.push_back (sections[f].front());
        else groups.push_back ({ f, sections[f] });
    }
    if (! singles.empty()) groups.push_back ({ std::string(), singles });

    for (auto& [family, members] : groups)
    {
        // Top to bottom by where each sounds best; the order chosen breaks ties.
        std::stable_sort (members.begin(), members.end(), [] (const auto& a, const auto& b) { return centreOf (*a.second) > centreOf (*b.second); });
        std::vector<Layer> roles (members.size(), Layer::none);
        const size_t n = members.size();
        auto role = [] (const Instrument& i) { return i.role; };

        if (! layers.harmony && ! layers.bass)
        {
            // A tune and nothing else: everyone plays it, in octaves - the
            // lower half the second voice, where there is one.
            for (size_t i = 0; i < n; ++i)
                roles[i] = ! layers.melody ? Layer::none
                         : (layers.second && n >= 2 && i >= (n + 1) / 2) ? Layer::second : Layer::melody;
        }
        else if (n == 1)
        {
            const auto& i = *members.front().second;
            roles[0] = role (i) == 'B' ? Layer::bass
                     : (role (i) == 'S' && layers.melody) ? Layer::melody
                     : layers.harmony ? Layer::inner : Layer::melody;
        }
        else
        {
            roles[0] = layers.melody ? Layer::melody : Layer::inner;
            size_t bottom = n - 1;
            bool otherBass = false;
            for (size_t i = 1; i + 1 < n; ++i) otherBass = otherBass || members[i].second->role == 'B';
            if (n >= 3 && isContrabass (*members[bottom].second) && otherBass)
            {
                roles[bottom] = Layer::bassDouble;   // the bass an octave down, never alone
                --bottom;
            }
            roles[bottom] = Layer::bass;
            std::vector<size_t> middle;
            for (size_t i = 1; i < bottom; ++i) middle.push_back (i);
            if (! layers.harmony)
            {
                // A tune and a bass: the upper half doubles one, the lower the other.
                for (size_t k = 0; k < middle.size(); ++k) roles[middle[k]] = k < (middle.size() + 1) / 2 ? Layer::melodyDouble : Layer::bass;
            }
            else
            {
                std::vector<const Instrument*> insts;
                for (const auto& m : members) insts.push_back (m.second);
                const int k = static_cast<int> (middle.size());
                const int extra = k - layers.innerNeeded;
                int doubles = 0;
                if (layers.melody && extra > 0 && doublesTheTune (family, insts))
                    doubles = std::min ((extra + 1) / 2, std::max (1, static_cast<int> (n) / 4));
                for (int m = 0; m < k; ++m)
                    roles[middle[static_cast<size_t> (m)]] = m < doubles ? Layer::melodyDouble : Layer::inner;
                // A second voice, where there is one, is the first doubling,
                // or else the top inner part.
                if (layers.second && k >= 1 && (doubles > 0 || k > layers.innerNeeded))
                    roles[middle.front()] = Layer::second;
            }
        }
        for (size_t i = 0; i < n; ++i)
        {
            Assignment a;
            a.partId = members[i].first;
            a.layer = roles[i];
            out.push_back (a);
        }
    }
    return out;
}

std::vector<Harmony> readHarmony (const Score& score, Tick at, const std::vector<Note>& chords, const std::vector<Note>& bass)
{
    std::vector<Harmony> out;
    if (chords.empty()) return out;
    // The Chords lane's reader, on a score of these notes alone.
    Score t;
    t.meters = score.meters;
    t.keys = score.keys;
    t.tempos = score.tempos;
    Part p;
    p.id = t.newId();
    p.instrument = "pno";
    Tick end = 0;
    for (const auto* line : { &chords, &bass })
        for (auto n : *line)
        {
            end = std::max (end, n.end());
            n.start += at;
            p.notes.push_back (n);
        }
    t.parts = { p };
    t.fitBars();
    const auto spans = detectChords (t, at, at + end);

    for (const auto& cs : spans)
    {
        Harmony h;
        h.start = cs.start - at;
        h.end = cs.end - at;
        if (h.end <= h.start) continue;
        std::set<int> pcs;
        for (int q : cs.pitches) pcs.insert (pcOf (q));
        // The bass: the separate bass line's lowest, or the chords' own.
        int low = 128;
        for (const auto* line : { &bass, &chords })
        {
            for (const auto& n : *line)
                if (n.start < h.end && n.end() > h.start) { low = std::min (low, n.pitch); pcs.insert (pcOf (n.pitch)); }
            if (low < 128) break;
        }
        if (pcs.empty()) continue;
        h.pcs.assign (pcs.begin(), pcs.end());
        h.bass = low < 128 ? pcOf (low) : h.pcs.front();
        h.root = rootFromName (cs.name, h.bass);

        // Where the chord is struck: two or more of its upper notes at once.
        std::map<Tick, std::vector<const Note*>> onsets;
        for (const auto& n : chords)
            if (n.start >= h.start && n.start < h.end && n.pitch != low) onsets[n.start].push_back (&n);
        int velocity = 0, count = 0;
        for (const auto& n : chords)
            if (n.start < h.end && n.end() > h.start) { velocity += n.velocity; ++count; }
        const int meanVelocity = count > 0 ? velocity / count : 80;
        for (const auto& [s, ns] : onsets)
        {
            if (ns.size() < 2) continue;
            Tick len = 0;
            int v = 0;
            for (const auto* n : ns) { len = std::max (len, n->length); v += n->velocity; }
            h.hits.push_back ({ s, len, v / static_cast<int> (ns.size()) });
        }
        if (h.hits.empty()) h.hits.push_back ({ h.start, h.end - h.start, meanVelocity });   // broken chords: held instead
        for (size_t i = 0; i < h.hits.size(); ++i)
        {
            const Tick next = i + 1 < h.hits.size() ? h.hits[i + 1].start : h.end;
            h.hits[i].length = std::max<Tick> (1, std::min (h.hits[i].length, next - h.hits[i].start));
        }
        out.push_back (h);
    }
    return out;
}

std::vector<std::vector<Note>> voiceInner (const std::vector<Harmony>& harmony, const std::vector<const Instrument*>& voices,
                                           const std::vector<Note>& above, const std::vector<Note>& below,
                                           const std::vector<Note>& source)
{
    const size_t m = voices.size();
    std::vector<std::vector<Note>> out (m);
    if (m == 0) return out;
    std::vector<int> previous (m, -1);

    for (const auto& h : harmony)
    {
        if (h.pcs.empty()) continue;
        const int top = pitchAt (above, h.start, h.end);
        const int floor = pitchAt (below, h.start, h.end);
        const int topPc = top >= 0 ? pcOf (top) : -1;
        const int bassPc = floor >= 0 ? pcOf (floor) : h.bass;

        // The chord's notes the tune and the bass leave over, most wanted
        // first; then the doublings.
        std::vector<int> needed, doubling;
        for (int pc : h.pcs)
        {
            if (pc != topPc && pc != bassPc) needed.push_back (pc);
            doubling.push_back (pc);
        }
        std::stable_sort (needed.begin(), needed.end(), [&] (int a, int b) { return neededRank (a, h.root) < neededRank (b, h.root); });
        std::stable_sort (doubling.begin(), doubling.end(), [&] (int a, int b) { return doublingRank (a, h.root) < doublingRank (b, h.root); });
        std::vector<int> wanted;
        for (int pc : needed) if (wanted.size() < m) wanted.push_back (pc);
        for (size_t i = 0; wanted.size() < m; ++i) wanted.push_back (doubling[i % doubling.size()]);

        // Each voice's note for a given pitch class: the nearest to where it
        // was, below the voice above, above the bass, in its own range.
        const int ceiling = top >= 0 ? top : 127;
        auto place = [&] (size_t v, int pc, int upper, int& cost) -> int
        {
            const auto& inst = *voices[v];
            int ref = previous[v];
            if (ref < 0)
            {
                // A first note where the chord as generated has it, if this
                // part can play it there: its spacing is the generator's.
                int nearest = -1;
                for (const auto& n : source)
                    if (n.start <= h.start + 1 && n.end() > h.start && pcOf (n.pitch) == pc && n.pitch >= inst.low && n.pitch <= inst.high
                        && (nearest < 0 || std::abs (n.pitch - centreOf (inst)) < std::abs (nearest - centreOf (inst))))
                        nearest = n.pitch;
                if (nearest < 0)
                    for (const auto& n : source)
                        if (n.start >= h.start && n.start < h.end && pcOf (n.pitch) == pc && n.pitch >= inst.low && n.pitch <= inst.high) { nearest = n.pitch; break; }
                ref = nearest;
            }
            if (ref < 0)
            {
                // A first note between the lines around it, near its best register.
                const int lo = floor >= 0 ? floor : inst.sweetLow;
                const int hi = top >= 0 ? top : inst.sweetHigh;
                const int spread = hi - (hi - lo) * static_cast<int> (v + 1) / static_cast<int> (m + 1);
                ref = (spread + centreOf (inst)) / 2;
            }
            int best = -1, bestCost = 1 << 20;
            for (int p = inst.low; p <= inst.high; ++p)
            {
                if (pcOf (p) != pc) continue;
                int c = std::abs (p - ref);
                if (p < inst.sweetLow || p > inst.sweetHigh) c += 4;
                if (p > upper) c += 24 + (p - upper) * 2;                  // crossing the voice above
                if (floor >= 0 && p < floor) c += 24 + (floor - p) * 2;    // or the bass
                if (upper < 128 && p >= 55 && upper - p > 12) c += upper - p - 12;   // close at the top
                if (p == upper) c += 4;                                     // a unison only where nothing else will do
                if (floor >= 0 && p < 55 && p - floor > 0 && p - floor < 7) c += 16;   // no close intervals low down
                if (floor >= 0 && p < 48 && p - floor > 0 && p - floor < 12) c += 6;
                if (c < bestCost) { bestCost = c; best = p; }
            }
            cost += bestCost;
            return best;
        };

        std::vector<int> chosen;
        if (m <= 6)
        {
            // Every order of the wanted notes, top voice first: the cheapest.
            std::vector<int> perm = wanted;
            std::sort (perm.begin(), perm.end());
            int bestCost = 1 << 30;
            do
            {
                int cost = 0, upper = ceiling;
                std::vector<int> pitches;
                for (size_t v = 0; v < m; ++v)
                {
                    const int p = place (v, perm[v], upper, cost);
                    pitches.push_back (p);
                    if (p >= 0) upper = p;
                }
                if (cost < bestCost) { bestCost = cost; chosen = pitches; }
            } while (std::next_permutation (perm.begin(), perm.end()));
        }
        else
        {
            // Many voices: each takes the cheapest note left, top down.
            std::multiset<int> left (wanted.begin(), wanted.end());
            int upper = ceiling;
            for (size_t v = 0; v < m; ++v)
            {
                int bestPc = *left.begin(), bestCost = 1 << 30, bestPitch = -1;
                for (int pc : std::set<int> (left.begin(), left.end()))
                {
                    int cost = 0;
                    const int p = place (v, pc, upper, cost);
                    if (cost < bestCost) { bestCost = cost; bestPc = pc; bestPitch = p; }
                }
                left.erase (left.find (bestPc));
                chosen.push_back (bestPitch);
                if (bestPitch >= 0) upper = bestPitch;
            }
        }

        for (size_t v = 0; v < m; ++v)
        {
            if (chosen[v] < 0) continue;
            previous[v] = chosen[v];
            for (const auto& hit : h.hits)
            {
                Note n;
                n.start = hit.start;
                n.length = hit.length;
                n.pitch = chosen[v];
                n.velocity = hit.velocity;
                out[v].push_back (n);
            }
        }
    }
    return out;
}

InsertReport orchestrate (Score& score, const GeneratedResult& fitted, const std::vector<uint32_t>& parts,
                          Tick from, Tick to, const InsertOptions& options)
{
    // What the result offers, by its parts' names as insertIntoRange always read them.
    const GeneratedPart* melody = nullptr;
    const GeneratedPart* second = nullptr;
    const GeneratedPart* bass = nullptr;
    const GeneratedPart* chords = nullptr;
    const GeneratedPart* drums = nullptr;
    std::vector<const GeneratedPart*> others;
    // Names first: a tune whose notes overlap by a hair is still the tune.
    for (const auto& gp : fitted.parts)
    {
        const auto n = lower (gp.name);
        const auto has = [&n] (const char* w) { return n.find (w) != std::string::npos; };
        if (gp.drums) { if (drums == nullptr) drums = &gp; else others.push_back (&gp); }
        else if (has ("bass") && bass == nullptr) bass = &gp;
        else if (has ("second") && second == nullptr) second = &gp;
        else if (has ("chord") && chords == nullptr) chords = &gp;
        else if ((has ("melody") || has ("tune") || has ("lead")) && melody == nullptr) melody = &gp;
        else others.push_back (&gp);
    }
    // Then what the unnamed ones sound like.
    std::vector<const GeneratedPart*> unnamed;
    unnamed.swap (others);
    for (const auto* gp : unnamed)
    {
        if (! gp->drums && chords == nullptr && polyphonyOf (gp->notes) > 2) chords = gp;
        else if (! gp->drums && melody == nullptr) melody = gp;
        else others.push_back (gp);
    }

    const std::vector<Note> none;
    const auto harmony = readHarmony (score, from, chords ? chords->notes : none, bass ? bass->notes : none);

    // The tune: the melody, or the top of the chords where it is struck.
    std::vector<Note> tune = melody ? melody->notes : std::vector<Note>();
    if (melody == nullptr && chords != nullptr)
        for (const auto& h : harmony)
            for (const auto& hit : h.hits)
            {
                int top = -1;
                for (const auto& n : chords->notes)
                    if (n.start <= hit.start && n.end() > hit.start) top = std::max (top, n.pitch);
                if (top < 0) continue;
                Note n;
                n.start = hit.start;
                n.length = hit.length;
                n.pitch = top;
                n.velocity = hit.velocity;
                tune.push_back (n);
            }
    // The bass: its own part, or the chords' lowest notes.
    std::vector<Note> low = bass ? bass->notes : std::vector<Note>();
    if (bass == nullptr && chords != nullptr)
        for (const auto& h : harmony)
        {
            int lowest = 128;
            for (const auto& n : chords->notes)
                if (n.start < h.end && n.end() > h.start) lowest = std::min (lowest, n.pitch);
            bool struck = false;
            for (const auto& n : chords->notes)
                if (n.pitch == lowest && n.start >= h.start && n.start < h.end)
                {
                    Note b = n;
                    b.length = std::min (b.length, h.end - b.start);
                    low.push_back (b);
                    struck = true;
                }
            if (! struck && lowest < 128)
            {
                Note b;
                b.start = h.start;
                b.length = h.end - h.start;
                b.pitch = lowest;
                b.velocity = h.hits.empty() ? 80 : h.hits.front().velocity;
                low.push_back (b);
            }
        }

    Layers layers;
    layers.melody = ! tune.empty();
    layers.second = second != nullptr && ! second->notes.empty();
    layers.bass = ! low.empty();
    layers.harmony = ! harmony.empty() && chords != nullptr;
    layers.drums = drums != nullptr;
    {
        // The chord notes left beside the tune and the bass, in most chords.
        std::map<int, int> counts;
        for (const auto& h : harmony)
            if (h.pcs.size() >= 2) ++counts[std::max (1, static_cast<int> (h.pcs.size()) - 2)];
        int most = 0;
        for (const auto& [k, c] : counts)
            if (c > most) { most = c; layers.innerNeeded = std::min (3, k); }
    }

    const auto plan = planOrchestra (score, parts, layers);

    InsertReport report;
    enum class Fit { instrument, fold, asIs };
    auto put = [&] (uint32_t partId, std::vector<Note> notes, Fit fit) -> std::vector<Note>
    {
        const auto* p = score.partById (partId);
        if (p == nullptr || notes.empty()) return {};
        const auto& inst = instrumentById (p->instrument);
        if (fit == Fit::instrument && options.fitToInstrument) notes = fitToInstrument (notes, inst);
        if (fit == Fit::fold) notes = shiftAndFold (notes, 0, inst);
        if (options.fitPolyphony)
        {
            int dropped = 0;
            notes = fitToPolyphony (notes, inst, &dropped);
            if (dropped > 0) report.thinnedParts.push_back (partId);
        }
        const auto ids = pasteNotes (score, partId, from, notes, to - from, true);
        report.newNotes.insert (ids.begin(), ids.end());
        return notes;
    };

    // The parts that stand alone: chords, drums, timpani, glockenspiel...
    bool usedSecond = false, usedChords = false, usedDrums = false, usedTune = false, usedBass = false;
    auto standsAlone = [&] (const Assignment& a)
    {
        const auto* p = score.partById (a.partId);
        if (p == nullptr) return true;
        const auto& inst = instrumentById (p->instrument);
        return inst.drums || inst.poly >= 2 || a.layer == Layer::chords || a.layer == Layer::drums || a.layer == Layer::timpani;
    };
    for (const auto& a : plan)
    {
        if (! standsAlone (a) || score.partById (a.partId) == nullptr) continue;
        switch (a.layer)
        {
            case Layer::chords: put (a.partId, chords->notes, Fit::instrument); usedChords = true; break;
            case Layer::drums: put (a.partId, drums->notes, Fit::asIs); usedDrums = true; break;
            case Layer::melody: put (a.partId, tune, Fit::instrument); usedTune = true; break;
            case Layer::bass: put (a.partId, low, Fit::instrument); usedBass = true; break;
            case Layer::timpani:
            {
                // The bass where the harmony changes, in the drums' compass.
                std::vector<Note> roots;
                for (const auto& h : harmony)
                    for (const auto& n : low)
                        if (n.start >= h.start && n.start < h.end) { roots.push_back (n); roots.back().length = std::min (n.length, h.end - n.start); break; }
                if (harmony.empty()) roots = low;
                put (a.partId, roots, Fit::fold);
                break;
            }
            case Layer::none: case Layer::melodyDouble: case Layer::second: case Layer::inner: case Layer::bassDouble: break;
        }
    }

    // The sections, by family as planOrchestra grouped them (the sections of
    // one together).
    std::map<std::string, std::vector<Assignment>> byFamily;
    std::vector<std::string> order;
    {
        std::map<std::string, int> sizes;
        for (const auto& a : plan)
        {
            const auto* p = score.partById (a.partId);
            if (p == nullptr) continue;
            if (standsAlone (a)) continue;
            ++sizes[instrumentById (p->instrument).family];
        }
        for (const auto& a : plan)
        {
            const auto* p = score.partById (a.partId);
            if (p == nullptr) continue;
            if (standsAlone (a)) continue;
            const auto& inst = instrumentById (p->instrument);
            const std::string key = sizes[inst.family] == 1 ? std::string() : inst.family;
            if (byFamily.find (key) == byFamily.end()) order.push_back (key);
            byFamily[key].push_back (a);
        }
    }

    for (const auto& key : order)
    {
        const auto& sec = byFamily[key];
        std::vector<Note> above, below;
        // The tune and its doublings first, so the inner parts go under them.
        for (const auto& a : sec)
            if (a.layer == Layer::melody) { above = put (a.partId, tune, Fit::instrument); usedTune = true; }
        if (above.empty()) above = tune;
        const auto tuneLine = above;    // the inner parts stay under the tune, not under its doublings
        for (const auto& a : sec)
        {
            const auto& inst = instrumentById (score.partById (a.partId)->instrument);
            if (a.layer == Layer::melodyDouble)
            {
                const auto line = put (a.partId, shiftAndFold (above, -12, inst), Fit::asIs);
                if (! line.empty()) above = line;
                usedTune = true;
            }
            if (a.layer == Layer::second)
            {
                const auto line = put (a.partId, second->notes, Fit::instrument);
                if (! line.empty()) above = line;
                usedSecond = true;
            }
        }
        for (const auto& a : sec)
            if (a.layer == Layer::bass) { below = put (a.partId, low, Fit::instrument); usedBass = true; }
        if (below.empty()) below = low;
        for (const auto& a : sec)
            if (a.layer == Layer::bassDouble)
                put (a.partId, shiftAndFold (below, -12, instrumentById (score.partById (a.partId)->instrument)), Fit::asIs);

        std::vector<uint32_t> innerIds;
        std::vector<const Instrument*> innerInsts;
        for (const auto& a : sec)
            if (a.layer == Layer::inner)
            {
                innerIds.push_back (a.partId);
                innerInsts.push_back (&instrumentById (score.partById (a.partId)->instrument));
            }
        if (! innerIds.empty())
        {
            const auto lines = voiceInner (harmony, innerInsts, tuneLine, below, chords->notes);
            for (size_t v = 0; v < innerIds.size(); ++v) put (innerIds[v], lines[v], Fit::asIs);
            usedChords = true;
        }
    }

    // Whatever found no chosen part is left out and named: no part is added.
    if (melody != nullptr && ! usedTune) report.unplaced.push_back (melody->name);
    if (second != nullptr && ! usedSecond) report.unplaced.push_back (second->name);
    if (bass != nullptr && ! usedBass) report.unplaced.push_back (bass->name);
    if (chords != nullptr && ! usedChords && ! usedTune && ! usedBass) report.unplaced.push_back (chords->name);
    if (drums != nullptr && ! usedDrums) report.unplaced.push_back (drums->name);
    for (const auto* gp : others) report.unplaced.push_back (gp->name);
    score.fitBars();
    return report;
}

} // namespace nt
