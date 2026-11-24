#include "DebugLog.h"
#include "Library.h"
#include "BinaryData.h"

namespace
{
    constexpr double maxSeconds = 12.0;
    constexpr int numPeaks = 300;

    std::vector<float> computePeaks (const juce::AudioBuffer<float>& b)
    {
        std::vector<float> p ((size_t) numPeaks, 0.0f);
        const int n = b.getNumSamples();
        if (n == 0)
            return p;

        for (int i = 0; i < numPeaks; ++i)
        {
            const int s = (int) ((juce::int64) i * n / numPeaks);
            const int e = juce::jmax (s + 1, (int) ((juce::int64) (i + 1) * n / numPeaks));
            float m = 0.0f;
            for (int c = 0; c < b.getNumChannels(); ++c)
                m = juce::jmax (m, b.getMagnitude (c, s, e - s));
            p[(size_t) i] = m;
        }

        const float top = *std::max_element (p.begin(), p.end());
        if (top > 0.0f)
            for (auto& v : p) v /= top;
        return p;
    }
}

int Library::chokeClassFor (int category, const juce::String& name)
{
    if (category < 0)
        return 0;

    const juce::String key (categories[(size_t) category].key);
    const auto lower = name.toLowerCase();

    if (key == "hihat")
        return (lower.startsWith ("op") || lower.contains ("open")) ? 2 : 1;
    if (key == "cymbal")
        return (lower.startsWith ("cymb op") || lower.contains ("open")) ? 4 : 3;
    return 0;
}

SamplePtr Library::decode (juce::AudioFormatManager& fm, std::unique_ptr<juce::InputStream> stream,
                           const juce::String& id, const juce::String& name, int category)
{
    if (stream == nullptr)
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::move (stream)));
    if (reader == nullptr || reader->lengthInSamples <= 1)
        return nullptr;

    const int length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * maxSeconds));
    const int channels = (int) juce::jlimit (1u, 2u, reader->numChannels);

    auto s = std::make_shared<SampleData>();
    s->buffer.setSize (channels, length);
    reader->read (&s->buffer, 0, length, 0, true, channels > 1);
    s->sampleRate = reader->sampleRate;
    s->id = id;
    s->name = name;
    s->category = category;
    s->chokeClass = chokeClassFor (category, name);
    s->peaks = computePeaks (s->buffer);
    return s;
}

const Library& Library::get()
{
    static const Library lib;
    return lib;
}

Library::Library()
{
    KERV_LOG ("Library: decoding sounds");
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    struct Entry { juce::String file; int resource; int category; };
    std::vector<Entry> entries;

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String file (BinaryData::originalFilenames[i]);
        const auto key = file.upToFirstOccurrenceOf ("__", false, false);
        for (int c = 0; c < numCategories; ++c)
            if (key == categories[(size_t) c].key)
                entries.push_back ({ file, i, c });
    }

    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) { return a.file < b.file; });

    for (const auto& e : entries)
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[e.resource], size);
        if (data == nullptr || size <= 0)
            continue;

        const auto name = e.file.fromLastOccurrenceOf ("__", false, false).upToLastOccurrenceOf (".", false, false);
        const auto id = juce::String (categories[(size_t) e.category].key) + "/" + name;

        if (auto s = decode (fm, std::make_unique<juce::MemoryInputStream> (data, (size_t) size, false), id, name, e.category))
            samples.push_back (std::move (s));
    }
}

SamplePtr Library::find (const juce::String& id) const
{
    for (const auto& s : samples)
        if (s->id == id)
            return s;
    return nullptr;
}

std::vector<SamplePtr> Library::inCategory (int category) const
{
    std::vector<SamplePtr> out;
    for (const auto& s : samples)
        if (s->category == category)
            out.push_back (s);
    return out;
}
