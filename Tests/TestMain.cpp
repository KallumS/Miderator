// Runs every TEST in the binary, or those whose name contains argv[1].
#include "Check.h"

#include <cstring>

int main (int argc, char** argv)
{
    int ran = 0;
    for (const auto& t : check::all())
    {
        if (argc > 1 && std::strstr (t.name, argv[1]) == nullptr) continue;
        check::current() = t.name;
        t.fn();
        ++ran;
    }
    if (check::failures() > 0)
    {
        std::printf ("%d failure(s) in %d tests\n", check::failures(), ran);
        return 1;
    }
    std::printf ("%d tests passed\n", ran);
    return 0;
}
