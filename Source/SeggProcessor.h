#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SeggEngine.h"
#include "DirectMidi.h"
#include "Presets.h"

class SeggProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SeggProcessor();
    ~SeggProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "DR. SEGG-UENCER"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    // the audio is always silent, and a host must never put the sequencer to sleep for that. Only VST3 can say "infinite".
    double getTailLengthSeconds() const override
    {
        return wrapperType == wrapperType_VST3 ? std::numeric_limits<double>::infinity() : 3600.0;
    }

    int getNumPrograms() override { return (int) segg::factoryPresets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, presets.getCurrent()); }
    void setCurrentProgram (int i) override { if (i < getNumPrograms()) presets.load (i); }
    const juce::String getProgramName (int i) override
    {
        return i >= 0 && i < getNumPrograms() ? juce::String (segg::factoryPresets()[(size_t) i].name) : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** Parameter id of a track parameter: tid (0, segg::pRate) == "t01_rate". */
    static juce::String tid (int track, segg::P p);

    /** Everything a preset holds, as XML (no port assignment: those belong to the machine, not to the music). */
    std::unique_ptr<juce::XmlElement> createPresetXml();
    bool applyPresetXml (const juce::XmlElement&);

    // the editor reads and writes track parameters through these, without a lookup by name
    float tf (int track, segg::P p) const noexcept { return trackPtr[(size_t) track][(size_t) p]->load (std::memory_order_relaxed); }
    int ti (int track, segg::P p) const noexcept { return (int) std::lround (tf (track, p)); }
    void setTrackParam (int track, segg::P p, float value)
    {
        if (auto* r = state.getParameter (tid (track, p)))
        {
            r->beginChangeGesture();
            r->setValueNotifyingHost (r->convertTo0to1 (value));
            r->endChangeGesture();
        }
    }

    /** Input map: the next note, CC or pitch wheel that arrives fills this row. -1 = not listening. */
    std::atomic<int> learnRow { -1 };
    /** Learn for a parameter: reuses its row or takes a free one. Returns the row, or -1 if the table is full. */
    int startLearn (const juce::String& paramId);
    int findMapRow (const juce::String& paramId) const;
    void clearMapRow (int row);
    /** Applies what the input map collected. Parameters are set here, on the message thread, never on the audio thread. */
    void pumpMidiMap();

    void panic() { panicRequested.store (true); }
    void setPortSlot (int slot, const juce::String& deviceId);

    juce::AudioProcessorValueTreeState state;
    segg::PresetManager presets;
    segg::Engine engine;
    std::array<segg::InMapRow, segg::kNumMapRows> inMap;
    segg::DirectMidi direct;

    std::atomic<bool> uiPlaying { false };
    std::atomic<float> uiBpm { 120.0f };
    std::atomic<float> uiScale { 1.0f };   // editor size, saved with the session

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct Sink;
    void pullParams() noexcept;
    void runBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&, bool bypassed);
    juce::ValueTree patternsToTree() const;
    void patternsFromTree (const juce::ValueTree&);
    juce::ValueTree portsToTree() const;
    void portsFromTree (const juce::ValueTree&);
    juce::ValueTree inMapToTree() const;
    void inMapFromTree (const juce::ValueTree&);
    juce::ValueTree outMapToTree() const;
    void outMapFromTree (const juce::ValueTree&);
    void timerCallback() override { pumpMidiMap(); }
    void handleLearn (int status, int d1, int d2) noexcept;

    std::array<std::array<std::atomic<float>*, segg::kNumP>, segg::kNumTracks> trackPtr {};
    std::atomic<float>* gSync {}, *gBpm {}, *gRun {}, *gRoot {}, *gScale {}, *gDensity {}, *gChaos {}, *gReset {}, *gKeyIn {}, *gOffset {};

    double sr = 48000.0, internalPpq = 0.0;
    bool internalRunning = false;
    int heldKey = -1;
    std::atomic<bool> panicRequested { false };
    struct MapValue { int param; float norm; };
    juce::AbstractFifo mapFifo { 1024 };
    std::array<MapValue, 1024> mapValues {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeggProcessor)
};
