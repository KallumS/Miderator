/*
    SystemSettings - what the Mac's Accessibility settings ask of every app
    (decision 0048): Increase Contrast, for stronger colours, and Reduce
    Motion, for a page that turns rather than scrolls while it plays. Read
    from NSWorkspace through the Objective-C runtime, so the file stays C++;
    elsewhere both are off.
*/

#pragma once

namespace nt::system
{

bool increaseContrast();
bool reduceMotion();

} // namespace nt::system
