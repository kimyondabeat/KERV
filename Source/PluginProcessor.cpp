#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float value)
    {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void resetParam (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());
    }

    // Transparent below -2 dB, then rounds peaks off so the output never hard-clips.
    inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    }

    constexpr const char* soundControlIds[] = { "pitch", "attack", "decay", "release", "rev", "dly", "filter" };
    constexpr const char* channelFxIds[]    = { "eqlow", "eqmid", "eqhigh", "comp", "sat" };
}

// ---------------------------------------------------------------------------
void Biquad::setShelf (bool high, double sampleRate, float freq, float gainDb)
{
    lastGain = gainDb;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / 2.0 * std::sqrt (2.0); // S = 1
    const double sA = 2.0 * std::sqrt (A) * alpha;
    double B0, B1, B2, A0, A1, A2;
    if (high)
    {
        B0 = A * ((A + 1) + (A - 1) * cw + sA);  B1 = -2 * A * ((A - 1) + (A + 1) * cw);  B2 = A * ((A + 1) + (A - 1) * cw - sA);
        A0 = (A + 1) - (A - 1) * cw + sA;        A1 = 2 * ((A - 1) - (A + 1) * cw);        A2 = (A + 1) - (A - 1) * cw - sA;
    }
    else
    {
        B0 = A * ((A + 1) - (A - 1) * cw + sA);  B1 = 2 * A * ((A - 1) - (A + 1) * cw);   B2 = A * ((A + 1) - (A - 1) * cw - sA);
        A0 = (A + 1) + (A - 1) * cw + sA;        A1 = -2 * ((A - 1) + (A + 1) * cw);       A2 = (A + 1) + (A - 1) * cw - sA;
    }
    b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
}

void Biquad::setPeak (double sampleRate, float freq, float q, float gainDb)
{
    lastGain = gainDb;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    const double alpha = std::sin (w0) / (2.0 * q);
    const double cw = std::cos (w0);
    const double A0 = 1 + alpha / A;
    b0 = (float) ((1 + alpha * A) / A0); b1 = (float) (-2 * cw / A0); b2 = (float) ((1 - alpha * A) / A0);
    a1 = (float) (-2 * cw / A0);         a2 = (float) ((1 - alpha / A) / A0);
}

// ---------------------------------------------------------------------------
KervProcessor::KervProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KERV", createLayout())
{
    Library::get(); // decode the built-in sounds now, not on the audio thread

    for (int p = 0; p < numPads; ++p)
    {
        auto get = [&] (const char* w) { return apvts.getRawParameterValue (pid (p, w)); };
        pp[(size_t) p] = { get ("vol"), get ("pan"), get ("pitch"), get ("attack"), get ("decay"), get ("release"),
                           get ("rev"), get ("dly"), get ("filter"), get ("eqlow"), get ("eqmid"), get ("eqhigh"),
                           get ("comp"), get ("sat"), get ("layer"), get ("blend"), get ("lpitch"), get ("loffset"),
                           get ("mute"), get ("solo") };
        pendingTriggers[(size_t) p].store (0.0f);
        pendingReleases[(size_t) p].store (false);
        hitCounts[(size_t) p].store (0);
        padCategory[(size_t) p].store (juce::jmax (0, padRoles[(size_t) p].category));
    }
    masterP = apvts.getRawParameterValue ("master");

    auto factory = Presets::factory();
    applyPresetXml (*factory.front());
    loadDefaultPattern();
    startTimerHz (2);
}

KervProcessor::~KervProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout KervProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto releaseText = [] (float v, int) { return v >= 0.995f ? String ("Full")
                                                              : String (juce::roundToInt (10.0 * std::pow (400.0, (double) v))) + " ms"; };
    auto pctText = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };

    for (int p = 0; p < numPads; ++p)
    {
        const String n = "Pad " + String (p + 1) + " ";
        auto f = [&] (const char* id, const String& name, float lo, float hi, float def, const String& label = {},
                      std::function<String (float, int)> text = nullptr)
        {
            auto attrs = AudioParameterFloatAttributes().withLabel (label);
            if (text) attrs = attrs.withStringFromValueFunction (text);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid (p, id), 1 }, n + name,
                                                               NormalisableRange<float> (lo, hi, 0.01f), def, attrs));
        };
        f ("vol", "Volume", 0.0f, 2.0f, 1.0f);
        f ("pan", "Pan", -1.0f, 1.0f, 0.0f);
        f ("pitch", "Pitch", -24.0f, 24.0f, 0.0f, "st");
        f ("attack", "Attack", 0.0f, 200.0f, 0.0f, "ms");
        f ("decay", "Decay", 0.0f, 1.0f, 1.0f, {}, pctText);
        f ("release", "Release", 0.0f, 1.0f, 1.0f, {}, releaseText);
        f ("rev", "Reverb", 0.0f, 1.0f, 0.0f, {}, pctText);
        f ("dly", "Delay", 0.0f, 1.0f, 0.0f, {}, pctText);
        f ("filter", "Filter", -1.0f, 1.0f, 0.0f);
        f ("eqlow", "EQ Low", -12.0f, 12.0f, 0.0f, "dB");
        f ("eqmid", "EQ Mid", -12.0f, 12.0f, 0.0f, "dB");
        f ("eqhigh", "EQ High", -12.0f, 12.0f, 0.0f, "dB");
        f ("comp", "Compressor", 0.0f, 1.0f, 0.0f, {}, pctText);
        f ("sat", "Saturation", 0.0f, 1.0f, 0.0f, {}, pctText);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (p, "layer"), 1 }, n + "Layer On", false));
        f ("blend", "Layer Blend", 0.0f, 1.0f, 0.5f, {}, pctText);
        f ("lpitch", "Layer Pitch", -24.0f, 24.0f, 0.0f, "st");
        f ("loffset", "Layer Offset", 0.0f, 60.0f, 0.0f, "ms");
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (p, "mute"), 1 }, n + "Mute", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (p, "solo"), 1 }, n + "Solo", false));
    }

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master",
                                                       NormalisableRange<float> (0.0f, 2.0f, 0.01f), 0.8f));
    return layout;
}

bool KervProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void KervProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    fadeLength = juce::jmax (16, (int) (sampleRate * 0.004));

    for (auto& v : voices) { v.active = false; v.data.reset(); }

    padBus.setSize (numPads * 2, chunk);
    revBus.setSize (2, chunk);
    dlyBus.setSize (2, chunk);
    previewBus.setSize (2, chunk);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) chunk, 2 };
    for (size_t p = 0; p < (size_t) numPads; ++p)
    {
        auto& c = channels[p];
        c.lowpass.prepare (spec);  c.lowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        c.highpass.prepare (spec); c.highpass.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        c.low.lastGain = c.mid.lastGain = c.high.lastGain = 1000.0f; // force a retune
        c.low.reset(); c.mid.reset(); c.high.reset();
        c.compEnv = 0.0f;
        for (auto* s : { &gainL[p], &gainR[p], &revSend[p], &dlySend[p] })
            s->reset (sampleRate, 0.02);
    }
    masterGain.reset (sampleRate, 0.02);

    reverb.setSampleRate (sampleRate);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.72f; rp.damping = 0.45f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
    reverb.setParameters (rp);
    reverb.reset();

    delayLine.setSize (2, (int) (sampleRate * 4.0));
    delayLine.clear();
    delayWrite = 0;
    delayDampL = delayDampR = 0.0f;

    internalPpq = 0.0;
    events.reserve (2048);
}

// ---------------------------------------------------------------------------
// Pads
void KervProcessor::setPadSample (int pad, SamplePtr s)
{
    auto old = std::atomic_exchange (&padSamples[(size_t) pad], std::move (s));
    if (old != nullptr)
        retired.push_back (std::move (old)); // freed by the timer, never on the audio thread
}

bool KervProcessor::assignSample (int pad, SamplePtr s)
{
    if (pad < 0 || pad >= numPads || ! canAssign (pad, s))
        return false;
    setPadSample (pad, std::move (s));
    return true;
}

void KervProcessor::assignLayer (int pad, SamplePtr s)
{
    if (pad < 0 || pad >= numPads)
        return;
    auto old = std::atomic_exchange (&layerSamples[(size_t) pad], std::move (s));
    if (old != nullptr)
        retired.push_back (std::move (old));
}

void KervProcessor::preview (SamplePtr s)
{
    std::atomic_store (&previewSample, std::move (s));
}

void KervProcessor::randomizeKit()
{
    juce::Random rnd;
    for (int p = 0; p < numPads; ++p)
    {
        const auto current = getPadSample (p);
        auto options = Library::get().inCategory (getPadCategory (p));

        // keep open/closed hats and cymbals in their role so the choke still makes sense
        if (current != nullptr && current->chokeClass != 0)
            options.erase (std::remove_if (options.begin(), options.end(),
                                           [&] (const SamplePtr& s) { return s->chokeClass != current->chokeClass; }),
                           options.end());
        if (options.size() > 1 && current != nullptr)
            options.erase (std::remove (options.begin(), options.end(), current), options.end());

        if (! options.empty())
            setPadSample (p, options[(size_t) rnd.nextInt ((int) options.size())]);
    }
    presetName = "Random Kit";
}

void KervProcessor::resetSoundControls (int pad)
{
    for (auto* id : soundControlIds)
        resetParam (apvts, pid (pad, id));
}

void KervProcessor::resetChannelFx (int pad)
{
    for (auto* id : channelFxIds)
        resetParam (apvts, pid (pad, id));
}

void KervProcessor::clearPattern()
{
    for (auto& row : pattern)
        for (auto& s : row)
            s.store (0);
}

void KervProcessor::loadDefaultPattern()
{
    for (int p = 0; p < numPads; ++p)
        for (int s = 0; s < numSteps; ++s)
        {
            const char c = defaultPattern[(size_t) p][s];
            setStep (p, s, c == 'X' ? 2 : c == 'x' ? 1 : 0);
        }
}

void KervProcessor::timerCallback()
{
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [] (const SamplePtr& s) { return s.use_count() <= 1; }),
                   retired.end());
}

// ---------------------------------------------------------------------------
// Voices
void KervProcessor::chokeClass (int cls)
{
    for (auto& v : voices)
        if (v.active && v.fadeRemaining == 0 && v.data != nullptr && v.data->chokeClass == cls)
            v.fadeRemaining = fadeLength;
}

void KervProcessor::startOneVoice (int pad, SamplePtr data, float gain, float pitch, int delaySamples)
{
    if (data == nullptr || data->buffer.getNumSamples() < 2 || gain <= 0.0001f)
        return;

    switch (data->chokeClass) // closed hat cuts open hat, cymbal cuts open cymbal
    {
        case 1: case 2: chokeClass (2); break;
        case 3: case 4: chokeClass (4); break;
        default: break;
    }

    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.order < target->order) target = &v;
    }

    float decay = 1.0f, attackMs = 0.0f;
    if (pad < numPads)
    {
        decay = pp[(size_t) pad].decay->load();
        attackMs = pp[(size_t) pad].attack->load();
    }

    target->data = std::move (data);
    target->position = 0.0;
    target->increment = (target->data->sampleRate / sr) * std::pow (2.0, pitch / 12.0);
    target->gain = gain;
    target->env = 1.0f;
    target->envCoef = decay >= 0.995f ? 1.0f : (float) std::exp (-1.0 / (0.03 * std::pow (100.0, (double) decay) * sr));
    target->relCoef = 1.0f;
    target->releasing = false;
    target->attackSamples = (int) (attackMs * 0.001 * sr);
    target->age = 0;
    target->startDelay = delaySamples;
    target->pad = pad;
    target->fadeRemaining = 0;
    target->order = ++voiceCounter;
    target->active = true;
}

void KervProcessor::startVoice (int pad, float velocity)
{
    const float vel = juce::jmap (juce::jlimit (0.0f, 1.0f, velocity), 0.12f, 1.0f);
    const auto& prm = pp[(size_t) pad];
    const float pitch = prm.pitch->load();
    auto main = getPadSample (pad);
    auto layer = prm.layerOn->load() > 0.5f ? getLayerSample (pad) : nullptr;

    // Blend: 0 = main only, 50 % = both full, 100 % = layer only
    const float b = prm.blend->load();
    const float mainGain = layer != nullptr ? juce::jmin (1.0f, 2.0f * (1.0f - b)) : 1.0f;
    const float layerGain = juce::jmin (1.0f, 2.0f * b);

    startOneVoice (pad, main, vel * mainGain, pitch, 0);
    if (layer != nullptr)
        startOneVoice (pad, layer, vel * layerGain, pitch + prm.layerPitch->load(),
                       (int) (prm.layerOffset->load() * 0.001 * sr));

    if (main != nullptr || layer != nullptr)
        hitCounts[(size_t) pad].fetch_add (1);
}

void KervProcessor::releaseVoices (int pad)
{
    const float rel = pp[(size_t) pad].release->load();
    if (rel >= 0.995f)
        return; // "Full": the sound always plays to the end
    const float coef = (float) std::exp (-1.0 / (0.01 * std::pow (400.0, (double) rel) * sr));
    for (auto& v : voices)
        if (v.active && v.pad == pad && ! v.releasing)
        {
            v.releasing = true;
            v.relCoef = coef;
        }
}

void KervProcessor::renderVoices (int from, int to)
{
    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        const auto& buf = v.data->buffer;
        const int len = buf.getNumSamples();
        const float* l = buf.getReadPointer (0);
        const float* r = buf.getReadPointer (buf.getNumChannels() > 1 ? 1 : 0);
        float* outL = v.pad < numPads ? padBus.getWritePointer (v.pad * 2)     : previewBus.getWritePointer (0);
        float* outR = v.pad < numPads ? padBus.getWritePointer (v.pad * 2 + 1) : previewBus.getWritePointer (1);

        for (int i = from; i < to; ++i)
        {
            if (v.startDelay > 0) { --v.startDelay; continue; }

            const int idx = (int) v.position;
            if (idx + 1 >= len || v.env < 0.0003f) { v.active = false; break; }

            const float frac = (float) (v.position - idx);
            float g = v.gain * v.env;
            v.env *= v.envCoef;
            if (v.releasing) v.env *= v.relCoef;
            if (v.age < v.attackSamples) g *= (float) v.age / (float) v.attackSamples;
            ++v.age;

            if (v.fadeRemaining > 0)
            {
                g *= (float) v.fadeRemaining / (float) fadeLength;
                if (--v.fadeRemaining == 0) v.active = false;
            }

            const int o = i - chunkStart;
            outL[o] += (l[idx] + frac * (l[idx + 1] - l[idx])) * g;
            outR[o] += (r[idx] + frac * (r[idx + 1] - r[idx])) * g;
            v.position += v.increment;
            if (! v.active) break;
        }
    }
}

void KervProcessor::processPadBuses (juce::AudioBuffer<float>& out, int start, int num)
{
    bool anySolo = false;
    for (const auto& p : pp)
        anySolo = anySolo || p.solo->load() > 0.5f;

    auto* oL = out.getWritePointer (0, start);
    auto* oR = out.getWritePointer (1, start);
    auto* rL = revBus.getWritePointer (0);
    auto* rR = revBus.getWritePointer (1);
    auto* dL = dlyBus.getWritePointer (0);
    auto* dR = dlyBus.getWritePointer (1);
    revBus.clear (0, num);
    dlyBus.clear (0, num);

    const float attCoef = (float) std::exp (-1.0 / (0.001 * sr));
    const float relCoef = (float) std::exp (-1.0 / (0.12 * sr));

    for (size_t p = 0; p < (size_t) numPads; ++p)
    {
        const auto& prm = pp[p];
        auto& ch = channels[p];
        const bool audible = prm.mute->load() < 0.5f && (! anySolo || prm.solo->load() > 0.5f);
        const float vol = audible ? prm.vol->load() : 0.0f;
        const float angle = (prm.pan->load() + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        gainL[p].setTargetValue (vol * std::cos (angle) * juce::MathConstants<float>::sqrt2);
        gainR[p].setTargetValue (vol * std::sin (angle) * juce::MathConstants<float>::sqrt2);
        revSend[p].setTargetValue (prm.rev->load());
        dlySend[p].setTargetValue (prm.dly->load());

        float* L = padBus.getWritePointer ((int) p * 2);
        float* R = padBus.getWritePointer ((int) p * 2 + 1);

        // Filter: left = low-pass, right = high-pass, middle = off
        const float f = prm.filter->load();
        if (f < -0.02f)
        {
            ch.lowpass.setCutoffFrequency (20000.0f * std::pow (0.0075f, -f));
            for (int i = 0; i < num; ++i) { L[i] = ch.lowpass.processSample (0, L[i]); R[i] = ch.lowpass.processSample (1, R[i]); }
        }
        else ch.lowpass.reset();
        if (f > 0.02f)
        {
            ch.highpass.setCutoffFrequency (20.0f * std::pow (250.0f, f));
            for (int i = 0; i < num; ++i) { L[i] = ch.highpass.processSample (0, L[i]); R[i] = ch.highpass.processSample (1, R[i]); }
        }
        else ch.highpass.reset();

        // 3-band EQ
        const float gl = prm.eqLow->load(), gm = prm.eqMid->load(), gh = prm.eqHigh->load();
        if (std::abs (gl - ch.low.lastGain) > 1.0e-4f)  ch.low.setShelf (false, sr, 120.0f, gl);
        if (std::abs (gm - ch.mid.lastGain) > 1.0e-4f)  ch.mid.setPeak (sr, 1200.0f, 0.8f, gm);
        if (std::abs (gh - ch.high.lastGain) > 1.0e-4f) ch.high.setShelf (true, sr, 7500.0f, gh);
        const bool eqOn = std::abs (gl) > 0.05f || std::abs (gm) > 0.05f || std::abs (gh) > 0.05f;
        if (eqOn)
            for (int i = 0; i < num; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float& x = c == 0 ? L[i] : R[i];
                    x = ch.high.process (c, ch.mid.process (c, ch.low.process (c, x)));
                }
        else { ch.low.reset(); ch.mid.reset(); ch.high.reset(); }

        // Compressor: one knob sets threshold + ratio, with automatic make-up gain
        const float comp = prm.comp->load();
        if (comp > 0.01f)
        {
            const float thr = -2.0f - 22.0f * comp, ratio = 1.0f + 5.0f * comp;
            // make-up gain matched for a drum hit around -6 dBFS, so turning the knob changes punch, not level
            const float makeup = juce::Decibels::decibelsToGain (juce::jmax (0.0f, -6.0f - thr) * (1.0f - 1.0f / ratio) * 0.9f);
            for (int i = 0; i < num; ++i)
            {
                const float in = juce::jmax (std::abs (L[i]), std::abs (R[i]));
                ch.compEnv = in > ch.compEnv ? attCoef * ch.compEnv + (1.0f - attCoef) * in
                                             : relCoef * ch.compEnv + (1.0f - relCoef) * in;
                const float levelDb = juce::Decibels::gainToDecibels (ch.compEnv, -100.0f);
                const float over = levelDb - thr;
                const float g = over > 0.0f ? juce::Decibels::decibelsToGain (-over * (1.0f - 1.0f / ratio)) : 1.0f;
                L[i] *= g * makeup; R[i] *= g * makeup;
            }
        }
        else ch.compEnv = 0.0f;

        // Saturation: warm tape-style drive
        const float sat = prm.sat->load();
        if (sat > 0.01f)
        {
            const float drive = 1.0f + 5.0f * sat, norm = 1.0f / std::tanh (drive), mix = 0.8f * sat;
            for (int i = 0; i < num; ++i)
            {
                L[i] += mix * (std::tanh (L[i] * drive) * norm - L[i]);
                R[i] += mix * (std::tanh (R[i] * drive) * norm - R[i]);
            }
        }

        for (int i = 0; i < num; ++i)
        {
            const float l = L[i] * gainL[p].getNextValue();
            const float r = R[i] * gainR[p].getNextValue();
            const float rs = revSend[p].getNextValue(), ds = dlySend[p].getNextValue();
            oL[i] += l; oR[i] += r;
            rL[i] += l * rs; rR[i] += r * rs;
            dL[i] += l * ds; dR[i] += r * ds;
        }
    }

    // Delay: dotted 1/8 synced to the tempo, ping-pong with a darker feedback
    const int delaySamples = juce::jlimit (1, delayLine.getNumSamples() - 1, (int) (0.75 * 60.0 / currentBpm * sr));
    const int size = delayLine.getNumSamples();
    auto* bl = delayLine.getWritePointer (0);
    auto* br = delayLine.getWritePointer (1);
    for (int i = 0; i < num; ++i)
    {
        const int readPos = (delayWrite - delaySamples + size) % size;
        const float wl = bl[readPos], wr = br[readPos];
        delayDampL += 0.35f * (wl - delayDampL);
        delayDampR += 0.35f * (wr - delayDampR);
        bl[delayWrite] = dL[i] + delayDampR * 0.38f;
        br[delayWrite] = dR[i] + delayDampL * 0.38f;
        delayWrite = (delayWrite + 1) % size;
        oL[i] += wl * 0.8f; oR[i] += wr * 0.8f;
    }

    reverb.processStereo (rL, rR, num);

    auto* pL = previewBus.getReadPointer (0);
    auto* pR = previewBus.getReadPointer (1);
    for (int i = 0; i < num; ++i)
    {
        const float m = masterGain.getNextValue();
        oL[i] = softClip ((oL[i] + rL[i] * 0.6f + pL[i]) * m);
        oR[i] = softClip ((oR[i] + rR[i] * 0.6f + pR[i]) * m);
    }
}

// ---------------------------------------------------------------------------
// Sequencer: follows the DAW's tempo and play button (or its own PLAY button).
void KervProcessor::collectSequencerEvents (int numSamples)
{
    double bpm = 100.0;
    bool hostIsPlaying = false;
    double hostPpq = 0.0;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostIsPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
        }

    currentBpm = juce::jlimit (20.0, 400.0, bpm);

    if (! hostIsPlaying && ! internalPlay.load())
    {
        playingStep.store (-1);
        internalPpq = 0.0;
        return;
    }

    const double ppqStart = hostIsPlaying ? hostPpq : internalPpq;
    const double samplesPerBeat = sr * 60.0 / currentBpm;
    const double ppqEnd = ppqStart + numSamples / samplesPerBeat;
    const double gate = 0.25 * 0.9; // a note lasts 90 % of a step (used by Release)

    const auto firstK = (juce::int64) std::floor (ppqStart * 4.0) - 2;
    const auto lastK  = (juce::int64) std::floor (ppqEnd * 4.0);

    for (auto k = firstK; k <= lastK; ++k)
    {
        const int step = (int) (((k % numSteps) + numSteps) % numSteps);
        const int inBeat = step % 4;
        const double tOn = ((double) k + kompaTiming[inBeat]) * 0.25;
        const double tOff = tOn + gate;

        for (int p = 0; p < numPads; ++p)
        {
            const int v = pattern[(size_t) p][(size_t) step].load();
            if (v == 0)
                continue;
            if (tOn >= ppqStart && tOn < ppqEnd)
                events.push_back ({ juce::jlimit (0, numSamples - 1, (int) ((tOn - ppqStart) * samplesPerBeat)), p,
                                    (v == 2 ? 1.0f : 0.78f) * kompaVelocity[inBeat], false });
            if (tOff >= ppqStart && tOff < ppqEnd)
                events.push_back ({ juce::jlimit (0, numSamples - 1, (int) ((tOff - ppqStart) * samplesPerBeat)), p, 0.0f, true });
        }
    }

    playingStep.store ((int) (((juce::int64) std::floor (ppqStart * 4.0) % numSteps + numSteps) % numSteps));
    if (! hostIsPlaying)
        internalPpq = ppqEnd;
}

void KervProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    events.clear();

    for (int p = 0; p < numPads; ++p)
    {
        const float v = pendingTriggers[(size_t) p].exchange (0.0f);
        if (v > 0.0f) events.push_back ({ 0, p, v, false });
        if (pendingReleases[(size_t) p].exchange (false)) events.push_back ({ 0, p, 0.0f, true });
    }

    if (auto s = std::atomic_exchange (&previewSample, SamplePtr()))
    {
        for (auto& v : voices)
            if (v.active && v.pad == numPads && v.fadeRemaining == 0)
                v.fadeRemaining = fadeLength;
        startOneVoice (numPads, std::move (s), 0.9f, 0.0f, 0);
    }

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pad = msg.getNoteNumber() - firstPadNote;
        const int pos = juce::jlimit (0, n - 1, meta.samplePosition);
        if (msg.isNoteOn() && pad >= 0 && pad < numPads)
            events.push_back ({ pos, pad, msg.getFloatVelocity(), false });
        else if (msg.isNoteOff() && pad >= 0 && pad < numPads)
            events.push_back ({ pos, pad, 0.0f, true });
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            for (auto& v : voices)
                if (v.active && v.fadeRemaining == 0) v.fadeRemaining = fadeLength;
    }

    collectSequencerEvents (n);
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.pos < b.pos; });
    masterGain.setTargetValue (masterP->load());

    size_t e = 0;
    for (chunkStart = 0; chunkStart < n; chunkStart += chunk)
    {
        const int chunkEnd = juce::jmin (n, chunkStart + chunk);
        padBus.clear();
        previewBus.clear();

        int cursor = chunkStart;
        while (e < events.size() && events[e].pos < chunkEnd)
        {
            const auto& ev = events[e++];
            if (ev.pos > cursor) { renderVoices (cursor, ev.pos); cursor = ev.pos; }
            if (ev.noteOff) releaseVoices (ev.pad);
            else            startVoice (ev.pad, ev.velocity);
        }
        renderVoices (cursor, chunkEnd);
        processPadBuses (buffer, chunkStart, chunkEnd - chunkStart);
    }
    chunkStart = 0;

    // Safe on the audio thread: the library or `retired` always holds another reference.
    for (auto& v : voices)
        if (! v.active && v.data != nullptr)
            v.data.reset();
}

// ---------------------------------------------------------------------------
// Kits / state
std::unique_ptr<juce::XmlElement> KervProcessor::createPresetXml (const juce::String& name) const
{
    auto xml = std::make_unique<juce::XmlElement> ("KERVPRESET");
    xml->setAttribute ("name", name);

    for (int p = 0; p < numPads; ++p)
    {
        auto* e = xml->createNewChildElement ("PAD");
        e->setAttribute ("i", p);
        e->setAttribute ("category", getPadCategory (p));
        if (auto s = getPadSample (p))   e->setAttribute ("sample", s->id);
        if (auto s = getLayerSample (p)) e->setAttribute ("layersample", s->id);

        for (auto* w : { "vol", "pan", "pitch", "attack", "decay", "release", "rev", "dly", "filter",
                         "eqlow", "eqmid", "eqhigh", "comp", "sat", "layer", "blend", "lpitch", "loffset", "mute", "solo" })
            e->setAttribute (w, apvts.getRawParameterValue (pid (p, w))->load());
    }
    return xml;
}

void KervProcessor::applyPresetXml (const juce::XmlElement& xml, bool includeParameters)
{
    presetName = xml.getStringAttribute ("name", "Kit");

    for (auto* e : xml.getChildWithTagNameIterator ("PAD"))
    {
        const int p = e->getIntAttribute ("i", -1);
        if (p < 0 || p >= numPads)
            continue;

        // pads 1-14 have a fixed category; the two random pads take the kit's
        const int fixed = padRoles[(size_t) p].category;
        const int cat = fixed >= 0 ? fixed : juce::jlimit (0, numCategories - 1, e->getIntAttribute ("category", Cat::Percs));
        padCategory[(size_t) p].store (cat);

        auto s = Library::get().find (e->getStringAttribute ("sample"));
        setPadSample (p, s != nullptr && s->category == cat ? s : nullptr);
        assignLayer (p, Library::get().find (e->getStringAttribute ("layersample")));

        if (includeParameters)
        {
            const std::pair<const char*, float> defaults[] = {
                { "vol", 1.0f }, { "pan", 0.0f }, { "pitch", 0.0f }, { "attack", 0.0f }, { "decay", 1.0f }, { "release", 1.0f },
                { "rev", 0.0f }, { "dly", 0.0f }, { "filter", 0.0f }, { "eqlow", 0.0f }, { "eqmid", 0.0f }, { "eqhigh", 0.0f },
                { "comp", 0.0f }, { "sat", 0.0f }, { "layer", 0.0f }, { "blend", 0.5f }, { "lpitch", 0.0f }, { "loffset", 0.0f },
                { "mute", 0.0f }, { "solo", 0.0f } };
            for (const auto& [w, def] : defaults)
                setParam (apvts, pid (p, w), (float) e->getDoubleAttribute (w, def));
        }
    }
}

void KervProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("KERVSTATE");
    root.setAttribute ("version", "1.0");
    root.addChildElement (apvts.copyState().createXml().release());
    root.addChildElement (createPresetXml (presetName).release());

    auto* seq = root.createNewChildElement ("PATTERN");
    for (int p = 0; p < numPads; ++p)
    {
        juce::String steps;
        for (int s = 0; s < numSteps; ++s)
        {
            const int v = getStep (p, s);
            steps << (v == 2 ? "X" : v == 1 ? "x" : ".");
        }
        seq->setAttribute ("p" + juce::String (p), steps);
    }
    copyXmlToBinary (root, destData);
}

void KervProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName ("KERVSTATE"))
        return;

    if (auto* params = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    if (auto* kit = xml->getChildByName ("KERVPRESET"))
        applyPresetXml (*kit, false);

    if (auto* seq = xml->getChildByName ("PATTERN"))
        for (int p = 0; p < numPads; ++p)
        {
            const auto steps = seq->getStringAttribute ("p" + juce::String (p));
            for (int s = 0; s < numSteps; ++s)
            {
                const auto c = s < steps.length() ? steps[s] : '.';
                setStep (p, s, c == 'X' ? 2 : c == 'x' ? 1 : 0);
            }
        }
}

juce::AudioProcessorEditor* KervProcessor::createEditor() { return new KervEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KervProcessor(); }
