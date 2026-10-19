// Audio Connect - by MiKeys Dsz Studios
// RouterEngine: process-wide singleton that owns the two physical output "slots"
//   slot 0 = Audio Interface (multi-channel)
//   slot 1 = PC output / Headphones (system device)
// Every plugin instance ("Send") pushes its audio into per-device lock-free lanes;
// each device callback pulls from the lanes and mixes them to the chosen output pairs.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

namespace ac
{
constexpr int kNumDevices = 2;
constexpr int kMaxPairs   = 16;           // up to 32 output channels (16 stereo buses)
constexpr int kRing       = 1 << 15;
constexpr int kRingMask   = kRing - 1;

//==============================================================================
// Single-producer / single-consumer ring with drift-compensated, resampling reader
struct Lane
{
    Lane() : l ((size_t) kRing, 0.0f), r ((size_t) kRing, 0.0f) {}

    std::vector<float> l, r;
    std::atomic<int64_t> writeCount { 0 };

    // consumer (device thread) only
    double readPos = 0.0, ratio = 1.0;
    bool primed = false;
};

struct Send
{
    Send()
    {
        for (auto& m : mask)  m.store (0);
        for (auto& d : uiDev)  d.store (0);
        for (auto& p : uiPair) p.store (1);
    }

    void push (int dev, const float* L, const float* R, int n);

    juce::String getName() const            { const juce::ScopedLock sl (nameLock); return name; }
    void setName (const juce::String& n)    { const juce::ScopedLock sl (nameLock); name = n; }

    std::atomic<uint32_t> mask[kNumDevices];   // bit p => output pair p+1 on that device
    Lane lanes[kNumDevices];
    std::atomic<double> hostRate { 44100.0 };
    std::atomic<int> targetMs { 20 };

    // for the UI "Output Map"
    std::atomic<int> uiDev[2];                 // 0 = off, 1 = interface, 2 = PC
    std::atomic<int> uiPair[2];                // 1-based

private:
    mutable juce::CriticalSection nameLock;
    juce::String name { "Loops" };
};

struct SendInfo
{
    juce::String name;
    int dev[2] {}, pair[2] {};
    const Send* ptr = nullptr;
};

class RouterEngine;

//==============================================================================
class DeviceOutput : public juce::AudioIODeviceCallback
{
public:
    DeviceOutput (RouterEngine&, int index);
    ~DeviceOutput() override;

    juce::StringArray getTypeNames();
    juce::StringArray getDeviceNames (const juce::String& type);

    bool open (const juce::String& type, const juce::String& device, bool remember = true);
    void close();
    bool autoDetect();            // picks the first non-onboard device (the audio interface)
    bool openSystemDefault();     // the PC's default output (headphone jack / speakers)
    bool rescan();                // true if the device list of the current driver changed

    bool isOpen() const                      { return numOutputs.load() > 0; }
    juce::String getTypeName() const         { return dm.getCurrentAudioDeviceType(); }
    juce::String getDeviceName() const;
    int getNumOutputs() const                { return numOutputs.load(); }
    int getNumPairs() const                  { return (numOutputs.load() + 1) / 2; }
    juce::StringArray getChannelNames() const;
    double getSampleRate() const             { return rate.load(); }
    int getBufferSize() const;
    juce::String getLastError() const        { return lastError; }

    juce::String savedType, savedDevice;

    void audioDeviceIoCallbackWithContext (const float* const*, int, float* const*, int, int,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

private:
    juce::AudioIODeviceType* findType (const juce::String&);

    RouterEngine& engine;
    const int index;
    juce::AudioDeviceManager dm;
    std::atomic<int> numOutputs { 0 };
    std::atomic<double> rate { 44100.0 };
    std::vector<float> scratchL, scratchR;
    juce::String lastError;
    juce::StringArray lastNames;
};

//==============================================================================
class RouterEngine : private juce::Timer
{
public:
    RouterEngine();
    ~RouterEngine() override;

    void addSend (Send*);
    void removeSend (Send*);
    std::vector<SendInfo> snapshot();
    int findFreePair (int deviceIndex, const Send* exclude);

    DeviceOutput& device (int i)   { return *devices[(size_t) i]; }
    void saveSettings();

    juce::ChangeBroadcaster changed;   // UI listens to this
    juce::SpinLock sendLock;
    std::vector<Send*> sends;

private:
    void timerCallback() override;
    void loadSettings();

    std::unique_ptr<DeviceOutput> devices[kNumDevices];
};
} // namespace ac
