#pragma once
// DR. SEGG-UENCER sequencing core. All 16 tracks run at once: each has its own clock, length and sequencer type, and
// tracks can drive each other through links. Real-time safe: no allocation, no locks.
//
// Time is measured in beats (ppq), so the music follows host tempo changes and renders the same offline. Events are
// handled in time order across all tracks; that is what makes links between tracks sample accurate.
#include "SeggCore.h"
#include "MidiMap.h"

namespace segg
{
struct Transport
{
    bool playing = false;
    double ppq = 0.0, bpm = 120.0, sampleRate = 48000.0;
    int numSamples = 0;
};

/** Live values for the editor. */
struct UiTrack
{
    std::atomic<int> pos { 0 }, cx { 0 }, cy { 0 }, cset { 0 }, lastNote { -1 }, rot { 0 }, pulses { 0 };
    std::atomic<std::uint32_t> reg { 0 }, fires { 0 };
    std::array<std::atomic<float>, kMaxBalls> bx {}, by {};
};

class Engine
{
public:
    static constexpr int kPoolSize = 8192;
    static constexpr int kPhysPerBeat = 96;

    std::array<TrackParams, kNumTracks> params {};
    std::array<Pattern, kNumTracks> patterns;
    std::array<UiTrack, kNumTracks> ui;
    std::array<OutMapRow, kNumMapRows> outMap;
    GlobalParams global;

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        wasPlaying = false;
        needsStop = true;      // a host can prepare again mid-session: close what is sounding in the next block
        for (int i = 0; i < kNumTracks; ++i) resetTrack (i);
    }

    /** Runs one block; out.emit (sampleOffset, port, status, data1, data2) receives the MIDI in time order. */
    template <class Sink>
    void process (const Transport& t, Sink& out) noexcept
    {
        if (t.numSamples <= 0) return;
        if (needsStop) { stopAll (out, 0); needsStop = false; }
        block.ppq0 = t.ppq;
        block.pps = t.bpm / (60.0 * sr);
        block.n = t.numSamples;
        const double ppq1 = t.ppq + t.numSamples * block.pps;

        if (! t.playing)
        {
            if (wasPlaying) stopAll (out, 0);
            wasPlaying = false;
            sampleClock += t.numSamples;
            return;
        }
        if (! wasPlaying)
        {
            if (global.resetOnPlay) for (int i = 0; i < kNumTracks; ++i) resetTrack (i);
            relocate (t.ppq);
            wasPlaying = true;
        }
        else if (std::abs (t.ppq - expectedPpq) > 2.0 * block.pps + 1.0e-4)
        {
            relocate (t.ppq);       // loop wrap or a jump. Pending note-offs count samples, so they still arrive
        }
        expectedPpq = ppq1;

        for (int i = 0; i < kNumTracks; ++i) syncTrack (i, t.ppq);

        // merge the events of all tracks in time order
        for (int guard = 0; guard < 100000; ++guard)
        {
            int best = -1; double bestT = ppq1; bool bestLinked = true;
            for (int i = 0; i < kNumTracks; ++i)
            {
                auto& tr = tracks[(size_t) i];
                if (! tr.active) continue;
                const double when = nextEvent (i);
                const bool linked = params[(size_t) i].i (pLsrc) > 0;
                // at the same instant a linked track goes after the others, so it sees what they just did
                if (when < bestT || (best >= 0 && when == bestT && bestLinked && ! linked)) { best = i; bestT = when; bestLinked = linked; }
            }
            if (best < 0) break;
            runEvent (best, bestT);
        }

        flush (out);
        updateOutMap (out, ppq1, t.numSamples);
        sampleClock += t.numSamples;
        publishUi();
    }

    /** All sounding notes off, on every port and channel. */
    template <class Sink>
    void panic (Sink& out) noexcept
    {
        stopAll (out, 0);
        for (int p = 0; p <= kNumSlots; ++p)
            for (int c = 0; c < 16; ++c) { out.emit (0, p, 0xB0 | c, 123, 0); out.emit (0, p, 0xB0 | c, 120, 0); }
    }

    int numPending() const noexcept { return poolCount; }
    int numSounding() const noexcept
    {
        int n = 0;
        for (auto& port : owner) for (auto& ch : port) for (auto v : ch) if (v != 0) ++n;
        return n;
    }

    /** The 16 steps of a pattern shown in the UI after the track's live mutation, as a pulse mask. */
    static std::uint32_t euclidMask (int steps, int pulses, int rot) noexcept
    {
        std::uint32_t m = 0;
        for (int i = 0; i < steps; ++i) if (euclidHit (pmod (i - rot, steps), pulses, steps)) m |= 1u << i;
        return m;
    }

private:
    struct Ball { float x = 0.5f, y = 0.5f, vx = 0, vy = 0; };

    struct Track
    {
        bool active = false, wasClocked = false;
        int rate = -1, type = -1, seed = -1;
        double stepBeats = 0.25, nextT = 0, physNext = 0, lastFire = -1.0e12, lastHit = -1.0e12;
        std::int64_t tick = 0, tickBase = 0, physCount = 0;
        Rng rng, modRng;        // modRng feeds the Random output, so mapping a track never changes its notes
        float sigNote = 0.5f, sigVel = 0.0f, sigRand = 0.0f;
        double gateUntil = -1.0e12;
        int transpose = 0, lastOffset = 0, melody = 0, mutRot = 0, mutPulses = 0, pos = 0, dir = 1, cartJitter = 0;
        float driftMs = 0;
        std::array<std::int8_t, kNumCells> stepMut {};
        std::uint32_t reg = 0;
        std::array<Ball, kMaxBalls> balls;
        bool hitPending = false;
        float hitPitch = 0, hitVel = 0;
    };

    struct Pending { std::int64_t when; std::uint32_t token; std::uint8_t port, status, d1, d2; };

    struct MapState { float value = 0.0f; int sent = -1, dest = DestOff, port = 0, ch = 0; std::int64_t nextSend = 0; };

    struct Fire { int offset = 0; float vel = 1.0f, gate = 1.0f, prob = 1.0f; int ratch = 1; };

    // ---------------- clocks

    double boundary (const Track& tr, const TrackParams& p, std::int64_t n) const noexcept
    {
        const double swing = (n & 1) != 0 ? 0.5 * p.f (pSwing) * 0.01 : 0.0;
        return ((double) n + swing) * tr.stepBeats;
    }

    void relocateTrack (int i, double ppq) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        tr.stepBeats = kRates[std::clamp (p.i (pRate), 0, kNumRates - 1)].beats;
        const double eps = 1.0e-9;
        auto n = (std::int64_t) std::floor (ppq / tr.stepBeats);
        while (boundary (tr, p, n) < ppq - eps) ++n;
        while (boundary (tr, p, n - 1) >= ppq - eps) --n;
        tr.tick = n;
        tr.nextT = boundary (tr, p, n);
        tr.physCount = (std::int64_t) std::ceil (ppq * kPhysPerBeat - eps);
        tr.physNext = (double) tr.physCount / kPhysPerBeat;
    }

    void relocate (double ppq) noexcept { for (int i = 0; i < kNumTracks; ++i) relocateTrack (i, ppq); }

    void syncTrack (int i, double ppq) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const bool on = p.b (pOn);
        const int rate = p.i (pRate), type = p.i (pType), seed = p.i (pSeed);
        const bool clocked = isClocked (p);
        if (seed != tr.seed) resetTrack (i);
        if (! on) { tr.active = false; return; }
        if (! tr.active || rate != tr.rate || type != tr.type || clocked != tr.wasClocked)
        {
            if (type == Physics && tr.type != Physics) launchBalls (tr);
            tr.rate = rate; tr.type = type; tr.wasClocked = clocked;
            tr.active = true;
            relocateTrack (i, ppq);
        }
    }

    bool isClocked (const TrackParams& p) const noexcept { return p.i (pLsrc) > 0 && p.i (pLmode) == LinkClock; }

    double nextEvent (int i) const noexcept
    {
        const auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const double clock = isClocked (p) ? 1.0e18 : tr.nextT;
        return p.i (pType) == Physics ? std::min (clock, tr.physNext) : clock;
    }

    void runEvent (int i, double when) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        if (p.i (pType) == Physics && tr.physNext <= when)
        {
            physicsStep (i, when);
            ++tr.physCount;
            tr.physNext = (double) tr.physCount / kPhysPerBeat;
            if (isClocked (p) || tr.nextT > when) return;
        }
        const auto tick = tr.tick;
        ++tr.tick;
        tr.nextT = boundary (tr, p, tr.tick);
        stepTrack (i, when, tick, 0);
    }

    // ---------------- reset

    void resetTrack (int i) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        tr.seed = p.i (pSeed);
        tr.rng.seed ((std::uint32_t) (tr.seed + 1) * 7919u + (std::uint32_t) i * 104729u);
        tr.reg = tr.rng.next();
        tr.modRng.seed ((std::uint32_t) (tr.seed + 1) * 31337u + (std::uint32_t) i);
        tr.gateUntil = -1.0e12;
        tr.transpose = tr.lastOffset = tr.melody = tr.mutRot = tr.mutPulses = tr.pos = tr.cartJitter = 0;
        tr.dir = 1;
        tr.tickBase = 0;
        tr.driftMs = 0;
        tr.stepMut.fill (0);
        tr.lastFire = tr.lastHit = -1.0e12;
        tr.hitPending = false;
        launchBalls (tr);
    }

    void launchBalls (Track& tr) noexcept
    {
        for (auto& b : tr.balls)
        {
            b.x = 0.15f + 0.7f * tr.rng.unit();
            b.y = 0.15f + 0.7f * tr.rng.unit();
            const float a = tr.rng.unit() * 6.2831853f;
            b.vx = std::cos (a);
            b.vy = std::sin (a);
        }
    }

    /** What a Reset link does to the receiving track. */
    void linkReset (int i) noexcept
    {
        auto& tr = tracks[(size_t) i];
        tr.tickBase = tr.tick;
        tr.pos = 0;
        tr.melody = 0;
        tr.dir = 1;
        if (params[(size_t) i].i (pType) == Physics) launchBalls (tr);
    }

    // ---------------- sequencers

    float mutateAmount (const TrackParams& p) const noexcept
    {
        return std::clamp ((p.f (pMutate) + global.chaos) * 0.01f, 0.0f, 1.0f);
    }

    void stepTrack (int i, double when, std::int64_t tick, int depth) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const auto cnt = tick - tr.tickBase;
        Fire f;
        bool hit = false;
        switch (p.i (pType))
        {
            case Euclid:    hit = euclidStep (i, cnt, f); break;
            case Step:      hit = stepStep (i, cnt, f); break;
            case Turing:    hit = turingStep (tr, p, f); break;
            case Cartesian: hit = cartesianStep (i, cnt, f); break;
            default:        // Physics: the clock only releases quantized hits
                if (p.b (pPquant) && tr.hitPending)
                {
                    tr.hitPending = false;
                    f.offset = (int) std::lround (tr.hitPitch * (float) p.i (pPrange));
                    f.vel = tr.hitVel;
                    hit = true;
                }
                break;
        }
        if (hit) fireNote (i, when, f, depth);
    }

    bool euclidStep (int i, std::int64_t cnt, Fire& f) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const int n = std::clamp (p.i (pEsteps), 1, kMaxEuclid);
        if (pmod (cnt, n) == 0 && tr.rng.chance (mutateAmount (p)))
        {
            // the variation wanders around the set pattern and tends to come back to it
            switch (tr.rng.range (3))
            {
                case 0:  tr.mutRot = std::clamp (tr.mutRot + (tr.rng.chance (0.5f) ? 1 : -1), -3, 3); break;
                case 1:  tr.mutPulses = std::clamp (tr.mutPulses + (tr.rng.chance (0.5f) ? 1 : -1), -2, 2); break;
                default: tr.mutRot /= 2; tr.mutPulses /= 2; break;
            }
        }
        const int k = std::clamp (p.i (pEpulses) + tr.mutPulses, 0, n);
        const int rot = p.i (pErot) + tr.mutRot;
        const int idx = pmod (cnt - rot, n);
        tr.pos = pmod (cnt, n);
        ui[(size_t) i].rot.store (rot, std::memory_order_relaxed);
        ui[(size_t) i].pulses.store (k, std::memory_order_relaxed);
        if (! euclidHit (idx, k, n)) return false;

        int pulse = 0;
        for (int s = 0; s < idx; ++s) if (euclidHit (s, k, n)) ++pulse;
        const int accents = std::clamp (p.i (pEacc), 0, k);
        f.vel = accents == 0 || euclidHit (pulse, accents, k) ? 1.0f : 0.68f;

        const int mel = std::clamp (p.i (pEmel), 1, kNumCells);
        if (mel > 1)
        {
            tr.melody %= mel;
            f.offset = patterns[(size_t) i].step[(size_t) tr.melody].pitch.load (std::memory_order_relaxed);
            tr.melody = (tr.melody + 1) % mel;
        }
        return true;
    }

    bool stepStep (int i, std::int64_t cnt, Fire& f) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const int len = std::clamp (p.i (pSlen), 1, kNumCells);
        switch (p.i (pSdir))
        {
            case 1: tr.pos = len - 1 - pmod (cnt, len); break;
            case 2:
            {
                const int period = std::max (1, 2 * (len - 1));
                const int ph = pmod (cnt, period);
                tr.pos = ph < len ? ph : period - ph;
                break;
            }
            case 3: tr.pos = tr.rng.range (len); break;
            case 4: tr.pos = pmod (tr.pos + tr.rng.range (3) - 1, len); break;
            default: tr.pos = pmod (cnt, len); break;
        }
        if (pmod (cnt, len) == 0)
        {
            const float m = mutateAmount (p);
            if (tr.rng.chance (m))
            {
                const auto c = (size_t) tr.rng.range (len);
                tr.stepMut[c] = (std::int8_t) (tr.rng.chance (0.6f) ? tr.rng.range (11) - 5 : 0);
            }
        }
        const auto idx = (size_t) tr.pos;
        return readCell (patterns[(size_t) i].step[idx], tr.stepMut[idx], f);
    }

    static bool readCell (const Cell& c, int extraPitch, Fire& f) noexcept
    {
        if (c.on.load (std::memory_order_relaxed) == 0) return false;
        f.offset = c.pitch.load (std::memory_order_relaxed) + extraPitch;
        f.vel = (float) c.vel.load (std::memory_order_relaxed) * 0.01f;
        f.gate = (float) c.gate.load (std::memory_order_relaxed) * 0.01f;
        f.prob = (float) c.prob.load (std::memory_order_relaxed) * 0.01f;
        f.ratch = std::max (1, (int) c.ratch.load (std::memory_order_relaxed));
        return true;
    }

    /** Shift register with a looped feedback bit, after the Music Thing Turing Machine. The big knob: 50 % replaces
        every bit with a random one, 100 % locks the loop, 0 % locks it at twice the length (inverted feedback). */
    bool turingStep (Track& tr, const TrackParams& p, Fire& f) noexcept
    {
        const int len = std::clamp (p.i (pTlen), 2, 32);
        const float k = p.f (pTknob);
        const float replace = std::min (1.0f, 1.0f - std::abs (k - 50.0f) / 50.0f + mutateAmount (p) * 0.25f);
        unsigned bit = (tr.reg >> (len - 1)) & 1u;
        if (k < 50.0f) bit ^= 1u;
        if (tr.rng.chance (replace)) bit = tr.rng.next() >> 31;
        tr.reg = (tr.reg << 1) | bit;
        tr.pos = len;
        const float dac = (float) (tr.reg & 0xffu) / 255.0f;
        // the gate reads a different slice of the register, so rhythm and pitch mutate together but are not the same
        const float pulse = (float) ((tr.reg >> 3) & 0xffu) / 255.0f;
        f.offset = (int) std::lround (dac * (float) p.i (pTrange));
        f.vel = 0.72f + 0.28f * pulse;
        return pulse <= p.f (pTdens) * 0.01f;
    }

    /** X and Y advance on separate clocks, after the Make Noise René. In Snake mode X walks a stored path and Y
        chooses the path. Pads that are switched off are denied: SEEK jumps to the next open pad, SLEEP stays silent. */
    bool cartesianStep (int i, std::int64_t cnt, Fire& f) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const int xdiv = std::clamp (p.i (pCxdiv), 1, 16), ydiv = std::clamp (p.i (pCydiv), 1, 16);
        const int w = std::clamp (p.i (pCw), 1, 4), h = std::clamp (p.i (pChgt), 1, 4);
        const bool xClock = pmod (cnt, xdiv) == 0, yClock = pmod (cnt, ydiv) == 0;
        if (! xClock && ! yClock) return false;
        if (xClock && tr.rng.chance (mutateAmount (p) * 0.5f)) tr.cartJitter = tr.rng.range (4);

        const auto& cells = patterns[(size_t) i].cart;
        const auto xCount = (cnt - pmod (cnt, xdiv)) / xdiv, yCount = (cnt - pmod (cnt, ydiv)) / ydiv;
        const bool snake = p.i (pCmode) == 1;
        const int set = pmod (p.i (pCpath) + (snake ? yCount : 0), kNumPaths);

        auto cellAt = [&] (int attempt)
        {
            if (snake) return (int) kPaths[set][(size_t) pmod (xCount + attempt, kNumCells)];
            const int start = pmod (yCount + tr.cartJitter, h) * w + pmod (xCount, w);
            const int walked = pmod (start + attempt, w * h);      // reading order, so SEEK always moves forward
            return (walked / w) * 4 + walked % w;
        };
        auto open = [&] (int c) { return c % 4 < w && c / 4 < h && cells[(size_t) c].on.load (std::memory_order_relaxed) != 0; };

        int cell = cellAt (0);
        if (! open (cell))
        {
            if (p.i (pCacc) == 1) cell = -1;
            else
            {
                int found = -1;
                for (int a = 1; a <= kNumCells && found < 0; ++a) if (open (cellAt (a))) found = cellAt (a);
                cell = found;
            }
        }
        auto& u = ui[(size_t) i];
        if (cell >= 0)
        {
            u.cx.store (cell % 4, std::memory_order_relaxed);
            u.cy.store (cell / 4, std::memory_order_relaxed);
        }
        u.cset.store (set, std::memory_order_relaxed);
        const int trig = p.i (pCtrig);
        const bool fires = trig == 0 ? xClock : (trig == 1 ? yClock : true);
        return cell >= 0 && fires && readCell (cells[(size_t) cell], 0, f);
    }

    void physicsHit (int i, double when, float pitch, float impact) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const float vel = std::clamp (0.35f + 0.65f * impact, 0.0f, 1.0f);
        if (p.b (pPquant)) { tr.hitPending = true; tr.hitPitch = pitch; tr.hitVel = vel; return; }
        if (when - tr.lastHit < 1.0 / 32.0) return;     // keeps a rattling ball from flooding the MIDI port
        tr.lastHit = when;
        Fire f;
        f.offset = (int) std::lround (pitch * (float) p.i (pPrange));
        f.vel = vel;
        fireNote (i, when, f, 0);
    }

    /** Fixed step in beats, so the balls stay in time with the host and render the same offline. */
    void physicsStep (int i, double when) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const int count = std::clamp (p.i (pPcount), 1, kMaxBalls), mode = p.i (pPmode);
        const float dt = 1.0f / (float) kPhysPerBeat;
        const float speed = 0.1f + p.f (pPspeed) * 0.039f;              // box widths per beat
        const float gravity = p.f (pPgrav) * 0.04f;
        const float e = 0.55f + p.f (pPelast) * 0.0045f;                // restitution
        const float r = 0.02f + p.f (pPsize) * 0.0013f;
        const bool wallNotes = mode != HitBalls, ballNotes = mode != HitWalls;

        if (tr.physCount % kPhysPerBeat == 0)
        {
            const float kick = p.f (pPkick) * 0.01f, m = mutateAmount (p);
            for (int b = 0; b < count; ++b)
            {
                auto& ball = tr.balls[(size_t) b];
                if (tr.rng.chance (kick))
                {
                    const float a = tr.rng.unit() * 6.2831853f;
                    ball.vx += std::cos (a); ball.vy += std::sin (a);
                }
                if (tr.rng.chance (m)) { ball.vx += 0.5f * tr.rng.bip(); ball.vy += 0.5f * tr.rng.bip(); }
            }
        }

        for (int b = 0; b < count; ++b)
        {
            auto& ball = tr.balls[(size_t) b];
            ball.vy += gravity * dt;
            const float mag = std::sqrt (ball.vx * ball.vx + ball.vy * ball.vy);
            if (mag > 2.0f) { ball.vx *= 2.0f / mag; ball.vy *= 2.0f / mag; }
            ball.x += ball.vx * speed * dt;
            ball.y += ball.vy * speed * dt;

            auto wall = [&] (float& pos, float& vel, float lo, float hi, float along)
            {
                if (pos >= lo && pos <= hi) return;
                pos = pos < lo ? lo + (lo - pos) : hi - (pos - hi);
                pos = std::clamp (pos, lo, hi);
                const float impact = std::abs (vel);
                vel = -vel * e;
                if (impact < 0.12f) { vel = 0; return; }       // settled on the wall: no more notes
                if (wallNotes) physicsHit (i, when, along, std::min (1.0f, impact));
            };
            wall (ball.x, ball.vx, r, 1.0f - r, 1.0f - ball.y);
            wall (ball.y, ball.vy, r, 1.0f - r, ball.x);
        }

        for (int a = 0; a < count; ++a)
            for (int b = a + 1; b < count; ++b)
            {
                auto& A = tr.balls[(size_t) a];
                auto& B = tr.balls[(size_t) b];
                const float dx = B.x - A.x, dy = B.y - A.y, d2 = dx * dx + dy * dy, minD = 2.0f * r;
                if (d2 >= minD * minD || d2 < 1.0e-9f) continue;
                const float d = std::sqrt (d2), nx = dx / d, ny = dy / d;
                const float rel = (A.vx - B.vx) * nx + (A.vy - B.vy) * ny;
                const float push = 0.5f * (minD - d);
                A.x -= nx * push; A.y -= ny * push;
                B.x += nx * push; B.y += ny * push;
                if (rel <= 0.0f) continue;                     // already moving apart
                const float im = 0.5f * (1.0f + e) * rel;
                A.vx -= im * nx; A.vy -= im * ny;
                B.vx += im * nx; B.vy += im * ny;
                if (ballNotes && rel > 0.1f) physicsHit (i, when, 0.5f * (A.x + B.x), std::min (1.0f, rel * 0.6f));
            }
    }

    // ---------------- notes

    /** A sequencer asks for a note: chance, links, rolls and chords are applied here for every type. */
    void fireNote (int i, double when, const Fire& f, int depth) noexcept
    {
        auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const int lsrc = p.i (pLsrc), lmode = p.i (pLmode);
        if (lsrc > 0 && lsrc - 1 != i)
        {
            const auto& src = tracks[(size_t) lsrc - 1];
            const bool recent = when - src.lastFire <= tr.stepBeats + 1.0e-9;
            if ((lmode == LinkGate && ! recent) || (lmode == LinkChoke && recent)) return;
        }
        if (! tr.rng.chance (std::min (1.0f, p.f (pProb) * 0.01f * global.density * 0.01f) * f.prob)) return;

        int ratch = f.ratch;
        if (tr.rng.chance (p.f (pRoll) * 0.01f)) ratch = std::max (ratch, p.i (pRolln));

        const int scale = p.i (pScale);          // 0 = follow the global scale
        const auto mask = scale == 0 ? rotatedMask (global.scale, global.root) : rotatedMask (scale - 1, global.root);
        const int root = snapToScale (p.i (pNote) + f.offset + tr.transpose + global.keyTranspose, mask);
        const auto chordMask = mask == 0xfff ? rotatedMask (1, global.root) : mask;   // chords in Chromatic: use major
        const int anchor = snapToScale (root, chordMask);

        std::array<int, 4> notes { root, -1000, -1000, -1000 };
        switch (p.i (pChord))
        {
            case 1: notes[1] = root + 12; break;
            case 2: notes[1] = degreesUp (anchor, 4, chordMask); break;
            case 3: notes[1] = degreesUp (anchor, 2, chordMask); notes[2] = degreesUp (anchor, 4, chordMask); break;
            case 4: notes[1] = degreesUp (anchor, 2, chordMask); notes[2] = degreesUp (anchor, 4, chordMask); notes[3] = degreesUp (anchor, 6, chordMask); break;
            case 5: notes[1] = degreesUp (anchor, 3, chordMask); notes[2] = degreesUp (anchor, 4, chordMask); break;
            case 6: notes[1] = degreesUp (anchor, 1, chordMask); notes[2] = degreesUp (anchor, 2, chordMask); break;
            default: break;
        }

        const float rnd = p.f (pVrand) * 0.01f * tr.rng.bip();
        const int vel = std::clamp ((int) std::lround (p.f (pVel) * f.vel * (1.0f + rnd)), 1, 127);

        const float drift = p.f (pDrift);
        tr.driftMs = std::clamp (tr.driftMs + tr.rng.bip() * drift * 0.04f, 0.0f, drift * 0.25f);
        const double delay = (p.f (pDelay) + tr.driftMs) * 0.001 * sr;

        const double beat = std::max (1.0e-6, block.pps);
        const double on = (when - block.ppq0) / beat + delay;
        const double each = tr.stepBeats / ratch;
        const double gateBeats = std::min (tr.stepBeats * p.f (pGate) * 0.01 * f.gate, ratch > 1 ? each * 0.9 : 1.0e9);
        const auto gateSamples = (std::int64_t) std::max (0.002 * sr, gateBeats / beat);
        const int port = std::clamp (p.i (pPort), 0, kNumSlots);
        const int ch = std::clamp (p.i (pCh), 1, 16) - 1;

        tr.sigNote = std::clamp (0.5f + (float) (f.offset + tr.transpose + global.keyTranspose) / 48.0f, 0.0f, 1.0f);
        tr.sigVel = (float) vel / 127.0f;
        tr.sigRand = tr.modRng.unit();
        tr.gateUntil = when + gateBeats;
        mapOnHit (i, port, ch, sampleClock + std::llround (on));

        if (p.b (pNotes))
        for (int r = 0; r < ratch; ++r)
            for (int note : notes)
            {
                if (note == -1000) continue;
                const auto start = sampleClock + std::llround (on + r * each / beat);
                const int velHere = std::clamp (vel - r * vel / (ratch * 3), 1, 127);
                schedule (port, ch, foldNote (note), velHere, start, start + gateSamples);
            }

        tr.lastFire = when;
        tr.lastOffset = f.offset;
        auto& u = ui[(size_t) i];
        u.lastNote.store (foldNote (root), std::memory_order_relaxed);
        u.fires.fetch_add (1, std::memory_order_relaxed);

        // links: other tracks listening to this one
        if (depth >= kMaxLinkDepth) return;
        for (int j = 0; j < kNumTracks; ++j)
        {
            if (j == i || ! tracks[(size_t) j].active) continue;
            const auto& q = params[(size_t) j];
            if (q.i (pLsrc) != i + 1) continue;
            switch (q.i (pLmode))
            {
                case LinkReset:     linkReset (j); break;
                case LinkTranspose: tracks[(size_t) j].transpose = f.offset; break;
                case LinkClock:
                {
                    auto& other = tracks[(size_t) j];
                    const auto tick = other.tick;
                    ++other.tick;
                    stepTrack (j, when, tick, depth + 1);
                    break;
                }
                default: break;     // Follow and Choke are read when the listening track fires
            }
        }
    }

    void schedule (int port, int ch, int note, int vel, std::int64_t on, std::int64_t off) noexcept
    {
        if (poolCount + 2 > kPoolSize) return;     // full: drop the note, never a note-off
        const auto token = ++tokenCounter == 0 ? ++tokenCounter : tokenCounter;
        pool[(size_t) poolCount++] = { on, token, (std::uint8_t) port, (std::uint8_t) (0x90 | ch), (std::uint8_t) note, (std::uint8_t) vel };
        pool[(size_t) poolCount++] = { off, token, (std::uint8_t) port, (std::uint8_t) (0x80 | ch), (std::uint8_t) note, 0 };
    }

    // ---------------- output map

    float mapSignal (int i, int source, double ppq) const noexcept
    {
        const auto& tr = tracks[(size_t) i];
        const auto& p = params[(size_t) i];
        const auto& u = ui[(size_t) i];
        const int type = p.i (pType);
        const auto steps = [&] { return type == Euclid ? std::clamp (p.i (pEsteps), 1, kMaxEuclid) : std::clamp (p.i (pSlen), 1, kNumCells); };
        switch (source)
        {
            case OutNote:     return tr.sigNote;
            case OutVelocity: return tr.sigVel;
            case OutRandom:   return tr.sigRand;
            case OutGate:     return ppq < tr.gateUntil ? 1.0f : 0.0f;
            case OutPosX:
                if (type == Turing)    return (float) (tr.reg & 0xffu) / 255.0f;
                if (type == Physics)   return tr.balls[0].x;
                if (type == Cartesian) return (float) u.cx.load (std::memory_order_relaxed) / 3.0f;
                return steps() > 1 ? (float) tr.pos / (float) (steps() - 1) : 0.0f;
            default:      // OutPosY
                if (type == Turing)    return (float) ((tr.reg >> 8) & 0xffu) / 255.0f;
                if (type == Physics)   return tr.balls[0].y;
                if (type == Cartesian) return (float) u.cy.load (std::memory_order_relaxed) / 3.0f;
                // a smooth ramp over the whole pattern
                return (float) (std::fmod (ppq / tr.stepBeats, (double) steps()) / (double) steps());
        }
    }

    static int mapValue (const OutMapRow& row, int dest, float x) noexcept
    {
        const float lo = (float) row.lo.load (std::memory_order_relaxed) * 0.01f, hi = (float) row.hi.load (std::memory_order_relaxed) * 0.01f;
        const float v = std::clamp (lo + (hi - lo) * x, 0.0f, 1.0f);
        return (int) std::lround (v * (dest == DestPitch ? 16383.0f : 127.0f));
    }

    static void mapMessage (const OutMapRow& row, int dest, int ch, int value, std::uint8_t& status, std::uint8_t& d1, std::uint8_t& d2) noexcept
    {
        if (dest == DestPitch)
        {
            status = (std::uint8_t) (0xE0 | ch); d1 = (std::uint8_t) (value & 0x7f); d2 = (std::uint8_t) ((value >> 7) & 0x7f);
        }
        else
        {
            status = (std::uint8_t) (0xB0 | ch); d1 = (std::uint8_t) std::clamp (row.number.load (std::memory_order_relaxed), 0, 127); d2 = (std::uint8_t) value;
        }
    }

    /** Hit-based rows without slew go out one sample before their note, so a synth has the new value when the note lands. */
    void mapOnHit (int i, int port, int ch, std::int64_t when) noexcept
    {
        for (size_t r = 0; r < kNumMapRows; ++r)
        {
            auto& row = outMap[r];
            const int dest = row.dest.load (std::memory_order_relaxed), source = row.source.load (std::memory_order_relaxed);
            if (dest == DestOff || row.track.load (std::memory_order_relaxed) != i || source > OutRandom) continue;
            if (row.slewMs.load (std::memory_order_relaxed) > 0 || poolCount >= kPoolSize) continue;
            const float x = mapSignal (i, source, 0.0);
            auto& st = mapState[r];
            st.value = x;
            const int value = mapValue (row, dest, x);
            if (value == st.sent) continue;
            st.sent = value;
            st.dest = dest; st.port = port; st.ch = ch;
            row.live.store (x, std::memory_order_relaxed);
            Pending m { when - 1, 0, (std::uint8_t) port, 0, 0, 0 };
            mapMessage (row, dest, ch, value, m.status, m.d1, m.d2);
            pool[(size_t) poolCount++] = m;
        }
    }

    /** Everything that moves between hits, or that glides. Capped at one message every 5 ms per row. */
    template <class Sink>
    void updateOutMap (Sink& out, double ppq, int numSamples) noexcept
    {
        const double seconds = numSamples / sr;
        for (size_t r = 0; r < kNumMapRows; ++r)
        {
            auto& row = outMap[r];
            const int dest = row.dest.load (std::memory_order_relaxed);
            if (dest == DestOff) continue;
            const int i = std::clamp (row.track.load (std::memory_order_relaxed), 0, kNumTracks - 1);
            if (! tracks[(size_t) i].active) continue;
            const auto& p = params[(size_t) i];
            auto& st = mapState[r];
            const float target = mapSignal (i, row.source.load (std::memory_order_relaxed), ppq);
            const int slew = row.slewMs.load (std::memory_order_relaxed);
            st.value = slew > 0 ? st.value + (target - st.value) * (float) (1.0 - std::exp (-seconds * 1000.0 / slew)) : target;
            const int value = mapValue (row, dest, st.value);
            const bool moved = dest == DestPitch ? std::abs (value - st.sent) >= 4 || (value != st.sent && slew == 0) : value != st.sent;
            if (! moved || sampleClock < st.nextSend) continue;
            st.sent = value;
            st.dest = dest;
            st.port = std::clamp (p.i (pPort), 0, kNumSlots);
            st.ch = std::clamp (p.i (pCh), 1, 16) - 1;
            st.nextSend = sampleClock + (std::int64_t) (0.005 * sr);
            row.live.store (st.value, std::memory_order_relaxed);
            std::uint8_t status = 0, d1 = 0, d2 = 0;
            mapMessage (row, dest, st.ch, value, status, d1, d2);
            out.emit (numSamples - 1, st.port, status, d1, d2);
        }
    }

    /** Sends what falls inside this block. Each note owns its pitch through a token: when a pitch is retriggered, the
        new note takes over and the old note-off does nothing, so overlapping gates never cut a note short. */
    template <class Sink>
    void flush (Sink& out) noexcept
    {
        const auto end = sampleClock + block.n;
        int due = 0;
        for (int i = 0; i < poolCount;)
        {
            if (pool[(size_t) i].when < end)
            {
                dueList[(size_t) due++] = pool[(size_t) i];
                pool[(size_t) i] = pool[(size_t) --poolCount];
            }
            else ++i;
        }
        std::sort (dueList.begin(), dueList.begin() + due, [] (const Pending& a, const Pending& b) {
            if (a.when != b.when) return a.when < b.when;
            return (a.status & 0xf0) < (b.status & 0xf0);   // note-offs first
        });
        for (int i = 0; i < due; ++i)
        {
            const auto& e = dueList[(size_t) i];
            const int offset = (int) std::clamp<std::int64_t> (e.when - sampleClock, 0, block.n - 1);
            auto& slot = owner[e.port][(size_t) (e.status & 15)][e.d1];
            if ((e.status & 0xf0) == 0x90)
            {
                if (slot != 0) out.emit (offset, e.port, 0x80 | (e.status & 15), e.d1, 0);
                slot = e.token;
                out.emit (offset, e.port, e.status, e.d1, e.d2);
            }
            else if ((e.status & 0xf0) != 0x80)
                out.emit (offset, e.port, e.status, e.d1, e.d2);     // output map: CC or pitch wheel
            else if (slot == e.token)
            {
                slot = 0;
                out.emit (offset, e.port, e.status, e.d1, 0);
            }
        }
    }

    template <class Sink>
    void stopAll (Sink& out, int offset) noexcept
    {
        poolCount = 0;
        for (auto& st : mapState)
        {   // a bend left in the air would detune the synth until the next message
            if (st.dest == DestPitch && st.sent >= 0 && st.sent != kPitchCentre)
                out.emit (offset, st.port, 0xE0 | st.ch, kPitchCentre & 0x7f, kPitchCentre >> 7);
            st = {};
        }
        for (int port = 0; port <= kNumSlots; ++port)
            for (int ch = 0; ch < 16; ++ch)
                for (int note = 0; note < 128; ++note)
                    if (owner[(size_t) port][(size_t) ch][(size_t) note] != 0)
                    {
                        owner[(size_t) port][(size_t) ch][(size_t) note] = 0;
                        out.emit (offset, port, 0x80 | ch, note, 0);
                    }
    }

    void publishUi() noexcept
    {
        for (int i = 0; i < kNumTracks; ++i)
        {
            const auto& tr = tracks[(size_t) i];
            auto& u = ui[(size_t) i];
            u.pos.store (tr.pos, std::memory_order_relaxed);
            u.reg.store (tr.reg, std::memory_order_relaxed);
            if (params[(size_t) i].i (pType) == Physics)
                for (int b = 0; b < kMaxBalls; ++b)
                {
                    u.bx[(size_t) b].store (tr.balls[(size_t) b].x, std::memory_order_relaxed);
                    u.by[(size_t) b].store (tr.balls[(size_t) b].y, std::memory_order_relaxed);
                }
        }
    }

    static constexpr int kMaxLinkDepth = 3;

    struct Block { double ppq0 = 0, pps = 0; int n = 0; } block;
    double sr = 48000.0, expectedPpq = 0;
    bool wasPlaying = false, needsStop = false;
    std::int64_t sampleClock = 0;
    std::uint32_t tokenCounter = 0;
    std::array<Track, kNumTracks> tracks;
    std::array<Pending, kPoolSize> pool {}, dueList {};
    int poolCount = 0;
    std::array<MapState, kNumMapRows> mapState {};
    std::array<std::array<std::array<std::uint32_t, 128>, 16>, kNumSlots + 1> owner {};
};
} // namespace segg
