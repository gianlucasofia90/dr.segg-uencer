#pragma once
#include "SeggProcessor.h"
#include "SeggUI.h"
#include "Params.h"

class SeggEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeggEditor (SeggProcessor& p) : AudioProcessorEditor (p), proc (p), list (p), view (p), routing (p)
    {
        setLookAndFeel (&lnf);
        using K = segg::Control::Kind;

        struct G { const char* id; const char* label; K kind; };
        for (auto g : { G { "gsync", "SYNC", K::Menu }, G { "gbpm", "BPM", K::Box }, G { "grun", "RUN", K::Toggle }, G { "groot", "ROOT", K::Menu },
                        G { "gscale", "SCALE", K::Menu }, G { "gdensity", "DENSITY", K::Box }, G { "gchaos", "CHAOS", K::Box } })
        {
            auto* ctl = globals.add (new segg::Control (p.state, g.id, g.label, g.kind));
            ctl->getProperties().set ("sgDark", true);
            ctl->setCaptionColour (segg::c::onPlate);
            addAndMakeVisible (ctl);
            globalById.set (g.id, ctl);
        }

        addAndMakeVisible (list);
        list.onSelect = [this] (int t) { selectTrack (t); };
        addAndMakeVisible (view);
        addChildComponent (routing);

        for (int t = 0; t < segg::kNumTypes; ++t)
        {
            auto* tab = tabs.add (new segg::TypeTab (t));
            tab->onClick = [this, t] { proc.setTrackParam (track, segg::pType, (float) t); };
            addAndMakeVisible (tab);
        }

        for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &delBtn, &panicBtn }) addAndMakeVisible (b);
        addAndMakeVisible (routingBtn);
        prevBtn.setButtonText ("<"); nextBtn.setButtonText (">");
        saveBtn.setButtonText ("SAVE"); delBtn.setButtonText ("DEL");
        panicBtn.setButtonText ("PANIC");
        routingBtn.setButtonText ("ROUTING");
        routingBtn.setClickingTogglesState (true);
        routingBtn.onClick = [this] { showRouting (routingBtn.getToggleState()); };
        panicBtn.onClick = [this] { proc.panic(); };
        addAndMakeVisible (presetBox);
        presetBox.getProperties().set ("sgDark", true);
        refreshPresets();
        presetBox.onChange = [this] {
            const int i = presetBox.getSelectedItemIndex();
            if (i >= 0 && i != proc.presets.getCurrent()) proc.presets.load (i);
        };
        prevBtn.onClick = [this] { proc.presets.prev(); refreshPresets(); };
        nextBtn.onClick = [this] { proc.presets.next(); refreshPresets(); };
        delBtn.onClick = [this] { proc.presets.removeCurrent(); refreshPresets(); };
        saveBtn.onClick = [this] {
            auto* w = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
            w->addTextEditor ("name", proc.presets.getCurrent() >= proc.presets.numFactory() ? proc.presets.getNames()[proc.presets.getCurrent()] : "My preset");
            w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
                if (r == 1) { proc.presets.save (w->getTextEditorContents ("name")); refreshPresets(); }
            }), true);
        };

        routeOffset = std::make_unique<segg::Control> (p.state, "goffset", "Port Offset", K::HBar);
        routeReset = std::make_unique<segg::Control> (p.state, "greset", "RESET ON PLAY", K::Toggle);
        routeKey = std::make_unique<segg::Control> (p.state, "gkeyin", "MIDI TRANSPOSE", K::Toggle);
        for (auto* ctl : { routeOffset.get(), routeReset.get(), routeKey.get() }) addChildComponent (ctl);

        sendLookAndFeelChange();
        setSize (1280, 800);
        selectTrack (0);
        startTimerHz (30);
    }
    ~SeggEditor() override { setLookAndFeel (nullptr); }

    void selectTrack (int t)
    {
        track = t;
        list.setSelected (t);
        view.setTrack (t);
        buildTrackControls();
        repaint();
    }

    void showRouting (bool on)
    {
        routingBtn.setToggleState (on, juce::dontSendNotification);
        routing.refresh();
        updateVisibility();
        repaint();
    }

    void setLaneForSnapshot (int l) { view.setLane (l); }
    void tickForSnapshot() { for (int i = 0; i < 40; ++i) { list.tick(); view.tick(); } updateVisibility(); }

    void paint (juce::Graphics& g) override
    {
        using namespace segg;
        const float W = (float) getWidth();
        // ground, checkerboard, orbit rings
        g.fillAll (c::ground);
        g.setColour (c::checker);
        for (int y = 352; y < kDeckY; y += 32)
            for (int x = ((y / 32) % 2) * 32; x < getWidth(); x += 64)
                g.fillRect (x, y, 32, juce::jmin (32, kDeckY - y));
        g.setColour (c::orbitB); g.drawEllipse (930 - 470, 430 - 470, 940, 940, 10.0f);
        g.setColour (c::orbitA); g.drawEllipse (930 - 370, 430 - 370, 740, 740, 10.0f);
        g.fillEllipse (668 - 22, 168 - 22, 44, 44);

        // header strip, wordmark, plates behind the global controls
        g.setColour (c::groundAlt);
        g.fillRect (0.0f, 0.0f, W, 64.0f);
        g.setFont (black (24.0f, -0.01f));
        const juce::String name ("DR. SEGG-UENCER");
        g.setColour (c::go);      g.drawText (name, 19, 12, 240, 40, juce::Justification::centredLeft);
        g.setColour (c::signal);  g.drawText (name, 17, 10, 240, 40, juce::Justification::centredLeft);
        g.setColour (c::onPlate); g.drawText (name, 15, 8, 240, 40, juce::Justification::centredLeft);
        taggedPlate (g, { 288, 14, 186, 34 }, c::tag);
        auto plateFor = [&] (juce::Rectangle<int> b) {
            auto q = b.toFloat().expanded (10, 4);
            g.setColour (juce::Colour (0x59000000)); g.fillPath (platePath (q), juce::AffineTransform::translation (3, 3));
            g.setColour (c::plate); g.fillPath (platePath (q));
        };
        plateFor ({ 742, 12, 186, 38 });
        plateFor ({ 948, 12, 148, 38 });
        plateFor ({ 1112, 12, 150, 38 });
        if (proc.state.getRawParameterValue ("gsync")->load() < 0.5f)
        {   // in DAW sync, the tempo coming from the host
            g.setColour (c::onPlate.withAlpha (0.8f));
            g.setFont (bold (11.0f, 0.06f));
            g.drawText ("TEMPO", 820, 12, 90, 16, juce::Justification::left);
            g.setColour (c::onPlate);
            g.setFont (pixel (10.0f));
            g.drawText (juce::String (proc.uiBpm.load(), 1) + " BPM", 820, 31, 100, 16, juce::Justification::left);
        }

        // tracks
        glassPanel (g, { 16, 76, 380, 708 }, "16 tracks", "Sequencers");

        // deck with the track's common settings
        g.setColour (c::modGround);
        g.fillRect (408, kDeckY, getWidth() - 408, getHeight() - kDeckY);
        g.setColour (c::onPlate.withAlpha (0.18f));
        g.fillRect (1030, kDeckY + 12, 1, getHeight() - kDeckY - 24);

        if (routingBtn.getToggleState())
        {
            glassPanel (g, { 408, 76, 856, 504 }, "Outputs", "Routing");
            return;
        }
        const int type = proc.ti (track, pType);
        glassPanel (g, { 408, 76, 856, 504 }, "Track " + juce::String (track + 1), kTypeNames[juce::jlimit (0, kNumTypes - 1, type)]);

        static const char* hints[kNumTypes] = {
            ("Spreads PULSES as evenly as possible over STEPS. Each track has its own length, so patterns of different lengths drift against each other. "
             "MELODY above 1 walks the pitch lane on every pulse."),
            "Sixteen steps with pitch, velocity, gate, chance and ratchet. Click the top row to switch steps, drag the bars to set values.",
            "A shift register in a loop. LOCK at 100 repeats the loop, 50 is fully random, 0 locks it at double length. In between the loop slips, a few notes at a time.",
            "X and Y run on separate clocks over a 4x4 grid. In Snake mode X walks a stored path and Y selects the next path. Denied pads are skipped (Seek) or stay silent (Sleep).",
            "Notes are born when balls hit the walls or each other: the point of impact sets the pitch, its force sets the velocity. Time runs in beats, so the balls follow the tempo." };
        g.setColour (c::inkSoft);
        g.setFont (medium (12.5f));
        g.drawFittedText (hints[juce::jlimit (0, kNumTypes - 1, type)], 872, 472, 376, 96, juce::Justification::topLeft, 6, 1.0f);

        const int port = proc.ti (track, pPort);
        g.setFont (pixel (10.0f));
        g.setColour (segg::c::onPlate);
        juce::String status;
        if (port == 0) status = "HOST OUTPUT";
        else if (! proc.direct.isSlotLive (port - 1)) { status = "PORT " + juce::String (port) + " NOT ASSIGNED"; g.setColour (segg::c::accent); }
        else status = "PORT " + juce::String (port) + " READY";
        g.drawText (status, 1046, kDeckY + 148, 216, 14, juce::Justification::left);
        if (! proc.uiPlaying.load())
        {
            g.setColour (segg::c::onPlate.withAlpha (0.7f));
            g.drawText (proc.state.getRawParameterValue ("gsync")->load() > 0.5f ? "PRESS RUN TO START" : "START THE HOST TRANSPORT", 1046, kDeckY + 168, 216, 14, juce::Justification::left);
        }
    }

    void resized() override
    {
        auto at = [this] (const char* id, int x, int y, int w, int h) { if (auto* c = globalById[id]) c->setBounds (x, y, w, h); };
        prevBtn.setBounds (246, 16, 38, 30);
        presetBox.setBounds (310, 20, 144, 22);
        nextBtn.setBounds (476, 16, 38, 30);
        saveBtn.setBounds (518, 16, 60, 30);
        delBtn.setBounds (578, 16, 52, 30);
        routingBtn.setBounds (632, 16, 106, 30);
        at ("gsync", 750, 12, 62, 38);
        at ("gbpm", 816, 12, 54, 38);
        at ("grun", 872, 16, 58, 30);
        at ("groot", 956, 12, 44, 38);
        at ("gscale", 1004, 12, 92, 38);
        at ("gdensity", 1120, 12, 66, 38);
        at ("gchaos", 1192, 12, 66, 38);

        list.setBounds (28, 128, 356, 648);
        panicBtn.setBounds (300, 86, 84, 30);
        view.setBounds (424, 132, 432, 436);
        routing.setBounds (424, 84, 824, 488);
        for (int t = 0; t < segg::kNumTypes; ++t) tabs[t]->setBounds (566 + t * 136, 84, 142, 42);

        routeOffset->setBounds (1046, 610, 214, 38);
        routeReset->setBounds (1046, 660, 214, 32);
        routeKey->setBounds (1046, 700, 214, 32);
        layoutTrackControls();
    }

    segg::Control* getTrackControl (segg::P p) { for (auto& e : trackControls) if (e.param == p) return e.control; return nullptr; }

private:
    static constexpr int kDeckY = 592;

    struct Entry { segg::P param; segg::Control* control; int group; };   // group: 0 = common, 1 + type = that sequencer

    void buildTrackControls()
    {
        using namespace segg;
        using K = Control::Kind;
        trackControls.clear();
        owned.clear();
        auto add = [this] (P param, const char* label, K kind, int group) {
            auto* ctl = owned.add (new Control (proc.state, SeggProcessor::tid (track, param), label, kind));
            if (group == 0) { ctl->getProperties().set ("sgDark", true); ctl->setCaptionColour (c::onPlate); }
            addChildComponent (ctl);
            trackControls.push_back ({ param, ctl, group });
        };
        add (pSwing, "Swing", K::Knob, 0);   add (pDrift, "Drift", K::Knob, 0);   add (pGate, "Gate", K::Knob, 0);
        add (pVel, "Velocity", K::Knob, 0);  add (pVrand, "Vel Rnd", K::Knob, 0); add (pProb, "Chance", K::Knob, 0);
        add (pRoll, "Roll", K::Knob, 0);     add (pMutate, "Mutate", K::Knob, 0);
        add (pRate, "Rate", K::Menu, 0);     add (pNote, "Note", K::Box, 0);      add (pScale, "Scale", K::Menu, 0);
        add (pChord, "Chord", K::Menu, 0);   add (pRolln, "Roll Hits", K::Box, 0); add (pDelay, "Delay", K::Box, 0);
        add (pSeed, "Seed", K::Box, 0);
        add (pPort, "Output", K::Menu, 0);   add (pCh, "Channel", K::Box, 0);
        add (pLsrc, "Link From", K::Menu, 0); add (pLmode, "Link Mode", K::Menu, 0);

        add (pEsteps, "Steps", K::Knob, 1 + Euclid);    add (pEpulses, "Pulses", K::Knob, 1 + Euclid);
        add (pErot, "Rotate", K::Knob, 1 + Euclid);     add (pEacc, "Accents", K::Knob, 1 + Euclid);
        add (pEmel, "Melody", K::Knob, 1 + Euclid);
        add (pSlen, "Length", K::Knob, 1 + Step);       add (pSdir, "Direction", K::Menu, 1 + Step);
        add (pTlen, "Length", K::Knob, 1 + Turing);     add (pTknob, "Lock", K::Knob, 1 + Turing);
        add (pTrange, "Range", K::Knob, 1 + Turing);    add (pTdens, "Density", K::Knob, 1 + Turing);
        add (pCxdiv, "X Clock", K::Knob, 1 + Cartesian); add (pCydiv, "Y Clock", K::Knob, 1 + Cartesian);
        add (pCmode, "Mode", K::Menu, 1 + Cartesian);   add (pCw, "Width", K::Knob, 1 + Cartesian);
        add (pChgt, "Height", K::Knob, 1 + Cartesian);  add (pCpath, "Path", K::Menu, 1 + Cartesian);
        add (pCacc, "Denied", K::Menu, 1 + Cartesian);  add (pCtrig, "Trigger", K::Menu, 1 + Cartesian);
        add (pPcount, "Balls", K::Knob, 1 + Physics);   add (pPspeed, "Speed", K::Knob, 1 + Physics);
        add (pPgrav, "Gravity", K::Knob, 1 + Physics);  add (pPelast, "Bounce", K::Knob, 1 + Physics);
        add (pPsize, "Size", K::Knob, 1 + Physics);     add (pPkick, "Kick", K::Knob, 1 + Physics);
        add (pPrange, "Range", K::Knob, 1 + Physics);   add (pPmode, "Notes", K::Menu, 1 + Physics);
        add (pPquant, "QUANTIZE", K::Toggle, 1 + Physics);

        for (auto& e : trackControls) { e.control->sendLookAndFeelChange(); }
        layoutTrackControls();
        updateVisibility();
    }

    void layoutTrackControls()
    {
        using namespace segg;
        auto place = [this] (P p, int x, int y, int w, int h) { if (auto* c = getTrackControl (p)) c->setBounds (x, y, w, h); };
        const int row1 = kDeckY + 12, row2 = kDeckY + 128;
        const P knobs[8] = { pSwing, pDrift, pGate, pVel, pVrand, pProb, pRoll, pMutate };
        for (int i = 0; i < 8; ++i) place (knobs[i], 418 + i * 76, row1, 76, 100);
        place (pRate, 420, row2, 92, 40);    place (pNote, 520, row2, 70, 38);   place (pScale, 598, row2, 108, 40);
        place (pChord, 714, row2, 84, 40);   place (pRolln, 806, row2, 62, 38);  place (pDelay, 876, row2, 70, 38);
        place (pSeed, 954, row2, 64, 38);
        place (pPort, 1046, row1, 110, 40);  place (pCh, 1166, row1, 96, 38);
        place (pLsrc, 1046, row1 + 56, 110, 40); place (pLmode, 1166, row1 + 56, 96, 40);

        // sequencer controls: a 3 x 3 grid beside the well
        for (auto& e : trackControls)
        {
            if (e.group == 0) continue;
            int index = 0;
            for (auto& o : trackControls) { if (&o == &e) break; if (o.group == e.group) ++index; }
            const int x = 866 + (index % 3) * 130, y = 132 + (index / 3) * 108;
            const bool knob = proc.state.getParameter (SeggProcessor::tid (track, e.param)) != nullptr
                           && dynamic_cast<juce::AudioParameterChoice*> (proc.state.getParameter (SeggProcessor::tid (track, e.param))) == nullptr;
            if (knob) e.control->setBounds (x, y, 124, 104);
            else      e.control->setBounds (x + 2, y + 26, 118, e.param == pPquant ? 34 : 40);
        }
    }

    void updateVisibility()
    {
        using namespace segg;
        const bool route = routingBtn.getToggleState();
        const int type = proc.ti (track, pType);
        for (auto& e : trackControls)
        {
            const bool shown = e.group == 0 ? ! (route && (e.param == pPort || e.param == pCh || e.param == pLsrc || e.param == pLmode)) : ! route && e.group == type + 1;
            if (e.control->isVisible() != shown) e.control->setVisible (shown);
        }
        view.setVisible (! route);
        routing.setVisible (route);
        for (auto* ctl : { routeOffset.get(), routeReset.get(), routeKey.get() }) ctl->setVisible (route);
        routeOffset->getProperties().set ("sgDark", true);
        routeOffset->setCaptionColour (c::onPlate);
        for (int t = 0; t < kNumTypes; ++t)
        {
            tabs[t]->setVisible (! route);
            tabs[t]->setToggleState (t == type, juce::dontSendNotification);
        }
        const bool free = proc.state.getRawParameterValue ("gsync")->load() > 0.5f;
        if (free != freeShown) { freeShown = free; repaint (0, 0, getWidth(), 64); }
        if (auto* c = globalById["gbpm"]) c->setVisible (free);
        if (auto* c = globalById["grun"]) c->setVisible (free);
    }

    void timerCallback() override
    {
        list.tick();
        if (view.isVisible()) view.tick();
        if (++slowTick >= 5)
        {
            slowTick = 0;
            const int type = proc.ti (track, segg::pType);
            if (type != lastType) { lastType = type; repaint(); }
            updateVisibility();
            if (routing.isVisible()) routing.repaint();
            repaint (1040, kDeckY + 140, 240, 50);
            if (const auto bpm = proc.uiBpm.load(); bpm != bpmShown) { bpmShown = bpm; repaint (812, 8, 110, 46); }
            if (presetBox.getSelectedItemIndex() != proc.presets.getCurrent()) refreshPresets();
        }
    }

    void refreshPresets()
    {
        presetBox.clear (juce::dontSendNotification);
        presetBox.addItemList (proc.presets.getNames(), 1);
        presetBox.setSelectedItemIndex (proc.presets.getCurrent(), juce::dontSendNotification);
    }

    SeggProcessor& proc;
    segg::LookAndFeel lnf;
    segg::TrackList list;
    segg::EngineView view;
    segg::RoutingPage routing;
    juce::OwnedArray<segg::TypeTab> tabs;
    juce::OwnedArray<segg::Control> globals, owned;
    juce::HashMap<juce::String, segg::Control*> globalById;
    std::vector<Entry> trackControls;
    std::unique_ptr<segg::Control> routeOffset, routeReset, routeKey;
    juce::TextButton prevBtn, nextBtn, saveBtn, delBtn, routingBtn, panicBtn;
    juce::ComboBox presetBox;
    int track = 0, slowTick = 0, lastType = -1;
    bool freeShown = false;
    float bpmShown = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeggEditor)
};
