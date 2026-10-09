#include "Spelling.h"

#include <cmath>

namespace nt
{

namespace
{
constexpr std::array<int, 7> letterPc { 0, 2, 4, 5, 7, 9, 11 };
constexpr std::array<int, 7> sharpOrder { 3, 0, 4, 1, 5, 2, 6 };   // F C G D A E B
constexpr std::array<int, 7> flatOrder  { 6, 2, 5, 1, 4, 0, 3 };   // B E A D G C F

// Which letter each pitch class takes when nothing else decides: a sharp key
// and a flat key disagree about all five black notes.
constexpr std::array<std::array<int, 2>, 12> sharpSpell {{
    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 3, 0 },
    { 3, 1 }, { 4, 0 }, { 4, 1 }, { 5, 0 }, { 5, 1 }, { 6, 0 } }};
constexpr std::array<std::array<int, 2>, 12> flatSpell {{
    { 0, 0 }, { 1, -1 }, { 1, 0 }, { 2, -1 }, { 2, 0 }, { 3, 0 },
    { 4, -1 }, { 4, 0 }, { 5, -1 }, { 5, 0 }, { 6, -1 }, { 6, 0 } }};

int wrapAccidental (int acc)
{
    if (acc > 6) acc -= 12;
    if (acc < -6) acc += 12;
    return acc;
}

KeySignature makeSignature (int count, bool flats)
{
    KeySignature sig;
    sig.count = count;
    sig.flats = flats && count > 0;
    const auto& order = sig.flats ? flatOrder : sharpOrder;
    for (int i = 0; i < std::min (count, 7); ++i)
        sig.map[static_cast<size_t> (order[static_cast<size_t> (i)])] = sig.flats ? -1 : 1;
    return sig;
}
} // namespace

KeySignature signatureFromFifths (int fifths)
{
    fifths = std::clamp (fifths, -7, 7);
    return makeSignature (std::abs (fifths), fifths < 0);
}

// Each of the fifteen signatures scored against what the scale spells, and
// the one leaving fewest accidentals on the page wins. Ties go to the smaller
// signature, then to the side the tonic leans, so C# major is seven sharps
// and Db major five flats.
KeySignature chooseSignature (const std::vector<std::pair<int, int>>& spelled, int tonicAccidental)
{
    KeySignature best;
    int bestScore = 0;
    bool have = false;
    for (int count = 0; count <= 7; ++count)
    {
        for (bool flats : { false, true })
        {
            if (count == 0 && flats) continue;
            const auto sig = makeSignature (count, flats);
            int score = 0;
            for (const auto& [letter, acc] : spelled)
                score += (sig.map[static_cast<size_t> (letter)] == acc) ? 1 : -1;
            const bool better = ! have || score > bestScore
                || (score == bestScore && count < best.count)
                || (score == bestScore && count == best.count && tonicAccidental < 0 && flats);
            if (better)
            {
                best = sig;
                bestScore = score;
                have = true;
            }
        }
    }
    return best;
}

KeyContext keyContext (int rootIndex, int scaleIndex)
{
    KeyContext ctx;
    rootIndex = std::clamp (rootIndex, 0, static_cast<int> (scaleview::roots.size()) - 1);
    scaleIndex = std::clamp (scaleIndex, 0, static_cast<int> (scaleview::scales.size()) - 1);
    ctx.root = rootIndex;
    ctx.scale = scaleIndex;

    const auto& root = scaleview::roots[static_cast<size_t> (rootIndex)];
    const auto& scale = scaleview::scales[static_cast<size_t> (scaleIndex)];
    const int rootPc = root.pitchClass();

    std::vector<std::pair<int, int>> spelled;
    for (size_t d = 0; d < scale.intervals.size(); ++d)
    {
        const int letter = (root.letter + scale.letterSteps[d]) % 7;
        const int pc = (rootPc + scale.intervals[d]) % 12;
        const int acc = wrapAccidental (pc - letterPc[static_cast<size_t> (letter)]);
        spelled.emplace_back (letter, acc);
        // A scale spelling two notes on one letter keeps the first: the lower
        // of the pair is the one the ear hears as the degree.
        if (! ctx.inScale[static_cast<size_t> (pc)])
        {
            ctx.inScale[static_cast<size_t> (pc)] = true;
            ctx.letter[static_cast<size_t> (pc)] = letter;
            ctx.accidental[static_cast<size_t> (pc)] = acc;
        }
    }
    ctx.signature = chooseSignature (spelled, root.accidental);
    ctx.leansFlat = scaleview::buildKey (rootIndex, scaleIndex).usesFlats
                    || (ctx.signature.flats && ctx.signature.count > 0);
    ctx.label = std::string (root.name) + " " + scale.name;
    return ctx;
}

Spelled spell (int pitch, const KeyContext& key)
{
    const int pc = ((pitch % 12) + 12) % 12;
    int letter, acc;
    if (key.inScale[static_cast<size_t> (pc)])
    {
        letter = key.letter[static_cast<size_t> (pc)];
        acc = key.accidental[static_cast<size_t> (pc)];
    }
    else
    {
        const auto& table = key.leansFlat ? flatSpell : sharpSpell;
        letter = table[static_cast<size_t> (pc)][0];
        acc = table[static_cast<size_t> (pc)][1];
    }
    // The written octave follows the letter, not the sounding pitch, or Cb4
    // would be filed an octave below the C it is written on.
    const int pitchWithoutAcc = pitch - acc;
    const int octave = static_cast<int> (std::floor (pitchWithoutAcc / 12.0)) - 1;
    Spelled s;
    s.letter = letter;
    s.accidental = acc;
    s.octave = octave;
    s.step = octave * 7 + letter;
    return s;
}

std::string stepName (int step)
{
    const int letter = ((step % 7) + 7) % 7;
    const int octave = static_cast<int> (std::floor (step / 7.0));
    return std::string (scaleview::letters[static_cast<size_t> (letter)]) + std::to_string (octave);
}

std::string pitchName (int pitch, const KeyContext& key)
{
    const auto s = spell (pitch, key);
    std::string name = scaleview::letters[static_cast<size_t> (s.letter)];
    switch (s.accidental)
    {
        case -2: name += "bb"; break;
        case -1: name += "b"; break;
        case 1: name += "#"; break;
        case 2: name += "x"; break;
        default: break;
    }
    return name + std::to_string (s.octave);
}

int clefBottomStep (Clef clef)
{
    switch (clef)
    {
        case Clef::treble:     return 30;   // E4
        case Clef::treble8vb:  return 30;   // the octave is the instrument's, not the clef's
        case Clef::bass:       return 18;   // G2
        case Clef::alto:       return 24;   // F3
        case Clef::tenor:      return 22;   // D3
        case Clef::percussion: return 30;
    }
    return 30;
}

int rootIndexByName (const std::string& name)
{
    for (size_t i = 0; i < scaleview::roots.size(); ++i)
        if (name == scaleview::roots[i].name) return static_cast<int> (i);
    return -1;
}

int scaleIndexByName (const std::string& name)
{
    for (size_t i = 0; i < scaleview::scales.size(); ++i)
        if (name == scaleview::scales[i].name) return static_cast<int> (i);
    return -1;
}

} // namespace nt
