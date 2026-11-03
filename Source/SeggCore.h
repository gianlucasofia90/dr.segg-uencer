#pragma once
// DR. SEGG-UENCER: shared definitions. No JUCE in this file or in SeggEngine.h, so the sequencing core stays plain C++.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace segg
{
static constexpr int kNumTracks = 16;
static constexpr int kNumCells = 16;     // STEP lanes and the 4x4 CARTESIAN grid
static constexpr int kNumSlots = 16;     // direct MIDI port slots; port 0 is the host output
static constexpr int kMaxBalls = 8;
static constexpr int kMaxEuclid = 32;

enum Type { Euclid, Step, Turing, Cartesian, Physics, kNumTypes };
static inline const char* kTypeNames[kNumTypes] = { "Euclid", "Step", "Turing", "Cartesian", "Physics" };

// Append only: hosts save choices by index.
static constexpr int kNumRates = 21;
struct RateDef { const char* name; double beats; };
static inline const RateDef kRates[kNumRates] = {
    { "4 bar", 16.0 }, { "2 bar", 8.0 }, { "1 bar", 4.0 }, { "1/2", 2.0 }, { "1/4.", 1.5 }, { "1/4", 1.0 },
    { "1/4T", 2.0 / 3.0 }, { "1/8.", 0.75 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 }, { "1/16.", 0.375 }, { "1/16", 0.25 },
    { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 }, { "1/32T", 1.0 / 12.0 }, { "1/64", 0.0625 },
    { "5:1", 0.2 }, { "7:1", 1.0 / 7.0 }, { "5:2", 0.4 }, { "7:2", 2.0 / 7.0 }, { "11:1", 1.0 / 11.0 } };   // N:M = N steps every M beats
static constexpr int kDefaultRate = 11;

struct ScaleDef { const char* name; std::uint16_t mask; };   // bit n = n semitones above the root
static constexpr int kNumScales = 19;
static inline const ScaleDef kScales[kNumScales] = {
    { "Chromatic",  0b111111111111 }, { "Major",      0b101010110101 }, { "Minor",      0b010110101101 },
    { "Dorian",     0b011010101101 }, { "Phrygian",   0b010110101011 }, { "Lydian",     0b101011010101 },
    { "Mixolydian", 0b011010110101 }, { "Locrian",    0b010101101011 }, { "Harm Minor", 0b100110101101 },
    { "Penta Min",  0b010010101001 }, { "Penta Maj",  0b001010010101 }, { "Blues",      0b010011101001 },
    { "Whole Tone", 0b010101010101 }, { "Diminished", 0b101101101101 }, { "Hirajoshi",  0b000110001101 },
    { "Iwato",      0b010001100011 }, { "Tetratonic", 0b000010010101 }, { "Fifths",     0b000010000001 },
    { "Octaves",    0b000000000001 } };
static inline const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

static constexpr int kNumChords = 7;
static inline const char* kChordNames[kNumChords] = { "Mono", "Octave", "Fifth", "Triad", "Seventh", "Sus", "Cluster" };

static constexpr int kNumDirs = 5;
static inline const char* kDirNames[kNumDirs] = { "Forward", "Reverse", "Pendulum", "Random", "Brownian" };

enum LinkMode { LinkReset, LinkClock, LinkTranspose, LinkGate, LinkChoke, kNumLinkModes };
static inline const char* kLinkNames[kNumLinkModes] = { "Reset", "Clock", "Transpose", "Follow", "Choke" };

static constexpr int kNumPaths = 8;
static inline const char* kPathNames[kNumPaths] = { "Rows", "Snake", "Columns", "Weave", "Spiral", "Diagonal", "Knight", "Checker" };
// Coordinates (cell index, row * 4 + col) in the order the X clock walks them. Y clock moves to the next set.
static inline const std::uint8_t kPaths[kNumPaths][kNumCells] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 0, 1, 2, 3, 7, 6, 5, 4, 8, 9, 10, 11, 15, 14, 13, 12 },
    { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 },
    { 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3 },
    { 0, 1, 2, 3, 7, 11, 15, 14, 13, 12, 8, 4, 5, 6, 10, 9 },
    { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 },
    { 0, 6, 13, 11, 2, 4, 9, 15, 5, 3, 10, 12, 1, 7, 14, 8 },
    { 0, 2, 5, 7, 8, 10, 13, 15, 1, 3, 4, 6, 9, 11, 12, 14 } };

enum WallMode { HitWalls, HitBalls, HitBoth, kNumHitModes };
static inline const char* kHitNames[kNumHitModes] = { "Walls", "Collisions", "Both" };

// Index of each per-track parameter. Append only: the order defines the parameter order seen by hosts.
enum P
{
    // common
    pOn, pType, pPort, pCh, pRate, pSwing, pDrift, pGate, pVel, pVrand, pProb, pRoll, pRolln, pNote, pScale, pChord,
    pMutate, pLsrc, pLmode, pDelay, pSeed,
    // euclid
    pEsteps, pEpulses, pErot, pEacc, pEmel,
    // step
    pSlen, pSdir,
    // turing
    pTlen, pTknob, pTrange, pTdens,
    // cartesian
    pCmode, pCpath, pCxdiv, pCydiv, pCw, pChgt, pCacc, pCtrig,
    // physics
    pPcount, pPspeed, pPgrav, pPelast, pPmode, pPrange, pPsize, pPquant, pPkick,
    // output
    pNotes,
    kNumP
};

// Parameter id suffixes, in the same order as enum P ("t01_rate" ...). Never rename: hosts save them.
static inline const char* kParamIds[kNumP] = {
    "on", "type", "port", "ch", "rate", "swing", "drift", "gate", "vel", "vrand", "prob", "roll", "rolln", "note", "scale", "chord",
    "mutate", "lsrc", "lmode", "delay", "seed",
    "esteps", "epulses", "erot", "eacc", "emel",
    "slen", "sdir",
    "tlen", "tknob", "trange", "tdens",
    "cmode", "cpath", "cxdiv", "cydiv", "cw", "chgt", "cacc", "ctrig",
    "pcount", "pspeed", "pgrav", "pelast", "pmode", "prange", "psize", "pquant", "pkick",
    "notes" };

struct TrackParams
{
    std::array<float, kNumP> v {};
    int i (P p) const noexcept { return (int) std::lround (v[(size_t) p]); }
    float f (P p) const noexcept { return v[(size_t) p]; }
    bool b (P p) const noexcept { return v[(size_t) p] > 0.5f; }
};

struct GlobalParams
{
    int root = 0, scale = 3;          // index into kScales
    float density = 100.0f;           // scales every track's Prob, 0..200 %
    float chaos = 0.0f;               // added to every track's Mutate, 0..100 %
    bool resetOnPlay = true;
    int keyTranspose = 0;             // from incoming MIDI notes, in semitones
};

/** One STEP lane column or CARTESIAN pad. Written by the UI, read by the audio thread, so all fields are atomics. */
struct Cell
{
    std::atomic<std::int8_t> on { 1 }, pitch { 0 }, vel { 100 }, gate { 100 }, prob { 100 }, ratch { 1 };

    void set (int onValue, int pitchValue, int velValue = 100, int gateValue = 100, int probValue = 100, int ratchValue = 1) noexcept
    {
        on = (std::int8_t) onValue; pitch = (std::int8_t) pitchValue; vel = (std::int8_t) velValue;
        gate = (std::int8_t) gateValue; prob = (std::int8_t) probValue; ratch = (std::int8_t) ratchValue;
    }
};

struct Pattern { std::array<Cell, kNumCells> step, cart; };

/** Small, fast, seedable generator: the same seed gives the same music. */
struct Rng
{
    std::uint32_t s = 0x9e3779b9u;
    void seed (std::uint32_t v) noexcept { s = v * 2654435761u + 0x9e3779b9u; if (s == 0) s = 1; for (int i = 0; i < 4; ++i) next(); }
    std::uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float unit() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }           // 0..1
    float bip() noexcept { return unit() * 2.0f - 1.0f; }                                    // -1..1
    int range (int n) noexcept { return n <= 1 ? 0 : (int) (next() % (std::uint32_t) n); }   // 0..n-1
    bool chance (float p) noexcept { return p >= 1.0f || (p > 0.0f && unit() < p); }
};

inline int pmod (std::int64_t a, int n) noexcept { const auto r = a % n; return (int) (r < 0 ? r + n : r); }

/** Maximally even rhythm: true where one of k pulses falls among n steps, with the first pulse on step 0. */
inline bool euclidHit (int index, int k, int n) noexcept
{
    if (k <= 0 || n <= 0) return false;
    if (k >= n) return true;
    return (index * k) % n < k;
}

inline std::uint16_t rotatedMask (int scale, int root) noexcept
{
    const unsigned m = kScales[std::clamp (scale, 0, kNumScales - 1)].mask;
    const int r = pmod (root, 12);
    return (std::uint16_t) (((m << r) | (m >> (12 - r))) & 0xfff);
}

/** Nearest note of the scale (mask is rotated to absolute pitch classes); ties go down. */
inline int snapToScale (int note, std::uint16_t mask) noexcept
{
    if (mask == 0) return note;
    for (int d = 0; d < 12; ++d)
    {
        if ((mask >> pmod (note - d, 12)) & 1u) return note - d;
        if ((mask >> pmod (note + d, 12)) & 1u) return note + d;
    }
    return note;
}

/** n scale degrees above note, which must already be in the scale. */
inline int degreesUp (int note, int n, std::uint16_t mask) noexcept
{
    if (mask == 0) return note;
    while (n > 0) { ++note; if ((mask >> pmod (note, 12)) & 1u) --n; }
    return note;
}

/** Keeps a note inside MIDI range by octaves. */
inline int foldNote (int note) noexcept
{
    while (note < 0) note += 12;
    while (note > 127) note -= 12;
    return note;
}
} // namespace segg
