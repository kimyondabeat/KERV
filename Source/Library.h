#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// One sound. Never changed after creation, so UI and audio threads can share it.
struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    juce::String id;          // "kick/Kick (Hard)" for built-in sounds, full path for user files
    juce::String name;        // "Kick (Hard)"
    int category = -1;        // index in categories, -1 for user files
    int chokeClass = 0;       // 1 closed hat, 2 open hat, 3 cymbal, 4 open cymbal
    std::vector<float> peaks; // waveform overview, 0..1
};

using SamplePtr = std::shared_ptr<const SampleData>;

struct Category
{
    const char* key;
    const char* label;
};

constexpr int numCategories = 12;
constexpr std::array<Category, numCategories> categories { {
    { "conga",   "Conga" },
    { "cowbell", "Cowbell" },
    { "crash",   "Crash" },
    { "cymbal",  "Cymbal" },
    { "fx",      "FX" },
    { "hihat",   "Hihat" },
    { "kick",    "Kick" },
    { "percs",   "Percs" },
    { "rim",     "Rim" },
    { "clap",    "Clap" },
    { "snare",   "Snare" },
    { "tom",     "Tom" },
} };

namespace Cat { enum { Conga, Cowbell, Crash, Cymbal, FX, Hihat, Kick, Percs, Rim, Clap, Snare, Tom }; }

// The sounds built into KERV.
class Library
{
public:
    static const Library& get();

    const std::vector<SamplePtr>& all() const { return samples; }
    SamplePtr find (const juce::String& id) const;
    std::vector<SamplePtr> inCategory (int category) const;

    // Shared helpers
    static SamplePtr decode (juce::AudioFormatManager&, std::unique_ptr<juce::InputStream>,
                             const juce::String& id, const juce::String& name, int category);
    static int chokeClassFor (int category, const juce::String& name);

private:
    Library();
    std::vector<SamplePtr> samples;
};
