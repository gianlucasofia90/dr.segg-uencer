#pragma once
// The last port configuration made in ROUTING, kept on this machine: a new instance starts from it, in any project.
// A saved session never reads it and never writes it: a session comes back exactly as it was saved.
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include "SeggCore.h"

namespace segg
{
struct PortChoice { juce::String id, name; };
using PortChoices = std::array<PortChoice, kNumSlots>;
using PortIds = std::array<juce::String, kNumSlots>;

class PortMemory
{
public:
    /** Tests only: keeps them away from the real file. */
    static juce::File& fileOverride() { static juce::File f; return f; }

    static juce::File file() { return fileOverride() != juce::File() ? fileOverride() : options().getDefaultFile(); }

    /** False if nothing has been configured on this machine yet. */
    static bool load (PortChoices& out)
    {
        const auto path = file();
        if (! path.existsAsFile()) return false;
        juce::InterProcessLock lock ("DrSeggUencerPorts");
        auto o = options();
        o.processLock = &lock;
        juce::PropertiesFile props (path, o);
        if (! props.containsKey (key ("slot", 0))) return false;
        for (int i = 0; i < kNumSlots; ++i)
            out[(size_t) i] = { props.getValue (key ("slot", i)), props.getValue (key ("name", i)) };
        return true;
    }

    static bool save (const PortIds& ids, const juce::Array<juce::MidiDeviceInfo>& devices)
    {
        PortChoices before;   // a device that is unplugged right now keeps the name it had
        load (before);
        juce::InterProcessLock lock ("DrSeggUencerPorts");
        auto o = options();
        o.processLock = &lock;
        juce::PropertiesFile props (file(), o);
        for (int i = 0; i < kNumSlots; ++i)
        {
            const auto& id = ids[(size_t) i];
            juce::String name = before[(size_t) i].id == id ? before[(size_t) i].name : juce::String();
            for (auto& d : devices) if (d.identifier == id) name = d.name;
            props.setValue (key ("slot", i), id);
            props.setValue (key ("name", i), name);
        }
        return props.save();
    }

    /** Port by port: the same device if it is there, otherwise one with the same name, because an id can change
        when an interface moves to another USB socket. A device that is gone keeps its id and shows as unavailable. */
    static PortIds match (const PortChoices& saved, const juce::Array<juce::MidiDeviceInfo>& devices)
    {
        PortIds out;
        juce::StringArray taken;
        for (int i = 0; i < kNumSlots; ++i)
        {
            out[(size_t) i] = saved[(size_t) i].id;
            for (auto& d : devices) if (d.identifier == out[(size_t) i]) taken.addIfNotAlreadyThere (d.identifier);
        }
        for (int i = 0; i < kNumSlots; ++i)
        {
            const auto& s = saved[(size_t) i];
            if (s.name.isEmpty() || taken.contains (s.id)) continue;
            for (auto& d : devices)
                if (d.name == s.name && ! taken.contains (d.identifier))
                {
                    out[(size_t) i] = d.identifier;
                    taken.add (d.identifier);
                    break;
                }
        }
        return out;
    }

private:
    static juce::String key (const char* prefix, int slot) { return prefix + juce::String (slot); }

    static juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Last Ports";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        o.folderName = ".config/Dr Segg-uencer";
       #else
        o.folderName = "Dr Segg-uencer";
       #endif
        o.millisecondsBeforeSaving = -1;    // written only by save(), never by a timer that could outlive the plugin
        return o;
    }
};
} // namespace segg
