#pragma once
#include "SeggProcessor.h"
#include "SeggUI.h"
#include "Params.h"

/** The whole interface, laid out on a fixed 1280 x 800 grid. SeggEditor scales it to the window. */
class SeggView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 1280, kH = 800;

    explicit SeggView (SeggProcessor& p) : proc (p), list (p), view (p), routing (p), mapPage (p)
    {
        backdrop.draw = [this] (juce::Graphics& g) { paintBackdrop (g); };
        backdrop.setInterceptsMouseClicks (false, false);
        backdrop.setOpaque (true);
        backdrop.setBufferedToImage (true);
        addAndMakeVisible (backdrop);
        setLookAndFeel (&lnf);
        using K = segg::Control::Kind;

        struct G { const char* id; const char* label; K kind; };
        for (auto g : { G { "gsync", "SYNC", K::Menu }, G { "gbpm", "BPM", K::Box }, G { "grun", "RUN", K::Toggle }, G { "groot", "ROOT", K::Menu },
                        G { "gscale", "SCALE", K::Menu }, G { "gdensity", "DENSITY", K::Box }, G { "gchaos", "CHAOS", K::Box } })
        {
            auto* ctl = globals.add (new segg::Control (p.state, g.id, g.label, g.kind));
            const bool header = juce::String (g.id) == "gsync" || juce::String (g.id) == "gbpm" || juce::String (g.id) == "grun";
            if (header) { ctl->getProperties().set ("sgDark", true); ctl->setCaptionColour (segg::c::onPlate); }
            addAndMakeVisible (ctl);
            globalById.set (g.id, ctl);
        }

        addAndMakeVisible (list);
        list.onSelect = [this] (int t) { selectTrack (t); };
        addAndMakeVisible (view);
        addChildComponent (routing);
        addChildComponent (mapPage);

        for (int t = 0; t < segg::kNumTypes; ++t)
        {
            auto* tab = tabs.add (new segg::TypeTab (t));
            tab->onClick = [this, t] { proc.setTrackParam (track, segg::pType, (float) t); };
            addAndMakeVisible (tab);
        }

        for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &delBtn, &panicBtn }) addAndMakeVisible (b);
        addAndMakeVisible (routingBtn);
        addAndMakeVisible (mapBtn);
        prevBtn.setButtonText ("<"); nextBtn.setButtonText (">");
        saveBtn.setButtonText ("SAVE"); delBtn.setButtonText ("DEL");
        panicBtn.setButtonText ("PANIC");
        routingBtn.setButtonText ("ROUTING");
        routingBtn.setClickingTogglesState (true);
        routingBtn.onClick = [this] { showRouting (routingBtn.getToggleState()); };
        mapBtn.setButtonText ("MIDI MAP");
        mapBtn.setClickingTogglesState (true);
        mapBtn.onClick = [this] { showMidiMap (mapBtn.getToggleState()); };
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
        for (auto* ctl : { routeReset.get(), routeKey.get() }) ctl->button.getProperties().set ("sgDeck", true);

        sendLookAndFeelChange();
        setSize (kW, kH);
        selectTrack (0);
        addMouseListener (this, true);       // right click on any control: MIDI Learn
        startTimerHz (30);
    }
    ~SeggView() override { removeMouseListener (this); setLookAndFeel (nullptr); }

    void selectTrack (int t)
    {
        track = t;
        list.setSelected (t);
        view.setTrack (t);
        buildTrackControls();
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() || e.eventComponent == nullptr) return;
        auto* ctl = dynamic_cast<segg::Control*> (e.eventComponent);
        if (ctl == nullptr) ctl = e.eventComponent->findParentComponentOfClass<segg::Control>();
        if (ctl == nullptr) return;
        const auto id = ctl->getParamId();
        auto* prm = proc.state.getParameter (id);
        if (prm == nullptr) return;
        const int row = proc.findMapRow (id);
        juce::PopupMenu menu;
        menu.addSectionHeader (prm->getName (64));
        menu.addItem (1, "MIDI Learn");
        menu.addItem (2, "Remove mapping", row >= 0);
        juce::Component::SafePointer<SeggView> self (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (ctl), [self, id, row] (int r) {
            if (self == nullptr) return;
            if (r == 1) self->proc.startLearn (id);
            if (r == 2) self->proc.clearMapRow (row);
        });
    }

    void showMidiMap (bool on, bool output = false)
    {
        mapBtn.setToggleState (on, juce::dontSendNotification);
        routingBtn.setToggleState (false, juce::dontSendNotification);
        mapPage.showOutput (output || mapPage.showingOutput());
        updateVisibility();
        repaint();
    }

    void setWindowsHelp (bool on) { routing.setWindowsHelp (on); }
    void showRoutingCv (bool on) { routing.showCv (on); }

    void showRouting (bool on)
    {
        routingBtn.setToggleState (on, juce::dontSendNotification);
        mapBtn.setToggleState (false, juce::dontSendNotification);
        routing.refresh();
        updateVisibility();
        repaint();
    }

    void setLaneForSnapshot (int l) { view.setLane (l); }
    void tickForSnapshot() { for (int i = 0; i < 40; ++i) { list.tick(); view.tick(); } updateVisibility(); }

    void paintBackdrop (juce::Graphics& g)
    {
        using namespace segg;
        const float W = (float) getWidth(), H = (float) getHeight();
        // ground: lab red, the diagonal slab with a plate edge, checkerboard below, yellow and chrome rings
        g.fillAll (c::ground);
        juce::Path slab;
        slab.addQuadrilateral (700, kHeaderH, W, kHeaderH, W, H, 1020, H);
        g.setColour (c::slab);
        g.fillPath (slab);
        g.setColour (c::plate);
        g.drawLine (700, kHeaderH, 1020, H, 6.0f);
        g.setColour (c::checker);
        for (int y = 432; y < getHeight(); y += 32)
            for (int x = ((y / 32) % 2) * 32; x < getWidth(); x += 64)
                g.fillRect (x, y, 32, 32);
        g.setColour (c::orbitA); g.drawEllipse (1010 - 410, 492 - 410, 820, 820, 10.0f);
        g.setColour (c::orbitB.withAlpha (0.85f)); g.drawEllipse (1010 - 340, 492 - 340, 680, 680, 10.0f);
        g.setColour (c::orbitA); g.fillEllipse (1010 - 9, 152 - 9, 18, 18);
        g.setColour (c::orbitB); g.fillEllipse (670 - 9, 492 - 9, 18, 18);

        // header strip with its hazard edge, wordmark, preset tag, tempo display
        g.setColour (c::groundAlt);
        g.fillRect (0.0f, 0.0f, W, (float) kHeaderH);
        hazard (g, { 0.0f, (float) kHeaderH - 6.0f, W, 6.0f });
        // logo: egg-robot mark + live wordmark (Rubik Black Italic), no offset shadow, as in SONIC GRAIN
        constexpr int markSize = 46;
        const juce::Rectangle<float> markBox ((float) kMargin, (kHeaderH - 6 - markSize) * 0.5f, (float) markSize, (float) markSize);
        if (logoMark != nullptr)
            logoMark->drawWithin (g, markBox, juce::RectanglePlacement::centred, 1.0f);
        g.setFont (black (30.0f, -0.01f));
        const int wmX = kMargin + markSize + 12;
        g.setColour (c::onPlate);
        g.drawText ("DR. SEGG-UENCER", juce::Rectangle<int> (wmX, 0, 400 - wmX, kHeaderH - 6), juce::Justification::centredLeft);
        taggedPlate (g, { 410, 17, 236, 34 }, c::tag);

        // transport console: one black box, every label inside it, flush with the right margin
        const bool freeRun = proc.state.getRawParameterValue ("gsync")->load() > 0.5f;   // RUN plate sits right of the box
        auto console = juce::Rectangle<float> (kConsoleX, 12.0f, (freeRun ? kRunX - 12.0f : W - kMargin) - kConsoleX, 46.0f);
        g.setColour (c::plate);
        g.fillRoundedRectangle (console, 4.0f);
        g.setColour (c::onPlate.withAlpha (0.12f));
        g.drawRoundedRectangle (console.reduced (0.5f), 4.0f, 1.0f);
        g.fillRect (kConsoleX + 96.0f, 20.0f, 1.0f, 30.0f);
        if (proc.state.getRawParameterValue ("gsync")->load() < 0.5f)
        {   // in DAW sync, the tempo coming from the host
            g.setColour (c::onPlate.withAlpha (0.8f));
            g.setFont (bold (11.0f, 0.06f));
            g.drawText ("TEMPO", (int) kConsoleX + 110, 16, 80, 14, juce::Justification::left);
            g.setFont (pixel (10.0f));
            g.drawText ("BPM  DAW", (int) kConsoleX + 110, 38, 80, 12, juce::Justification::left);
            g.setColour (c::accent);
            g.setFont (pixel (20.0f));
            g.drawText (juce::String (proc.uiBpm.load(), 1), (int) (W - kMargin) - 100, 23, 88, 24, juce::Justification::right);
        }

        // tracks
        // every block sits kMargin from the window edge and kMargin from its neighbours
        glassPanel (g, { 16, 88, 376, 696 }, "16 tracks", "Sequencers");
        g.setColour (c::ink.withAlpha (0.14f));
        g.fillRect (32, 688, 344, 1);
        g.setColour (c::inkSoft);
        g.setFont (medium (10.0f, 0.3f));
        g.drawText ("GLOBAL", 34, 694, 120, 12, juce::Justification::left);

        // deck with the track's common settings
        {
            const auto deck = juce::Rectangle<float> (408.0f, (float) kDeckY, W - 408.0f - kMargin, H - kDeckY - kMargin);
            juce::Path body;
            body.addRoundedRectangle (deck, 10.0f);
            juce::DropShadow (c::panelShadow, 28, { 0, 12 }).drawForPath (g, body);
            g.setColour (c::modGround);
            g.fillPath (body);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (body);
            hazard (g, deck.withHeight (8.0f));
        }
        g.setColour (c::onPlate.withAlpha (0.18f));
        g.fillRect (1030, kDeckY + 22, 1, getHeight() - kDeckY - kMargin - 38);

        if (routingBtn.getToggleState())
        {
            glassPanel (g, { 408, 88, 856, 488 }, "Outputs", "Routing");
            return;
        }
        if (mapBtn.getToggleState())
        {
            glassPanel (g, { 408, 88, 856, 488 }, "Note, CC, pitch wheel", "MIDI Map");
            g.setColour (c::onPlate.withAlpha (0.85f));
            g.setFont (medium (12.5f));
            g.drawFittedText (mapPage.showingOutput()
                                  ? "Each row turns what a track is doing into CC or pitch wheel, on the track's own port and channel. Switch off NOTES on a track to use it only as a modulator."
                                  : "Right click any control for MIDI Learn. Controllers must reach this track: in Live set its MIDI From to the controller and Monitor to In.",
                              1044, kDeckY + 24, 204, 150, juce::Justification::topLeft, 9, 1.0f);
            return;
        }
        const int type = proc.ti (track, pType);
        glassPanel (g, { 408, 88, 856, 488 }, "Track " + juce::String (track + 1), kTypeNames[juce::jlimit (0, kNumTypes - 1, type)]);

        static const char* hints[kNumTypes] = {
            ("Spreads PULSES as evenly as possible over STEPS. Each track has its own length, so patterns of different lengths drift against each other. "
             "MELODY above 1 walks the pitch lane on every pulse."),
            "Sixteen steps with pitch, velocity, gate, chance and ratchet. Click the top row to switch steps, drag the bars to set values.",
            "A shift register in a loop. LOCK at 100 repeats the loop, 50 is fully random, 0 locks it at double length. In between the loop slips, a few notes at a time.",
            "X and Y run on separate clocks over a 4x4 grid. In Snake mode X walks a stored path and Y selects the next path. Denied pads are skipped (Seek) or stay silent (Sleep).",
            "Notes are born when balls hit the walls or each other: the point of impact sets the pitch, its force sets the velocity. Time runs in beats, so the balls follow the tempo." };
        g.setColour (c::inkSoft);
        g.setFont (medium (12.5f));
        g.drawFittedText (hints[juce::jlimit (0, kNumTypes - 1, type)], 880, 478, 368, 84, juce::Justification::topLeft, 5, 1.0f);

        const int port = proc.ti (track, pPort);
        g.setFont (pixel (10.0f));
        g.setColour (segg::c::onPlate);
        juce::String status;
        if (port == 0) status = "HOST OUTPUT";
        else if (! proc.direct.isSlotLive (port - 1)) { status = "PORT " + juce::String (port) + " NOT ASSIGNED"; g.setColour (segg::c::accent); }
        else status = "PORT " + juce::String (port) + " READY";
        if (port > 0 && proc.direct.isSlotFailed (port - 1)) { status = "PORT " + juce::String (port) + " UNAVAILABLE"; g.setColour (segg::c::accent); }
        g.drawText (status, 1044, kDeckY + 154, 204, 12, juce::Justification::left);
        const int learning = proc.learnRow.load();
        if (learning >= 0)
        {
            g.setColour (segg::c::accent);
            g.drawText ("LEARN: MOVE A CONTROL", 1044, kDeckY + 168, 204, 12, juce::Justification::left);
        }
        else if (! proc.uiPlaying.load())
        {
            g.setColour (segg::c::onPlate.withAlpha (0.7f));
            g.drawText (proc.state.getRawParameterValue ("gsync")->load() > 0.5f ? "PRESS RUN TO START" : "START THE HOST TRANSPORT", 1044, kDeckY + 168, 204, 12, juce::Justification::left);
        }
    }

    void resized() override
    {
        backdrop.setBounds (getLocalBounds());
        auto at = [this] (const char* id, int x, int y, int w, int h) { if (auto* c = globalById[id]) c->setBounds (x, y, w, h); };
        prevBtn.setBounds (368, 20, 36, 30);
        presetBox.setBounds (436, 23, 180, 22);
        nextBtn.setBounds (654, 20, 36, 30);
        saveBtn.setBounds (692, 20, 56, 30);
        delBtn.setBounds (746, 20, 48, 30);
        routingBtn.setBounds (794, 20, 102, 30);
        mapBtn.setBounds (892, 20, 106, 30);
        at ("gsync", (int) kConsoleX + 10, 15, 78, 40);
        at ("gbpm", (int) kConsoleX + 108, 15, 62, 40);
        at ("grun", (int) kRunX, 20, 87, 32);
        at ("groot", 32, 712, 76, 40);
        at ("gscale", 116, 712, 100, 40);
        at ("gdensity", 224, 712, 72, 38);
        at ("gchaos", 304, 712, 72, 38);

        list.setBounds (32, 140, 344, 16 * segg::TrackList::kRowH);
        panicBtn.setBounds (290, 100, 89, 30);
        view.setBounds (424, 144, 440, 416);
        routing.setBounds (424, 98, 824, 462);
        mapPage.setBounds (424, 98, 824, 462);
        for (int t = 0; t < segg::kNumTypes; ++t) tabs[t]->setBounds (604 + t * 128, 96, 126, 42);

        routeOffset->setBounds (1044, kDeckY + 22, 204, 38);
        routeReset->setBounds (1044, kDeckY + 72, 207, 32);
        routeKey->setBounds (1044, kDeckY + 112, 207, 32);
        layoutTrackControls();
    }

    segg::Control* getTrackControl (segg::P p) { for (auto& e : trackControls) if (e.param == p) return e.control; return nullptr; }

private:
    static constexpr int kDeckY = 592, kHeaderH = 72, kMargin = 16;
    static constexpr float kConsoleX = 1000.0f, kRunX = 1180.0f;

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
        add (pRate, "Rate", K::Menu, 0);     add (pNote, "Note", K::Menu, 0);     add (pScale, "Scale", K::Menu, 0);
        add (pChord, "Chord", K::Menu, 0);   add (pRolln, "Roll Hits", K::Box, 0); add (pDelay, "Delay", K::Box, 0);
        add (pSeed, "Seed", K::Box, 0);
        add (pPort, "Output", K::Menu, 0);   add (pCh, "Channel", K::Box, 0);
        add (pLsrc, "Link From", K::Menu, 0); add (pLmode, "Link Mode", K::Menu, 0);
        add (pNotes, "NOTES", K::Toggle, 0);

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
        const int row1 = kDeckY + 20, row2 = kDeckY + 130, row3 = kDeckY + 22;
        const P knobs[8] = { pSwing, pDrift, pGate, pVel, pVrand, pProb, pRoll, pMutate };
        for (int i = 0; i < 8; ++i) place (knobs[i], 424 + i * 74, row1, 74, 100);
        place (pRate, 424, row2, 92, 40);    place (pNote, 524, row2, 70, 40);   place (pScale, 602, row2, 108, 40);
        place (pChord, 718, row2, 84, 40);   place (pRolln, 810, row2, 62, 38);  place (pDelay, 880, row2, 66, 38);
        place (pSeed, 954, row2, 62, 38);
        place (pPort, 1044, row3, 100, 40);  place (pCh, 1152, row3, 96, 38);
        place (pLsrc, 1044, row3 + 52, 100, 40); place (pLmode, 1152, row3 + 52, 96, 40);
        place (pNotes, 1044, row3 + 98, 100, 28);
        if (auto* c = getTrackControl (pNotes)) c->button.getProperties().set ("sgDeck", true);

        // sequencer controls beside the well: knobs on a 4-column grid, then menus and switches two by two
        auto isKnob = [this] (P p) {
            auto* prm = proc.state.getParameter (SeggProcessor::tid (track, p));
            return prm != nullptr && dynamic_cast<juce::AudioParameterChoice*> (prm) == nullptr;
        };
        for (int group = 1; group <= kNumTypes; ++group)
        {
            int knobs = 0, others = 0;
            for (auto& e : trackControls) if (e.group == group && isKnob (e.param)) ++knobs;
            const int knobRows = (knobs + 3) / 4;
            int k = 0;
            for (auto& e : trackControls)
            {
                if (e.group != group) continue;
                if (isKnob (e.param)) { e.control->setBounds (880 + (k % 4) * 92, 148 + (k / 4) * 100, 92, 96); ++k; }
                else
                {
                    const int x = 884 + (others % 2) * 184, y = 152 + knobRows * 100 + (others / 2) * 52;
                    e.control->setBounds (x, y, 172, e.param == pPquant ? 30 : 40);
                    if (e.param == pPquant) e.control->setBounds (x + 8, y + 12, 150, 30);
                    ++others;
                }
            }
        }
    }

    void updateVisibility()
    {
        using namespace segg;
        const bool routed = routingBtn.getToggleState(), mapped = mapBtn.getToggleState(), route = routed || mapped;
        const int type = proc.ti (track, pType);
        for (auto& e : trackControls)
        {
            const bool side = e.param == pPort || e.param == pCh || e.param == pLsrc || e.param == pLmode || e.param == pNotes;
            const bool shown = e.group == 0 ? ! (route && side) : ! route && e.group == type + 1;
            if (e.control->isVisible() != shown) e.control->setVisible (shown);
        }
        view.setVisible (! route);
        routing.setVisible (routed);
        mapPage.setVisible (mapped);
        for (auto* ctl : { routeOffset.get(), routeReset.get(), routeKey.get() }) ctl->setVisible (routed);
        routeOffset->getProperties().set ("sgDark", true);
        routeOffset->setCaptionColour (c::onPlate);
        for (int t = 0; t < kNumTypes; ++t)
        {
            tabs[t]->setVisible (! route);
            tabs[t]->setToggleState (t == type, juce::dontSendNotification);
        }
        const bool free = proc.state.getRawParameterValue ("gsync")->load() > 0.5f;
        if (free != freeShown) { freeShown = free; backdrop.repaint (0, 0, getWidth(), kHeaderH); }
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
            if (routing.isVisible()) routing.sync();
            if (mapPage.isVisible()) mapPage.sync();
            const int port = proc.ti (track, segg::pPort);
            const int statusNow = port * 64 + (port > 0 && proc.direct.isSlotLive (port - 1) ? 1 : 0) + (port > 0 && proc.direct.isSlotFailed (port - 1) ? 2 : 0)
                                + (proc.learnRow.load() >= 0 ? 4 : 0) + (proc.uiPlaying.load() ? 8 : 0);
            if (statusNow != statusShown) { statusShown = statusNow; backdrop.repaint (1040, kDeckY + 140, 240, 50); }
            if (const auto bpm = proc.uiBpm.load(); bpm != bpmShown) { bpmShown = bpm; backdrop.repaint ((int) kConsoleX, 8, 1280 - (int) kConsoleX, 54); }
            if (presetBox.getSelectedItemIndex() != proc.presets.getCurrent()) refreshPresets();
        }
    }

    void refreshPresets()
    {
        presetBox.clear (juce::dontSendNotification);
        presetBox.addItemList (proc.presets.getNames(), 1);
        presetBox.setSelectedItemIndex (proc.presets.getCurrent(), juce::dontSendNotification);
    }

    /** A child that draws through its owner: the static artwork, cached so animated controls never redraw it. */
    struct Layer : juce::Component
    {
        std::function<void (juce::Graphics&)> draw;
        void paint (juce::Graphics& g) override { draw (g); }
    };
    Layer backdrop;

    SeggProcessor& proc;
    std::unique_ptr<juce::Drawable> logoMark { juce::Drawable::createFromImageData (SeggFonts::drseggmark_svg, SeggFonts::drseggmark_svgSize) };
    segg::LookAndFeel lnf;
    segg::TrackList list;
    segg::EngineView view;
    segg::RoutingPage routing;
    segg::MidiMapPage mapPage;
    juce::OwnedArray<segg::TypeTab> tabs;
    juce::OwnedArray<segg::Control> globals, owned;
    juce::HashMap<juce::String, segg::Control*> globalById;
    std::vector<Entry> trackControls;
    std::unique_ptr<segg::Control> routeOffset, routeReset, routeKey;
    juce::TextButton prevBtn, nextBtn, saveBtn, delBtn, routingBtn, mapBtn, panicBtn;
    juce::ComboBox presetBox;
    int track = 0, slowTick = 0, lastType = -1, statusShown = -1;
    bool freeShown = false;
    float bpmShown = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeggView)
};

/** Resizable window with the proportions locked: the view is drawn as vectors at the window's own scale,
    so it stays sharp at any size. The size is saved with the session. */
class SeggEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int kW = SeggView::kW, kH = SeggView::kH;
    static constexpr double kMinScale = 0.6, kMaxScale = 2.0;

    explicit SeggEditor (SeggProcessor& p) : AudioProcessorEditor (p), proc (p), ui (p)
    {
        addAndMakeVisible (ui);
        ui.setBounds (0, 0, kW, kH);
        setResizable (true, true);
        setResizeLimits (juce::roundToInt (kW * kMinScale), juce::roundToInt (kH * kMinScale),
                         juce::roundToInt (kW * kMaxScale), juce::roundToInt (kH * kMaxScale));
        getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
        const double s = juce::jlimit (kMinScale, kMaxScale, (double) p.uiScale.load());
        setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
    }

    void resized() override
    {
        const float s = (float) getWidth() / (float) kW;
        ui.setTransform (juce::AffineTransform::scale (s));
        proc.uiScale = s;
    }

    void paint (juce::Graphics& g) override { g.fillAll (segg::c::ground); }

    // used by the headless tests
    void selectTrack (int t)          { ui.selectTrack (t); }
    void showRouting (bool on)        { ui.showRouting (on); }
    void showRoutingCv (bool on)      { ui.showRoutingCv (on); }
    void showMidiMap (bool on, bool output) { ui.showMidiMap (on, output); }
    void setWindowsHelp (bool on)     { ui.setWindowsHelp (on); }
    void setLaneForSnapshot (int l)   { ui.setLaneForSnapshot (l); }
    void tickForSnapshot()            { ui.tickForSnapshot(); }
    segg::Control* trackControl (segg::P p) { return ui.getTrackControl (p); }

private:
    SeggProcessor& proc;
    SeggView ui;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeggEditor)
};
