#include "Templates.h"

#include "Instruments.h"

namespace nt
{

const std::vector<ScoreTemplate>& scoreTemplates()
{
    static const std::vector<ScoreTemplate> list {
        { "Piano", "Solo and small", "Piano", { "pno" } },
        { "String Quartet", "Solo and small", "Two violins, viola, cello", { "vln1", "vln2", "vla", "vc" } },
        { "Wind Quintet", "Solo and small", "Flute, oboe, clarinet, horn, bassoon", { "fl", "ob", "cl", "hn", "bsn" } },
        { "Brass Quintet", "Solo and small", "Two trumpets, horn, trombone, tuba", { "tpt", "tpt", "hn", "tbn", "tuba" } },
        { "Choir", "Solo and small", "Soprano, alto, tenor, bass", { "sop", "alto", "ten", "bass" } },
        { "Band", "Solo and small", "Piano, guitar, bass guitar, drums", { "pno", "gtr", "ebass", "kit" } },
        { "Jazz Combo", "Solo and small", "Trumpet, tenor sax, piano, upright bass, drums", { "tpt", "tsax", "pno", "ubass", "kit" } },

        { "Full Strings", "Orchestral sections", "Violins I and II, violas, cellos, double basses",
          { "vln1", "vln2", "vla", "vc", "cb" } },
        { "Full Woodwinds", "Orchestral sections", "Piccolo, flutes, oboes, cor anglais, clarinets, bass clarinet, bassoons, contrabassoon",
          { "picc", "fl", "fl", "ob", "ob", "eh", "cl", "cl", "bcl", "bsn", "bsn", "cbsn" } },
        { "Full Brass", "Orchestral sections", "Four horns, three trumpets, two trombones, bass trombone, tuba",
          { "hn", "hn", "hn", "hn", "tpt", "tpt", "tpt", "tbn", "tbn", "btbn", "tuba" } },
        { "Orchestral Percussion", "Orchestral sections", "Timpani, glockenspiel, xylophone, marimba, vibraphone",
          { "timp", "glock", "xyl", "mar", "vib" } },

        { "Chamber Orchestra", "Orchestras and bands", "Single winds, horn, trumpet, timpani, strings",
          { "fl", "ob", "cl", "bsn", "hn", "tpt", "timp", "vln1", "vln2", "vla", "vc", "cb" } },
        { "Full Orchestra", "Orchestras and bands", "Every section: woodwinds in pairs, brass, timpani, glockenspiel, harp, strings",
          { "picc", "fl", "fl", "ob", "ob", "cl", "cl", "bsn", "bsn",
            "hn", "hn", "hn", "hn", "tpt", "tpt", "tpt", "tbn", "tbn", "btbn", "tuba",
            "timp", "glock", "harp",
            "vln1", "vln2", "vla", "vc", "cb" } },
        { "Big Band", "Orchestras and bands", "Five saxes, four trumpets, four trombones, guitar, piano, bass, drums",
          { "asax", "asax", "tsax", "tsax", "bsax", "tpt", "tpt", "tpt", "tpt", "tbn", "tbn", "tbn", "btbn",
            "gtr", "pno", "ubass", "kit" } },

        { "Empty", "", "No instruments: add them in the Parts tab", {} },
    };
    return list;
}

const ScoreTemplate* templateByName (const std::string& name)
{
    for (const auto& t : scoreTemplates())
        if (t.name == name) return &t;
    return nullptr;
}

Score scoreFromTemplate (const ScoreTemplate& t)
{
    Score score;
    score.title = t.instruments.empty() ? "Untitled" : t.name;
    score.tempos = { { 0, 100.0 } };
    for (const auto& id : t.instruments)
    {
        int count = 0, seen = 0;
        for (const auto& other : t.instruments) count += other == id ? 1 : 0;
        for (const auto& p : score.parts) seen += p.instrument == id ? 1 : 0;
        Part p;
        p.id = score.newId();
        p.instrument = id;
        p.name = instrumentById (id).name + (count > 1 ? " " + std::to_string (seen + 1) : std::string());
        score.parts.push_back (p);
    }
    score.normalise();
    return score;
}

} // namespace nt
