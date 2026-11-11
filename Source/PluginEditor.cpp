#include "PluginEditor.h"

using namespace ui;

static juce::Font makeFont(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}

// ===========================================================================
// LookAndFeel
// ===========================================================================
SuperLookAndFeel::SuperLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::outlineColourId, line);
    setColour(juce::ComboBox::textColourId, textMain);
    setColour(juce::ComboBox::arrowColourId, accent);
    setColour(juce::ComboBox::focusedOutlineColourId, accent);

    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff151a22));
    setColour(juce::PopupMenu::textColourId, textMain);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::buttonOnColourId, accent2.withAlpha(0.85f));
    setColour(juce::TextButton::textColourOffId, textDim);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    setColour(juce::Label::textColourId, textMain);
}

void SuperLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                        float pos, float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(5.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = juce::jmax(3.0f, radius * 0.14f);
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto colour = s.findColour(juce::Slider::rotarySliderFillColourId);

    juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // Arka yay
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff232936));
    g.strokePath(track, stroke);

    // Değer yayı (hafif parlama + ana çizgi)
    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour(colour.withAlpha(0.22f));
        g.strokePath(value, juce::PathStrokeType(lineW + 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colour);
        g.strokePath(value, stroke);
    }

    // Knob gövdesi
    const float bodyR = arcR - lineW * 1.3f;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2d3441), centre.x, centre.y - bodyR,
                                           juce::Colour(0xff171b23), centre.x, centre.y + bodyR, false));
    g.fillEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour(juce::Colour(0xff0c0f14));
    g.drawEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // İşaretçi çizgi
    juce::Path pointer;
    pointer.addRoundedRectangle(-1.6f, -bodyR + 4.0f, 3.2f, bodyR * 0.42f, 1.6f);
    g.setColour(juce::Colours::white);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

juce::Label* SuperLookAndFeel::createSliderTextBox(juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(s);
    l->setFont(makeFont(12.5f));
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, textMain);
    l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

// ===========================================================================
// DecayView
// ===========================================================================
DecayView::DecayView(juce::AudioProcessorValueTreeState& state) : apvts(state)
{
    startTimerHz(30);
}

void DecayView::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(r, 12.0f);
    g.setColour(line);
    g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, 1.0f);

    const float decay = val("decay");
    const float pre   = val("predelay") * 0.001f;
    const float damp  = val("damping");
    const float diff  = val("diffusion");
    const float shim  = val("shimmer");
    const float mix   = val("mix");
    const bool freeze = val("freeze") > 0.5f;

    auto area = r.reduced(20.0f, 14.0f);
    area.removeFromTop(14.0f); // üst yazılar için boşluk

    const float span = juce::jlimit(2.0f, 30.0f, decay * 1.25f);
    const int n = 84;
    const float slot = area.getWidth() / (float) n;
    const float barW = slot * 0.55f;

    const float bright = juce::jmap(damp, 1000.0f, 20000.0f, 0.0f, 1.0f);
    const auto tint = accent2.interpolatedWith(accent, bright);
    const float jitter = 0.08f + 0.5f * (1.0f - diff / 0.75f);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1) * span;
        float h = 0.0f;

        if (i == 0)
            h = 1.0f; // kuru vuruş
        else if (t >= pre)
            h = freeze ? 0.8f : 0.85f * std::pow(juce::jmax(0.0f, 1.0f - (t - pre) / decay), 1.5f);

        if (i > 0)
        {
            const float rnd = std::sin((float) i * 12.9898f) * 43758.5453f;
            const float r01 = rnd - std::floor(rnd);
            const float wob = 0.5f + 0.5f * std::sin(phase * 1.5f + (float) i * 0.7f);
            h *= 1.0f - jitter * r01 * (0.4f + 0.6f * wob);
        }
        if (h <= 0.004f) continue;

        const float barH = h * area.getHeight();
        juce::Rectangle<float> bar(area.getX() + (float) i * slot + (slot - barW) * 0.5f,
                                   area.getBottom() - barH, barW, barH);

        g.setColour(i == 0 ? juce::Colours::white.withAlpha(0.9f)
                           : tint.withAlpha((0.55f + 0.45f * mix) * (0.55f + 0.45f * h)));
        g.fillRoundedRectangle(bar, barW * 0.4f);

        // Shimmer: barların üstünde yükselen parıltılar
        if (shim > 0.01f && i > 0 && i % 3 == 0)
        {
            const float yy = bar.getY() - 7.0f - 6.0f * std::sin(phase * 2.0f + (float) i);
            g.setColour(accent2.brighter(0.6f).withAlpha(shim * (0.55f + 0.45f * h)));
            g.fillEllipse(bar.getCentreX() - 2.4f, yy, 4.8f, 4.8f);
        }
    }

    g.setColour(textDim);
    g.setFont(makeFont(11.5f, true));
    auto top = r.reduced(20.0f, 10.0f).removeFromTop(16.0f);
    g.drawText("RT60  " + apvts.getParameter("decay")->getCurrentValueAsText(), top,
               juce::Justification::centredLeft);
    g.drawText(freeze ? "FROZEN" : apvts.getParameter("mode")->getCurrentValueAsText().toUpperCase(),
               top, juce::Justification::centredRight);
}

// ===========================================================================
// ModeSelector
// ===========================================================================
static const char* kModeNames[4] = { "Room", "Hall", "Plate", "Cathedral" };

ModeSelector::ModeSelector(juce::RangedAudioParameter& param)
    : attachment(param, [this](float v) { selected = juce::jlimit(0, 3, juce::roundToInt(v)); repaint(); })
{
    attachment.sendInitialUpdate();
}

int ModeSelector::indexAt(juce::Point<float> p) const
{
    return juce::jlimit(0, 3, (int) (p.x / ((float) getWidth() / 4.0f)));
}

void ModeSelector::mouseDown(const juce::MouseEvent& e)
{
    attachment.setValueAsCompleteGesture((float) indexAt(e.position));
}

void ModeSelector::mouseMove(const juce::MouseEvent& e)
{
    const int i = indexAt(e.position);
    if (i != hover) { hover = i; repaint(); }
}

void ModeSelector::mouseExit(const juce::MouseEvent&)
{
    hover = -1; repaint();
}

void ModeSelector::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(line);
    g.drawRoundedRectangle(r.reduced(0.5f), r.getHeight() * 0.5f, 1.0f);

    const float w = r.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto cell = juce::Rectangle<float>(r.getX() + (float) i * w, r.getY(), w, r.getHeight()).reduced(4.0f);

        if (i == selected)
        {
            g.setColour(accent.withAlpha(0.16f));
            g.fillRoundedRectangle(cell, cell.getHeight() * 0.5f);
            g.setColour(accent.withAlpha(0.9f));
            g.drawRoundedRectangle(cell.reduced(0.5f), cell.getHeight() * 0.5f, 1.2f);
        }
        else if (i == hover)
        {
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.fillRoundedRectangle(cell, cell.getHeight() * 0.5f);
        }

        g.setColour(i == selected ? juce::Colours::white : textDim);
        g.setFont(makeFont(13.5f, i == selected));
        g.drawText(kModeNames[i], cell, juce::Justification::centred);
    }
}

// ===========================================================================
// Editor
// ===========================================================================
void SuperReverbEditor::addKnob(Knob& k, const juce::String& paramId, const juce::String& text, juce::Colour colour)
{
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 18);
    k.slider.setTextBoxIsEditable(false);
    k.slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
    k.slider.setColour(juce::Slider::textBoxTextColourId, textMain);
    k.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    k.label.setText(text, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setFont(makeFont(11.0f, true));
    k.label.setColour(juce::Label::textColourId, textDim);

    addAndMakeVisible(k.slider);
    addAndMakeVisible(k.label);
    k.att = std::make_unique<SliderAtt>(proc.apvts, paramId, k.slider);
}

SuperReverbEditor::SuperReverbEditor(SuperReverbProcessor& p)
    : AudioProcessorEditor(&p), proc(p), view(p.apvts), modeSelector(*p.apvts.getParameter("mode"))
{
    setLookAndFeel(&lnf);
    setSize(760, 560);

    addAndMakeVisible(view);
    addAndMakeVisible(modeSelector);

    for (int i = 0; i < SuperReverbProcessor::getNumPresets(); ++i)
        presetBox.addItem(SuperReverbProcessor::getPresetName(i), i + 1);
    presetBox.setTextWhenNothingSelected("Presets");
    presetBox.onChange = [this]
    {
        const int i = presetBox.getSelectedId() - 1;
        if (i >= 0) proc.applyPreset(i);
    };
    addAndMakeVisible(presetBox);

    freezeBtn.setClickingTogglesState(true);
    addAndMakeVisible(freezeBtn);
    freezeAtt = std::make_unique<ButtonAtt>(proc.apvts, "freeze", freezeBtn);

    addKnob(mix,      "mix",      "MIX",       accent);
    addKnob(decay,    "decay",    "DECAY",     accent);
    addKnob(size,     "size",     "SIZE",      accent);
    addKnob(predelay, "predelay", "PRE-DELAY", accent);
    addKnob(damping,  "damping",  "DAMPING",   accent);
    addKnob(shimmer,  "shimmer",  "SHIMMER",   accent2);

    addKnob(diffusion, "diffusion", "DIFFUSION", accent);
    addKnob(mod,       "mod",       "MOD",       accent);
    addKnob(width,     "width",     "WIDTH",     accent);
    addKnob(lowcut,    "lowcut",    "LOW CUT",   accent);
    addKnob(highcut,   "highcut",   "HIGH CUT",  accent);
    addKnob(duck,      "duck",      "DUCK",      accent2);
    addKnob(duckrel,   "duckrel",   "DUCK REL",  accent2);
}

SuperReverbEditor::~SuperReverbEditor()
{
    setLookAndFeel(nullptr);
}

void SuperReverbEditor::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Üstte hafif turkuaz ışıma
    g.setGradientFill(juce::ColourGradient(accent.withAlpha(0.10f), (float) getWidth() * 0.2f, -40.0f,
                                           accent.withAlpha(0.0f), (float) getWidth() * 0.2f, 220.0f, true));
    g.fillRect(getLocalBounds().removeFromTop(260));

    g.setColour(textMain);
    g.setFont(makeFont(24.0f, true));
    g.drawText("SUPER REVERB", 28, 16, 300, 30, juce::Justification::centredLeft);
    g.setColour(textDim);
    g.setFont(makeFont(12.0f));
    g.drawText("shimmer  /  ducking  /  freeze", 29, 44, 300, 16, juce::Justification::centredLeft);

    g.setColour(line);
    g.drawHorizontalLine(412, 28.0f, (float) getWidth() - 28.0f);
}

void SuperReverbEditor::resized()
{
    auto area = getLocalBounds().reduced(28, 0);

    // Başlık satırı
    auto header = area.removeFromTop(66).withTrimmedTop(16).withTrimmedBottom(14);
    freezeBtn.setBounds(header.removeFromRight(92));
    header.removeFromRight(10);
    presetBox.setBounds(header.removeFromRight(170));

    // Mod seçici
    modeSelector.setBounds(area.removeFromTop(38));
    area.removeFromTop(12);

    // Görsel
    view.setBounds(area.removeFromTop(132));
    area.removeFromTop(10);

    auto place = [](juce::Rectangle<int> cell, Knob& k)
    {
        k.label.setBounds(cell.removeFromTop(16));
        k.slider.setBounds(cell);
    };

    // Büyük knob'lar
    auto rowA = area.removeFromTop(142);
    {
        const int w = rowA.getWidth() / 6;
        Knob* ks[] = { &mix, &decay, &size, &predelay, &damping, &shimmer };
        for (auto* k : ks) place(rowA.removeFromLeft(w), *k);
    }

    area.removeFromTop(12); // ayırıcı çizgi burada

    // Küçük knob'lar
    auto rowB = area.removeFromTop(112);
    {
        const int w = rowB.getWidth() / 7;
        Knob* ks[] = { &diffusion, &mod, &width, &lowcut, &highcut, &duck, &duckrel };
        for (auto* k : ks) place(rowB.removeFromLeft(w), *k);
    }
}
