#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <memory>
#include <vector>
#include "Library.h"

constexpr int numPads = 16;
constexpr int numSteps = 16;
constexpr int firstPadNote = 36; // pad 1 = MIDI note 36

// ---------------------------------------------------------------------------
// The 16 pads. Every pad belongs to one category and only takes sounds from it.
// Pads 15 and 16 are "random" pads: each kit decides their category.
struct PadRole
{
    const char* label;
    int category; // -1 = set by the kit
};

constexpr std::array<PadRole, numPads> padRoles { {
    { "Kick",      Cat::Kick },   { "Snare",     Cat::Snare },  { "Rim",       Cat::Rim },   { "Clap",   Cat::Clap },
    { "Hihat",     Cat::Hihat },  { "Cymbal",    Cat::Cymbal }, { "Crash",     Cat::Crash }, { "Conga",  Cat::Conga },
    { "Cowbell",   Cat::Cowbell },{ "Percs",     Cat::Percs },  { "Tom",       Cat::Tom },   { "FX",     Cat::FX },
    { "Conga 2",   Cat::Conga },  { "Cymb Open", Cat::Cymbal }, { "Random 1",  -1 },         { "Random 2", -1 },
} };

// One Kompa feel for the sequencer (timing in 16ths and velocity, per 16th inside a beat).
constexpr float kompaTiming[4]   = { 0.0f, 0.04f, 0.0f, 0.07f };
constexpr float kompaVelocity[4] = { 1.0f, 0.86f, 0.95f, 0.90f };

// Default pattern: a Kompa groove in the style of "Herard 86" ('.' off, 'x' normal, 'X' accent).
constexpr std::array<const char*, numPads> defaultPattern { {
    "X...X...X...X...",   // 1 kick
    "......x.........",   // 2 snare
    "....X.......X.x.",   // 3 rim
    "................",   // 4 clap
    "................",   // 5 hihat
    "X..X..X.X..X..XX",   // 6 cymbal
    "................",   // 7 crash
    "...x...........x",   // 8 conga
    "X..x....X..x....",   // 9 cowbell
    "................",   // 10 percs
    "..x.......x.....",   // 11 tom
    "................",   // 12 fx
    ".x..............",   // 13 conga 2
    "................",   // 14 cymb open
    "................",   // 15 random 1
    "................",   // 16 random 2
} };

// ---------------------------------------------------------------------------
// Kits (presets) are XML:
// <KERVPRESET name><PAD i category sample layer vol pan pitch .../></KERVPRESET>
namespace Presets
{
    std::vector<std::unique_ptr<juce::XmlElement>> factory();
    juce::File userFolder();
    juce::Array<juce::File> userPresets();
    constexpr const char* fileExtension = ".kervpreset";
}
