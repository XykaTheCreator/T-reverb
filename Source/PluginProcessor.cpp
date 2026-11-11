#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using PID = juce::ParameterID;
using NR  = juce::NormalisableRange<float>;

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;

    Attr percentAttr()
    {
        return Attr().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })
                     .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue() / 100.0f; });
    }
    Attr secondsAttr()
    {
        return Attr().withStringFromValueFunction([](float v, int) { return juce::String(v, v < 10.0f ? 2 : 1) + " s"; })
                     .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue(); });
    }
    Attr sizeAttr()
    {
        return Attr().withStringFromValueFunction([](float v, int) { return juce::String(v, 2) + "x"; })
                     .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue(); });
    }
    Attr msAttr()
    {
        return Attr().withStringFromValueFunction([](float v, int) { return juce::String(v, v < 10.0f ? 1 : 0) + " ms"; })
                     .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue(); });
    }
    Attr hzAttr()
    {
        return Attr().withStringFromValueFunction([](float v, int)
                     { return v >= 1000.0f ? juce::String(v / 1000.0f, 1) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; })
                     .withValueFromStringFunction([](const juce::String& t)
                     { return t.containsIgnoreCase("k") ? t.getFloatValue() * 1000.0f : t.getFloatValue(); });
    }

    struct Preset
    {
        const char* name;
        int mode;
        float mix, decay, size, pre, damp, diff, mod, width, low, high, shim, duck, rel;
    };

    const Preset kPresets[] =
    {
        //  isim              mode  mix   decay size  pre  damp   diff  mod   width low   high   shim  duck  rel
        { "Init",             1,   0.30f, 3.0f, 1.0f, 10,  8000, 0.60f, 0.30f, 1.0f,  20, 20000, 0.0f, 0.0f, 250 },
        { "Vocal Plate",      2,   0.22f, 1.8f, 0.9f, 20,  9000, 0.70f, 0.40f, 1.0f, 150, 10000, 0.0f, 0.3f, 200 },
        { "Ducked Vocal",     2,   0.35f, 3.5f, 1.1f, 30,  8000, 0.70f, 0.40f, 1.0f, 200, 12000, 0.0f, 0.7f, 300 },
        { "Small Room",       0,   0.20f, 0.6f, 0.8f,  0,  7000, 0.60f, 0.20f, 0.8f, 100, 12000, 0.0f, 0.0f, 250 },
        { "Dark Drums",       0,   0.25f, 1.2f, 1.0f,  0,  3500, 0.60f, 0.20f, 0.8f, 200,  6000, 0.0f, 0.0f, 250 },
        { "Warm Hall",        1,   0.30f, 2.8f, 1.0f, 15,  5000, 0.65f, 0.30f, 1.0f,  80,  9000, 0.0f, 0.0f, 250 },
        { "Big Cathedral",    3,   0.35f, 8.0f, 1.4f, 30,  6000, 0.70f, 0.35f, 1.0f, 120, 10000, 0.0f, 0.0f, 250 },
        { "Shimmer Pad",      1,   0.40f,10.0f, 1.5f, 25,  9000, 0.70f, 0.50f, 1.0f, 150, 16000, 0.6f, 0.0f, 250 },
        { "Ambient Space",    3,   0.50f,20.0f, 2.0f, 40,  5000, 0.75f, 0.60f, 1.0f, 200,  8000, 0.3f, 0.0f, 250 },
    };
    constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
}

int SuperReverbProcessor::getNumPresets() { return kNumPresets; }
juce::String SuperReverbProcessor::getPresetName(int i) { return juce::String(kPresets[juce::jlimit(0, kNumPresets - 1, i)].name); }

void SuperReverbProcessor::applyPreset(int i)
{
    if (i < 0 || i >= kNumPresets) return;
    const auto& p = kPresets[i];
    auto set = [this](const char* id, float v)
    {
        if (auto* prm = apvts.getParameter(id))
            prm->setValueNotifyingHost(prm->convertTo0to1(v));
    };
    set("mode", (float) p.mode);   set("mix", p.mix);        set("decay", p.decay);
    set("size", p.size);           set("predelay", p.pre);   set("damping", p.damp);
    set("diffusion", p.diff);      set("mod", p.mod);        set("width", p.width);
    set("lowcut", p.low);          set("highcut", p.high);   set("shimmer", p.shim);
    set("duck", p.duck);           set("duckrel", p.rel);    set("freeze", 0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout SuperReverbProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(PID{"mode", 1}, "Mode",
        juce::StringArray{ "Room", "Hall", "Plate", "Cathedral" }, 1));

    params.push_back(std::make_unique<APF>(PID{"mix", 1},       "Mix",           NR(0.0f, 1.0f, 0.001f), 0.30f, percentAttr()));
    params.push_back(std::make_unique<APF>(PID{"decay", 1},     "Decay (s)",     NR(0.2f, 30.0f, 0.01f, 0.4f), 3.0f, secondsAttr()));
    params.push_back(std::make_unique<APF>(PID{"size", 1},      "Size",          NR(0.5f, 2.0f, 0.001f), 1.0f, sizeAttr()));
    params.push_back(std::make_unique<APF>(PID{"predelay", 1},  "Pre-Delay (ms)",NR(0.0f, 200.0f, 0.1f, 0.5f), 10.0f, msAttr()));
    params.push_back(std::make_unique<APF>(PID{"damping", 1},   "Damping (Hz)",  NR(1000.0f, 20000.0f, 1.0f, 0.3f), 8000.0f, hzAttr()));
    params.push_back(std::make_unique<APF>(PID{"diffusion", 1}, "Diffusion",     NR(0.0f, 0.75f, 0.001f), 0.6f, percentAttr()));
    params.push_back(std::make_unique<APF>(PID{"mod", 1},       "Modulation",    NR(0.0f, 1.0f, 0.001f), 0.3f, percentAttr()));
    params.push_back(std::make_unique<APF>(PID{"width", 1},     "Width",         NR(0.0f, 1.0f, 0.001f), 1.0f, percentAttr()));
    params.push_back(std::make_unique<APF>(PID{"lowcut", 1},    "Low Cut (Hz)",  NR(20.0f, 1000.0f, 1.0f, 0.4f), 20.0f, hzAttr()));
    params.push_back(std::make_unique<APF>(PID{"highcut", 1},   "High Cut (Hz)", NR(1000.0f, 20000.0f, 1.0f, 0.3f), 20000.0f, hzAttr()));
    params.push_back(std::make_unique<APF>(PID{"shimmer", 1},   "Shimmer",       NR(0.0f, 1.0f, 0.001f), 0.0f, percentAttr()));
    params.push_back(std::make_unique<APF>(PID{"duck", 1},      "Duck",          NR(0.0f, 1.0f, 0.001f), 0.0f, percentAttr()));
    params.push_back(std::make_unique<APF>(PID{"duckrel", 1},   "Duck Release (ms)", NR(50.0f, 1000.0f, 1.0f, 0.5f), 250.0f, msAttr()));

    params.push_back(std::make_unique<juce::AudioParameterBool>(PID{"freeze", 1}, "Freeze", false));

    return { params.begin(), params.end() };
}

SuperReverbProcessor::SuperReverbProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
}

void SuperReverbProcessor::prepareToPlay(double sampleRate, int)
{
    reverb.prepare(sampleRate);
}

bool SuperReverbProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SuperReverbProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    FDNReverb::Params p;
    p.mode       = (int) get("mode");
    p.mix        = get("mix");
    p.decay      = get("decay");
    p.size       = get("size");
    p.preDelayMs = get("predelay");
    p.dampHz     = get("damping");
    p.diffusion  = get("diffusion");
    p.mod        = get("mod");
    p.width      = get("width");
    p.lowCutHz   = get("lowcut");
    p.highCutHz  = get("highcut");
    p.shimmer    = get("shimmer");
    p.duck       = get("duck");
    p.duckRelMs  = get("duckrel");
    p.freeze     = get("freeze") > 0.5f;
    reverb.setParams(p);

    reverb.process(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
}

juce::AudioProcessorEditor* SuperReverbProcessor::createEditor()
{
    return new SuperReverbEditor(*this);
}

void SuperReverbProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SuperReverbProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuperReverbProcessor();
}
