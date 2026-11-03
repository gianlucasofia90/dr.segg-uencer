#pragma once
// Factory presets: one sketch for each way of working the plugin is meant for, plus two routing templates.
// The first three take their cue from the production habits of Amber, Incunabula and Selected Ambient Works 85-92:
// long loops of different lengths, mutation in small amounts, gates much longer or much shorter than the step.
#include <juce_audio_processors/juce_audio_processors.h>
#include "SeggCore.h"

class SeggProcessor;

namespace segg
{
/** Handle used by the preset recipes: t ("rate", 11) ("note", 36).steps ({ 0, R, 3, ... }). */
struct TrackEdit
{
    static constexpr int R = -128;   // rest
    juce::AudioProcessorValueTreeState& s;
    Pattern& pat;
    int track;

    TrackEdit& operator() (const char* id, float value);
    TrackEdit& steps (std::initializer_list<int> pitches);
    TrackEdit& pads (std::initializer_list<int> pitches);
};

struct PresetEdit
{
    juce::AudioProcessorValueTreeState& s;
    std::array<Pattern, kNumTracks>& patterns;
    void global (const char* id, float value);
    TrackEdit track (int oneBased) { return { s, patterns[(size_t) oneBased - 1], oneBased - 1 }; }
};

struct FactoryPreset { const char* name; void (*build) (PresetEdit&); };
const std::vector<FactoryPreset>& factoryPresets();

/** Factory + user presets (XML files in ~/Music/Dr Segg-uencer Presets). */
class PresetManager
{
public:
    explicit PresetManager (SeggProcessor& p) : proc (p) { refresh(); }

    static juce::File userDir();
    void refresh();
    const juce::StringArray& getNames() const { return names; }
    int getCurrent() const { return current; }
    /** After a session is restored: remember which preset it came from, without loading it over the session. */
    void markCurrent (int index) { current = juce::jlimit (0, juce::jmax (0, names.size() - 1), index); }
    int numFactory() const { return (int) factoryPresets().size(); }
    void load (int index);
    bool save (const juce::String& name);
    bool removeCurrent();
    void next() { load ((current + 1) % juce::jmax (1, names.size())); }
    void prev() { load ((current - 1 + names.size()) % juce::jmax (1, names.size())); }
    void resetPatterns();

private:
    void clearToDefaults();
    SeggProcessor& proc;
    juce::StringArray names;
    juce::Array<juce::File> userFiles;
    int current = 0;
};
} // namespace segg
