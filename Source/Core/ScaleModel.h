/*
    ScaleModel - the musical core of ScaleView, with no dependency on JUCE or
    on any host, so it can be unit tested on its own.

    Note names are spelled for the key that is selected rather than always
    being sharps or flats: each degree of a seven-note scale takes the next
    letter of the alphabet and whatever accidental that letter then needs.
    C# major reads C# D# E# F# G# A# B#, while Db major - the same seven
    notes - reads Db Eb F Gb Ab Bb C.

    Ported from the ScaleView ReaScripts for REAPER.
*/

#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace scaleview
{

// Pitch classes: 0 = C ... 11 = B. These plain names are used for the notes
// that are outside the selected scale, where the key implies no spelling.
inline constexpr std::array<const char*, 12> sharpNames {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline constexpr std::array<const char*, 12> flatNames {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

// The seven letters and the pitch class each one names on its own.
inline constexpr std::array<const char*, 7> letters { "C", "D", "E", "F", "G", "A", "B" };
inline constexpr std::array<int, 7>         letterPitches { 0, 2, 4, 5, 7, 9, 11 };

// Which circle sits where: five black keys on the top row, seven white on the
// bottom, the black ones over the gaps between the white keys they sit between.
inline constexpr std::array<int, 5> blackPitches { 1, 3, 6, 8, 10 };
inline constexpr std::array<int, 7> whitePitches { 0, 2, 4, 5, 7, 9, 11 };
inline constexpr std::array<int, 5> blackSlots   { 1, 2, 4, 5, 6 };

struct Root
{
    const char* name;
    int letter;       // index into letters
    int accidental;   // semitones, -1 flat, +1 sharp

    int pitchClass() const
    {
        return ((letterPitches[static_cast<size_t> (letter)] + accidental) % 12 + 12) % 12;
    }
};

/*  Both spellings of every pitch class, plus Cb. C# major and Db major are the
    same seven notes spelled differently, so each needs its own entry.
*/
inline const std::vector<Root> roots {
    { "C",  0,  0 }, { "C#", 0,  1 }, { "Db", 1, -1 }, { "D",  1,  0 },
    { "D#", 1,  1 }, { "Eb", 2, -1 }, { "E",  2,  0 }, { "F",  3,  0 },
    { "F#", 3,  1 }, { "Gb", 4, -1 }, { "G",  4,  0 }, { "G#", 4,  1 },
    { "Ab", 5, -1 }, { "A",  5,  0 }, { "A#", 5,  1 }, { "Bb", 6, -1 },
    { "B",  6,  0 }, { "Cb", 0, -1 },
};

/*  intervals are semitones from the root; letterSteps is how many letter names
    each degree sits above the root letter, which is what makes the spelling
    come out right. A seven-note scale walks the letters in order; the others
    follow the conventional spelling, so major blues repeats a letter for its
    b3 and 3 (C D Eb E G A) and the diminished scales repeat one across their
    eight notes.
*/
struct Scale
{
    const char* name;
    std::vector<int> intervals;
    std::vector<int> letterSteps;
};

inline const std::vector<Scale> scales {
    { "Major",            { 0, 2, 4, 5, 7, 9, 11 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Minor (Natural)",  { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Ionian",           { 0, 2, 4, 5, 7, 9, 11 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Aeolian",          { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
    { "Major Pentatonic", { 0, 2, 4, 7, 9 },        { 0, 1, 2, 4, 5 } },
    { "Minor Pentatonic", { 0, 3, 5, 7, 10 },       { 0, 2, 3, 4, 6 } },
    { "Major Blues",      { 0, 2, 3, 4, 7, 9 },     { 0, 1, 2, 2, 4, 5 } },
    { "Minor Blues",      { 0, 3, 5, 6, 7, 10 },    { 0, 2, 3, 4, 4, 6 } },
    { "Whole Tone",       { 0, 2, 4, 6, 8, 10 },    { 0, 1, 2, 3, 4, 5 } },
    { "Diminished Whole-Half", { 0, 2, 3, 5, 6, 8, 9, 11 }, { 0, 1, 2, 3, 4, 5, 5, 6 } },
    { "Diminished Half-Whole", { 0, 1, 3, 4, 6, 7, 9, 10 }, { 0, 1, 2, 2, 3, 4, 5, 6 } },
};

// Highlight colours offered in the menu; the first is the default. Keep these
// pale: the note names drawn on top of them are dark.
struct Highlight
{
    const char* name;
    int red, green, blue;   // 0..255

    double luminance() const
    {
        return (0.2126 * red + 0.7152 * green + 0.0722 * blue) / 255.0;
    }
};

/*  Deliberately nothing the ring around a played note could hide in: that ring
    is #FFF200, so a highlight of the same colour would swallow it where the two
    touch. White is the nearest the palette comes, at 1.17:1 - the tightest the
    margin has been, and only on the antialiased edge, since both ring strokes
    are drawn outside the filled circle. White was barred while the ring was
    white and became free when the ring turned yellow; Gold, which it replaced,
    resolves to no index and falls back to the default, which is tested.
*/
inline const std::vector<Highlight> highlights {
    { "Teal",        51, 204, 158 },
    { "Orange",     250, 140,  38 },
    { "Light Green", 140, 222, 102 },
    { "Light Blue", 102, 184, 250 },
    { "Light Pink", 250, 158, 199 },
    { "White",      255, 255, 255 },
};

/*  Spells pitch class pc using the given letter, e.g. letter G and pc 6 gives
    "Gb". Returns an empty string when that would need more than a double
    accidental, which only happens in spellings nobody writes.
*/
inline std::string spellAs (int letter, int pitchClass)
{
    const auto index = static_cast<size_t> (((letter % 7) + 7) % 7);
    const std::string name { letters[index] };

    const int offset = ((pitchClass - letterPitches[index] + 6) % 12 + 12) % 12 - 6;

    switch (offset)
    {
        case -2: return name + "bb";
        case -1: return name + "b";
        case  0: return name;
        case  1: return name + "#";
        case  2: return name + "x";
        default: return {};
    }
}

/*  Which of the twelve circles are lit, and how each one is named, for one
    key. A default-constructed Key is the "no scale selected" state: nothing
    lit, plain sharp names.
*/
struct Key
{
    std::array<bool, 12> lit {};
    std::array<bool, 12> tonic {};  // its 1st, 3rd and 5th degrees, for the bass
    std::array<std::string, 12> names {};
    std::string label { "No scale selected" };
    bool hasScale { false };
    bool usesFlats { false };   // which way the key leans, for chord symbols

    Key()
    {
        for (int pc = 0; pc < 12; ++pc)
            names[static_cast<size_t> (pc)] = sharpNames[static_cast<size_t> (pc)];
    }
};

inline Key buildKey (int rootIndex, int scaleIndex)
{
    Key key;

    if (rootIndex < 0 || rootIndex >= static_cast<int> (roots.size())
        || scaleIndex < 0 || scaleIndex >= static_cast<int> (scales.size()))
        return key;

    const auto& root  = roots[static_cast<size_t> (rootIndex)];
    const auto& scale = scales[static_cast<size_t> (scaleIndex)];
    const int rootPc  = root.pitchClass();

    std::array<bool, 12> spelled {};
    int sharps = 0, flats = 0;

    for (size_t degree = 0; degree < scale.intervals.size(); ++degree)
    {
        const int pc = (rootPc + scale.intervals[degree]) % 12;
        const auto name = spellAs (root.letter + scale.letterSteps[degree], pc);

        key.lit[static_cast<size_t> (pc)] = true;
        if (degree == 0 || degree == 2 || degree == 4)   // the 1st, 3rd and 5th
            key.tonic[static_cast<size_t> (pc)] = true;

        if (! name.empty())
        {
            key.names[static_cast<size_t> (pc)] = name;
            spelled[static_cast<size_t> (pc)] = true;

            if (name.find ('#') != std::string::npos || name.find ('x') != std::string::npos)
                ++sharps;
            if (name.find ('b', 1) != std::string::npos)   // skip the note letter B
                ++flats;
        }
    }

    // The notes outside the scale have no spelling of their own, so name them
    // in whichever direction the key leans.
    key.usesFlats = flats > sharps;
    const auto& outside = key.usesFlats ? flatNames : sharpNames;

    for (size_t pc = 0; pc < 12; ++pc)
        if (! spelled[pc])
            key.names[pc] = outside[pc];

    key.label = std::string (root.name) + " " + scale.name;
    key.hasScale = true;
    return key;
}

/*  Chord naming, built rather than looked up.

    A chord symbol has two halves. The bottom half - third, fifth and seventh -
    is a closed vocabulary: those three can only combine so many ways, and each
    combination has a name musicians agree on. That half is a table, ordered by
    how common the quality is.

    The top half - sixths, ninths, elevenths, thirteenths and their alterations
    - is not closed, so it is described rather than matched. Whatever the bottom
    half did not account for is read off as an extension, which is why a voicing
    nobody thought to tabulate still comes out with a name.

    This is a port of the ReaScript's engine and must stay in step with it, line
    for line. Every weight below was arrived at by breaking a test; see that
    repository's CLAUDE.md for what each one is holding up.
*/
inline const std::map<std::string, int> coreRank {
    { "maj/P/none",   1 }, { "min/P/none",   2 },
    { "maj/P/b7",     3 }, { "min/P/b7",     4 }, { "maj/P/maj7",  5 },
    { "min/b/b7",     6 }, { "min/b/bb7",    7 }, { "min/b/none",  8 },
    { "maj/#/none",   9 },
    { "sus4/P/none", 10 }, { "sus2/P/none", 11 },
    { "min/P/maj7",  12 },
    { "maj/#/b7",    13 }, { "maj/b/b7",    14 }, { "maj/#/maj7", 15 },
    /*  maj7b5 belongs in this list and was missing from it: every altered
        fifth carrying a seventh that musicians play is named here, and
        players write maj7b5 constantly as the Lydian tonic. Without it the
        quality paid rankUnnamed plus rankTwiceOdd, 37 before any slash, and
        lost to readings with no third in them at all. */
    { "maj/b/maj7",  16 },
    { "sus4/P/b7",   17 }, { "sus2/P/b7",   18 },
    { "sus4/P/maj7", 19 }, { "sus2/P/maj7", 20 },

    /*  A missing third is a different chord, not a thinner one, so these sit
        well below anything with a third in it - and by more than a slash
        costs. A third-less shape over a perfect fifth is a real voicing, but
        one whose fifth is also altered is odd twice over, exactly as
        rankTwiceOdd has it for the qualities that keep their third. Those two
        sit past costInversion from an unnamed quality with a third (25), so
        D E A# reads Bb(b5)/E rather than E7b5(no3). The perfect-fifth pair
        stay where they were, which keeps C G Bb reading C7(no3). */
    { "none/P/b7",   30 }, { "none/P/maj7", 31 }, { "none/P/none", 34 },
    { "none/b/maj7", 41 }, { "none/b/b7",   42 },
};

//  A quality the table does not name is ranked by the interval that decides
//  most about a chord: the third.
inline const std::map<std::string, int> rankUnnamed {
    { "maj", 25 }, { "min", 25 }, { "sus4", 70 }, { "sus2", 70 }, { "none", 90 },
};

//  The handful of qualities with names of their own; the rest are built.
inline const std::map<std::string, std::string> specialNames {
    { "min/b/none",    "dim"     }, { "min/b/bb7",  "dim7"    },
    { "min/b/b7",      "min7b5"  }, { "maj/#/none", "aug"     },
    { "maj/#/b7",      "aug7"    }, { "maj/#/maj7", "maj7#5"  },
    { "maj/b/b7",      "7b5"     }, { "min/P/maj7", "minMaj7" },
    { "min/none/maj7", "minMaj7" },
};

inline constexpr int rankTwiceOdd  = 12;  // odd fifth AND odd seventh
inline constexpr int rankNoFifth   = 20;  // a triad that has lost its fifth
inline constexpr int rankNoFifth7  =  4;  // a seventh chord voiced as a shell
inline constexpr int rankEleventh  =  6;  // a sus4 carrying a seventh and a ninth
inline constexpr int costInversion = 14;  // naming the bass after a slash
inline constexpr int costNatural   =  1;  // an extension the number implies
inline constexpr int costAdd       =  2;  // one that has to be spelled out
inline constexpr int costClash     =  6;  // a natural 11th fighting a major 3rd
inline constexpr int costAltered   =  5;  // b9, #9, #11, b13 colouring a chord
inline constexpr int costClashing  = 18;  // one that does not belong there
inline constexpr int costSusExtra  = 12;  // a suspension carrying added tones
inline constexpr int costSixth     =  4;  // enough that C E A stays Amin/C
/*  A minor sixth is charged one more than any other, because it is the one
    sixth that is also something else: A C E F# is Amin6 and equally the
    half-diminished on its third, F#min7b5. The two tie exactly at 4, and the
    half-diminished is the name Scaler prints, the name jazznet's labels carry
    and the name analysts give it. */
inline constexpr int costMinSixth  =  5;

struct Core { std::string third, fifth, seventh; std::array<bool, 12> used {}; };

/*  Reading the intervals present into a third, a fifth and a seventh.

    Where both a flattened and a raised fifth are sounding, which one is *the*
    fifth is a real choice and not a lookup, so preferSharpFive lets the caller
    ask for the other reading and cost them both. C E F# G# B was named
    Cmaj7b5b13 because the flattened fifth was taken greedily, leaving the G#
    described as a b13 over a chord that is not at home with one; the other way
    round it is Cmaj7#5#11, 14 cheaper. */
inline Core readCore (const std::array<bool, 12>& has, bool preferSharpFive = false)
{
    Core c;
    c.used[0] = true;

    if      (has[4]) { c.third = "maj";  c.used[4] = true; }
    else if (has[3]) { c.third = "min";  c.used[3] = true; }
    else if (has[5]) { c.third = "sus4"; c.used[5] = true; }
    else if (has[2]) { c.third = "sus2"; c.used[2] = true; }
    else             { c.third = "none"; }

    if      (has[7]) { c.fifth = "P"; c.used[7] = true; }
    else if (has[6] && has[8] && preferSharpFive) { c.fifth = "#"; c.used[8] = true; }
    else if (has[6]) { c.fifth = "b"; c.used[6] = true; }
    else if (has[8]) { c.fifth = "#"; c.used[8] = true; }
    else             { c.fifth = "none"; }

    // A diminished triad takes the 9 as a doubly flattened seventh.
    if      (has[10]) { c.seventh = "b7";   c.used[10] = true; }
    else if (has[11]) { c.seventh = "maj7"; c.used[11] = true; }
    else if (c.third == "min" && c.fifth == "b" && has[9])
                      { c.seventh = "bb7";  c.used[9]  = true; }
    else              { c.seventh = "none"; }

    return c;
}

inline std::string coreName (const std::string& third, const std::string& fifth,
                             const std::string& seventh)
{
    const auto special = specialNames.find (third + "/" + fifth + "/" + seventh);
    if (special != specialNames.end()) return special->second;

    const std::string base = third == "min"  ? "min"
                           : third == "sus4" ? "sus4"
                           : third == "sus2" ? "sus2" : "";
    const std::string sev  = seventh == "b7"   ? "7"
                           : seventh == "bb7"  ? "dim7"
                           : seventh == "maj7" ? (third == "min" ? "Maj7" : "maj7") : "";
    const std::string alt  = fifth == "b" ? "b5" : fifth == "#" ? "#5" : "";

    // Sevenths are written before a sus, not after it: 7sus4, never sus47.
    std::string name = (third == "sus4" || third == "sus2") ? sev + base : base + sev;
    name += alt;

    // A bare altered fifth has to be bracketed or the symbol reads as a note
    // name: C(b5) is a chord on C, Cb5 looks like one on C flat.
    if (name == alt && ! alt.empty()) name = "(" + alt + ")";
    return name;
}

inline int rankOf (const std::string& third, const std::string& fifth,
                   const std::string& seventh)
{
    if (fifth == "none")
    {
        const auto it = coreRank.find (third + "/P/" + seventh);
        const int rank = it != coreRank.end() ? it->second : rankUnnamed.at (third);
        return rank + (seventh == "none" ? rankNoFifth : rankNoFifth7);
    }

    const auto it = coreRank.find (third + "/" + fifth + "/" + seventh);
    if (it != coreRank.end()) return it->second;

    return rankUnnamed.at (third) + (seventh != "none" ? rankTwiceOdd : 0);
}

//  interval -> { how it is written, whether it is an alteration, which degree }
struct Extension { const char* text; bool altered; int degree; };
inline const std::map<int, Extension> extensions {
    { 1, { "b9",  true,  0 } }, { 2, { "9",   false, 9  } },
    { 3, { "#9",  true,  0 } }, { 5, { "11",  false, 11 } },
    { 6, { "#11", true,  0 } }, { 8, { "b13", true,  0 } },
};

struct Reading { std::string name; int cost = 0; int rank = 0; };

/*  Names the shape `has` read from `root`, and prices that reading. The cost is
    the whole of the musical judgement: how unusual the quality is, what its
    extensions cost, and whether the root had to be named after a slash.
*/
inline Reading analyseAs (const std::array<bool, 12>& has, int root, int bass,
                          bool preferSharpFive)
{
    const Core c = readCore (has, preferSharpFive);
    const int rank = rankOf (c.third, c.fifth, c.seventh);

    Reading out;
    out.name = coreName (c.third, c.fifth, c.seventh);
    out.cost = rank;
    out.rank = rank;

    std::map<int, bool> naturals;
    std::vector<std::string> altered;
    bool sixth = false, asEleventh = false;

    for (size_t i = 1; i <= 11; ++i)
    {
        if (! has[i] || c.used[i]) continue;

        if (i == 9)
        {
            if (c.seventh == "none") sixth = true; else naturals[13] = true;
            continue;
        }

        const auto ext = extensions.find (static_cast<int> (i));
        if (ext == extensions.end())
        {
            // Only 11 arrives here: readCore always takes 4, 7 and 10 when
            // present and 9 was dealt with above. This is the major seventh
            // left over when a flattened one took the seventh's place.
            altered.emplace_back ("maj7");
        }
        else if (ext->second.altered) altered.emplace_back (ext->second.text);
        else naturals[ext->second.degree] = true;
    }

    if (c.seventh != "none" && c.seventh != "bb7")
    {
        /*  A stacked number claims every degree beneath it, so it may only be
            used when the ninth is actually played - Hutchinson's chord list
            prints Cm11 and Cm7(11) side by side, six noteheads against five.
            An altered ninth still fills the place, since the same list prints
            C13sus(b9) with six noteheads.
        */
        bool ninth = naturals[9];
        for (const auto& token : altered)
            if (token == "b9" || token == "#9") ninth = true;

        int number = 0;
        if      (ninth && naturals[13]) number = 13;
        else if (ninth && naturals[11] && c.third != "maj") number = 11;
        else if (naturals[9]) number = 9;

        if (c.third == "sus4" && c.seventh == "b7" && naturals[9]
            && (c.fifth == "P" || c.fifth == "none"))
        {
            // A sus4 carrying a seventh and a ninth is how an eleventh chord is
            // voiced, so it is named as one rather than as a suspension.
            out.name = "11";
            number = 11;
            asEleventh = true;
            naturals[11] = true;
            out.cost = rankEleventh + (c.fifth == "none" ? rankNoFifth7 : 0);
        }
        else if (number != 0)
        {
            const auto at = out.name.find ('7');
            if (at != std::string::npos)
                out.name = out.name.substr (0, at) + std::to_string (number)
                         + out.name.substr (at + 1);
        }

        std::vector<int> spare;
        for (const int degree : { 9, 11, 13 })
        {
            if (! naturals[degree]) continue;

            const bool implied = number != 0 && degree <= number
                                 && ! (degree == 11 && c.third == "maj");
            if (implied) out.cost += costNatural;
            else
            {
                out.cost += (degree == 11 && c.third == "maj") ? costClash : costAdd;
                spare.push_back (degree);
            }
        }

        // Everything the number did not account for, bracketed as the chord
        // lists print it: Cm7(11), C7(13), C7(11,13).
        if (! spare.empty())
        {
            std::string list;
            for (size_t i = 0; i < spare.size(); ++i)
                list += (i ? "," : "") + std::to_string (spare[i]);
            out.name += "(" + list + ")";
        }
    }
    else
    {
        if (sixth)
        {
            out.cost += (c.third == "min" && c.fifth == "P") ? costMinSixth : costSixth;

            // The sixth stands where a seventh would, so the symbol is rebuilt
            // around it. An altered fifth has to survive that: C Eb G# A is
            // min6#5, and naming it Cmin6 claims a fifth nobody played.
            const std::string mark = c.fifth == "b" ? "b5" : c.fifth == "#" ? "#5" : "";

            if (naturals[9] && (c.third == "maj" || c.third == "min"))
            {
                naturals[9] = false;
                out.name = (c.third == "min" ? "min6/9" : "6/9") + mark;
                out.cost += costAdd;
            }
            else if (c.third == "min")  out.name = "min6" + mark;
            else if (c.third == "maj")  out.name = c.fifth == "#" ? "aug6" : "6" + mark;
            else if (c.third == "none") out.name = "6" + mark;
            else                        out.name += "(add6)";
        }

        /*  A diminished seventh carrying a ninth is a dim9, not a dim7 with a
            note stuck on the end - the bb7 only sits in this branch because it
            is spelled as a sixth. Only the spelling changes: it still costs
            what an added tone costs, so which reading wins is untouched. */
        if (c.seventh == "bb7" && naturals[9])
        {
            naturals[9] = false;
            const auto at = out.name.find ("dim7");
            if (at != std::string::npos) out.name.replace (at, 4, "dim9");
            out.cost += costAdd;
        }

        for (const int degree : { 9, 11, 13 })
        {
            if (! naturals[degree]) continue;
            out.cost += (degree == 11 && c.third == "maj") ? costClash : costAdd;
            out.name += (out.name.empty() ? "add" : "Add") + std::to_string (degree);
        }
    }

    /*  An alteration has to belong to the chord under it. A b9, a #9 and a b13
        are the dominant's; a complete triad is also at home with one, which is
        how Scaler 3 reads a chord - except a b13, whose note is nearly always a
        chord tone of something plainer (E G B with a C in it is Cmaj7 inverted,
        not Emin wearing a b13).
    */
    const bool dominant = c.third == "maj" && c.seventh == "b7";
    const bool triad = (c.third == "maj" || c.third == "min") && c.fifth == "P";

    /*  An altered fifth is no bar on a dominant standing on its own root: over
        a major third and a flat seventh with the root in the bass, a b9 or #9
        over a b5 or #5 is the altered dominant - 7#5b9, 7b5#9, 7b9b13 - which
        every lead sheet writes. Only from the bass; allowing it from an
        inversion too read F A C G C# over C as Aaug7#9/C. Ported from
        ScaleView Pro, October 2026.
    */
    const bool plainFifth = c.fifth != "b" && c.fifth != "#";

    for (const auto& token : altered)
    {
        const bool athome = token != "maj7"
                            && ((dominant && (plainFifth || root == bass))
                                || (plainFifth && (token == "#11"
                                                   || (triad && token != "b13"))));

        //  A flattened sixth is a b13 only when a seventh is under it; without
        //  one it is an added flat sixth. Both sevenths at once is a cluster,
        //  so it is bracketed or the two names run together as "G7maj7".
        const std::string shown = (token == "b13" && c.seventh == "none") ? "b6"
                                : token == "maj7" ? "(maj7)" : token;

        out.cost += athome ? costAltered : costClashing;
        out.name += ((out.name.empty() && c.seventh == "none") ? "add" : "") + shown;
    }

    //  A suspension replaces the third rather than decorating it, so it does
    //  not carry added tones - "sus4 add6 add9" is not a chord anybody writes.
    if ((c.third == "sus4" || c.third == "sus2") && ! asEleventh)
    {
        int carried = static_cast<int> (altered.size());
        for (const int degree : { 9, 11, 13 }) if (naturals[degree]) ++carried;
        if (sixth) ++carried;
        out.cost += carried * costSusExtra;
    }

    //  A missing third is the one omission that has to be said out loud, and it
    //  is said last: maj7b5(no3), not maj7(no3)b5.
    if (c.third == "none")
        out.name = out.name.empty() ? "5" : out.name + "(no3)";

    if (root != bass) out.cost += costInversion;
    return out;
}

/*  With both fifths sounding and no perfect one between them, neither is
    obviously the fifth, so both readings are costed and the cheaper wins.
    Everywhere else there is nothing to choose and the second is not built. */
inline Reading analyse (const std::array<bool, 12>& has, int root, int bass)
{
    Reading reading = analyseAs (has, root, bass, false);
    if (has[6] && has[8] && ! has[7])
    {
        const Reading alt = analyseAs (has, root, bass, true);
        if (alt.cost < reading.cost) return alt;
    }
    return reading;
}

/*  A chord symbol is written with the spellings real keys are built on - the
    eighteen in `roots`. Anything else falls back to a plain name leaning the
    way the key does: the double accidentals a key like Gb minor blues produces
    (Amin/C, never Bbbmin/Dbb) and the theoretical spellings nobody builds a
    chord on, B#, E# and Fb.
*/
/*  "Simplify Note Names": names every note the way a piano key is named -
    sharps for the black keys, and no double accidentals, so Bbb reads A and Cb
    reads B. The scale stays exactly as chosen, label and lit notes included;
    only the spelling changes, which is also what the chord symbols are built
    from, so they simplify with it.
*/
inline Key simplified (Key key)
{
    for (size_t pc = 0; pc < 12; ++pc)
        key.names[pc] = sharpNames[pc];

    key.usesFlats = false;
    return key;
}

inline std::string chordNoteName (int pitchClass, const Key& key)
{
    const auto& name = key.names[static_cast<size_t> (pitchClass)];

    for (const Root& root : roots)
        if (name == root.name) return name;

    return (key.usesFlats ? flatNames : sharpNames)[static_cast<size_t> (pitchClass)];
}

/*  With no scale chosen the naming still needs a key to settle the readings
    that are a genuine draw, so it assumes C major. The assumption is invisible:
    no circle lights, and choosing C major explicitly gives identical names.
*/
inline constexpr std::array<bool, 12> assumedKey {
    true, false, true, false, true, true, false, true, false, true, false, true
};

//  The same assumption's first, third and fifth degrees - C, E and G - for the
//  doubled-degree rule below. Change the assumed key and this changes with it.
inline constexpr std::array<bool, 12> assumedTonic {
    true, false, false, false, true, false, false, true, false, false, false, false
};

/*  A doubled degree of the key claims the bass.

    The bass is the lowest note sounding - that is what a slash chord names,
    and everything else here rests on it. The one exception is a root that is a
    first, third or fifth degree of the key and is sounding in more than one
    octave: a doubled root is how a chord is voiced in root position, so the
    reading is that the player laid the chord out around its root rather than
    inverting it, and the slash comes off.

    It can only ever remove a slash, never invent one, so what follows a slash
    is always the lowest note. The cost model is untouched: the reading is
    still chosen with the lowest note as the bass, and this decides only how
    the winner is written down.
*/
inline bool readAsRootPosition (int root, int bass, const std::array<int, 12>& voices,
                                const Key& key)
{
    if (root == bass) return true;
    if (voices[static_cast<size_t> (root)] < 2) return false;
    const auto& tonic = key.hasScale ? key.tonic : assumedTonic;
    return tonic[static_cast<size_t> (root)];
}

/*  Names the chord made by the notes being held, or an empty string when
    nothing is. heldNotes are MIDI note numbers; the lowest is the bass, which
    is found separately from the root and named after a slash when they differ -
    unless the root is doubled and is a degree of the key's tonic triad, which
    reads as root position. See readAsRootPosition.
*/
inline std::string chordName (const std::vector<int>& heldNotes, const Key& key)
{
    if (heldNotes.empty()) return {};

    std::array<bool, 12> classes {};
    std::array<int, 12> voices {};   // how many octaves each pitch class sounds in
    for (const int note : heldNotes)
    {
        const auto pc = static_cast<size_t> (((note % 12) + 12) % 12);
        classes[pc] = true;
        ++voices[pc];
    }

    const int bass = ((*std::min_element (heldNotes.begin(), heldNotes.end()) % 12) + 12) % 12;

    int count = 0;
    for (const bool held : classes) if (held) ++count;
    if (count == 1) return chordNoteName (bass, key);

    const auto spellOut = [&]
    {
        std::string spelled;
        for (int pc = 0; pc < 12; ++pc)
            if (classes[static_cast<size_t> (pc)])
            {
                if (! spelled.empty()) spelled += ' ';
                spelled += chordNoteName (pc, key);
            }
        return spelled;
    };

    /*  Two notes are an interval rather than a chord, with two exceptions: the
        bare fifth, and a third.

        A third is enough to name a chord - B D is a B minor, C E a C major -
        but the missing fifth cannot be silent here the way it is in a fuller
        voicing, since printing "C" for C E would claim a G nobody is playing.
        A third also has only one reading: four semitones are a major third one
        way and a minor sixth the other, and only one of those has its third.
        Everything else two notes can be is an interval and reads out as
        notes. */
    if (count == 2)
    {
        for (int root = 0; root < 12; ++root)
            if (classes[static_cast<size_t> (root)]
                && classes[static_cast<size_t> ((root + 7) % 12)])
            {
                std::string name = chordNoteName (root, key) + "5";
                if (! readAsRootPosition (root, bass, voices, key))
                    name += "/" + chordNoteName (bass, key);
                return name;
            }

        for (int root = 0; root < 12; ++root)
        {
            if (! classes[static_cast<size_t> (root)]) continue;
            const char* quality = classes[static_cast<size_t> ((root + 4) % 12)] ? "maj"
                                : classes[static_cast<size_t> ((root + 3) % 12)] ? "min"
                                : nullptr;
            if (quality == nullptr) continue;
            std::string name = chordNoteName (root, key) + quality + "(no5)";
            if (! readAsRootPosition (root, bass, voices, key))
                name += "/" + chordNoteName (bass, key);
            return name;
        }

        return spellOut();
    }

    //  Past a certain thickness there is no chord left to find, only a cluster.
    if (count > 7) return spellOut();

    struct Best { int root = -1; std::string name; int cost = 0, rank = 0, fit = 0;
                  bool slash = false; };
    Best best;

    for (int root = 0; root < 12; ++root)
    {
        if (! classes[static_cast<size_t> (root)]) continue;

        std::array<bool, 12> has {};
        for (int pc = 0; pc < 12; ++pc)
            if (classes[static_cast<size_t> (pc)])
                has[static_cast<size_t> (((pc - root) % 12 + 12) % 12)] = true;

        const Reading reading = analyse (has, root, bass);

        //  The scale only breaks a draw: a root that is a degree of it wins,
        //  then a reading whose notes sit in it, then a reading that needs no
        //  slash (ScaleView Pro, October 2026), then the commoner quality.
        const auto& lit = key.hasScale ? key.lit : assumedKey;
        int fit = lit[static_cast<size_t> (root)] ? 100 : 0;
        for (int pc = 0; pc < 12; ++pc)
            if (classes[static_cast<size_t> (pc)] && lit[static_cast<size_t> (pc)]) ++fit;

        const bool slash = root != bass;
        if (best.root < 0
            || reading.cost < best.cost
            || (reading.cost == best.cost && fit > best.fit)
            || (reading.cost == best.cost && fit == best.fit && best.slash && ! slash)
            || (reading.cost == best.cost && fit == best.fit && slash == best.slash
                && reading.rank < best.rank))
            best = { root, reading.name, reading.cost, reading.rank, fit, slash };
    }

    std::string name = chordNoteName (best.root, key) + best.name;
    if (! readAsRootPosition (best.root, bass, voices, key))
        name += "/" + chordNoteName (bass, key);
    return name;
}

} // namespace scaleview
