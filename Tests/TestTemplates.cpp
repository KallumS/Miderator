#include "Check.h"

#include "Instruments.h"
#include "Perform.h"
#include "Templates.h"

#include <set>

using namespace nt;

TEST ("templates: every instrument in every template exists")
{
    std::set<std::string> known;
    for (const auto& i : instruments()) known.insert (i.id);
    for (const auto& t : scoreTemplates())
        for (const auto& id : t.instruments)
        {
            if (known.count (id) == 0) std::printf ("  %s: no instrument '%s'\n", t.name.c_str(), id.c_str());
            CHECK (known.count (id) != 0);
        }
    CHECK (templateByName ("Big Band") != nullptr);
    CHECK (templateByName ("Full Orchestra") != nullptr);
    CHECK (templateByName ("No such thing") == nullptr);
}

TEST ("templates: repeated instruments are numbered, single ones are not")
{
    const auto s = scoreFromTemplate (*templateByName ("Full Brass"));
    CHECK_EQ (s.parts.size(), size_t (11));
    CHECK_EQ (s.parts[0].name, std::string ("Horn 1"));
    CHECK_EQ (s.parts[3].name, std::string ("Horn 4"));
    CHECK_EQ (s.parts[6].name, std::string ("Trumpet 3"));
    CHECK_EQ (s.parts[9].name, std::string ("Bass Trombone"));
    CHECK_EQ (s.parts[10].name, std::string ("Tuba"));
    CHECK_EQ (s.title, std::string ("Full Brass"));
    std::set<uint32_t> ids;
    for (const auto& p : s.parts) ids.insert (p.id);
    CHECK_EQ (ids.size(), s.parts.size());
    CHECK_EQ (scoreFromTemplate (*templateByName ("Empty")).title, std::string ("Untitled"));
}

TEST ("templates: a big band's baritone sax is written a sixth and an octave up")
{
    const auto& bari = instrumentById ("bsax");
    CHECK_EQ (bari.id, std::string ("bsax"));
    CHECK_EQ (bari.writtenPitch (48, true), 48 + 21);
    CHECK_EQ (bari.program, 67);
}

TEST ("templates: every part of the full orchestra has a channel of its own")
{
    const auto s = scoreFromTemplate (*templateByName ("Full Orchestra"));
    CHECK_EQ (s.parts.size(), size_t (28));
    const auto ch = channelsForParts (s);
    std::set<int> used (ch.begin(), ch.end());
    CHECK_EQ (used.size(), ch.size());
    for (int c : ch)
    {
        CHECK (c % channelsPerBank != drumChannel);
        CHECK (c != liveChannel && c != previewChannel);
    }
    CHECK_EQ (banksForParts (ch), 2);

    // The big band's drums share channel 10, every other part has its own.
    const auto band = scoreFromTemplate (*templateByName ("Big Band"));
    const auto bc = channelsForParts (band);
    CHECK_EQ (bc.back(), drumChannel);
    CHECK_EQ (std::set<int> (bc.begin(), bc.end()).size(), bc.size());
    CHECK_EQ (banksForParts (bc), 2);
    CHECK_EQ (banksForParts (channelsForParts (scoreFromTemplate (*templateByName ("String Quartet")))), 1);
}
