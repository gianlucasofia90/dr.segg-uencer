#pragma once
// Direct MIDI output: a track can bypass the host and play straight to a port of the MIDI interface, or to a virtual
// port the plugin creates. Needed in Ableton Live, which merges a plugin's MIDI output onto a single channel.
#include <juce_audio_devices/juce_audio_devices.h>
#include "SeggCore.h"

namespace segg
{
class DirectMidi : private juce::Thread
{
public:
    /** Value stored in a slot to ask for a virtual port instead of an existing device. */
    static juce::String virtualId() { return "virtual"; }
    static juce::String virtualName (int slot) { return "DR. SEGG-UENCER " + juce::String (slot + 1); }

    DirectMidi() : juce::Thread ("Dr Segg-uencer MIDI") { pendingMsgs.reserve (kFifoSize); }
    ~DirectMidi() override
    {
        stopThread (1000);
        // the plugin is going away: nothing may stay held on the hardware
        for (auto& out : outputs)
            if (out != nullptr)
                for (int ch = 1; ch <= 16; ++ch) out->sendMessageNow (juce::MidiMessage::allNotesOff (ch));
    }

    /** Message thread. Empty id closes the slot. Returns false if the port could not be opened. */
    bool setSlot (int slot, const juce::String& id)
    {
        std::unique_ptr<juce::MidiOutput> device;
       #if ! JUCE_WINDOWS   // Windows has no virtual MIDI ports of its own
        if (id == virtualId())
            device = juce::MidiOutput::createNewDevice (virtualName (slot));
        else
       #endif
        if (id.isNotEmpty() && id != virtualId())
            device = juce::MidiOutput::openDevice (id);
        const bool ok = id.isEmpty() || device != nullptr;
        {
            const juce::ScopedLock sl (outputLock);
            live[(size_t) slot].store (device != nullptr);
            outputs[(size_t) slot] = std::move (device);
        }
        ids[(size_t) slot] = id;
        if (live[(size_t) slot].load() && ! isThreadRunning()) startRealtimeOrHigh();
        return ok;
    }

    /** Tests only: delivers to this function instead of a device. */
    void attachTap (std::function<void (int, const juce::MidiMessage&)> fn)
    {
        tap = std::move (fn);
        for (auto& l : live) l.store (true);
        startRealtimeOrHigh();
    }

    juce::String getSlot (int slot) const { return ids[(size_t) slot]; }
    bool isSlotLive (int slot) const noexcept { return live[(size_t) slot].load (std::memory_order_relaxed); }

    /** Audio thread. dueMs is on the juce::Time::getMillisecondCounterHiRes clock. */
    void push (int slot, double dueMs, int status, int d1, int d2) noexcept
    {
        if (! live[(size_t) slot].load (std::memory_order_relaxed)) return;
        const auto w = fifo.write (1);
        if (w.blockSize1 + w.blockSize2 == 0) { ++dropped; return; }
        buffer[(size_t) (w.blockSize1 > 0 ? w.startIndex1 : w.startIndex2)]
            = { dueMs, (std::uint8_t) slot, (std::uint8_t) status, (std::uint8_t) d1, (std::uint8_t) d2 };
    }

    /** Current output estimate: one audio block of look-ahead hides the jitter of the host's block callback. */
    double blockStartMs (int numSamples, double sampleRate) noexcept
    {
        const double now = juce::Time::getMillisecondCounterHiRes(), blockMs = 1000.0 * numSamples / sampleRate;
        if (lineMs == 0.0 || std::abs (now - lineMs) > 4.0 * blockMs + 20.0) lineMs = now;
        else lineMs += 0.02 * (now - lineMs);
        const double start = lineMs;
        lineMs += blockMs;
        return start + blockMs;
    }

    int getDropped() const noexcept { return dropped.load(); }

private:
    struct Msg { double due; std::uint8_t slot, status, d1, d2; };
    static constexpr int kFifoSize = 16384;

    void startRealtimeOrHigh() { startThread (juce::Thread::Priority::highest); }

    void run() override
    {
        while (! threadShouldExit())
        {
            const auto r = fifo.read (fifo.getNumReady());
            auto take = [this] (int start, int n) { for (int i = 0; i < n; ++i) pendingMsgs.push_back (buffer[(size_t) (start + i)]); };
            take (r.startIndex1, r.blockSize1);
            take (r.startIndex2, r.blockSize2);

            const double now = juce::Time::getMillisecondCounterHiRes();
            std::stable_sort (pendingMsgs.begin(), pendingMsgs.end(), [] (const Msg& a, const Msg& b) { return a.due < b.due; });
            size_t sent = 0;
            {
                const juce::ScopedLock sl (outputLock);
                for (; sent < pendingMsgs.size() && pendingMsgs[sent].due <= now + 0.25; ++sent)
                {
                    const auto& m = pendingMsgs[sent];
                    const juce::MidiMessage msg ((int) m.status, (int) m.d1, (int) m.d2);
                    if (tap) tap (m.slot, msg);
                    else if (auto* out = outputs[m.slot].get()) out->sendMessageNow (msg);
                }
            }
            pendingMsgs.erase (pendingMsgs.begin(), pendingMsgs.begin() + (std::ptrdiff_t) sent);
            juce::Thread::sleep (1);
        }
    }

    juce::AbstractFifo fifo { kFifoSize };
    std::array<Msg, kFifoSize> buffer {};
    std::vector<Msg> pendingMsgs;
    std::array<std::unique_ptr<juce::MidiOutput>, kNumSlots> outputs;
    std::array<std::atomic<bool>, kNumSlots> live {};
    std::array<juce::String, kNumSlots> ids;
    juce::CriticalSection outputLock;
    std::function<void (int, const juce::MidiMessage&)> tap;
    std::atomic<int> dropped { 0 };
    double lineMs = 0.0;
};
} // namespace segg
