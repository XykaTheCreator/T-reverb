#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bgTop   (0xff10141b);
    const juce::Colour bgBottom(0xff0a0d12);
    const juce::Colour panel   (0xff0d1016);
    const juce::Colour line    (0xff232936);
    const juce::Colour accent  (0xff3ddcc4); // turkuaz
    const juce::Colour accent2 (0xff8b7bff); // mor
    const juce::Colour textDim (0xff8d97a6);
    const juce::Colour textMain(0xffe8edf3);
}

// ---------------------------------------------------------------------------
// Görünüm: parlayan yay knob'lar, sade combo ve butonlar
// ---------------------------------------------------------------------------
class SuperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SuperLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
};

// ---------------------------------------------------------------------------
// Ortadaki canlı decay görseli (parametrelere göre şekil alır)
// ---------------------------------------------------------------------------
class DecayView : public juce::Component, private juce::Timer
{
public:
    explicit DecayView(juce::AudioProcessorValueTreeState& state);
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override { phase += 0.06f; repaint(); }
    float val(const char* id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::AudioProcessorValueTreeState& apvts;
    float phase = 0.0f;
};

// ---------------------------------------------------------------------------
// Room / Hall / Plate / Cathedral seçici
// ---------------------------------------------------------------------------
class ModeSelector : public juce::Component
{
public:
    explicit ModeSelector(juce::RangedAudioParameter& param);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    int indexAt(juce::Point<float> p) const;

    int selected = 1, hover = -1;
    juce::ParameterAttachment attachment;
};

// ---------------------------------------------------------------------------
class SuperReverbEditor : public juce::AudioProcessorEditor
{
public:
    explicit SuperReverbEditor(SuperReverbProcessor&);
    ~SuperReverbEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    void addKnob(Knob& k, const juce::String& paramId, const juce::String& text, juce::Colour colour);

    SuperLookAndFeel lnf; // ilk üye: en son yok edilir
    SuperReverbProcessor& proc;

    DecayView view;
    ModeSelector modeSelector;
    juce::ComboBox presetBox;
    juce::TextButton freezeBtn { "FREEZE" };
    std::unique_ptr<ButtonAtt> freezeAtt;

    // Büyük knob'lar
    Knob mix, decay, size, predelay, damping, shimmer;
    // Küçük knob'lar
    Knob diffusion, mod, width, lowcut, highcut, duck, duckrel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperReverbEditor)
};
