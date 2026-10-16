#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour bg       { 0xff15171c };
const juce::Colour panel    { 0xff1f232b };
const juce::Colour raised   { 0xff2a2f39 };
const juce::Colour text     { 0xffc9ced8 };
const juce::Colour dim      { 0xff6d7584 };
const juce::Colour accent   { 0xffff7a45 };   // orange
const juce::Colour accent2  { 0xff3fd0c9 };   // teal

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
};
}

class CurveButton : public juce::Component
{
public:
    explicit CurveButton (int shapeId) : id (shapeId) { setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick (id); }
    std::function<void (int)> onClick;
    bool selected = false;
private:
    int id;
};

class CurveDisplay : public juce::Component
{
public:
    explicit CurveDisplay (PulseDuckProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    PulseDuckProcessor& proc;
};

class PulseDuckEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PulseDuckEditor (PulseDuckProcessor&);
    ~PulseDuckEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setParam (const juce::String& id, float plainValue);
    int  getInt (const juce::String& id) const;

    PulseDuckProcessor& proc;
    ui::LookAndFeel lnf;

    juce::OwnedArray<juce::TextButton> rateButtons, modeButtons;
    juce::TextButton prevShape { "<" }, nextShape { ">" };
    juce::OwnedArray<CurveButton> shapeButtons;
    CurveDisplay display;

    juce::Slider mixKnob, freqSlider;
    juce::TextButton bandButton { "BAND" }, listenButton { "LISTEN" };
    juce::Label shapeName;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> mixAtt, freqAtt;
    std::unique_ptr<BA> bandAtt, listenAtt;

    juce::Rectangle<int> knobArea, mixLabelArea, freqLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulseDuckEditor)
};
