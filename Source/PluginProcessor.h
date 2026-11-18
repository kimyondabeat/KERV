#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "Library.h"
#include "Presets.h"

// Simple stereo biquad (RBJ cookbook), allocation-free so it can be retuned on the audio thread.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    float lastGain = 1000.0f;
    void setShelf (bool high, double sr, float freq, float gainDb);
    void setPeak (double sr, float freq, float q, float gainDb);
    inline float process (int ch, float x)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
};

class KervProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    KervProcessor();
    ~KervProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "KERV"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Pads (message thread) ----
    int getPadCategory (int pad) const { return padCategory[(size_t) pad].load(); }
    bool canAssign (int pad, const SamplePtr& s) const { return s == nullptr || s->category == getPadCategory (pad); }
    bool assignSample (int pad, SamplePtr);           // refused when the sound is from another category
    void assignLayer (int pad, SamplePtr);            // the layer can be any sound
    SamplePtr getPadSample (int pad) const { return std::atomic_load (&padSamples[(size_t) pad]); }
    SamplePtr getLayerSample (int pad) const { return std::atomic_load (&layerSamples[(size_t) pad]); }
    void triggerPad (int pad, float velocity) { pendingTriggers[(size_t) pad].store (velocity); }
    void releasePad (int pad) { pendingReleases[(size_t) pad].store (true); }
    void preview (SamplePtr);
    uint32_t getHitCount (int pad) const { return hitCounts[(size_t) pad].load(); }
    void randomizeKit();
    void resetSoundControls (int pad);
    void resetChannelFx (int pad);

    // ---- Sequencer ----
    int getStep (int pad, int step) const { return pattern[(size_t) pad][(size_t) step].load(); }
    void setStep (int pad, int step, int value) { pattern[(size_t) pad][(size_t) step].store ((uint8_t) value); }
    void clearPattern();
    void loadDefaultPattern();
    int getPlayingStep() const { return playingStep.load(); }
    void setInternalPlay (bool shouldPlay) { internalPlay.store (shouldPlay); }
    bool getInternalPlay() const { return internalPlay.load(); }

    // ---- Kits ----
    std::unique_ptr<juce::XmlElement> createPresetXml (const juce::String& name) const;
    void applyPresetXml (const juce::XmlElement&, bool includeParameters = true);
    juce::String getPresetName() const { return presetName; }

    juce::AudioProcessorValueTreeState apvts;

    static juce::String pid (int pad, const char* what) { return "p" + juce::String (pad + 1) + "_" + what; }

private:
    struct Voice
    {
        SamplePtr data;
        double position = 0.0, increment = 1.0;
        float gain = 1.0f;
        float env = 1.0f, envCoef = 1.0f, relCoef = 1.0f;
        int attackSamples = 0, age = 0, startDelay = 0;
        int pad = -1;            // numPads = preview
        int fadeRemaining = 0;
        uint32_t order = 0;
        bool active = false, releasing = false;
    };

    struct Event { int pos; int pad; float velocity; bool noteOff; };

    struct PadParams
    {
        std::atomic<float> *vol, *pan, *pitch, *attack, *decay, *release, *rev, *dly, *filter,
                           *eqLow, *eqMid, *eqHigh, *comp, *sat, *layerOn, *blend, *layerPitch, *layerOffset, *mute, *solo;
    };

    struct Channel // per-pad mixing state
    {
        Biquad low, mid, high;
        float compEnv = 0.0f;
        juce::dsp::StateVariableTPTFilter<float> lowpass, highpass;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setPadSample (int pad, SamplePtr);
    void startVoice (int pad, float velocity);
    void startOneVoice (int pad, SamplePtr data, float gain, float pitch, int delaySamples);
    void releaseVoices (int pad);
    void chokeClass (int cls);
    void renderVoices (int from, int to);
    void processPadBuses (juce::AudioBuffer<float>& out, int start, int num);
    void collectSequencerEvents (int numSamples);
    void timerCallback() override;

    std::array<SamplePtr, numPads> padSamples, layerSamples;
    std::array<std::atomic<int>, numPads> padCategory {};
    std::vector<SamplePtr> retired;
    SamplePtr previewSample;
    juce::String presetName { "Classic Kompa" };

    std::array<std::atomic<float>, numPads> pendingTriggers {};
    std::array<std::atomic<bool>, numPads> pendingReleases {};
    std::array<std::atomic<uint32_t>, numPads> hitCounts {};

    std::array<Voice, 128> voices;
    uint32_t voiceCounter = 0;
    double sr = 44100.0;
    int fadeLength = 64;
    static constexpr int chunk = 256;
    int chunkStart = 0;
    juce::AudioBuffer<float> padBus, revBus, dlyBus, previewBus;

    std::array<Channel, numPads> channels;
    std::array<juce::SmoothedValue<float>, numPads> gainL, gainR, revSend, dlySend;
    juce::SmoothedValue<float> masterGain;
    juce::Reverb reverb;
    juce::AudioBuffer<float> delayLine;
    int delayWrite = 0;
    float delayDampL = 0.0f, delayDampR = 0.0f;

    std::array<std::array<std::atomic<uint8_t>, numSteps>, numPads> pattern {};
    std::atomic<int> playingStep { -1 };
    std::atomic<bool> internalPlay { false };
    double internalPpq = 0.0, currentBpm = 100.0;
    std::vector<Event> events;

    std::array<PadParams, numPads> pp {};
    std::atomic<float>* masterP = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KervProcessor)
};
