#include "Presets.h"

namespace
{
    struct Slot { int category; const char* sample; float vol = 1.0f, pan = 0.0f; };

    struct Kit
    {
        const char* name;
        std::array<Slot, numPads> pads;
    };

    std::unique_ptr<juce::XmlElement> toXml (const Kit& k)
    {
        auto xml = std::make_unique<juce::XmlElement> ("KERVPRESET");
        xml->setAttribute ("name", k.name);
        for (int i = 0; i < numPads; ++i)
        {
            const auto& s = k.pads[(size_t) i];
            auto* e = xml->createNewChildElement ("PAD");
            e->setAttribute ("i", i);
            e->setAttribute ("category", s.category);
            if (juce::String (s.sample).isNotEmpty())
                e->setAttribute ("sample", juce::String (categories[(size_t) s.category].key) + "/" + s.sample);
            e->setAttribute ("vol", s.vol);
            e->setAttribute ("pan", s.pan);
        }
        return xml;
    }
}

std::vector<std::unique_ptr<juce::XmlElement>> Presets::factory()
{
    using namespace Cat;
    std::vector<std::unique_ptr<juce::XmlElement>> out;

    // The artist's kits (pads 13-16: second conga, open cymbal, two random sounds).
    out.push_back (toXml ({ "Classic Kompa", { {
        { Kick, "Kick (Clean)" }, { Snare, "Snare (Dirty)" }, { Rim, "Rim (Secousse)" }, { Clap, "Clap (ZingZing)" },
        { Hihat, "HiHat (ChikChik)" }, { Cymbal, "Cymb (Sizzle)" }, { Crash, "Crash (Zepon)" }, { Conga, "Conga (Bembe)" },
        { Cowbell, "Cowbell (Feray)" }, { Percs, "Percs (Flick)" }, { Tom, "Tom (Low)" }, { FX, "FX (Impact)" },
        { Conga, "Conga (Fuego)" }, { Cymbal, "Cymb Op (Hiss)" }, { Cowbell, "Cowbell (Cling)" }, { Tom, "Zouk Tom (Chaleur)" } } } }));

    out.push_back (toXml ({ "Modern Kompa", { {
        { Kick, "Kick (Dark)" }, { Snare, "Snare (Vintage)" }, { Rim, "Rim (StickUp)" }, { Clap, "Clap (Souflet)" },
        { Hihat, "HiHat (SizzlePop)" }, { Cymbal, "Cymb (Stood)" }, { Crash, "Crash (Wildfire)" }, { Conga, "Conga (Tambourin)" },
        { Cowbell, "Cowbell (Cling)" }, { Percs, "Percs (ZigZag)" }, { Tom, "Zouk Tom (Chaleur)" }, { FX, "FX (Pulsewave)" },
        { Conga, "Siye (Koupe)" }, { Cymbal, "Cymb Op (Airy)" }, { Snare, "Snare (Tranchant)" }, { Clap, "Clap (Popper)" } } } }));

    out.push_back (toXml ({ "Afro Kompa", { {
        { Kick, "Kick (Warm)" }, { Snare, "Snare (Boule)" }, { Rim, "Rim (Plumen)" }, { Clap, "Clap (Popper)" },
        { Hihat, "HiHat (VitVit)" }, { Cymbal, "Cymb (Van)" }, { Crash, "Crash (Stormy)" }, { Conga, "Conga (Ritual)" },
        { Cowbell, "Cowbell (Tingala)" }, { Percs, "Percs (Krak)" }, { Tom, "Tom (Bright)" }, { FX, "FX (Clap)" },
        { Conga, "Conga (Pilon)" }, { Cymbal, "Cymb Op (Bloom)" }, { Snare, "Snare (Pike)" }, { Cowbell, "Cowbell (Klakatak)" } } } }));

    out.push_back (toXml ({ "Zouk Love", { {
        { Kick, "Kick (Deep)" }, { Snare, "Snare (Claque)" }, { Rim, "Rim (Woodshot)" }, { Clap, "Clap (KoutMen)" },
        { Hihat, "HiHat (Fofile)" }, { Cymbal, "Cymb (Crashy)" }, { Crash, "Crash (Blaze)" }, { Conga, "Conga (Pilon)" },
        { Cowbell, "" }, { Percs, "Percs (Claqueur)" }, { Tom, "Zouk Tom (Rasin)" }, { FX, "FX (Cap)" },
        { Conga, "Conga (Bembe)" }, { Cymbal, "Cymb Op (Rush)" }, { FX, "FX (Clap)" }, { FX, "FX (Ye)" } } } }));

    out.push_back (toXml ({ "kimyondabeat", { {
        { Kick, "Kick (Hard)" }, { Snare, "Snare (Sek)" }, { Rim, "" }, { Clap, "Clap (Souflet)" },
        { Hihat, "Op Hat (Breeze)" }, { Cymbal, "Cymb (Sizzle)" }, { Crash, "Crash (Thunder)" }, { Conga, "Siye (Raspa)" },
        { Cowbell, "Cowbell (TicTac)" }, { Percs, "Percs (Zestful)" }, { Tom, "Zouk Tom (Rasin)" }, { FX, "FX (Glitch)" },
        { Conga, "Conga (Ritual)" }, { Cymbal, "Cymb Op (Bloom)" }, { FX, "FX (Crash)" }, { Tom, "Zouk Tom (Chaleur)" } } } }));

    return out;
}

juce::File Presets::userFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KERV").getChildFile ("Kits");
    dir.createDirectory();
    return dir;
}

juce::Array<juce::File> Presets::userPresets()
{
    auto files = userFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    files.sort();
    return files;
}
