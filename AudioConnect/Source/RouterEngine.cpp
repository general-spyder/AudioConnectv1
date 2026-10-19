#include "RouterEngine.h"

namespace ac
{
//==============================================================================
void Send::push (int dev, const float* L, const float* R, int n)
{
    auto& ln = lanes[dev];
    const int64_t w = ln.writeCount.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const auto idx = (size_t) ((w + i) & kRingMask);
        ln.l[idx] = L[i];
        ln.r[idx] = R[i];
    }
    ln.writeCount.store (w + n, std::memory_order_release);
}

// Pull n samples at the device rate from a lane. Resamples (linear) and gently steers the
// ratio so the buffer fill stays near the target -> handles different sample rates AND
// clock drift between the DAW's driver and this device's driver.
static bool pullLane (Send& s, Lane& ln, int n, double devRate, float* oL, float* oR)
{
    const double hostRate = s.hostRate.load();
    const double base     = hostRate / juce::jmax (1.0, devRate);
    const double target   = juce::jmax (s.targetMs.load() * 0.001 * hostRate, base * n * 2.0 + 64.0);

    const int64_t w = ln.writeCount.load (std::memory_order_acquire);
    double avail = (double) w - ln.readPos;

    if (! ln.primed)
    {
        if (avail < target) return false;
        ln.readPos = (double) w - target;
        ln.ratio   = base;
        ln.primed  = true;
        avail      = target;
    }

    if (avail > kRing * 0.75)                      // producer ran far ahead: resync
    {
        ln.readPos = (double) w - target;
        avail = target;
    }

    const double err    = (avail - target) / target;
    const double wanted = base * (1.0 + juce::jlimit (-0.02, 0.02, err * 0.01));
    ln.ratio += (wanted - ln.ratio) * 0.05;

    if (avail < ln.ratio * n + 2.0)                // underrun
    {
        ln.primed = false;
        return false;
    }

    double pos = ln.readPos;
    for (int i = 0; i < n; ++i)
    {
        const auto i0   = (int64_t) std::floor (pos);
        const auto frac = (float) (pos - (double) i0);
        const auto a = (size_t) (i0 & kRingMask), b = (size_t) ((i0 + 1) & kRingMask);
        oL[i] = ln.l[a] + (ln.l[b] - ln.l[a]) * frac;
        oR[i] = ln.r[a] + (ln.r[b] - ln.r[a]) * frac;
        pos += ln.ratio;
    }
    ln.readPos = pos;
    return true;
}

//==============================================================================
static juce::File settingsFile()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("MiKeys Dsz Studios").getChildFile ("Audio Connect");
    d.createDirectory();
    return d.getChildFile ("devices.xml");
}

static bool looksOnboard (const juce::String& n)
{
    for (auto* k : { "realtek", "speaker", "headphone", "primary sound", "microsoft", "nvidia",
                     "hdmi", "built-in", "default", "digital audio", "steam", "virtual", "voicemeeter" })
        if (n.containsIgnoreCase (k)) return true;
    return false;
}

//==============================================================================
DeviceOutput::DeviceOutput (RouterEngine& e, int i) : engine (e), index (i)
{
    dm.initialise (0, 0, nullptr, false);
    dm.addAudioCallback (this);
}

DeviceOutput::~DeviceOutput()
{
    dm.removeAudioCallback (this);
    dm.closeAudioDevice();
}

juce::AudioIODeviceType* DeviceOutput::findType (const juce::String& n)
{
    for (auto* t : dm.getAvailableDeviceTypes())
        if (t->getTypeName() == n) return t;
    return nullptr;
}

juce::StringArray DeviceOutput::getTypeNames()
{
    juce::StringArray out;
    for (auto* t : dm.getAvailableDeviceTypes())
        if (t->getDeviceNames (false).size() > 0)
            out.add (t->getTypeName());
    return out;
}

juce::StringArray DeviceOutput::getDeviceNames (const juce::String& type)
{
    if (auto* t = findType (type)) return t->getDeviceNames (false);
    return {};
}

juce::String DeviceOutput::getDeviceName() const
{
    if (auto* d = dm.getCurrentAudioDevice()) return d->getName();
    return {};
}

juce::StringArray DeviceOutput::getChannelNames() const
{
    if (auto* d = dm.getCurrentAudioDevice()) return d->getOutputChannelNames();
    return {};
}

int DeviceOutput::getBufferSize() const
{
    if (auto* d = dm.getCurrentAudioDevice()) return d->getCurrentBufferSizeSamples();
    return 0;
}

bool DeviceOutput::open (const juce::String& type, const juce::String& device, bool remember)
{
    lastError = {};

    if (dm.getCurrentAudioDeviceType() != type)
        dm.setCurrentAudioDeviceType (type, true);

    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    s.inputDeviceName.clear();
    s.outputDeviceName = device;
    s.useDefaultInputChannels  = false;
    s.useDefaultOutputChannels = false;
    s.inputChannels.clear();
    s.outputChannels.clear();
    s.outputChannels.setRange (0, kMaxPairs * 2, true);   // request every output the device has

    lastError = dm.setAudioDeviceSetup (s, true);

    if (lastError.isEmpty())
    {
        savedType = type;
        savedDevice = device;
        if (remember) engine.saveSettings();
    }
    engine.changed.sendChangeMessage();
    return lastError.isEmpty();
}

void DeviceOutput::close()
{
    dm.closeAudioDevice();
    numOutputs = 0;
    engine.changed.sendChangeMessage();
}

bool DeviceOutput::autoDetect()
{
    for (auto* pref : { "ASIO", "Windows Audio (Exclusive Mode)", "Windows Audio (Low Latency Mode)",
                        "CoreAudio", "JACK", "ALSA", "Windows Audio", "DirectSound" })
    {
        if (auto* t = findType (pref))
        {
            t->scanForDevices();
            for (auto& n : t->getDeviceNames (false))
                if (! looksOnboard (n))
                    return open (pref, n);
        }
    }
    lastError = "No external audio interface found. Plug it in and press Rescan.";
    engine.changed.sendChangeMessage();
    return false;
}

bool DeviceOutput::openSystemDefault()
{
    for (auto* pref : { "Windows Audio", "CoreAudio", "ALSA", "DirectSound" })
    {
        if (auto* t = findType (pref))
        {
            t->scanForDevices();
            auto names = t->getDeviceNames (false);
            const int di = t->getDefaultDeviceIndex (false);
            if (juce::isPositiveAndBelow (di, names.size()))
                return open (pref, names[di]);
        }
    }
    return false;
}

bool DeviceOutput::rescan()
{
    auto* t = dm.getCurrentAudioDeviceType().isNotEmpty() ? dm.getCurrentDeviceTypeObject() : nullptr;
    if (t == nullptr) return false;

    auto before = t->getDeviceNames (false);
    t->scanForDevices();
    auto after = t->getDeviceNames (false);
    return before != after;
}

void DeviceOutput::audioDeviceAboutToStart (juce::AudioIODevice* d)
{
    numOutputs = d->getActiveOutputChannels().countNumberOfSetBits();
    rate = d->getCurrentSampleRate();
    const auto sz = (size_t) (d->getCurrentBufferSizeSamples() * 4 + 4096);
    scratchL.assign (sz, 0.0f);
    scratchR.assign (sz, 0.0f);
}

void DeviceOutput::audioDeviceStopped() { numOutputs = 0; }

void DeviceOutput::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out,
                                                    int numOut, int n,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr) juce::FloatVectorOperations::clear (out[c], n);

    if (n > (int) scratchL.size()) return;

    juce::SpinLock::ScopedTryLockType lock (engine.sendLock);
    if (! lock.isLocked()) return;

    const double devRate = rate.load();
    float* L = scratchL.data();
    float* R = scratchR.data();

    for (auto* s : engine.sends)
    {
        auto& lane = s->lanes[index];
        const uint32_t mask = s->mask[index].load();

        if (mask == 0) { lane.primed = false; continue; }
        if (! pullLane (*s, lane, n, devRate, L, R)) continue;

        for (int p = 0; p < kMaxPairs; ++p)
        {
            if ((mask & (1u << p)) == 0) continue;
            const int cl = p * 2, cr = cl + 1;
            if (cl >= numOut || out[cl] == nullptr) continue;

            if (cr < numOut && out[cr] != nullptr)
            {
                juce::FloatVectorOperations::add (out[cl], L, n);
                juce::FloatVectorOperations::add (out[cr], R, n);
            }
            else   // odd last channel: fold to mono
            {
                for (int i = 0; i < n; ++i) out[cl][i] += 0.5f * (L[i] + R[i]);
            }
        }
    }
}

//==============================================================================
RouterEngine::RouterEngine()
{
    for (int i = 0; i < kNumDevices; ++i)
        devices[i] = std::make_unique<DeviceOutput> (*this, i);

    loadSettings();

    for (int i = 0; i < kNumDevices; ++i)
    {
        auto& d = *devices[i];
        bool ok = false;
        if (d.savedDevice.isNotEmpty() && d.getDeviceNames (d.savedType).contains (d.savedDevice))
            ok = d.open (d.savedType, d.savedDevice, false);
        if (! ok)
            ok = (i == 0) ? d.autoDetect() : d.openSystemDefault();
    }
    startTimer (3000);   // hot-plug detection
}

RouterEngine::~RouterEngine() { stopTimer(); }

void RouterEngine::addSend (Send* s)
{
    { juce::SpinLock::ScopedLockType l (sendLock); sends.push_back (s); }
    changed.sendChangeMessage();
}

void RouterEngine::removeSend (Send* s)
{
    { juce::SpinLock::ScopedLockType l (sendLock);
      sends.erase (std::remove (sends.begin(), sends.end(), s), sends.end()); }
    changed.sendChangeMessage();
}

std::vector<SendInfo> RouterEngine::snapshot()
{
    std::vector<SendInfo> v;
    juce::SpinLock::ScopedLockType l (sendLock);
    for (auto* s : sends)
    {
        SendInfo i;
        i.name = s->getName();
        i.ptr = s;
        for (int k = 0; k < 2; ++k) { i.dev[k] = s->uiDev[k].load(); i.pair[k] = s->uiPair[k].load(); }
        v.push_back (i);
    }
    return v;
}

int RouterEngine::findFreePair (int dev, const Send* exclude)
{
    const int np = juce::jlimit (1, kMaxPairs, device (dev).getNumPairs());
    juce::SpinLock::ScopedLockType l (sendLock);
    for (int p = 1; p <= np; ++p)
    {
        bool used = false;
        for (auto* s : sends)
            if (s != exclude)
                for (int k = 0; k < 2; ++k)
                    if (s->uiDev[k].load() == dev + 1 && s->uiPair[k].load() == p) used = true;
        if (! used) return p;
    }
    return 1;
}

void RouterEngine::timerCallback()
{
    bool any = false;
    for (int i = 0; i < kNumDevices; ++i)
    {
        auto& d = *devices[i];
        if (! d.rescan()) continue;
        any = true;

        if (! d.isOpen() && d.savedDevice.isNotEmpty()
            && d.getDeviceNames (d.savedType).contains (d.savedDevice))
            d.open (d.savedType, d.savedDevice, false);                    // interface re-plugged
        else if (d.isOpen() && ! d.getDeviceNames (d.getTypeName()).contains (d.getDeviceName()))
            d.close();                                                      // interface unplugged
        else if (! d.isOpen() && i == 0 && d.savedDevice.isEmpty())
            d.autoDetect();                                                 // first interface plugged in
    }
    if (any) changed.sendChangeMessage();
}

void RouterEngine::saveSettings()
{
    juce::XmlElement root ("AudioConnect");
    for (int i = 0; i < kNumDevices; ++i)
    {
        auto* e = root.createNewChildElement ("Device");
        e->setAttribute ("index", i);
        e->setAttribute ("type", devices[i]->savedType);
        e->setAttribute ("name", devices[i]->savedDevice);
    }
    root.writeTo (settingsFile());
}

void RouterEngine::loadSettings()
{
    if (auto xml = juce::XmlDocument::parse (settingsFile()))
        for (auto* e : xml->getChildIterator())
        {
            const int i = e->getIntAttribute ("index", -1);
            if (juce::isPositiveAndBelow (i, kNumDevices))
            {
                devices[i]->savedType   = e->getStringAttribute ("type");
                devices[i]->savedDevice = e->getStringAttribute ("name");
            }
        }
}
} // namespace ac
