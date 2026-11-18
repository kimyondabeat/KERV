#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    // The active palette (changed from Settings > Interface colour).
    inline juce::Colour bg       { 0xff101014 };
    inline juce::Colour panel    { 0xff191921 };
    inline juce::Colour panel2   { 0xff22222c };
    inline juce::Colour line     { 0xff2c2c38 };
    inline juce::Colour primary  { 0xff8b5cf6 };
    inline juce::Colour accent   { 0xffc4a5ff };
    inline juce::Colour text     { 0xffffffff };
    inline juce::Colour dim      { 0xff9a98ad };
    inline juce::Colour faint    { 0xff5d5b6e };
    inline juce::Colour onPrimary{ 0xffffffff };
    inline bool light = false;
    const juce::Colour star     { 0xffffc94d };

    struct Palette { const char* name; juce::uint32 bg, panel, panel2, line, primary, accent, text, dim, faint; bool light; };
    constexpr int numPalettes = 4;
    inline const Palette palettes[numPalettes] = {
        { "KERV Purple",    0xff101014, 0xff191921, 0xff22222c, 0xff2c2c38, 0xff8b5cf6, 0xffc4a5ff, 0xffffffff, 0xff9a98ad, 0xff5d5b6e, false },
        { "Black & Green",  0xff0b0f0c, 0xff141a16, 0xff1c241e, 0xff27322a, 0xff22c55e, 0xff86efac, 0xffffffff, 0xff93a39a, 0xff56635b, false },
        { "Grey & Orange",  0xff1c1c1f, 0xff27272b, 0xff313136, 0xff404047, 0xfff97316, 0xfffdba74, 0xffffffff, 0xffa1a1aa, 0xff6b6b73, false },
        { "Cream & Cherry", 0xffece4d6, 0xfff7f1e6, 0xffe9e0d1, 0xffd6cab7, 0xffb5122e, 0xffd94a62, 0xff1f1b18, 0xff6d645a, 0xffa3998c, true },
    };

    inline void apply (int index)
    {
        const auto& p = palettes[juce::jlimit (0, numPalettes - 1, index)];
        bg = juce::Colour (p.bg); panel = juce::Colour (p.panel); panel2 = juce::Colour (p.panel2); line = juce::Colour (p.line);
        primary = juce::Colour (p.primary); accent = juce::Colour (p.accent); text = juce::Colour (p.text);
        dim = juce::Colour (p.dim); faint = juce::Colour (p.faint); light = p.light;
        onPrimary = juce::Colours::white;
    }

    inline void drawStar (juce::Graphics& g, juce::Rectangle<float> r, bool filled, juce::Colour c)
    {
        juce::Path p;
        p.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0.0f);
        g.setColour (c);
        if (filled) g.fillPath (p);
        else g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    inline juce::Font font (float h, bool bold = false) { return juce::Font (h, bold ? juce::Font::bold : juce::Font::plain); }
}

class KervLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KervLookAndFeel() { refreshColours(); }

    void refreshColours()
    {
        using namespace Theme;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::TextButton::buttonColourId, panel2);
        setColour (juce::TextButton::buttonOnColourId, primary);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, onPrimary);
        setColour (juce::ComboBox::backgroundColourId, panel2);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, primary);
        setColour (juce::PopupMenu::highlightedTextColourId, onPrimary);
        setColour (juce::TextEditor::backgroundColourId, panel2);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, line);
        setColour (juce::TextEditor::focusedOutlineColourId, primary);
        setColour (juce::TextEditor::highlightColourId, primary.withAlpha (0.5f));
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::Label::textColourId, dim);
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, primary.withAlpha (0.6f));
        setColour (juce::BubbleComponent::backgroundColourId, panel);
        setColour (juce::BubbleComponent::outlineColourId, primary);
        setColour (juce::TooltipWindow::backgroundColourId, panel);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, primary);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::AlertWindow::outlineColourId, primary);
        setColour (juce::Slider::textBoxTextColourId, text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace Theme;
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float track = juce::jmax (2.5f, radius * 0.14f);
        const float r = radius - track * 0.5f;

        juce::Path bgArc;
        bgArc.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
        g.setColour (line);
        g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setGradientFill (juce::ColourGradient (primary, c.x - radius, c.y + radius, accent, c.x + radius, c.y - radius, false));
        g.strokePath (arc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float inner = r - track * 1.3f;
        g.setColour (panel2);
        g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);
        g.setColour (line.brighter (0.2f));
        g.drawEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2, 1.0f);

        juce::Path dot;
        dot.addEllipse (-2.0f, -inner + 3.0f, 4.0f, 4.0f);
        g.setColour (accent);
        g.fillPath (dot, juce::AffineTransform::rotation (angle).translated (c));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical)
            return LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);

        using namespace Theme;
        const float cx = x + w * 0.5f;
        const auto track = juce::Rectangle<float> (cx - 3.0f, (float) y, 6.0f, (float) h);
        g.setColour (line);
        g.fillRoundedRectangle (track, 3.0f);
        g.setGradientFill (juce::ColourGradient (accent, cx, pos, primary, cx, (float) (y + h), false));
        g.fillRoundedRectangle (track.withTop (pos), 3.0f);
        g.setColour (text);
        g.fillRoundedRectangle (cx - 11.0f, pos - 4.0f, 22.0f, 8.0f, 3.0f);
        g.setColour (primary);
        g.fillRect (cx - 7.0f, pos - 0.75f, 14.0f, 1.5f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down) override
    {
        using namespace Theme;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = b.getToggleState() ? primary : base;
        if (down) c = primary.brighter (0.1f);
        else if (over) c = c.brighter (0.08f);
        if (! b.isEnabled()) c = c.withAlpha (0.4f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 6.0f);
        if (! b.getToggleState())
        {
            g.setColour (line);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Theme::font (juce::jmin (13.0f, h * 0.55f), true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Theme::font (14.0f, true); }
    juce::Font getPopupMenuFont() override { return Theme::font (14.0f); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        using namespace Theme;
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (panel2);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 18.0f, ay = h * 0.5f;
        arrow.addTriangle (ax - 5.0f, ay - 2.5f, ax + 5.0f, ay - 2.5f, ax, ay + 3.5f);
        g.setColour (accent);
        g.fillPath (arrow);
    }
};
