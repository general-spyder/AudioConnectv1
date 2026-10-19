// Audio Connect - by MiKeys Dsz Studios
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "RouterEngine.h"

class AudioConnectProcessor : public juce::AudioProcessor
{
public:
    AudioConnectProcessor();
    ~AudioConnectProcessor() override;

    const juce::String getName() const override { return "Audio Connect"; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::String getInstanceName() const { return send.getName(); }
    void setInstanceName (const juce::String& n) { send.setName (n); }
    void autoAssign();

    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<ac::RouterEngine> engine;
    std::atomic<float> level { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ac::Send send;
    std::atomic<float> *pGain, *pPass, *pLatency, *pDestA, *pPairA, *pDestB, *pPairB;
    std::vector<float> tmpL, tmpR;
    float lastGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioConnectProcessor)
};
