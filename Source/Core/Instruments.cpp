#include "Instruments.h"

namespace nt
{

namespace
{
Instrument make (std::string id, std::string name, std::string shortName, std::string family,
                 int low, int high, int sweetLow, int sweetHigh, char role, int poly, double fast,
                 int leap, bool breath, int program, std::vector<Clef> staves, CCShape cc,
                 std::string catalogueId)
{
    Instrument i;
    i.id = std::move (id);
    i.name = std::move (name);
    i.shortName = std::move (shortName);
    i.family = std::move (family);
    i.low = low; i.high = high;
    i.sweetLow = sweetLow; i.sweetHigh = sweetHigh;
    i.role = role; i.poly = poly; i.fast = fast; i.leap = leap; i.breath = breath;
    i.program = program;
    i.staves = std::move (staves);
    i.cc = cc;
    i.catalogueId = std::move (catalogueId);
    return i;
}

Instrument transposing (Instrument i, int octave, int transposition, std::string inName)
{
    i.octave = octave;
    i.transposition = transposition;
    i.transposedName = std::move (inName);
    return i;
}

std::vector<Instrument> build()
{
    using C = Clef;
    using S = CCShape;
    const std::vector<C> grand { C::treble, C::bass };

    std::vector<Instrument> list;
    auto add = [&list] (Instrument i) { list.push_back (std::move (i)); };

    //            id      name            short    family        low high  sweet    role poly fast  leap breath GM   staves  cc  catalogue
    // Strings: mc_orchestra.lua's numbers, unchanged.
    add (make ("vln1", "Violin I",      "Vln. I",  "Strings",     55, 100, 67, 91, 'S', 1, 0.09, 12, false, 40, { C::treble }, S::strings, "vln1"));
    add (make ("vln2", "Violin II",     "Vln. II", "Strings",     55,  96, 60, 84, 'A', 1, 0.09, 12, false, 40, { C::treble }, S::strings, "vln2"));
    add (make ("vla",  "Viola",         "Vla.",    "Strings",     48,  88, 52, 76, 'T', 1, 0.10, 12, false, 41, { C::alto },   S::strings, "vla"));
    add (make ("vc",   "Cello",         "Vc.",     "Strings",     36,  81, 40, 67, 'B', 1, 0.10, 12, false, 42, { C::bass },   S::strings, "vc"));
    add (transposing (make ("cb", "Double Bass", "Cb.", "Strings", 28, 60, 28, 50, 'B', 1, 0.18, 9, false, 43, { C::bass }, S::strings, "cb"), 12, 0, {}));

    // Woodwind
    add (transposing (make ("picc", "Piccolo", "Picc.", "Woodwind", 74, 108, 79, 100, 'S', 1, 0.09, 12, true, 72, { C::treble }, S::woodwinds, "picc"), -12, 0, {}));
    add (make ("fl",   "Flute",         "Fl.",     "Woodwind",    60,  96, 67, 91, 'S', 1, 0.09, 12, true,  73, { C::treble }, S::woodwinds, "fl"));
    add (make ("ob",   "Oboe",          "Ob.",     "Woodwind",    58,  91, 62, 84, 'S', 1, 0.11, 10, true,  68, { C::treble }, S::woodwinds, "ob"));
    add (transposing (make ("eh", "English Horn", "E.H.", "Woodwind", 52, 81, 55, 74, 'A', 1, 0.12, 10, true, 69, { C::treble }, S::woodwinds, "eh"), 0, 7, "in F"));
    add (transposing (make ("cl", "Clarinet", "Cl.", "Woodwind",  50,  91, 55, 84, 'A', 1, 0.09, 12, true,  71, { C::treble }, S::woodwinds, "cl"), 0, 2, "in Bb"));
    add (transposing (make ("bcl", "Bass Clarinet", "B. Cl.", "Woodwind", 38, 74, 40, 65, 'B', 1, 0.12, 12, true, 71, { C::bass }, S::woodwinds, "bcl"), 0, 2, "in Bb"));   // bass clef, German system: a tone up
    add (make ("bsn",  "Bassoon",       "Bsn.",    "Woodwind",    34,  76, 38, 67, 'B', 1, 0.11, 12, true,  70, { C::bass },   S::woodwinds, "bsn"));
    add (transposing (make ("cbsn", "Contrabassoon", "Cbsn.", "Woodwind", 22, 53, 26, 48, 'B', 1, 0.25, 9, true, 70, { C::bass }, S::woodwinds, "cbsn"), 12, 0, {}));
    add (transposing (make ("asax", "Alto Saxophone", "A. Sax.", "Woodwind", 49, 80, 53, 76, 'A', 1, 0.09, 12, true, 65, { C::treble }, S::woodwinds, {}), 0, 9, "in Eb"));
    add (transposing (make ("tsax", "Tenor Saxophone", "T. Sax.", "Woodwind", 44, 75, 48, 70, 'T', 1, 0.09, 12, true, 66, { C::treble8vb }, S::woodwinds, {}), 12, 2, "in Bb"));
    // Sounds an octave and a sixth below what is written; set like the tenor,
    // an octave up on an octave clef, so the concert score reads true.
    add (transposing (make ("bsax", "Baritone Saxophone", "Bari. Sax.", "Woodwind", 36, 69, 40, 62, 'B', 1, 0.10, 12, true, 67, { C::treble8vb }, S::woodwinds, {}), 12, 9, "in Eb"));

    // Brass
    add (transposing (make ("hn", "Horn", "Hn.", "Brass",          35,  77, 48, 72, 'A', 1, 0.14,  9, true,  60, { C::treble }, S::brass, "hn"), 0, 7, "in F"));
    add (transposing (make ("tpt", "Trumpet", "Tpt.", "Brass",     52,  82, 60, 79, 'S', 1, 0.10,  9, true,  56, { C::treble }, S::brass, "tpt"), 0, 2, "in Bb"));
    add (make ("tbn",  "Trombone",      "Tbn.",    "Brass",       40,  72, 45, 67, 'T', 1, 0.17,  7, true,  57, { C::bass },   S::brass, "tbn"));
    add (make ("btbn", "Bass Trombone", "B. Tbn.", "Brass",       34,  65, 36, 60, 'B', 1, 0.20,  7, true,  57, { C::bass },   S::brass, "btbn"));
    add (make ("tuba", "Tuba",          "Tba.",    "Brass",       28,  58, 31, 53, 'B', 1, 0.20,  7, true,  58, { C::bass },   S::brass, "tuba"));

    // Percussion
    add (make ("timp", "Timpani",       "Timp.",   "Percussion",  40,  56, 40, 56, 'B', 2, 0.09, 12, false, 47, { C::bass },   S::none, "timp"));
    add (transposing (make ("glock", "Glockenspiel", "Glock.", "Percussion", 79, 108, 79, 103, 'S', 2, 0.13, 12, false, 9, { C::treble }, S::none, "glock"), -24, 0, {}));
    add (transposing (make ("xyl", "Xylophone", "Xyl.", "Percussion", 65, 108, 67, 100, 'S', 2, 0.09, 12, false, 13, { C::treble }, S::none, "xyl"), -12, 0, {}));
    add (make ("mar",  "Marimba",       "Mar.",    "Percussion",  45,  96, 48, 88, 'A', 4, 0.09, 12, false, 12, grand,           S::none, "mar"));
    add (make ("vib",  "Vibraphone",    "Vib.",    "Percussion",  53,  89, 55, 84, 'A', 4, 0.09, 12, false, 11, { C::treble }, S::none, {}));
    {
        auto kit = make ("kit", "Drum Kit", "Dr.", "Percussion",  35,  81, 35, 81, 'B', 4, 0.05, 127, false, 0, { C::percussion }, S::none, {});
        kit.drums = true;
        add (kit);
    }

    // Keys and harp
    add (make ("harp", "Harp",          "Hp.",     "Keys & Harp", 23, 102, 36, 84, 'A', 4, 0.11, 12, false, 46, grand,           S::none, "harp"));
    add (transposing (make ("cel", "Celesta", "Cel.", "Keys & Harp", 60, 108, 67, 96, 'S', 4, 0.09, 12, false, 8, grand, S::none, "cel"), -12, 0, {}));
    add (make ("pno",  "Piano",         "Pno.",    "Keys & Harp", 21, 108, 43, 84, 'A', 6, 0.07, 12, false,  0, grand,           S::none, "pno"));

    // Voices: the ranges a choir is written for, the singer's own comfort
    // inside them as the sweet spot.
    add (make ("sop",  "Soprano",       "S.",      "Voices",      60,  81, 64, 77, 'S', 1, 0.15,  9, true,  52, { C::treble }, S::neutral, {}));
    add (make ("alto", "Alto",          "A.",      "Voices",      53,  74, 57, 69, 'A', 1, 0.15,  9, true,  52, { C::treble }, S::neutral, {}));
    add (transposing (make ("ten", "Tenor", "T.", "Voices",       48,  69, 50, 65, 'T', 1, 0.15,  9, true,  52, { C::treble8vb }, S::neutral, {}), 12, 0, {}));
    add (make ("bass", "Bass",          "B.",      "Voices",      40,  64, 43, 60, 'B', 1, 0.15,  9, true,  52, { C::bass },   S::neutral, {}));

    // Band
    add (transposing (make ("gtr", "Guitar", "Gtr.", "Band",      40,  83, 45, 76, 'A', 6, 0.08, 12, false, 24, { C::treble8vb }, S::none, {}), 12, 0, {}));
    add (transposing (make ("ebass", "Bass Guitar", "Bass", "Band", 28, 67, 28, 55, 'B', 1, 0.09, 12, false, 33, { C::bass }, S::none, {}), 12, 0, {}));
    // The jazz double bass: plucked, so General MIDI's acoustic bass, not the
    // bowed contrabass of the orchestra.
    add (transposing (make ("ubass", "Upright Bass", "U. Bass", "Band", 28, 67, 28, 55, 'B', 1, 0.10, 12, false, 32, { C::bass }, S::none, {}), 12, 0, {}));

    return list;
}
} // namespace

const std::vector<Instrument>& instruments()
{
    static const std::vector<Instrument> list = build();
    return list;
}

bool hasInstrument (const std::string& id)
{
    for (const auto& i : instruments())
        if (i.id == id) return true;
    return false;
}

const Instrument& instrumentById (const std::string& id)
{
    for (const auto& i : instruments())
        if (i.id == id) return i;
    for (const auto& i : instruments())
        if (i.id == "pno") return i;
    return instruments().front();
}

const std::vector<std::string>& instrumentFamilies()
{
    static const std::vector<std::string> families {
        "Strings", "Woodwind", "Brass", "Percussion", "Keys & Harp", "Voices", "Band" };
    return families;
}

} // namespace nt
