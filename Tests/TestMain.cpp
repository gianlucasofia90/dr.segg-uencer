// DR. SEGG-UENCER headless tests: drive the processor with a fake host transport and check the MIDI it plays.
// Run:  DrSeggUencerTests                        -> unit tests (exit code 1 on failure)
//       DrSeggUencerTests --snap file track [page] [preset] [scale] -> render the editor to a PNG, track 1-16,
//                                     page 0 = tracks, 1 = routing, 10+ = step lane; scale = window size (1 = 1280 x 800)
#include <juce_audio_processors/juce_audio_processors.h>
#include "SeggProcessor.h"
#include "SeggEditor.h"
#include <map>
#include <set>
#include <vector>
#include <cstdio>
#include <chrono>

using namespace segg;

struct FakeHead : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static int failures = 0, passes = 0;
#define CHECK(c, msg) do { if (! (c)) { std::printf ("FAIL: %s\n", msg); ++failures; } else ++passes; } while (0)

struct Ev { long long sample; int status, ch, note, vel; bool on() const { return status == 0x90; } };

struct Rig
{
    // on the heap, like in a host: the processor is too big for a Windows stack
    std::unique_ptr<SeggProcessor> owned = std::make_unique<SeggProcessor>();
    SeggProcessor& p = *owned;
    FakeHead head;
    double sr = 48000.0;
    int block = 512;
    long long clock = 0;
    std::vector<Ev> ev;

    explicit Rig (int blockSize = 512, double bpm = 120.0) : block (blockSize)
    {
        head.bpm = bpm;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);
    }
    void set (const juce::String& id, float v)
    {
        auto* r = p.state.getParameter (id);
        if (r == nullptr) { std::printf ("no parameter %s\n", id.toRawUTF8()); ++failures; return; }
        r->setValueNotifyingHost (r->convertTo0to1 (v));
    }
    void set (int track, P what, float v) { set (SeggProcessor::tid (track, what), v); }
    void solo (int track, Type type)
    {
        for (int t = 0; t < kNumTracks; ++t) set (t, pOn, t == track ? 1.0f : 0.0f);
        set (track, pType, (float) type);
    }
    /** Plays for a number of beats and records every event with its absolute sample position. */
    void run (double beats)
    {
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        const auto n = (long long) (beats * 60.0 / head.bpm * sr / block);
        for (long long b = 0; b < n; ++b)
        {
            midi.clear();
            buf.applyGain (1.0f);
            p.processBlock (buf, midi);
            for (const auto m : midi)
            {
                const auto msg = m.getMessage();
                if (msg.isNoteOn()) ev.push_back ({ clock + m.samplePosition, 0x90, msg.getChannel(), msg.getNoteNumber(), msg.getVelocity() });
                else if (msg.isNoteOff()) ev.push_back ({ clock + m.samplePosition, 0x80, msg.getChannel(), msg.getNoteNumber(), 0 });
                else ev.push_back ({ clock + m.samplePosition, msg.getRawData()[0] & 0xf0, msg.getChannel(), msg.getRawData()[1], 0 });
            }
            clock += block;
            if (head.playing) head.ppq += block * head.bpm / (60.0 * sr);
        }
    }
    void stop() { head.playing = false; run (0.1); }
    std::vector<Ev> ons (int ch = 0) const
    {
        std::vector<Ev> out;
        for (auto& e : ev) if (e.on() && (ch == 0 || e.ch == ch)) out.push_back (e);
        return out;
    }
    double samplesPerBeat() const { return 60.0 / head.bpm * sr; }
    /** Every note-on must be followed by its own note-off: nothing left hanging. */
    bool balanced() const
    {
        std::map<int, int> open;
        for (auto& e : ev)
        {
            const int key = e.ch * 128 + e.note;
            if (e.on()) { if (open[key] > 0) return false; open[key] = 1; }
            else if (e.status == 0x80) { if (open[key] == 0) return false; open[key] = 0; }
        }
        for (auto& kv : open) if (kv.second != 0) return false;
        return true;
    }
    std::vector<int> pattern (int steps, double stepBeats, int ch = 0) const
    {
        std::vector<int> hit ((size_t) steps, 0);
        for (auto& e : ons (ch))
        {
            const double s = (double) e.sample / (samplesPerBeat() * stepBeats);
            const auto idx = (long long) std::floor (s + 1.0e-6);
            if (idx >= 0 && idx < steps) hit[(size_t) idx] = 1;
        }
        return hit;
    }
};

static bool same (const std::vector<int>& a, std::initializer_list<int> b) { return a == std::vector<int> (b); }

static void testParameters()
{
    Rig r;
    std::set<juce::String> ids;
    for (auto* p : r.p.getParameters())
        if (auto* w = dynamic_cast<juce::RangedAudioParameter*> (p)) ids.insert (w->getParameterID());
    CHECK ((int) ids.size() == kNumTracks * kNumP + 10, "every parameter has a unique id");
    for (int t = 0; t < kNumTracks; ++t) for (int p = 0; p < kNumP; ++p)
        if (r.p.state.getParameter (SeggProcessor::tid (t, (P) p)) == nullptr) { CHECK (false, "track parameter missing"); return; }
    CHECK (r.p.getTailLengthSeconds() >= 3600.0, "long tail: hosts will not put the sequencer to sleep");
    CHECK (r.p.producesMidi() && r.p.acceptsMidi() && ! r.p.isMidiEffect(), "instrument that produces MIDI, loadable in Live");
}

static void testEuclid()
{
    CHECK (euclidHit (0, 3, 8) && euclidHit (3, 3, 8) && euclidHit (6, 3, 8) && ! euclidHit (1, 3, 8), "E(3,8) is the tresillo");
    int count = 0;
    for (int i = 0; i < 16; ++i) if (euclidHit (i, 7, 16)) ++count;
    CHECK (count == 7, "pulse count is exact");

    Rig r;
    r.solo (0, Euclid);
    r.set (0, pEsteps, 8); r.set (0, pEpulses, 3); r.set (0, pRate, 11);
    r.run (4.0);
    CHECK (same (r.pattern (16, 0.25), { 1,0,0,1,0,0,1,0, 1,0,0,1,0,0,1,0 }), "euclid plays E(3,8) on the 1/16 grid");

    Rig s;
    s.solo (0, Euclid);
    s.set (0, pEsteps, 8); s.set (0, pEpulses, 3); s.set (0, pErot, 2);
    s.run (2.0);
    CHECK (same (s.pattern (8, 0.25), { 1,0,1,0,0,1,0,0 }), "rotate moves the pattern to the right");

    // sample accuracy: 1/16 at 120 BPM is exactly 6000 samples, whatever the block size
    Rig a (333);
    a.solo (0, Euclid);
    a.set (0, pEsteps, 1); a.set (0, pEpulses, 1);
    a.run (4.0);
    bool exact = ! a.ons().empty();
    for (size_t i = 0; i < a.ons().size(); ++i) if (std::llabs (a.ons()[i].sample - (long long) i * 6000) > 1) exact = false;
    CHECK (exact, "notes land on the exact sample with an odd block size");

    Rig acc;
    acc.solo (0, Euclid);
    acc.set (0, pEsteps, 16); acc.set (0, pEpulses, 8); acc.set (0, pEacc, 2);
    acc.run (4.0);
    std::set<int> vels;
    for (auto& e : acc.ons()) vels.insert (e.vel);
    CHECK (vels.size() == 2, "accents make two velocity levels");
}

static void testStep()
{
    Rig r;
    r.solo (0, Step);
    r.set (0, pSlen, 4); r.set (0, pScale, 1); r.set (0, pNote, 60);
    auto& cells = r.p.engine.patterns[0].step;
    cells[0].set (1, 0); cells[1].set (0, 0); cells[2].set (1, 7); cells[3].set (1, 12);
    r.run (2.0);
    std::vector<int> notes;
    for (auto& e : r.ons()) notes.push_back (e.note);
    CHECK (notes == std::vector<int> ({ 60, 67, 72, 60, 67, 72 }), "step sequencer plays pitches and rests, wraps at Length");

    Rig d;
    d.solo (0, Step);
    d.set (0, pSlen, 4); d.set (0, pSdir, 2); d.set (0, pScale, 1); d.set (0, pNote, 60);
    for (int i = 0; i < 4; ++i) d.p.engine.patterns[0].step[(size_t) i].set (1, i);
    d.run (2.0);
    std::vector<int> pend;
    for (auto& e : d.ons()) pend.push_back (e.note - 60);
    CHECK (pend == std::vector<int> ({ 0, 1, 2, 3, 2, 1, 0, 1 }), "pendulum turns around without repeating the ends");

    Rig g;
    g.solo (0, Step);
    g.set (0, pSlen, 1);
    g.p.engine.patterns[0].step[0].set (1, 0, 100, 100, 100, 4);
    g.set (0, pScale, 1); g.set (0, pGate, 100);
    g.run (1.0);
    g.stop();
    CHECK (g.ons().size() == 16 && g.balanced(), "per-step ratchet: 4 hits inside each step, each closed");
}

/** Length of the shortest loop in the sequence of note numbers. */
static int loopLength (const std::vector<int>& v)
{
    for (int len = 1; len <= (int) v.size() / 3; ++len)
    {
        bool ok = true;
        for (size_t i = (size_t) len; i < v.size() && ok; ++i) ok = v[i] == v[i - (size_t) len];
        if (ok) return len;
    }
    return 0;
}

static void testTuring()
{
    auto notes = [] (float knob, int len) {
        Rig r;
        r.solo (0, Turing);
        r.set (0, pTlen, (float) len); r.set (0, pTknob, knob); r.set (0, pTdens, 100); r.set (0, pTrange, 48); r.set (0, pScale, 1);
        r.set (0, pNote, 36); r.set (0, pGate, 50);
        r.run (64.0);
        std::vector<int> v;
        for (auto& e : r.ons()) v.push_back (e.note);
        // skip the first bars: the register still holds its random start
        v.erase (v.begin(), v.begin() + 64);
        return v;
    };
    CHECK (loopLength (notes (100, 8)) == 8, "fully locked: loops at Length");
    CHECK (loopLength (notes (100, 13)) == 13, "fully locked: odd Length too");
    CHECK (loopLength (notes (0, 8)) == 16, "fully counter-clockwise: loop doubles, as on the module");
    CHECK (loopLength (notes (50, 8)) == 0, "centre: never settles into a loop");
    const auto slip = notes (90, 8);
    int changed = 0;
    for (size_t i = 8; i < slip.size(); ++i) if (slip[i] != slip[i - 8]) ++changed;
    CHECK (changed > 2 && changed < (int) slip.size() / 2, "in between: the loop slips, a few notes at a time");

    Rig d;
    d.solo (0, Turing);
    d.set (0, pTdens, 30);
    d.run (32.0);
    CHECK (d.ons().size() > 10 && d.ons().size() < 128 * 7 / 10, "Density thins the gates");
}

static void testCartesian()
{
    Rig r;
    r.solo (0, Cartesian);
    r.set (0, pScale, 1); r.set (0, pNote, 60); r.set (0, pCxdiv, 1); r.set (0, pCydiv, 4); r.set (0, pGate, 50);
    auto& cells = r.p.engine.patterns[0].cart;
    for (int i = 0; i < kNumCells; ++i) cells[(size_t) i].set (1, i);
    r.run (2.0);   // 8 sixteenths
    std::vector<int> a;
    for (auto& e : r.ons()) a.push_back (e.note - 60);
    CHECK (a == std::vector<int> ({ 0, 1, 2, 3, 4, 5, 6, 7 }), "X clock walks a row, Y clock moves to the next one");

    Rig x;
    x.solo (0, Cartesian);
    x.set (0, pScale, 1); x.set (0, pNote, 60); x.set (0, pCxdiv, 1); x.set (0, pCydiv, 3); x.set (0, pCw, 4); x.set (0, pChgt, 2);
    for (int i = 0; i < kNumCells; ++i) x.p.engine.patterns[0].cart[(size_t) i].set (1, i);
    x.run (3.0);   // 12 sixteenths: y changes every 3
    std::vector<int> b;
    for (auto& e : x.ons()) b.push_back (e.note - 60);
    CHECK (b == std::vector<int> ({ 0, 1, 2, 7, 4, 5, 2, 3, 0, 5, 6, 7 }), "independent axes: 4 against 3 traces a Cartesian path");

    Rig s;
    s.solo (0, Cartesian);
    s.set (0, pScale, 1); s.set (0, pNote, 60); s.set (0, pCmode, 1); s.set (0, pCpath, 1); s.set (0, pCxdiv, 1); s.set (0, pCydiv, 16);
    for (int i = 0; i < kNumCells; ++i) s.p.engine.patterns[0].cart[(size_t) i].set (1, i);
    s.run (2.0);
    std::vector<int> c;
    for (auto& e : s.ons()) c.push_back (e.note - 60);
    CHECK (c == std::vector<int> ({ 0, 1, 2, 3, 7, 6, 5, 4 }), "Snake: X walks the stored path");

    for (auto denied : { 0, 1 })
    {
        Rig v;
        v.solo (0, Cartesian);
        v.set (0, pScale, 1); v.set (0, pNote, 60); v.set (0, pCydiv, 16); v.set (0, pChgt, 1); v.set (0, pCacc, (float) denied);
        for (int i = 0; i < kNumCells; ++i) v.p.engine.patterns[0].cart[(size_t) i].set (1, i);
        v.p.engine.patterns[0].cart[1].on = 0;
        v.run (1.0);
        std::vector<int> o;
        for (auto& e : v.ons()) o.push_back (e.note - 60);
        if (denied == 0) CHECK (o == std::vector<int> ({ 0, 2, 2, 3 }), "denied pad, Seek: jumps forward to the next open pad");
        else             CHECK (o == std::vector<int> ({ 0, 2, 3 }), "denied pad, Sleep: stays silent");
    }

    bool perm = true;
    for (auto& path : kPaths)
    {
        std::set<int> seen;
        for (auto c2 : path) seen.insert (c2);
        perm = perm && seen.size() == kNumCells;
    }
    CHECK (perm, "every Snake path visits all 16 pads");
}

static void testPhysics()
{
    Rig r;
    r.solo (0, Physics);
    r.set (0, pPcount, 4); r.set (0, pPspeed, 50); r.set (0, pPmode, HitBoth); r.set (0, pScale, 1); r.set (0, pPrange, 36);
    r.run (64.0);
    std::set<int> pitches, vels;
    for (auto& e : r.ons()) { pitches.insert (e.note); vels.insert (e.vel); }
    CHECK (r.ons().size() > 50, "bouncing balls play notes");
    CHECK (pitches.size() > 8, "pitch comes from where the ball hits");
    CHECK (vels.size() > 3, "velocity comes from the impact speed");
    bool inside = true;
    for (int b = 0; b < 4; ++b)
    {
        const float x = r.p.engine.ui[0].bx[(size_t) b].load(), y = r.p.engine.ui[0].by[(size_t) b].load();
        inside = inside && x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f;
    }
    CHECK (inside, "balls stay in the box");

    Rig c;
    c.solo (0, Physics);
    c.set (0, pPcount, 1); c.set (0, pPmode, HitBalls);
    c.run (32.0);
    CHECK (c.ons().empty(), "Collisions mode: a lone ball makes no sound");

    Rig d;
    d.solo (0, Physics);
    d.set (0, pPcount, 3); d.set (0, pPgrav, 100); d.set (0, pPelast, 0); d.set (0, pPspeed, 60);
    d.run (32.0);
    const auto n1 = d.ons().size();
    d.run (32.0);
    CHECK (n1 > 0 && d.ons().size() == n1, "gravity without bounce: balls settle and go quiet, no rattling");

    Rig q;
    q.solo (0, Physics);
    q.set (0, pPcount, 8); q.set (0, pPspeed, 100); q.set (0, pPquant, 1); q.set (0, pRate, 11);
    q.run (32.0);
    bool grid = ! q.ons().empty();
    for (auto& e : q.ons()) if (e.sample % 6000 != 0) grid = false;
    CHECK (grid, "Quantize puts hits on the track grid");

    Rig f;
    f.solo (0, Physics);
    f.set (0, pPcount, 8); f.set (0, pPspeed, 100); f.set (0, pPsize, 100);
    f.run (32.0);
    CHECK (f.ons().size() <= 32 * 32 + 1, "rattling is limited to a 1/32 note, so a MIDI port cannot flood");
}

static void testSimultaneous()
{
    Rig r;
    for (int t = 0; t < kNumTracks; ++t)
    {
        r.set (t, pOn, 1);
        r.set (t, pType, (float) (t % kNumTypes));
        r.set (t, pCh, (float) (t + 1));
        r.set (t, pRate, (float) (8 + t % 7));
        r.set (t, pEpulses, 5);
        r.set (t, pTdens, 80);
        r.set (t, pPspeed, 70);
    }
    r.run (64.0);
    bool all = true;
    for (int ch = 1; ch <= 16; ++ch) if (r.ons (ch).empty()) { std::printf ("  channel %d silent\n", ch); all = false; }
    CHECK (all, "all 16 tracks play at once, each sequencer type on its own channel");

    // polymeter: 1/16 and 1/16T keep their own grids at the same time
    Rig p;
    for (int t = 0; t < 3; ++t) p.set (t, pOn, 1);
    for (int t = 0; t < 3; ++t) { p.set (t, pEsteps, 1); p.set (t, pEpulses, 1); }
    p.set (0, pRate, 11); p.set (1, pRate, 12); p.set (2, pRate, 16);
    p.run (4.0);
    CHECK (p.ons (1).size() == 16 && p.ons (2).size() == 24 && p.ons (3).size() == 20, "independent rates: 16, 24 and 20 hits in the same bar");
    bool triplets = true;
    for (size_t i = 0; i < p.ons (2).size(); ++i) if (std::llabs (p.ons (2)[i].sample - (long long) i * 4000) > 1) triplets = false;
    CHECK (triplets, "the triplet track stays exact next to the others");
}

static void testLinks()
{
    Rig c;
    c.set (0, pOn, 1); c.set (0, pEsteps, 16); c.set (0, pEpulses, 4);
    c.set (1, pOn, 1); c.set (1, pType, Step); c.set (1, pSlen, 3); c.set (1, pLsrc, 1); c.set (1, pLmode, LinkClock);
    c.set (1, pScale, 1); c.set (1, pNote, 60);
    for (int i = 0; i < 3; ++i) c.p.engine.patterns[1].step[(size_t) i].set (1, i);
    c.run (8.0);
    std::vector<int> notes; bool together = true;
    for (size_t i = 0; i < c.ons (2).size(); ++i)
    {
        notes.push_back (c.ons (2)[i].note - 60);
        if (c.ons (2)[i].sample != c.ons (1)[i].sample) together = false;
    }
    CHECK (notes == std::vector<int> ({ 0, 1, 2, 0, 1, 2, 0, 1 }), "Clock link: the follower advances only when the source fires");
    CHECK (together, "Clock link is sample accurate");

    Rig s;
    s.set (0, pOn, 1); s.set (0, pEsteps, 8); s.set (0, pEpulses, 1);
    s.set (1, pOn, 1); s.set (1, pType, Step); s.set (1, pSlen, 16); s.set (1, pLsrc, 1); s.set (1, pLmode, LinkReset);
    s.set (1, pScale, 1); s.set (1, pNote, 60);
    for (int i = 0; i < 16; ++i) s.p.engine.patterns[1].step[(size_t) i].set (1, i);
    s.run (4.0);
    int top = 0;
    for (auto& e : s.ons (2)) top = juce::jmax (top, e.note - 60);
    CHECK (top <= 8, "Reset link: the follower restarts on every source hit");

    Rig t;
    t.set (0, pOn, 1); t.set (0, pType, Step); t.set (0, pRate, 5); t.set (0, pSlen, 2); t.set (0, pScale, 1);
    t.p.engine.patterns[0].step[0].set (1, 0); t.p.engine.patterns[0].step[1].set (1, 5);
    t.set (1, pOn, 1); t.set (1, pEsteps, 1); t.set (1, pEpulses, 1); t.set (1, pRate, 5); t.set (1, pLsrc, 1); t.set (1, pLmode, LinkTranspose);
    t.set (1, pScale, 1); t.set (1, pNote, 60);
    t.run (4.0);
    std::vector<int> tr;
    for (auto& e : t.ons (2)) tr.push_back (e.note);
    CHECK (tr == std::vector<int> ({ 60, 65, 60, 65 }), "Transpose link: the follower moves with the source pitch");

    Rig f;
    f.set (0, pOn, 1); f.set (0, pRate, 5); f.set (0, pEsteps, 4); f.set (0, pEpulses, 1);
    f.set (1, pOn, 1); f.set (1, pRate, 5); f.set (1, pEsteps, 1); f.set (1, pEpulses, 1); f.set (1, pLsrc, 1); f.set (1, pLmode, LinkGate);
    f.run (16.0);
    CHECK (f.ons (2).size() == 8, "Follow: plays only right after the source");

    Rig k;
    k.set (0, pOn, 1); k.set (0, pRate, 5); k.set (0, pEsteps, 4); k.set (0, pEpulses, 1);
    k.set (1, pOn, 1); k.set (1, pRate, 5); k.set (1, pEsteps, 1); k.set (1, pEpulses, 1); k.set (1, pLsrc, 1); k.set (1, pLmode, LinkChoke);
    k.run (16.0);
    CHECK (k.ons (2).size() == 8, "Choke: the source silences the follower");

    Rig loop;   // two tracks clocking each other must not lock up
    loop.set (0, pOn, 1); loop.set (0, pLsrc, 2); loop.set (0, pLmode, LinkClock);
    loop.set (1, pOn, 1); loop.set (1, pLsrc, 1); loop.set (1, pLmode, LinkClock);
    loop.set (2, pOn, 1); loop.set (2, pLsrc, 3); loop.set (2, pLmode, LinkClock);
    loop.run (8.0);
    CHECK (true, "circular links do not hang");
}

static void testScale()
{
    Rig r;
    r.set ("groot", 2); r.set ("gscale", 3);   // D dorian
    r.solo (0, Turing);
    r.set (0, pTknob, 50); r.set (0, pTdens, 100); r.set (0, pTrange, 48); r.set (0, pChord, 4);
    r.run (64.0);
    const auto mask = rotatedMask (3, 2);
    bool inScale = ! r.ons().empty();
    for (auto& e : r.ons()) if (((mask >> (e.note % 12)) & 1) == 0) inScale = false;
    CHECK (inScale, "melody and chord tones stay in the scale");
    CHECK (r.ons().size() >= 4 * 200, "Seventh chord: four notes per hit");

    CHECK (snapToScale (61, rotatedMask (1, 0)) == 60, "snap goes to the nearest note, ties downwards");
    Rig d;
    d.set ("gscale", 2); d.solo (0, Euclid);
    d.set (0, pScale, 1); d.set (0, pNote, 37);
    d.run (1.0);
    CHECK (! d.ons().empty() && d.ons()[0].note == 37, "track scale Chromatic leaves a drum note alone");
}

static void testTiming()
{
    // gate, swing, delay
    Rig r;
    r.solo (0, Euclid);
    r.set (0, pEsteps, 1); r.set (0, pEpulses, 1); r.set (0, pGate, 25); r.set (0, pSwing, 100); r.set (0, pDelay, 10);
    r.run (2.0);
    const auto o = r.ons();
    CHECK (o.size() == 8 && o[0].sample == 480, "Delay shifts notes by exactly 10 ms");
    CHECK (std::llabs (o[1].sample - (6000 + 3000 + 480)) <= 1, "full Swing moves the off-step half a step later");
    long long off = -1;
    for (auto& e : r.ev) if (e.status == 0x80) { off = e.sample; break; }
    CHECK (std::llabs (off - (480 + 1500)) <= 1, "Gate 25 % is a quarter of the step");

    // tempo change while playing: the grid follows
    Rig t;
    t.solo (0, Euclid);
    t.set (0, pEsteps, 1); t.set (0, pEpulses, 1); t.set (0, pRate, 5);
    t.run (4.0);
    t.head.bpm = 60;
    t.run (4.0);
    const auto q = t.ons();
    CHECK (q.size() == 8 && std::llabs ((q[7].sample - q[6].sample) - 48000) < 600, "follows tempo changes");

    // loop wrap in the arrangement
    Rig l;
    l.solo (0, Euclid);
    l.set (0, pEsteps, 16); l.set (0, pEpulses, 4); l.set (0, pGate, 400); l.set (0, pRate, 11);
    l.run (4.0);
    l.head.ppq = 0.0;
    l.run (4.0);
    CHECK (l.ons().size() == 8, "loop wrap: no lost or doubled hits");

    // stop in the middle of long gates
    Rig s;
    s.set (0, pOn, 1); s.set (0, pEsteps, 1); s.set (0, pEpulses, 1); s.set (0, pGate, 800); s.set (0, pRate, 5); s.set (0, pChord, 3);
    s.run (2.5);
    s.stop();
    CHECK (s.balanced(), "stopping the transport closes every open note");
    CHECK (s.p.engine.numSounding() == 0 && s.p.engine.numPending() == 0, "nothing left pending after stop");

    // overlapping gates on the same pitch: note-off of the first must not cut the second
    Rig v;
    v.solo (0, Euclid);
    v.set (0, pEsteps, 1); v.set (0, pEpulses, 1); v.set (0, pGate, 300);
    v.run (2.0);
    v.stop();
    CHECK (v.balanced(), "overlapping gates retrigger cleanly");

    // bypass
    Rig b;
    b.set (0, pOn, 1); b.set (0, pGate, 800); b.set (0, pRate, 5); b.set (0, pEpulses, 16);
    b.run (1.5);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
    b.p.processBlockBypassed (buf, midi);
    CHECK (midi.getNumEvents() > 0 && b.p.engine.numSounding() == 0, "bypass releases the notes");

    // playing from a later position keeps the pattern where the song puts it
    Rig pos;
    pos.solo (0, Euclid);
    pos.set (0, pEsteps, 8); pos.set (0, pEpulses, 3);
    pos.head.ppq = 16.75;   // sixteenth 67, index 3 of the 8-step pattern
    pos.run (0.1);
    CHECK (pos.ons().size() == 1 && pos.ons()[0].sample == 0, "starting mid-song lands on the right step");

    // panic
    Rig n;
    n.p.panic();
    n.run (0.1);
    int cc = 0;
    for (auto& e : n.ev) if (e.status == 0xB0) ++cc;
    CHECK (cc >= 32, "Panic sends all-notes-off on the host output");
}

static std::vector<Ev> render (int block, float seedOffset)
{
    Rig r (block);
    for (int t = 0; t < kNumTracks; ++t)
    {
        r.set (t, pOn, 1); r.set (t, pType, (float) (t % kNumTypes)); r.set (t, pCh, (float) (t + 1));
        r.set (t, pMutate, 30); r.set (t, pProb, 80); r.set (t, pRoll, 20); r.set (t, pSeed, (float) t + seedOffset);
        r.set (t, pPspeed, 60); r.set (t, pDrift, 20);
    }
    r.run (64.0);
    return r.ons();
}

static void testDeterminism()
{
    auto same2 = [] (const std::vector<Ev>& a, const std::vector<Ev>& b, long long tol) {
        if (a.size() != b.size()) return false;
        std::multiset<std::pair<int, int>> x, y;
        for (size_t i = 0; i < a.size(); ++i) { x.insert ({ a[i].ch, a[i].note }); y.insert ({ b[i].ch, b[i].note }); }
        if (x != y) return false;
        std::vector<long long> ta, tb;
        for (auto& e : a) ta.push_back (e.sample);
        for (auto& e : b) tb.push_back (e.sample);
        std::sort (ta.begin(), ta.end()); std::sort (tb.begin(), tb.end());
        for (size_t i = 0; i < ta.size(); ++i) if (std::llabs (ta[i] - tb[i]) > tol) return false;
        return true;
    };
    const auto a = render (512, 0);
    CHECK (same2 (a, render (512, 0), 0), "same seed, same music: bounces and takes match");
    CHECK (same2 (a, render (64, 0), 2), "same music whatever the audio buffer size");
    CHECK (! same2 (a, render (512, 100), 2), "a different Seed gives different music");
}

static void testState()
{
    Rig a;
    a.set (3, pType, Cartesian); a.set (3, pRate, 7); a.set ("gchaos", 42);
    a.p.engine.patterns[3].cart[5].set (0, -12, 77, 88, 66, 3);
    a.p.engine.patterns[9].step[15].set (1, 24, 120, 60, 50, 2);
    juce::MemoryBlock mb;
    a.p.getStateInformation (mb);
    Rig b;
    b.p.setStateInformation (mb.getData(), (int) mb.getSize());
    auto& c = b.p.engine.patterns[3].cart[5];
    auto& s = b.p.engine.patterns[9].step[15];
    CHECK (c.on.load() == 0 && c.pitch.load() == -12 && c.vel.load() == 77 && c.gate.load() == 88 && c.prob.load() == 66 && c.ratch.load() == 3, "pads survive save and reload");
    CHECK (s.pitch.load() == 24 && s.ratch.load() == 2, "step lanes survive save and reload");
    CHECK ((int) b.p.state.getRawParameterValue (SeggProcessor::tid (3, pType))->load() == Cartesian, "parameters survive save and reload");
    CHECK (std::abs (b.p.state.getRawParameterValue ("gchaos")->load() - 42.0f) < 0.01f, "global parameters survive save and reload");

    // a session saved on preset 3 must not be overwritten when the host asks for that program again
    Rig e;
    e.p.presets.load (3);
    e.set (0, pVel, 33);
    juce::MemoryBlock session;
    e.p.getStateInformation (session);
    Rig f;
    f.p.setStateInformation (session.getData(), (int) session.getSize());
    CHECK (f.p.getCurrentProgram() == 3 && f.p.ti (0, pVel) == 33, "reopening a session keeps both the preset name and the edits");

    // garbage must not crash
    const char junk[] = "not a state";
    b.p.setStateInformation (junk, (int) sizeof (junk));
    CHECK (true, "unreadable state is ignored");

    // user preset round trip, in a temporary name
    a.p.presets.save ("__segg_test");
    const int idx = a.p.presets.getCurrent();
    Rig d;
    d.p.presets.refresh();
    CHECK (d.p.presets.getNames().contains ("__segg_test"), "user preset is listed");
    d.p.presets.load (d.p.presets.getNames().indexOf ("__segg_test"));
    CHECK (d.p.engine.patterns[3].cart[5].vel.load() == 77, "user preset restores patterns");
    a.p.presets.removeCurrent();
    juce::ignoreUnused (idx);
}

static void testPresets()
{
    auto count = (int) factoryPresets().size();
    for (int i = 0; i < count; ++i)
    {
        Rig r;
        r.p.presets.load (i);
        for (int rep = 0; rep < 2; ++rep) r.run (48.0);
        r.stop();
        const juce::String name = factoryPresets()[(size_t) i].name;
        bool host = false;
        for (int t = 0; t < kNumTracks; ++t) if (r.p.engine.params[(size_t) t].i (pOn) != 0 && r.p.engine.params[(size_t) t].i (pPort) == 0) host = true;
        if (host) CHECK (! r.ons().empty(), (name + ": plays").toRawUTF8());
        CHECK (r.balanced(), (name + ": no hanging notes").toRawUTF8());
        double seconds = 96.0 * 60.0 / r.head.bpm;
        CHECK ((double) r.ev.size() / seconds < 900.0, (name + ": keeps a sane MIDI rate").toRawUTF8());
        std::printf ("  %-18s %5d notes on host in %.0f s\n", name.toRawUTF8(), (int) r.ons().size(), seconds);
    }
}

static void testRouting()
{
    Rig r;
    r.solo (0, Euclid);
    r.set (0, pPort, 3); r.set (0, pEpulses, 16);
    r.run (2.0);
    CHECK (r.ev.empty(), "a track on a port does not also reach the host");

    Rig s;
    s.set (0, pOn, 1); s.set (0, pCh, 5);
    s.run (2.0);
    CHECK (! s.ons (5).empty() && s.ons (1).empty(), "channel selection reaches the host output");
}

static void testDirectPorts()
{
    // the path that matters for hardware: from the audio thread, through the FIFO, out in order and on time
    struct Got { int slot, status, note; double ms; };
    std::vector<Got> got;
    juce::CriticalSection lock;
    {
        Rig r;
        r.p.direct.attachTap ([&] (int slot, const juce::MidiMessage& m) {
            const juce::ScopedLock sl (lock);
            got.push_back ({ slot, m.getRawData()[0], m.getNoteNumber(), juce::Time::getMillisecondCounterHiRes() });
        });
        r.solo (0, Euclid);
        r.set (0, pPort, 2); r.set (0, pCh, 7); r.set (0, pEsteps, 1); r.set (0, pEpulses, 1); r.set (0, pRate, 5);
        r.set (0, pGate, 50); r.set (0, pScale, 1); r.set (0, pNote, 64);
        // feed blocks in real time, the way a host does
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        const double start = juce::Time::getMillisecondCounterHiRes(), blockMs = 512.0 / 48.0;
        for (int b = 0; b < 180; ++b)
        {
            midi.clear();
            r.p.processBlock (buf, midi);
            r.head.ppq += 512 * r.head.bpm / (60.0 * r.sr);
            juce::Time::waitForMillisecondCounter ((juce::uint32) (start + (b + 1) * blockMs));
        }
        juce::Thread::sleep (200);
        CHECK (midi.getNumEvents() == 0, "direct notes stay off the host output");
    }
    const juce::ScopedLock sl (lock);
    std::vector<double> ons;
    bool routed = true;
    for (auto& g : got)
    {
        if (g.slot != 1 || g.note != 64 || (g.status & 15) != 6) routed = false;
        if ((g.status & 0xf0) == 0x90) ons.push_back (g.ms);
    }
    CHECK (ons.size() >= 3, "notes reach the port");
    CHECK (routed, "port 2, channel 7: slot and channel arrive intact");
    double worst = 0;
    for (size_t i = 1; i < ons.size(); ++i) worst = juce::jmax (worst, std::abs (ons[i] - ons[i - 1] - 500.0));
    std::printf ("  direct port jitter: worst %.2f ms over %d quarter notes\n", worst, (int) ons.size());
    CHECK (worst < 30.0, "a steady pulse stays steady through the dispatch thread");   // loose: CI machines are busy
}

static void testSpeed()
{
    Rig r (32);
    for (int t = 0; t < kNumTracks; ++t)
    {
        r.set (t, pOn, 1); r.set (t, pType, Physics); r.set (t, pPcount, 8); r.set (t, pPspeed, 100); r.set (t, pChord, 4);
    }
    const auto t0 = std::chrono::steady_clock::now();
    r.run (120.0);
    const double used = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double load = used / (120.0 * 60.0 / r.head.bpm) * 100.0;
    std::printf ("  worst case load: %.2f %% of one core at 32 samples\n", load);
    CHECK (load < 10.0, "16 physics tracks run far faster than real time");
    CHECK (r.p.engine.numPending() < Engine::kPoolSize, "event pool never fills up");
}

static void runSnap (const juce::String& path, int track, int page, int preset, float scale);

int main (int argc, char** argv)
{
    if (argc > 1 && juce::String (argv[1]) == "--snap")
    {
        runSnap (argc > 2 ? juce::String (argv[2]) : juce::String ("snap_segg.png"), argc > 3 ? juce::String (argv[3]).getIntValue() : 1,
                 argc > 4 ? juce::String (argv[4]).getIntValue() : 0, argc > 5 ? juce::String (argv[5]).getIntValue() : 2,
                 argc > 6 ? juce::String (argv[6]).getFloatValue() : 1.0f);
        return 0;
    }
    juce::ScopedJuceInitialiser_GUI init;
    testParameters();
    testEuclid();
    testStep();
    testTuring();
    testCartesian();
    testPhysics();
    testSimultaneous();
    testLinks();
    testScale();
    testTiming();
    testDeterminism();
    testState();
    testPresets();
    testRouting();
    testDirectPorts();
    testSpeed();
    std::printf ("%d checks passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}

static void runSnap (const juce::String& path, int track, int page, int preset, float scale)
{
    juce::ScopedJuceInitialiser_GUI init;
    {
        Rig r (512, 120.0);
        r.p.presets.load (preset);
        r.run (20.0);
        std::unique_ptr<juce::AudioProcessorEditor> ed (r.p.createEditor());
        if (auto* oe = dynamic_cast<SeggEditor*> (ed.get()))
        {
            oe->selectTrack (track - 1);
            oe->showRouting (page == 1);
            if (page >= 10) oe->setLaneForSnapshot (page - 10);
            oe->tickForSnapshot();
        }
        ed->setSize (juce::roundToInt (1280 * scale), juce::roundToInt (800 * scale));
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (path));
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, out);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
    juce::Thread::sleep (50);
}
