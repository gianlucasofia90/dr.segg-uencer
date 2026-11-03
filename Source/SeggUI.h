#pragma once
// DR. SEGG-UENCER UI parts. SONIC GRAIN design language (glass panels, slanted black plates, dark knobs with a
// yellow value arc, pixel-font readouts) with the Egg Lab ground: lab red, a diagonal slab, hazard stripes.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SeggProcessor.h"
#include "SeggFonts.h"

namespace segg
{
namespace c
{
    // Egg Lab: only the ground layer differs from SONIC GRAIN, every other name keeps its value
    const juce::Colour ground    { 0xffc8102e };   // lab red, white on it 5.9:1
    const juce::Colour groundAlt { 0xff7d0a1c };   // header strip
    const juce::Colour slab      { 0xffa00d25 };   // diagonal band behind the right side
    const juce::Colour modGround { 0xff17191d };   // track deck
    const juce::Colour glass     { 0xf5f6f7fa };   // rgba(246, 247, 250, 0.96): almost opaque, no pink cast over red
    const juce::Colour panelShadow { 0x523c000a };
    const juce::Colour glassEdge { 0xe6ffffff };
    const juce::Colour ink       { 0xff0b1430 };
    const juce::Colour inkSoft   { 0xff33415f };
    const juce::Colour plate     { 0xff111111 };
    const juce::Colour onPlate   { 0xffffffff };
    const juce::Colour accent    { 0xffffd21f };
    const juce::Colour onAccent  { 0xff0b1430 };
    const juce::Colour go        { 0xff1f9e1a };
    const juce::Colour signal    { 0xffe90101 };
    const juce::Colour tag       { 0xff0123c0 };
    const juce::Colour orbitA    { 0xffffd21f };   // yellow ring
    const juce::Colour orbitB    { 0xffe8ecf1 };   // chrome ring
    const juce::Colour knob      { 0xff16212f };
    const juce::Colour knobHi    { 0xff3a4b63 };
    const juce::Colour track     { 0x240b1430 };   // ink at 14 %
    const juce::Colour trackDark { 0x24ffffff };   // white at 14 %, for controls on plates and the deck
    const juce::Colour display   { 0xeb082660 };   // rgba(8, 38, 96, 0.92)
    const juce::Colour wave      { 0xffffffff };
    const juce::Colour checker   { 0x21000000 };   // black 13 % on the red ground
    const juce::Colour grid      { 0x2effffff };
}

/** Embedded faces (Rubik, Silkscreen; SIL Open Font License, see Resources/fonts). */
struct Fonts
{
    static Fonts& get() { static Fonts f; return f; }
    juce::Typeface::Ptr medium, bold, black, pixel;
private:
    Fonts()
    {
        medium = juce::Typeface::createSystemTypefaceFor (SeggFonts::RubikMedium_ttf, SeggFonts::RubikMedium_ttfSize);
        bold   = juce::Typeface::createSystemTypefaceFor (SeggFonts::RubikBold_ttf, SeggFonts::RubikBold_ttfSize);
        black  = juce::Typeface::createSystemTypefaceFor (SeggFonts::RubikBlackItalic_ttf, SeggFonts::RubikBlackItalic_ttfSize);
        pixel  = juce::Typeface::createSystemTypefaceFor (SeggFonts::SilkscreenRegular_ttf, SeggFonts::SilkscreenRegular_ttfSize);
    }
};

inline juce::Font face (const juce::Typeface::Ptr& t, float size, float tracking = 0.0f)
{
    return juce::Font (juce::FontOptions (t).withHeight (size)).withExtraKerningFactor (tracking);
}
inline juce::Font medium (float size, float tracking = 0.0f) { return face (Fonts::get().medium, size, tracking); }
inline juce::Font bold (float size, float tracking = 0.0f)   { return face (Fonts::get().bold, size, tracking); }
inline juce::Font black (float size, float tracking = 0.0f)  { return face (Fonts::get().black, size, tracking); }
// Silkscreen draws on a grid of 1/8 em and JUCE sizes a font by ascent + descent (1.28 em): x 1.024 puts every
// font pixel on exactly one (10) or two (20) screen pixels, so the readouts stay sharp instead of smeared.
inline juce::Font pixel (float size)                         { return face (Fonts::get().pixel, size * 1.024f); }

/** A rectangle skewed by -30 degrees: the plate every button, tab and tag is drawn on. */
inline juce::Path platePath (juce::Rectangle<float> r)
{
    const float lean = juce::jmin (r.getHeight() * 0.577f, r.getWidth() * 0.3f);
    juce::Path p;
    p.addQuadrilateral (r.getX() + lean, r.getY(), r.getRight(), r.getY(),
                        r.getRight() - lean, r.getBottom(), r.getX(), r.getBottom());
    return p;
}

/** Plate with the stacked hard shadow (signal, then go) used by section tags and the preset tag. */
inline void taggedPlate (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill)
{
    g.setColour (c::go);     g.fillPath (platePath (r.translated (7, 7)));
    g.setColour (c::signal); g.fillPath (platePath (r.translated (4, 4)));
    g.setColour (fill);      g.fillPath (platePath (r));
}

/** Hazard stripes (accent on plate, 45 degrees): the edge of the header and of the track deck. */
inline void hazard (juce::Graphics& g, juce::Rectangle<float> r, float band = 10.0f)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (r.getSmallestIntegerContainer());
    g.setColour (c::plate);
    g.fillRect (r);
    g.setColour (c::accent);
    const float h = r.getHeight();
    for (float x = r.getX() - h; x < r.getRight(); x += band * 2.0f)
    {
        juce::Path p;
        p.addQuadrilateral (x, r.getBottom(), x + band, r.getBottom(), x + band + h, r.getY(), x + h, r.getY());
        g.fillPath (p);
    }
}

/** Glass panel with its two-line title: kicker word, then the section word on a tagged plate. */
inline void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& kicker, const juce::String& title)
{
    juce::Path body;
    body.addRoundedRectangle (r, 10.0f);
    juce::DropShadow (c::panelShadow, 28, { 0, 12 }).drawForPath (g, body);
    g.setColour (c::glass);
    g.fillPath (body);
    g.setColour (c::glassEdge);
    g.strokePath (body, juce::PathStrokeType (1.0f));

    g.setColour (c::inkSoft);
    g.setFont (medium (10.0f, 0.3f));
    g.drawText (kicker.toUpperCase(), (int) r.getX() + 16, (int) r.getY() + 8, 200, 12, juce::Justification::left);
    const auto f = bold (13.0f, 0.22f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, title.toUpperCase()) + 36.0f;
    auto tr = juce::Rectangle<float> (r.getX() + 18, r.getY() + 22, tw, 22);
    taggedPlate (g, tr, c::plate);
    g.setColour (c::onPlate);
    g.setFont (f);
    g.drawText (title.toUpperCase(), tr.reduced (14, 0), juce::Justification::centred);
}

/** Controls on a dark ground (header plates, modulation deck) carry this property. */
inline bool isOnDark (const juce::Component& comp)
{
    for (auto* p = &comp; p != nullptr; p = p->getParentComponent())
        if (p->getProperties()["sgDark"]) return true;
    return false;
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr int kValueH = 13, kCaptionH = 15;   // knob readout and caption rows

    LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, c::ink);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, c::accent);
        setColour (juce::Label::textColourId, c::ink);
        setColour (juce::ComboBox::textColourId, c::ink);
        setColour (juce::ComboBox::arrowColourId, c::ink);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xfff6f7fa));
        setColour (juce::PopupMenu::textColourId, c::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c::plate);
        setColour (juce::PopupMenu::highlightedTextColourId, c::onPlate);
        setColour (juce::TextButton::buttonColourId, c::plate);
        setColour (juce::TextButton::buttonOnColourId, c::accent);
        setColour (juce::TextButton::textColourOffId, c::onPlate);
        setColour (juce::TextButton::textColourOnId, c::onAccent);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xfff6f7fa));
        setColour (juce::AlertWindow::textColourId, c::ink);
        setColour (juce::AlertWindow::outlineColourId, c::plate);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, c::ink);
        setColour (juce::TextEditor::highlightColourId, c::accent);
        setColour (juce::TextEditor::outlineColourId, c::inkSoft);
        setColour (juce::TextEditor::focusedOutlineColourId, c::ink);
        setColour (juce::CaretComponent::caretColourId, c::ink);
    }

    juce::Font getComboBoxFont (juce::ComboBox& b) override { return medium (b.getWidth() < 62 ? 10.5f : 12.0f); }
    juce::Font getPopupMenuFont() override { return medium (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return black (juce::jmin (12.5f, h * 0.55f), 0.04f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    juce::Font getAlertWindowTitleFont() override { return bold (15.0f); }
    juce::Font getAlertWindowMessageFont() override { return medium (13.0f); }
    juce::Font getAlertWindowFont() override { return medium (12.0f); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (pixel (10.0f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, isOnDark (s) ? c::onPlate : c::ink);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::white);
        l->setColour (juce::Label::textWhenEditingColourId, c::ink);
        l->setColour (juce::Label::outlineWhenEditingColourId, c::ink);
        return l;
    }

    // rotary knobs: knob on top, then a gap for the caption (a sibling Label), then the value
    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        if (! s.isRotary()) return juce::LookAndFeel_V4::getSliderLayout (s);
        juce::Slider::SliderLayout l;
        auto r = s.getLocalBounds();
        l.textBoxBounds = r.removeFromBottom (kValueH);
        r.removeFromBottom (kCaptionH);
        l.sliderBounds = r;
        return l;
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1,
                           juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (size, size);
        const auto ctr = r.getCentre();
        const float ar = size * 0.5f - 3.0f, br = size * 0.5f - 9.0f;
        const bool dark = isOnDark (s);
        auto arc = [&] (float rad, float from, float to, juce::Colour col, float width)
        {
            juce::Path p;
            p.addCentredArc (ctr.x, ctr.y, rad, rad, 0, from, to, true);
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        const float ang = a0 + pos * (a1 - a0);

        arc (ar, a0, a1, dark ? c::trackDark : c::track, 4.0f);
        if (pos > 0.002f) arc (ar, a0, ang, c::accent, 4.0f);

        // body
        juce::Path body;
        body.addEllipse (ctr.x - br, ctr.y - br, br * 2, br * 2);
        juce::DropShadow (juce::Colour (0x73000000), 6, { 0, 3 }).drawForPath (g, body);
        g.setGradientFill (juce::ColourGradient (c::knobHi, ctr.x, ctr.y - br, c::knob, ctr.x, ctr.y + br, false));
        g.fillPath (body);
        // pointer
        g.setColour (c::onPlate);
        g.drawLine ({ { ctr.x + br * 0.3f * std::sin (ang), ctr.y - br * 0.3f * std::cos (ang) },
                      { ctr.x + (br - 3) * std::sin (ang), ctr.y - (br - 3) * std::cos (ang) } }, 2.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const bool dark = isOnDark (s);
        g.setColour (dark ? c::trackDark : c::track);
        g.fillRoundedRectangle (r, 4.0f);
        const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
        if (style == juce::Slider::LinearBarVertical || style == juce::Slider::LinearVertical)
        {
            const float zero = bip ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
            const float yz = r.getBottom() - zero * r.getHeight();
            const float yv = pos;   // already a pixel position
            auto bar = juce::Rectangle<float>::leftTopRightBottom (r.getX(), juce::jmin (yz, yv), r.getRight(), juce::jmax (yz, yv));
            g.setColour (bip ? (yv < yz ? (dark ? c::accent : c::go) : c::signal) : c::accent);   // go is lost on the green deck
            g.fillRoundedRectangle (bar, 3.0f);
            if (bip) { g.setColour (dark ? c::onPlate.withAlpha (0.6f) : c::inkSoft); g.fillRect (r.getX() + 3, yz - 0.5f, r.getWidth() - 6, 1.0f); }
        }
        else
        {
            const float zero = bip ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
            const float xz = r.getX() + zero * r.getWidth();
            const float xv = pos;
            auto bar = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (xz, xv), r.getY(), juce::jmax (xz, xv), r.getBottom());
            g.setColour (bip ? (xv >= xz ? (dark ? c::accent : c::go) : c::signal) : c::accent);
            g.fillRoundedRectangle (bar, 3.0f);
            if (bip) { g.setColour (dark ? c::onPlate.withAlpha (0.6f) : c::inkSoft); g.fillRect (xz - 0.5f, r.getY() + 3, 1.0f, r.getHeight() - 6); }
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f, 2.0f).withTrimmedRight (3.0f).withTrimmedBottom (1.0f);
        const bool on = b.getToggleState();
        const bool rec = b.getProperties()["sgRec"];
        auto p = platePath (r);
        g.setColour (juce::Colour (0x59000000));
        g.fillPath (p, juce::AffineTransform::translation (3, 3));
        // on the black deck an off plate would vanish: lift it a step
        juce::Colour fill = b.getProperties()["sgDeck"] ? juce::Colour (0xff2c3038) : c::plate;
        if (on) fill = rec ? c::signal : c::accent;
        else if (down) fill = c::tag.darker (0.3f);
        else if (over) fill = c::tag;
        g.setColour (fill);
        g.fillPath (p);
        if (b.getClickingTogglesState())
        {   // LED: state is never colour alone
            const float d = juce::jmin (8.0f, r.getHeight() * 0.34f);
            auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.58f + d * 0.5f + 2, r.getCentreY() });
            g.setColour (on ? (rec ? c::onPlate : c::go) : c::onPlate.withAlpha (0.25f));
            g.fillEllipse (led);
            if (on && ! rec) { g.setColour (c::onAccent); g.drawEllipse (led, 1.2f); }
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f, 2.0f).withTrimmedRight (3.0f).withTrimmedBottom (1.0f);
        const bool on = b.getToggleState(), rec = b.getProperties()["sgRec"];
        if (b.getClickingTogglesState()) r.removeFromLeft (r.getHeight() * 0.58f + 12.0f);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (on && ! rec ? c::onAccent : c::onPlate);
        g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred, false);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        if (box.getWidth() < 62) { label.setBounds (1, 1, box.getWidth() - 2, box.getHeight() - 2); label.setJustificationType (juce::Justification::centred); }
        else { label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2); label.setJustificationType (juce::Justification::centredLeft); }
        label.setBorderSize ({ 0, 3, 0, 2 });
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, isOnDark (box) ? c::onPlate : c::ink);
        label.setMinimumHorizontalScale (0.7f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        const bool dark = isOnDark (box);
        g.setColour (dark ? juce::Colour (0x14ffffff) : juce::Colours::white);
        g.fillRoundedRectangle (r, 4.0f);
        const bool over = box.isMouseOver (true);
        g.setColour (dark ? c::onPlate.withAlpha (over ? 0.6f : 0.2f) : (over ? c::inkSoft : c::ink.withAlpha (0.18f)));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        if (w < 62) return;
        juce::Path chev;
        const float ax = (float) w - 11.0f, ay = h * 0.5f;
        chev.startNewSubPath (ax - 3.5f, ay - 2.0f); chev.lineTo (ax, ay + 1.5f); chev.lineTo (ax + 3.5f, ay - 2.0f);
        g.setColour (dark ? c::onPlate : c::ink);
        g.strokePath (chev, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (c::ink.withAlpha (0.25f));
        g.drawRect (0, 0, w, h, 1);
    }
};

/** Parameter-bound control. Kinds: Knob, Box (number bar), VSlider, HBar, Toggle, Menu. Empty label = no caption.
    Knobs carry their caption between the knob and the value; the other kinds above the control. */
class Control : public juce::Component
{
public:
    enum class Kind { Knob, Box, VSlider, HBar, Toggle, Menu };

    Control (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, Kind k)
        : kind (k), paramId (id)
    {
        caption.setText (label.toUpperCase(), juce::dontSendNotification);
        caption.setJustificationType (k == Kind::Knob ? juce::Justification::centred : juce::Justification::centredLeft);
        caption.setFont (bold (11.0f, 0.06f));
        caption.setColour (juce::Label::textColourId, c::ink);
        caption.setBorderSize ({});
        caption.setMinimumHorizontalScale (0.75f);
        caption.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (caption);
        caption.setVisible (label.isNotEmpty() && k != Kind::Toggle);

        if (k == Kind::Toggle)
        {
            button.setClickingTogglesState (true);
            button.setButtonText (label);
            addAndMakeVisible (button);
            ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
        }
        else if (k == Kind::Menu)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))) combo.addItemList (p->choices, 1);
            addAndMakeVisible (combo);
            ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, combo);
        }
        else
        {
            using S = juce::Slider;
            switch (k)
            {
                case Kind::Knob: slider.setSliderStyle (S::RotaryHorizontalVerticalDrag);
                                 slider.setTextBoxStyle (S::TextBoxBelow, false, 80, LookAndFeel::kValueH); break;
                case Kind::Box:
                case Kind::VSlider: slider.setSliderStyle (S::LinearBarVertical);
                                 slider.setTextBoxStyle (S::NoTextBox, false, 0, 0); break;
                default:         slider.setSliderStyle (S::LinearBar);
                                 slider.setTextBoxStyle (S::NoTextBox, false, 0, 0); break;
            }
            slider.setDoubleClickReturnValue (true, 0.0);
            addAndMakeVisible (slider);
            sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
            if (auto* p = s.getParameter (id)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            if (k != Kind::Knob) slider.setTooltip (label);
            slider.onValueChange = [this] { repaint(); };
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (kind == Kind::Toggle) { button.setBounds (r); return; }
        if (kind == Kind::Knob)
        {
            slider.setBounds (r);
            caption.setBounds (0, r.getBottom() - LookAndFeel::kValueH - LookAndFeel::kCaptionH, r.getWidth(), LookAndFeel::kCaptionH);
            caption.toFront (false);
            return;
        }
        if (caption.isVisible()) caption.setBounds (r.removeFromTop (16).withTrimmedBottom (2));
        if (kind == Kind::Menu) { combo.setBounds (r.withHeight (juce::jmin (r.getHeight(), 22))); return; }
        slider.setBounds (r);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (kind == Kind::Box || kind == Kind::HBar)
        {
            const auto t = slider.getTextFromValue (slider.getValue());
            const auto f = pixel (10.0f);
            auto sb = slider.getBounds().toFloat();
            const float tw = juce::GlyphArrangement::getStringWidth (f, t) + 8.0f;
            auto patch = juce::Rectangle<float> (tw, 12.0f).withCentre (sb.getCentre());
            if (patch.getHeight() < sb.getHeight() - 2)
            {
                g.setColour (isOnDark (*this) ? c::plate.withAlpha (0.75f) : c::glass.withAlpha (1.0f));
                g.fillRoundedRectangle (patch, 2.0f);
            }
            g.setColour (isOnDark (*this) ? c::onPlate : c::ink);
            g.setFont (f);
            g.drawText (t, sb, juce::Justification::centred);
        }
    }

    void setCaptionColour (juce::Colour col) { caption.setColour (juce::Label::textColourId, col); }

    juce::Slider slider;
    juce::TextButton button;
    juce::ComboBox combo;

private:
    Kind kind;
    juce::String paramId;
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
};

/** Display well: the recessed dark-blue rectangle data is drawn in. */
inline void displayWell (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (c::display);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (c::plate);                                   // black cockpit bezel
    g.drawRoundedRectangle (r.reduced (3.0f), 4.0f, 6.0f);
    g.setColour (c::onPlate.withAlpha (0.12f));
    g.drawRoundedRectangle (r.reduced (6.5f), 3.0f, 1.0f);
    g.setColour (c::onPlate.withAlpha (0.18f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

/** One colour, one meaning: a hit is accent whatever the sequencer. The type is named in words (EUC, STP ...). */
inline juce::Colour typeColour (int) { return c::accent; }

inline juce::String routeText (int port, int ch) { return (port == 0 ? juce::String ("HOST") : "P" + juce::String (port)) + " " + juce::String (ch); }

/** Small live picture of a track, used in the track list and on the type tabs. */
inline void drawMini (juce::Graphics& g, SeggProcessor& p, int t, juce::Rectangle<float> r, juce::Colour col, juce::Colour empty = c::trackDark)
{
    const int type = p.ti (t, pType);
    const auto& u = p.engine.ui[(size_t) t];
    const int pos = u.pos.load (std::memory_order_relaxed);
    const bool playing = p.uiPlaying.load (std::memory_order_relaxed) && p.ti (t, pOn) != 0;
    switch (type)
    {
        case Euclid:
        {
            const int n = juce::jlimit (1, kMaxEuclid, p.ti (t, pEsteps));
            const int k = playing ? u.pulses.load (std::memory_order_relaxed) : p.ti (t, pEpulses);
            const int rot = playing ? u.rot.load (std::memory_order_relaxed) : p.ti (t, pErot);
            const float w = r.getWidth() / (float) kMaxEuclid * juce::jlimit (1.0f, 2.0f, 16.0f / (float) n * 2.0f);
            for (int i = 0; i < n; ++i)
            {
                auto b = juce::Rectangle<float> (r.getX() + (float) i * w, r.getY() + 2, juce::jmax (1.5f, w - 1.5f), r.getHeight() - 4);
                const bool hit = euclidHit (pmod (i - rot, n), k, n);
                g.setColour (playing && i == pos ? c::signal : (hit ? col : empty));
                g.fillRect (hit || (playing && i == pos) ? b : b.withTrimmedTop (b.getHeight() - 2));
            }
            break;
        }
        case Step:
        {
            const int n = juce::jlimit (1, kNumCells, p.ti (t, pSlen));
            const float w = r.getWidth() / (float) kNumCells;
            const auto& cells = p.engine.patterns[(size_t) t].step;
            for (int i = 0; i < n; ++i)
            {
                const auto& cell = cells[(size_t) i];
                const float v = 0.5f + (float) cell.pitch.load (std::memory_order_relaxed) / 48.0f;
                const float bh = juce::jlimit (2.0f, r.getHeight(), r.getHeight() * v);
                auto b = juce::Rectangle<float> (r.getX() + (float) i * w, r.getBottom() - bh, w - 1.5f, bh);
                g.setColour (playing && i == pos ? c::signal : (cell.on.load (std::memory_order_relaxed) != 0 ? col : empty));
                g.fillRect (b);
            }
            break;
        }
        case Turing:
        {
            const int n = juce::jlimit (2, 32, p.ti (t, pTlen));
            const auto reg = u.reg.load (std::memory_order_relaxed);
            const float w = juce::jmin (r.getWidth() / (float) n, 8.0f);
            for (int i = 0; i < n; ++i)
            {
                g.setColour (((reg >> i) & 1u) != 0 ? col : empty);
                g.fillRect (r.getX() + (float) i * w, r.getY() + 4, w - 1.5f, r.getHeight() - 8);
            }
            break;
        }
        case Cartesian:
        {
            const float s = r.getHeight() / 4.0f;
            const auto& cells = p.engine.patterns[(size_t) t].cart;
            const int cx = u.cx.load (std::memory_order_relaxed), cy = u.cy.load (std::memory_order_relaxed);
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                {
                    const bool here = playing && x == cx && y == cy;
                    g.setColour (here ? c::signal : (cells[(size_t) (y * 4 + x)].on.load (std::memory_order_relaxed) != 0 ? col : empty));
                    g.fillRect (r.getX() + (float) x * s * 1.6f, r.getY() + (float) y * s, s * 1.6f - 1.0f, s - 1.0f);
                }
            break;
        }
        default:
        {
            g.setColour (empty);
            g.drawRect (r, 1.0f);
            const int n = juce::jlimit (1, kMaxBalls, p.ti (t, pPcount));
            g.setColour (col);
            for (int b = 0; b < n; ++b)
                g.fillEllipse (r.getX() + 3 + u.bx[(size_t) b].load (std::memory_order_relaxed) * (r.getWidth() - 6) - 2.5f,
                               r.getY() + 3 + u.by[(size_t) b].load (std::memory_order_relaxed) * (r.getHeight() - 6) - 2.5f, 5, 5);
            break;
        }
    }
}

/** All 16 tracks at a glance, each with its own live picture. Click a row to edit it, click the LED to switch it. */
class TrackList : public juce::Component
{
public:
    static constexpr int kRowH = 34, kLedW = 56;
    std::function<void (int)> onSelect;

    explicit TrackList (SeggProcessor& p) : proc (p) {}

    void setSelected (int t) { selected = t; repaint(); }

    /** Called by the editor's timer. */
    void tick()
    {
        for (int t = 0; t < kNumTracks; ++t)
        {
            const auto f = proc.engine.ui[(size_t) t].fires.load (std::memory_order_relaxed);
            if (f != seen[(size_t) t]) { seen[(size_t) t] = f; flash[(size_t) t] = 1.0f; }
            else flash[(size_t) t] *= 0.78f;
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        static const char* tags[kNumTypes] = { "EUC", "STP", "TUR", "CAR", "PHY" };
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto r = juce::Rectangle<float> (0, (float) (t * kRowH), (float) getWidth() - 4, (float) kRowH - 5);
            const bool on = proc.ti (t, pOn) != 0, sel = t == selected;
            const int type = proc.ti (t, pType);
            // selected: accent with a signal offset; live: black plate; off: a faint well on the glass
            if (sel) { g.setColour (c::signal); g.fillRoundedRectangle (r.translated (3, 3), 4.0f); }
            g.setColour (sel ? c::accent : (on ? c::plate : c::ink.withAlpha (0.08f)));
            g.fillRoundedRectangle (r, 4.0f);
            const auto text  = sel ? c::onAccent : (on ? c::onPlate : c::inkSoft);
            const auto hit   = sel ? c::onAccent : (on ? c::accent : c::ink.withAlpha (0.26f));
            const auto empty = sel ? c::ink.withAlpha (0.25f) : (on ? c::trackDark : c::ink.withAlpha (0.12f));

            g.setColour (text);
            g.setFont (black (14.0f));
            g.drawText (juce::String (t + 1), juce::Rectangle<float> (4, r.getY(), 26, r.getHeight()), juce::Justification::centred);

            // on/off LED, brighter on every note
            auto led = juce::Rectangle<float> (11, 11).withCentre ({ 42, r.getCentreY() });
            const float fl = flash[(size_t) t];
            g.setColour (on ? c::go.interpolatedWith (c::accent, fl) : c::ink.withAlpha (0.2f));
            g.fillEllipse (led);
            if (on) { g.setColour (sel ? c::onAccent : c::onPlate.withAlpha (0.85f)); g.drawEllipse (led.expanded (1.0f), 1.5f); }

            g.setFont (pixel (10.0f));
            g.setColour (sel ? c::onAccent : (on ? c::accent : c::inkSoft));
            g.drawText (tags[juce::jlimit (0, kNumTypes - 1, type)], juce::Rectangle<float> (56, r.getY(), 32, r.getHeight()), juce::Justification::centredLeft);

            auto mini = juce::Rectangle<float> (90, r.getY() + 5, r.getWidth() - 90 - 66, r.getHeight() - 10);
            drawMini (g, proc, t, mini, hit, empty);

            g.setColour (sel ? c::onAccent : (on ? c::onPlate.withAlpha (0.8f) : c::inkSoft));
            g.drawText (routeText (proc.ti (t, pPort), proc.ti (t, pCh)), juce::Rectangle<float> (r.getRight() - 66, r.getY(), 58, r.getHeight()),
                        juce::Justification::centredRight);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int t = juce::jlimit (0, kNumTracks - 1, e.y / kRowH);
        if (e.x >= 32 && e.x < kLedW) proc.setTrackParam (t, pOn, proc.ti (t, pOn) != 0 ? 0.0f : 1.0f);
        else if (onSelect) onSelect (t);
    }

private:
    SeggProcessor& proc;
    int selected = 0;
    std::array<std::uint32_t, kNumTracks> seen {};
    std::array<float, kNumTracks> flash {};
};

/** The big well: the running sequencer of the selected track. The pattern types are edited here directly. */
class EngineView : public juce::Component
{
public:
    explicit EngineView (SeggProcessor& p) : proc (p)
    {
        for (auto* b : { &rndBtn, &clrBtn }) addChildComponent (b);
        rndBtn.setButtonText ("RND");
        clrBtn.setButtonText ("CLR");
        rndBtn.onClick = [this] { randomise(); };
        clrBtn.onClick = [this] { clearCells(); };
    }

    void setTrack (int t) { track = t; history.fill (-1); histCount = 0; }
    int laneShown() const { return lane; }
    void setLane (int l) { lane = l; repaint(); }

    void tick()
    {
        const int type = proc.ti (track, pType);
        const bool editable = type == Step || type == Cartesian || (type == Euclid && proc.ti (track, pEmel) > 1);
        rndBtn.setVisible (editable);
        clrBtn.setVisible (editable);
        const auto& u = proc.engine.ui[(size_t) track];
        const auto f = u.fires.load (std::memory_order_relaxed);
        if (f != lastFires)
        {
            lastFires = f;
            flash = 1.0f;
            history[(size_t) (histCount++ % (int) history.size())] = u.lastNote.load (std::memory_order_relaxed);
        }
        else flash *= 0.8f;
        for (int b = 0; b < kMaxBalls; ++b)
        {
            for (int k = kTrail - 1; k > 0; --k) trail[(size_t) b][(size_t) k] = trail[(size_t) b][(size_t) k - 1];
            trail[(size_t) b][0] = { u.bx[(size_t) b].load (std::memory_order_relaxed), u.by[(size_t) b].load (std::memory_order_relaxed) };
        }
        repaint();
    }

    void resized() override
    {
        clrBtn.setBounds (getWidth() - 62, 8, 54, 26);
        rndBtn.setBounds (getWidth() - 118, 8, 54, 26);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        displayWell (g, r);
        switch (proc.ti (track, pType))
        {
            case Euclid:    paintEuclid (g, r); break;
            case Step:      paintLanes (g, r.reduced (12.0f), juce::jlimit (1, kNumCells, proc.ti (track, pSlen)), proc.engine.ui[(size_t) track].pos.load()); break;
            case Turing:    paintTuring (g, r); break;
            case Cartesian: paintCartesian (g, r); break;
            default:        paintPhysics (g, r); break;
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int type = proc.ti (track, pType);
        if (type == Step || (type == Euclid && proc.ti (track, pEmel) > 1)) laneMouse (e, true);
        else if (type == Cartesian)
        {
            padDown = padAt (e.position);
            padMoved = false;
            if (padDown >= 0) padStart = proc.engine.patterns[(size_t) track].cart[(size_t) padDown].pitch.load();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int type = proc.ti (track, pType);
        if (type == Step || (type == Euclid && proc.ti (track, pEmel) > 1)) laneMouse (e, false);
        else if (type == Cartesian && padDown >= 0 && std::abs (e.getDistanceFromDragStartY()) > 3)
        {
            padMoved = true;
            proc.engine.patterns[(size_t) track].cart[(size_t) padDown].pitch = (std::int8_t) juce::jlimit (-36, 48, padStart - e.getDistanceFromDragStartY() / 5);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (proc.ti (track, pType) == Cartesian && padDown >= 0 && ! padMoved)
        {
            auto& cell = proc.engine.patterns[(size_t) track].cart[(size_t) padDown];
            cell.on = cell.on.load() != 0 ? 0 : 1;
        }
        padDown = -1;
        paintOn = -1;
    }

private:
    static constexpr int kTrail = 14, kLaneTabs = 5;
    static inline const char* kLaneNames[kLaneTabs] = { "PITCH", "VEL", "GATE", "CHANCE", "RATCH" };

    struct LaneRange { int lo, hi; };
    static LaneRange laneRange (int l)
    {
        static const LaneRange ranges[kLaneTabs] = { { -24, 24 }, { 1, 127 }, { 1, 127 }, { 0, 100 }, { 1, 8 } };
        return ranges[l];
    }
    static std::atomic<std::int8_t>& laneField (Cell& c2, int l)
    {
        switch (l)
        {
            case 1:  return c2.vel;
            case 2:  return c2.gate;
            case 3:  return c2.prob;
            case 4:  return c2.ratch;
            default: return c2.pitch;
        }
    }

    juce::Rectangle<float> lanesArea() const
    {
        auto r = getLocalBounds().toFloat().reduced (12.0f);
        return proc.ti (track, pType) == Euclid ? r.removeFromBottom (92) : r;
    }

    int effectiveLane() const { return proc.ti (track, pType) == Euclid ? 0 : lane; }

    void paintLanes (juce::Graphics& g, juce::Rectangle<float> r, int len, int pos)
    {
        const bool compact = proc.ti (track, pType) == Euclid;
        const int shown = effectiveLane();
        if (! compact)
        {
            auto tabs = r.removeFromTop (26);
            g.setFont (pixel (10.0f));
            for (int l = 0; l < kLaneTabs; ++l)
            {
                auto t = tabs.removeFromLeft (58).reduced (1, 2);
                g.setColour (l == shown ? c::accent : c::trackDark);
                g.fillRoundedRectangle (t, 3.0f);
                g.setColour (l == shown ? c::onAccent : c::onPlate);
                g.drawText (kLaneNames[l], t, juce::Justification::centred);
            }
            r.removeFromTop (8);
        }
        auto gates = r.removeFromTop (18);
        auto foot = r.removeFromBottom (compact ? 0.0f : 18.0f);
        r.removeFromTop (4);
        const float w = r.getWidth() / (float) kNumCells;
        const auto range = laneRange (shown);
        const bool bip = range.lo < 0;
        const float zero = bip ? r.getCentreY() : r.getBottom();
        auto& cells = proc.engine.patterns[(size_t) track].step;
        const auto col = typeColour (Step);
        const bool playing = proc.uiPlaying.load (std::memory_order_relaxed);
        for (int i = 0; i < kNumCells; ++i)
        {
            auto& cell = cells[(size_t) i];
            const bool active = i < len, on = cell.on.load() != 0;
            const float x = r.getX() + (float) i * w;
            const float alpha = active ? 1.0f : 0.3f;
            if (playing && i == pos && ! compact)
            {
                g.setColour (c::signal);
                g.drawRoundedRectangle (juce::Rectangle<float> (x - 2, gates.getY() - 3, w + 2, r.getBottom() - gates.getY() + 6), 3.0f, 2.0f);
            }
            auto pad = juce::Rectangle<float> (x, gates.getY(), w - 2, gates.getHeight());
            g.setColour ((on ? col : c::trackDark).withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (pad, 2.0f);
            g.setColour (c::trackDark.withMultipliedAlpha (alpha));
            g.fillRect (x, r.getY(), w - 2, r.getHeight());
            const int v = laneField (cell, shown).load();
            const float norm = (float) (v - range.lo) / (float) (range.hi - range.lo);
            const float y = r.getBottom() - norm * r.getHeight();
            g.setColour ((on ? c::onPlate : c::onPlate.withAlpha (0.35f)).withMultipliedAlpha (alpha));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x, juce::jmin (y, zero), x + w - 2, juce::jmax (juce::jmax (y, zero), juce::jmin (y, zero) + 2)));
        }
        if (bip) { g.setColour (c::onPlate.withAlpha (0.4f)); g.fillRect (r.getX(), zero - 0.5f, r.getWidth() - 2, 1.0f); }
        if (! compact && hoverCol >= 0)
        {
            g.setFont (pixel (10.0f));
            g.setColour (c::onPlate);
            auto& cell = cells[(size_t) hoverCol];
            g.drawText ("STEP " + juce::String (hoverCol + 1) + "  " + kLaneNames[shown] + " " + juce::String ((int) laneField (cell, shown).load()), foot, juce::Justification::centredLeft);
        }
    }

    void laneMouse (const juce::MouseEvent& e, bool down)
    {
        const bool compact = proc.ti (track, pType) == Euclid;
        auto r = lanesArea();
        if (! compact)
        {
            auto tabs = r.removeFromTop (26);
            if (down && tabs.contains (e.position))
            {
                const int l = (int) ((e.position.x - tabs.getX()) / 58.0f);
                if (l < kLaneTabs) { lane = l; repaint(); }
                return;
            }
            r.removeFromTop (8);
        }
        auto gates = r.removeFromTop (18);
        if (! compact) r.removeFromBottom (18);
        r.removeFromTop (4);
        const float w = r.getWidth() / (float) kNumCells;
        const int col = (int) ((e.position.x - r.getX()) / w);
        if (col < 0 || col >= kNumCells) return;
        auto& cell = proc.engine.patterns[(size_t) track].step[(size_t) col];
        if (down && gates.contains (e.position))
        {
            paintOn = cell.on.load() != 0 ? 0 : 1;
            cell.on = (std::int8_t) paintOn;
            return;
        }
        if (paintOn >= 0) { cell.on = (std::int8_t) paintOn; return; }
        const int shown = effectiveLane();
        const auto range = laneRange (shown);
        const float norm = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
        laneField (cell, shown) = (std::int8_t) juce::roundToInt ((float) range.lo + norm * (float) (range.hi - range.lo));
        hoverCol = col;
    }

    void paintEuclid (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const int mel = proc.ti (track, pEmel);
        if (mel > 1) paintLanes (g, lanesArea(), mel, -1);
        auto area = r.reduced (20.0f);
        if (mel > 1) area.removeFromBottom (100);
        const auto ctr = area.getCentre();
        const float rad = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 12.0f;
        const int n = juce::jlimit (1, kMaxEuclid, proc.ti (track, pEsteps));
        const auto& u = proc.engine.ui[(size_t) track];
        const bool playing = proc.uiPlaying.load (std::memory_order_relaxed);
        const int k = playing ? u.pulses.load (std::memory_order_relaxed) : juce::jmin (n, proc.ti (track, pEpulses));
        const int rot = playing ? u.rot.load (std::memory_order_relaxed) : proc.ti (track, pErot);
        const int acc = juce::jlimit (0, juce::jmax (1, k), proc.ti (track, pEacc));
        const int pos = u.pos.load (std::memory_order_relaxed);
        const auto col = typeColour (Euclid);

        auto point = [&] (int i, float radius) {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) n;
            return juce::Point<float> (ctr.x + radius * std::sin (a), ctr.y - radius * std::cos (a));
        };
        g.setColour (c::grid);
        g.drawEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (ctr), 1.0f);

        juce::Path poly;
        bool started = false;
        for (int i = 0; i < n; ++i)
            if (euclidHit (pmod (i - rot, n), k, n))
            {
                const auto q = point (i, rad);
                if (! started) { poly.startNewSubPath (q); started = true; } else poly.lineTo (q);
            }
        if (started)
        {
            poly.closeSubPath();
            g.setColour (col.withAlpha (0.14f)); g.fillPath (poly);
            g.setColour (col.withAlpha (0.7f));  g.strokePath (poly, juce::PathStrokeType (1.5f));
        }
        int pulse = 0;
        const float dot = juce::jlimit (5.0f, 13.0f, 140.0f / (float) n);
        for (int s = 0; s < n; ++s)
        {
            const int i = pmod (s + rot, n);     // walk the pulses in playing order
            const bool hit = euclidHit (s, k, n);
            const bool accent = hit && acc > 0 && euclidHit (pulse, acc, k);
            if (hit) ++pulse;
            const auto q = point (i, rad);
            const float d = (hit ? dot : dot * 0.5f) + (accent ? 4.0f : 0.0f);
            g.setColour (hit ? col : c::onPlate.withAlpha (0.35f));
            g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (q));
        }
        if (playing)
        {
            g.setColour (c::signal);
            g.drawLine ({ ctr, point (pos, rad) }, 2.0f);
            g.drawEllipse (juce::Rectangle<float> (dot + 7, dot + 7).withCentre (point (pos, rad)), 2.0f);
        }
        g.setColour (c::onPlate);
        g.setFont (black (34.0f));
        g.drawText (juce::String (k) + "/" + juce::String (n), juce::Rectangle<float> (140, 44).withCentre (ctr), juce::Justification::centred);
        g.setFont (pixel (10.0f));
        if (playing)
            g.drawText ("STEP " + juce::String (pos + 1).paddedLeft ('0', 2), juce::Rectangle<float> (140, 14).withCentre (ctr.translated (0, 28)),
                        juce::Justification::centred);
        if (mel <= 1)
        {   // corner readouts, clear of the RND and CLR buttons that only show with MELODY
            auto in = r.reduced (18.0f, 14.0f);
            g.drawText ("LEN " + juce::String (n) + "  ROT " + juce::String (rot), in.removeFromTop (14), juce::Justification::left);
            g.setColour (c::accent);
            g.drawText ("ACC " + juce::String (acc), in.withHeight (14), juce::Justification::right);
        }
    }

    void paintTuring (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto plot = r.removeFromBottom (96).reduced (14.0f, 10.0f);
        auto area = r.reduced (16.0f);
        const auto ctr = area.getCentre();
        const float rad = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 12.0f;
        const int n = juce::jlimit (2, 32, proc.ti (track, pTlen));
        const auto reg = proc.engine.ui[(size_t) track].reg.load (std::memory_order_relaxed);
        const auto col = typeColour (Turing);
        for (int i = 0; i < n; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) n;
            const juce::Point<float> q (ctr.x + rad * std::sin (a), ctr.y - rad * std::cos (a));
            const float s = juce::jlimit (6.0f, 16.0f, 170.0f / (float) n);
            auto b = juce::Rectangle<float> (s, s).withCentre (q);
            g.setColour (((reg >> i) & 1u) != 0 ? col : c::trackDark);
            g.fillRect (b);
            if (i < 8) { g.setColour (c::onPlate.withAlpha (0.7f)); g.drawRect (b, 1.0f); }   // the bits that make the pitch
        }
        g.setColour (c::signal);       // write head: bit 0
        g.drawEllipse (juce::Rectangle<float> (24, 24).withCentre ({ ctr.x, ctr.y - rad }), 2.0f);
        const float k = proc.tf (track, pTknob);
        const char* state = k >= 99.5f ? "LOCKED" : (k <= 0.5f ? "LOCKED X2" : (std::abs (k - 50.0f) < 6.0f ? "RANDOM" : "SLIPPING"));
        g.setColour (c::onPlate);
        g.setFont (black (34.0f));
        g.drawText (juce::String (juce::roundToInt (k)) + " %", juce::Rectangle<float> (rad * 1.6f, 40).withCentre (ctr.translated (0, -16)), juce::Justification::centred);
        g.setFont (pixel (10.0f));
        g.drawText ("LOCK  " + juce::String (state), juce::Rectangle<float> (rad * 1.7f, 14).withCentre (ctr.translated (0, 8)), juce::Justification::centred);
        const int note = proc.engine.ui[(size_t) track].lastNote.load (std::memory_order_relaxed);
        const auto noteText = note >= 0 ? juce::String (kNoteNames[note % 12]) + juce::String (note / 12 - 2) : juce::String ("--");
        g.setColour (c::onPlate.withAlpha (0.85f));
        g.drawText ("LENGTH " + juce::String (n) + "  NOTE " + noteText, juce::Rectangle<float> (rad * 1.7f, 14).withCentre (ctr.translated (0, 26)), juce::Justification::centred);

        // the notes that came out, newest on the right
        g.setColour (c::trackDark);
        g.fillRoundedRectangle (plot, 3.0f);
        int lo = 127, hi = 0;
        for (int v : history) if (v >= 0) { lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
        const int span = juce::jmax (12, hi - lo);
        const int size = (int) history.size();
        const float w = plot.getWidth() / (float) size;
        for (int i = 0; i < size; ++i)
        {
            g.setColour (i == size - 1 ? c::signal : col);
            const int v = history[(size_t) pmod (histCount - size + i, size)];
            if (v < 0) continue;
            g.fillRect (plot.getX() + (float) i * w, plot.getBottom() - 8 - (float) (v - lo) / (float) span * (plot.getHeight() - 12), juce::jmax (2.0f, w - 1), 4.0f);
        }
    }

    juce::Rectangle<float> padBounds (int cell) const
    {
        auto r = getLocalBounds().toFloat().reduced (20.0f);
        r.removeFromBottom (22);
        r.removeFromTop (22);       // room for the RND and CLR buttons
        const float side = juce::jmin (r.getWidth(), r.getHeight()), gap = 8.0f, s = (side - 3 * gap) / 4.0f;
        const auto origin = juce::Rectangle<float> (side, side).withCentre (r.getCentre()).getTopLeft();
        return { origin.x + (float) (cell % 4) * (s + gap), origin.y + (float) (cell / 4) * (s + gap), s, s };
    }

    int padAt (juce::Point<float> q) const
    {
        for (int i = 0; i < kNumCells; ++i) if (padBounds (i).contains (q)) return i;
        return -1;
    }

    void paintCartesian (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto& cells = proc.engine.patterns[(size_t) track].cart;
        const auto& u = proc.engine.ui[(size_t) track];
        const int w = proc.ti (track, pCw), h = proc.ti (track, pChgt);
        const int cx = u.cx.load (std::memory_order_relaxed), cy = u.cy.load (std::memory_order_relaxed);
        const bool playing = proc.uiPlaying.load (std::memory_order_relaxed), snake = proc.ti (track, pCmode) == 1;
        const auto col = typeColour (Cartesian);
        for (int i = 0; i < kNumCells; ++i)
        {
            auto b = padBounds (i);
            auto& cell = cells[(size_t) i];
            const bool on = cell.on.load() != 0, inside = i % 4 < w && i / 4 < h;
            const bool here = playing && i == cy * 4 + cx;
            if (! inside) { g.setColour (c::onPlate.withAlpha (0.05f)); g.fillRoundedRectangle (b, 4.0f); continue; }
            if (! on)
            {   // denied: hatched and labelled, so the state does not rest on colour alone
                juce::Graphics::ScopedSaveState save (g);
                juce::Path clip; clip.addRoundedRectangle (b, 4.0f);
                g.reduceClipRegion (clip);
                g.setColour (c::onPlate.withAlpha (0.10f));
                for (float x = b.getX() - b.getHeight(); x < b.getRight(); x += 12.0f)
                    g.drawLine (x, b.getBottom(), x + b.getHeight(), b.getY(), 5.0f);
                g.setColour (c::onPlate.withAlpha (here ? 0.9f : 0.25f));
                g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, here ? 2.0f : 1.0f);
                g.setColour (c::onPlate.withAlpha (0.7f));
                g.setFont (pixel (10.0f));
                g.drawText ("DENIED", b.reduced (8, 6), juce::Justification::topLeft);
                continue;
            }
            g.setColour (here ? col : c::onPlate.withAlpha (0.12f));
            g.fillRoundedRectangle (b, 4.0f);
            if (here) { g.setColour (c::signal); g.drawRoundedRectangle (b.expanded (1.0f), 5.0f, 2.0f); }
            const int v = cell.pitch.load();
            g.setColour (here ? c::onAccent : c::onPlate);
            g.setFont (bold (14.0f));
            g.drawText ((v > 0 ? "+" : "") + juce::String (v), b.reduced (8, 6), juce::Justification::topLeft);
        }
        if (snake)
        {
            const int set = pmod (u.cset.load (std::memory_order_relaxed), kNumPaths);
            juce::Path path;
            for (int i = 0; i < kNumCells; ++i)
            {
                const auto q = padBounds (kPaths[set][(size_t) i]).getCentre();
                if (i == 0) path.startNewSubPath (q); else path.lineTo (q);
            }
            juce::Path dashed;
            const float dash[] = { 4.0f, 4.0f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, path, dash, 2);
            g.setColour (c::onPlate.withAlpha (0.5f));
            g.fillPath (dashed);
        }
        g.setColour (c::onPlate);
        g.setFont (pixel (10.0f));
        const auto text = snake ? "PATH " + juce::String (kPathNames[pmod (u.cset.load (std::memory_order_relaxed), kNumPaths)]).toUpperCase()
                                : "X EVERY " + juce::String (proc.ti (track, pCxdiv)) + "   Y EVERY " + juce::String (proc.ti (track, pCydiv));
        g.drawText (text + "   CLICK TO DENY, DRAG TO SET PITCH", r.removeFromBottom (26), juce::Justification::centred);
    }

    void paintPhysics (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto area = r.reduced (16.0f);
        const float side = juce::jmin (area.getWidth(), area.getHeight());
        auto box = juce::Rectangle<float> (side, side).withCentre (area.getCentre());
        const auto col = c::wave;
        g.setColour (c::onPlate.withAlpha (0.04f));
        g.fillRect (box);
        g.setColour (c::onPlate.withAlpha (0.6f).interpolatedWith (c::accent, flash));
        g.drawRect (box, 1.0f + 2.5f * flash);
        const int n = juce::jlimit (1, kMaxBalls, proc.ti (track, pPcount));
        const float rad = (0.02f + proc.tf (track, pPsize) * 0.0013f) * side;
        for (int b = 0; b < n; ++b)
        {
            for (int k = kTrail - 1; k >= 0; --k)
            {
                const auto q = trail[(size_t) b][(size_t) k];
                const float a = 1.0f - (float) k / (float) kTrail;
                const float d = rad * 2.0f * (k == 0 ? 1.0f : 0.5f * a);
                g.setColour (col.withAlpha (k == 0 ? 1.0f : 0.35f * a));
                g.fillEllipse (juce::Rectangle<float> (d, d).withCentre ({ box.getX() + q.x * side, box.getY() + q.y * side }));
            }
        }
        const float gravity = proc.tf (track, pPgrav);
        if (gravity > 0.5f)
        {
            g.setColour (c::onPlate.withAlpha (0.6f));
            g.setFont (pixel (10.0f));
            g.drawText ("GRAVITY", box.reduced (8).removeFromBottom (14), juce::Justification::centredRight);
        }
    }

    void randomise()
    {
        static const int degrees[8] = { 0, 0, 2, 3, 5, 7, 10, 12 };
        auto rnd = juce::Random::getSystemRandom();
        const bool pads = proc.ti (track, pType) == Cartesian;
        auto& cells = pads ? proc.engine.patterns[(size_t) track].cart : proc.engine.patterns[(size_t) track].step;
        for (auto& cell : cells)
        {
            cell.pitch = (std::int8_t) degrees[rnd.nextInt (8)];
            cell.on = (std::int8_t) (rnd.nextFloat() < (pads ? 0.8f : 0.55f) ? 1 : 0);
            cell.vel = (std::int8_t) (70 + rnd.nextInt (50));
        }
    }

    void clearCells()
    {
        const bool pads = proc.ti (track, pType) == Cartesian;
        auto& cells = pads ? proc.engine.patterns[(size_t) track].cart : proc.engine.patterns[(size_t) track].step;
        for (auto& cell : cells) cell.set (pads ? 1 : 0, 0);
    }

    SeggProcessor& proc;
    juce::TextButton rndBtn, clrBtn;
    int track = 0, lane = 0, hoverCol = -1, padDown = -1, padStart = 0, paintOn = -1, histCount = 0;
    bool padMoved = false;
    float flash = 0;
    std::uint32_t lastFires = 0;
    std::array<int, 64> history {};
    std::array<std::array<juce::Point<float>, kTrail>, kMaxBalls> trail {};
};

/** Selects the sequencer type of the track: a slanted plate with the name and a small icon. */
class TypeTab : public juce::Button
{
public:
    explicit TypeTab (int t) : juce::Button (kTypeNames[t]), type (t) { setRadioGroupId (1); setClickingTogglesState (false); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().withTrimmedRight (8).withTrimmedBottom (8);
        const bool on = getToggleState();
        if (on) taggedPlate (g, r, typeColour (type));
        else
        {
            g.setColour (juce::Colour (0x59000000)); g.fillPath (platePath (r), juce::AffineTransform::translation (3, 3));
            g.setColour (down ? c::tag.darker (0.3f) : (over ? c::tag : c::plate)); g.fillPath (platePath (r));
        }
        g.setColour (on ? c::onAccent : c::onPlate);
        g.setFont (black (12.0f, 0.04f));
        g.drawText (getName().toUpperCase(), r.reduced (r.getHeight() * 0.5f, 0), juce::Justification::centred, false);
    }

private:
    int type;
};

/** Assigns the 8 direct ports. Hardware interface ports, or virtual ports that Live can use as a MIDI input. */
class RoutingPage : public juce::Component
{
public:
    explicit RoutingPage (SeggProcessor& p) : proc (p)
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto* box = boxes.add (new juce::ComboBox());
            addAndMakeVisible (box);
            box->onChange = [this, i] {
                const int id = boxes[i]->getSelectedId();
                proc.setPortSlot (i, id == 1 ? juce::String() : (id == 2 ? DirectMidi::virtualId() : deviceIds[id - 3]));
                repaint();
            };
        }
        addAndMakeVisible (rescan);
        rescan.setButtonText ("RESCAN");
        rescan.onClick = [this] { refresh(); };
        refresh();
    }

    void refresh()
    {
        // a device that was unplugged may be back
        for (int i = 0; i < kNumSlots; ++i)
            if (! proc.direct.isSlotLive (i) && proc.direct.getSlot (i).isNotEmpty()) proc.direct.setSlot (i, proc.direct.getSlot (i));

        auto devices = juce::MidiOutput::getAvailableDevices();
        deviceIds.clear();
        juce::StringArray names;
        for (auto& d : devices) { deviceIds.add (d.identifier); names.add (d.name); }
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto* box = boxes[i];
            box->clear (juce::dontSendNotification);
            box->addItem ("Not assigned", 1);
           #if ! JUCE_WINDOWS
            box->addItem ("Virtual port: " + DirectMidi::virtualName (i), 2);
           #endif
            for (int d = 0; d < names.size(); ++d) box->addItem (names[d], d + 3);
            const auto current = proc.direct.getSlot (i);
            const int found = deviceIds.indexOf (current);
            if (current.isEmpty()) box->setSelectedId (1, juce::dontSendNotification);
            else if (current == DirectMidi::virtualId()) box->setSelectedId (2, juce::dontSendNotification);
            else if (found >= 0) box->setSelectedId (found + 3, juce::dontSendNotification);
            else box->setText ("Missing device", juce::dontSendNotification);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            const auto box = boxes[i]->getBounds();
            g.setFont (black (13.0f));
            g.setColour (c::ink);
            g.drawText (juce::String (i + 1), box.getX() - 44, box.getY(), 24, 22, juce::Justification::centredLeft);
            auto led = juce::Rectangle<float> (10, 10).withCentre ({ (float) box.getX() - 14, (float) box.getY() + 11 });
            g.setColour (proc.direct.isSlotLive (i) ? c::go : c::inkSoft.withAlpha (0.3f));
            g.fillEllipse (led);

            juce::StringArray used;
            for (int t = 0; t < kNumTracks; ++t) if (proc.ti (t, pPort) == i + 1) used.add ("T" + juce::String (t + 1));
            g.setFont (pixel (10.0f));
            g.setColour (c::inkSoft);
            g.drawText (used.isEmpty() ? juce::String ("-") : used.joinIntoString (" "), box.getRight() + 8, box.getY(), kColumnW - kBoxW - 58, 22,
                        juce::Justification::centredLeft, true);
        }
        auto r = juce::Rectangle<int> (0, 284, getWidth() - 8, getHeight() - 284);
        struct Note { const char* head; const char* body; };
        for (auto n : { Note { "HARDWARE", "Assign the ports of your MIDI interface above, then choose Port and Channel on each track." },
                        Note { "PLUGINS IN ABLETON LIVE", "Choose a virtual port. In Live's MIDI settings set Track to On for that input, then on each "
                                                          "instrument track pick it under MIDI From, with the track's channel, and set Monitor to In." },
                        Note { "HOST", "Live merges the plugin's output onto one channel. Use it for a single receiving track, such as a Drum Rack." } })
        {
            g.setColour (c::ink);
            g.setFont (bold (11.0f, 0.06f));
            g.drawText (n.head, r.removeFromTop (17), juce::Justification::left);
            g.setFont (medium (13.0f));
            g.drawFittedText (n.body, r.removeFromTop (38), juce::Justification::topLeft, 2, 1.0f);
            r.removeFromTop (6);
        }
    }

    void resized() override
    {
        for (int i = 0; i < kNumSlots; ++i) boxes[i]->setBounds (44 + (i / 8) * kColumnW, 48 + i % 8 * 28, kBoxW, 22);
        rescan.setBounds (getWidth() - 100, 2, 96, 30);
    }

private:
    static constexpr int kColumnW = 412, kBoxW = 258;
    SeggProcessor& proc;
    juce::OwnedArray<juce::ComboBox> boxes;
    juce::StringArray deviceIds;
    juce::TextButton rescan;
};
} // namespace segg
