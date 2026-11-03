#pragma once
// The internal MIDI map, in two directions. No JUCE here: the tables are read by the audio thread.
//   input:  notes, CC and pitch wheel coming into the plugin move any parameter
//   output: what a track is doing goes out as CC or pitch wheel, on that track's port and channel
#include "SeggCore.h"

namespace segg
{
static constexpr int kNumMapRows = 16;

// Append only: saved by index.
enum InType { InOff, InCC, InNote, InPitch, kNumInTypes };
static inline const char* kInTypeNames[kNumInTypes] = { "Off", "CC", "Note", "Pitch Wheel" };

enum InMode { ModeAbsolute, ModeToggle, kNumInModes };
static inline const char* kInModeNames[kNumInModes] = { "Absolute", "Toggle" };

enum OutSource { OutNote, OutVelocity, OutRandom, OutGate, OutPosX, OutPosY, kNumOutSources };
static inline const char* kOutSourceNames[kNumOutSources] = { "Note", "Velocity", "Random", "Gate", "Position X", "Position Y" };

enum OutDest { DestOff, DestCC, DestPitch, kNumOutDests };
static inline const char* kOutDestNames[kNumOutDests] = { "Off", "CC", "Pitch Wheel" };

static constexpr int kPitchCentre = 8192;

/** Incoming message -> parameter. Written by the UI, read by the audio thread. */
struct InMapRow
{
    std::atomic<int> type { InOff }, channel { 0 } /* 0 = any */, number { 1 }, param { -1 }, lo { 0 }, hi { 100 }, mode { ModeAbsolute };
    std::atomic<int> toggled { 0 }, high { 0 }, hits { 0 };

    void clear() noexcept { type = InOff; channel = 0; number = 1; param = -1; lo = 0; hi = 100; mode = ModeAbsolute; toggled = 0; high = 0; }
};

/** Track signal -> CC or pitch wheel. */
struct OutMapRow
{
    std::atomic<int> track { 0 }, source { OutNote }, dest { DestOff }, number { 1 }, lo { 0 }, hi { 100 }, slewMs { 0 };
    std::atomic<float> live { 0.0f };    // last value sent, 0..1, for the page's meter

    void clear() noexcept { track = 0; source = OutNote; dest = DestOff; number = 1; lo = 0; hi = 100; slewMs = 0; live = 0.0f; }
};

/** Runs one incoming message through the table. send (paramIndex, normalisedValue) is called for each match.
    Returns true if any row took the message, so a mapped note does not also transpose the sequencers. */
template <class Send>
bool applyInputMap (std::array<InMapRow, kNumMapRows>& rows, int status, int d1, int d2, Send&& send) noexcept
{
    const int kind = status & 0xf0, ch = (status & 15) + 1;
    const bool noteOn = kind == 0x90 && d2 > 0, noteOff = kind == 0x80 || (kind == 0x90 && d2 == 0);
    bool taken = false;
    for (auto& row : rows)
    {
        const int type = row.type.load (std::memory_order_relaxed), param = row.param.load (std::memory_order_relaxed);
        if (type == InOff || param < 0) continue;
        const int wanted = row.channel.load (std::memory_order_relaxed);
        if (wanted != 0 && wanted != ch) continue;

        float x = -1.0f;      // 0..1, or negative when the row does not fire
        const bool toggle = row.mode.load (std::memory_order_relaxed) == ModeToggle;
        const int number = row.number.load (std::memory_order_relaxed);
        if (type == InCC && kind == 0xB0 && d1 == number)
        {
            if (! toggle) x = (float) d2 / 127.0f;
            else
            {   // a button: it fires when the value crosses the middle upwards
                const int high = d2 >= 64 ? 1 : 0;
                taken = true;
                if (high == 1 && row.high.exchange (high) == 0) x = row.toggled.fetch_xor (1) == 0 ? 1.0f : 0.0f;
                else row.high.store (high, std::memory_order_relaxed);
            }
        }
        else if (type == InNote && (noteOn || noteOff) && d1 == number)
        {
            taken = true;
            if (! toggle) x = noteOn ? 1.0f : 0.0f;
            else if (noteOn) x = row.toggled.fetch_xor (1) == 0 ? 1.0f : 0.0f;
        }
        else if (type == InPitch && kind == 0xE0)
        {
            x = (float) (((d2 & 0x7f) << 7) | (d1 & 0x7f)) / 16383.0f;
        }
        if (x < 0.0f) continue;

        taken = true;
        row.hits.fetch_add (1, std::memory_order_relaxed);
        const float lo = (float) row.lo.load (std::memory_order_relaxed) * 0.01f, hi = (float) row.hi.load (std::memory_order_relaxed) * 0.01f;
        send (param, std::clamp (lo + (hi - lo) * x, 0.0f, 1.0f));
    }
    return taken;
}
} // namespace segg
