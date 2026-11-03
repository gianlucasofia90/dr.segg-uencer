#include "SeggProcessor.h"
#include "SeggEditor.h"
#include "Params.h"

using namespace segg;

juce::String SeggProcessor::tid (int track, P p)
{
    return "t" + juce::String (track + 1).paddedLeft ('0', 2) + "_" + kParamIds[p];
}

static juce::String noteName (int n) { return juce::String (kNoteNames[n % 12]) + juce::String (n / 12 - 2); }

juce::AudioProcessorValueTreeState::ParameterLayout SeggProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray types, rates, roots, scales, trackScales, chords, dirs, links, paths, hits, ports, sources;
    for (auto* s : kTypeNames) types.add (s);
    for (auto& r : kRates) rates.add (r.name);
    for (auto* s : kNoteNames) roots.add (s);
    trackScales.add ("Global");
    for (auto& s : kScales) { scales.add (s.name); trackScales.add (s.name); }
    for (auto* s : kChordNames) chords.add (s);
    for (auto* s : kDirNames) dirs.add (s);
    for (auto* s : kLinkNames) links.add (s);
    for (auto* s : kPathNames) paths.add (s);
    for (auto* s : kHitNames) hits.add (s);
    ports.add ("Host");
    for (int i = 1; i <= kNumSlots; ++i) ports.add ("Port " + juce::String (i));
    sources.add ("Off");
    for (int i = 1; i <= kNumTracks; ++i) sources.add ("T" + juce::String (i));

    // global
    l.add (cparam ("gsync", "Sync", { "DAW", "FREE" }, 0));
    l.add (fparam ("gbpm", "Free BPM", 20, 300, 120, "bpm"));
    l.add (bparam ("grun", "Free Run", false));
    l.add (cparam ("groot", "Root", roots, 0));
    l.add (cparam ("gscale", "Scale", scales, 3));
    l.add (fparam ("gdensity", "Density", 0, 200, 100, "%"));
    l.add (fparam ("gchaos", "Chaos", 0, 100, 0, "%"));
    l.add (bparam ("greset", "Reset on Play", true));
    l.add (bparam ("gkeyin", "MIDI Transpose", false));
    l.add (fparam ("goffset", "Port Offset", 0, 200, 0, "ms"));

    // tracks: the order follows enum segg::P
    for (int t = 0; t < kNumTracks; ++t)
    {
        const juce::String nm = "T" + juce::String (t + 1) + " ";
        auto id = [t] (P p) { return tid (t, p); };
        l.add (bparam (id (pOn), nm + "On", t == 0));
        l.add (cparam (id (pType), nm + "Type", types, 0));
        l.add (cparam (id (pPort), nm + "Port", ports, 0));
        l.add (iparam (id (pCh), nm + "Channel", 1, 16, t + 1));
        l.add (cparam (id (pRate), nm + "Rate", rates, kDefaultRate));
        l.add (fparam (id (pSwing), nm + "Swing", 0, 100, 0, "%"));
        l.add (fparam (id (pDrift), nm + "Drift", 0, 100, 0, "%"));
        l.add (fparam (id (pGate), nm + "Gate", 1, 800, 50, "%", 100));
        l.add (fparam (id (pVel), nm + "Velocity", 1, 127, 100));
        l.add (fparam (id (pVrand), nm + "Vel Random", 0, 100, 0, "%"));
        l.add (fparam (id (pProb), nm + "Chance", 0, 100, 100, "%"));
        l.add (fparam (id (pRoll), nm + "Roll", 0, 100, 0, "%"));
        l.add (iparam (id (pRolln), nm + "Roll Hits", 2, 8, 3));
        l.add (iparam (id (pNote), nm + "Note", 0, 127, 48, [] (int v, int) { return noteName (v); }));
        l.add (cparam (id (pScale), nm + "Scale", trackScales, 0));
        l.add (cparam (id (pChord), nm + "Chord", chords, 0));
        l.add (fparam (id (pMutate), nm + "Mutate", 0, 100, 0, "%"));
        l.add (cparam (id (pLsrc), nm + "Link From", sources, 0));
        l.add (cparam (id (pLmode), nm + "Link Mode", links, 0));
        l.add (fparam (id (pDelay), nm + "Delay", 0, 200, 0, "ms"));
        l.add (iparam (id (pSeed), nm + "Seed", 0, 9999, t + 1));
        // euclid
        l.add (iparam (id (pEsteps), nm + "Euclid Steps", 1, kMaxEuclid, 16));
        l.add (iparam (id (pEpulses), nm + "Euclid Pulses", 0, kMaxEuclid, 4));
        l.add (iparam (id (pErot), nm + "Euclid Rotate", 0, kMaxEuclid - 1, 0));
        l.add (iparam (id (pEacc), nm + "Euclid Accents", 0, kMaxEuclid, 0));
        l.add (iparam (id (pEmel), nm + "Euclid Melody", 1, kNumCells, 1));
        // step
        l.add (iparam (id (pSlen), nm + "Step Length", 1, kNumCells, 16));
        l.add (cparam (id (pSdir), nm + "Step Direction", dirs, 0));
        // turing
        l.add (iparam (id (pTlen), nm + "Turing Length", 2, 32, 16));
        l.add (fparam (id (pTknob), nm + "Turing Lock", 0, 100, 75, "%"));
        l.add (iparam (id (pTrange), nm + "Turing Range", 0, 48, 12));
        l.add (fparam (id (pTdens), nm + "Turing Density", 0, 100, 60, "%"));
        // cartesian
        l.add (cparam (id (pCmode), nm + "Cart Mode", { "Free XY", "Snake" }, 0));
        l.add (cparam (id (pCpath), nm + "Cart Path", paths, 0));
        l.add (iparam (id (pCxdiv), nm + "Cart X Clock", 1, 16, 1));
        l.add (iparam (id (pCydiv), nm + "Cart Y Clock", 1, 16, 4));
        l.add (iparam (id (pCw), nm + "Cart Width", 1, 4, 4));
        l.add (iparam (id (pChgt), nm + "Cart Height", 1, 4, 4));
        l.add (cparam (id (pCacc), nm + "Cart Denied", { "Seek", "Sleep" }, 0));
        l.add (cparam (id (pCtrig), nm + "Cart Trigger", { "X", "Y", "X+Y" }, 0));
        // physics
        l.add (iparam (id (pPcount), nm + "Phys Balls", 1, kMaxBalls, 3));
        l.add (fparam (id (pPspeed), nm + "Phys Speed", 0, 100, 30, "%"));
        l.add (fparam (id (pPgrav), nm + "Phys Gravity", 0, 100, 0, "%"));
        l.add (fparam (id (pPelast), nm + "Phys Bounce", 0, 100, 100, "%"));
        l.add (cparam (id (pPmode), nm + "Phys Notes", hits, HitBoth));
        l.add (iparam (id (pPrange), nm + "Phys Range", 0, 48, 24));
        l.add (fparam (id (pPsize), nm + "Phys Size", 0, 100, 30, "%"));
        l.add (bparam (id (pPquant), nm + "Phys Quantize", false));
        l.add (fparam (id (pPkick), nm + "Phys Kick", 0, 100, 0, "%"));
        // output
        l.add (bparam (id (pNotes), nm + "Notes", true));
    }
    return l;
}

SeggProcessor::SeggProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "DRSEGG", createLayout()),
      presets (*this)
{
    for (int t = 0; t < kNumTracks; ++t)
        for (int p = 0; p < kNumP; ++p)
            trackPtr[(size_t) t][(size_t) p] = state.getRawParameterValue (tid (t, (P) p));
    gSync = state.getRawParameterValue ("gsync");       gBpm = state.getRawParameterValue ("gbpm");
    gRun = state.getRawParameterValue ("grun");         gRoot = state.getRawParameterValue ("groot");
    gScale = state.getRawParameterValue ("gscale");     gDensity = state.getRawParameterValue ("gdensity");
    gChaos = state.getRawParameterValue ("gchaos");     gReset = state.getRawParameterValue ("greset");
    gKeyIn = state.getRawParameterValue ("gkeyin");     gOffset = state.getRawParameterValue ("goffset");
    pullParams();
    presets.resetPatterns();
    startTimerHz (60);
}

bool SeggProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SeggProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    pullParams();
    engine.prepare (sampleRate);
    internalRunning = false;
    portsReady.store (true);
}

void SeggProcessor::pullParams() noexcept
{
    for (size_t t = 0; t < kNumTracks; ++t)
        for (size_t p = 0; p < kNumP; ++p)
            engine.params[t].v[p] = trackPtr[t][p]->load (std::memory_order_relaxed);
    auto& g = engine.global;
    g.root = (int) gRoot->load();
    g.scale = (int) gScale->load();
    g.density = gDensity->load();
    g.chaos = gChaos->load();
    g.resetOnPlay = gReset->load() > 0.5f;
}

/** Hands the engine's MIDI to the host buffer (port 0) or to the direct ports. */
struct SeggProcessor::Sink
{
    juce::MidiBuffer& host;
    DirectMidi& direct;
    double startMs, msPerSample;

    void emit (int offset, int port, int status, int d1, int d2) noexcept
    {
        if (port == 0)
        {
            const juce::uint8 b[3] = { (juce::uint8) status, (juce::uint8) d1, (juce::uint8) d2 };
            host.addEvent (b, 3, offset);
        }
        else direct.push (port - 1, startMs + offset * msPerSample, status, d1, d2);
    }
};

void SeggProcessor::runBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // incoming notes transpose the sequencers, relative to C3 (MIDI 60)
    const bool keyIn = gKeyIn->load() > 0.5f;
    for (const auto m : midi)
    {
        if (m.numBytes < 2) continue;
        const int status = m.data[0], d1 = m.data[1], d2 = m.numBytes > 2 ? m.data[2] : 0;
        handleLearn (status, d1, d2);
        const bool taken = applyInputMap (inMap, status, d1, d2, [this] (int param, float norm) {
            const auto w = mapFifo.write (1);       // dropped if the message thread has stalled: the next value replaces it
            if (w.blockSize1 + w.blockSize2 > 0) mapValues[(size_t) (w.blockSize1 > 0 ? w.startIndex1 : w.startIndex2)] = { param, norm };
        });
        if (keyIn && ! taken && (status & 0xf0) == 0x90 && d2 > 0) heldKey = d1;
    }
    midi.clear();
    pullParams();
    engine.global.keyTranspose = keyIn && heldKey >= 0 ? heldKey - 60 : 0;

    Transport t;
    t.sampleRate = sr;
    t.numSamples = buffer.getNumSamples();

    const bool freeMode = gSync->load() > 0.5f;
    if (freeMode)
    {
        const bool run = gRun->load() > 0.5f;
        if (run && ! internalRunning) internalPpq = 0.0;
        internalRunning = run;
        t.playing = run;
        t.bpm = juce::jmax (1.0f, gBpm->load());
        t.ppq = internalPpq;
        if (run) internalPpq += t.numSamples * t.bpm / (60.0 * sr);
    }
    else
    {
        internalRunning = false;
        juce::Optional<juce::AudioPlayHead::PositionInfo> pos;
        if (auto* ph = getPlayHead()) pos = ph->getPosition();
        if (pos.hasValue())
        {
            t.playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
            t.ppq = pos->getPpqPosition().orFallback (0.0);
            t.bpm = pos->getBpm().orFallback (120.0);
            if (t.bpm <= 0.0) t.bpm = 120.0;
        }
    }
    if (bypassed) t.playing = false;
    uiPlaying.store (t.playing, std::memory_order_relaxed);
    uiBpm.store ((float) t.bpm, std::memory_order_relaxed);

    Sink sink { midi, direct, direct.blockStartMs (t.numSamples, sr) + gOffset->load(), 1000.0 / sr };
    engine.process (t, sink);
    if (panicRequested.exchange (false)) engine.panic (sink);
}

void SeggProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) { runBlock (buffer, midi, false); }
void SeggProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) { runBlock (buffer, midi, true); }

// ---------------- input map

void SeggProcessor::handleLearn (int status, int d1, int d2) noexcept
{
    const int row = learnRow.load (std::memory_order_relaxed);
    if (row < 0 || row >= kNumMapRows) return;
    const int kind = status & 0xf0;
    const bool noteOn = kind == 0x90 && d2 > 0;
    if (! noteOn && kind != 0xB0 && kind != 0xE0) return;
    auto& r = inMap[(size_t) row];
    r.channel = (status & 15) + 1;
    r.number = kind == 0xE0 ? 0 : d1;
    r.mode = noteOn ? ModeToggle : ModeAbsolute;     // a key switches, a knob or a wheel sweeps
    r.toggled = 0;
    r.type = noteOn ? InNote : (kind == 0xB0 ? InCC : InPitch);
    learnRow.store (-1);
}

int SeggProcessor::findMapRow (const juce::String& paramId) const
{
    if (auto* prm = state.getParameter (paramId))
        for (int i = 0; i < kNumMapRows; ++i)
            if (inMap[(size_t) i].param.load() == prm->getParameterIndex()) return i;
    return -1;
}

int SeggProcessor::startLearn (const juce::String& paramId)
{
    auto* prm = state.getParameter (paramId);
    if (prm == nullptr) return -1;
    int row = findMapRow (paramId);
    for (int i = 0; i < kNumMapRows && row < 0; ++i)
        if (inMap[(size_t) i].param.load() < 0) row = i;
    if (row < 0) return -1;
    inMap[(size_t) row].param = prm->getParameterIndex();
    learnRow.store (row);
    return row;
}

void SeggProcessor::clearMapRow (int row)
{
    if (learnRow.load() == row) learnRow.store (-1);
    inMap[(size_t) row].clear();
}

void SeggProcessor::pumpMidiMap()
{
    const auto r = mapFifo.read (mapFifo.getNumReady());
    const auto& params = getParameters();
    auto apply = [&] (int start, int n) {
        for (int i = 0; i < n; ++i)
        {
            const auto v = mapValues[(size_t) (start + i)];
            if (auto* prm = params[v.param])
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (v.norm);
                prm->endChangeGesture();
            }
        }
    };
    apply (r.startIndex1, r.blockSize1);
    apply (r.startIndex2, r.blockSize2);
}

// ---------------- state

juce::ValueTree SeggProcessor::patternsToTree() const
{
    juce::ValueTree tree ("PATTERNS");
    auto cells = [] (const std::array<Cell, kNumCells>& a) {
        juce::StringArray out;
        for (auto& c : a)
            out.add (juce::String ((int) c.on.load()) + " " + juce::String ((int) c.pitch.load()) + " " + juce::String ((int) c.vel.load()) + " "
                     + juce::String ((int) c.gate.load()) + " " + juce::String ((int) c.prob.load()) + " " + juce::String ((int) c.ratch.load()));
        return out.joinIntoString (";");
    };
    for (int t = 0; t < kNumTracks; ++t)
    {
        juce::ValueTree node ("TRACK");
        node.setProperty ("step", cells (engine.patterns[(size_t) t].step), nullptr);
        node.setProperty ("cart", cells (engine.patterns[(size_t) t].cart), nullptr);
        tree.appendChild (node, nullptr);
    }
    return tree;
}

void SeggProcessor::patternsFromTree (const juce::ValueTree& tree)
{
    auto cells = [] (std::array<Cell, kNumCells>& a, const juce::String& text) {
        auto rows = juce::StringArray::fromTokens (text, ";", {});
        for (int i = 0; i < juce::jmin (rows.size(), kNumCells); ++i)
        {
            auto f = juce::StringArray::fromTokens (rows[i], " ", {});
            if (f.size() < 6) continue;
            a[(size_t) i].set (f[0].getIntValue() != 0 ? 1 : 0, juce::jlimit (-48, 48, f[1].getIntValue()), juce::jlimit (1, 127, f[2].getIntValue()),
                               juce::jlimit (1, 127, f[3].getIntValue()), juce::jlimit (0, 100, f[4].getIntValue()), juce::jlimit (1, 8, f[5].getIntValue()));
        }
    };
    for (int t = 0; t < juce::jmin (tree.getNumChildren(), kNumTracks); ++t)
    {
        const auto node = tree.getChild (t);
        cells (engine.patterns[(size_t) t].step, node["step"].toString());
        cells (engine.patterns[(size_t) t].cart, node["cart"].toString());
    }
}

juce::ValueTree SeggProcessor::portsToTree() const
{
    juce::ValueTree tree ("PORTS");
    for (int i = 0; i < kNumSlots; ++i) tree.setProperty ("slot" + juce::String (i), direct.getSlot (i), nullptr);
    return tree;
}

void SeggProcessor::portsFromTree (const juce::ValueTree& tree)
{
    for (int i = 0; i < kNumSlots; ++i) setPortSlot (i, tree["slot" + juce::String (i)].toString());
}

void SeggProcessor::setPortSlot (int slot, const juce::String& deviceId)
{
    if (direct.getSlot (slot) != deviceId) direct.setSlot (slot, deviceId);
}

void SeggProcessor::assignPort (int slot, const juce::String& deviceId)
{
    portsPending.store (false);
    setPortSlot (slot, deviceId);
    PortIds ids;
    for (int i = 0; i < kNumSlots; ++i) ids[(size_t) i] = direct.getSlot (i);
    PortMemory::save (ids, juce::MidiOutput::getAvailableDevices());
}

void SeggProcessor::restoreLastPorts()
{
    if (! portsPending.exchange (false)) return;
    PortChoices last;
    if (! PortMemory::load (last)) return;
    const auto ids = PortMemory::match (last, juce::MidiOutput::getAvailableDevices());
    for (int i = 0; i < kNumSlots; ++i) setPortSlot (i, ids[(size_t) i]);
}

juce::ValueTree SeggProcessor::inMapToTree() const
{
    juce::ValueTree tree ("INMAP");
    const auto& params = getParameters();
    for (auto& row : inMap)
    {
        juce::ValueTree node ("ROW");
        juce::String id;     // by id: parameter indexes can move between versions
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (params[row.param.load()])) id = prm->getParameterID();
        node.setProperty ("type", row.type.load(), nullptr);
        node.setProperty ("ch", row.channel.load(), nullptr);
        node.setProperty ("num", row.number.load(), nullptr);
        node.setProperty ("param", id, nullptr);
        node.setProperty ("lo", row.lo.load(), nullptr);
        node.setProperty ("hi", row.hi.load(), nullptr);
        node.setProperty ("mode", row.mode.load(), nullptr);
        tree.appendChild (node, nullptr);
    }
    return tree;
}

void SeggProcessor::inMapFromTree (const juce::ValueTree& tree)
{
    for (int i = 0; i < kNumMapRows; ++i)
    {
        auto& row = inMap[(size_t) i];
        row.clear();
        const auto node = tree.getChild (i);
        if (! node.isValid()) continue;
        auto* prm = state.getParameter (node["param"].toString());
        if (prm == nullptr) continue;
        row.param = prm->getParameterIndex();
        row.channel = juce::jlimit (0, 16, (int) node["ch"]);
        row.number = juce::jlimit (0, 127, (int) node["num"]);
        row.lo = juce::jlimit (0, 100, (int) node["lo"]);
        row.hi = juce::jlimit (0, 100, (int) node["hi"]);
        row.mode = juce::jlimit (0, kNumInModes - 1, (int) node["mode"]);
        row.type = juce::jlimit (0, kNumInTypes - 1, (int) node["type"]);
    }
}

juce::ValueTree SeggProcessor::outMapToTree() const
{
    juce::ValueTree tree ("OUTMAP");
    for (auto& row : engine.outMap)
    {
        juce::ValueTree node ("ROW");
        node.setProperty ("track", row.track.load(), nullptr);
        node.setProperty ("source", row.source.load(), nullptr);
        node.setProperty ("dest", row.dest.load(), nullptr);
        node.setProperty ("num", row.number.load(), nullptr);
        node.setProperty ("lo", row.lo.load(), nullptr);
        node.setProperty ("hi", row.hi.load(), nullptr);
        node.setProperty ("slew", row.slewMs.load(), nullptr);
        tree.appendChild (node, nullptr);
    }
    return tree;
}

void SeggProcessor::outMapFromTree (const juce::ValueTree& tree)
{
    for (int i = 0; i < kNumMapRows; ++i)
    {
        auto& row = engine.outMap[(size_t) i];
        row.clear();
        const auto node = tree.getChild (i);
        if (! node.isValid()) continue;
        row.track = juce::jlimit (0, kNumTracks - 1, (int) node["track"]);
        row.source = juce::jlimit (0, kNumOutSources - 1, (int) node["source"]);
        row.number = juce::jlimit (0, 127, (int) node["num"]);
        row.lo = juce::jlimit (0, 100, (int) node["lo"]);
        row.hi = juce::jlimit (0, 100, (int) node["hi"]);
        row.slewMs = juce::jlimit (0, 5000, (int) node["slew"]);
        row.dest = juce::jlimit (0, kNumOutDests - 1, (int) node["dest"]);
    }
}

std::unique_ptr<juce::XmlElement> SeggProcessor::createPresetXml()
{
    auto copy = state.copyState();
    copy.appendChild (patternsToTree(), nullptr);
    copy.appendChild (outMapToTree(), nullptr);      // the output map is part of the music
    return copy.createXml();
}

bool SeggProcessor::applyPresetXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (state.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (xml);
    const auto patterns = tree.getChildWithName ("PATTERNS"), outMap = tree.getChildWithName ("OUTMAP");
    for (auto* name : { "PATTERNS", "OUTMAP", "PORTS", "INMAP" }) tree.removeChild (tree.getChildWithName (name), nullptr);
    state.replaceState (tree);
    patternsFromTree (patterns);
    outMapFromTree (outMap);
    return true;
}

void SeggProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = createPresetXml())
    {
        if (! portsPending.load())      // before the ports are settled there is nothing to save: the state stays "new"
            xml->addChildElement (portsToTree().createXml().release());
        xml->addChildElement (inMapToTree().createXml().release());    // controllers, like ports, stay out of presets
        xml->setAttribute ("presetIndex", presets.getCurrent());
        xml->setAttribute ("uiScale", (double) uiScale.load());
        copyXmlToBinary (*xml, dest);
    }
}

void SeggProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (applyPresetXml (*xml))
        {
            if (auto* ports = xml->getChildByName ("PORTS"))
            {
                portsPending.store (false);
                portsFromTree (juce::ValueTree::fromXml (*ports));
            }
            if (auto* map = xml->getChildByName ("INMAP")) inMapFromTree (juce::ValueTree::fromXml (*map));
            presets.markCurrent (xml->getIntAttribute ("presetIndex", 0));
            uiScale = (float) juce::jlimit (0.6, 2.0, xml->getDoubleAttribute ("uiScale", 1.0));
        }
    }
}

juce::AudioProcessorEditor* SeggProcessor::createEditor()
{
    restoreLastPorts();     // ROUTING must show the real ports, even in a host that opens the window before it plays
    return new SeggEditor (*this);
}

#ifndef SEGG_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SeggProcessor(); }
#endif

// ---------------- presets

namespace segg
{
static void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float value)
{
    if (auto* r = s.getParameter (id))
    {
        const float norm = r->convertTo0to1 (value);
        if (std::abs (r->getValue() - norm) > 1.0e-6f) r->setValueNotifyingHost (norm);
    }
    else jassertfalse;
}

TrackEdit& TrackEdit::operator() (const char* id, float value)
{
    setParam (s, "t" + juce::String (track + 1).paddedLeft ('0', 2) + "_" + id, value);
    return *this;
}

TrackEdit& TrackEdit::steps (std::initializer_list<int> pitches)
{
    size_t i = 0;
    for (int v : pitches) { if (i < kNumCells) pat.step[i++].set (v == R ? 0 : 1, v == R ? 0 : v); }
    return *this;
}

TrackEdit& TrackEdit::pads (std::initializer_list<int> pitches)
{
    size_t i = 0;
    for (int v : pitches) { if (i < kNumCells) pat.cart[i++].set (v == R ? 0 : 1, v == R ? 0 : v); }
    return *this;
}

void PresetEdit::global (const char* id, float value) { setParam (s, id, value); }

void PresetEdit::out (int row, int oneBasedTrack, int source, int dest, int number, int lo, int hi, int slewMs)
{
    auto& r = outMap[(size_t) row];
    r.track = oneBasedTrack - 1; r.source = source; r.number = number; r.lo = lo; r.hi = hi; r.slewMs = slewMs; r.dest = dest;
}

static constexpr int R = TrackEdit::R;

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> p = {
        { "Init", [] (PresetEdit&) {} },

        { "Amber Drift", [] (PresetEdit& e) {
            e.global ("groot", 2); e.global ("gscale", 3);
            e.track (1) ("on", 1) ("type", Turing) ("rate", 3) ("tlen", 11) ("tknob", 90) ("trange", 12) ("tdens", 72) ("gate", 380)
                        ("vel", 64) ("chord", 3) ("note", 48) ("drift", 30) ("mutate", 6);
            e.track (2) ("on", 1) ("type", Turing) ("rate", 4) ("tlen", 7) ("tknob", 94) ("trange", 19) ("tdens", 55) ("gate", 160)
                        ("vel", 70) ("note", 60) ("drift", 20);
            e.track (3) ("on", 1) ("type", Physics) ("rate", 8) ("pcount", 3) ("pspeed", 7) ("pmode", HitWalls) ("prange", 24) ("note", 60)
                        ("gate", 500) ("vel", 58) ("vrand", 18);
            e.track (4) ("on", 1) ("type", Euclid) ("rate", 5) ("esteps", 13) ("epulses", 3) ("note", 36) ("gate", 250) ("vel", 82);
            e.track (5) ("on", 1) ("type", Euclid) ("rate", 9) ("esteps", 17) ("epulses", 5) ("note", 72) ("vel", 42) ("vrand", 30)
                        ("prob", 70) ("mutate", 22) ("emel", 5) ("gate", 120)
                        .steps ({ 0, 3, 7, 10, 14 });
        } },

        { "Incunabula Grid", [] (PresetEdit& e) {
            e.global ("groot", 9); e.global ("gscale", 2);
            e.track (1) ("on", 1) ("type", Step) ("scale", 1) ("note", 36) ("gate", 40) ("vel", 112)
                        .steps ({ 0, R, R, R, R, R, 0, R, R, R, 0, R, R, R, R, R });
            e.track (2) ("on", 1) ("scale", 1) ("note", 38) ("esteps", 16) ("epulses", 2) ("erot", 4) ("gate", 40) ("vel", 104) ("roll", 12) ("rolln", 3) ("mutate", 8);
            e.track (3) ("on", 1) ("scale", 1) ("note", 42) ("esteps", 15) ("epulses", 11) ("eacc", 4) ("gate", 30) ("vel", 84) ("vrand", 28) ("swing", 18) ("drift", 12) ("roll", 6) ("rolln", 2);
            e.track (4) ("on", 1) ("scale", 1) ("note", 45) ("rate", 12) ("esteps", 19) ("epulses", 7) ("gate", 40) ("vel", 76) ("prob", 80) ("mutate", 24);
            e.track (5) ("on", 1) ("type", Step) ("note", 33) ("gate", 58) ("vel", 96) ("mutate", 10)
                        .steps ({ 0, R, 0, 12, R, 3, R, 0, 7, R, 0, R, 10, R, 0, 3 });
            e.track (6) ("on", 1) ("type", Cartesian) ("rate", 8) ("cxdiv", 3) ("cydiv", 5) ("note", 57) ("chord", 4) ("gate", 180) ("vel", 66) ("ctrig", 2)
                        .pads ({ 0, 3, R, 7, 5, R, 8, 3, R, 0, 10, 5, 7, R, 3, 12 });
            e.track (7) ("on", 1) ("type", Turing) ("rate", 8) ("tlen", 9) ("tknob", 82) ("trange", 14) ("tdens", 38) ("note", 69) ("gate", 90) ("vel", 74) ("swing", 10);
        } },

        { "Ambient Works", [] (PresetEdit& e) {
            e.global ("groot", 5); e.global ("gscale", 2);
            e.track (1) ("on", 1) ("scale", 1) ("rate", 5) ("esteps", 4) ("epulses", 4) ("note", 36) ("gate", 30) ("vel", 104);
            e.track (2) ("on", 1) ("scale", 1) ("rate", 8) ("esteps", 2) ("epulses", 1) ("erot", 1) ("note", 42) ("gate", 40) ("vel", 74) ("vrand", 14) ("swing", 12);
            e.track (3) ("on", 1) ("scale", 1) ("rate", 5) ("esteps", 4) ("epulses", 2) ("erot", 1) ("note", 38) ("gate", 30) ("vel", 92);
            e.track (4) ("on", 1) ("type", Step) ("note", 53) ("gate", 42) ("vel", 82) ("swing", 12) ("vrand", 10) ("mutate", 5)
                        .steps ({ 0, 7, 12, 7, 15, 12, 7, 3, 0, 7, 12, 7, 19, 15, 12, 7 });
            e.track (5) ("on", 1) ("type", Step) ("rate", 2) ("slen", 4) ("note", 41) ("chord", 4) ("gate", 390) ("vel", 62)
                        .steps ({ 0, -4, 3, -2 });
            e.track (6) ("on", 1) ("type", Turing) ("rate", 7) ("tlen", 8) ("tknob", 96) ("trange", 12) ("tdens", 42) ("note", 65) ("gate", 220) ("vel", 66);
        } },

        { "Cross Talk", [] (PresetEdit& e) {
            e.global ("groot", 7); e.global ("gscale", 4);
            e.track (1) ("on", 1) ("esteps", 16) ("epulses", 5) ("note", 36) ("gate", 40) ("vel", 104);
            e.track (2) ("on", 1) ("type", Step) ("slen", 7) ("lsrc", 1) ("lmode", LinkClock) ("note", 48) ("gate", 120) ("vel", 88)
                        .steps ({ 0, 3, 7, 1, 10, 7, 13 });
            e.track (3) ("on", 1) ("type", Turing) ("rate", 12) ("tlen", 13) ("tknob", 88) ("trange", 12) ("tdens", 50) ("lsrc", 1) ("lmode", LinkReset) ("note", 60) ("gate", 70) ("vel", 74);
            e.track (4) ("on", 1) ("type", Cartesian) ("rate", 8) ("cxdiv", 1) ("cydiv", 3) ("lsrc", 3) ("lmode", LinkTranspose) ("note", 48) ("gate", 140) ("vel", 70)
                        .pads ({ 0, 7, 3, 10, 12, R, 7, 5, 3, 0, R, 7, 10, 5, 12, 3 });
            e.track (5) ("on", 1) ("rate", 13) ("esteps", 11) ("epulses", 8) ("lsrc", 2) ("lmode", LinkGate) ("scale", 1) ("note", 42) ("gate", 25) ("vel", 70) ("vrand", 24);
            e.track (6) ("on", 1) ("rate", 12) ("esteps", 13) ("epulses", 9) ("lsrc", 1) ("lmode", LinkChoke) ("scale", 1) ("note", 39) ("gate", 30) ("vel", 66);
        } },

        { "Collider Field", [] (PresetEdit& e) {
            e.global ("groot", 0); e.global ("gscale", 15);
            e.track (1) ("on", 1) ("type", Physics) ("pcount", 2) ("pspeed", 11) ("pmode", HitWalls) ("prange", 12) ("note", 36) ("gate", 300) ("vel", 86);
            e.track (2) ("on", 1) ("type", Physics) ("pcount", 5) ("pspeed", 22) ("pmode", HitBalls) ("psize", 55) ("prange", 24) ("note", 60) ("gate", 400) ("vel", 74);
            e.track (3) ("on", 1) ("type", Physics) ("pcount", 3) ("pspeed", 34) ("pgrav", 22) ("pelast", 88) ("pkick", 35) ("prange", 36) ("note", 60) ("gate", 200) ("vel", 76);
            e.track (4) ("on", 1) ("type", Physics) ("rate", 12) ("pquant", 1) ("pcount", 4) ("pspeed", 62) ("prange", 24) ("note", 72) ("gate", 60) ("vel", 62) ("vrand", 22);
        } },

        { "Rene Lattice", [] (PresetEdit& e) {
            e.global ("groot", 4); e.global ("gscale", 4);
            e.track (1) ("on", 1) ("type", Cartesian) ("cxdiv", 1) ("cydiv", 5) ("note", 40) ("gate", 70) ("vel", 92)
                        .pads ({ 0, 3, 7, 12, 1, R, 5, 10, 7, 8, R, 3, 12, 10, 7, 0 });
            e.track (2) ("on", 1) ("type", Cartesian) ("rate", 8) ("cmode", 1) ("cpath", 1) ("cxdiv", 1) ("cydiv", 16) ("note", 52) ("gate", 110) ("vel", 80)
                        .pads ({ 0, 7, 12, 15, 3, 10, 14, 7, 5, 12, 19, 10, 8, 3, 0, 7 });
            e.track (3) ("on", 1) ("type", Cartesian) ("rate", 12) ("cxdiv", 3) ("cydiv", 4) ("cw", 3) ("cacc", 1) ("note", 64) ("gate", 80) ("vel", 74) ("ctrig", 2)
                        .pads ({ 0, R, 7, 3, R, 12, 3, 5, 7, R, 15, 0, 10, 2, R, 12 });
            e.track (4) ("on", 1) ("scale", 1) ("esteps", 16) ("epulses", 4) ("note", 36) ("gate", 40) ("vel", 100);
        } },

        { "Turing Choir", [] (PresetEdit& e) {
            e.global ("groot", 11); e.global ("gscale", 5);
            e.track (1) ("on", 1) ("type", Turing) ("rate", 2) ("tlen", 5) ("tknob", 92) ("trange", 12) ("tdens", 80) ("note", 47) ("chord", 3) ("gate", 300) ("vel", 62);
            e.track (2) ("on", 1) ("type", Turing) ("rate", 3) ("tlen", 7) ("tknob", 8) ("trange", 12) ("tdens", 66) ("note", 59) ("gate", 240) ("vel", 66);
            e.track (3) ("on", 1) ("type", Turing) ("rate", 5) ("tlen", 11) ("tknob", 90) ("trange", 12) ("tdens", 50) ("note", 66) ("gate", 200) ("vel", 62);
            e.track (4) ("on", 1) ("type", Turing) ("rate", 8) ("tlen", 13) ("tknob", 86) ("trange", 24) ("tdens", 34) ("note", 71) ("gate", 140) ("vel", 56) ("drift", 30);
        } },

        { "Hardware 8", [] (PresetEdit& e) {
            e.global ("gscale", 2);
            for (int t = 1; t <= 8; ++t)
            {
                auto tr = e.track (t);
                tr ("on", 1) ("port", (float) t) ("ch", 1) ("type", (float) ((t - 1) % kNumTypes)) ("note", 36.0f + 6.0f * (float) (t - 1));
            }
        } },

        { "Bus 16", [] (PresetEdit& e) {
            e.global ("gscale", 2);
            for (int t = 1; t <= 16; ++t)
            {
                auto tr = e.track (t);
                tr ("on", 1) ("port", 1) ("ch", (float) t) ("type", (float) ((t - 1) % kNumTypes)) ("note", 36.0f + 3.0f * (float) (t - 1)) ("prob", 80);
            }
        } },

        { "Hardware 16", [] (PresetEdit& e) {
            e.global ("gscale", 2);
            for (int t = 1; t <= 16; ++t)
            {
                auto tr = e.track (t);
                tr ("on", 1) ("port", (float) t) ("ch", 1) ("type", (float) ((t - 1) % kNumTypes)) ("note", 36.0f + 3.0f * (float) (t - 1)) ("prob", 80);
            }
        } },
        { "CC Lanes", [] (PresetEdit& e) {
            e.global ("groot", 9); e.global ("gscale", 2);
            e.track (1) ("on", 1) ("type", Turing) ("rate", 8) ("tlen", 8) ("tknob", 92) ("trange", 14) ("tdens", 70) ("note", 45) ("gate", 80);
            e.track (2) ("on", 1) ("type", Turing) ("rate", 11) ("tlen", 11) ("tknob", 88) ("notes", 0) ("ch", 1);
            e.out (0, 2, OutPosX, DestCC, 74, 20, 90, 60);
            e.track (3) ("on", 1) ("type", Physics) ("pcount", 1) ("pspeed", 12) ("notes", 0) ("ch", 1);
            e.out (1, 3, OutPosY, DestCC, 71, 10, 70, 0);
            e.track (4) ("on", 1) ("type", Euclid) ("rate", 5) ("esteps", 7) ("epulses", 3) ("notes", 0) ("ch", 1);
            e.out (2, 4, OutRandom, DestPitch, 0, 42, 58, 180);
        } },
    };
    return p;
}

juce::File PresetManager::userDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Dr Segg-uencer Presets");
    d.createDirectory();
    return d;
}

void PresetManager::refresh()
{
    names.clear();
    for (auto& f : factoryPresets()) names.add (juce::String ("Factory / ") + f.name);
    userFiles = userDir().findChildFiles (juce::File::findFiles, false, "*.segg");
    userFiles.sort();
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
}

void PresetManager::clearToDefaults()
{
    for (auto* p : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (std::abs (r->getValue() - r->getDefaultValue()) > 1.0e-6f) r->setValueNotifyingHost (r->getDefaultValue());
    resetPatterns();
}

/** A starting point that plays as soon as a track is switched to STEP or CARTESIAN. */
void PresetManager::resetPatterns()
{
    for (auto& pat : proc.engine.patterns)
    {
        for (int i = 0; i < kNumCells; ++i)
        {
            pat.step[(size_t) i].set (i % 4 == 0 ? 1 : 0, 0);
            pat.cart[(size_t) i].set (1, (i / 4) * 5 + (i % 4) * 2);
        }
    }
    for (auto& row : proc.engine.outMap) row.clear();
}

void PresetManager::load (int index)
{
    if (index < 0 || index >= names.size()) return;
    current = index;
    if (index < numFactory())
    {
        clearToDefaults();
        PresetEdit e { proc.state, proc.engine.patterns, proc.engine.outMap };
        factoryPresets()[(size_t) index].build (e);
    }
    else if (auto xml = juce::XmlDocument::parse (userFiles[index - numFactory()]))
        proc.applyPresetXml (*xml);
}

bool PresetManager::save (const juce::String& name)
{
    auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto f = userDir().getChildFile (clean + ".segg");
    auto xml = proc.createPresetXml();
    const bool ok = xml != nullptr && xml->writeTo (f);
    refresh();
    current = names.indexOf (clean);
    return ok;
}

bool PresetManager::removeCurrent()
{
    if (current < numFactory()) return false;
    const bool ok = userFiles[current - numFactory()].deleteFile();
    refresh();
    current = 0;
    return ok;
}
} // namespace segg
