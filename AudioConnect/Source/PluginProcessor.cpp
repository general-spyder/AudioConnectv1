#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioConnectProcessor::AudioConnectProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AUDIOCONNECT", createLayout())
{
    pGain    = apvts.getRawParameterValue ("gain");
    pPass    = apvts.getRawParameterValue ("pass");
    pLatency = apvts.getRawParameterValue ("latency");
    pDestA   = apvts.getRawParameterValue ("destA");
    pPairA   = apvts.getRawParameterValue ("pairA");
    pDestB   = apvts.getRawParameterValue ("destB");
    pPairB   = apvts.getRawParameterValue ("pairB");

    engine->addSend (&send);
}

AudioConnectProcessor::~AudioConnectProcessor()
{
    engine->removeSend (&send);
}

AudioProcessorValueTreeState::ParameterLayout AudioConnectProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    const StringArray dests { "Off", "Audio Interface", "PC / Headphones" };

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain",
                                                  NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "pass", 1 }, "Pass-through to host", false));
    l.add (std::make_unique<AudioParameterInt>   (ParameterID { "latency", 1 }, "Buffer (ms)", 5, 200, 20));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "destA", 1 }, "Route A", dests, 1));
    l.add (std::make_unique<AudioParameterInt>   (ParameterID { "pairA", 1 }, "Route A pair", 1, ac::kMaxPairs, 1));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "destB", 1 }, "Route B", dests, 0));
    l.add (std::make_unique<AudioParameterInt>   (ParameterID { "pairB", 1 }, "Route B pair", 1, ac::kMaxPairs, 1));
    return l;
}

void AudioConnectProcessor::prepareToPlay (double sr, int bs)
{
    const auto n = (size_t) jmax (bs, 4096);
    tmpL.assign (n, 0.0f);
    tmpR.assign (n, 0.0f);
    send.hostRate = sr;
    lastGain = Decibels::decibelsToGain (pGain->load(), -60.0f);
}

bool AudioConnectProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void AudioConnectProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0 || n == 0) return;

    const int a  = (int) pDestA->load(), b  = (int) pDestB->load();
    const int pa = jlimit (1, ac::kMaxPairs, (int) pPairA->load());
    const int pb = jlimit (1, ac::kMaxPairs, (int) pPairB->load());

    uint32_t m[2] = { 0, 0 };
    if (a > 0) m[a - 1] |= 1u << (pa - 1);
    if (b > 0) m[b - 1] |= 1u << (pb - 1);

    send.uiDev[0] = a;  send.uiPair[0] = pa;
    send.uiDev[1] = b;  send.uiPair[1] = pb;
    send.hostRate = getSampleRate();
    send.targetMs = (int) pLatency->load();
    send.mask[0].store (m[0]);
    send.mask[1].store (m[1]);

    if (m[0] != 0 || m[1] != 0)
    {
        const float g1 = Decibels::decibelsToGain (pGain->load(), -60.0f);
        const float g0 = lastGain;
        lastGain = g1;

        const float* inL = buffer.getReadPointer (0);
        const float* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : inL;

        const int chunk = (int) tmpL.size();
        for (int off = 0; off < n; off += chunk)
        {
            const int len = jmin (chunk, n - off);
            for (int i = 0; i < len; ++i)
            {
                const float g = g0 + (g1 - g0) * (float) (off + i) / (float) n;
                tmpL[(size_t) i] = inL[off + i] * g;
                tmpR[(size_t) i] = inR[off + i] * g;
            }
            for (int d = 0; d < ac::kNumDevices; ++d)
                if (m[d] != 0) send.push (d, tmpL.data(), tmpR.data(), len);
        }
    }

    const float peak = buffer.getMagnitude (0, n);
    level.store (jmax (peak, level.load() * 0.92f));

    if (pPass->load() < 0.5f)
        buffer.clear();            // audio now leaves via Audio Connect only
}

void AudioConnectProcessor::autoAssign()
{
    const int dev = (int) pDestA->load();
    if (dev <= 0) return;
    const int p = engine->findFreePair (dev - 1, &send);
    if (auto* prm = apvts.getParameter ("pairA"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) p));
        prm->endChangeGesture();
    }
}

void AudioConnectProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("instanceName", send.getName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void AudioConnectProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            send.setName (vt.getProperty ("instanceName", "Loops").toString());
            apvts.replaceState (vt);
        }
}

AudioProcessorEditor* AudioConnectProcessor::createEditor() { return new AudioConnectEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AudioConnectProcessor(); }
