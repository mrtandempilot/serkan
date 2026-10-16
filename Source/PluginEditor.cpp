#include "PluginEditor.h"

using namespace juce;

static Font boldFont (float h) { return Font (FontOptions (h, Font::bold)); }

static Path shapePath (int id, Rectangle<float> r, int steps = 64)
{
    Path p;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float g = shapes::evaluate (id, t);
        const float x = r.getX() + t * r.getWidth();
        const float y = r.getBottom() - g * r.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}

// ------------------------------------------------------------------ LookAndFeel
ui::LookAndFeel::LookAndFeel()
{
    setColour (Label::textColourId, text);
    setColour (Slider::textBoxTextColourId, text);
}

void ui::LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, Slider&)
{
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (10.0f);
    const float size = jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float radius = size * 0.5f;
    const float angle = start + pos * (end - start);
    const float thick = radius * 0.10f;

    Path track;
    track.addCentredArc (c.x, c.y, radius - thick, radius - thick, 0.0f, start, end, true);
    g.setColour (raised);
    g.strokePath (track, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

    Path val;
    val.addCentredArc (c.x, c.y, radius - thick, radius - thick, 0.0f, start, angle, true);
    g.setColour (accent);
    g.strokePath (val, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

    const float inner = radius * 0.70f;
    g.setGradientFill (ColourGradient (raised.brighter (0.15f), c.x, c.y - inner,
                                       panel.darker (0.3f), c.x, c.y + inner, false));
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);

    // duck-bill pointer
    Path bill;
    bill.addRoundedRectangle (-inner * 0.12f, -inner * 0.95f, inner * 0.24f, inner * 0.45f, inner * 0.12f);
    g.setColour (accent);
    g.fillPath (bill, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void ui::LookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        Slider::SliderStyle, Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    auto track = Rectangle<float> ((float) x, cy - 3.0f, (float) w, 6.0f);
    g.setColour (raised);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (accent2);
    g.fillRoundedRectangle (track.withRight (pos), 3.0f);
    g.setColour (text);
    g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    g.setColour (bg);
    g.fillEllipse (pos - 3.0f, cy - 3.0f, 6.0f, 6.0f);
}

void ui::LookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const auto onColour = b.getProperties()["teal"] ? accent2 : accent;
    g.setColour (on ? onColour : (over ? raised.brighter (0.08f) : raised));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
}

void ui::LookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (boldFont (14.0f));
    g.setColour (b.getToggleState() ? bg : text);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

// ------------------------------------------------------------------ CurveButton
void CurveButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (selected ? ui::accent : (isMouseOver() ? ui::raised.brighter (0.08f) : ui::raised));
    g.fillRoundedRectangle (r, 5.0f);

    auto area = r.reduced (r.getWidth() * 0.2f, r.getHeight() * 0.22f);
    g.setColour (selected ? ui::bg : ui::text);
    g.strokePath (shapePath (id, area, 32), PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

// ------------------------------------------------------------------ CurveDisplay
void CurveDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 8.0f);

    auto plot = r.reduced (10.0f, 12.0f);

    // grid
    g.setColour (ui::raised);
    for (int i = 1; i < 4; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 4.0f;
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }

    // input waveform (mirrored peaks)
    const float mid = plot.getCentreY();
    g.setColour (ui::dim.withAlpha (0.7f));
    for (int i = 0; i < PulseDuckProcessor::historySize; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) PulseDuckProcessor::historySize;
        const float a = jmin (1.0f, proc.inputHistory[(size_t) i].load()) * plot.getHeight() * 0.45f;
        g.drawVerticalLine ((int) x, mid - a, mid + a);
    }

    // curve
    const int shapeId = (int) proc.apvts.getRawParameterValue ("shape")->load();
    const float mix   = proc.apvts.getRawParameterValue ("mix")->load() * 0.01f;
    Path curve;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = (float) i / 200.0f;
        const float gval = 1.0f - mix * (1.0f - shapes::evaluate (shapeId, t));
        const float x = plot.getX() + t * plot.getWidth();
        const float y = plot.getBottom() - gval * plot.getHeight();
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    Path fill (curve);
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (ui::accent.withAlpha (0.22f), 0, plot.getY(),
                                       ui::accent.withAlpha (0.0f), 0, plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (ui::accent);
    g.strokePath (curve, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // playhead
    if (proc.uiActive.load())
    {
        const float x = plot.getX() + proc.uiPhase.load() * plot.getWidth();
        g.setColour (ui::text.withAlpha (0.85f));
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.5f);
    }
}

// ------------------------------------------------------------------ Editor
PulseDuckEditor::PulseDuckEditor (PulseDuckProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);

    const StringArray rates { "1/8", "1/4", "1/2", "1/1" };
    for (int i = 0; i < rates.size(); ++i)
    {
        auto* b = rateButtons.add (new TextButton (rates[i]));
        b->onClick = [this, i] { setParam ("rate", (float) i); };
        addAndMakeVisible (b);
    }
    const StringArray modes { "SYNC", "MIDI", "AUDIO" };
    for (int i = 0; i < modes.size(); ++i)
    {
        auto* b = modeButtons.add (new TextButton (modes[i]));
        b->getProperties().set ("teal", true);
        b->onClick = [this, i] { setParam ("mode", (float) i); };
        addAndMakeVisible (b);
    }

    for (int i = 0; i < shapes::count; ++i)
    {
        auto* s = shapeButtons.add (new CurveButton (i));
        s->onClick = [this] (int id) { setParam ("shape", (float) id); };
        addAndMakeVisible (s);
    }
    prevShape.onClick = [this] { setParam ("shape", (float) ((getInt ("shape") + shapes::count - 1) % shapes::count)); };
    nextShape.onClick = [this] { setParam ("shape", (float) ((getInt ("shape") + 1) % shapes::count)); };
    addAndMakeVisible (prevShape);
    addAndMakeVisible (nextShape);

    shapeName.setJustificationType (Justification::centred);
    shapeName.setFont (boldFont (14.0f));
    shapeName.setColour (Label::textColourId, ui::dim);
    addAndMakeVisible (shapeName);

    addAndMakeVisible (display);

    mixKnob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    mixKnob.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (mixKnob);
    mixAtt = std::make_unique<SA> (proc.apvts, "mix", mixKnob);

    freqSlider.setSliderStyle (Slider::LinearHorizontal);
    freqSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (freqSlider);
    freqAtt = std::make_unique<SA> (proc.apvts, "bandFreq", freqSlider);

    bandButton.setClickingTogglesState (true);
    bandButton.getProperties().set ("teal", true);
    listenButton.setClickingTogglesState (true);
    addAndMakeVisible (bandButton);
    addAndMakeVisible (listenButton);
    bandAtt   = std::make_unique<BA> (proc.apvts, "band", bandButton);
    listenAtt = std::make_unique<BA> (proc.apvts, "listen", listenButton);

    setSize (900, 460);
    startTimerHz (30);
}

PulseDuckEditor::~PulseDuckEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int PulseDuckEditor::getInt (const String& id) const
{
    return (int) proc.apvts.getRawParameterValue (id)->load();
}

void PulseDuckEditor::setParam (const String& id, float plain)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void PulseDuckEditor::timerCallback()
{
    const int rate = getInt ("rate"), mode = getInt ("mode"), shape = getInt ("shape");
    for (int i = 0; i < rateButtons.size(); ++i) rateButtons[i]->setToggleState (i == rate, dontSendNotification);
    for (int i = 0; i < modeButtons.size(); ++i) modeButtons[i]->setToggleState (i == mode, dontSendNotification);
    for (int i = 0; i < shapeButtons.size(); ++i)
    {
        const bool sel = (i == shape);
        if (shapeButtons[i]->selected != sel) { shapeButtons[i]->selected = sel; shapeButtons[i]->repaint(); }
    }
    shapeName.setText (shapes::name (shape), dontSendNotification);
    freqSlider.setEnabled (bandButton.getToggleState());
    listenButton.setEnabled (bandButton.getToggleState());
    display.repaint();
    repaint (mixLabelArea);
    repaint (freqLabelArea);
}

void PulseDuckEditor::paint (Graphics& g)
{
    g.fillAll (ui::bg);

    // left panel
    auto left = getLocalBounds().removeFromLeft (290).reduced (12).toFloat();
    g.setColour (ui::panel);
    g.fillRoundedRectangle (left, 10.0f);

    // title
    g.setColour (ui::text);
    g.setFont (boldFont (26.0f));
    g.drawText ("SERKAN DUCK", 24, 22, 260, 30, Justification::left);
    g.setColour (ui::accent);
    g.setFont (boldFont (14.0f));
    g.drawText ("PRO", 24, 50, 260, 18, Justification::left);

    g.setColour (ui::text);
    g.setFont (boldFont (16.0f));
    g.drawText ("MIX " + String (proc.apvts.getRawParameterValue ("mix")->load(), 0) + "%",
                mixLabelArea, Justification::centred);

    g.setColour (bandButton.getToggleState() ? ui::text : ui::dim);
    g.setFont (boldFont (13.0f));
    g.drawText (String (proc.apvts.getRawParameterValue ("bandFreq")->load(), 0) + " Hz",
                freqLabelArea, Justification::centredRight);
}

void PulseDuckEditor::resized()
{
    auto area = getLocalBounds();

    // left column
    auto left = area.removeFromLeft (290).reduced (24);
    left.removeFromTop (56);
    knobArea = left.removeFromTop (220);
    mixKnob.setBounds (knobArea);
    mixLabelArea = left.removeFromTop (28);
    left.removeFromTop (14);
    auto bandRow = left.removeFromTop (34);
    bandButton.setBounds (bandRow.removeFromLeft (90));
    bandRow.removeFromLeft (8);
    listenButton.setBounds (bandRow.removeFromLeft (90));
    freqLabelArea = bandRow;
    left.removeFromTop (6);
    freqSlider.setBounds (left.removeFromTop (30));

    // right column
    auto right = area.reduced (12);
    auto top = right.removeFromTop (38);
    for (auto* b : rateButtons) { b->setBounds (top.removeFromLeft (64)); top.removeFromLeft (4); }
    for (int i = modeButtons.size(); --i >= 0;) { modeButtons[i]->setBounds (top.removeFromRight (80)); top.removeFromRight (4); }

    right.removeFromTop (8);
    auto grid = right.removeFromBottom (110);
    right.removeFromBottom (6);
    auto nav = right.removeFromBottom (34);
    right.removeFromBottom (8);
    display.setBounds (right);

    prevShape.setBounds (nav.removeFromLeft (64));
    nextShape.setBounds (nav.removeFromRight (64));
    shapeName.setBounds (nav);

    const int cols = 8, rows = 2;
    const int cw = grid.getWidth() / cols, ch = grid.getHeight() / rows;
    for (int i = 0; i < shapeButtons.size(); ++i)
        shapeButtons[i]->setBounds (grid.getX() + (i % cols) * cw, grid.getY() + (i / cols) * ch, cw, ch);
}
