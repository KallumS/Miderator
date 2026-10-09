#include "Perform.h"

#include "Instruments.h"

#include <algorithm>

namespace nt
{

std::vector<int> channelsForParts (const Score& score)
{
    // The channels a pitched part may have, in the order they are handed out.
    std::vector<int> free;
    for (int bank = 0; bank < maxBanks; ++bank)
        for (int ch = 0; ch < channelsPerBank; ++ch)
        {
            if (ch == drumChannel) continue;
            if (bank == 0 && (ch == liveChannel || ch == previewChannel)) continue;
            free.push_back (bank * channelsPerBank + ch);
        }
    std::vector<int> channels;
    size_t next = 0;
    for (const auto& part : score.parts)
    {
        if (instrumentById (part.instrument).drums) { channels.push_back (drumChannel); continue; }
        channels.push_back (free[next]);
        next = (next + 1) % free.size();
    }
    return channels;
}

int banksForParts (const std::vector<int>& channels)
{
    int banks = 1;
    for (int ch : channels) banks = std::max (banks, ch / channelsPerBank + 1);
    return banks;
}

bool partAudible (const Score& score, size_t partIndex)
{
    bool anySolo = false;
    for (const auto& p : score.parts) anySolo = anySolo || p.solo;
    const auto& part = score.parts[partIndex];
    if (part.mute) return false;
    return ! anySolo || part.solo;
}

std::vector<PlayEvent> perform (const Score& score, const PerformOptions& options)
{
    std::vector<PlayEvent> events;
    const Tick from = std::max<Tick> (0, options.from);
    const Tick to = options.to < 0 ? std::max (score.endTick(), score.lastNoteEnd()) : options.to;
    const auto channels = channelsForParts (score);

    for (size_t pi = 0; pi < score.parts.size(); ++pi)
    {
        const auto& part = score.parts[pi];
        if (options.honourMuteAndSolo && ! partAudible (score, pi)) continue;
        const auto& inst = instrumentById (part.instrument);
        const int ch = channels[pi];
        const int partIndex = static_cast<int> (pi);

        if (options.includeSetup)
        {
            if (! inst.drums)
                events.push_back ({ 0, PlayEvent::program, ch, inst.program, 0, partIndex, 0 });
            events.push_back ({ 0, PlayEvent::controller, ch, 7,
                                std::clamp (static_cast<int> (part.volume * 127.0f + 0.5f), 0, 127), partIndex, 0 });
            events.push_back ({ 0, PlayEvent::controller, ch, 11, 127, partIndex, 0 });
        }

        for (const auto& n : part.notes)
        {
            if (n.end() <= from || n.start >= to) continue;
            // A note that began before the range is left out rather than
            // started late; one that runs past the end is cut at it.
            if (n.start < from) continue;
            const Tick end = std::min (n.end(), to);
            events.push_back ({ n.start - from, PlayEvent::noteOn, ch, n.pitch, n.velocity, partIndex, n.id });
            events.push_back ({ end - from, PlayEvent::noteOff, ch, n.pitch, 0, partIndex, n.id });
        }

        if (options.autoCC && part.autoCC && inst.cc != CCShape::none)
        {
            AutoCCOptions ao;
            ao.onlyControllers = options.autoCCControllers;
            for (const auto& cc : autoCC (score, part, ao))
            {
                if (cc.at < from || cc.at >= to) continue;
                int value = cc.value;
                // The part's own volume is the ceiling CC7 breathes under.
                if (cc.controller == 7) value = static_cast<int> (value * part.volume + 0.5f);
                events.push_back ({ cc.at - from, PlayEvent::controller, ch, cc.controller,
                                    std::clamp (value, 0, 127), partIndex, 0 });
            }
        }
    }

    std::stable_sort (events.begin(), events.end(), [] (const PlayEvent& a, const PlayEvent& b)
    {
        if (a.at != b.at) return a.at < b.at;
        return static_cast<int> (a.type) < static_cast<int> (b.type);
    });
    return events;
}

} // namespace nt
