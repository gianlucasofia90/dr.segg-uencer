#pragma once
// The CV table. No JUCE here: the table is read by the audio thread.
// CV leaves through extra audio outputs, in stereo pairs (CV 1-2 ... CV 15-16). The voltages are real only on a
// DC-coupled interface: fullScale tells the engine which voltage that interface reaches at digital 1.0.
#include "SeggCore.h"

namespace segg
{
static constexpr int kNumCvOuts = 16, kNumCvBuses = kNumCvOuts / 2;
static constexpr double kCvTriggerSeconds = 0.005;
static constexpr int kCvMaxMv = 10000;       // Min and Max of a row, in millivolts
static constexpr int kCvMaxSlewMs = 5000;
static constexpr float kCvMinFullScale = 1.0f, kCvMaxFullScale = 20.0f, kCvDefaultFullScale = 10.0f;
static constexpr int kCvDefaultZeroNote = 60;

// Append only: saved by index.
enum CvSignal { CvOff, CvPitch, CvGate, CvTrigger, CvVelocity, CvRandom, CvPosX, CvPosY, kNumCvSignals };
static inline const char* kCvSignalNames[kNumCvSignals] = { "Off", "Pitch", "Gate", "Trigger", "Velocity", "Random", "Position X", "Position Y" };

/** Track signal -> one CV output. Written by the UI, read by the audio thread. */
struct CvRow
{
    std::atomic<int> track { 0 }, signal { CvOff }, loMv { 0 }, hiMv { 5000 }, slewMs { 0 };

    void clear() noexcept { track = 0; signal = CvOff; loMv = 0; hiMv = 5000; slewMs = 0; }
};
} // namespace segg
