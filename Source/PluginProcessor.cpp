#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

PulseDuckProcessor::PulseDuckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& h : inputHistory) h.store (0.0f);

    rateP     = apvts.getRawParameterValue ("rate");
    modeP     = apvts.getRawParameterValue ("mode");
    shapeP    = apvts.getRawParameterValue ("shape");
    mixP      = apvts.getRawParameterValue ("mix");
    bandP     = apvts.getRawParameterValue ("band");
    bandFreqP = apvts.getRawParameterValue ("bandFreq");
    listenP   = apvts.getRawParameterValue ("listen");
}

AudioProcessorValueTreeState::ParameterLayout PulseDuckProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Rate",
                                                         StringArray { "1/8", "1/4", "1/2", "1/1" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Trigger",
                                                         StringArray { "Sync", "MIDI", "Audio" }, 0));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "shape", 1 }, "Shape", 0, shapes::count - 1, 2));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                        AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "band", 1 }, "Band", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bandFreq", 1 }, "Band Freq",
                                                        NormalisableRange<float> (30.0f, 1000.0f, 0.1f, 0.4f), 150.0f,
                                                        AudioParameterFloatAttributes().withLabel ("Hz")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "listen", 1 }, "Listen", false));
    return { p.begin(), p.end() };
}

double PulseDuckProcessor::beatsForRate (int i)
{
    static const double beats[] = { 0.5, 1.0, 2.0, 4.0 };
    return beats[jlimit (0, 3, i)];
}

void PulseDuckProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    smoothCoef = 1.0f - std::exp (-1.0f / (0.0003f * (float) sr));   // ~0.3 ms de-click
    fastRel    = std::exp (-1.0f / (0.020f * (float) sr));
    slowAtt    = std::exp (-1.0f / (0.030f * (float) sr));
    slowRel    = std::exp (-1.0f / (0.200f * (float) sr));
    cooldownSamples = (int) (0.06 * sr);

    dsp::ProcessSpec spec { sr, (uint32) samplesPerBlock, 2 };
    lowpass.prepare (spec);
    highpass.prepare (spec);
    lowpass.setType (dsp::LinkwitzRileyFilterType::lowpass);
    highpass.setType (dsp::LinkwitzRileyFilterType::highpass);
    currentCutoff = -1.0f;

    gains.assign ((size_t) jmax (samplesPerBlock, 1), 1.0f);
    smoothedGain = 1.0f;
    envFast = envSlow = 0.0f;
    cooldown = 0;
    phase = 1.0;
    running = false;
}

bool PulseDuckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != out) return false;

    if (layouts.inputBuses.size() > 1)
    {
        auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != AudioChannelSet::mono() && sc != AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void PulseDuckProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (n == 0) return;
    if ((int) gains.size() < n) gains.resize ((size_t) n, 1.0f);

    auto main = getBusBuffer (buffer, true, 0);
    const int numCh = main.getNumChannels();

    AudioBuffer<float> side;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
        side = getBusBuffer (buffer, true, 1);
    const bool hasSide = side.getNumChannels() > 0;

    // --- host timing ---
    double bpm = lastBpm;
    std::optional<double> ppq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    if (bpm < 20.0 || bpm > 999.0) bpm = 120.0;
    lastBpm = bpm;

    const int mode     = (int) modeP->load();
    const int shapeId  = (int) shapeP->load();
    const float mix    = mixP->load() * 0.01f;
    const double bpp   = beatsForRate ((int) rateP->load());
    const double inc   = (bpm / 60.0) / bpp / sr;

    if (mode == modeSync && playing && ppq.has_value())
    {
        phase = std::fmod (*ppq / bpp, 1.0);
        if (phase < 0.0) phase += 1.0;
    }

    auto midiIt = midi.cbegin();

    for (int s = 0; s < n; ++s)
    {
        // MIDI triggers (sample accurate)
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= s)
        {
            if (mode == modeMidi && (*midiIt).getMessage().isNoteOn())
                trigger();
            ++midiIt;
        }

        float inPeak = 0.0f;
        for (int c = 0; c < numCh; ++c)
            inPeak = jmax (inPeak, std::abs (main.getSample (c, s)));

        // Audio transient triggers
        if (mode == modeAudio)
        {
            float det = 0.0f;
            if (hasSide)
                for (int c = 0; c < side.getNumChannels(); ++c)
                    det = jmax (det, std::abs (side.getSample (c, s)));
            else
                det = inPeak;

            envFast = det > envFast ? det : det + (envFast - det) * fastRel;
            const float k = det > envSlow ? slowAtt : slowRel;
            envSlow = det + (envSlow - det) * k;

            if (cooldown > 0) --cooldown;
            else if (envFast > 0.02f && envFast > envSlow * 1.6f)
            {
                trigger();
                cooldown = cooldownSamples;
            }
        }

        // Shape value
        float target;
        bool active = true;
        if (mode == modeSync)
        {
            target = shapes::evaluate (shapeId, (float) phase);
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
        else
        {
            if (running)
            {
                target = shapes::evaluate (shapeId, (float) phase);
                phase += inc;
                if (phase >= 1.0) { phase = 1.0; running = false; }
            }
            else
            {
                target = 1.0f;
                active = false;
            }
        }

        target = 1.0f - mix * (1.0f - target);
        smoothedGain += (target - smoothedGain) * smoothCoef;
        gains[(size_t) s] = smoothedGain;

        // waveform history for the display, binned by phase
        const int bin = jlimit (0, historySize - 1, (int) (phase * historySize));
        if (bin != lastBin)
        {
            if (lastBin >= 0) inputHistory[(size_t) lastBin].store (binPeak);
            lastBin = bin;
            binPeak = 0.0f;
        }
        binPeak = jmax (binPeak, inPeak);
        uiActive.store (active);
    }

    uiPhase.store ((float) phase);

    // --- apply ---
    const bool band   = bandP->load() > 0.5f;
    const bool listen = listenP->load() > 0.5f;

    if (! band)
    {
        for (int c = 0; c < numCh; ++c)
        {
            auto* d = main.getWritePointer (c);
            for (int s = 0; s < n; ++s) d[s] *= gains[(size_t) s];
        }
        return;
    }

    const float cutoff = bandFreqP->load();
    if (std::abs (cutoff - currentCutoff) > 0.01f)
    {
        lowpass.setCutoffFrequency (cutoff);
        highpass.setCutoffFrequency (cutoff);
        currentCutoff = cutoff;
    }

    for (int c = 0; c < numCh; ++c)
    {
        auto* d = main.getWritePointer (c);
        for (int s = 0; s < n; ++s)
        {
            const float x  = d[s];
            const float lo = lowpass.processSample (c, x)  * gains[(size_t) s];
            const float hi = highpass.processSample (c, x);
            d[s] = listen ? lo : lo + hi;
        }
    }
}

AudioProcessorEditor* PulseDuckProcessor::createEditor()
{
    return new PulseDuckEditor (*this);
}

void PulseDuckProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PulseDuckProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PulseDuckProcessor();
}
