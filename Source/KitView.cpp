#include "PluginEditor.h"

using namespace Theme;

namespace
{
    // Positions come from the kit layout sketch (panel of 849 x 642 px): centre, size, shape, pads it plays.
    // pads[0] is played by a left click, pads[1] by a right click (open cymbal, conga 2).
    const KitView::Piece pieces[12] = {
        { "CRASH",   277, 134, 192,  50, KitView::cymbalShape,  {  6, -1 } },
        { "CYMBAL",  698, 135, 206,  52, KitView::cymbalShape,  {  5, 13 } },
        { "HIHAT",   115, 245, 154,  40, KitView::cymbalShape,  {  4, -1 } },
        { "TOM",     463, 222, 114, 114, KitView::drumShape,    { 10, -1 } },
        { "COWBELL", 608, 254,  80,  60, KitView::cowbellShape, {  8, -1 } },
        { "CONGA",   732, 315, 108, 156, KitView::congaShape,   {  7, 12 } },
        { "FX",       97, 370,  66,  66, KitView::padShape,     { 11, -1 } },
        { "SNARE",   264, 363, 134, 134, KitView::drumShape,    {  1, -1 } },
        { "KICK",    459, 440, 182, 182, KitView::drumShape,    {  0, -1 } },
        { "CLAP",    622, 402,  90,  90, KitView::drumShape,    {  3, -1 } },
        { "RIM",      97, 491,  66,  66, KitView::padShape,     {  2, -1 } },
        { "PERCS",   726, 491,  58,  52, KitView::triangleShape, {  9, -1 } },
    };
    // the part of the sketch that holds the kit (labels included)
    constexpr float sketchX = 25.0f, sketchY = 92.0f, sketchW = 800.0f, sketchH = 510.0f;

    // Kit colours from the original sketch: dark drums, brass cymbals, orange when a piece is hit.
    #define hitColour Theme::primary
    #define hitBright Theme::accent
    // drums stay dark in every colour scheme, like a real kit
    const juce::Colour drumFill    { 0xff23232b };
    const juce::Colour drumRim     { 0xff464652 };
    const juce::Colour padFill     { 0xff2b2b33 };
    const juce::Colour padEdge     { 0xff3d3d48 };

    juce::Path shapePath (const KitView::Piece& p, juce::Rectangle<float> r)
    {
        juce::Path path;
        switch (p.shape)
        {
            case KitView::padShape:     path.addRoundedRectangle (r, r.getWidth() * 0.2f); break;
            case KitView::cowbellShape:
            {
                const float inset = r.getWidth() * 0.18f;
                path.startNewSubPath (r.getX() + inset, r.getY());
                path.lineTo (r.getRight() - inset, r.getY());
                path.lineTo (r.getRight(), r.getBottom());
                path.lineTo (r.getX(), r.getBottom());
                path.closeSubPath();
                path = path.createPathWithRoundedCorners (4.0f);
                break;
            }
            case KitView::triangleShape:
            {
                // a percussion triangle: open corner at the top right, with its beater
                const float t = juce::jmax (3.0f, r.getWidth() * 0.07f);
                juce::Path tri;
                tri.startNewSubPath (r.getCentreX() + t * 0.6f, r.getY() + t);
                tri.lineTo (r.getRight() - t * 0.5f, r.getBottom() - t * 0.5f);
                tri.lineTo (r.getX() + t * 0.5f, r.getBottom() - t * 0.5f);
                tri.lineTo (r.getCentreX() - t * 0.4f, r.getY() + t * 1.6f);
                juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (path, tri);
                break;
            }
            case KitView::cymbalShape:
            case KitView::drumShape:
            case KitView::congaShape:
            default: path.addEllipse (r); break;
        }
        return path;
    }
}

KitView::KitView (MainView& m) : main (m)
{
    for (int p = 0; p < numPads; ++p)
        lastHits[(size_t) p] = m.processor.getHitCount (p);
}

juce::Rectangle<float> KitView::boundsOf (int i) const
{
    auto area = getLocalBounds().toFloat().reduced (24.0f, 40.0f);
    const float k = juce::jmin (area.getWidth() / sketchW, area.getHeight() / sketchH);
    const float ox = area.getCentreX() - (sketchX + sketchW * 0.5f) * k;
    const float oy = area.getCentreY() - (sketchY + sketchH * 0.5f) * k;
    const auto& p = pieces[i];
    return { ox + (p.cx - p.w * 0.5f) * k, oy + (p.cy - p.h * 0.5f) * k, p.w * k, p.h * k };
}

int KitView::pieceAt (juce::Point<float> pt) const
{
    // smallest piece first so the kick doesn't swallow its neighbours
    int best = -1;
    float bestArea = 1.0e9f;
    for (int i = 0; i < 12; ++i)
    {
        const auto r = boundsOf (i);
        const bool inside = pieces[i].shape == triangleShape ? r.expanded (6.0f).contains (pt)
                                                             : shapePath (pieces[i], r.expanded (4.0f)).contains (pt);
        if (inside && r.getWidth() * r.getHeight() < bestArea)
        {
            best = i;
            bestArea = r.getWidth() * r.getHeight();
        }
    }
    return best;
}

void KitView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 18.0f);
    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 18.0f, 1.0f);

    g.setColour (dim);
    g.setFont (font (12.0f, true));
    g.drawText ("K I T", 26, 18, 100, 16, juce::Justification::left);

    for (int i = 0; i < 12; ++i)
    {
        const auto& p = pieces[i];
        const auto r = boundsOf (i);
        const auto path = shapePath (p, r);
        const float gl = glow[(size_t) i];
        const bool selected = main.selectedPad == p.pads[0] || main.selectedPad == p.pads[1];
        const bool hot = hover == i;

        if (gl > 0.01f) // cheap halo: a few soft strokes instead of a blurred shadow
            for (int ring = 4; ring >= 1; --ring)
            {
                g.setColour (hitColour.withAlpha (0.11f * gl * (5 - ring)));
                g.strokePath (path, juce::PathStrokeType ((float) ring * 7.0f * gl));
            }

        if (p.shape == cymbalShape)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a7a58), r.getX(), r.getY(),
                                                     juce::Colour (0xff5c5040), r.getRight(), r.getBottom(), false));
            g.fillPath (path);
            g.setColour (juce::Colour (0xffa8966c).withAlpha (0.8f));
            g.strokePath (path, juce::PathStrokeType (1.5f));
            // grooves + bell
            g.setColour (juce::Colour (0x22000000));
            g.drawEllipse (r.reduced (r.getWidth() * 0.18f, r.getHeight() * 0.18f), 1.0f);
            g.setColour (juce::Colour (0xff9c8a62));
            g.fillEllipse (r.withSizeKeepingCentre (r.getWidth() * 0.16f, r.getHeight() * 0.32f));
        }
        else if (p.shape == triangleShape)
        {
            g.setColour (juce::Colour (0xff8f8f9c));
            g.fillPath (path);
            g.setColour (juce::Colour (0xff5d5d68));
            g.drawLine (r.getRight() - r.getWidth() * 0.05f, r.getY() + r.getHeight() * 0.05f,
                        r.getCentreX() + r.getWidth() * 0.05f, r.getCentreY() + r.getHeight() * 0.1f, 2.0f); // beater
        }
        else
        {
            g.setColour (p.shape == drumShape || p.shape == congaShape ? drumFill : padFill);
            g.fillPath (path);
            const float rimW = p.shape == drumShape ? juce::jmax (3.0f, r.getWidth() * 0.035f) : 2.0f;
            g.setColour (p.shape == padShape || p.shape == cowbellShape ? padEdge : drumRim);
            g.strokePath (path, juce::PathStrokeType (rimW));
            if (p.shape == drumShape)
            {
                g.setColour (juce::Colour (0x10ffffff));
                g.fillEllipse (r.reduced (r.getWidth() * 0.12f));
            }
        }

        if (hot)
        {
            g.setColour (hitColour.withAlpha (0.12f));
            g.fillPath (path);
        }

        if (gl > 0.01f)
        {
            g.setColour (hitColour.withAlpha (0.95f * gl));
            g.fillPath (path);
            g.setColour (hitBright.withAlpha (gl));
            g.strokePath (path, juce::PathStrokeType (2.0f));
        }

        // label: NAME note, then the sound loaded on the pad
        auto label = juce::Rectangle<float> (r.getCentreX() - 110.0f, r.getBottom() + 8.0f, 220.0f, 18.0f);
        g.setColour (gl > 0.3f || selected ? hitBright : text);
        g.setFont (font (15.0f, true));
        g.drawText (juce::String (p.name), label, juce::Justification::centred);

        const auto s = main.processor.getPadSample (p.pads[0]);
        g.setColour (dim.withAlpha (0.8f));
        g.setFont (font (11.0f));
        g.drawText (s != nullptr ? s->name : juce::String ("Empty - click +"), label.translated (0.0f, 18.0f).toNearestInt(),
                    juce::Justification::centred);

        // [+] : choose a sound for this instrument
        const auto plus = plusOf (i);
        g.setColour (hoverPlus == i ? primary : panel2);
        g.fillEllipse (plus);
        g.setColour (hoverPlus == i ? onPrimary : dim);
        g.drawLine (plus.getCentreX() - 4.5f, plus.getCentreY(), plus.getCentreX() + 4.5f, plus.getCentreY(), 1.8f);
        g.drawLine (plus.getCentreX(), plus.getCentreY() - 4.5f, plus.getCentreX(), plus.getCentreY() + 4.5f, 1.8f);
    }
}

void KitView::mouseDown (const juce::MouseEvent& e)
{
    if (const int plus = plusAt (e.position); plus >= 0)
    {
        main.openPadInLibrary (pieces[plus].pads[0]); // [+] opens the pad's category in the library
        return;
    }

    const int i = pieceAt (e.position);
    if (i < 0)
        return;

    const auto& p = pieces[i];
    heldPad = (e.mods.isPopupMenu() && p.pads[1] >= 0) ? p.pads[1] : p.pads[0];
    main.processor.triggerPad (heldPad, 0.95f);
    main.selectPad (heldPad);
    repaint();
}

void KitView::mouseUp (const juce::MouseEvent&)
{
    if (heldPad >= 0)
        main.processor.releasePad (heldPad);
    heldPad = -1;
}

void KitView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = pieceAt (e.position);
    if (i >= 0 && plusAt (e.position) < 0)
        main.openPadInLibrary (main.selectedPad); // double-click: choose this pad's sound in its category
}

void KitView::mouseMove (const juce::MouseEvent& e)
{
    const int plus = plusAt (e.position);
    const int i = plus >= 0 ? -1 : pieceAt (e.position);
    if (i != hover || plus != hoverPlus)
    {
        hover = i;
        hoverPlus = plus;
        setMouseCursor (i >= 0 || plus >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

juce::Rectangle<float> KitView::plusOf (int i) const
{
    const auto r = boundsOf (i);
    const float nameW = font (15.0f, true).getStringWidthFloat (pieces[i].name);
    return juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ r.getCentreX() + nameW * 0.5f + 16.0f, r.getBottom() + 17.0f });
}

int KitView::plusAt (juce::Point<float> pt) const
{
    for (int i = 0; i < 12; ++i)
        if (plusOf (i).expanded (3.0f).contains (pt))
            return i;
    return -1;
}

void KitView::refresh()
{
    // Only repaint the pieces that change, so the kit stays light on the CPU while a song plays.
    for (int i = 0; i < 12; ++i)
    {
        auto& gl = glow[(size_t) i];
        bool dirty = false;
        for (int pad : pieces[i].pads)
        {
            if (pad < 0) continue;
            const auto c = main.processor.getHitCount (pad);
            if (c != lastHits[(size_t) pad]) { lastHits[(size_t) pad] = c; gl = 1.0f; dirty = true; }
        }
        if (gl > 0.0f) { gl = juce::jmax (0.0f, gl - 0.075f); dirty = true; }
        if (dirty)
        {
            const auto r = boundsOf (i);
            repaint (r.expanded (34.0f).getUnion ({ r.getCentreX() - 112.0f, r.getBottom(), 224.0f, 48.0f }).getSmallestIntegerContainer());
        }
    }
}
