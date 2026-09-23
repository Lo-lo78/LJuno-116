// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SynthEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace ljuno
{
namespace
{
constexpr float epsilon = 0.0000001f;

// LArp Routing (0..2):
// 0 = Synth + LArp MIDI, 1 = Synth LArp + MIDI Direct, 2 = LArp MIDI Only.
bool larpPlaysSynth (int state)
{
    return state == 0 || state == 1;
}

bool larpSendsArpeggiatedMidi (int state)
{
    return state == 0 || state == 2;
}

bool larpUsesDirectMidiOutput (int state)
{
    return state == 1;
}

float microMotionStationaryScale (float alpha)
{
    // For x[n] = (1-alpha) x[n-1] + alpha U[-1,1], this scale gives a
    // freshly drawn uniform value the same variance as the settled process.
    // Starting at full-scale random would otherwise create a long audible
    // pitch/filter glide before slow Micro Motion reaches its normal range.
    const auto boundedAlpha = juce::jlimit (0.0f, 0.5f, alpha);
    return boundedAlpha > 0.0f
        ? std::sqrt (boundedAlpha / (2.0f - boundedAlpha))
        : 0.0f;
}

float envelopeIncrement (float seconds, double sampleRate)
{
    return seconds <= 0.0f
         ? 1.0f
         : 1.0f / std::max (1.0f, seconds * static_cast<float> (sampleRate));
}

double wrapPhase (double phase)
{
    return phase - std::floor (phase);
}

void filterSinCos (double angle, double& sine, double& cosine)
{
    // Filter cutoffs are clamped below 0.225 of the sample rate, so the angle
    // is always in [0, 0.45*pi]. On this short interval these polynomials are
    // below the precision that survives conversion to float coefficients, and
    // avoid two CRT transcendental calls per active voice and sample.
    const auto x2 = angle * angle;
    sine = angle * (1.0 + x2 * (-1.0 / 6.0
        + x2 * (1.0 / 120.0
        + x2 * (-1.0 / 5040.0
        + x2 * (1.0 / 362880.0
        + x2 * (-1.0 / 39916800.0
        + x2 * (1.0 / 6227020800.0)))))));
    cosine = 1.0 + x2 * (-1.0 / 2.0
        + x2 * (1.0 / 24.0
        + x2 * (-1.0 / 720.0
        + x2 * (1.0 / 40320.0
        + x2 * (-1.0 / 3628800.0
        + x2 * (1.0 / 479001600.0
        + x2 * (-1.0 / 87178291200.0)))))));
}
}

void SynthEngine::prepare (double rate)
{
    sampleRate = rate > 0.0 ? rate : 44100.0;
    voices = {};
    ageCounter = 0;
    rolandVoice = 0;
    sequencerRolandVoice = {};
    sustainPedal = { false, false, false };
    pitchBend = { 0.0f, 0.0f, 0.0f };
    modWheel = { 0.0f, 0.0f, 0.0f };
    channelAftertouch = { 0.0f, 0.0f, 0.0f };
    globalPitchEnvelope1 = globalPitchEnvelope2 = 0.0f;
    pinkLeft = pinkRight = brownLeft = brownRight = 0.0f;
    randomState = 0x1165a17u;
    lfoState1 = {};
    lfoState2 = {};
    monoNoteStack = {};
    monoVelocityStack = {};
    monoNoteCount = 0;
    sourceMonoNoteStack = {};
    sourceMonoVelocityStack = {};
    sourceMonoNoteCount = { 0, 0 };
    pitchArpHeldNotes = {};
    pitchArpLiveSorted = {};
    pitchArpLiveFree = {};
    pitchArpLatchSorted = {};
    pitchArpLatchFree = {};
    pitchArpKeyDown = {};
    pitchArpSustained = {};
    pitchArpHeldCount = pitchArpLiveCount = pitchArpLatchCount = 0;
    pitchArpLatchValid = false;
    pitchArpPoolDirty = true;
    pitchArpState1 = {};
    pitchArpState2 = {};
    pitchArpState2.randomSeed = 1000;
    larpState = {};
    larpState.randomSeed = 0x6c617270u;
    sequencerRuntime = {};
    for (int i = 0; i < static_cast<int> (sequencerRuntime.size()); ++i)
        sequencerRuntime[static_cast<std::size_t> (i)].randomSeed =
            0x51e90001u + static_cast<std::uint32_t> (i * 0x001f123bu);
    previousSequencerMode = 0;
    previousSequencerEnabled = false;
    previousLArpEnabled = false;
    previousSourceMidiChannels = { 0, 0, 0 };
    previousSourceNoteSources = { 0, 0, 0 };
    previousSourceVoiceModes = { 0, 0 };
    previousIndependentVoiceRouting = false;
    parameterCacheReady = false;

    chorusBufferLeft.assign (static_cast<std::size_t> (std::floor (sampleRate * 0.035)) + 4, 0.0f);
    chorusBufferRight.assign (chorusBufferLeft.size(), 0.0f);
    delayBufferLeft.assign (static_cast<std::size_t> (std::floor (sampleRate * 16.0)) + 4, 0.0f);
    delayBufferRight.assign (delayBufferLeft.size(), 0.0f);
    delaySilentSamples = delayBufferLeft.size();
    chorusWritePosition = delayWritePosition = 0;
    chorusPhase = 0.0;
    chorusRateSmoothed = chorusTail = 0.0f;
    chorusHpXLeft = chorusHpYLeft = chorusHpXRight = chorusHpYRight = 0.0f;
    delayTimeCurrent = 0.0f;
    delayLpLeft = delayLpRight = 0.0f;
    delayHpXLeft = delayHpYLeft = delayHpXRight = delayHpYRight = 0.0f;
    activeDelayMode = -1;
    lwsDelayWritePosition = 0;
    lwsDelayMixActive = false;
    lwsDelayTime1 = lwsDelayTime2 = 0.0f;
    lwsDelayWetLp1 = lwsDelayWetLp2 = 0.0f;
    lwsDelayWetHpX1 = lwsDelayWetHpY1 = lwsDelayWetHpX2 = lwsDelayWetHpY2 = 0.0f;
    lwsDelayFbLp1 = lwsDelayFbLp2 = 0.0f;
    lwsDelayFbHpX1 = lwsDelayFbHpY1 = lwsDelayFbHpX2 = lwsDelayFbHpY2 = 0.0f;
    lwsDelayPan1L = 0.9807852804f; lwsDelayPan1R = 0.1950903220f;
    lwsDelayPan2L = 0.1950903220f; lwsDelayPan2R = 0.9807852804f;
    dcXLeft.fill (0.0f); dcYLeft.fill (0.0f);
    dcXRight.fill (0.0f); dcYRight.fill (0.0f);
    eqStemLeft = {};
    eqStemRight = {};
    cachedEqFrequency.fill (-1.0f);
    cachedEqGain.fill (std::numeric_limits<float>::quiet_NaN());
    cachedDelayTone = -1.0f;
    cachedReverbDecay = cachedReverbBassMultiplier = -1.0f;
    cachedReverbXover = cachedReverbDamping = -1.0f;
    effectCoefficientsReady = false;
    compressorState = {};
    reverbCompressorState = {};
    glueEnvelope = 0.0f;
    compressorRmsCoefficient = static_cast<float> (std::exp (-1.0 / (0.01 * sampleRate)));

    reverbPredelayLeft.assign (
        static_cast<std::size_t> (std::floor (0.12 * sampleRate)) + 8, 0.0f);
    reverbPredelayRight.assign (reverbPredelayLeft.size(), 0.0f);
    reverbPredelayWriteLeft = reverbPredelayWriteRight = 0;
    deepIdle = true;
    static constexpr std::array<double, 8> diffuserTimes {
        0.020346, 0.024421, 0.031604, 0.027333,
        0.022904, 0.029291, 0.013458, 0.019123
    };
    static constexpr std::array<double, 8> delayTimes {
        0.153129, 0.210389, 0.127837, 0.256891,
        0.174713, 0.192303, 0.125000, 0.219991
    };
    static constexpr std::array<float, 8> diffuserCoefficients {
        0.625f, -0.625f, 0.625f, -0.625f,
        0.625f, -0.625f, 0.625f, -0.625f
    };
    for (std::size_t line = 0; line < reverbLines.size(); ++line)
    {
        auto& reverbLine = reverbLines[line];
        const auto diffuserSize = std::max (1, static_cast<int> (
            std::floor (diffuserTimes[line] * sampleRate + 0.5)));
        const auto delaySize = std::max (8, static_cast<int> (
            std::floor (delayTimes[line] * sampleRate + 0.5)) - diffuserSize);
        reverbLine.diffuser.assign (static_cast<std::size_t> (diffuserSize), 0.0f);
        reverbLine.delay.assign (static_cast<std::size_t> (delaySize), 0.0f);
        reverbLine.diffuserPosition = reverbLine.delayPosition = 0;
        reverbLine.diffuserCoefficient = diffuserCoefficients[line];
        reverbLine.lowState = reverbLine.highState = 0.0f;
    }

    const auto lwsReverbLength = static_cast<std::size_t> (std::min (
        300000.0, std::floor (sampleRate * 0.50) + 32.0));
    for (auto& buffer : lwsReverbBuffers)
        buffer.assign (lwsReverbLength, 0.0f);
    activeReverbMode = -1;
    resetLwsReverbState();

    reverbWetSmoothed = reverbWidthSmoothed = 0.0f;
    reverbEarlyLevelSmoothed = reverbEarlyPanSmoothed = 0.0f;
    reverbEarlyRatioSmoothed = 1.0f;
    reverbOutputLeft = reverbOutputRight = reverbTail = 0.0f;
    chorusHpCoefficient = static_cast<float> (
        std::exp (-juce::MathConstants<double>::twoPi * 120.0 / sampleRate));
    dcCoefficient = static_cast<float> (
        std::exp (-juce::MathConstants<double>::twoPi * 5.0 / sampleRate));
}

float SynthEngine::value (juce::AudioProcessorValueTreeState& state, const char* id) const
{
    if (id != nullptr && id[0] == 's' && id[1] == 'l' && id[2] == 'i'
        && id[3] == 'd' && id[4] == 'e' && id[5] == 'r')
    {
        auto sliderNumber = 0;
        for (auto* c = id + 6; *c >= '0' && *c <= '9'; ++c)
            sliderNumber = sliderNumber * 10 + (*c - '0');
        if (juce::isPositiveAndBelow (sliderNumber, sequencerParameterOverrideSlots)
            && sequencerParameterOverrideActive[static_cast<std::size_t> (sliderNumber)])
            return sequencerParameterOverrideValues[static_cast<std::size_t> (sliderNumber)];
    }

    if (const auto* raw = state.getRawParameterValue (id))
        return raw->load();
    return 0.0f;
}

SynthEngine::Params SynthEngine::readParams (juce::AudioProcessorValueTreeState& s,
                                              double tempoBpm) const
{
    Params p;
    p.inputGainDb = value (s, "slider001");
    p.masterVolumeDb = value (s, "slider256");
    p.masterToneSemitones = value (s, "slider062");
    p.voiceCount = juce::jlimit (1, 16, juce::roundToInt (value (s, "slider002")));
    p.wave1 = juce::roundToInt (value (s, "slider003"));
    p.pwm = value (s, "slider004");
    p.attack = value (s, "slider005");
    p.decay = value (s, "slider006");
    p.sustain = value (s, "slider007");
    p.release = value (s, "slider008");
    p.portamento = { value (s, "slider381"), value (s, "slider382"), value (s, "slider386") };
    p.polyMode = juce::roundToInt (value (s, "slider010"));
    p.balance = value (s, "slider011");
    p.detune2 = value (s, "slider012");
    p.pan1 = value (s, "slider013");
    p.pan2 = value (s, "slider014");

    p.lowPassCutoff = value (s, "slider015");
    p.lowPassResonance = value (s, "slider016");
    p.highPassCutoff = value (s, "slider017");
    p.highPassResonance = value (s, "slider018");
    p.localLowPassCutoff = { value (s, "slider396"), value (s, "slider400"),
                              value (s, "slider404") };
    p.localLowPassResonance = { value (s, "slider397"), value (s, "slider401"),
                                 value (s, "slider405") };
    p.localLowPassSlope = { juce::roundToInt (value (s, "slider408")),
                            juce::roundToInt (value (s, "slider409")),
                            juce::roundToInt (value (s, "slider410")) };
    p.localHighPassCutoff = { value (s, "slider398"), value (s, "slider402"),
                               value (s, "slider406") };
    p.localHighPassResonance = { value (s, "slider399"), value (s, "slider403"),
                                  value (s, "slider407") };
    p.filterEnvelope = value (s, "slider019");
    p.pitchEnvelopeAmount = value (s, "slider020");
    p.filterUsesAdsr2 = value (s, "slider021") >= 0.5f;
    p.pitchAdsr2Blend = value (s, "slider022");
    p.lowPassSlope = juce::roundToInt (value (s, "slider023"));
    p.drift = value (s, "slider024");
    p.filterLayerRouting = value (s, "slider059") >= 0.5f;
    p.lowPassLayer1 = value (s, "slider086") >= 0.5f;
    p.lowPassLayer2 = value (s, "slider099") >= 0.5f;
    p.lowPassNoise = value (s, "slider285") >= 0.5f;
    p.highPassLayer1 = value (s, "slider109") >= 0.5f;
    p.highPassLayer2 = value (s, "slider129") >= 0.5f;
    p.highPassNoise = value (s, "slider286") >= 0.5f;
    p.formantLayer1 = value (s, "slider167") >= 0.5f;
    p.formantLayer2 = value (s, "slider255") >= 0.5f;
    p.formantNoise = value (s, "slider287") >= 0.5f;
    p.microMotionSpeed = value (s, "slider288");
    p.microMotionPitch1 = value (s, "slider289");
    p.microMotionPitch2 = value (s, "slider290");
    p.microMotionPwm1 = value (s, "slider291");
    p.microMotionPwm2 = value (s, "slider292");
    p.microMotionPan1 = value (s, "slider293");
    p.microMotionPan2 = value (s, "slider294");
    p.microMotionLowPass = value (s, "slider295");
    p.microMotionHighPass = value (s, "slider296");
    p.microMotionFormant = value (s, "slider297");

    p.lfo1.mode = juce::roundToInt (value (s, "slider025"));
    p.lfo1.rate = value (s, "slider026");
    p.lfo1.wave = juce::roundToInt (value (s, "slider027"));
    p.legacyLfoVolume1 = value (s, "slider028");
    p.legacyLfoLowPass1 = value (s, "slider029");
    p.legacyLfoPan1 = value (s, "slider030");
    p.legacyLfoPitch1 = value (s, "slider031");
    p.lfo1.smooth = value (s, "slider032");
    p.legacyLfoPwm1 = value (s, "slider033");
    p.legacyLfoHighPass1 = value (s, "slider034");

    p.lfo2.mode = juce::roundToInt (value (s, "slider035"));
    p.lfo2.rate = value (s, "slider036");
    p.lfo2.wave = juce::roundToInt (value (s, "slider037"));
    p.legacyLfoVolume2 = value (s, "slider038");
    p.legacyLfoLowPass2 = value (s, "slider039");
    p.legacyLfoPan2 = value (s, "slider040");
    p.legacyLfoPitch2 = value (s, "slider041");
    p.lfo2.smooth = value (s, "slider042");
    p.legacyLfoPwm2 = value (s, "slider043");
    p.legacyLfoHighPass2 = value (s, "slider044");
    p.lfo1.envelopeRate = value (s, "slider045");
    p.lfo2.envelopeRate = value (s, "slider046");
    p.lfo2.crossRate = value (s, "slider047");
    p.lfo1.crossRate = value (s, "slider048");

    p.level1 = value (s, "slider049");
    p.noiseLevel = value (s, "slider050");
    p.noiseColor = value (s, "slider051");
    p.noiseType = juce::roundToInt (value (s, "slider280"));
    p.noisePitch = value (s, "slider281");
    p.lfo1NoisePitch = value (s, "slider282");
    p.lfo2NoisePitch = value (s, "slider283");
    p.noiseStereo = value (s, "slider284");
    p.noisePan = value (s, "slider395");

    p.lfo1PitchL1 = value (s, "slider336");
    p.lfo1PitchL2 = value (s, "slider337");
    p.lfo2PitchL1 = value (s, "slider338");
    p.lfo2PitchL2 = value (s, "slider339");
    p.lfo1VolumeNoise = value (s, "slider340");
    p.lfo2VolumeNoise = value (s, "slider341");
    p.lfo1PanL1 = value (s, "slider342");
    p.lfo1PanL2 = value (s, "slider343");
    p.lfo1PanNoise = value (s, "slider344");
    p.lfo2PanL1 = value (s, "slider345");
    p.lfo2PanL2 = value (s, "slider346");
    p.lfo2PanNoise = value (s, "slider347");
    p.lfo1LowPassL1 = value (s, "slider348");
    p.lfo1LowPassL2 = value (s, "slider349");
    p.lfo1LowPassNoise = value (s, "slider350");
    p.lfo2LowPassL1 = value (s, "slider351");
    p.lfo2LowPassL2 = value (s, "slider352");
    p.lfo2LowPassNoise = value (s, "slider353");
    p.lfo1HighPassL1 = value (s, "slider354");
    p.lfo1HighPassL2 = value (s, "slider355");
    p.lfo1HighPassNoise = value (s, "slider356");
    p.lfo2HighPassL1 = value (s, "slider357");
    p.lfo2HighPassL2 = value (s, "slider358");
    p.lfo2HighPassNoise = value (s, "slider359");
    p.lfo1PwmL1 = value (s, "slider360");
    p.lfo1PwmL2 = value (s, "slider361");
    p.lfo2PwmL1 = value (s, "slider362");
    p.lfo2PwmL2 = value (s, "slider363");
    p.wave2 = juce::roundToInt (value (s, "slider052"));
    p.octave2 = juce::roundToInt (value (s, "slider053"));
    p.semitone2 = juce::roundToInt (value (s, "slider054"));
    p.keyFollowFilter = value (s, "slider055");
    p.velocityFilter = value (s, "slider056");
    p.velocityVolume = { value (s, "slider383"), value (s, "slider384"), value (s, "slider385") };
    p.level2 = value (s, "slider058");
    // Legacy wet controls remain in the stable parameter API, but the current
    // FX architecture uses per-source sends and runs each wet engine at unity.
    p.chorusLevel = 1.0f;
    p.chorusSend = { value (s, "slider365"), value (s, "slider366"), value (s, "slider367") };
    p.delaySend = { value (s, "slider368"), value (s, "slider369"), value (s, "slider370") };
    p.reverbSend = { value (s, "slider371"), value (s, "slider372"), value (s, "slider373") };
    p.auxOutput = {
        juce::jlimit (0, 4, juce::roundToInt (value (s, "slider374"))),
        juce::jlimit (0, 4, juce::roundToInt (value (s, "slider375"))),
        juce::jlimit (0, 4, juce::roundToInt (value (s, "slider376"))),
        juce::jlimit (0, 4, juce::roundToInt (value (s, "slider377")))
    };
    p.sourceMidiChannel = {
        juce::jlimit (0, 16, juce::roundToInt (value (s, "slider378"))),
        juce::jlimit (0, 16, juce::roundToInt (value (s, "slider379"))),
        juce::jlimit (0, 16, juce::roundToInt (value (s, "slider380")))
    };
    p.sourceNoteSource = {
        juce::jlimit (0, 2, juce::roundToInt (value (s, "slider392"))),
        juce::jlimit (0, 2, juce::roundToInt (value (s, "slider393"))),
        juce::jlimit (0, 2, juce::roundToInt (value (s, "slider394")))
    };
    p.chorusRate = value (s, "slider061");
    p.chorusWidth = value (s, "slider063");
    p.octave1 = juce::roundToInt (value (s, "slider064"));
    p.semitone1 = juce::roundToInt (value (s, "slider065"));
    p.phaseMod12 = value (s, "slider066");
    p.phaseMod21 = value (s, "slider067");
    p.metal1 = value (s, "slider068");
    p.metal2 = value (s, "slider069");
    p.shark1 = value (s, "slider070");
    p.shark2 = value (s, "slider071");
    p.sync1 = value (s, "slider072");
    p.sync2 = value (s, "slider073");
    p.waveModLfo1 = value (s, "slider074");
    p.waveModLfo2 = value (s, "slider075");
    p.lfo1.delay = value (s, "slider076");
    p.lfo2.delay = value (s, "slider077");
    p.pitchBendRange = { value (s, "slider387"), value (s, "slider388"), value (s, "slider389") };
    p.sourceVoiceMode = {
        juce::jlimit (0, 2, juce::roundToInt (value (s, "slider390"))),
        juce::jlimit (0, 2, juce::roundToInt (value (s, "slider391")))
    };
    p.lfo1ModWheelAmount = value (s, "slider079");
    p.compressor.thresholdDb = value (s, "slider080");
    p.compressor.ratio = juce::roundToInt (value (s, "slider081"));
    p.compressor.makeupDb = value (s, "slider082");
    p.compressor.attackMicroseconds = value (s, "slider083");
    p.compressor.releaseMilliseconds = value (s, "slider084");
    p.compressor.mix = value (s, "slider085") * 0.01f;

    p.attack2 = value (s, "slider087");
    p.decay2 = value (s, "slider088");
    p.sustain2 = value (s, "slider089");
    p.release2 = value (s, "slider090");
    p.noteScalePan = value (s, "slider091");
    p.pingPongPan = value (s, "slider092");
    p.panEnvelope = value (s, "slider093");
    p.lfo2AftertouchAmount = value (s, "slider094");
    p.keyFollowVolume = value (s, "slider095");
    p.keyFollowFilterMode = juce::roundToInt (value (s, "slider096"));
    p.morph1 = value (s, "slider097");
    p.morph2 = value (s, "slider098");

    // Wave and Morph are coupled in the normal synth UI: choosing one of the
    // five classic Wave choices also moves Morph to the corresponding exact
    // waveform. Parameter locks bypass APVTS parameterChanged(), so reproduce
    // that coupling here or a Wave lock would only distinguish classic-vs-
    // SuperWave while the old Morph value kept rendering the previous shape.
    const auto wave1Locked = sequencerParameterOverrideActive[3];
    const auto wave2Locked = sequencerParameterOverrideActive[52];
    const auto morph1Locked = sequencerParameterOverrideActive[97];
    const auto morph2Locked = sequencerParameterOverrideActive[98];
    if (wave1Locked && p.wave1 < 5)
        p.morph1 = juce::jlimit (0.0f, 1.0f, p.wave1 * 0.25f);
    else if (morph1Locked && ! wave1Locked && p.wave1 < 5)
        p.wave1 = juce::jlimit (0, 4, juce::roundToInt (p.morph1 * 4.0f));
    if (wave2Locked && p.wave2 < 5)
        p.morph2 = juce::jlimit (0.0f, 1.0f, p.wave2 * 0.25f);
    else if (morph2Locked && ! wave2Locked && p.wave2 < 5)
        p.wave2 = juce::jlimit (0, 4, juce::roundToInt (p.morph2 * 4.0f));

    p.lfo1VolumeL1 = value (s, "slider120");
    p.lfo2VolumeL1 = value (s, "slider121");
    p.lfo1VolumeL2 = value (s, "slider122");
    p.lfo2VolumeL2 = value (s, "slider123");
    p.lfo1.squarePwm = value (s, "slider124");
    p.lfo2.squarePwm = value (s, "slider125");
    p.lfo1FormantMorph = value (s, "slider126");
    p.lfo2FormantMorph = value (s, "slider127");
    p.formantEnvelope = value (s, "slider128");
    p.lfo1Morph1 = value (s, "slider130");
    p.lfo1Morph2 = value (s, "slider131");
    p.lfo2Morph1 = value (s, "slider132");
    p.lfo2Morph2 = value (s, "slider133");
    p.monoNoteMode = juce::roundToInt (value (s, "slider134"));
    p.monoPortamentoMode = juce::roundToInt (value (s, "slider135"));
    p.voicePanAlternate = value (s, "slider136");
    p.monoUnisonVoices = juce::jlimit (1, 16, juce::roundToInt (value (s, "slider137")));
    p.monoUnisonDetune = value (s, "slider138");
    p.ampBlend1 = value (s, "slider157");
    p.ampBlend2 = value (s, "slider158");
    p.panAdsr2Blend = value (s, "slider159");
    p.noiseBlend = value (s, "slider165");
    p.pitchReleaseDirection = value (s, "slider166");
    p.splitWidth = value (s, "slider162");
    p.splitNote = value (s, "slider163");
    p.splitInverted = value (s, "slider164") >= 0.5f;
    p.formantEnabled = value (s, "slider168") >= 0.5f;
    p.formantMorph = value (s, "slider169");
    p.compressorPosition = juce::roundToInt (value (s, "slider139"));
    p.reverbMode = juce::jlimit (0, 2, juce::roundToInt (value (s, "slider140")));
    p.reverbOn = p.reverbMode != 0;
    p.reverbPredelayMs = value (s, "slider141");
    p.reverbXoverHz = value (s, "slider142");
    p.reverbBassMultiplier = value (s, "slider143");
    p.reverbDecaySeconds = value (s, "slider144");
    p.reverbDampingHz = value (s, "slider145");
    p.reverbWidth = value (s, "slider146");
    p.reverbWet = 1.0f;
    p.reverbEarlyLevel = value (s, "slider148");
    p.reverbEarlyPan = value (s, "slider149");
    p.reverbEarlyRatio = value (s, "slider150");
    p.reverbCompressor.thresholdDb = value (s, "slider151");
    p.reverbCompressor.ratio = juce::roundToInt (value (s, "slider152"));
    p.reverbCompressor.makeupDb = value (s, "slider153");
    p.reverbCompressor.attackMicroseconds = value (s, "slider154");
    p.reverbCompressor.releaseMilliseconds = value (s, "slider155");
    p.reverbCompressor.mix = value (s, "slider156") * 0.01f;
    p.compressor.sidechain = value (s, "slider160") >= 0.5f;
    p.reverbCompressor.sidechain = value (s, "slider161") >= 0.5f;

    p.delayMode = juce::jlimit (0, 2, juce::roundToInt (value (s, "slider100")));
    p.delayOn = p.delayMode != 0;
    p.delayTime = value (s, "slider101");
    p.delayFeedback = value (s, "slider102");
    p.delayMix = 1.0f;
    p.delayTone = value (s, "slider104");
    p.delayMono = value (s, "slider105") >= 0.5f;
    p.delaySync = juce::roundToInt (value (s, "slider106"));
    p.delayLfo1 = value (s, "slider107");
    p.delayLfo2 = value (s, "slider108");
    p.delay2GlideMs = value (s, "slider298");
    p.delay2Speed1 = value (s, "slider299");
    p.delay2Speed2 = value (s, "slider300");
    p.delay2Feedback1 = value (s, "slider301");
    p.delay2Feedback2 = value (s, "slider302");
    p.delay2ToneLeft = value (s, "slider303");
    p.delay2ToneRight = value (s, "slider304");
    p.delay2StereoSpread = value (s, "slider305");
    p.delay2TapeDrive = value (s, "slider306");
    p.delay2Time = value (s, "slider307");
    p.delay2Sync = juce::roundToInt (value (s, "slider308"));
    p.delay2Mix = 1.0f;
    p.delay2Mono = value (s, "slider310") >= 0.5f;
    p.delay2Lfo1 = value (s, "slider311");
    p.delay2Lfo2 = value (s, "slider312");
    constexpr std::array<const char*, 5> eqFrequencyIds {
        "slider110", "slider111", "slider112", "slider113", "slider118"
    };
    constexpr std::array<const char*, 5> eqGainIds {
        "slider114", "slider115", "slider116", "slider117", "slider119"
    };
    for (std::size_t band = 0; band < p.eqFrequency.size(); ++band)
    {
        p.eqFrequency[band] = value (s, eqFrequencyIds[band]);
        p.eqGain[band] = value (s, eqGainIds[band]);
    }

    constexpr std::array<const char*, 5> superWave1WaveIds {
        "slider170", "slider171", "slider172", "slider173", "slider174"
    };
    constexpr std::array<const char*, 5> superWave1PitchIds {
        "slider175", "slider176", "slider177", "slider178", "slider179"
    };
    constexpr std::array<const char*, 5> superWave1WidthIds {
        "slider180", "slider181", "slider182", "slider183", "slider184"
    };
    constexpr std::array<const char*, 5> superWave2WaveIds {
        "slider186", "slider187", "slider188", "slider189", "slider190"
    };
    constexpr std::array<const char*, 5> superWave2PitchIds {
        "slider191", "slider192", "slider193", "slider194", "slider195"
    };
    constexpr std::array<const char*, 5> superWave2WidthIds {
        "slider196", "slider197", "slider198", "slider199", "slider200"
    };
    for (std::size_t step = 0; step < 5; ++step)
    {
        p.superWave1.waves[step] = juce::roundToInt (value (s, superWave1WaveIds[step]));
        p.superWave1.pitch[step] = value (s, superWave1PitchIds[step]);
        p.superWave1.width[step] = std::max (1, juce::roundToInt (value (s, superWave1WidthIds[step])));
        p.superWave2.waves[step] = juce::roundToInt (value (s, superWave2WaveIds[step]));
        p.superWave2.pitch[step] = value (s, superWave2PitchIds[step]);
        p.superWave2.width[step] = std::max (1, juce::roundToInt (value (s, superWave2WidthIds[step])));
    }
    p.superWave1.length = value (s, "slider185");
    p.superWave2.length = value (s, "slider201");

    p.lfo1.oneShot = value (s, "slider220") >= 0.5f;
    p.lfo2.oneShot = value (s, "slider221") >= 0.5f;
    p.lfo1.oneShotPercent = value (s, "slider222");
    p.lfo2.oneShotPercent = value (s, "slider223");
    p.lfo1.phase = value (s, "slider224");
    p.lfo2.phase = value (s, "slider225");
    p.lfo1.upperSquash = value (s, "slider274");
    p.lfo1.lowerSquash = value (s, "slider275");
    p.lfo2.upperSquash = value (s, "slider276");
    p.lfo2.lowerSquash = value (s, "slider277");
    p.superWave1.character = value (s, "slider226");
    p.superWave2.character = value (s, "slider227");
    p.superWave1.lfo1Length = value (s, "slider228");
    p.superWave1.lfo2Length = value (s, "slider229");
    p.superWave2.lfo1Length = value (s, "slider230");
    p.superWave2.lfo2Length = value (s, "slider231");
    p.superWave1.invertOddVoices = value (s, "slider232") >= 0.5f;
    p.superWave2.invertOddVoices = value (s, "slider233") >= 0.5f;
    p.pitchArp1.mode = juce::roundToInt (value (s, "slider257"));
    p.pitchArp1.rate = value (s, "slider258");
    p.pitchArp1.octaveUp = juce::roundToInt (value (s, "slider259"));
    p.pitchArp1.octaveDown = juce::roundToInt (value (s, "slider260"));
    p.pitchArp1.glide = value (s, "slider261");
    p.pitchArpLowPassDepth = value (s, "slider262");
    p.pitchArpHighPassDepth = value (s, "slider263");
    p.pitchArpPanDepth = value (s, "slider264");
    p.pitchArp1.volumeDepth = value (s, "slider265");
    p.pitchArp1.pwmDepth = value (s, "slider266");
    p.pitchArp2.mode = juce::roundToInt (value (s, "slider267"));
    p.pitchArp2.rate = value (s, "slider268");
    p.pitchArp2.octaveUp = juce::roundToInt (value (s, "slider269"));
    p.pitchArp2.octaveDown = juce::roundToInt (value (s, "slider270"));
    p.pitchArp2.glide = value (s, "slider271");
    p.pitchArp2.volumeDepth = value (s, "slider272");
    p.pitchArp2.pwmDepth = value (s, "slider273");
    p.pitchArp1.pitchMovement = value (s, "slider278") >= 0.5f;
    p.pitchArp2.pitchMovement = value (s, "slider279") >= 0.5f;

    p.larp.state = juce::roundToInt (value (s, "slider202"));
    p.larp.division = value (s, "slider203");
    p.larp.pattern = juce::roundToInt (value (s, "slider204"));
    p.larp.shuffle = value (s, "slider205");
    p.larp.skipProbability = value (s, "slider206");
    p.larp.length = value (s, "slider207");
    p.larp.repeatRandomDepth = value (s, "slider208");
    p.larp.pitchRandomDepth = value (s, "slider209");
    p.larp.velocityRandomDepth = value (s, "slider210");
    p.larp.lengthRandomMode = juce::roundToInt (value (s, "slider211"));
    p.larp.octaveUp = juce::roundToInt (value (s, "slider212"));
    p.larp.octaveDown = juce::roundToInt (value (s, "slider213"));
    p.larp.octaveMode = juce::roundToInt (value (s, "slider214"));
    p.larp.repeat = juce::roundToInt (value (s, "slider215"));
    p.larp.legatoPattern = juce::roundToInt (value (s, "slider216"));
    p.larp.chordHold = value (s, "slider217") >= 0.5f;
    p.larp.adaptiveDivision = value (s, "slider218") >= 0.5f;
    p.larp.shuffleVelocity = value (s, "slider219");
    p.larp.shuffleLength = value (s, "slider234");
    p.larp.microShiftDepth = value (s, "slider235");
    p.larp.microShiftMode = juce::roundToInt (value (s, "slider236"));
    p.larp.microShiftVelocityDepth = value (s, "slider237");
    p.larp.microShiftLengthDepth = value (s, "slider238");
    p.larp.modeSwitch = juce::roundToInt (value (s, "slider239"));
    p.larp.ratePattern = juce::roundToInt (value (s, "slider240"));
    p.larp.ratePatternSpeed = value (s, "slider241");
    p.larp.freeShuffle = value (s, "slider242") >= 0.5f;
    p.larp.sustainQuantize = juce::roundToInt (value (s, "slider244"));
    p.larp.resetSustain = value (s, "slider245") >= 0.5f;
    p.larp.midiChannel = juce::jlimit (0, 16, juce::roundToInt (value (s, "slider246")));
    p.larp.volumeDepth = value (s, "slider247");
    p.larp.lowPassDepth = value (s, "slider248");
    p.larp.panDepth = value (s, "slider249");
    p.larp.pitchDepth = value (s, "slider250");
    p.larp.pwmDepth = value (s, "slider251");
    p.larp.highPassDepth = value (s, "slider252");
    p.larp.lfo1RateDepth = value (s, "slider253");
    p.larp.lfo2RateDepth = value (s, "slider254");
    p.tempoBpm = juce::jlimit (1.0, 999.0, tempoBpm);
    return p;
}

void SynthEngine::setActiveSequencerParameterLocks (int sequenceIndex,
                                                        const SequencerStep& step)
{
    if (! juce::isPositiveAndBelow (sequenceIndex, static_cast<int> (activeStepParameterLocks.size())))
        return;

    auto& active = activeStepParameterLocks[static_cast<std::size_t> (sequenceIndex)];
    active = {};
    active.count = juce::jlimit (0, SequencerStep::maximumParameterLocks,
                                 step.parameterLockCount);
    for (int i = 0; i < active.count; ++i)
    {
        const auto& lock = step.parameterLocks[static_cast<std::size_t> (i)];
        active.sliderNumbers[static_cast<std::size_t> (i)] = lock.sliderNumber;
        active.values[static_cast<std::size_t> (i)] = lock.value;
    }
    active.generation = ++sequencerParameterLockGeneration;
    rebuildSequencerParameterOverrides();
}

void SynthEngine::clearActiveSequencerParameterLocks (int sequenceIndex)
{
    if (! juce::isPositiveAndBelow (sequenceIndex, static_cast<int> (activeStepParameterLocks.size())))
        return;
    auto& active = activeStepParameterLocks[static_cast<std::size_t> (sequenceIndex)];
    if (active.count == 0 && active.generation == 0)
        return;
    active = {};
    rebuildSequencerParameterOverrides();
}

void SynthEngine::rebuildSequencerParameterOverrides()
{
    sequencerParameterOverrideActive.fill (false);
    sequencerParameterOverrideValues.fill (0.0f);
    std::array<std::uint64_t, sequencerParameterOverrideSlots> generations {};

    // The most recently triggered sequencer lane wins if two currently active
    // steps lock the same synth parameter. When that lane advances to a step
    // without the lock, the older still-active lock from another lane becomes
    // effective again.
    for (const auto& active : activeStepParameterLocks)
    {
        for (int i = 0; i < active.count; ++i)
        {
            const auto sliderNumber = active.sliderNumbers[static_cast<std::size_t> (i)];
            if (! juce::isPositiveAndBelow (sliderNumber, sequencerParameterOverrideSlots))
                continue;
            const auto index = static_cast<std::size_t> (sliderNumber);
            if (sequencerParameterOverrideActive[index]
                && generations[index] > active.generation)
                continue;
            sequencerParameterOverrideActive[index] = true;
            sequencerParameterOverrideValues[index] = active.values[static_cast<std::size_t> (i)];
            generations[index] = active.generation;
        }
    }
    ++sequencerParameterLockRevision;
}

void SynthEngine::refreshCachedParamsForSequencerLocks (
    juce::AudioProcessorValueTreeState& state, double tempoBpm)
{
    cachedParams = readParams (state, tempoBpm);
    cachedRenderConstants = makeRenderConstants (cachedParams);
    updateEffectCoefficients (cachedParams);
    for (auto& voice : voices)
    {
        voice.cachedPitchFrequency = -1.0;
        voice.cachedPerformancePitch = std::numeric_limits<float>::max();
        voice.cachedPitchBend1 = std::numeric_limits<float>::max();
        voice.cachedPitchBend2 = std::numeric_limits<float>::max();
        voice.lowPassCoefficientFrequency = -1.0f;
        voice.highPassCoefficientFrequency = -1.0f;
    }
    pitchArpPoolDirty = true;
    larpState.chordDirty = true;
    cachedSequencerParameterLockRevision = sequencerParameterLockRevision;
}

bool SynthEngine::usesAdsr2 (const Params& p)
{
    return p.filterUsesAdsr2
        || p.pitchAdsr2Blend > epsilon
        || p.ampBlend1 > epsilon
        || p.ampBlend2 > epsilon
        || p.panAdsr2Blend > epsilon
        || p.noiseBlend > epsilon;
}

void SynthEngine::setVoicePerformanceTargets (Voice& v, int note, float velocity,
                                               const Params& p, bool instant)
{
    v.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    const auto keyVolumeAmount = (p.keyFollowVolume - 0.5f) * 2.0f;
    const auto keyVolumeSlope = 0.08f + keyVolumeAmount * 2.5f;
    const auto keyFilterAmount = (p.keyFollowFilter - 0.5f) * 2.0f;
    const auto keyFilterSlope = 0.15f + keyFilterAmount * 4.0f;
    const auto keyFilterOctaves = (note - 60) * keyFilterSlope / 12.0f;
    const auto keyVolumeTarget = std::exp2 ((note - 60) * keyVolumeSlope / 12.0f);
    const auto keyFilterTarget = std::exp2 (keyFilterOctaves);
    v.keyFollowLowPassTarget = keyFilterTarget;
    v.keyFollowOctavesTarget = keyFilterOctaves;
    v.lowPassCoefficientFrequency = -1.0f;
    v.highPassCoefficientFrequency = -1.0f;
    if (instant)
    {
        v.velocitySmoothed = v.velocityFilterSmoothed = v.velocity;
        v.keyFollowVolumeGain = keyVolumeTarget;
        v.keyFollowLowPass = keyFilterTarget;
    }
}

SynthEngine::RenderConstants SynthEngine::makeRenderConstants (const Params& p) const
{
    RenderConstants d;
    d.lfo1 = p.lfo1;
    d.lfo2 = p.lfo2;
    const auto tempoScale = static_cast<float> (p.tempoBpm / 240.0);
    d.lfo1.rate *= tempoScale;
    d.lfo2.rate *= tempoScale;
    const auto activeDelayLfo1 = p.delayMode == 2 ? p.delay2Lfo1 : p.delayLfo1;
    const auto activeDelayLfo2 = p.delayMode == 2 ? p.delay2Lfo2 : p.delayLfo2;
    d.delayNeedsLfo = p.delayOn
                   && (std::abs (activeDelayLfo1) > epsilon
                       || std::abs (activeDelayLfo2) > epsilon);
    const auto lfo1AudioDestination = std::abs (p.legacyLfoVolume1) > epsilon
        || std::abs (p.legacyLfoLowPass1) > epsilon
        || std::abs (p.legacyLfoHighPass1) > epsilon
        || std::abs (p.legacyLfoPan1) > epsilon
        || std::abs (p.legacyLfoPitch1) > epsilon
        || std::abs (p.legacyLfoPwm1) > epsilon
        || std::abs (p.lfo1PitchL1) > epsilon || std::abs (p.lfo1PitchL2) > epsilon
        || std::abs (p.lfo1NoisePitch) > epsilon
        || std::abs (p.lfo1VolumeL1) > epsilon || std::abs (p.lfo1VolumeL2) > epsilon
        || std::abs (p.lfo1VolumeNoise) > epsilon
        || std::abs (p.lfo1PanL1) > epsilon || std::abs (p.lfo1PanL2) > epsilon
        || std::abs (p.lfo1PanNoise) > epsilon
        || std::abs (p.lfo1LowPassL1) > epsilon || std::abs (p.lfo1LowPassL2) > epsilon
        || std::abs (p.lfo1LowPassNoise) > epsilon
        || std::abs (p.lfo1HighPassL1) > epsilon || std::abs (p.lfo1HighPassL2) > epsilon
        || std::abs (p.lfo1HighPassNoise) > epsilon
        || std::abs (p.lfo1PwmL1) > epsilon || std::abs (p.lfo1PwmL2) > epsilon
        || std::abs (p.waveModLfo1) > epsilon
        || (p.delayOn && std::abs (activeDelayLfo1) > epsilon)
        || std::abs (p.lfo1Morph1) > epsilon || std::abs (p.lfo1Morph2) > epsilon
        || (p.formantEnabled && std::abs (p.lfo1FormantMorph) > epsilon)
        || (p.wave1 == 5 && std::abs (p.superWave1.lfo1Length) > epsilon)
        || (p.wave2 == 5 && std::abs (p.superWave2.lfo1Length) > epsilon);
    const auto lfo2AudioDestination = std::abs (p.legacyLfoVolume2) > epsilon
        || std::abs (p.legacyLfoLowPass2) > epsilon
        || std::abs (p.legacyLfoHighPass2) > epsilon
        || std::abs (p.legacyLfoPan2) > epsilon
        || std::abs (p.legacyLfoPitch2) > epsilon
        || std::abs (p.legacyLfoPwm2) > epsilon
        || std::abs (p.lfo2PitchL1) > epsilon || std::abs (p.lfo2PitchL2) > epsilon
        || std::abs (p.lfo2NoisePitch) > epsilon
        || std::abs (p.lfo2VolumeL1) > epsilon || std::abs (p.lfo2VolumeL2) > epsilon
        || std::abs (p.lfo2VolumeNoise) > epsilon
        || std::abs (p.lfo2PanL1) > epsilon || std::abs (p.lfo2PanL2) > epsilon
        || std::abs (p.lfo2PanNoise) > epsilon
        || std::abs (p.lfo2LowPassL1) > epsilon || std::abs (p.lfo2LowPassL2) > epsilon
        || std::abs (p.lfo2LowPassNoise) > epsilon
        || std::abs (p.lfo2HighPassL1) > epsilon || std::abs (p.lfo2HighPassL2) > epsilon
        || std::abs (p.lfo2HighPassNoise) > epsilon
        || std::abs (p.lfo2PwmL1) > epsilon || std::abs (p.lfo2PwmL2) > epsilon
        || std::abs (p.waveModLfo2) > epsilon
        || (p.delayOn && std::abs (activeDelayLfo2) > epsilon)
        || std::abs (p.lfo2Morph1) > epsilon || std::abs (p.lfo2Morph2) > epsilon
        || (p.formantEnabled && std::abs (p.lfo2FormantMorph) > epsilon)
        || (p.wave1 == 5 && std::abs (p.superWave1.lfo2Length) > epsilon)
        || (p.wave2 == 5 && std::abs (p.superWave2.lfo2Length) > epsilon);
    d.lfo1Needed = lfo1AudioDestination
                || (lfo2AudioDestination && std::abs (p.lfo2.crossRate) > epsilon)
                || (larpPlaysSynth (p.larp.state)
                    && std::abs (p.larp.lfo1RateDepth) > epsilon);
    d.lfo2Needed = lfo2AudioDestination
                || (lfo1AudioDestination && std::abs (p.lfo1.crossRate) > epsilon)
                || (larpPlaysSynth (p.larp.state)
                    && std::abs (p.larp.lfo2RateDepth) > epsilon);
    d.morph1Dynamic = std::abs (p.lfo1Morph1) > epsilon
                   || std::abs (p.lfo2Morph1) > epsilon;
    d.morph2Dynamic = std::abs (p.lfo1Morph2) > epsilon
                   || std::abs (p.lfo2Morph2) > epsilon;
    d.continuousPitchModulation = std::abs (p.legacyLfoPitch1) > epsilon
                               || std::abs (p.legacyLfoPitch2) > epsilon
                               || std::abs (p.lfo1PitchL1) > epsilon
                               || std::abs (p.lfo1PitchL2) > epsilon
                               || std::abs (p.lfo2PitchL1) > epsilon
                               || std::abs (p.lfo2PitchL2) > epsilon
                               || (larpPlaysSynth (p.larp.state)
                                   && std::abs (p.larp.pitchDepth) > epsilon);
    const auto scaledMorph1 = juce::jlimit (0.0f, 1.0f, p.morph1) * 4.0f;
    const auto scaledMorph2 = juce::jlimit (0.0f, 1.0f, p.morph2) * 4.0f;
    d.staticWave1 = juce::jlimit (0, 4, static_cast<int> (std::floor (scaledMorph1)));
    d.staticWave2 = juce::jlimit (0, 4, static_cast<int> (std::floor (scaledMorph2)));
    d.staticWaveBlend1 = juce::jlimit (0.0f, 1.0f, scaledMorph1 - d.staticWave1);
    d.staticWaveBlend2 = juce::jlimit (0.0f, 1.0f, scaledMorph2 - d.staticWave2);
    d.balance1 = std::sqrt (juce::jlimit (0.0f, 1.0f, 1.0f - p.balance));
    d.balance2 = std::sqrt (juce::jlimit (0.0f, 1.0f, p.balance));
    d.layer1PanLeft = panGain (p.pan1, true);
    d.layer1PanRight = panGain (p.pan1, false);
    d.layer2PanLeft = panGain (p.pan2, true);
    d.layer2PanRight = panGain (p.pan2, false);
    d.microMotionAny = p.microMotionPitch1 > epsilon
                    || p.microMotionPitch2 > epsilon
                    || p.microMotionPwm1 > epsilon
                    || p.microMotionPwm2 > epsilon
                    || p.microMotionPan1 > epsilon
                    || p.microMotionPan2 > epsilon
                    || p.microMotionLowPass > epsilon
                    || p.microMotionHighPass > epsilon
                    || p.microMotionFormant > epsilon;
    if (d.microMotionAny)
    {
        const auto speedCurve = p.microMotionSpeed * 3.0f - 2.0f;
        d.microMotionAlpha = std::min (0.5f, 0.01f * std::pow (100.0f, speedCurve));
    }
    d.pwmCoefficient = 1.0f / (1.0f + 800.0f / static_cast<float> (sampleRate));
    d.adsr2Used = usesAdsr2 (p);
    d.attackIncrement1 = envelopeIncrement (p.attack, sampleRate);
    d.decayIncrement1 = envelopeIncrement (p.decay, sampleRate);
    d.releaseIncrement1 = envelopeIncrement (p.release, sampleRate);
    d.attackIncrement2 = envelopeIncrement (p.attack2, sampleRate);
    d.decayIncrement2 = envelopeIncrement (p.decay2, sampleRate);
    d.releaseIncrement2 = envelopeIncrement (p.release2, sampleRate);
    d.releaseShape = (p.pitchReleaseDirection - 0.5f) * 2.0f;
    d.portamentoAmount = { p.portamento[0] * p.portamento[0],
                           p.portamento[1] * p.portamento[1],
                           p.portamento[2] * p.portamento[2] };
    d.stealCoefficient = static_cast<float> (std::exp (-1.0 / (0.002 * sampleRate)));
    d.velocityCoefficient = static_cast<float> (1.0 - std::exp (-1.0 / (0.010 * sampleRate)));
    d.velocityFilterCoefficient = static_cast<float> (1.0 - std::exp (-1.0 / (0.025 * sampleRate)));
    d.keyFollowCoefficient = static_cast<float> (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));
    d.velocityRuntimeNeeded = p.velocityFilter > epsilon
                           || p.velocityVolume[0] > epsilon
                           || p.velocityVolume[1] > epsilon
                           || p.velocityVolume[2] > epsilon;
    d.oscillatorTuning1 = std::exp2 (p.octave1 + p.semitone1 / 12.0);
    d.oscillatorTuning2 = std::exp2 (p.octave2 + p.semitone2 / 12.0
                                              + p.detune2 / 1200.0);

    constexpr auto lpMinimum = 40.0f;
    const auto lpBaseMaximum = std::min (18000.0f, static_cast<float> (sampleRate * 0.45));
    d.lpBase = lpMinimum * std::pow (lpBaseMaximum / lpMinimum,
                                     juce::jlimit (-1.0f, 1.0f, p.lowPassCutoff));
    if (p.lowPassSlope != 0)
        d.lpBase = std::max (20.0f, d.lpBase);
    const auto keyFollowAmount = (p.keyFollowFilter - 0.5f) * 2.0f;
    d.keyFollowSlope = 0.15f + keyFollowAmount * 4.0f;
    d.lpCoefficientMaximum = std::max (20.0001f,
        static_cast<float> (sampleRate * 0.5 * 0.45));
    const auto lpResonance = juce::jlimit (0.0f, 1.0f, p.lowPassResonance);
    d.lpQ = 0.5f + lpResonance * lpResonance * 11.5f;

    const auto anyLfoLowPass = std::abs (p.legacyLfoLowPass1) > epsilon
                            || std::abs (p.legacyLfoLowPass2) > epsilon
                            || std::abs (p.lfo1LowPassL1) > epsilon
                            || std::abs (p.lfo1LowPassL2) > epsilon
                            || std::abs (p.lfo1LowPassNoise) > epsilon
                            || std::abs (p.lfo2LowPassL1) > epsilon
                            || std::abs (p.lfo2LowPassL2) > epsilon
                            || std::abs (p.lfo2LowPassNoise) > epsilon;
    const auto anyLfoHighPass = std::abs (p.legacyLfoHighPass1) > epsilon
                             || std::abs (p.legacyLfoHighPass2) > epsilon
                             || std::abs (p.lfo1HighPassL1) > epsilon
                             || std::abs (p.lfo1HighPassL2) > epsilon
                             || std::abs (p.lfo1HighPassNoise) > epsilon
                             || std::abs (p.lfo2HighPassL1) > epsilon
                             || std::abs (p.lfo2HighPassL2) > epsilon
                             || std::abs (p.lfo2HighPassNoise) > epsilon;
    d.highPassEnabled = p.highPassCutoff > epsilon
                      || anyLfoHighPass
                      || (larpPlaysSynth (p.larp.state)
                          && std::abs (p.larp.highPassDepth) > epsilon)
                      || p.microMotionHighPass > epsilon;
    d.lowPassStatic = std::abs (p.filterEnvelope) <= epsilon
                    && ! anyLfoLowPass
                    && (! larpPlaysSynth (p.larp.state)
                        || std::abs (p.larp.lowPassDepth) <= epsilon)
                    && p.microMotionLowPass <= epsilon
                    && std::abs (p.velocityFilter) <= epsilon;
    d.highPassStatic = d.highPassEnabled
                     && ! anyLfoHighPass
                     && (! larpPlaysSynth (p.larp.state)
                         || std::abs (p.larp.highPassDepth) <= epsilon);
    if (p.microMotionHighPass > epsilon)
        d.highPassStatic = false;
    constexpr auto hpMinimum = 20.0f;
    const auto hpMaximum = std::min (6000.0f, static_cast<float> (sampleRate * 0.45));
    d.hpBase = hpMinimum * std::pow (hpMaximum / hpMinimum,
                                     juce::jlimit (0.0f, 1.0f, p.highPassCutoff));
    d.hpCoefficientMaximum = std::max (10.0001f,
        static_cast<float> (sampleRate * 0.5 * 0.45));
    const auto hpResonance = juce::jlimit (0.0f, 1.0f, p.highPassResonance);
    d.hpQ = 0.5f + hpResonance * hpResonance * 7.5f;

    const auto keyVolumeAmount = (p.keyFollowVolume - 0.5f) * 2.0f;
    const auto keyVolumeSlope = 0.08f + keyVolumeAmount * 2.5f;
    for (int note = 0; note < 128; ++note)
    {
        d.noteVolumeGain[static_cast<std::size_t> (note)]
            = std::exp2 ((note - 60) * keyVolumeSlope / 12.0f);
        d.noteKeyFollowOctaves[static_cast<std::size_t> (note)]
            = (note - 60) * d.keyFollowSlope / 12.0f;
        d.noteHpKeyFollow[static_cast<std::size_t> (note)]
            = (note - 60) * 0.08f / 12.0f;
    }

    // Split gains depend only on the MIDI note and block-stable split controls.
    // Precompute the exact equal-power curve once instead of taking two square
    // roots for every active voice on every sample.
    for (int note = 0; note < 128; ++note)
    {
        auto gain1 = 1.0f;
        auto gain2 = 1.0f;
        if (p.splitWidth > epsilon)
        {
            const auto noteDelta = static_cast<float> (note) - p.splitNote;
            if (p.splitWidth <= 1.0f)
            {
                gain1 = noteDelta <= 0.0f ? 1.0f : 0.0f;
                gain2 = noteDelta <= 0.0f ? 0.0f : 1.0f;
            }
            else
            {
                const auto crossfade = juce::jlimit (0.0f, 1.0f,
                    (noteDelta + p.splitWidth * 0.5f) / p.splitWidth);
                gain1 = std::sqrt (1.0f - crossfade);
                gain2 = std::sqrt (crossfade);
            }
            if (p.splitInverted)
                std::swap (gain1, gain2);
        }
        d.splitGain1[static_cast<std::size_t> (note)] = gain1;
        d.splitGain2[static_cast<std::size_t> (note)] = gain2;
    }

    // Mono-unison detune/pan geometry is also block-stable. Keep the original
    // equations, but remove exp2/sqrt work from the inner voice loop.
    const auto unisonVoices = juce::jlimit (1, 16, p.monoUnisonVoices);
    const auto unisonSteps = std::max (1, unisonVoices / 2);
    const auto maximumSemitones = p.monoUnisonDetune * p.monoUnisonDetune * 0.5f;
    const auto semitonesPerStep = maximumSemitones / unisonSteps;
    for (int clone = 1; clone < unisonVoices; ++clone)
    {
        const auto index = static_cast<std::size_t> (clone - 1);
        const auto rank = (clone + 1) / 2;
        const auto sign = (clone & 1) != 0 ? 1.0f : -1.0f;
        d.unisonDetuneMultiplier[index] = std::exp2 (sign * rank * semitonesPerStep / 12.0f);
        const auto clonePan = sign * p.voicePanAlternate
                            * (static_cast<float> (rank) / unisonSteps);
        d.unisonPanLeft[index] = panGain (clonePan, true);
        d.unisonPanRight[index] = panGain (clonePan, false);
    }
    d.unisonNormalisation = 1.0f / std::sqrt (static_cast<float> (unisonVoices));
    d.noisePitchMultiplier = std::exp2 (p.noisePitch / 12.0f);
    d.needsEnvelope1 = p.ampBlend1 < 0.999f || p.ampBlend2 < 0.999f
                    || (p.noiseLevel > epsilon && p.noiseBlend < 0.999f);
    d.needsEnvelope2 = p.ampBlend1 > 0.001f || p.ampBlend2 > 0.001f
                    || (p.noiseLevel > epsilon && p.noiseBlend > 0.001f);

    // Local-filter controls are block-stable and refreshed immediately when a
    // parameter change or Parameter Lock rebuilds the render constants. Keep
    // the exact existing cutoff/Q formulas, but calculate them once here
    // instead of once per active voice and sample.
    constexpr auto localLpMinimum = 40.0f;
    const auto localLpMaximum = std::min (18000.0f, static_cast<float> (sampleRate * 0.45));
    constexpr auto localHpMinimum = 20.0f;
    const auto localHpMaximum = std::min (6000.0f, static_cast<float> (sampleRate * 0.45));
    for (std::size_t source = 0; source < 3; ++source)
    {
        const auto lpControl = juce::jlimit (-1.0f, 1.0f, p.localLowPassCutoff[source]);
        const auto lpResonance = juce::jlimit (0.0f, 1.0f, p.localLowPassResonance[source]);
        const auto hpControl = juce::jlimit (0.0f, 1.0f, p.localHighPassCutoff[source]);
        const auto hpResonance = juce::jlimit (0.0f, 1.0f, p.localHighPassResonance[source]);
        const auto lpSlope = juce::jlimit (0, 1, p.localLowPassSlope[source]);

        d.localLowPassActive[source] = lpControl < 0.9999f || lpResonance > epsilon;
        d.localHighPassActive[source] = hpControl > 0.0001f || hpResonance > epsilon;
        d.localLowPassSlope[source] = lpSlope;

        auto lpFrequency = localLpMinimum * std::pow (localLpMaximum / localLpMinimum, lpControl);
        if (lpSlope != 0)
            lpFrequency = std::max (20.0f, lpFrequency);
        d.localLowPassFrequency[source] = juce::jlimit (20.0f, d.lpCoefficientMaximum, lpFrequency);
        d.localLowPassQ[source] = 0.5f + lpResonance * lpResonance * 11.5f;

        const auto hpFrequency = localHpMinimum * std::pow (localHpMaximum / localHpMinimum, hpControl);
        d.localHighPassFrequency[source] = juce::jlimit (10.0f, d.hpCoefficientMaximum, hpFrequency);
        d.localHighPassQ[source] = 0.5f + hpResonance * hpResonance * 7.5f;
    }

    d.centredFinalPan = std::abs (p.panEnvelope) <= epsilon
                      && std::abs (p.pingPongPan) <= epsilon
                       && std::abs (p.noteScalePan) <= epsilon
                       && std::abs (p.pitchArpPanDepth) <= epsilon
                       && (! larpPlaysSynth (p.larp.state)
                           || std::abs (p.larp.panDepth) <= epsilon)
                      && (p.voiceCount <= 1
                          || std::abs (p.voicePanAlternate) <= epsilon);
    d.centrePanGain = d.centredFinalPan ? panGain (0.0f, true) : 0.0f;
    d.inputGain = juce::Decibels::decibelsToGain (p.inputGainDb);
    d.masterGain = juce::Decibels::decibelsToGain (p.masterVolumeDb);
    return d;
}

void SynthEngine::process (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi,
                           juce::AudioProcessorValueTreeState& state,
                           const SequencerState& sequencerState, double tempoBpm,
                           bool includeStereoInput, const float* sidechainLeft,
                           const float* sidechainRight,
                           const std::array<float*, 8>& auxOutputs,
                           std::uint64_t parameterRevision)
{
    deepIdle = false;
    if (! parameterCacheReady || parameterRevision != cachedParameterRevision
        || sequencerParameterLockRevision != cachedSequencerParameterLockRevision
        || tempoBpm != cachedParams.tempoBpm)
    {
        const auto previousParams = cachedParams;
        const auto previousRenderConstants = cachedRenderConstants;
        const auto hadCachedParams = parameterCacheReady;
        cachedParams = readParams (state, tempoBpm);
        cachedRenderConstants = makeRenderConstants (cachedParams);
        updateEffectCoefficients (cachedParams);
        // The JSFX rebuilds its block-rate increment and filter caches after
        // any slider change. Do the same here so the per-voice fast paths can
        // safely reuse their values for the rest of an unchanged block.
        for (auto& voice : voices)
        {
            voice.cachedPitchFrequency = -1.0;
            voice.lowPassCoefficientFrequency = -1.0f;
            voice.highPassCoefficientFrequency = -1.0f;

            if (! hadCachedParams)
                continue;

            if (voice.active && cachedRenderConstants.microMotionAny)
            {
                if (! previousRenderConstants.microMotionAny)
                {
                    seedVoiceMicroMotion (voice);
                }
                else if (cachedRenderConstants.microMotionAlpha
                         != previousRenderConstants.microMotionAlpha)
                {
                    const auto oldScale = microMotionStationaryScale (
                        previousRenderConstants.microMotionAlpha);
                    const auto newScale = microMotionStationaryScale (
                        cachedRenderConstants.microMotionAlpha);
                    const auto ratio = oldScale > 0.0f ? newScale / oldScale : 0.0f;
                    voice.microMotionCommon *= ratio;
                    voice.microMotionIndependent1 *= ratio;
                    voice.microMotionIndependent2 *= ratio;
                    voice.microMotionOut1 = voice.microMotionCommon * 0.60f
                                          + voice.microMotionIndependent1 * 0.40f;
                    voice.microMotionOut2 = voice.microMotionCommon * 0.60f
                                          + voice.microMotionIndependent2 * 0.40f;
                }
            }

            const auto sourceFilterSplit = [] (const Params& params)
            {
                const auto different = [] (float a, float b, float c)
                {
                    return std::abs (a - b) > epsilon || std::abs (a - c) > epsilon;
                };
                return different (params.lfo1LowPassL1, params.lfo1LowPassL2,
                                  params.lfo1LowPassNoise)
                    || different (params.lfo2LowPassL1, params.lfo2LowPassL2,
                                  params.lfo2LowPassNoise)
                    || different (params.lfo1HighPassL1, params.lfo1HighPassL2,
                                  params.lfo1HighPassNoise)
                    || different (params.lfo2HighPassL1, params.lfo2HighPassL2,
                                  params.lfo2HighPassNoise);
            };
            const auto modeChanged = previousParams.filterLayerRouting
                                  != cachedParams.filterLayerRouting
                                  || sourceFilterSplit (previousParams)
                                     != sourceFilterSplit (cachedParams);
            const auto layerRoutesChanged =
                   previousParams.lowPassLayer1 != cachedParams.lowPassLayer1
                || previousParams.lowPassLayer2 != cachedParams.lowPassLayer2
                || previousParams.highPassLayer1 != cachedParams.highPassLayer1
                || previousParams.highPassLayer2 != cachedParams.highPassLayer2
                || previousParams.formantLayer1 != cachedParams.formantLayer1
                || previousParams.formantLayer2 != cachedParams.formantLayer2;
            const auto noiseRoutesChanged =
                   previousParams.lowPassNoise != cachedParams.lowPassNoise
                || previousParams.highPassNoise != cachedParams.highPassNoise
                || previousParams.formantNoise != cachedParams.formantNoise;

            if (modeChanged || (noiseRoutesChanged && cachedParams.filterLayerRouting))
            {
                voice.lowPassLeft = voice.lowPassRight = {};
                voice.lowPass2Left = voice.lowPass2Right = {};
                voice.highPassLeft = voice.highPassRight = {};
                voice.formantLeft = voice.formantRight = {};
                voice.routedFilters = {};
                voice.formantWasEnabled = false;
                voice.formantPosition = -1.0f;
                voice.formantControlCounter = 0;
            }
            else if (layerRoutesChanged)
            {
                voice.routedFilters = {};
            }
        }
        pitchArpPoolDirty = true;
        larpState.chordDirty = true;
        cachedParameterRevision = parameterRevision;
        cachedSequencerParameterLockRevision = sequencerParameterLockRevision;
        parameterCacheReady = true;
    }

    const auto& p = cachedParams;
    const auto& renderConstants = cachedRenderConstants;
    const auto routingMode = juce::jlimit (0, 2, sequencerState.getRoutingMode());
    const auto sourceMidiRoutingActive = p.sourceMidiChannel[0] != 0
                                      || p.sourceMidiChannel[1] != 0
                                      || p.sourceMidiChannel[2] != 0;
    const auto independentPortamento = std::abs (p.portamento[0] - p.portamento[1]) > epsilon
                                   || std::abs (p.portamento[0] - p.portamento[2]) > epsilon
                                   || std::abs (p.portamento[1] - p.portamento[2]) > epsilon;
    const auto independentVoiceMode = p.sourceVoiceMode[0] != 0 || p.sourceVoiceMode[1] != 0;
    const auto sourceNoteRoutingActive = p.sourceNoteSource[0] != 0
                                      || p.sourceNoteSource[1] != 0
                                      || p.sourceNoteSource[2] != 0;
    const auto sequencerHasSynthTarget = std::any_of (p.sourceNoteSource.begin(),
                                                       p.sourceNoteSource.end(),
                                                       [] (int source) { return source == 1; });
    const auto larpHasSynthTarget = std::any_of (p.sourceNoteSource.begin(),
                                                 p.sourceNoteSource.end(),
                                                 [] (int source) { return source == 2; });
    // Global Direct is the neutral state. Synth-capable routing modes become
    // active only when at least one source selects that generator. MIDI Only is
    // intentionally usable by itself as a MIDI processor for a downstream synth.
    const auto sequencerEnabled = sequencerHasSynthTarget || routingMode == 2;
    const auto larpGeneratesNotes = larpHasSynthTarget || p.larp.state == 2;
    const auto independentVoiceRouting = sourceMidiRoutingActive || independentPortamento
                                      || independentVoiceMode || sourceNoteRoutingActive;

    // Changing source MIDI assignments while notes are held can otherwise leave
    // a note owned by the old source mask with no matching Note Off on the new
    // route. Treat a routing change as an all-notes reset; parameter changes are
    // infrequent and this keeps the three source registers deterministic.
    if (p.sourceMidiChannel != previousSourceMidiChannels
        || p.sourceNoteSource != previousSourceNoteSources
        || p.sourceVoiceMode != previousSourceVoiceModes
        || independentVoiceRouting != previousIndependentVoiceRouting)
    {
        voices = {};
        rolandVoice = 0;
        sequencerRolandVoice = {};
        monoNoteStack = {};
        monoVelocityStack = {};
        monoNoteCount = 0;
        sourceMonoNoteStack = {};
        sourceMonoVelocityStack = {};
        sourceMonoNoteCount = { 0, 0 };
        sustainPedal = { false, false, false };
        pitchBend = { 0.0f, 0.0f, 0.0f };
        modWheel = { 0.0f, 0.0f, 0.0f };
        channelAftertouch = { 0.0f, 0.0f, 0.0f };
        previousSourceMidiChannels = p.sourceMidiChannel;
        previousSourceNoteSources = p.sourceNoteSource;
        previousSourceVoiceModes = p.sourceVoiceMode;
        previousIndependentVoiceRouting = independentVoiceRouting;
    }
    // Three fixed sequencer lanes: Layer 1, Layer 2, and Noise.
    // Voices is intentionally unrelated: it remains only the synth polyphony.
    constexpr int activeSequenceCount = 3;
    std::array<SequencerConfig, SequencerState::maximumSequences> sequenceConfigs {};
    for (int i = 0; i < activeSequenceCount; ++i)
        sequenceConfigs[static_cast<std::size_t> (i)] = sequencerState.getConfig (i);

    // Parameter locks are synth-only. A lane that is no longer connected to its
    // internal source, or MIDI Only routing, must immediately release its locks.
    for (int i = 0; i < activeSequenceCount; ++i)
        if (routingMode == 2 || p.sourceNoteSource[static_cast<std::size_t> (i)] != 1)
            clearActiveSequencerParameterLocks (i);

    // LArp and Sequencer are independent MIDI generators. Both may run at the
    // same time; Global Note Source chooses which generator owns L1/L2/Noise.
    const juce::MidiBuffer inputMidi (midi);
    midi.clear();
    auto event = inputMidi.begin();

    // Disconnecting the last Global target must also clean up any generated
    // downstream note that was already active. Otherwise Direct could leave a
    // hanging arpeggiated/sequenced note in the next instrument.
    if (previousSequencerEnabled && ! sequencerEnabled)
        for (int i = 0; i < SequencerState::maximumSequences; ++i)
            stopSequencer (i, 0, midi, p, previousSequencerMode, true);
    if (previousLArpEnabled && ! larpGeneratesNotes)
    {
        releaseLArpOutput (0, midi, p, true);
        resetLArpState (true);
        larpState.previousMode = p.larp.state;
        larpState.inputChannelSetting = juce::jlimit (0, 16, p.larp.midiChannel);
        larpState.outputChannel = larpState.inputChannelSetting > 0
                                ? larpState.inputChannelSetting : 1;
    }
    previousSequencerEnabled = sequencerEnabled;
    previousLArpEnabled = larpGeneratesNotes;

    const auto previousMode = previousSequencerMode;
    if (routingMode != previousMode)
    {
        for (int i = 0; i < SequencerState::maximumSequences; ++i)
        {
            releaseSequencerNote (i, 0, midi, p, previousMode);
            const auto seed = sequencerRuntime[static_cast<std::size_t> (i)].randomSeed;
            sequencerRuntime[static_cast<std::size_t> (i)] = {};
            sequencerRuntime[static_cast<std::size_t> (i)].randomSeed =
                seed != 0 ? seed : 0x51e90001u + static_cast<std::uint32_t> (i * 0x001f123bu);
        }

        // The extra Layer-2/Noise registers are also used by direct per-source
        // MIDI routing. Discard them only when neither the sequencer nor source
        // MIDI routing needs the independent banks.
        if (routingMode == 0 && ! independentVoiceRouting)
        {
            for (int voiceIndex = layer2VoiceBankStart;
                 voiceIndex < static_cast<int> (voices.size()); ++voiceIndex)
                voices[static_cast<std::size_t> (voiceIndex)] = {};
            sequencerRolandVoice = {};
        }
        previousSequencerMode = routingMode;
    }

    // Legacy sequence slots above the three fixed lanes remain stored for preset
    // compatibility, but they are never active in the three-lane sequencer.
    for (int i = activeSequenceCount; i < SequencerState::maximumSequences; ++i)
        if (sequencerRuntime[static_cast<std::size_t> (i)].running
            || sequencerRuntime[static_cast<std::size_t> (i)].waitingForLaunch
            || sequencerRuntime[static_cast<std::size_t> (i)].currentNote >= 0)
            stopSequencer (i, 0, midi, p, routingMode, true);

    {
        const auto larpInputChannel = juce::jlimit (0, 16, p.larp.midiChannel);
        const auto larpModeChanged = p.larp.state != larpState.previousMode
                                  || larpInputChannel != larpState.inputChannelSetting;
        if (larpModeChanged)
        {
            releaseLArpOutput (0, midi, p, true);
            // Note Source isolates LArp from Direct/Sequencer parts, so changing
            // LArp mode/channel must not send a synth-wide All Notes Off.
            resetLArpState (true);
            larpState.previousMode = p.larp.state;
            larpState.inputChannelSetting = larpInputChannel;
            larpState.outputChannel = larpInputChannel > 0 ? larpInputChannel : 1;
        }

        if (p.larp.resetSustain && ! larpState.resetSustainWasDown)
        {
            releaseLArpOutput (0, midi, p, true);
            resetLArpState (true);
            larpState.previousMode = p.larp.state;
            larpState.inputChannelSetting = juce::jlimit (0, 16, p.larp.midiChannel);
            larpState.outputChannel = larpState.inputChannelSetting > 0
                                    ? larpState.inputChannelSetting : 1;
            larpState.resetSustainWasDown = true;
        }
        else if (! p.larp.resetSustain)
        {
            larpState.resetSustainWasDown = false;
        }
    }

    // A per-sequence MIDI input-channel change must release the previous output first.
    // Omni (0) accepts every input channel. Generated MIDI follows the concrete
    // channel that started the current phrase, while fixed 1..16 keeps the
    // historical same-channel input/output behaviour.
    for (int i = 0; i < activeSequenceCount; ++i)
    {
        auto& runtime = sequencerRuntime[static_cast<std::size_t> (i)];
        const auto inputChannel = juce::jlimit (0, 16,
            sequenceConfigs[static_cast<std::size_t> (i)].midiChannel);
        if (runtime.inputChannelSetting != inputChannel)
        {
            stopSequencer (i, 0, midi, p, routingMode, true);
            runtime.inputChannelSetting = inputChannel;
            runtime.outputChannel = inputChannel > 0 ? inputChannel : 1;
        }
    }

    auto* left = audio.getWritePointer (0);
    auto* right = audio.getNumChannels() > 1 ? audio.getWritePointer (1) : left;

    auto blockHadInput = false;
    auto blockOutputMagnitude = 0.0f;
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        while (event != inputMidi.end() && (*event).samplePosition <= sample)
        {
            const auto& message = (*event).getMessage();
            auto sequencerConsumed = false;
            if (sequencerEnabled)
                sequencerConsumed = handleSequencerInput (message, sample, midi, p,
                                                           sequencerState, sequenceConfigs,
                                                           activeSequenceCount, routingMode);

            if (larpGeneratesNotes)
                handleLArpInput (message, sample, midi, p);

            // Source performance controllers are always live. Note On/Off reaches
            // only sources whose Note Source is Direct; generated sources ignore
            // direct notes but still receive their own Pitch Bend/CC1/Aftertouch/CC64.
            handleSourceRoutedMidi (message, p);

            // Routing matrix:
            //   mode 0 = generated notes to synth + generated MIDI downstream
            //   mode 1 = generated notes to synth + original MIDI downstream
            //   mode 2 = direct synth + generated MIDI downstream
            // A generator may suppress the original event, but a second generator
            // must never re-enable it after the first one has consumed it.
            auto passOriginal = true;
            if (sequencerEnabled && sequencerConsumed && routingMode != 1)
                passOriginal = false;

            if (larpGeneratesNotes && message.isNoteOnOrOff())
            {
                const auto larpChannel = juce::jlimit (0, 16, p.larp.midiChannel);
                const auto larpMatches = larpChannel == 0 || message.isForChannel (larpChannel);
                if (larpMatches && ! larpUsesDirectMidiOutput (p.larp.state))
                    passOriginal = false;
            }
            // In modes that send generated LArp MIDI, handleLArpInput() already
            // forwards controller/pressure/bend traffic once, so avoid duplicates.
            if (larpGeneratesNotes && ! message.isNoteOnOrOff()
                && larpSendsArpeggiatedMidi (p.larp.state))
                passOriginal = false;

            if (passOriginal)
                midi.addEvent (message, (*event).samplePosition);
            ++event;
        }

        if (sequencerEnabled)
            advanceSequencers (sample, midi, p, state, tempoBpm, sequencerState,
                               sequenceConfigs, activeSequenceCount, routingMode);
        if (cachedSequencerParameterLockRevision != sequencerParameterLockRevision)
            refreshCachedParamsForSequencerLocks (state, tempoBpm);
        if (larpGeneratesNotes)
            advanceLArp (sample, midi, p);

        const auto inputLeft = includeStereoInput ? left[sample] : 0.0f;
        const auto inputRight = includeStereoInput ? right[sample] : 0.0f;
        blockHadInput = blockHadInput || inputLeft != 0.0f || inputRight != 0.0f;
        const auto detectorLeft = sidechainLeft != nullptr ? sidechainLeft[sample] : 0.0f;
        const auto detectorRight = sidechainRight != nullptr ? sidechainRight[sample] : 0.0f;
        float l = 0.0f, r = 0.0f;
        std::array<float, 8> aux {};
        render (l, r, aux, p, renderConstants, inputLeft, inputRight,
                detectorLeft, detectorRight);
        blockOutputMagnitude = std::max (blockOutputMagnitude,
                                         std::max (std::abs (l), std::abs (r)));
        left[sample] = l;
        right[sample] = r;
        for (int channel = 0; channel < 8; ++channel)
            if (auto* destination = auxOutputs[static_cast<std::size_t> (channel)])
            {
                destination[sample] = aux[static_cast<std::size_t> (channel)];
                blockOutputMagnitude = std::max (blockOutputMagnitude,
                                                  std::abs (aux[static_cast<std::size_t> (channel)]));
            }
    }

    const auto voicesLive = std::any_of (voices.begin(), voices.end(),
                                         [] (const Voice& voice) { return voice.active; });
    const auto effectsQuiet = chorusTail <= 0.00000001f
                           && delaySilentSamples >= delayBufferLeft.size()
                           && reverbTail <= 0.00002f
                           && blockOutputMagnitude <= 0.0000001f;
    const auto larpClockActive = larpGeneratesNotes
                              && (larpState.heldCount > 0 || larpState.currentNote >= 0);
    auto sequencerClockActive = false;
    if (sequencerEnabled)
        for (int i = 0; i < activeSequenceCount; ++i)
        {
            const auto& runtime = sequencerRuntime[static_cast<std::size_t> (i)];
            sequencerClockActive = sequencerClockActive || runtime.running
                                 || runtime.waitingForLaunch || runtime.heldCount > 0
                                 || runtime.currentNote >= 0;
        }
    if (! voicesLive && ! blockHadInput && effectsQuiet
        && ! larpClockActive && ! sequencerClockActive)
        enterDeepIdle();
}

float SynthEngine::sequencerRandom (SequencerRuntime& runtime) noexcept
{
    auto& seed = runtime.randomSeed;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return static_cast<float> (seed & 0x00ffffffu) / 16777216.0f;
}

double SynthEngine::sequencerStepSamples (const SequencerConfig& config,
                                           const Params& p) const noexcept
{
    const auto tempo = juce::jlimit (1.0, 999.0, p.tempoBpm);
    const auto division = juce::jlimit (0.03125, 64.0,
                                        static_cast<double> (config.bpmDivision));
    return juce::jmax (1.0, sampleRate * 60.0 / tempo / division);
}

void SynthEngine::emitSequencerMessage (int sequenceIndex, const juce::MidiMessage& message,
                                         int sampleOffset, juce::MidiBuffer& output,
                                         const Params& p, int routingMode,
                                         int portamentoFromNote)
{
    const auto clampedOffset = std::max (0, sampleOffset);
    if (routingMode == 0 || routingMode == 2)
        output.addEvent (message, clampedOffset);
    if ((routingMode == 0 || routingMode == 1)
        && juce::isPositiveAndBelow (sequenceIndex, 3)
        && p.sourceNoteSource[static_cast<std::size_t> (sequenceIndex)] == 1)
    {
        // Sequence 1 owns L1, Sequence 2 owns L2 and Sequence 3 owns Noise only
        // when that source explicitly selects Sequencer as its Note Source.
        const auto layerMask = 1 << sequenceIndex;
        const auto source = static_cast<std::size_t> (sequenceIndex);

        // Sequencer step controllers belong to the lane/source that generated
        // them. Keep CC1, pressure, bend and sustain source-local instead of
        // falling through the historical synth-wide controller path.
        if (message.isController() && message.getControllerNumber() == 1)
        {
            modWheel[source] = message.getControllerValue() / 127.0f;
        }
        else if (message.isChannelPressure() || message.isAftertouch())
        {
            channelAftertouch[source] = message.isChannelPressure()
                ? message.getChannelPressureValue() / 127.0f
                : message.getAfterTouchValue() / 127.0f;
        }
        else if (message.isPitchWheel())
        {
            pitchBend[source] = juce::jlimit (-1.0f, 1.0f,
                (message.getPitchWheelValue() - 8192) / 8192.0f);
        }
        else if (message.isController() && message.getControllerNumber() == 64)
        {
            const auto sustainNow = message.getControllerValue() >= 64;
            sustainPedal[source] = sustainNow;
            if (! sustainNow)
            {
                for (auto& v : voices)
                {
                    if (! v.active || v.held || (v.layerMask & layerMask) == 0
                        || sustainForMask (v.layerMask))
                        continue;
                    v.stage = Stage::release;
                    if (v.stage2 != Stage::idle)
                        v.stage2 = Stage::release;
                }
            }
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (auto& v : voices)
            {
                if (! v.active || (v.layerMask & layerMask) == 0)
                    continue;
                v.held = false;
                v.stage = Stage::release;
                if (v.stage2 != Stage::idle)
                    v.stage2 = Stage::release;
            }
        }
        else
        {
            handleMidi (message, p, layerMask);
            if (message.isNoteOn() && portamentoFromNote >= 0
                && portamentoForMask (p, layerMask) > epsilon)
            {
                // Sequencer glide must follow the musical transition between
                // consecutive sequence notes, not whichever free/release voice
                // the allocator happened to return. This makes a Portamento
                // parameter lock behave exactly as the synth control does, but
                // with the previous sequencer note as the glide origin.
                const auto startFrequency = 440.0
                    * std::exp2 ((juce::jlimit (0, 127, portamentoFromNote) - 69) / 12.0);
                Voice* newest = nullptr;
                // A queued replacement must win over an already sounding voice
                // with the same note number, because the queued register is the
                // one that will become this sequencer transition.
                for (auto& voice : voices)
                    if (voice.pending && voice.pendingLayerMask == layerMask
                        && voice.pendingNote == message.getNoteNumber())
                    {
                        newest = &voice;
                        break;
                    }
                if (newest == nullptr)
                    for (auto& voice : voices)
                        if (voice.active && voice.held && voice.layerMask == layerMask
                            && voice.note == message.getNoteNumber()
                            && (newest == nullptr || voice.age >= newest->age))
                            newest = &voice;
                if (newest != nullptr)
                {
                    newest->frequency = std::max (1.0, startFrequency);
                    newest->cachedPitchFrequency = -1.0;
                }
            }
        }
    }
}

void SynthEngine::releaseSequencerNote (int sequenceIndex, int sampleOffset,
                                         juce::MidiBuffer& output, const Params& p,
                                         int routingMode)
{
    if (! juce::isPositiveAndBelow (sequenceIndex,
                                    static_cast<int> (sequencerRuntime.size())))
        return;
    auto& runtime = sequencerRuntime[static_cast<std::size_t> (sequenceIndex)];

    if (runtime.currentNote >= 0)
    {
        emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOff (
                                  juce::jlimit (1, 16, runtime.outputChannel),
                                  juce::jlimit (0, 127, runtime.currentNote)),
                              sampleOffset, output, p, routingMode);
        runtime.currentNote = -1;
    }

    for (int inputNote = 0; inputNote < 128; ++inputNote)
    {
        const auto index = static_cast<std::size_t> (inputNote);
        if (! runtime.activePolyVoice[index])
            continue;
        emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOff (
                                  juce::jlimit (1, 16, runtime.outputChannel),
                                  juce::jlimit (0, 127, runtime.activePolyOutputNote[index])),
                              sampleOffset, output, p, routingMode);
        runtime.activePolyVoice[index] = false;
        runtime.activePolyOutputNote[index] = 0;
    }

    runtime.noteTimer = 0.0;
    runtime.activeDuration = 0.0;
    runtime.activeLegato = false;
}

void SynthEngine::stopSequencer (int sequenceIndex, int sampleOffset,
                                  juce::MidiBuffer& output, const Params& p,
                                  int routingMode, bool clearHeld)
{
    if (! juce::isPositiveAndBelow (sequenceIndex,
                                    static_cast<int> (sequencerRuntime.size())))
        return;
    auto& runtime = sequencerRuntime[static_cast<std::size_t> (sequenceIndex)];
    releaseSequencerNote (sequenceIndex, sampleOffset, output, p, routingMode);
    clearActiveSequencerParameterLocks (sequenceIndex);
    runtime.running = false;
    runtime.waitingForLaunch = false;
    runtime.launchRemaining = 0.0;
    runtime.stepTimer = 0.0;
    runtime.stepInterval = 0.0;
    runtime.repeatCounter = 0;
    runtime.activeRepeatTarget = 1;
    runtime.shufflePhase = 0;
    if (clearHeld)
    {
        runtime.held.fill (false);
        runtime.heldVelocity.fill (0);
        runtime.heldAge.fill (0);
        runtime.heldAgeCounter = 1;
        runtime.heldCount = 0;
        runtime.inputNote = -1;
        runtime.previousInputNote = -1;
    }
}

void SynthEngine::startSequencer (int sequenceIndex, int note, int velocity,
                                   const SequencerConfig& config)
{
    if (! juce::isPositiveAndBelow (sequenceIndex,
                                    static_cast<int> (sequencerRuntime.size())))
        return;
    auto& runtime = sequencerRuntime[static_cast<std::size_t> (sequenceIndex)];
    runtime.inputNote = juce::jlimit (0, 127, note);
    // previousInputNote is managed by handleSequencerInput().  Do not overwrite it
    // here: Next Trigger and All Trigger need the already-held previous note so
    // releasing the newest key can return to it (with or without retrigger).
    runtime.triggerVelocity = juce::jlimit (1, 127, velocity);
    runtime.baseTranspose = runtime.inputNote - 60;
    runtime.direction = config.playbackMode == 2 ? -1 : 1;
    runtime.position = config.playbackMode == 2 ? config.endStep - 1 : config.startStep - 1;
    runtime.position = juce::jlimit (0, SequencerState::stepsPerSequence - 1,
                                     runtime.position);
    runtime.repeatCounter = 0;
    runtime.activeRepeatTarget = 1;
    runtime.shufflePhase = 0;
    runtime.stepTimer = 0.0;
    runtime.stepInterval = 0.0;
    runtime.noteTimer = 0.0;
    runtime.activeDuration = 0.0;
    runtime.activeLegato = false;

    const auto base = sequencerStepSamples (config, cachedParams);
    const auto coarse = static_cast<double> (juce::jmax (0, config.launchStep - 1)) * base;
    auto fine = static_cast<double> (config.launchOffsetMs) * sampleRate / 1000.0;
    if (config.launchStep <= 1)
        fine = juce::jmax (0.0, fine);
    runtime.launchRemaining = juce::jmax (0.0, coarse + fine);

    // A physical chord normally arrives as a short train of Note Ons rather than
    // as one atomic MIDI event.  Starting Poly on the very first Note On made the
    // first step get cut and restarted while the rest of the chord was still
    // arriving, most noticeably in All Trigger.  Give Poly a short 10 ms capture window
    // before the first step.  Existing Launch Step / Launch Offset remain in
    // control when they already request a longer delay.  Repeated All Trigger
    // Note Ons call startSequencer() again, so this also acts as a debounce: the
    // first step is emitted once, with the complete chord collected so far.
    if (config.midiInputPolyphony != 0 && sequenceIndex < 2)
    {
        constexpr double chordCaptureMs = 10.0;
        const auto chordCaptureSamples = sampleRate * chordCaptureMs / 1000.0;
        runtime.launchRemaining = juce::jmax (runtime.launchRemaining,
                                               chordCaptureSamples);
    }

    runtime.waitingForLaunch = runtime.launchRemaining > 0.0;
    runtime.running = ! runtime.waitingForLaunch;
}

int SynthEngine::nextSequencerPosition (SequencerRuntime& runtime,
                                         const SequencerConfig& config, bool commit)
{
    const auto first = juce::jlimit (0, SequencerState::stepsPerSequence - 1,
                                     config.startStep - 1);
    const auto last = juce::jlimit (first, SequencerState::stepsPerSequence - 1,
                                    config.endStep - 1);
    auto position = juce::jlimit (first, last, runtime.position);
    auto direction = runtime.direction == 0 ? 1 : runtime.direction;

    switch (config.playbackMode)
    {
        case 1: // Free: stop is signalled by returning one position past the range.
            position += direction;
            break;
        case 2: // Reverse
            --position;
            if (position < first)
                position = last;
            direction = -1;
            break;
        case 3: // Pendulum
            position += direction;
            if (position > last)
            {
                direction = -1;
                position = juce::jmax (first, last - 1);
            }
            else if (position < first)
            {
                direction = 1;
                position = juce::jmin (last, first + 1);
            }
            break;
        case 4: // Random
        {
            const auto count = juce::jmax (1, last - first + 1);
            position = first + juce::jlimit (0, count - 1,
                static_cast<int> (sequencerRandom (runtime) * static_cast<float> (count)));
            break;
        }
        default: // Cyclic
            ++position;
            if (position > last)
                position = first;
            direction = 1;
            break;
    }

    if (commit)
    {
        runtime.position = position;
        runtime.direction = direction;
    }
    return position;
}

void SynthEngine::triggerSequencerStep (int sequenceIndex, int sampleOffset,
                                         juce::MidiBuffer& output, const Params& p,
                                         juce::AudioProcessorValueTreeState& parameterState,
                                         double tempoBpm, const SequencerState& state,
                                         const SequencerConfig& config,
                                         int routingMode, bool previousLegato)
{
    auto& runtime = sequencerRuntime[static_cast<std::size_t> (sequenceIndex)];
    const auto stepIndex = juce::jlimit (0, SequencerState::stepsPerSequence - 1,
                                         runtime.position);
    const auto step = state.getStep (sequenceIndex, stepIndex);
    const auto baseSamples = sequencerStepSamples (config, p);
    // Noise has one generator by design.  Poly input is meaningful only for the
    // two pitched oscillator lanes; Sequence 3 (Noise) always follows the existing
    // monophonic last-note-priority path.
    const auto polyInput = config.midiInputPolyphony != 0 && sequenceIndex < 2;

    // When the mode changes while running, clean up notes created by the other mode
    // before the next step is generated.
    if (polyInput && runtime.currentNote >= 0)
        releaseSequencerNote (sequenceIndex, sampleOffset, output, p, routingMode);
    else if (! polyInput
             && std::any_of (runtime.activePolyVoice.begin(), runtime.activePolyVoice.end(),
                             [] (bool active) { return active; }))
        releaseSequencerNote (sequenceIndex, sampleOffset, output, p, routingMode);

    // Repeat randomization follows the JSFX idea: depth controls the chance of
    // substituting a musically bounded 1..16 repeat count.
    if (runtime.repeatCounter == 0)
    {
        auto repeatTarget = juce::jlimit (1, 16, step.repeat);
        const auto repeatDepth = juce::jlimit (0.0f, 1.0f, config.repeatRandomDepth);
        if (repeatDepth > 0.0f)
            repeatTarget = 1 + static_cast<int> (sequencerRandom (runtime)
                                                  * 16.0f * repeatDepth);
        runtime.activeRepeatTarget = juce::jlimit (1, 16, repeatTarget);
        const auto lockTargetsSynth = sequenceIndex >= 0 && sequenceIndex < 3
                                   && p.sourceNoteSource[static_cast<std::size_t> (sequenceIndex)] == 1
                                   && routingMode != 2;
        if (lockTargetsSynth)
            setActiveSequencerParameterLocks (sequenceIndex, step);
        else
            clearActiveSequencerParameterLocks (sequenceIndex);

        // Apply the lock cache before this step emits its Note On. This matters
        // for parameters sampled during voice creation (for example Portamento,
        // Poly Mode and ADSR2 usage), not only for parameters read while rendering.
        if (cachedSequencerParameterLockRevision != sequencerParameterLockRevision)
            refreshCachedParamsForSequencerLocks (parameterState, tempoBpm);
    }

    auto gate = juce::jmax (0.01f, step.length / 100.0f);
    const auto lengthRandom = juce::jlimit (0.0f, 1.0f, config.noteLengthRandomDepth);
    if (lengthRandom > 0.0f && sequencerRandom (runtime) < lengthRandom)
    {
        auto r = 0.0f;
        for (int n = 0; n < 6; ++n)
            r += sequencerRandom (runtime) - 0.5f;
        r = (r / 3.0f) + 0.5f;
        gate = juce::jlimit (0.01f, 2.5f, gate * r * 3.0f);
    }
    const auto desiredDuration = juce::jmax (1.0, baseSamples * static_cast<double> (gate));
    runtime.activeDuration = desiredDuration;
    runtime.noteTimer = 0.0;
    const auto newLegato = config.legato && step.length >= 100;

    const auto skip = step.note <= 0
                   || sequencerRandom (runtime) < juce::jlimit (0.0f, 1.0f,
                                                                config.noteSkipProbability);
    if (skip)
    {
        releaseSequencerNote (sequenceIndex, sampleOffset, output, p, routingMode);
    }
    else if (polyInput)
    {
        // One random pitch displacement belongs to the step, not to each chord note,
        // so the interval structure played by the user is preserved exactly.
        auto randomPitchOffset = 0;
        if (config.noteRandomDepth > 0.0f)
        {
            const auto depth = juce::jlimit (0.0f, 4.0f, config.noteRandomDepth);
            const auto span = juce::jmax (1, juce::roundToInt (depth * 12.0f));
            randomPitchOffset = juce::roundToInt ((sequencerRandom (runtime) * 2.0f - 1.0f)
                                                   * static_cast<float> (span));
        }

        auto randomVelocityOffset = 0;
        if (config.velocityRandomDepth > 0.0f)
        {
            const auto depth = juce::jlimit (0.0f, 4.0f, config.velocityRandomDepth);
            auto r = 0.0f;
            for (int n = 0; n < 6; ++n)
                r += sequencerRandom (runtime) - 0.5f;
            r = juce::jlimit (-1.0f, 1.0f, r / 3.0f);
            randomVelocityOffset = juce::roundToInt (r * 12.0f * depth);
        }

        // Without legato, every step retriggers the complete currently held chord.
        if (! (previousLegato && newLegato))
            releaseSequencerNote (sequenceIndex, sampleOffset, output, p, routingMode);

        for (int inputNote = 0; inputNote < 128; ++inputNote)
        {
            const auto index = static_cast<std::size_t> (inputNote);
            if (! runtime.held[index])
            {
                if (runtime.activePolyVoice[index])
                {
                    emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOff (
                                              runtime.outputChannel,
                                              juce::jlimit (0, 127, runtime.activePolyOutputNote[index])),
                                          sampleOffset, output, p, routingMode);
                    runtime.activePolyVoice[index] = false;
                    runtime.activePolyOutputNote[index] = 0;
                }
                continue;
            }

            auto note = inputNote + (step.note - 60)
                      + config.octaveShift * 12 + config.semitoneShift
                      + randomPitchOffset;
            note = juce::jlimit (0, 127, note);

            auto velocity = step.velocity
                          - (127 - juce::jlimit (1, 127, runtime.heldVelocity[index]))
                          + randomVelocityOffset;
            velocity = juce::jlimit (1, 127, velocity);

            if (runtime.activePolyVoice[index])
            {
                const auto oldNote = runtime.activePolyOutputNote[index];
                if (oldNote == note)
                    continue;

                // Legato moves this chord voice to its new relative note without
                // retriggering the voices whose output note did not change.
                emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOn (
                                          runtime.outputChannel, note,
                                          static_cast<juce::uint8> (velocity)),
                                      sampleOffset, output, p, routingMode, oldNote);
                emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOff (
                                          runtime.outputChannel,
                                          juce::jlimit (0, 127, oldNote)),
                                      sampleOffset, output, p, routingMode);
                runtime.activePolyOutputNote[index] = note;
            }
            else
            {
                emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOn (
                                          runtime.outputChannel, note,
                                          static_cast<juce::uint8> (velocity)),
                                      sampleOffset, output, p, routingMode);
                runtime.activePolyVoice[index] = true;
                runtime.activePolyOutputNote[index] = note;
            }
        }

        // releaseSequencerNote() clears timing state, so restore the duration for
        // the newly generated polyphonic step.
        runtime.activeDuration = desiredDuration;
        runtime.noteTimer = 0.0;
    }
    else
    {
        // Historical monophonic sequencer path: keep its behaviour unchanged.
        auto note = step.note + runtime.baseTranspose
                  + config.octaveShift * 12 + config.semitoneShift;
        if (config.noteRandomDepth > 0.0f)
        {
            const auto depth = juce::jlimit (0.0f, 4.0f, config.noteRandomDepth);
            const auto span = juce::jmax (1, juce::roundToInt (depth * 12.0f));
            note += juce::roundToInt ((sequencerRandom (runtime) * 2.0f - 1.0f)
                                      * static_cast<float> (span));
        }
        note = juce::jlimit (0, 127, note);

        auto velocity = step.velocity - (127 - runtime.triggerVelocity);
        if (config.velocityRandomDepth > 0.0f)
        {
            const auto depth = juce::jlimit (0.0f, 4.0f, config.velocityRandomDepth);
            auto r = 0.0f;
            for (int n = 0; n < 6; ++n)
                r += sequencerRandom (runtime) - 0.5f;
            r = juce::jlimit (-1.0f, 1.0f, r / 3.0f);
            velocity += juce::roundToInt (r * 12.0f * depth);
        }
        velocity = juce::jlimit (1, 127, velocity);

        if (runtime.currentNote >= 0 && runtime.currentNote != note)
        {
            const auto previousSequenceNote = runtime.currentNote;
            if (previousLegato && newLegato)
            {
                emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOn (
                                          runtime.outputChannel, note,
                                          static_cast<juce::uint8> (velocity)),
                                      sampleOffset, output, p, routingMode,
                                      previousSequenceNote);
                releaseSequencerNote (sequenceIndex, sampleOffset, output, p, routingMode);
                runtime.currentNote = note;
            }
            else
            {
                releaseSequencerNote (sequenceIndex, sampleOffset, output, p, routingMode);
                emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOn (
                                          runtime.outputChannel, note,
                                          static_cast<juce::uint8> (velocity)),
                                      sampleOffset, output, p, routingMode,
                                      previousSequenceNote);
                runtime.currentNote = note;
            }
        }
        else if (runtime.currentNote != note)
        {
            emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOn (
                                      runtime.outputChannel, note,
                                      static_cast<juce::uint8> (velocity)),
                                  sampleOffset, output, p, routingMode);
            runtime.currentNote = note;
        }
        else if (! previousLegato)
        {
            releaseSequencerNote (sequenceIndex, sampleOffset, output, p, routingMode);
            emitSequencerMessage (sequenceIndex, juce::MidiMessage::noteOn (
                                      runtime.outputChannel, note,
                                      static_cast<juce::uint8> (velocity)),
                                  sampleOffset, output, p, routingMode);
            runtime.currentNote = note;
        }
    }

    runtime.activeLegato = ! skip && newLegato;

    auto interval = baseSamples;
    const auto shuffle = juce::jlimit (0.0f, 1.0f, config.shuffle);
    if (shuffle > 0.0f)
        interval *= runtime.shufflePhase == 0 ? (1.0 + 0.5 * shuffle)
                                             : (1.0 - 0.5 * shuffle);
    runtime.shufflePhase ^= 1;

    const auto shift = (juce::jlimit (0.0f, 1.0f, step.shift) - 0.5f) * 2.0f;
    const auto shiftSamples = juce::jlimit (-baseSamples * 0.45, baseSamples * 0.45,
                                             static_cast<double> (shift) * baseSamples * 0.5);
    runtime.stepTimer = -shiftSamples;
    runtime.stepInterval = juce::jmax (baseSamples * 0.1, interval);
}

void SynthEngine::advanceSequencers (int sampleOffset, juce::MidiBuffer& output,
                                      const Params& p,
                                      juce::AudioProcessorValueTreeState& parameterState,
                                      double tempoBpm, const SequencerState& state,
                                      const std::array<SequencerConfig, SequencerState::maximumSequences>& configs,
                                      int activeSequenceCount, int routingMode)
{
    for (int i = 0; i < activeSequenceCount; ++i)
    {
        auto& runtime = sequencerRuntime[static_cast<std::size_t> (i)];
        const auto& config = configs[static_cast<std::size_t> (i)];

        if (runtime.waitingForLaunch)
        {
            runtime.launchRemaining -= 1.0;
            if (runtime.launchRemaining > 0.0)
                continue;
            runtime.waitingForLaunch = false;
            runtime.running = true;
            runtime.stepTimer = 0.0;
            runtime.launchRemaining = 0.0;
        }
        if (! runtime.running)
            continue;

        if (runtime.stepTimer <= 0.0 && runtime.stepInterval <= 0.0)
        {
            const auto previousLegato = runtime.activeLegato;
            triggerSequencerStep (i, sampleOffset, output, p, parameterState, tempoBpm,
                                  state, config, routingMode, previousLegato);
        }

        runtime.stepTimer += 1.0;
        runtime.noteTimer += 1.0;

        const auto hasPolyNotes = std::any_of (runtime.activePolyVoice.begin(),
                                                   runtime.activePolyVoice.end(),
                                                   [] (bool active) { return active; });
        if ((runtime.currentNote >= 0 || hasPolyNotes) && ! runtime.activeLegato
            && runtime.noteTimer >= runtime.activeDuration)
            releaseSequencerNote (i, sampleOffset, output, p, routingMode);

        if (runtime.stepTimer < runtime.stepInterval)
            continue;

        const auto previousLegato = runtime.activeLegato;
        ++runtime.repeatCounter;
        if (runtime.repeatCounter < runtime.activeRepeatTarget)
        {
            runtime.stepTimer = 0.0;
            triggerSequencerStep (i, sampleOffset, output, p, parameterState, tempoBpm,
                                  state, config, routingMode, previousLegato);
            continue;
        }

        runtime.repeatCounter = 0;
        if (config.playbackMode == 1)
        {
            if (runtime.position >= config.endStep - 1)
            {
                stopSequencer (i, sampleOffset, output, p, routingMode, false);
                continue;
            }
            ++runtime.position;
        }
        else
        {
            nextSequencerPosition (runtime, config, true);
        }
        runtime.stepTimer = 0.0;
        triggerSequencerStep (i, sampleOffset, output, p, parameterState, tempoBpm,
                              state, config, routingMode, previousLegato);
    }
}

bool SynthEngine::handleSequencerInput (
    const juce::MidiMessage& message, int sampleOffset, juce::MidiBuffer& output,
    const Params& p, const SequencerState& state,
    const std::array<SequencerConfig, SequencerState::maximumSequences>& configs,
    int activeSequenceCount, int routingMode)
{
    juce::ignoreUnused (state);
    if (! message.isNoteOnOrOff())
    {
        if (message.isController()
            && (message.getControllerNumber() == 120
                || message.getControllerNumber() == 121
                || message.getControllerNumber() == 123))
        {
            for (int i = 0; i < activeSequenceCount; ++i)
                if (configs[static_cast<std::size_t> (i)].midiChannel == 0
                    || message.isForChannel (configs[static_cast<std::size_t> (i)].midiChannel))
                    stopSequencer (i, sampleOffset, output, p, routingMode, true);
        }
        return false;
    }

    auto consumed = false;
    for (int i = 0; i < activeSequenceCount; ++i)
    {
        const auto& config = configs[static_cast<std::size_t> (i)];
        const auto configuredChannel = juce::jlimit (0, 16, config.midiChannel);
        if (configuredChannel != 0 && ! message.isForChannel (configuredChannel))
            continue;
        consumed = true;
        auto& runtime = sequencerRuntime[static_cast<std::size_t> (i)];
        const auto note = juce::jlimit (0, 127, message.getNoteNumber());
        // Layer 1 and Layer 2 may follow every held key. Noise remains
        // intentionally monophonic even if an older preset stored Poly here.
        const auto polyInput = config.midiInputPolyphony != 0 && i < 2;

        if (message.isNoteOn())
        {
            if (configuredChannel == 0 && runtime.heldCount == 0)
                runtime.outputChannel = juce::jlimit (1, 16, message.getChannel());

            if (! runtime.held[static_cast<std::size_t> (note)])
            {
                runtime.held[static_cast<std::size_t> (note)] = true;
                ++runtime.heldCount;
            }

            // True monophonic last-note priority. Every held key keeps its place
            // in the stack, so C-E-G returns G -> E -> C as keys are released.
            runtime.heldVelocity[static_cast<std::size_t> (note)] =
                juce::jlimit (1, 127, static_cast<int> (message.getVelocity()));
            runtime.heldAge[static_cast<std::size_t> (note)] = runtime.heldAgeCounter++;
            if (runtime.heldAgeCounter == 0)
            {
                // Practically unreachable, but keep ordering valid after wrap.
                std::uint64_t nextAge = 1;
                for (int n = 0; n < 128; ++n)
                    if (runtime.held[static_cast<std::size_t> (n)])
                        runtime.heldAge[static_cast<std::size_t> (n)] = nextAge++;
                runtime.heldAgeCounter = nextAge;
            }

            if (polyInput)
            {
                runtime.previousInputNote = runtime.inputNote;
                runtime.inputNote = note;
                runtime.baseTranspose = note - 60;
                runtime.triggerVelocity = runtime.heldVelocity[static_cast<std::size_t> (note)];

                // In Poly, Next Trigger, Legato and Return Trigger keep the shared
                // sequencer phase. The new chord voice joins on the next generated
                // step. All Trigger is the explicit whole-chord restart mode.
                if (! runtime.running && ! runtime.waitingForLaunch)
                    startSequencer (i, note, runtime.triggerVelocity, config);
                else if (config.midiInputMode == 2)
                {
                    releaseSequencerNote (i, sampleOffset, output, p, routingMode);
                    startSequencer (i, note, runtime.triggerVelocity, config);
                }
                continue;
            }

            runtime.previousInputNote = runtime.inputNote;
            const auto doRetrigger = (! runtime.running && ! runtime.waitingForLaunch)
                                  || config.midiInputMode == 2
                                  || (config.midiInputMode == 0 && runtime.heldCount > 1);
            if (doRetrigger)
            {
                releaseSequencerNote (i, sampleOffset, output, p, routingMode);
                startSequencer (i, note,
                                runtime.heldVelocity[static_cast<std::size_t> (note)],
                                config);
            }
            else
            {
                if (runtime.inputNote >= 0)
                    runtime.baseTranspose += note - runtime.inputNote;
                else
                    runtime.baseTranspose = note - 60;
                runtime.inputNote = note;
                runtime.triggerVelocity = runtime.heldVelocity[static_cast<std::size_t> (note)];
            }
            continue;
        }

        if (runtime.held[static_cast<std::size_t> (note)])
        {
            runtime.held[static_cast<std::size_t> (note)] = false;
            runtime.heldVelocity[static_cast<std::size_t> (note)] = 0;
            runtime.heldAge[static_cast<std::size_t> (note)] = 0;
            runtime.heldCount = juce::jmax (0, runtime.heldCount - 1);
        }

        if (polyInput)
        {
            const auto noteIndex = static_cast<std::size_t> (note);
            if (runtime.activePolyVoice[noteIndex])
            {
                emitSequencerMessage (i, juce::MidiMessage::noteOff (
                                          runtime.outputChannel,
                                          juce::jlimit (0, 127, runtime.activePolyOutputNote[noteIndex])),
                                      sampleOffset, output, p, routingMode);
                runtime.activePolyVoice[noteIndex] = false;
                runtime.activePolyOutputNote[noteIndex] = 0;
            }

            if (runtime.heldCount <= 0)
            {
                stopSequencer (i, sampleOffset, output, p, routingMode, true);
                continue;
            }

            // Keep last-note information valid in case the user switches back to
            // Mono while keys are still held.
            int fallback = -1;
            std::uint64_t newestAge = 0;
            for (int n = 0; n < 128; ++n)
            {
                const auto index = static_cast<std::size_t> (n);
                if (runtime.held[index] && runtime.heldAge[index] >= newestAge)
                {
                    newestAge = runtime.heldAge[index];
                    fallback = n;
                }
            }
            runtime.previousInputNote = runtime.inputNote;
            runtime.inputNote = fallback;
            if (fallback >= 0)
            {
                runtime.baseTranspose = fallback - 60;
                runtime.triggerVelocity = juce::jlimit (
                    1, 127, runtime.heldVelocity[static_cast<std::size_t> (fallback)]);
            }

            // All Trigger reacts to every chord change. Return Trigger keeps its
            // historical meaning on note release: the remaining chord restarts.
            // Use the same start path as Note On so the restart is guaranteed to
            // begin from Start Step and, in Poly, benefits from the short chord
            // capture window instead of firing a weak/truncated first step.
            if (config.midiInputMode == 2 || config.midiInputMode == 3)
            {
                releaseSequencerNote (i, sampleOffset, output, p, routingMode);
                startSequencer (i, fallback, runtime.triggerVelocity, config);
            }
            continue;
        }

        // Releasing a non-current key only removes it from the held-note stack.
        if (note != runtime.inputNote)
        {
            if (runtime.heldCount == 0)
                stopSequencer (i, sampleOffset, output, p, routingMode, true);
            continue;
        }

        if (runtime.heldCount <= 0)
        {
            stopSequencer (i, sampleOffset, output, p, routingMode, true);
            continue;
        }

        // Return to the most recently pressed key that is still physically held.
        int fallback = -1;
        std::uint64_t newestAge = 0;
        for (int n = 0; n < 128; ++n)
        {
            const auto index = static_cast<std::size_t> (n);
            if (runtime.held[index] && runtime.heldAge[index] >= newestAge)
            {
                newestAge = runtime.heldAge[index];
                fallback = n;
            }
        }

        if (fallback >= 0)
        {
            runtime.previousInputNote = runtime.inputNote;
            runtime.inputNote = fallback;
            runtime.baseTranspose = fallback - 60;
            runtime.triggerVelocity = juce::jlimit (
                1, 127, runtime.heldVelocity[static_cast<std::size_t> (fallback)]);

            // MIDI Input Legato is deliberately stronger than step Legato: as long
            // as at least one physical key remains held, changing the last-note
            // priority must never release, restart or reposition the sequencer.
            // The new transposition is simply picked up by the running phrase.
            if (config.midiInputMode == 1)
                continue;

            releaseSequencerNote (i, sampleOffset, output, p, routingMode);

            // All Trigger and Return Trigger restart on the return. Next Trigger
            // releases the old generated note but leaves the sequencer clock/phase
            // running so the fallback note appears on the next sequenced trigger.
            if (config.midiInputMode == 2 || config.midiInputMode == 3)
                startSequencer (i, fallback, runtime.triggerVelocity, config);
        }
        else
        {
            stopSequencer (i, sampleOffset, output, p, routingMode, false);
            runtime.inputNote = -1;
            runtime.previousInputNote = -1;
        }
    }
    return consumed;
}


float SynthEngine::larpRandom()
{
    auto& seed = larpState.randomSeed;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return static_cast<float> (seed & 0x00ffffffu) / 16777216.0f;
}

void SynthEngine::resetLArpState (bool clearHeld)
{
    const auto previousMode = larpState.previousMode;
    const auto outputChannel = larpState.outputChannel;
    const auto inputChannelSetting = larpState.inputChannelSetting;
    const auto resetWasDown = larpState.resetSustainWasDown;
    const auto seed = larpState.randomSeed;
    std::array<bool, 128> held = larpState.held;
    std::array<bool, 128> keyDown = larpState.keyDown;
    std::array<int, 128> velocity = larpState.velocity;
    std::array<int, 128> age = larpState.age;
    const auto heldCount = larpState.heldCount;

    larpState = {};
    larpState.previousMode = previousMode;
    larpState.outputChannel = outputChannel;
    larpState.inputChannelSetting = inputChannelSetting;
    larpState.resetSustainWasDown = resetWasDown;
    larpState.randomSeed = seed != 0 ? seed : 0x6c617270u;
    larpState.currentNote = -1;
    larpState.arpStep = -1;
    larpState.direction = 1;
    larpState.velocity.fill (100);
    larpState.fractalVelocity = 1.0f;
    larpState.brownianLength = 1.0f;
    larpState.chordDirty = true;

    if (! clearHeld)
    {
        larpState.held = held;
        larpState.keyDown = keyDown;
        larpState.velocity = velocity;
        larpState.age = age;
        larpState.heldCount = heldCount;
        larpState.forceTrigger = heldCount > 0;
    }
}

void SynthEngine::emitLArp (const juce::MidiMessage& message, int sampleOffset,
                            juce::MidiBuffer& output, const Params& p)
{
    if (larpSendsArpeggiatedMidi (p.larp.state))
        output.addEvent (message, std::max (0, sampleOffset));

    if (message.isNoteOn())
        larpState.outputActive[static_cast<std::size_t> (message.getNoteNumber())] = true;
    else if (message.isNoteOff())
        larpState.outputActive[static_cast<std::size_t> (message.getNoteNumber())] = false;

    if (larpPlaysSynth (p.larp.state) && message.isNoteOnOrOff())
    {
        auto trackedPitchArp = false;
        for (int sourceIndex = 0; sourceIndex < 3; ++sourceIndex)
        {
            if (p.sourceNoteSource[static_cast<std::size_t> (sourceIndex)] != 2)
                continue;
            handleMidi (message, p, 1 << sourceIndex, ! trackedPitchArp);
            trackedPitchArp = true;
        }
    }
}

void SynthEngine::releaseLArpOutput (int sampleOffset, juce::MidiBuffer& output,
                                     const Params& p, bool releaseSynth)
{
    const auto channel = juce::jlimit (1, 16, larpState.outputChannel);
    const auto sendMidiCleanup = larpSendsArpeggiatedMidi (larpState.previousMode);
    for (int note = 0; note < 128; ++note)
    {
        if (! larpState.outputActive[static_cast<std::size_t> (note)])
            continue;
        const auto off = juce::MidiMessage::noteOff (channel, note);
        if (sendMidiCleanup)
            output.addEvent (off, std::max (0, sampleOffset));
        if (releaseSynth)
        {
            auto trackedPitchArp = false;
            for (int sourceIndex = 0; sourceIndex < 3; ++sourceIndex)
            {
                if (p.sourceNoteSource[static_cast<std::size_t> (sourceIndex)] != 2)
                    continue;
                handleMidi (off, p, 1 << sourceIndex, ! trackedPitchArp);
                trackedPitchArp = true;
            }
        }
        larpState.outputActive[static_cast<std::size_t> (note)] = false;
    }
    if (sendMidiCleanup)
    {
        output.addEvent (juce::MidiMessage::controllerEvent (channel, 64, 0),
                         std::max (0, sampleOffset));
        output.addEvent (juce::MidiMessage::allNotesOff (channel),
                         std::max (0, sampleOffset));
    }
    larpState.currentNote = -1;
    larpState.modulation = 0.0f;
    larpState.panStage = 0.0f;
}

void SynthEngine::handleLArpInput (const juce::MidiMessage& message, int sampleOffset,
                                   juce::MidiBuffer& output, const Params& p)
{
    const auto configuredChannel = juce::jlimit (0, 16, p.larp.midiChannel);
    const auto channelMatches = configuredChannel == 0
                             || message.isForChannel (configuredChannel);

    if ((message.isNoteOnOrOff()) && ! channelMatches)
        return;

    if (message.isNoteOn())
    {
        const auto note = juce::jlimit (0, 127, message.getNoteNumber());
        const auto wasEmpty = larpState.heldCount == 0;
        if (configuredChannel == 0 && wasEmpty)
            larpState.outputChannel = juce::jlimit (1, 16, message.getChannel());
        if (p.larp.chordHold && larpState.newChordPending)
        {
            larpState.held.fill (false);
            larpState.pendingRelease.fill (false);
            larpState.heldCount = 0;
            larpState.newChordPending = false;
        }
        if (! larpState.held[static_cast<std::size_t> (note)])
        {
            larpState.held[static_cast<std::size_t> (note)] = true;
            ++larpState.heldCount;
        }
        larpState.keyDown[static_cast<std::size_t> (note)] = true;
        larpState.pendingRelease[static_cast<std::size_t> (note)] = false;
        larpState.velocity[static_cast<std::size_t> (note)] =
            juce::jlimit (1, 127, static_cast<int> (message.getVelocity()));
        larpState.age[static_cast<std::size_t> (note)] = larpState.ageCounter++;
        larpState.chordDirty = true;
        larpState.forceTrigger = wasEmpty || larpState.forceTrigger;
        return;
    }

    if (message.isNoteOff())
    {
        const auto note = juce::jlimit (0, 127, message.getNoteNumber());
        larpState.keyDown[static_cast<std::size_t> (note)] = false;
        if (p.larp.chordHold)
        {
            const auto anyKeyDown = std::any_of (larpState.keyDown.begin(),
                                                 larpState.keyDown.end(),
                                                 [] (bool down) { return down; });
            if (! anyKeyDown && ! larpState.sustain)
                larpState.newChordPending = true;
            return;
        }
        if (larpState.sustain)
        {
            larpState.pendingRelease[static_cast<std::size_t> (note)] = true;
            return;
        }
        if (larpState.held[static_cast<std::size_t> (note)])
        {
            larpState.held[static_cast<std::size_t> (note)] = false;
            larpState.heldCount = std::max (0, larpState.heldCount - 1);
            larpState.chordDirty = true;
        }
        return;
    }

    if (message.isController() && message.getControllerNumber() == 64)
    {
        if (! channelMatches)
            return;
        const auto sustainNow = message.getControllerValue() >= 64;
        if (larpState.sustain && ! sustainNow)
        {
            for (int note = 0; note < 128; ++note)
            {
                if (! larpState.pendingRelease[static_cast<std::size_t> (note)])
                    continue;
                larpState.pendingRelease[static_cast<std::size_t> (note)] = false;
                if (larpState.held[static_cast<std::size_t> (note)])
                {
                    larpState.held[static_cast<std::size_t> (note)] = false;
                    larpState.heldCount = std::max (0, larpState.heldCount - 1);
                }
            }
            larpState.chordDirty = true;
            if (p.larp.chordHold)
                larpState.newChordPending = true;
        }
        larpState.sustain = sustainNow;
        // Sustain belongs to the arp memory; downstream instruments receive
        // an explicit pedal-up so generated note lengths remain authoritative.
        emitLArp (juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, larpState.outputChannel), 64, 0),
                  sampleOffset, output, p);
        return;
    }

    if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        releaseLArpOutput (sampleOffset, output, p, larpPlaysSynth (p.larp.state));
        resetLArpState (true);
        larpState.previousMode = p.larp.state;
        larpState.inputChannelSetting = juce::jlimit (0, 16, p.larp.midiChannel);
        if (larpState.inputChannelSetting > 0)
            larpState.outputChannel = larpState.inputChannelSetting;
        return;
    }

    // Controllers, pressure, pitch wheel and other non-note messages retain
    // their original timing. In Synth + LArp they also reach LJuno itself.
    emitLArp (message, sampleOffset, output, p);
}

void SynthEngine::rebuildLArpChord (const LArpParameters& p)
{
    larpState.previousChordCount = larpState.chordCount;
    larpState.chordCount = 0;
    const auto add = [this] (int sourceNote, int octave)
    {
        const auto note = sourceNote + octave * 12;
        if (note < 0 || note > 127)
            return;
        if (larpState.chordCount >= static_cast<int> (larpState.chord.size()))
            return;
        const auto index = static_cast<std::size_t> (larpState.chordCount++);
        larpState.chord[index] = note;
        larpState.chordVelocity[index] =
            larpState.velocity[static_cast<std::size_t> (sourceNote)];
    };

    if (p.octaveMode == 0)
    {
        for (int note = 0; note < 128; ++note)
            if (larpState.held[static_cast<std::size_t> (note)])
                for (int octave = p.octaveDown; octave <= p.octaveUp; ++octave)
                    add (note, octave);
    }
    else
    {
        for (int octave = p.octaveDown; octave <= p.octaveUp; ++octave)
            for (int note = 0; note < 128; ++note)
                if (larpState.held[static_cast<std::size_t> (note)])
                    add (note, octave);
    }

    if (larpState.chordCount != larpState.previousChordCount)
    {
        larpState.arpIndex = 0;
        if (larpState.previousChordCount == 0 && larpState.chordCount > 0)
        {
            larpState.arpStep = -1;
            larpState.sequenceTimer = 0.0;
            larpState.forceTrigger = true;
        }
    }
    larpState.chordDirty = false;
}

int SynthEngine::chooseLArpIndex (int mode, int count)
{
    if (count <= 1)
        return 0;
    const auto wrap = [count] (int value)
    {
        value %= count;
        return value < 0 ? value + count : value;
    };

    switch (mode)
    {
        case 0: return wrap (larpState.arpStep);                         // Up
        case 1: return count - 1 - wrap (larpState.arpStep);             // Down
        case 2:                                                         // UpDown
            larpState.arpIndex += larpState.direction;
            if (larpState.arpIndex >= count || larpState.arpIndex < 0)
            {
                larpState.direction = -larpState.direction;
                larpState.arpIndex += larpState.direction * 2;
            }
            return juce::jlimit (0, count - 1, larpState.arpIndex);
        case 3:                                                         // AccumulateReset
        {
            const auto position = wrap (larpState.arpStep);
            return ((larpState.arpStep / count) & 1) != 0
                ? count - 1 - position : position;
        }
        case 4:                                                         // InsideOut
        {
            const auto centre = (count - 1) / 2;
            const auto position = wrap (larpState.arpStep);
            return juce::jlimit (0, count - 1,
                (position & 1) == 0 ? centre - position / 2
                                    : centre + position / 2 + 1);
        }
        case 5:                                                         // LivePattern
        {
            const auto wanted = wrap (larpState.arpStep);
            std::array<std::pair<int, int>, 64> ordered {};
            for (int index = 0; index < count; ++index)
            {
                const auto note = larpState.chord[static_cast<std::size_t> (index)];
                ordered[static_cast<std::size_t> (index)] =
                    { larpState.age[static_cast<std::size_t> (note % 12 + (note / 12) * 12)], index };
            }
            std::sort (ordered.begin(), ordered.begin() + count);
            return ordered[static_cast<std::size_t> (wanted)].second;
        }
        case 6: return wrap (larpState.arpStep * 2);                     // SkipOne
        case 7:                                                         // ZigZag
        {
            const auto centre = (count - 1) / 2;
            const auto distance = (larpState.arpStep + 1) / 2;
            return juce::jlimit (0, count - 1,
                (larpState.arpStep & 1) == 0 ? centre + distance : centre - distance);
        }
        case 8:                                                         // Brownian
            larpState.arpIndex += larpRandom() < 0.5f ? -1 : 1;
            return larpState.arpIndex = juce::jlimit (0, count - 1, larpState.arpIndex);
        case 9:                                                         // BrownianDrift
        {
            const auto centre = (count - 1) * 0.5f;
            const auto towardCentre = larpState.arpIndex > centre ? -1 : 1;
            larpState.arpIndex += larpRandom() < 0.5f ? towardCentre
                                                      : (larpRandom() < 0.5f ? -1 : 1);
            return larpState.arpIndex = juce::jlimit (0, count - 1, larpState.arpIndex);
        }
        case 10: return juce::jlimit (0, count - 1,
                                      static_cast<int> (larpRandom() * count));
        case 11:                                                        // FractalBounce
            larpState.fractalPosition += larpRandom() < 0.4f ? 1.0f
                : (larpRandom() < 0.67f ? -1.0f : (larpRandom() < 0.5f ? 2.0f : -2.0f));
            while (larpState.fractalPosition < 0.0f) larpState.fractalPosition += count;
            while (larpState.fractalPosition >= count) larpState.fractalPosition -= count;
            return static_cast<int> (larpState.fractalPosition);
        case 12:                                                        // PendulumMemory
            if (larpRandom() < 0.25f)
                larpState.fractalVelocity += larpRandom() < 0.5f ? -1.0f : 1.0f;
            larpState.fractalVelocity = juce::jlimit (-2.0f, 2.0f,
                                                       larpState.fractalVelocity);
            if (std::abs (larpState.fractalVelocity) < 0.5f)
                larpState.fractalVelocity = 1.0f;
            larpState.fractalPosition += larpState.fractalVelocity;
            if (larpState.fractalPosition < 0.0f
                || larpState.fractalPosition >= static_cast<float> (count))
            {
                larpState.fractalPosition = juce::jlimit (
                    0.0f, static_cast<float> (count - 1), larpState.fractalPosition);
                larpState.fractalVelocity = -larpState.fractalVelocity;
            }
            return juce::jlimit (0, count - 1,
                                 static_cast<int> (larpState.fractalPosition));
        case 13:                                                        // Orbit
            larpState.fractalPosition += 1.0f
                + (larpRandom() < 0.35f ? (larpRandom() < 0.5f ? -1.0f : 1.0f) : 0.0f)
                + (larpRandom() < 0.15f ? (larpRandom() < 0.5f ? -2.0f : 2.0f) : 0.0f);
            while (larpState.fractalPosition < 0.0f) larpState.fractalPosition += count;
            while (larpState.fractalPosition >= count) larpState.fractalPosition -= count;
            return static_cast<int> (larpState.fractalPosition);
        case 14:                                                        // DrunkMirror
            larpState.fractalPosition += 1.0f
                + (larpRandom() < 0.35f ? (larpRandom() < 0.5f ? -1.0f : 1.0f) : 0.0f);
            while (larpState.fractalPosition < 0.0f) larpState.fractalPosition += count;
            while (larpState.fractalPosition >= count) larpState.fractalPosition -= count;
            return static_cast<int> (larpState.fractalPosition);
        default: return wrap (larpState.arpStep);
    }
}

bool SynthEngine::isLArpLegatoStep (int pattern, int step)
{
    switch (pattern)
    {
        case 1:  return true;
        case 2:  return step % 2 == 0;
        case 3:  return step % 3 == 0;
        case 4:  return step % 3 == 1;
        case 5:  return step % 3 == 2;
        case 6:  return step % 4 == 0;
        case 7:  return step % 4 == 1;
        case 8:  return step % 4 == 2;
        case 9:  return step % 4 == 3;
        case 10: return larpRandom() < 0.4f;
        case 11: return step % 2 == 1;
        case 12: return step % 4 == 0;
        case 13: return step % 4 < 2;
        case 14: return larpRandom() < 0.2f;
        default: return false;
    }
}

void SynthEngine::advanceLArp (int sampleOffset, juce::MidiBuffer& output,
                               const Params& params)
{
    const auto& p = params.larp;
    if (larpState.chordDirty)
        rebuildLArpChord (p);

    if (larpState.chordCount <= 0)
    {
        if (larpState.currentNote >= 0)
            releaseLArpOutput (sampleOffset, output, params, larpPlaysSynth (p.state));
        larpState.sequenceTimer = larpState.noteTimer = 0.0;
        larpState.arpStep = -1;
        larpState.repeatCounter = 0;
        larpState.modulation = larpState.panStage = 0.0f;
        return;
    }

    const auto samplesPerBeat = sampleRate * 60.0 / params.tempoBpm;
    larpState.ratePatternPhase += juce::MathConstants<double>::twoPi
                                * std::max (0.001f, p.ratePatternSpeed)
                                / samplesPerBeat;
    if (larpState.ratePatternPhase >= juce::MathConstants<double>::twoPi)
    {
        larpState.ratePatternPhase -= juce::MathConstants<double>::twoPi;
        larpState.ratePatternRandom = 0.85f + 0.30f * larpRandom();
    }

    auto rateMultiplier = 1.0;
    switch (p.ratePattern)
    {
        case 1:
        {
            const auto sine = std::sin (larpState.ratePatternPhase);
            rateMultiplier = 1.0 + 0.5 * sine;
            break;
        }
        case 2:
            rateMultiplier = larpState.ratePatternRandom;
            break;
        case 3:
        {
            const auto sine = std::sin (larpState.ratePatternPhase);
            rateMultiplier = sine > 0.0 ? 1.25 : 0.75;
            break;
        }
        case 4:
            rateMultiplier = 0.6 + 0.6 * std::sin (larpState.ratePatternPhase * 0.15);
            break;
        case 5:
        {
            const auto phase01 = larpState.ratePatternPhase / juce::MathConstants<double>::twoPi;
            rateMultiplier = 1.3 - phase01 * 0.6;
            break;
        }
        case 6:
        {
            const auto phase01 = larpState.ratePatternPhase / juce::MathConstants<double>::twoPi;
            rateMultiplier = 0.7 + phase01 * 0.6;
            break;
        }
        case 7:
        {
            const auto sine = std::sin (larpState.ratePatternPhase);
            rateMultiplier = sine > 0.0 ? 1.15 : 0.85;
            break;
        }
        case 8:
        {
            const auto phase01 = larpState.ratePatternPhase / juce::MathConstants<double>::twoPi;
            rateMultiplier = phase01 < 0.5 ? 0.7 + phase01 * 1.2
                                           : 1.9 - phase01 * 1.2;
            break;
        }
        default:
            break;
    }
    auto stepSamples = samplesPerBeat
        / std::max (0.01, static_cast<double> (p.division) * rateMultiplier);
    if (p.adaptiveDivision && larpState.chordCount > 1)
        stepSamples /= larpState.chordCount;
    stepSamples = std::max (samplesPerBeat / std::max (0.01f, p.division) / 64.0,
                            stepSamples);

    const auto shuffleHit = p.shuffle >= 0.0f ? (larpState.arpStep & 1) != 0
                                              : (larpState.arpStep & 1) == 0;
    if (p.shuffle != 0.0f)
    {
        const auto shuffleAmount = std::pow (std::abs (p.shuffle) * 0.5, 0.85);
        const auto longShuffleStep = p.freeShuffle ? (larpState.shufflePhase & 1) != 0
                                                   : shuffleHit;
        stepSamples *= longShuffleStep ? 1.0 + shuffleAmount : 1.0 - shuffleAmount;
    }

    larpState.sequenceTimer += 1.0;
    larpState.noteTimer += 1.0;
    const auto triggerThreshold = std::max (1.0, stepSamples + larpState.microShiftSamples);
    if (! larpState.forceTrigger && larpState.sequenceTimer < triggerThreshold)
    {
        if (larpState.currentNote >= 0 && ! larpState.currentLegato
            && ! larpState.nextLegato && larpState.noteTimer >= larpState.activeDuration)
        {
            emitLArp (juce::MidiMessage::noteOff (juce::jlimit (1, 16, larpState.outputChannel), larpState.currentNote),
                      sampleOffset, output, params);
            larpState.currentNote = -1;
        }
        return;
    }

    larpState.forceTrigger = false;
    larpState.sequenceTimer = std::max (0.0, larpState.sequenceTimer - stepSamples);
    if (p.freeShuffle)
        ++larpState.shufflePhase;

    if (larpState.repeatCounter <= 0)
    {
        ++larpState.arpStep;
        auto effectivePattern = p.pattern;
        if (p.modeSwitch > 0)
        {
            const auto phrase = std::max (8, larpState.chordCount * 2);
            if (p.modeSwitch == 3 && larpState.arpStep % phrase >= phrase / 2)
                effectivePattern = (p.pattern + 2) % 15;
            else if (p.modeSwitch == 5 && larpState.arpStep % phrase >= phrase / 2)
                effectivePattern = p.pattern == 12 ? 11 : (p.pattern == 11 ? 12 : 13);
            else if ((p.modeSwitch == 1 || p.modeSwitch == 2 || p.modeSwitch == 6)
                     && larpState.arpStep % phrase == 0)
                effectivePattern = 11 + static_cast<int> (larpRandom() * 4.0f);
        }
        larpState.arpIndex = chooseLArpIndex (effectivePattern, larpState.chordCount);
        const auto extraRepeats = p.repeatRandomDepth > 0.0f
            ? static_cast<int> (std::pow (larpRandom(), 1.0f - p.repeatRandomDepth)
                                * 32.0f * p.repeatRandomDepth) : 0;
        larpState.repeatCounter = juce::jlimit (1, 64,
                                                std::max (1, p.repeat) + extraRepeats);
    }

    const auto index = juce::jlimit (0, larpState.chordCount - 1, larpState.arpIndex);
    const auto normalizedPosition = larpState.chordCount > 1
        ? static_cast<float> (index) / static_cast<float> (larpState.chordCount - 1)
        : 0.5f;
    auto microEnergy = 0.0f;
    larpState.microShiftSamples = 0.0;
    if (p.microShiftMode > 0 && p.microShiftDepth > 0.0f && larpState.chordCount > 1)
    {
        const auto maximum = stepSamples * 0.4 * p.microShiftDepth;
        switch (p.microShiftMode)
        {
            case 1: larpState.microShiftSamples = (normalizedPosition - 0.5f) * 2.0 * maximum; break;
            case 2: larpState.microShiftSamples = (larpState.arpStep & 1) ? maximum : -maximum; break;
            case 3: larpState.microShiftSamples = (normalizedPosition - 0.5f) * 2.0 * maximum; break;
            case 4: larpState.microShiftSamples = (larpRandom() * 2.0f - 1.0f) * maximum; break;
            case 8: larpState.microShiftSamples = (1.0f - normalizedPosition) * maximum; break;
            case 9: larpState.microShiftSamples = normalizedPosition * maximum; break;
            case 10: larpState.microShiftSamples = ((larpState.arpStep & 1)
                ? 1.0f - normalizedPosition : normalizedPosition) * maximum; break;
            case 11: larpState.microShiftSamples = std::sin (
                larpState.arpStep * juce::MathConstants<double>::twoPi / larpState.chordCount) * maximum; break;
            case 12: larpState.microShiftSamples = larpState.arpStep % 4 == 0 ? maximum : 0.0; break;
            default: larpState.microShiftSamples = (larpRandom() * 2.0f - 1.0f) * maximum; break;
        }
        microEnergy = maximum > 0.0 ? static_cast<float> (
            std::min (1.0, std::abs (larpState.microShiftSamples) / maximum)) : 0.0f;
    }

    --larpState.repeatCounter;
    if (p.skipProbability > 0.0f && larpRandom() < p.skipProbability)
        return;

    auto note = larpState.chord[static_cast<std::size_t> (index)];
    if (p.pitchRandomDepth > 0.0f)
        note += static_cast<int> ((larpRandom() * 2.0f - 1.0f)
                                 * 12.0f * p.pitchRandomDepth);
    note = juce::jlimit (0, 127, note);
    auto velocity = static_cast<float> (
        larpState.chordVelocity[static_cast<std::size_t> (index)]);
    if (p.velocityRandomDepth > 0.0f)
    {
        const auto u1 = std::max (0.000001f, larpRandom());
        const auto gaussian = std::sqrt (-2.0f * std::log (u1))
                            * std::cos (juce::MathConstants<float>::twoPi * larpRandom());
        velocity += gaussian * 20.0f * p.velocityRandomDepth;
    }
    if (microEnergy > 0.0f)
        velocity *= 1.0f - microEnergy * p.microShiftVelocityDepth;
    if (shuffleHit)
        velocity *= 1.0f - p.shuffleVelocity;
    const auto midiVelocity = static_cast<juce::uint8> (
        juce::jlimit (5, 127, juce::roundToInt (velocity)));

    larpState.currentLegato = isLArpLegatoStep (p.legatoPattern, larpState.arpStep);
    larpState.nextLegato = isLArpLegatoStep (p.legatoPattern, larpState.arpStep + 1);
    auto lengthMultiplier = 1.0f;
    if (p.lengthRandomMode == 1) lengthMultiplier = larpRandom();
    else if (p.lengthRandomMode == 2)
    {
        const auto u1 = std::max (0.000001f, larpRandom());
        lengthMultiplier = 1.0f + 0.35f * std::sqrt (-2.0f * std::log (u1))
                                      * std::cos (juce::MathConstants<float>::twoPi * larpRandom());
    }
    else if (p.lengthRandomMode == 3)
    {
        larpState.brownianLength = juce::jlimit (0.2f, 1.8f,
            larpState.brownianLength + (larpRandom() * 2.0f - 1.0f) * 0.25f);
        lengthMultiplier = larpState.brownianLength;
    }
    else if (p.lengthRandomMode == 4)
        lengthMultiplier = larpState.arpStep % 4 == 0 ? 1.5f : 0.6f;
    auto gate = std::max (0.02f, p.length) * lengthMultiplier;
    if (p.microShiftMode > 0)
        gate *= 1.0f + p.microShiftLengthDepth * 2.0f;
    larpState.activeDuration = larpState.currentLegato
        ? stepSamples * 1.05 : stepSamples * gate;
    if (shuffleHit && ! larpState.currentLegato)
        larpState.activeDuration = std::max (stepSamples * 0.05,
            larpState.activeDuration * (1.0 - p.shuffleLength));

    larpState.modulation = larpState.chordCount > 1
        ? normalizedPosition * 2.0f - 1.0f : 0.0f;
    if ((larpState.chordCount & 1) != 0)
    {
        if (index == 0) larpState.panStage = 0.0f;
        else
        {
            const auto pair = (index + 1) / 2;
            const auto magnitude = static_cast<float> (pair)
                                 / std::max (1, larpState.chordCount / 2);
            larpState.panStage = (index & 1) != 0 ? -magnitude : magnitude;
        }
    }
    else
    {
        const auto magnitude = static_cast<float> (2 * (index / 2) + 1)
                             / std::max (1, larpState.chordCount - 1);
        larpState.panStage = (index & 1) == 0 ? -magnitude : magnitude;
    }

    if (larpState.currentNote >= 0 && larpState.currentNote != note)
        emitLArp (juce::MidiMessage::noteOff (juce::jlimit (1, 16, larpState.outputChannel), larpState.currentNote),
                  sampleOffset, output, params);
    if (larpState.currentNote != note || ! larpState.currentLegato)
    {
        if (larpState.currentNote == note)
            emitLArp (juce::MidiMessage::noteOff (juce::jlimit (1, 16, larpState.outputChannel), note),
                      sampleOffset, output, params);
        emitLArp (juce::MidiMessage::noteOn (juce::jlimit (1, 16, larpState.outputChannel), note, midiVelocity),
                  sampleOffset, output, params);
        larpState.currentNote = note;
        larpState.noteTimer = 0.0;
    }
}

void SynthEngine::randomizeUnisonPhases (Voice& voice)
{
    // JSFX reset_voice_unison(): every true mono retrigger gives each clone
    // an independent analogue-style phase offset. The central voice itself
    // still starts at phase zero, exactly like the reference.
    for (std::size_t clone = 0; clone < voice.unisonPhase1.size(); ++clone)
    {
        voice.unisonPhase1[clone] = wrapPhase (0.5 * (randomSigned() + 1.0f));
        voice.unisonPhase2[clone] = wrapPhase (0.5 * (randomSigned() + 1.0f));
    }
}

void SynthEngine::seedVoiceMicroMotion (Voice& voice)
{
    const auto stationaryScale = microMotionStationaryScale (
        cachedRenderConstants.microMotionAlpha);
    voice.microMotionCommon = randomSigned() * stationaryScale;
    voice.microMotionIndependent1 = randomSigned() * stationaryScale;
    voice.microMotionIndependent2 = randomSigned() * stationaryScale;
    voice.microMotionOut1 = voice.microMotionCommon * 0.60f
                          + voice.microMotionIndependent1 * 0.40f;
    voice.microMotionOut2 = voice.microMotionCommon * 0.60f
                          + voice.microMotionIndependent2 * 0.40f;
    voice.microMotionPitchMultiplier1 = 1.0f;
    voice.microMotionPitchMultiplier2 = 1.0f;
}

void SynthEngine::advanceVoiceMicroMotion (Voice& voice, const Params& p,
                                            const RenderConstants& d)
{
    if (! d.microMotionAny)
        return;

    const auto memory = 1.0f - d.microMotionAlpha;
    const auto advance = [this, memory, &d] (float current)
    {
        return juce::jlimit (-1.0f, 1.0f,
                             memory * current + d.microMotionAlpha * randomSigned());
    };
    voice.microMotionCommon = advance (voice.microMotionCommon);
    voice.microMotionIndependent1 = advance (voice.microMotionIndependent1);
    voice.microMotionIndependent2 = advance (voice.microMotionIndependent2);
    voice.microMotionOut1 = voice.microMotionCommon * 0.60f
                          + voice.microMotionIndependent1 * 0.40f;
    voice.microMotionOut2 = voice.microMotionCommon * 0.60f
                          + voice.microMotionIndependent2 * 0.40f;
    voice.microMotionPitchMultiplier1 = p.microMotionPitch1 > epsilon
        ? std::max (0.5f, 1.0f + voice.microMotionOut1 * p.microMotionPitch1 * 2.0f)
        : 1.0f;
    voice.microMotionPitchMultiplier2 = p.microMotionPitch2 > epsilon
        ? std::max (0.5f, 1.0f + voice.microMotionOut2 * p.microMotionPitch2 * 2.0f)
        : 1.0f;
}

float SynthEngine::portamentoForMask (const Params& p, int layerMask) noexcept
{
    // Source-routed voices use their own glide amount. Historical combined
    // voices are only used when all three portamento values are equal, so L1
    // remains the correct shared value for that compatibility path.
    if ((layerMask & 2) != 0 && (layerMask & 1) == 0)
        return p.portamento[1];
    if ((layerMask & 4) != 0 && (layerMask & 3) == 0)
        return p.portamento[2];
    return p.portamento[0];
}

bool SynthEngine::sustainForMask (int layerMask) const noexcept
{
    return (((layerMask & 1) != 0) && sustainPedal[0])
        || (((layerMask & 2) != 0) && sustainPedal[1])
        || (((layerMask & 4) != 0) && sustainPedal[2]);
}

bool SynthEngine::anySustainPedal() const noexcept
{
    return sustainPedal[0] || sustainPedal[1] || sustainPedal[2];
}

bool SynthEngine::sourceUsesMono (const Params& p, int sourceIndex) noexcept
{
    sourceIndex = juce::jlimit (0, 1, sourceIndex);
    const auto mode = p.sourceVoiceMode[static_cast<std::size_t> (sourceIndex)];
    if (mode == 2)
        return true;
    if (mode == 1)
        return false;
    return p.voiceCount == 1;
}

void SynthEngine::handleSourceRoutedMidi (const juce::MidiMessage& message, const Params& p)
{
    const auto channel = juce::jlimit (1, 16, message.getChannel());
    const auto matches = [channel] (int wanted)
    {
        return wanted == 0 || wanted == channel;
    };

    if (message.isController() && message.getControllerNumber() == 1)
    {
        const auto amount = message.getControllerValue() / 127.0f;
        for (int sourceIndex = 0; sourceIndex < 3; ++sourceIndex)
            if (matches (p.sourceMidiChannel[static_cast<std::size_t> (sourceIndex)]))
                modWheel[static_cast<std::size_t> (sourceIndex)] = amount;
        return;
    }

    if (message.isChannelPressure() || message.isAftertouch())
    {
        const auto amount = message.isChannelPressure()
            ? message.getChannelPressureValue() / 127.0f
            : message.getAfterTouchValue() / 127.0f;
        for (int sourceIndex = 0; sourceIndex < 3; ++sourceIndex)
            if (matches (p.sourceMidiChannel[static_cast<std::size_t> (sourceIndex)]))
                channelAftertouch[static_cast<std::size_t> (sourceIndex)] = amount;
        return;
    }

    if (message.isPitchWheel())
    {
        const auto bend = juce::jlimit (-1.0f, 1.0f,
            (message.getPitchWheelValue() - 8192) / 8192.0f);
        for (int sourceIndex = 0; sourceIndex < 3; ++sourceIndex)
            if (matches (p.sourceMidiChannel[static_cast<std::size_t> (sourceIndex)]))
                pitchBend[static_cast<std::size_t> (sourceIndex)] = bend;
        return;
    }

    if (message.isController() && message.getControllerNumber() == 64)
    {
        const auto sustainNow = message.getControllerValue() >= 64;
        const auto hadAnySustain = anySustainPedal();
        auto affectedMask = 0;
        for (int sourceIndex = 0; sourceIndex < 3; ++sourceIndex)
        {
            if (! matches (p.sourceMidiChannel[static_cast<std::size_t> (sourceIndex)]))
                continue;
            sustainPedal[static_cast<std::size_t> (sourceIndex)] = sustainNow;
            affectedMask |= 1 << sourceIndex;
        }

        if (affectedMask == 0)
            return;

        if (! sustainNow)
        {
            for (auto& v : voices)
            {
                if (! v.active || v.held || (v.layerMask & affectedMask) == 0
                    || sustainForMask (v.layerMask))
                    continue;
                v.stage = Stage::release;
                if (v.stage2 != Stage::idle)
                    v.stage2 = Stage::release;
            }

            // Pitch Arp is still a global musical processor. Release its sustained
            // notes only when no source pedal remains down.
            if (hadAnySustain && ! anySustainPedal())
                trackPitchArpMidi (message, p);
        }
        return;
    }

    if (! message.isNoteOnOrOff())
    {
        handleMidi (message, p);
        return;
    }

    const auto independentPortamento = std::abs (p.portamento[0] - p.portamento[1]) > epsilon
                                   || std::abs (p.portamento[0] - p.portamento[2]) > epsilon
                                   || std::abs (p.portamento[1] - p.portamento[2]) > epsilon;
    const auto midiRoutingActive = p.sourceMidiChannel[0] != 0
                                || p.sourceMidiChannel[1] != 0
                                || p.sourceMidiChannel[2] != 0;
    const auto independentVoiceMode = p.sourceVoiceMode[0] != 0 || p.sourceVoiceMode[1] != 0;
    const auto independentNoteSource = p.sourceNoteSource[0] != 0
                                    || p.sourceNoteSource[1] != 0
                                    || p.sourceNoteSource[2] != 0;

    if (! midiRoutingActive && ! independentPortamento && ! independentVoiceMode
        && ! independentNoteSource)
    {
        handleMidi (message, p, 7);
        return;
    }

    auto trackedPitchArp = false;
    const auto route = [&] (int sourceIndex, int mask)
    {
        if (p.sourceNoteSource[static_cast<std::size_t> (sourceIndex)] != 0
            || ! matches (p.sourceMidiChannel[static_cast<std::size_t> (sourceIndex)]))
            return;
        handleMidi (message, p, mask, ! trackedPitchArp);
        trackedPitchArp = true;
    };
    route (0, 1);
    route (1, 2);
    route (2, 4);
}

void SynthEngine::handleMidi (const juce::MidiMessage& message, const Params& p,
                              int layerMask, bool trackPitchArp)
{
    layerMask = juce::jlimit (1, 7, layerMask);
    if (trackPitchArp)
        trackPitchArpMidi (message, p);

    // Explicit or inherited per-source mono modes use independent note stacks
    // and independent voice banks. The historical combined mono path remains
    // untouched when both layers are still following the global behaviour.
    if ((layerMask == 1 || layerMask == 2))
    {
        const auto sourceIndex = layerMask == 1 ? 0 : 1;
        if (sourceUsesMono (p, sourceIndex))
        {
            if (message.isNoteOn())
            {
                handleSourceMonoNoteOn (sourceIndex, message.getNoteNumber(),
                                        message.getFloatVelocity(), p);
                return;
            }
            if (message.isNoteOff())
            {
                handleSourceMonoNoteOff (sourceIndex, message.getNoteNumber(), p);
                return;
            }
        }
    }

    // The historical monophonic keyboard path remains unchanged. Layer-specific
    // sequencer notes use the normal voice allocator so the three lanes can overlap.
    if (message.isNoteOn() && p.voiceCount == 1 && layerMask == 7)
    {
        handleMonoNoteOn (message.getNoteNumber(), message.getFloatVelocity(), p);
        return;
    }
    if (message.isNoteOff() && p.voiceCount == 1 && layerMask == 7)
    {
        handleMonoNoteOff (message.getNoteNumber(), p);
        return;
    }

    if (message.isNoteOn())
    {
        monoNoteCount = 0;

        // Normal MIDI and Sequencer 1 use the historical first bank. Sequencer 2
        // gets an equally large independent bank, so a four-note chord means four
        // Layer-1 voices PLUS four Layer-2 voices, just as four normal synth voices
        // would contain both layers. Noise intentionally has one dedicated slot.
        const auto bankStart = layerMask == 2 ? layer2VoiceBankStart
                             : layerMask == 4 ? noiseSequencerVoiceIndex
                                              : 0;
        const auto bankCount = layerMask == 4 ? 1 : p.voiceCount;
        const auto activeBegin = voices.begin() + bankStart;
        const auto activeEnd = activeBegin + bankCount;
        const auto anyHeld = std::any_of (activeBegin, activeEnd,
                                         [] (const Voice& voice) { return voice.active && voice.held; });

        if (p.polyMode == 1)
        {
            // Poly-2 has fixed cyclic registers. A phrase always begins at
            // register one, while active release tails keep the pitch, phase,
            // drift and filter history that make its portamento orderly.
            auto& registerIndex = layerMask == 7
                ? rolandVoice
                : sequencerRolandVoice[static_cast<std::size_t> (
                    layerMask == 1 ? 0 : layerMask == 2 ? 1 : 2)];
            if (! anyHeld)
            {
                registerIndex = 0;
                retriggerLfos (p);
            }

            auto& v = voices[static_cast<std::size_t> (bankStart
                                                       + (registerIndex % bankCount))];
            registerIndex = (registerIndex + 1) % bankCount;
            const auto wasActive = v.active;
            const auto previousFrequency = v.frequency;
            const auto previousDrift = v.drift;
            const auto previousPing = v.ping;

            if (! wasActive)
            {
                v = {};
                v.frequency = previousFrequency;
                v.drift = randomSigned();
                if (cachedRenderConstants.microMotionAny)
                    seedVoiceMicroMotion (v);
                v.pwm1 = v.pwm2 = juce::jlimit (0.05f, 0.95f, p.pwm);
            }
            else
            {
                v.drift = previousDrift;
                v.releasePitch = v.releasePitch2 = 0.0f;
            }

            v.active = v.held = true;
            v.pending = false;
            v.pendingNote = -1;
            v.pendingLayerMask = 7;
            v.pendingVelocity = 0.0f;
            v.layerMask = layerMask;
            v.note = message.getNoteNumber();
            setVoicePerformanceTargets (v, v.note, message.getFloatVelocity(), p,
                                        ! wasActive);
            v.stage = Stage::attack;
            v.pitchEnvelope = 0.0f;
            if (usesAdsr2 (p))
            {
                if (! wasActive || v.stage2 == Stage::idle)
                    v.envelope2 = 0.0f;
                v.stage2 = Stage::attack;
            }
            else
            {
                v.stage2 = Stage::idle;
                v.envelope2 = 0.0f;
            }
            v.age = ++ageCounter;
            v.ping = -previousPing;
            v.targetFrequency = 440.0 * std::exp2 ((v.note - 69) / 12.0);
            if (portamentoForMask (p, layerMask) <= epsilon)
                v.frequency = v.targetFrequency;
            return;
        }

        auto* allocated = allocate (p, layerMask);
        if (allocated == nullptr)
            return;
        auto& v = *allocated;
        if (v.active)
        {
            // Modern mode avoids a hard discontinuity when every voice is in
            // use. It queues the new note behind the selected register and
            // fades the old envelope with the JSFX two-millisecond steal.
            v.pending = true;
            v.pendingNote = message.getNoteNumber();
            v.pendingLayerMask = layerMask;
            v.pendingVelocity = message.getFloatVelocity();
            // The reference begins the filter key-follow transition as soon
            // as the replacement is queued, while the old note fades out.
            setVoicePerformanceTargets (v, v.pendingNote, v.velocity, p, false);
            v.held = false;
            v.stage = Stage::steal;
            v.releasePitch = v.releasePitch2 = 0.0f;
            return;
        }

        retriggerLfos (p);
        const auto previousFrequency = v.frequency;
        const auto previousPing = v.ping;
        v = {};
        v.active = v.held = true;
        v.layerMask = layerMask;
        v.note = message.getNoteNumber();
        setVoicePerformanceTargets (v, v.note, message.getFloatVelocity(), p, true);
        v.stage = Stage::attack;
        v.stage2 = usesAdsr2 (p) ? Stage::attack : Stage::idle;
        v.age = ++ageCounter;
        v.drift = randomSigned();
        if (cachedRenderConstants.microMotionAny)
            seedVoiceMicroMotion (v);
        v.pwm1 = v.pwm2 = juce::jlimit (0.05f, 0.95f, p.pwm);
        v.ping = -previousPing;
        v.targetFrequency = 440.0 * std::exp2 ((v.note - 69) / 12.0);
        v.frequency = portamentoForMask (p, layerMask) > 0.0f
            ? previousFrequency : v.targetFrequency;
    }
    else if (message.isNoteOff())
    {
        for (auto& v : voices)
            if (v.pending && v.pendingNote == message.getNoteNumber()
                && v.pendingLayerMask == layerMask)
            {
                v.pending = false;
                v.pendingNote = -1;
                v.pendingLayerMask = 7;
                v.pendingVelocity = 0.0f;
            }

        Voice* released = nullptr;
        for (auto& v : voices)
        {
            if (! v.active || v.note != message.getNoteNumber() || ! v.held
                || v.layerMask != layerMask)
                continue;
            if (released == nullptr || v.age > released->age)
                released = &v;
        }
        if (released != nullptr)
        {
            released->held = false;
            if (! sustainForMask (released->layerMask))
            {
                released->stage = Stage::release;
                if (released->stage2 != Stage::idle)
                    released->stage2 = Stage::release;
            }
        }
    }
    else if (message.isController() && message.getControllerNumber() == 64)
    {
        const auto sustainNow = message.getControllerValue() >= 64;
        sustainPedal = { sustainNow, sustainNow, sustainNow };
        if (! sustainNow)
        {
            for (auto& v : voices)
            {
                if (! v.active || v.held)
                    continue;
                v.stage = Stage::release;
                if (v.stage2 != Stage::idle)
                    v.stage2 = Stage::release;
            }
        }
    }
    else if (message.isController() && message.getControllerNumber() == 1)
    {
        const auto amount = message.getControllerValue() / 127.0f;
        modWheel = { amount, amount, amount };
    }
    else if (message.isChannelPressure() || message.isAftertouch())
    {
        const auto amount = message.isChannelPressure()
            ? message.getChannelPressureValue() / 127.0f
            : message.getAfterTouchValue() / 127.0f;
        channelAftertouch = { amount, amount, amount };
    }
    else if (message.isPitchWheel())
    {
        const auto bend = juce::jlimit (-1.0f, 1.0f,
            (message.getPitchWheelValue() - 8192) / 8192.0f);
        pitchBend = { bend, bend, bend };
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        monoNoteCount = 0;
        sourceMonoNoteCount = { 0, 0 };
        for (auto& v : voices)
        {
            v.held = false;
            v.stage = Stage::release;
            if (v.stage2 != Stage::idle)
                v.stage2 = Stage::release;
        }
    }
}

void SynthEngine::removePitchArpNote (int note)
{
    for (int index = 0; index < pitchArpHeldCount; ++index)
    {
        if (pitchArpHeldNotes[static_cast<std::size_t> (index)] != note)
            continue;
        for (int move = index; move + 1 < pitchArpHeldCount; ++move)
            pitchArpHeldNotes[static_cast<std::size_t> (move)]
                = pitchArpHeldNotes[static_cast<std::size_t> (move + 1)];
        --pitchArpHeldCount;
        return;
    }
}

void SynthEngine::refreshPitchArpLiveSets()
{
    pitchArpLiveCount = pitchArpHeldCount;
    for (int index = 0; index < pitchArpLiveCount; ++index)
    {
        const auto note = pitchArpHeldNotes[static_cast<std::size_t> (index)];
        pitchArpLiveFree[static_cast<std::size_t> (index)] = note;
        pitchArpLiveSorted[static_cast<std::size_t> (index)] = note;
    }
    // Biscot1 keeps the first played note as the phrase root. Sorting changes
    // traversal order, never the tonal centre of an already running phrase.
    pitchArpLiveSortedRoot = pitchArpLiveCount > 0 ? pitchArpLiveFree[0] : 60;
    pitchArpLiveFreeRoot = pitchArpLiveSortedRoot;
    std::sort (pitchArpLiveSorted.begin(),
               pitchArpLiveSorted.begin() + pitchArpLiveCount);
    pitchArpPoolDirty = true;
}

void SynthEngine::recordPitchArpSidNote (PitchArpState& state, int note,
                                         bool firstNote)
{
    if (firstNote || state.sidSequenceLength <= 0)
    {
        state.sidSequence[0] = note;
        state.sidSequenceLength = 1;
        return;
    }

    if (state.sidSequenceLength < static_cast<int> (state.sidSequence.size()))
    {
        const auto position = juce::jlimit (
            0, state.sidSequenceLength,
            state.stepIndex < 0 ? 0 : state.stepIndex + 1);
        for (int index = state.sidSequenceLength; index > position; --index)
            state.sidSequence[static_cast<std::size_t> (index)]
                = state.sidSequence[static_cast<std::size_t> (index - 1)];
        state.sidSequence[static_cast<std::size_t> (position)] = note;
        ++state.sidSequenceLength;
        return;
    }

    const auto position = state.stepIndex < 0 ? 0
        : (state.stepIndex + 1) % static_cast<int> (state.sidSequence.size());
    state.sidSequence[static_cast<std::size_t> (position)] = note;
}

void SynthEngine::trackPitchArpMidi (const juce::MidiMessage& message, const Params&)
{
    const auto copyLiveToLatch = [this]
    {
        pitchArpLatchCount = pitchArpLiveCount;
        std::copy_n (pitchArpLiveSorted.begin(), pitchArpLatchCount,
                     pitchArpLatchSorted.begin());
        std::copy_n (pitchArpLiveFree.begin(), pitchArpLatchCount,
                     pitchArpLatchFree.begin());
        pitchArpLatchSortedRoot = pitchArpLiveSortedRoot;
        pitchArpLatchFreeRoot = pitchArpLiveFreeRoot;
        pitchArpLatchValid = pitchArpLatchCount > 0;
    };

    if (message.isNoteOn())
    {
        const auto note = juce::jlimit (0, 127, message.getNoteNumber());
        const auto firstNote = pitchArpHeldCount == 0;
        pitchArpKeyDown[static_cast<std::size_t> (note)] = true;
        pitchArpSustained[static_cast<std::size_t> (note)] = false;
        removePitchArpNote (note);
        if (pitchArpHeldCount == static_cast<int> (pitchArpHeldNotes.size()))
        {
            std::move (pitchArpHeldNotes.begin() + 1, pitchArpHeldNotes.end(),
                       pitchArpHeldNotes.begin());
            --pitchArpHeldCount;
        }
        pitchArpHeldNotes[static_cast<std::size_t> (pitchArpHeldCount++)] = note;
        refreshPitchArpLiveSets();
        // Snapshot on every note-on. Note-off never rewrites it, otherwise the
        // release sequence would collapse as keys are lifted one by one.
        copyLiveToLatch();
        recordPitchArpSidNote (pitchArpState1, note, firstNote);
        recordPitchArpSidNote (pitchArpState2, note, firstNote);
        if (firstNote)
        {
            pitchArpState1.stepClock = pitchArpState2.stepClock = 0.0;
            pitchArpState1.stepIndex = pitchArpState2.stepIndex = -1;
            pitchArpState1.pitchTarget = pitchArpState1.pitchCurrent = 0.0f;
            pitchArpState2.pitchTarget = pitchArpState2.pitchCurrent = 0.0f;
        }
        return;
    }

    if (message.isNoteOff())
    {
        const auto note = juce::jlimit (0, 127, message.getNoteNumber());
        pitchArpKeyDown[static_cast<std::size_t> (note)] = false;
        if (anySustainPedal())
        {
            pitchArpSustained[static_cast<std::size_t> (note)] = true;
            return;
        }
        removePitchArpNote (note);
        pitchArpSustained[static_cast<std::size_t> (note)] = false;
        return;
    }

    if (message.isController() && message.getControllerNumber() == 64
        && message.getControllerValue() < 64)
    {
        auto releasedCount = 0;
        for (int index = 0; index < pitchArpHeldCount; ++index)
        {
            const auto note = pitchArpHeldNotes[static_cast<std::size_t> (index)];
            if (! pitchArpKeyDown[static_cast<std::size_t> (note)])
                ++releasedCount;
        }
        for (int index = pitchArpHeldCount - 1; index >= 0; --index)
        {
            const auto note = pitchArpHeldNotes[static_cast<std::size_t> (index)];
            if (! pitchArpKeyDown[static_cast<std::size_t> (note)])
            {
                removePitchArpNote (note);
                pitchArpSustained[static_cast<std::size_t> (note)] = false;
            }
        }
        return;
    }

    if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        pitchArpHeldCount = 0;
        pitchArpKeyDown.fill (false);
        pitchArpSustained.fill (false);
        pitchArpPoolDirty = true;
    }
}

void SynthEngine::buildPitchArpPool (PitchArpState& state,
                                     const PitchArpParameters& parameters)
{
    state.poolCount = 0;
    state.rootNote = 60;
    if (parameters.mode <= 0)
        return;

    const auto freeMode = parameters.mode == 5;
    // The note-on snapshot remains authoritative throughout the phrase and
    // release, matching Biscot1's stable root and full release arpeggio.
    const auto useLatch = pitchArpLatchValid && pitchArpLatchCount > 0;
    const auto useLive = ! useLatch && pitchArpHeldCount > 0 && pitchArpLiveCount > 0;
    if (! useLive && ! useLatch)
        return;

    const auto count = useLive ? pitchArpLiveCount : pitchArpLatchCount;
    const auto& notes = freeMode
        ? (useLive ? pitchArpLiveFree : pitchArpLatchFree)
        : (useLive ? pitchArpLiveSorted : pitchArpLatchSorted);
    state.rootNote = freeMode
        ? (useLive ? pitchArpLiveFreeRoot : pitchArpLatchFreeRoot)
        : (useLive ? pitchArpLiveSortedRoot : pitchArpLatchSortedRoot);
    if (parameters.mode == 1)
    {
        state.poolCount = state.sidSequenceLength;
        return;
    }
    const auto octaveUp = std::max (0, parameters.octaveUp);
    const auto octaveDown = std::abs (std::min (0, parameters.octaveDown));
    const auto append = [&state] (int note)
    {
        if (state.poolCount < static_cast<int> (state.pool.size()))
            state.pool[static_cast<std::size_t> (state.poolCount++)] = note;
    };

    if (parameters.mode == 3)
    {
        for (int octave = octaveUp; octave >= -octaveDown; --octave)
            for (int index = count - 1; index >= 0; --index)
                append (notes[static_cast<std::size_t> (index)] + octave * 12);
    }
    else
    {
        std::array<int, 160> linear {};
        auto linearCount = 0;
        for (int octave = -octaveDown; octave <= octaveUp; ++octave)
            for (int index = 0; index < count; ++index)
            {
                const auto note = notes[static_cast<std::size_t> (index)] + octave * 12;
                if (parameters.mode == 4)
                {
                    if (linearCount < static_cast<int> (linear.size()))
                        linear[static_cast<std::size_t> (linearCount++)] = note;
                }
                else
                    append (note);
            }
        if (parameters.mode == 4)
        {
            for (int index = 0; index < linearCount; ++index)
                append (linear[static_cast<std::size_t> (index)]);
            for (int index = linearCount - 2; index > 0; --index)
                append (linear[static_cast<std::size_t> (index)]);
        }
    }

    if (state.poolCount == 0)
        append (state.rootNote);
    if (state.stepIndex >= state.poolCount)
        state.stepIndex = -1;
}

void SynthEngine::advancePitchArps (const Params& p)
{
    if (pitchArpPoolDirty)
    {
        buildPitchArpPool (pitchArpState1, p.pitchArp1);
        buildPitchArpPool (pitchArpState2, p.pitchArp2);
        pitchArpPoolDirty = false;
    }

    const auto samplesPerFourBeats = sampleRate * 60.0 / p.tempoBpm * 4.0;
    const auto advance = [samplesPerFourBeats]
        (PitchArpState& state, const PitchArpParameters& parameters, double randomOffset)
    {
        if (parameters.mode > 0 && state.poolCount > 0)
        {
            state.stepClock += std::max (0.125f, parameters.rate) / samplesPerFourBeats;
            auto newStep = false;
            if (state.stepIndex < 0)
            {
                state.stepClock = 0.0;
                newStep = true;
            }
            else if (state.stepClock >= 1.0)
            {
                state.stepClock -= 1.0;
                newStep = true;
            }
            if (newStep)
            {
                state.stepIndex = (state.stepIndex + 1) % state.poolCount;
                auto selectedIndex = state.stepIndex;
                if (parameters.mode == 6)
                {
                    ++state.randomSeed;
                    auto randomValue = std::abs (std::sin (
                        state.randomSeed * 12.9898 + randomOffset) * 43758.5453);
                    randomValue -= std::floor (randomValue);
                    selectedIndex = juce::jlimit (0, state.poolCount - 1,
                        static_cast<int> (std::floor (randomValue * state.poolCount)));
                }
                const auto selectedNote = parameters.mode == 1
                    ? state.sidSequence[static_cast<std::size_t> (selectedIndex)]
                    : state.pool[static_cast<std::size_t> (selectedIndex)];
                state.pitchTarget = static_cast<float> (selectedNote - state.rootNote);
            }
        }
        else
        {
            state.stepClock = 0.0;
            state.stepIndex = -1;
            state.pitchTarget = 0.0f;
        }

        const auto glideAmount = juce::jlimit (0.0f, 1.0f, parameters.glide);
        if (glideAmount <= 0.0f)
            state.pitchCurrent = state.pitchTarget;
        else
        {
            const auto glideSquared = glideAmount * glideAmount;
            const auto glideCoefficient = 1.0f / (1.0f + glideSquared * 2000.0f);
            state.pitchCurrent += (state.pitchTarget - state.pitchCurrent)
                                * glideCoefficient;
            if (std::abs (state.pitchTarget - state.pitchCurrent) < 0.00001f)
                state.pitchCurrent = state.pitchTarget;
        }
    };

    advance (pitchArpState1, p.pitchArp1, 1.2345);
    advance (pitchArpState2, p.pitchArp2, 9.8765);
}

void SynthEngine::removeSourceMonoNote (int sourceIndex, int note)
{
    sourceIndex = juce::jlimit (0, 1, sourceIndex);
    auto& count = sourceMonoNoteCount[static_cast<std::size_t> (sourceIndex)];
    auto& notes = sourceMonoNoteStack[static_cast<std::size_t> (sourceIndex)];
    auto& velocities = sourceMonoVelocityStack[static_cast<std::size_t> (sourceIndex)];
    for (int index = 0; index < count; ++index)
    {
        if (notes[static_cast<std::size_t> (index)] != note)
            continue;
        for (int move = index; move + 1 < count; ++move)
        {
            notes[static_cast<std::size_t> (move)] = notes[static_cast<std::size_t> (move + 1)];
            velocities[static_cast<std::size_t> (move)] = velocities[static_cast<std::size_t> (move + 1)];
        }
        --count;
        return;
    }
}

void SynthEngine::handleSourceMonoNoteOn (int sourceIndex, int note, float velocity,
                                          const Params& p)
{
    sourceIndex = juce::jlimit (0, 1, sourceIndex);
    const auto layerMask = sourceIndex == 0 ? 1 : 2;
    const auto bankStart = sourceIndex == 0 ? 0 : layer2VoiceBankStart;
    auto& notes = sourceMonoNoteStack[static_cast<std::size_t> (sourceIndex)];
    auto& velocities = sourceMonoVelocityStack[static_cast<std::size_t> (sourceIndex)];
    auto& count = sourceMonoNoteCount[static_cast<std::size_t> (sourceIndex)];

    removeSourceMonoNote (sourceIndex, note);
    const auto previousNoteHeld = count > 0;
    if (count < static_cast<int> (notes.size()))
    {
        notes[static_cast<std::size_t> (count)] = note;
        velocities[static_cast<std::size_t> (count)] = velocity;
        ++count;
    }

    auto& v = voices[static_cast<std::size_t> (bankStart)];
    const auto wasActive = v.active;
    const auto wasLegato = wasActive && previousNoteHeld;
    const auto allowPortamento = p.monoPortamentoMode == 0
                              || (p.monoPortamentoMode == 1 && wasLegato)
                              || (p.monoPortamentoMode == 2 && ! wasLegato);
    const auto forceInstant = p.portamento[static_cast<std::size_t> (sourceIndex)] <= epsilon
                           || ! allowPortamento;
    const auto retrigger = p.monoNoteMode == 0 || ! wasActive || ! previousNoteHeld;
    const auto previousFrequency = v.frequency;
    const auto previousDrift = v.drift;
    const auto previousPing = v.ping;
    const auto target = 440.0 * std::exp2 ((note - 69) / 12.0);

    if (retrigger)
    {
        v = {};
        v.active = v.held = true;
        v.layerMask = layerMask;
        v.note = note;
        setVoicePerformanceTargets (v, v.note, velocity, p, true);
        v.stage = Stage::attack;
        v.stage2 = usesAdsr2 (p) ? Stage::attack : Stage::idle;
        v.age = ++ageCounter;
        v.drift = wasActive ? previousDrift : randomSigned();
        randomizeUnisonPhases (v);
        if (cachedRenderConstants.microMotionAny)
            seedVoiceMicroMotion (v);
        v.pwm1 = v.pwm2 = juce::jlimit (0.05f, 0.95f, p.pwm);
        v.ping = -previousPing;
        v.targetFrequency = target;
        v.frequency = ! wasActive || forceInstant ? target : std::max (1.0, previousFrequency);
        retriggerLfos (p);
        return;
    }

    v.active = v.held = true;
    v.layerMask = layerMask;
    v.note = note;
    setVoicePerformanceTargets (v, v.note, velocity, p, false);
    v.targetFrequency = target;
    v.age = ++ageCounter;
    if (forceInstant)
        v.frequency = target;
    if (v.stage == Stage::release || v.stage == Stage::idle)
        v.stage = Stage::sustain;
    if (usesAdsr2 (p) && (v.stage2 == Stage::release || v.stage2 == Stage::idle))
        v.stage2 = Stage::sustain;
}

void SynthEngine::handleSourceMonoNoteOff (int sourceIndex, int note, const Params& p)
{
    sourceIndex = juce::jlimit (0, 1, sourceIndex);
    const auto layerMask = sourceIndex == 0 ? 1 : 2;
    const auto bankStart = sourceIndex == 0 ? 0 : layer2VoiceBankStart;
    auto& notes = sourceMonoNoteStack[static_cast<std::size_t> (sourceIndex)];
    auto& velocities = sourceMonoVelocityStack[static_cast<std::size_t> (sourceIndex)];
    auto& count = sourceMonoNoteCount[static_cast<std::size_t> (sourceIndex)];
    auto& v = voices[static_cast<std::size_t> (bankStart)];
    const auto wasCurrent = v.active && v.layerMask == layerMask && v.note == note;
    removeSourceMonoNote (sourceIndex, note);

    if (count > 0)
    {
        if (! wasCurrent)
            return;

        const auto stackIndex = static_cast<std::size_t> (count - 1);
        const auto nextNote = notes[stackIndex];
        const auto nextVelocity = velocities[stackIndex];
        const auto allowPortamento = p.monoPortamentoMode != 2;
        const auto forceInstant = p.portamento[static_cast<std::size_t> (sourceIndex)] <= epsilon
                               || ! allowPortamento;
        const auto retrigger = p.monoNoteMode == 0;
        const auto previousFrequency = v.frequency;
        const auto previousDrift = v.drift;
        const auto previousPing = v.ping;
        const auto target = 440.0 * std::exp2 ((nextNote - 69) / 12.0);

        if (retrigger)
        {
            v = {};
            v.active = v.held = true;
            v.layerMask = layerMask;
            v.note = nextNote;
            setVoicePerformanceTargets (v, v.note, nextVelocity, p, true);
            v.stage = Stage::attack;
            v.stage2 = usesAdsr2 (p) ? Stage::attack : Stage::idle;
            v.age = ++ageCounter;
            v.drift = previousDrift;
            randomizeUnisonPhases (v);
            if (cachedRenderConstants.microMotionAny)
                seedVoiceMicroMotion (v);
            v.pwm1 = v.pwm2 = juce::jlimit (0.05f, 0.95f, p.pwm);
            v.ping = -previousPing;
            v.targetFrequency = target;
            v.frequency = forceInstant ? target : std::max (1.0, previousFrequency);
            retriggerLfos (p);
        }
        else
        {
            v.held = true;
            v.layerMask = layerMask;
            v.note = nextNote;
            setVoicePerformanceTargets (v, v.note, nextVelocity, p, false);
            v.targetFrequency = target;
            v.age = ++ageCounter;
            if (forceInstant)
                v.frequency = target;
            if (v.stage == Stage::release || v.stage == Stage::idle)
                v.stage = Stage::sustain;
            if (usesAdsr2 (p) && (v.stage2 == Stage::release || v.stage2 == Stage::idle))
                v.stage2 = Stage::sustain;
        }
        return;
    }

    if (! v.active || v.layerMask != layerMask)
        return;
    v.held = false;
    if (! sustainPedal[static_cast<std::size_t> (sourceIndex)])
    {
        v.stage = Stage::release;
        if (v.stage2 != Stage::idle)
            v.stage2 = Stage::release;
    }
}

void SynthEngine::removeMonoNote (int note)
{
    for (int index = 0; index < monoNoteCount; ++index)
    {
        if (monoNoteStack[static_cast<std::size_t> (index)] != note)
            continue;
        for (int move = index; move + 1 < monoNoteCount; ++move)
        {
            monoNoteStack[static_cast<std::size_t> (move)]
                = monoNoteStack[static_cast<std::size_t> (move + 1)];
            monoVelocityStack[static_cast<std::size_t> (move)]
                = monoVelocityStack[static_cast<std::size_t> (move + 1)];
        }
        --monoNoteCount;
        return;
    }
}

void SynthEngine::handleMonoNoteOn (int note, float velocity, const Params& p)
{
    removeMonoNote (note);
    const auto previousNoteHeld = monoNoteCount > 0;
    if (monoNoteCount < static_cast<int> (monoNoteStack.size()))
    {
        monoNoteStack[static_cast<std::size_t> (monoNoteCount)] = note;
        monoVelocityStack[static_cast<std::size_t> (monoNoteCount)] = velocity;
        ++monoNoteCount;
    }

    auto& v = voices[0];
    const auto wasActive = v.active;
    const auto wasLegato = wasActive && previousNoteHeld;
    const auto allowPortamento = p.monoPortamentoMode == 0
                              || (p.monoPortamentoMode == 1 && wasLegato)
                              || (p.monoPortamentoMode == 2 && ! wasLegato);
    const auto forceInstant = p.portamento[0] <= epsilon || ! allowPortamento;
    const auto retrigger = p.monoNoteMode == 0 || ! wasActive || ! previousNoteHeld;
    const auto previousFrequency = v.frequency;
    const auto previousDrift = v.drift;
    const auto previousPing = v.ping;
    const auto target = 440.0 * std::exp2 ((note - 69) / 12.0);

    if (retrigger)
    {
        v = {};
        v.active = v.held = true;
        v.note = note;
        setVoicePerformanceTargets (v, v.note, velocity, p, true);
        v.stage = Stage::attack;
        v.stage2 = usesAdsr2 (p) ? Stage::attack : Stage::idle;
        v.age = ++ageCounter;
        v.drift = wasActive ? previousDrift : randomSigned();
        randomizeUnisonPhases (v);
        if (cachedRenderConstants.microMotionAny)
            seedVoiceMicroMotion (v);
        v.pwm1 = v.pwm2 = juce::jlimit (0.05f, 0.95f, p.pwm);
        v.ping = -previousPing;
        v.targetFrequency = target;
        v.frequency = ! wasActive || forceInstant ? target : std::max (1.0, previousFrequency);
        retriggerLfos (p);
        return;
    }

    v.active = v.held = true;
    v.note = note;
    setVoicePerformanceTargets (v, v.note, velocity, p, false);
    v.targetFrequency = target;
    v.age = ++ageCounter;
    if (forceInstant)
        v.frequency = target;
    if (v.stage == Stage::release || v.stage == Stage::idle)
        v.stage = Stage::sustain;
    if (usesAdsr2 (p) && (v.stage2 == Stage::release || v.stage2 == Stage::idle))
        v.stage2 = Stage::sustain;
}

void SynthEngine::handleMonoNoteOff (int note, const Params& p)
{
    auto& v = voices[0];
    const auto wasCurrent = v.active && v.note == note;
    removeMonoNote (note);

    if (monoNoteCount > 0)
    {
        if (! wasCurrent)
            return;

        const auto stackIndex = static_cast<std::size_t> (monoNoteCount - 1);
        const auto nextNote = monoNoteStack[stackIndex];
        const auto nextVelocity = monoVelocityStack[stackIndex];
        const auto allowPortamento = p.monoPortamentoMode != 2;
        const auto forceInstant = p.portamento[0] <= epsilon || ! allowPortamento;
        const auto retrigger = p.monoNoteMode == 0;
        const auto previousFrequency = v.frequency;
        const auto previousDrift = v.drift;
        const auto previousPing = v.ping;
        const auto target = 440.0 * std::exp2 ((nextNote - 69) / 12.0);

        if (retrigger)
        {
            v = {};
            v.active = v.held = true;
            v.note = nextNote;
            setVoicePerformanceTargets (v, v.note, nextVelocity, p, true);
            v.stage = Stage::attack;
            v.stage2 = usesAdsr2 (p) ? Stage::attack : Stage::idle;
            v.age = ++ageCounter;
            v.drift = previousDrift;
            randomizeUnisonPhases (v);
            if (cachedRenderConstants.microMotionAny)
                seedVoiceMicroMotion (v);
            v.pwm1 = v.pwm2 = juce::jlimit (0.05f, 0.95f, p.pwm);
            v.ping = -previousPing;
            v.targetFrequency = target;
            v.frequency = forceInstant ? target : std::max (1.0, previousFrequency);
            retriggerLfos (p);
        }
        else
        {
            v.held = true;
            v.note = nextNote;
            setVoicePerformanceTargets (v, v.note, nextVelocity, p, false);
            v.targetFrequency = target;
            v.age = ++ageCounter;
            if (forceInstant)
                v.frequency = target;
            if (v.stage == Stage::release || v.stage == Stage::idle)
                v.stage = Stage::sustain;
            if (usesAdsr2 (p) && (v.stage2 == Stage::release || v.stage2 == Stage::idle))
                v.stage2 = Stage::sustain;
        }
        return;
    }

    if (! v.active)
        return;
    v.held = false;
    if (! anySustainPedal())
    {
        v.stage = Stage::release;
        if (v.stage2 != Stage::idle)
            v.stage2 = Stage::release;
    }
}

SynthEngine::Voice* SynthEngine::allocate (const Params& p, int layerMask)
{
    const auto bankStart = layerMask == 2 ? layer2VoiceBankStart
                         : layerMask == 4 ? noiseSequencerVoiceIndex
                                          : 0;
    const auto bankCount = layerMask == 4 ? 1 : p.voiceCount;
    auto begin = voices.begin() + bankStart;
    const auto end = begin + bankCount;

    if (const auto free = std::find_if (begin, end,
                                        [] (const Voice& v) { return ! v.active; });
        free != end)
        return &*free;

    Voice* oldestReleased = nullptr;
    Voice* oldestAny = nullptr;
    for (auto voice = begin; voice != end; ++voice)
    {
        if (voice->stage == Stage::steal)
            continue;
        if (! voice->held && (oldestReleased == nullptr || voice->age < oldestReleased->age))
            oldestReleased = &*voice;
        if (oldestAny == nullptr || voice->age < oldestAny->age)
            oldestAny = &*voice;
    }
    return oldestReleased != nullptr ? oldestReleased : oldestAny;
}

float SynthEngine::advanceEnvelope (Voice& v, const Params& p, float attackIncrement,
                                    float decayIncrement, float releaseIncrement,
                                    float stealCoefficient)
{
    switch (v.stage)
    {
        case Stage::attack:
            v.envelope += (1.0f - v.envelope) * attackIncrement;
            if (v.envelope >= 0.999f)
            {
                v.envelope = 1.0f;
                v.stage = Stage::decay;
            }
            break;
        case Stage::decay:
            v.envelope += (p.sustain - v.envelope) * decayIncrement;
            if (std::abs (v.envelope - p.sustain) < 0.0005f)
            {
                v.envelope = p.sustain;
                v.stage = Stage::sustain;
            }
            break;
        case Stage::sustain:
            v.envelope = p.sustain;
            break;
        case Stage::release:
            v.envelope *= 1.0f - releaseIncrement;
            if (v.envelope <= 0.00001f)
            {
                v.envelope = 0.0f;
                v.stage = Stage::idle;
            }
            break;
        case Stage::steal:
            v.envelope *= stealCoefficient;
            if (v.envelope <= 0.0001f)
            {
                const auto registerFrequency = v.frequency;
                const auto registerDrift = v.drift;
                const auto registerPing = v.ping;
                const auto pending = v.pending;
                const auto pendingNote = v.pendingNote;
                const auto pendingLayerMask = v.pendingLayerMask;
                const auto pendingVelocity = v.pendingVelocity;
                v = {};
                v.frequency = registerFrequency;

                if (pending)
                {
                    v.active = v.held = true;
                    v.layerMask = pendingLayerMask;
                    v.note = pendingNote;
                    setVoicePerformanceTargets (v, v.note, pendingVelocity, p, true);
                    v.stage = Stage::attack;
                    v.stage2 = usesAdsr2 (p) ? Stage::attack : Stage::idle;
                    v.age = ++ageCounter;
                    v.drift = registerDrift;
                    v.ping = registerPing;
                    if (cachedRenderConstants.microMotionAny)
                        seedVoiceMicroMotion (v);
                    v.pwm1 = v.pwm2 = juce::jlimit (0.05f, 0.95f, p.pwm);
                    v.targetFrequency = 440.0 * std::exp2 ((v.note - 69) / 12.0);
                    if (portamentoForMask (p, v.layerMask) <= epsilon)
                        v.frequency = v.targetFrequency;
                }
            }
            break;
        case Stage::idle:
            v.envelope = 0.0f;
            break;
    }
    return juce::jlimit (0.0f, 1.0f, v.envelope);
}

float SynthEngine::advanceEnvelope2 (Voice& v, const Params& p, bool adsr2Used,
                                     float attackIncrement, float decayIncrement,
                                     float releaseIncrement) const
{
    if (! adsr2Used)
    {
        v.envelope2 = 0.0f;
        v.stage2 = Stage::idle;
        return 0.0f;
    }

    // The JSFX starts ADSR2 for an already-held voice when a destination is
    // enabled after note-on. This keeps live automation and preset changes
    // consistent without requiring the player to retrigger the note.
    if (v.stage2 == Stage::idle && v.held)
    {
        v.envelope2 = 0.0f;
        v.stage2 = Stage::attack;
    }

    switch (v.stage2)
    {
        case Stage::attack:
            v.envelope2 += (1.0f - v.envelope2) * attackIncrement;
            if (v.envelope2 >= 0.999f)
            {
                v.envelope2 = 1.0f;
                v.stage2 = Stage::decay;
            }
            break;
        case Stage::decay:
            v.envelope2 += (p.sustain2 - v.envelope2) * decayIncrement;
            if (std::abs (v.envelope2 - p.sustain2) < 0.0005f)
            {
                v.envelope2 = p.sustain2;
                v.stage2 = Stage::sustain;
            }
            break;
        case Stage::sustain:
            v.envelope2 = p.sustain2;
            break;
        case Stage::release:
            v.envelope2 *= 1.0f - releaseIncrement;
            if (v.envelope2 <= 0.00001f)
            {
                v.envelope2 = 0.0f;
                v.stage2 = Stage::idle;
            }
            break;
        case Stage::idle:
            v.envelope2 = 0.0f;
            break;
    }
    return juce::jlimit (0.0f, 1.0f, v.envelope2);
}

void SynthEngine::retriggerLfos (const Params& p)
{
    const auto retrigger = [] (LfoState& state, const LfoParameters& parameters)
    {
        const auto oneShot = parameters.oneShot && parameters.oneShotPercent > 0.0f;
        if (oneShot)
        {
            state.oneShotPosition = 0.0;
            state.oneShotDone = false;
        }
        if (parameters.mode == 1 || oneShot)
        {
            state.phase = 0.0;
            state.previousPhase = 1.0f;
            state.delayEnvelope = 0.0f;
        }
    };
    retrigger (lfoState1, p.lfo1);
    retrigger (lfoState2, p.lfo2);
}

float SynthEngine::advanceLfo (LfoState& state, const LfoParameters& p,
                               float envelopeSource, float crossSource)
{
    const auto baseCyclesPerSecond = std::max (0.0f, p.rate);
    const auto rateMultiplier = std::exp2 (envelopeSource * p.envelopeRate)
                              * std::exp2 (crossSource * p.crossRate);
    const auto increment = std::max (0.0,
        static_cast<double> (baseCyclesPerSecond * rateMultiplier) / sampleRate);
    const auto oneShot = p.oneShot && p.oneShotPercent > 0.0f;

    if (! (oneShot && state.oneShotDone))
    {
        auto advance = increment;
        if (oneShot)
        {
            const auto limit = juce::jlimit (0.0, 1.0,
                                              static_cast<double> (p.oneShotPercent) * 0.01);
            advance = std::min (advance, std::max (0.0, limit - state.oneShotPosition));
            state.oneShotPosition += advance;
            if (state.oneShotPosition >= limit)
                state.oneShotDone = true;
        }
        state.phase = wrapPhase (state.phase + advance);
    }

    const auto effectivePhase = wrapPhase (state.phase + juce::jlimit (0.0f, 1.0f, p.phase));
    float raw = 0.0f;
    switch (p.wave)
    {
        case 0: raw = static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * effectivePhase)); break;
        case 1: raw = effectivePhase < 0.5 ? static_cast<float> (effectivePhase * 4.0 - 1.0)
                                          : static_cast<float> (3.0 - effectivePhase * 4.0); break;
        case 2: raw = static_cast<float> (effectivePhase * 2.0 - 1.0); break;
        case 3: raw = static_cast<float> (1.0 - effectivePhase * 2.0); break;
        case 4: raw = effectivePhase < juce::jlimit (0.05f, 0.95f, p.squarePwm) ? 1.0f : -1.0f; break;
        default:
            if (effectivePhase < state.previousPhase)
                state.sampleHold = randomSigned();
            raw = state.sampleHold;
            break;
    }

    const auto smoothSquared = p.smooth * p.smooth;
    if (smoothSquared > 0.0f)
    {
        const auto coefficient = p.wave == 5
                               ? 1.0f / (1.0f + smoothSquared * 2000.0f)
                               : 1.0f / (1.0f + smoothSquared * 400.0f);
        state.smoothState += (raw - state.smoothState) * coefficient;
        state.coreValue = state.smoothState;
    }
    else
    {
        state.smoothState = raw;
        state.coreValue = raw;
    }
    state.previousPhase = static_cast<float> (effectivePhase);

    if (p.delay > epsilon)
    {
        state.delayEnvelope += (1.0f - state.delayEnvelope)
                             * (1.0f / std::max (1.0f, p.delay * static_cast<float> (sampleRate)));
        return state.coreValue * state.delayEnvelope;
    }

    state.delayEnvelope = 1.0f;
    return state.coreValue;
}

void SynthEngine::render (float& left, float& right, std::array<float, 8>& aux,
                          const Params& p, const RenderConstants& d,
                          float dryInputLeft, float dryInputRight,
                          float sidechainLeft, float sidechainRight)
{
    const auto sourceMidiRoutingActive = p.sourceMidiChannel[0] != 0
                                      || p.sourceMidiChannel[1] != 0
                                      || p.sourceMidiChannel[2] != 0;
    const auto independentPortamento = std::abs (p.portamento[0] - p.portamento[1]) > epsilon
                                   || std::abs (p.portamento[0] - p.portamento[2]) > epsilon
                                   || std::abs (p.portamento[1] - p.portamento[2]) > epsilon;
    const auto independentVoiceMode = p.sourceVoiceMode[0] != 0 || p.sourceVoiceMode[1] != 0;
    const auto sourceNoteRoutingActive = p.sourceNoteSource[0] != 0
                                      || p.sourceNoteSource[1] != 0
                                      || p.sourceNoteSource[2] != 0;
    const auto extendedVoiceBanksActive = previousSequencerMode != 0
                                       || sourceMidiRoutingActive
                                       || independentPortamento
                                       || independentVoiceMode
                                       || sourceNoteRoutingActive;
    advancePitchArps (p);
    float lfo1 = 0.0f, lfo2 = 0.0f;
    if (d.lfo1Needed || d.lfo2Needed)
    {
        auto voicesLive = false;
        for (int voiceIndex = 0; voiceIndex < static_cast<int> (voices.size()); ++voiceIndex)
        {
            const auto slotEnabled = extendedVoiceBanksActive
                ? (voiceIndex < p.voiceCount
                   || (voiceIndex >= layer2VoiceBankStart
                       && voiceIndex < layer2VoiceBankStart + p.voiceCount)
                   || voiceIndex == noiseSequencerVoiceIndex)
                : voiceIndex < p.voiceCount;
            if (slotEnabled && voices[static_cast<std::size_t> (voiceIndex)].active)
            {
                voicesLive = true;
                break;
            }
        }
        if (voicesLive || d.delayNeedsLfo)
        {
            if (d.lfo1Needed)
            {
                auto lfoParameters = d.lfo1;
                if (larpPlaysSynth (p.larp.state)
                    && std::abs (p.larp.lfo1RateDepth) > epsilon)
                    lfoParameters.rate *= std::pow (
                        16.0f, larpState.modulation * p.larp.lfo1RateDepth);
                lfo1 = advanceLfo (lfoState1, lfoParameters,
                                   globalPitchEnvelope1, lfoState2.coreValue);
                lfo1 *= lfo1 >= 0.0f ? 1.0f - d.lfo1.upperSquash
                                     : 1.0f - d.lfo1.lowerSquash;

            }
            if (d.lfo2Needed)
            {
                auto lfoParameters = d.lfo2;
                if (larpPlaysSynth (p.larp.state)
                    && std::abs (p.larp.lfo2RateDepth) > epsilon)
                    lfoParameters.rate *= std::pow (
                        16.0f, larpState.modulation * p.larp.lfo2RateDepth);
                lfo2 = advanceLfo (lfoState2, lfoParameters,
                                   globalPitchEnvelope2, lfoState1.coreValue);
                lfo2 *= lfo2 >= 0.0f ? 1.0f - d.lfo2.upperSquash
                                     : 1.0f - d.lfo2.lowerSquash;

            }
        }
    }

    std::array<float, 3> sourceLfo1 { lfo1, lfo1, lfo1 };
    std::array<float, 3> sourceLfo2 { lfo2, lfo2, lfo2 };
    for (std::size_t source = 0; source < sourceLfo1.size(); ++source)
    {
        if (std::abs (modWheel[source]) > epsilon
            && std::abs (p.lfo1ModWheelAmount) > epsilon)
            sourceLfo1[source] *= 1.0f + modWheel[source] * p.lfo1ModWheelAmount;
        if (std::abs (channelAftertouch[source]) > epsilon
            && std::abs (p.lfo2AftertouchAmount) > epsilon)
            sourceLfo2[source] *= 1.0f + channelAftertouch[source] * p.lfo2AftertouchAmount;
    }
    const auto globalModWheel = std::max ({ modWheel[0], modWheel[1], modWheel[2] });
    const auto globalAftertouch = std::max ({ channelAftertouch[0], channelAftertouch[1],
                                              channelAftertouch[2] });
    const auto lfo1Global = std::abs (p.lfo1ModWheelAmount) > epsilon
        ? lfo1 * (1.0f + globalModWheel * p.lfo1ModWheelAmount) : lfo1;
    const auto lfo2Global = std::abs (p.lfo2AftertouchAmount) > epsilon
        ? lfo2 * (1.0f + globalAftertouch * p.lfo2AftertouchAmount) : lfo2;

    const auto pitchArp1Active = p.pitchArp1.mode > 0 && pitchArpState1.poolCount > 0;
    const auto pitchArp2Active = p.pitchArp2.mode > 0 && pitchArpState2.poolCount > 0;
    const auto pitchArp1Semitones = pitchArpState1.pitchCurrent;
    const auto pitchArp2Semitones = pitchArpState2.pitchCurrent;
    const auto pitchArp1PitchDynamic = p.pitchArp1.pitchMovement
        && (pitchArp1Active || std::abs (pitchArp1Semitones) > epsilon);
    const auto pitchArp2PitchDynamic = p.pitchArp2.pitchMovement
        && (pitchArp2Active || std::abs (pitchArp2Semitones) > epsilon);
    const auto pitchArpRootFrequency1 = pitchArp1PitchDynamic
        ? 440.0 * std::exp2 ((pitchArpState1.rootNote - 69) / 12.0) : 0.0;
    const auto pitchArpRootFrequency2 = pitchArp2PitchDynamic
        ? 440.0 * std::exp2 ((pitchArpState2.rootNote - 69) / 12.0) : 0.0;
    auto pitchArpMix = 0.0f;
    auto pitchArpMixCount = 0;
    if (p.pitchArp1.mode > 0)
    {
        pitchArpMix += pitchArp1Semitones;
        ++pitchArpMixCount;
    }
    if (p.pitchArp2.mode > 0)
    {
        pitchArpMix += pitchArp2Semitones;
        ++pitchArpMixCount;
    }
    if (pitchArpMixCount > 0)
        pitchArpMix /= static_cast<float> (pitchArpMixCount);
    const auto pitchArpLowPassOctaves = pitchArpMix / 12.0f * p.pitchArpLowPassDepth;
    const auto pitchArpHighPassOctaves = pitchArpMix / 12.0f * p.pitchArpHighPassDepth;
    const auto pitchArpPan = pitchArpMix / 12.0f * p.pitchArpPanDepth;
    const auto pitchArpLowPassDynamic = pitchArpMixCount > 0
                                     && std::abs (p.pitchArpLowPassDepth) > epsilon;
    const auto pitchArpHighPassDynamic = pitchArpMixCount > 0
                                      && std::abs (p.pitchArpHighPassDepth) > epsilon;
    const auto pitchArp1Volume = std::max (
        0.0f, 1.0f + pitchArp1Semitones / 12.0f * p.pitchArp1.volumeDepth);
    const auto pitchArp2Volume = std::max (
        0.0f, 1.0f + pitchArp2Semitones / 12.0f * p.pitchArp2.volumeDepth);
    const auto larpActive = larpPlaysSynth (p.larp.state);
    const std::array<float, 3> larpModulation {{
        larpActive && p.sourceNoteSource[0] == 2 ? larpState.modulation : 0.0f,
        larpActive && p.sourceNoteSource[1] == 2 ? larpState.modulation : 0.0f,
        larpActive && p.sourceNoteSource[2] == 2 ? larpState.modulation : 0.0f
    }};
    const std::array<float, 3> larpVolume {{
        std::max (0.0f, 1.0f + larpModulation[0] * p.larp.volumeDepth),
        std::max (0.0f, 1.0f + larpModulation[1] * p.larp.volumeDepth),
        std::max (0.0f, 1.0f + larpModulation[2] * p.larp.volumeDepth)
    }};
    const auto lfo1PitchL1Depth = juce::jlimit (-48.0f, 48.0f, p.lfo1PitchL1 + p.legacyLfoPitch1);
    const auto lfo1PitchL2Depth = juce::jlimit (-48.0f, 48.0f, p.lfo1PitchL2 + p.legacyLfoPitch1);
    const auto lfo1PitchNoiseDepth = juce::jlimit (-48.0f, 48.0f, p.lfo1NoisePitch + p.legacyLfoPitch1);
    const auto lfo2PitchL1Depth = juce::jlimit (-48.0f, 48.0f, p.lfo2PitchL1 + p.legacyLfoPitch2);
    const auto lfo2PitchL2Depth = juce::jlimit (-48.0f, 48.0f, p.lfo2PitchL2 + p.legacyLfoPitch2);
    const auto lfo2PitchNoiseDepth = juce::jlimit (-48.0f, 48.0f, p.lfo2NoisePitch + p.legacyLfoPitch2);

    const auto lfo1PwmL1Depth = juce::jlimit (-1.0f, 1.0f, p.lfo1PwmL1 + p.legacyLfoPwm1);
    const auto lfo1PwmL2Depth = juce::jlimit (-1.0f, 1.0f, p.lfo1PwmL2 + p.legacyLfoPwm1);
    const auto lfo2PwmL1Depth = juce::jlimit (-1.0f, 1.0f, p.lfo2PwmL1 + p.legacyLfoPwm2);
    const auto lfo2PwmL2Depth = juce::jlimit (-1.0f, 1.0f, p.lfo2PwmL2 + p.legacyLfoPwm2);
    const auto pwmTarget1 = juce::jlimit (0.05f, 0.95f,
        p.pwm + sourceLfo1[0] * lfo1PwmL1Depth + sourceLfo2[0] * lfo2PwmL1Depth
              + pitchArp1Semitones / 12.0f * p.pitchArp1.pwmDepth
              + larpModulation[0] * p.larp.pwmDepth);
    const auto pwmTarget2 = juce::jlimit (0.05f, 0.95f,
        p.pwm + sourceLfo1[1] * lfo1PwmL2Depth + sourceLfo2[1] * lfo2PwmL2Depth
              + pitchArp2Semitones / 12.0f * p.pitchArp2.pwmDepth
              + larpModulation[1] * p.larp.pwmDepth);

    const std::array<float, 3> larpPitchMod {{
        larpModulation[0] * p.larp.pitchDepth * 48.0f,
        larpModulation[1] * p.larp.pitchDepth * 48.0f,
        larpModulation[2] * p.larp.pitchDepth * 48.0f
    }};
    const auto pitchLfoL1 = sourceLfo1[0] * lfo1PitchL1Depth + sourceLfo2[0] * lfo2PitchL1Depth
                          + larpPitchMod[0];
    const auto pitchLfoL2 = sourceLfo1[1] * lfo1PitchL2Depth + sourceLfo2[1] * lfo2PitchL2Depth
                          + larpPitchMod[1];
    const auto pitchLfoNoise = sourceLfo1[2] * lfo1PitchNoiseDepth + sourceLfo2[2] * lfo2PitchNoiseDepth
                             + larpPitchMod[2];

    const std::array<float, 3> lowPassLfoOctaves {{
        sourceLfo1[0] * juce::jlimit (-8.0f, 8.0f, p.lfo1LowPassL1 + p.legacyLfoLowPass1)
            + sourceLfo2[0] * juce::jlimit (-8.0f, 8.0f, p.lfo2LowPassL1 + p.legacyLfoLowPass2),
        sourceLfo1[1] * juce::jlimit (-8.0f, 8.0f, p.lfo1LowPassL2 + p.legacyLfoLowPass1)
            + sourceLfo2[1] * juce::jlimit (-8.0f, 8.0f, p.lfo2LowPassL2 + p.legacyLfoLowPass2),
        sourceLfo1[2] * juce::jlimit (-8.0f, 8.0f, p.lfo1LowPassNoise + p.legacyLfoLowPass1)
            + sourceLfo2[2] * juce::jlimit (-8.0f, 8.0f, p.lfo2LowPassNoise + p.legacyLfoLowPass2)
    }};
    const std::array<float, 3> highPassLfoOctaves {{
        sourceLfo1[0] * juce::jlimit (-8.0f, 8.0f, p.lfo1HighPassL1 + p.legacyLfoHighPass1)
            + sourceLfo2[0] * juce::jlimit (-8.0f, 8.0f, p.lfo2HighPassL1 + p.legacyLfoHighPass2),
        sourceLfo1[1] * juce::jlimit (-8.0f, 8.0f, p.lfo1HighPassL2 + p.legacyLfoHighPass1)
            + sourceLfo2[1] * juce::jlimit (-8.0f, 8.0f, p.lfo2HighPassL2 + p.legacyLfoHighPass2),
        sourceLfo1[2] * juce::jlimit (-8.0f, 8.0f, p.lfo1HighPassNoise + p.legacyLfoHighPass1)
            + sourceLfo2[2] * juce::jlimit (-8.0f, 8.0f, p.lfo2HighPassNoise + p.legacyLfoHighPass2)
    }};

    const auto sourceVolumeL1 = std::max (0.0f,
        (1.0f + sourceLfo1[0] * juce::jlimit (-1.0f, 1.0f, p.lfo1VolumeL1 + p.legacyLfoVolume1))
      * (1.0f + sourceLfo2[0] * juce::jlimit (-1.0f, 1.0f, p.lfo2VolumeL1 + p.legacyLfoVolume2)));
    const auto sourceVolumeL2 = std::max (0.0f,
        (1.0f + sourceLfo1[1] * juce::jlimit (-1.0f, 1.0f, p.lfo1VolumeL2 + p.legacyLfoVolume1))
      * (1.0f + sourceLfo2[1] * juce::jlimit (-1.0f, 1.0f, p.lfo2VolumeL2 + p.legacyLfoVolume2)));
    const auto sourceVolumeNoise = std::max (0.0f,
        (1.0f + sourceLfo1[2] * juce::jlimit (-1.0f, 1.0f, p.lfo1VolumeNoise + p.legacyLfoVolume1))
      * (1.0f + sourceLfo2[2] * juce::jlimit (-1.0f, 1.0f, p.lfo2VolumeNoise + p.legacyLfoVolume2)));

    const auto larpPanStage = larpActive ? larpState.panStage * p.larp.panDepth : 0.0f;
    const auto sourcePanL1 = sourceLfo1[0] * juce::jlimit (-1.0f, 1.0f, p.lfo1PanL1 + p.legacyLfoPan1)
                           + sourceLfo2[0] * juce::jlimit (-1.0f, 1.0f, p.lfo2PanL1 + p.legacyLfoPan2)
                           + (p.sourceNoteSource[0] == 2 ? larpPanStage : 0.0f);
    const auto sourcePanL2 = sourceLfo1[1] * juce::jlimit (-1.0f, 1.0f, p.lfo1PanL2 + p.legacyLfoPan1)
                           + sourceLfo2[1] * juce::jlimit (-1.0f, 1.0f, p.lfo2PanL2 + p.legacyLfoPan2)
                           + (p.sourceNoteSource[1] == 2 ? larpPanStage : 0.0f);
    const auto sourcePanNoise = sourceLfo1[2] * juce::jlimit (-1.0f, 1.0f, p.lfo1PanNoise + p.legacyLfoPan1)
                              + sourceLfo2[2] * juce::jlimit (-1.0f, 1.0f, p.lfo2PanNoise + p.legacyLfoPan2)
                              + (p.sourceNoteSource[2] == 2 ? larpPanStage : 0.0f);
    const auto morph1 = juce::jlimit (0.0f, 1.0f, p.morph1
                                      + sourceLfo1[0] * p.lfo1Morph1 + sourceLfo2[0] * p.lfo2Morph1);
    const auto morph2 = juce::jlimit (0.0f, 1.0f, p.morph2
                                      + sourceLfo1[1] * p.lfo1Morph2 + sourceLfo2[1] * p.lfo2Morph2);
    const auto dynamicWaveMod1 = std::min (10.0f, std::abs (sourceLfo1[0]) * p.waveModLfo1
                                                 + std::abs (sourceLfo2[0]) * p.waveModLfo2);
    const auto dynamicWaveMod2 = std::min (10.0f, std::abs (sourceLfo1[1]) * p.waveModLfo1
                                                 + std::abs (sourceLfo2[1]) * p.waveModLfo2);
    const auto noBaseWaveMod = std::abs (p.metal1) <= epsilon && std::abs (p.metal2) <= epsilon
                            && std::abs (p.shark1) <= epsilon && std::abs (p.shark2) <= epsilon
                            && std::abs (p.sync1) <= epsilon && std::abs (p.sync2) <= epsilon;
    const auto waveModValue = [noBaseWaveMod] (float base, float dynamicWaveMod)
    {
        return noBaseWaveMod ? dynamicWaveMod : juce::jlimit (0.0f, 10.0f, base + dynamicWaveMod);
    };
    const auto metal1 = waveModValue (p.metal1, dynamicWaveMod1);
    const auto metal2 = waveModValue (p.metal2, dynamicWaveMod2);
    const auto shark1 = waveModValue (p.shark1, dynamicWaveMod1);
    const auto shark2 = waveModValue (p.shark2, dynamicWaveMod2);
    const auto sync1 = waveModValue (p.sync1, dynamicWaveMod1);
    const auto sync2 = waveModValue (p.sync2, dynamicWaveMod2);
    const auto phaseMod21 = p.phaseMod21 * p.phaseMod21 * 0.15f
                          * (p.wave1 == 5 ? superWavePmGain (p.superWave1)
                                         : morphPmGain (morph1));
    const auto phaseMod12 = p.phaseMod12 * p.phaseMod12 * 0.15f
                          * (p.wave2 == 5 ? superWavePmGain (p.superWave2)
                                         : morphPmGain (morph2));
    const auto layer1CanEmit = std::abs (p.level1 * d.balance1) > epsilon;
    const auto layer2CanEmit = std::abs (p.level2 * d.balance2) > epsilon;
    const auto oscillator1Needed = layer1CanEmit || (layer2CanEmit && phaseMod12 > epsilon);
    const auto oscillator2Needed = layer2CanEmit || (layer1CanEmit && phaseMod21 > epsilon);

    const auto biquadStatesMatch = [] (const BiquadState& a, const BiquadState& b)
    {
        return a.x1 == b.x1 && a.x2 == b.x2 && a.y1 == b.y1 && a.y2 == b.y2;
    };
    const auto processStereoBiquad = [&biquadStatesMatch] (float& signalLeft,
                                                           float& signalRight,
                                                           BiquadState& stateLeft,
                                                           BiquadState& stateRight,
                                                           const BiquadCoefficients& coefficients)
    {
        // Centred layers enter both channels with exactly the same sample.
        // If their histories also match, a single filter kernel produces the
        // exact same result as two independent identical calls.
        if (signalLeft == signalRight && biquadStatesMatch (stateLeft, stateRight))
        {
            signalLeft = processBiquad (signalLeft, stateLeft, coefficients);
            signalRight = signalLeft;
            stateRight = stateLeft;
            return;
        }
        signalLeft = processBiquad (signalLeft, stateLeft, coefficients);
        signalRight = processBiquad (signalRight, stateRight, coefficients);
    };

    const auto sourcePanModulationActive = std::abs (p.legacyLfoPan1) > epsilon
        || std::abs (p.legacyLfoPan2) > epsilon
        || std::abs (p.lfo1PanL1) > epsilon || std::abs (p.lfo1PanL2) > epsilon
        || std::abs (p.lfo1PanNoise) > epsilon
        || std::abs (p.lfo2PanL1) > epsilon || std::abs (p.lfo2PanL2) > epsilon
        || std::abs (p.lfo2PanNoise) > epsilon;
    const auto threeDepthsDiffer = [] (float a, float b, float c)
    {
        return std::abs (a - b) > epsilon || std::abs (a - c) > epsilon;
    };
    const auto expressionLfo1Differs = std::abs (p.lfo1ModWheelAmount) > epsilon
        && threeDepthsDiffer (modWheel[0], modWheel[1], modWheel[2]);
    const auto expressionLfo2Differs = std::abs (p.lfo2AftertouchAmount) > epsilon
        && threeDepthsDiffer (channelAftertouch[0], channelAftertouch[1], channelAftertouch[2]);
    const auto sourceFilterModulationActive =
           threeDepthsDiffer (p.lfo1LowPassL1, p.lfo1LowPassL2, p.lfo1LowPassNoise)
        || threeDepthsDiffer (p.lfo2LowPassL1, p.lfo2LowPassL2, p.lfo2LowPassNoise)
        || threeDepthsDiffer (p.lfo1HighPassL1, p.lfo1HighPassL2, p.lfo1HighPassNoise)
        || threeDepthsDiffer (p.lfo2HighPassL1, p.lfo2HighPassL2, p.lfo2HighPassNoise)
        || expressionLfo1Differs || expressionLfo2Differs;
    // Multi-output requires the three source stems at all times. Keeping the
    // filter paths separate is mathematically equivalent for these linear
    // filters, while preserving L1/L2/Noise identities for sends and aux outs.
    constexpr auto sourceFxRoutingActive = true;
    const auto separateFilterBuses = true;
    const auto separateSourceBuses = true;
    (void) sourcePanModulationActive;
    (void) sourceFilterModulationActive;

    float fxLayer1Left = 0.0f, fxLayer1Right = 0.0f;
    float fxLayer2Left = 0.0f, fxLayer2Right = 0.0f;
    float fxNoiseLeft = 0.0f, fxNoiseRight = 0.0f;
    float pitchEnvelopeMaximum = 0.0f;
    const auto renderVoiceSlots = extendedVoiceBanksActive
        ? static_cast<int> (voices.size())
        : p.voiceCount;
    for (int voiceIndex = 0; voiceIndex < renderVoiceSlots; ++voiceIndex)
    {
        if (extendedVoiceBanksActive)
        {
            const auto slotEnabled = voiceIndex < p.voiceCount
                || (voiceIndex >= layer2VoiceBankStart
                    && voiceIndex < layer2VoiceBankStart + p.voiceCount)
                || voiceIndex == noiseSequencerVoiceIndex;
            if (! slotEnabled)
                continue;
        }

        auto& v = voices[static_cast<size_t> (voiceIndex)];
        if (! v.active)
            continue;

        advanceVoiceMicroMotion (v, p, d);

        if (d.velocityRuntimeNeeded)
        {
            v.velocitySmoothed += (v.velocity - v.velocitySmoothed) * d.velocityCoefficient;
            v.velocitySmoothed = juce::jlimit (0.0f, 1.0f, v.velocitySmoothed);
            if (v.velocitySmoothed >= v.velocityFilterSmoothed)
                v.velocityFilterSmoothed = v.velocitySmoothed;
            else
                v.velocityFilterSmoothed += (v.velocitySmoothed - v.velocityFilterSmoothed)
                                          * d.velocityFilterCoefficient;
            v.velocityFilterSmoothed = juce::jlimit (0.0f, 1.0f,
                                                      v.velocityFilterSmoothed);
        }

        const auto keyFollowDelta = v.keyFollowLowPassTarget - v.keyFollowLowPass;
        bool keyFollowSettled = false;
        if (std::abs (keyFollowDelta) <= 0.000001f)
        {
            v.keyFollowLowPass = v.keyFollowLowPassTarget;
            keyFollowSettled = true;
        }
        else if (keyFollowDelta >= 0.0f)
        {
            // The JSFX makes upward register moves immediate and smooths only
            // downward moves, which is part of Poly-2's characteristic sweep.
            v.keyFollowLowPass = v.keyFollowLowPassTarget;
            keyFollowSettled = true;
        }
        else
        {
            v.keyFollowLowPass += keyFollowDelta * d.keyFollowCoefficient;
            keyFollowSettled = std::abs (v.keyFollowLowPassTarget
                                         - v.keyFollowLowPass) <= 0.000001f;
        }
        v.keyFollowLowPass = std::max (0.000001f, v.keyFollowLowPass);
        const auto keyFollowOctaves = keyFollowSettled
                                    ? v.keyFollowOctavesTarget
                                    : std::log2 (v.keyFollowLowPass);

        const auto envelope1 = advanceEnvelope (v, p, d.attackIncrement1,
                                                d.decayIncrement1, d.releaseIncrement1,
                                                d.stealCoefficient);
        const auto envelope2 = advanceEnvelope2 (v, p, d.adsr2Used, d.attackIncrement2,
                                                  d.decayIncrement2, d.releaseIncrement2);

        if (v.stage == Stage::attack)
            v.pitchEnvelope += (1.0f - v.pitchEnvelope) * d.attackIncrement1;
        else if (v.stage == Stage::decay)
            v.pitchEnvelope += (0.0f - v.pitchEnvelope) * d.decayIncrement1;
        else
            v.pitchEnvelope = 0.0f;

        if (std::abs (d.releaseShape) > 0.001f && v.stage == Stage::release)
            v.releasePitch += (d.releaseShape - v.releasePitch)
                            * d.releaseIncrement1;
        else
            v.releasePitch = 0.0f;
        if (std::abs (d.releaseShape) > 0.001f && v.stage2 == Stage::release)
            v.releasePitch2 += (d.releaseShape - v.releasePitch2)
                             * d.releaseIncrement2;
        else
            v.releasePitch2 = 0.0f;

        const auto pitchEnvelope1 = v.pitchEnvelope + v.releasePitch;
        const auto pitchEnvelope2 = envelope2 + v.releasePitch2;
        const auto pitchEnvelope = pitchEnvelope1 * (1.0f - p.pitchAdsr2Blend)
                                 + pitchEnvelope2 * p.pitchAdsr2Blend;
        pitchEnvelopeMaximum = std::max (pitchEnvelopeMaximum, pitchEnvelope);

        const auto voicePortamentoAmount = (v.layerMask & 2) != 0 && (v.layerMask & 1) == 0
            ? d.portamentoAmount[1]
            : ((v.layerMask & 4) != 0 && (v.layerMask & 3) == 0
                ? d.portamentoAmount[2]
                : d.portamentoAmount[0]);
        if (voicePortamentoAmount > 0.0f)
        {
            v.frequency += (v.targetFrequency - v.frequency)
                         / (1.0 + voicePortamentoAmount * 2000.0);
            v.frequency = std::max (1.0, v.frequency);
        }
        else
            v.frequency = v.targetFrequency;

        const auto commonPerformancePitch = p.masterToneSemitones
                                          + v.drift * p.drift * 0.5f
                                          + pitchEnvelope * p.pitchEnvelopeAmount;
        const auto bendSemitones1 = pitchBend[0] * p.pitchBendRange[0];
        const auto bendSemitones2 = pitchBend[1] * p.pitchBendRange[1];
        const auto bendSemitonesNoise = pitchBend[2] * p.pitchBendRange[2];
        const auto performancePitch1 = commonPerformancePitch
                                     + bendSemitones1 + pitchLfoL1;
        const auto performancePitch2 = commonPerformancePitch
                                     + bendSemitones2 + pitchLfoL2;
        const auto performancePitchNoise = commonPerformancePitch
                                         + bendSemitonesNoise + pitchLfoNoise;
        const auto rootFrequency1 = pitchArp1PitchDynamic
            ? pitchArpRootFrequency1 : v.frequency;
        const auto rootFrequency2 = pitchArp2PitchDynamic
            ? pitchArpRootFrequency2 : v.frequency;
        if (pitchArp1PitchDynamic || pitchArp2PitchDynamic)
        {
            const auto base1 = rootFrequency1
                             * std::exp2 ((performancePitch1
                                          + (p.pitchArp1.pitchMovement
                                             ? pitchArp1Semitones : 0.0f)) / 12.0);
            const auto base2 = rootFrequency2
                             * std::exp2 ((performancePitch2
                                          + (p.pitchArp2.pitchMovement
                                             ? pitchArp2Semitones : 0.0f)) / 12.0);
            v.cachedIncrement1 = std::min (0.49, base1 * d.oscillatorTuning1 / sampleRate);
            v.cachedIncrement2 = std::min (0.49, base2 * d.oscillatorTuning2 / sampleRate);
        }
        else if (d.continuousPitchModulation)
        {
            const auto base1 = v.frequency * std::exp2 (performancePitch1 / 12.0);
            const auto base2 = v.frequency * std::exp2 (performancePitch2 / 12.0);
            v.cachedIncrement1 = std::min (0.49, base1 * d.oscillatorTuning1 / sampleRate);
            v.cachedIncrement2 = std::min (0.49, base2 * d.oscillatorTuning2 / sampleRate);
        }
        else if (v.frequency != v.cachedPitchFrequency
                 || commonPerformancePitch != v.cachedPerformancePitch
                 || bendSemitones1 != v.cachedPitchBend1
                 || bendSemitones2 != v.cachedPitchBend2)
        {
            const auto base1 = v.frequency
                * std::exp2 ((commonPerformancePitch + bendSemitones1) / 12.0);
            const auto base2 = v.frequency
                * std::exp2 ((commonPerformancePitch + bendSemitones2) / 12.0);
            v.cachedIncrement1 = std::min (0.49, base1 * d.oscillatorTuning1 / sampleRate);
            v.cachedIncrement2 = std::min (0.49, base2 * d.oscillatorTuning2 / sampleRate);
            v.cachedPitchFrequency = v.frequency;
            v.cachedPerformancePitch = commonPerformancePitch;
            v.cachedPitchBend1 = bendSemitones1;
            v.cachedPitchBend2 = bendSemitones2;
        }
        const auto increment1 = std::min (0.49, v.cachedIncrement1
                                                * v.microMotionPitchMultiplier1);
        const auto increment2 = std::min (0.49, v.cachedIncrement2
                                                * v.microMotionPitchMultiplier2);
        v.phase1 = wrapPhase (v.phase1 + increment1);
        v.phase2 = wrapPhase (v.phase2 + increment2);
        v.pwm1 += (pwmTarget1 - v.pwm1) * d.pwmCoefficient;
        v.pwm2 += (pwmTarget2 - v.pwm2) * d.pwmCoefficient;
        const auto renderedPwm1 = p.microMotionPwm1 > epsilon
            ? juce::jlimit (0.05f, 0.95f, v.pwm1
                + v.microMotionOut1 * p.microMotionPwm1 * 4.0f)
            : v.pwm1;
        const auto renderedPwm2 = p.microMotionPwm2 > epsilon
            ? juce::jlimit (0.05f, 0.95f, v.pwm2
                + v.microMotionOut2 * p.microMotionPwm2 * 4.0f)
            : v.pwm2;

        const auto oddVoiceSign1 = p.superWave1.invertOddVoices && (voiceIndex & 1) == 0
                                  ? -1.0f : 1.0f;
        const auto oddVoiceSign2 = p.superWave2.invertOddVoices && (voiceIndex & 1) == 0
                                  ? -1.0f : 1.0f;
        const auto superWaveLength1 = juce::jlimit (0.125f, 16.0f,
            p.superWave1.length + (sourceLfo1[0] * p.superWave1.lfo1Length
                                 + sourceLfo2[0] * p.superWave1.lfo2Length) * oddVoiceSign1);
        const auto superWaveLength2 = juce::jlimit (0.125f, 16.0f,
            p.superWave2.length + (sourceLfo1[1] * p.superWave2.lfo1Length
                                 + sourceLfo2[1] * p.superWave2.lfo2Length) * oddVoiceSign2);
        const auto renderOscillatorPair = [&] (double sourcePhase1, double sourcePhase2,
                                                double sourceIncrement1, double sourceIncrement2,
                                                float& triangle1, float& triangle2,
                                                bool applyCrossModulation,
                                                bool renderFirst, bool renderSecond)
        {
            auto renderedPhase1 = sourcePhase1;
            auto renderedPhase2 = sourcePhase2;
            if (renderFirst && applyCrossModulation && phaseMod21 > epsilon)
                renderedPhase1 = wrapPhase (renderedPhase1 + (p.wave2 == 5
                    ? modulationSuperWave (p.superWave2, renderedPhase2, sourceIncrement2,
                                           renderedPwm2, triangle2, metal2, shark2, sync2,
                                           superWaveLength2)
                    : modulationMorph (morph2, renderedPhase2, renderedPwm2)) * phaseMod21);
            if (renderSecond && applyCrossModulation && phaseMod12 > epsilon)
                renderedPhase2 = wrapPhase (renderedPhase2 + (p.wave1 == 5
                    ? modulationSuperWave (p.superWave1, renderedPhase1, sourceIncrement1,
                                           renderedPwm1, triangle1, metal1, shark1, sync1,
                                           superWaveLength1)
                    : modulationMorph (morph1, renderedPhase1, renderedPwm1)) * phaseMod12);

            auto first = 0.0f;
            if (renderFirst)
                first = p.wave1 == 5
                    ? superWaveOscillator (p.superWave1, renderedPhase1, sourceIncrement1,
                                           renderedPwm1, triangle1, metal1, shark1, sync1,
                                           superWaveLength1)
                    : (! d.morph1Dynamic && d.staticWaveBlend1 <= epsilon
                        ? oscillator (d.staticWave1, renderedPhase1, sourceIncrement1,
                                      renderedPwm1, triangle1, metal1, shark1, sync1)
                        : morphOscillator (morph1, renderedPhase1, sourceIncrement1, renderedPwm1,
                                           triangle1, metal1, shark1, sync1));
            auto second = 0.0f;
            if (renderSecond)
                second = p.wave2 == 5
                    ? superWaveOscillator (p.superWave2, renderedPhase2, sourceIncrement2,
                                           renderedPwm2, triangle2, metal2, shark2, sync2,
                                           superWaveLength2)
                    : (! d.morph2Dynamic && d.staticWaveBlend2 <= epsilon
                        ? oscillator (d.staticWave2, renderedPhase2, sourceIncrement2,
                                      renderedPwm2, triangle2, metal2, shark2, sync2)
                        : morphOscillator (morph2, renderedPhase2, sourceIncrement2, renderedPwm2,
                                           triangle2, metal2, shark2, sync2));
            return std::array<float, 2> { first, second };
        };

        const auto voiceOscillator1Needed = oscillator1Needed && (v.layerMask & 1) != 0;
        const auto voiceOscillator2Needed = oscillator2Needed && (v.layerMask & 2) != 0;
        const auto oscillators = renderOscillatorPair (v.phase1, v.phase2, increment1, increment2,
                                                        v.triangleState1, v.triangleState2, true,
                                                        voiceOscillator1Needed,
                                                        voiceOscillator2Needed);
        const auto ampEnvelope1 = envelope1 * (1.0f - p.ampBlend1)
                                + envelope2 * p.ampBlend1;
        const auto ampEnvelope2 = envelope1 * (1.0f - p.ampBlend2)
                                + envelope2 * p.ampBlend2;
        const auto splitNoteIndex = static_cast<std::size_t> (juce::jlimit (0, 127, v.note));
        const auto splitGain1 = d.splitGain1[splitNoteIndex];
        const auto splitGain2 = d.splitGain2[splitNoteIndex];

        const auto layer1Gain = (v.layerMask & 1) != 0
            ? p.level1 * d.balance1 * ampEnvelope1 * sourceVolumeL1
                * splitGain1 * pitchArp1Volume
            : 0.0f;
        const auto layer2Gain = (v.layerMask & 2) != 0
            ? p.level2 * d.balance2 * ampEnvelope2 * sourceVolumeL2
                * splitGain2 * pitchArp2Volume
            : 0.0f;
        const auto layer1 = oscillators[0] * layer1Gain;
        const auto layer2 = oscillators[1] * layer2Gain;
        const auto layer1Pan = p.microMotionPan1 > epsilon
            ? juce::jlimit (-1.0f, 1.0f,
                            p.pan1 + v.microMotionOut1 * p.microMotionPan1 * 10.0f)
            : p.pan1;
        const auto layer2Pan = p.microMotionPan2 > epsilon
            ? juce::jlimit (-1.0f, 1.0f,
                            p.pan2 + v.microMotionOut2 * p.microMotionPan2 * 10.0f)
            : p.pan2;
        const auto layer1PanLeft = p.microMotionPan1 > epsilon
            ? panGain (layer1Pan, true) : d.layer1PanLeft;
        const auto layer1PanRight = p.microMotionPan1 > epsilon
            ? panGain (layer1Pan, false) : d.layer1PanRight;
        const auto layer2PanLeft = p.microMotionPan2 > epsilon
            ? panGain (layer2Pan, true) : d.layer2PanLeft;
        const auto layer2PanRight = p.microMotionPan2 > epsilon
            ? panGain (layer2Pan, false) : d.layer2PanRight;
        auto routedLayer1Left = layer1 * layer1PanLeft;
        auto routedLayer1Right = layer1 * layer1PanRight;
        auto routedLayer2Left = layer2 * layer2PanLeft;
        auto routedLayer2Right = layer2 * layer2PanRight;
        auto voiceLeft = routedLayer1Left + routedLayer2Left;
        auto voiceRight = routedLayer1Right + routedLayer2Right;

        const auto monoVoice = v.layerMask == 1 ? sourceUsesMono (p, 0)
                             : v.layerMask == 2 ? sourceUsesMono (p, 1)
                             : (p.voiceCount == 1 && (v.layerMask & 3) != 0);
        const auto unisonVoices = monoVoice ? p.monoUnisonVoices : 1;
        if (unisonVoices > 1)
        {
            const auto monoPan1 = 0.5f * (layer1PanLeft + layer1PanRight);
            const auto monoPan2 = 0.5f * (layer2PanLeft + layer2PanRight);
            for (int clone = 1; clone < unisonVoices; ++clone)
            {
                const auto stateIndex = static_cast<std::size_t> (clone - 1);
                const auto detuneMultiplier = d.unisonDetuneMultiplier[stateIndex];
                const auto cloneIncrement1 = std::min (0.49, increment1 * detuneMultiplier);
                const auto cloneIncrement2 = std::min (0.49, increment2 * detuneMultiplier);
                v.unisonPhase1[stateIndex] = wrapPhase (v.unisonPhase1[stateIndex]
                                                        + cloneIncrement1);
                v.unisonPhase2[stateIndex] = wrapPhase (v.unisonPhase2[stateIndex]
                                                        + cloneIncrement2);
                const auto cloneOscillators = renderOscillatorPair (
                    v.unisonPhase1[stateIndex], v.unisonPhase2[stateIndex],
                    cloneIncrement1, cloneIncrement2,
                    v.unisonTriangle1[stateIndex], v.unisonTriangle2[stateIndex],
                    false, voiceOscillator1Needed, voiceOscillator2Needed);
                const auto cloneMono = cloneOscillators[0] * layer1Gain * monoPan1
                                     + cloneOscillators[1] * layer2Gain * monoPan2;
                const auto clonePanLeft = d.unisonPanLeft[stateIndex];
                const auto clonePanRight = d.unisonPanRight[stateIndex];
                if (separateSourceBuses)
                {
                    const auto cloneLayer1 = cloneOscillators[0] * layer1Gain * monoPan1;
                    const auto cloneLayer2 = cloneOscillators[1] * layer2Gain * monoPan2;
                    routedLayer1Left += cloneLayer1 * clonePanLeft;
                    routedLayer1Right += cloneLayer1 * clonePanRight;
                    routedLayer2Left += cloneLayer2 * clonePanLeft;
                    routedLayer2Right += cloneLayer2 * clonePanRight;
                }
                else
                {
                    voiceLeft += cloneMono * clonePanLeft;
                    voiceRight += cloneMono * clonePanRight;
                }
            }
            const auto normalisation = d.unisonNormalisation;
            if (separateSourceBuses)
            {
                routedLayer1Left *= normalisation;
                routedLayer1Right *= normalisation;
                routedLayer2Left *= normalisation;
                routedLayer2Right *= normalisation;
                voiceLeft = routedLayer1Left + routedLayer2Left;
                voiceRight = routedLayer1Right + routedLayer2Right;
            }
            else
            {
                voiceLeft *= normalisation;
                voiceRight *= normalisation;
            }
        }

        const auto panDirection = (v.note & 1) != 0 ? -1.0f : 1.0f;
        const auto applySourcePan = [] (float& busLeft, float& busRight, float pan)
        {
            if (std::abs (pan) <= epsilon)
                return;
            constexpr auto centre = 0.7071067811865476f;
            busLeft *= panGain (pan, true) / centre;
            busRight *= panGain (pan, false) / centre;
        };
        applySourcePan (routedLayer1Left, routedLayer1Right, panDirection * sourcePanL1);
        applySourcePan (routedLayer2Left, routedLayer2Right, panDirection * sourcePanL2);

        auto routedNoiseLeft = 0.0f;
        auto routedNoiseRight = 0.0f;
        const auto noiseEnabledForVoice = (v.layerMask & 4) != 0;
        if (noiseEnabledForVoice && p.noiseLevel > epsilon)
        {
            const auto noiseBase = rootFrequency1
                * std::exp2 ((performancePitchNoise
                              + (p.pitchArp1.pitchMovement ? pitchArp1Semitones : 0.0f)) / 12.0);
            const auto noiseClockIncrement = std::min (0.49,
                noiseBase * d.oscillatorTuning1 / sampleRate) * 4.0
                * d.noisePitchMultiplier;
            auto noise = renderNoisePair (v, p.noiseType,
                                          p.noiseColor * 2.0f - 1.0f,
                                          noiseClockIncrement);

            // Classic Color already crossfades its original brown/pink/white
            // generator. Every explicitly selected noise instead gets the same
            // centred 0..1 tone control: 0.5 is an exact bypass, lower values
            // soften transitions and upper values accent them.
            if (p.noiseType != 0)
            {
                constexpr auto colourLowPass = 0.035f;
                v.noiseColourLowLeft += colourLowPass
                                      * (noise[0] - v.noiseColourLowLeft);
                v.noiseColourLowRight += colourLowPass
                                       * (noise[1] - v.noiseColourLowRight);
                const auto highLeft = noise[0] - v.noiseColourPreviousLeft;
                const auto highRight = noise[1] - v.noiseColourPreviousRight;
                v.noiseColourPreviousLeft = noise[0];
                v.noiseColourPreviousRight = noise[1];
                const auto colour = juce::jlimit (0.0f, 1.0f, p.noiseColor);
                if (colour < 0.5f)
                {
                    const auto amount = (0.5f - colour) * 2.0f;
                    noise[0] = noise[0] * (1.0f - amount)
                             + v.noiseColourLowLeft * amount;
                    noise[1] = noise[1] * (1.0f - amount)
                             + v.noiseColourLowRight * amount;
                }
                else if (colour > 0.5f)
                {
                    const auto amount = (colour - 0.5f) * 1.5f;
                    noise[0] = juce::jlimit (-1.0f, 1.0f, noise[0] + highLeft * amount);
                    noise[1] = juce::jlimit (-1.0f, 1.0f, noise[1] + highRight * amount);
                }
            }

            if (p.noiseType == 10)
            {
                constexpr auto digitalDustGain = 3.9810717055349722f; // +12 dB
                noise[0] *= digitalDustGain;
                noise[1] *= digitalDustGain;
            }

            // Zero is a true mono source at the original left-channel level;
            // one restores the independently generated right channel.
            noise[1] = noise[0] + (noise[1] - noise[0])
                                * juce::jlimit (0.0f, 1.0f, p.noiseStereo);
            const auto noiseEnvelope = envelope1 * (1.0f - p.noiseBlend)
                                     + envelope2 * p.noiseBlend;
            const auto noiseGain = p.noiseLevel * std::max (0.0f, noiseEnvelope)
                                 * sourceVolumeNoise;
            routedNoiseLeft = noise[0] * noiseGain;
            routedNoiseRight = noise[1] * noiseGain;
            applySourcePan (routedNoiseLeft, routedNoiseRight,
                            juce::jlimit (-1.0f, 1.0f,
                                          p.noisePan + panDirection * sourcePanNoise));
            if (separateSourceBuses)
            {
                voiceLeft = routedNoiseLeft;
                voiceRight = routedNoiseRight;
            }
            else
            {
                voiceLeft += routedNoiseLeft;
                voiceRight += routedNoiseRight;
            }
        }
        else if (separateSourceBuses)
        {
            voiceLeft = 0.0f;
            voiceRight = 0.0f;
        }

        // Pan needs separate source buses only until the sources have been
        // positioned. If the filters themselves do not need independent LFO
        // depths, merge here and retain the original single-filter fast path.
        if (separateSourceBuses && ! separateFilterBuses)
        {
            voiceLeft += routedLayer1Left + routedLayer2Left;
            voiceRight += routedLayer1Right + routedLayer2Right;
        }

        // Optional local filters sit before the historical main filter.  They use
        // exactly the main filter's musical cutoff mapping and Q curves rather
        // than an EQ-style Hz range.  At the neutral extremes they are true
        // bypass, preserving the existing sound and avoiding needless CPU.
        const auto processLocalFilter = [&] (float& busLeft, float& busRight,
                                              LocalFilterState& state,
                                              std::size_t sourceIndex)
        {
            const auto lowPassActive = d.localLowPassActive[sourceIndex];
            const auto highPassActive = d.localHighPassActive[sourceIndex];
            const auto lpSlope = d.localLowPassSlope[sourceIndex];

            if (lowPassActive)
            {
                if (! state.lowPassActive)
                {
                    state.lowPassLeft = state.lowPassRight = {};
                    state.lowPass2Left = state.lowPass2Right = {};
                    state.lowPassCoefficientFrequency = -1.0f;
                    state.lowPassCoefficientQ = -1.0f;
                    state.lowPassActive = true;
                }

                const auto lpFrequency = d.localLowPassFrequency[sourceIndex];
                const auto lpQ = d.localLowPassQ[sourceIndex];

                if (lpFrequency != state.lowPassCoefficientFrequency
                    || lpQ != state.lowPassCoefficientQ)
                {
                    state.lowPassCoefficients = makeLowPass (sampleRate, lpFrequency, lpQ);
                    state.lowPassCoefficientFrequency = lpFrequency;
                    state.lowPassCoefficientQ = lpQ;
                }

                processStereoBiquad (busLeft, busRight,
                                     state.lowPassLeft, state.lowPassRight,
                                     state.lowPassCoefficients);
                if (lpSlope != 0)
                    processStereoBiquad (busLeft, busRight,
                                         state.lowPass2Left, state.lowPass2Right,
                                         state.lowPassCoefficients);
                else
                    state.lowPass2Left = state.lowPass2Right = {};
            }
            else if (state.lowPassActive)
            {
                state.lowPassLeft = state.lowPassRight = {};
                state.lowPass2Left = state.lowPass2Right = {};
                state.lowPassCoefficientFrequency = -1.0f;
                state.lowPassCoefficientQ = -1.0f;
                state.lowPassActive = false;
            }

            if (highPassActive)
            {
                if (! state.highPassActive)
                {
                    state.highPassLeft = state.highPassRight = {};
                    state.highPassCoefficientFrequency = -1.0f;
                    state.highPassCoefficientQ = -1.0f;
                    state.highPassActive = true;
                }

                const auto hpFrequency = d.localHighPassFrequency[sourceIndex];
                const auto hpQ = d.localHighPassQ[sourceIndex];

                if (hpFrequency != state.highPassCoefficientFrequency
                    || hpQ != state.highPassCoefficientQ)
                {
                    state.highPassCoefficients = makeHighPass (sampleRate, hpFrequency, hpQ);
                    state.highPassCoefficientFrequency = hpFrequency;
                    state.highPassCoefficientQ = hpQ;
                }

                processStereoBiquad (busLeft, busRight,
                                     state.highPassLeft, state.highPassRight,
                                     state.highPassCoefficients);
            }
            else if (state.highPassActive)
            {
                state.highPassLeft = state.highPassRight = {};
                state.highPassCoefficientFrequency = -1.0f;
                state.highPassCoefficientQ = -1.0f;
                state.highPassActive = false;
            }
        };

        processLocalFilter (routedLayer1Left, routedLayer1Right, v.localFilters[0], 0);
        processLocalFilter (routedLayer2Left, routedLayer2Right, v.localFilters[1], 1);
        processLocalFilter (voiceLeft, voiceRight, v.localFilters[2], 2);

        if (p.formantEnabled)
        {
            if (! v.formantWasEnabled)
            {
                v.formantLeft = {};
                v.formantRight = {};
                for (auto& route : v.routedFilters)
                {
                    route.formantLeft = {};
                    route.formantRight = {};
                }
                v.formantPosition = -1.0f;
                v.formantControlCounter = 0;
                v.formantWasEnabled = true;
            }
            const auto formantEnvelope = p.filterUsesAdsr2 ? envelope2 : envelope1;
            const auto formantPosition = juce::jlimit (0.0f, 5.0f,
                p.formantMorph + lfo1Global * p.lfo1FormantMorph
                               + lfo2Global * p.lfo2FormantMorph
                               + formantEnvelope * p.formantEnvelope
                               + v.microMotionCommon * p.microMotionFormant * 15.0f);
            const auto formantIsModulated = std::abs (p.lfo1FormantMorph) > epsilon
                                          || std::abs (p.lfo2FormantMorph) > epsilon
                                          || std::abs (p.formantEnvelope) > epsilon
                                          || p.microMotionFormant > epsilon;
            if (v.formantPosition < 0.0f
                || std::abs (formantPosition - v.formantPosition) > epsilon
                   && (! formantIsModulated || v.formantControlCounter <= 0))
            {
                v.formantCoefficients = makeFormants (sampleRate, formantPosition);
                v.formantPosition = formantPosition;
                v.formantControlCounter = formantIsModulated ? 7 : 0;
            }
            else if (formantIsModulated)
            {
                --v.formantControlCounter;
            }

            const auto processFormant = [&] (float& busLeft, float& busRight,
                                             std::array<BiquadState, 3>& statesLeft,
                                             std::array<BiquadState, 3>& statesRight)
            {
                const auto inputLeft = busLeft;
                const auto inputRight = busRight;
                busLeft = 0.0f;
                busRight = 0.0f;
                for (std::size_t band = 0; band < v.formantCoefficients.bands.size(); ++band)
                {
                    busLeft += v.formantCoefficients.amplitudes[band]
                             * processBiquad (inputLeft, statesLeft[band],
                                              v.formantCoefficients.bands[band]);
                    busRight += v.formantCoefficients.amplitudes[band]
                              * processBiquad (inputRight, statesRight[band],
                                               v.formantCoefficients.bands[band]);
                }
                busLeft *= 2.0f;
                busRight *= 2.0f;
            };

            if (! separateFilterBuses)
            {
                processFormant (voiceLeft, voiceRight, v.formantLeft, v.formantRight);
            }
            else
            {
                if (! p.filterLayerRouting || p.formantLayer1)
                    processFormant (routedLayer1Left, routedLayer1Right,
                                    v.routedFilters[0].formantLeft,
                                    v.routedFilters[0].formantRight);
                if (! p.filterLayerRouting || p.formantLayer2)
                    processFormant (routedLayer2Left, routedLayer2Right,
                                    v.routedFilters[1].formantLeft,
                                    v.routedFilters[1].formantRight);
                if (! p.filterLayerRouting || p.formantNoise)
                    processFormant (voiceLeft, voiceRight,
                                    v.routedFilters[2].formantLeft,
                                    v.routedFilters[2].formantRight);
            }
        }
        else
        {
            v.formantWasEnabled = false;
        }

        const auto noteIndex = static_cast<std::size_t> (juce::jlimit (0, 127, v.note));
        const auto keyFollowTerm = p.keyFollowFilterMode != 0
                                 ? keyFollowOctaves
                                 : keyFollowOctaves * envelope1;
        auto velocityForFilter = v.velocity;
        if (p.velocityFilter > epsilon)
        {
            const auto attackWeight = std::max (0.0f, 1.0f - envelope1 * 6.5f);
            velocityForFilter = v.velocity * attackWeight
                              + v.velocityFilterSmoothed * (1.0f - attackWeight);
        }
        velocityForFilter = juce::jlimit (0.0f, 1.0f, velocityForFilter);
        const auto velocityInverse = (1.0f - velocityForFilter)
                                   * (1.0f - velocityForFilter);
        const auto filterEnvelope = p.filterUsesAdsr2 ? envelope2 : envelope1;
        const auto lpEnvelopeOctaves = p.filterEnvelope * filterEnvelope * 4.0f
                                     - velocityInverse * p.velocityFilter * 6.0f;
        const auto commonLowPassOctaves = lpEnvelopeOctaves + keyFollowTerm
                                           + pitchArpLowPassOctaves
                                           + v.microMotionCommon
                                               * p.microMotionLowPass * 20.0f;
        const auto commonHighPassOctaves = d.noteHpKeyFollow[noteIndex]
                                            + pitchArpHighPassOctaves
                                            + v.microMotionCommon
                                                * p.microMotionHighPass * 20.0f;

        if (! separateFilterBuses)
        {
            const auto lowPassNeedsUpdate = ! d.lowPassStatic || ! keyFollowSettled
                                         || pitchArpLowPassDynamic
                                         || v.lowPassCoefficientFrequency < 0.0f
                                         || d.lpQ != v.lowPassCoefficientQ;
            if (lowPassNeedsUpdate)
            {
                const auto lpFrequency = juce::jlimit (20.0f, d.lpCoefficientMaximum,
                    d.lpBase * std::exp2 (commonLowPassOctaves + lowPassLfoOctaves[0]));
                if (lpFrequency != v.lowPassCoefficientFrequency || d.lpQ != v.lowPassCoefficientQ)
                {
                    v.lowPassCoefficients = makeLowPass (sampleRate, lpFrequency, d.lpQ);
                    v.lowPassCoefficientFrequency = lpFrequency;
                    v.lowPassCoefficientQ = d.lpQ;
                }
            }
            processStereoBiquad (voiceLeft, voiceRight, v.lowPassLeft, v.lowPassRight,
                                 v.lowPassCoefficients);
            if (p.lowPassSlope != 0)
                processStereoBiquad (voiceLeft, voiceRight,
                                     v.lowPass2Left, v.lowPass2Right,
                                     v.lowPassCoefficients);

            if (d.highPassEnabled)
            {
                const auto highPassNeedsUpdate = ! d.highPassStatic
                                              || pitchArpHighPassDynamic
                                              || v.highPassCoefficientFrequency < 0.0f
                                              || d.hpQ != v.highPassCoefficientQ;
                if (highPassNeedsUpdate)
                {
                    const auto hpFrequency = juce::jlimit (10.0f, d.hpCoefficientMaximum,
                        d.hpBase * std::exp2 (commonHighPassOctaves + highPassLfoOctaves[0]));
                    if (hpFrequency != v.highPassCoefficientFrequency
                        || d.hpQ != v.highPassCoefficientQ)
                    {
                        v.highPassCoefficients = makeHighPass (sampleRate, hpFrequency, d.hpQ);
                        v.highPassCoefficientFrequency = hpFrequency;
                        v.highPassCoefficientQ = d.hpQ;
                    }
                }
                processStereoBiquad (voiceLeft, voiceRight,
                                     v.highPassLeft, v.highPassRight,
                                     v.highPassCoefficients);
            }
        }
        else
        {
            if (! sourceFilterModulationActive)
            {
                const auto lowPassNeedsUpdate = ! d.lowPassStatic || ! keyFollowSettled
                                             || pitchArpLowPassDynamic
                                             || v.lowPassCoefficientFrequency < 0.0f
                                             || d.lpQ != v.lowPassCoefficientQ;
                if (lowPassNeedsUpdate)
                {
                    const auto lpFrequency = juce::jlimit (20.0f, d.lpCoefficientMaximum,
                        d.lpBase * std::exp2 (commonLowPassOctaves + lowPassLfoOctaves[0]));
                    if (lpFrequency != v.lowPassCoefficientFrequency
                        || d.lpQ != v.lowPassCoefficientQ)
                    {
                        v.lowPassCoefficients = makeLowPass (sampleRate, lpFrequency, d.lpQ);
                        v.lowPassCoefficientFrequency = lpFrequency;
                        v.lowPassCoefficientQ = d.lpQ;
                    }
                }
            }

            const auto processSourceLowPass = [&] (float& busLeft, float& busRight,
                                                    RoutedFilterState& route,
                                                    std::size_t sourceIndex,
                                                    bool enabled)
            {
                if (! enabled)
                    return;
                const BiquadCoefficients* coefficients = &v.lowPassCoefficients;
                if (sourceFilterModulationActive)
                {
                    const auto lpFrequency = juce::jlimit (20.0f, d.lpCoefficientMaximum,
                        d.lpBase * std::exp2 (commonLowPassOctaves
                                              + lowPassLfoOctaves[sourceIndex]));
                    if (lpFrequency != route.lowPassCoefficientFrequency
                        || d.lpQ != route.lowPassCoefficientQ)
                    {
                        route.lowPassCoefficients = makeLowPass (sampleRate, lpFrequency, d.lpQ);
                        route.lowPassCoefficientFrequency = lpFrequency;
                        route.lowPassCoefficientQ = d.lpQ;
                    }
                    coefficients = &route.lowPassCoefficients;
                }
                processStereoBiquad (busLeft, busRight, route.lowPassLeft, route.lowPassRight,
                                     *coefficients);
                if (p.lowPassSlope != 0)
                    processStereoBiquad (busLeft, busRight,
                                         route.lowPass2Left, route.lowPass2Right,
                                         *coefficients);
            };
            processSourceLowPass (routedLayer1Left, routedLayer1Right, v.routedFilters[0], 0,
                                  ! p.filterLayerRouting || p.lowPassLayer1);
            processSourceLowPass (routedLayer2Left, routedLayer2Right, v.routedFilters[1], 1,
                                  ! p.filterLayerRouting || p.lowPassLayer2);
            processSourceLowPass (voiceLeft, voiceRight, v.routedFilters[2], 2,
                                  ! p.filterLayerRouting || p.lowPassNoise);

            if (d.highPassEnabled)
            {
                if (! sourceFilterModulationActive)
                {
                    const auto highPassNeedsUpdate = ! d.highPassStatic
                                                  || pitchArpHighPassDynamic
                                                  || v.highPassCoefficientFrequency < 0.0f
                                                  || d.hpQ != v.highPassCoefficientQ;
                    if (highPassNeedsUpdate)
                    {
                        const auto hpFrequency = juce::jlimit (10.0f, d.hpCoefficientMaximum,
                            d.hpBase * std::exp2 (commonHighPassOctaves
                                                  + highPassLfoOctaves[0]));
                        if (hpFrequency != v.highPassCoefficientFrequency
                            || d.hpQ != v.highPassCoefficientQ)
                        {
                            v.highPassCoefficients = makeHighPass (sampleRate, hpFrequency, d.hpQ);
                            v.highPassCoefficientFrequency = hpFrequency;
                            v.highPassCoefficientQ = d.hpQ;
                        }
                    }
                }

                const auto processSourceHighPass = [&] (float& busLeft, float& busRight,
                                                         RoutedFilterState& route,
                                                         std::size_t sourceIndex,
                                                         bool enabled)
                {
                    if (! enabled)
                        return;
                    const BiquadCoefficients* coefficients = &v.highPassCoefficients;
                    if (sourceFilterModulationActive)
                    {
                        const auto hpFrequency = juce::jlimit (10.0f, d.hpCoefficientMaximum,
                            d.hpBase * std::exp2 (commonHighPassOctaves
                                                  + highPassLfoOctaves[sourceIndex]));
                        if (hpFrequency != route.highPassCoefficientFrequency
                            || d.hpQ != route.highPassCoefficientQ)
                        {
                            route.highPassCoefficients = makeHighPass (sampleRate, hpFrequency, d.hpQ);
                            route.highPassCoefficientFrequency = hpFrequency;
                            route.highPassCoefficientQ = d.hpQ;
                        }
                        coefficients = &route.highPassCoefficients;
                    }
                    processStereoBiquad (busLeft, busRight,
                                         route.highPassLeft, route.highPassRight,
                                         *coefficients);
                };
                processSourceHighPass (routedLayer1Left, routedLayer1Right,
                                       v.routedFilters[0], 0,
                                       ! p.filterLayerRouting || p.highPassLayer1);
                processSourceHighPass (routedLayer2Left, routedLayer2Right,
                                       v.routedFilters[1], 1,
                                       ! p.filterLayerRouting || p.highPassLayer2);
                processSourceHighPass (voiceLeft, voiceRight,
                                       v.routedFilters[2], 2,
                                       ! p.filterLayerRouting || p.highPassNoise);
            }
            // Velocity Volume is now source-specific. Apply the same historical
            // curve independently before the three sources are summed, so equal
            // L1/L2/Noise values reproduce the old global behaviour exactly.
            const auto velocityGain = [&] (std::size_t source)
            {
                const auto amount = p.velocityVolume[source];
                return amount > epsilon
                    ? std::max (0.0f, 1.0f - amount + v.velocitySmoothed * amount)
                    : 1.0f;
            };
            const auto velocityL1 = velocityGain (0);
            const auto velocityL2 = velocityGain (1);
            const auto velocityNoise = velocityGain (2);
            routedLayer1Left *= velocityL1 * larpVolume[0];
            routedLayer1Right *= velocityL1 * larpVolume[0];
            routedLayer2Left *= velocityL2 * larpVolume[1];
            routedLayer2Right *= velocityL2 * larpVolume[1];
            voiceLeft *= velocityNoise * larpVolume[2];
            voiceRight *= velocityNoise * larpVolume[2];

            // Preserve the three fully processed source buses until final voice
            // pan. The ordinary dry voice remains their exact sum.
            const auto sourceNoiseLeft = voiceLeft;
            const auto sourceNoiseRight = voiceRight;
            voiceLeft += routedLayer1Left + routedLayer2Left;
            voiceRight += routedLayer1Right + routedLayer2Right;

            if (sourceFxRoutingActive)
            {
                routedNoiseLeft = sourceNoiseLeft;
                routedNoiseRight = sourceNoiseRight;
            }
        }

        const auto postGain = v.keyFollowVolumeGain;
        voiceLeft *= postGain;
        voiceRight *= postGain;

        auto finalPan = 0.0f;
        if (! d.centredFinalPan)
        {
            const auto panEnvelope = envelope1 * (1.0f - p.panAdsr2Blend)
                                   + envelope2 * p.panAdsr2Blend;
            finalPan += panEnvelope * p.panEnvelope;
            finalPan += v.ping * p.pingPongPan;
            finalPan += juce::jlimit (-1.0f, 1.0f, (v.note - 60) / 24.0f) * p.noteScalePan;
            finalPan += pitchArpPan;
            // In the JSFX this final alternating pan is polyphonic only. In mono,
            // slider136 belongs exclusively to the individual unison clones.
            if (p.voiceCount > 1)
                finalPan += (voiceIndex & 1) != 0 ? p.voicePanAlternate
                                                 : -p.voicePanAlternate;
        }

        const auto finalPanLeft = d.centredFinalPan ? d.centrePanGain
                                                    : panGain (finalPan, true);
        const auto finalPanRight = d.centredFinalPan ? d.centrePanGain
                                                     : panGain (finalPan, false);
        left += voiceLeft * finalPanLeft;
        right += voiceRight * finalPanRight;

        if (sourceFxRoutingActive)
        {
            fxLayer1Left += routedLayer1Left * postGain * finalPanLeft;
            fxLayer1Right += routedLayer1Right * postGain * finalPanRight;
            fxLayer2Left += routedLayer2Left * postGain * finalPanLeft;
            fxLayer2Right += routedLayer2Right * postGain * finalPanRight;
            fxNoiseLeft += routedNoiseLeft * postGain * finalPanLeft;
            fxNoiseRight += routedNoiseRight * postGain * finalPanRight;
        }

        if ((! d.needsEnvelope1 || v.stage == Stage::idle)
            && (! d.needsEnvelope2 || v.stage2 == Stage::idle))
        {
            const auto registerFrequency = v.frequency;
            v = {};
            v.frequency = registerFrequency;
        }
    }
    globalPitchEnvelope1 = globalPitchEnvelope2 = pitchEnvelopeMaximum;

    const auto makeSend = [fxLayer1Left, fxLayer1Right,
                           fxLayer2Left, fxLayer2Right,
                           fxNoiseLeft, fxNoiseRight]
                          (const std::array<float, 3>& send)
    {
        return std::array<float, 2> {
            fxLayer1Left * send[0] + fxLayer2Left * send[1] + fxNoiseLeft * send[2],
            fxLayer1Right * send[0] + fxLayer2Right * send[1] + fxNoiseRight * send[2]
        };
    };

    // The three dry sources and Chorus/Delay wet returns remain separate up to
    // the Reverb insertion point. This lets the existing EQ and dynamics keep
    // their historical position while later exposing coherent stems.
    auto chorusBus = makeSend (p.chorusSend);
    const auto chorusDryLeft = chorusBus[0], chorusDryRight = chorusBus[1];
    processChorus (chorusBus[0], chorusBus[1], p);
    auto chorusWetLeft = chorusBus[0] - chorusDryLeft;
    auto chorusWetRight = chorusBus[1] - chorusDryRight;

    auto delayBus = makeSend (p.delaySend);
    const auto delayDryLeft = delayBus[0], delayDryRight = delayBus[1];
    processDelay (delayBus[0], delayBus[1], p, lfo1Global, lfo2Global);
    auto delayWetLeft = delayBus[0] - delayDryLeft;
    auto delayWetRight = delayBus[1] - delayDryRight;

    // Reverb keeps its historical raw send/input position and remains outside
    // the global EQ. Its own compressor is still internal to processReverb().
    const auto reverbBusInput = makeSend (p.reverbSend);

    std::array<std::array<float, 2>, preReverbStemCount> stems {{
        { fxLayer1Left, fxLayer1Right },
        { fxLayer2Left, fxLayer2Right },
        { fxNoiseLeft, fxNoiseRight },
        { chorusWetLeft, chorusWetRight },
        { delayWetLeft, delayWetRight }
    }};

    // Input gain, DC blocking and the global EQ are linear. Running identical
    // coefficients with independent state per stem makes their sum exactly the
    // same signal as the old stereo bus while retaining source identity.
    for (std::size_t index = 0; index < stems.size(); ++index)
    {
        auto& stem = stems[index];
        stem[0] *= d.inputGain;
        stem[1] *= d.inputGain;

        const auto inLeft = stem[0];
        stem[0] = inLeft - dcXLeft[index] + dcCoefficient * dcYLeft[index];
        dcXLeft[index] = inLeft;
        dcYLeft[index] = stem[0];
        const auto inRight = stem[1];
        stem[1] = inRight - dcXRight[index] + dcCoefficient * dcYRight[index];
        dcXRight[index] = inRight;
        dcYRight[index] = stem[1];

        processEqualizer (stem[0], stem[1], index);
    }

    const auto sumStems = [&stems]
    {
        std::array<float, 2> sum {};
        for (const auto& stem : stems)
        {
            sum[0] += stem[0];
            sum[1] += stem[1];
        }
        return sum;
    };

    const auto compressorEnabled = p.compressor.mix > epsilon;
    if (compressorEnabled && p.compressorPosition == 0)
    {
        const auto detectorBus = sumStems();
        const auto scale = compressorScale (
            p.compressor.sidechain ? sidechainLeft : detectorBus[0],
            p.compressor.sidechain ? sidechainRight : detectorBus[1],
            p.compressor, compressorState, p.compressor.ratio <= 4, false);
        for (auto& stem : stems)
        {
            stem[0] *= scale;
            stem[1] *= scale;
        }
    }
    else if (! compressorEnabled)
    {
        compressorState.runningAverage *= 0.9995f;
        compressorState.runningDb *= 0.9995f;
    }

    // Reverb remains outside the global EQ. The final glue/limiter/master
    // stage is intentionally deferred until after Reverb and the optional
    // Post-Reverb Main Compressor, so all logical outputs share one linked
    // final stage exactly like LR-608.

    // Reverb wet is generated after the global EQ. The Reverb Compressor
    // remains wet-only inside processReverb().
    auto reverbBus = reverbBusInput;
    const auto reverbDryLeft = reverbBus[0], reverbDryRight = reverbBus[1];
    processReverb (reverbBus[0], reverbBus[1], p, sidechainLeft, sidechainRight);
    auto reverbWetLeft = reverbBus[0] - reverbDryLeft;
    auto reverbWetRight = reverbBus[1] - reverbDryRight;

    // Main compressor Post Reverb remains a single linked processor. Its
    // detector hears the same full LJuno mix as before; one scale is then
    // applied to every logical component before routing.
    auto directInputLeft = dryInputLeft;
    auto directInputRight = dryInputRight;
    if (compressorEnabled && p.compressorPosition != 0)
    {
        auto detectorBus = sumStems();
        detectorBus[0] += reverbWetLeft + directInputLeft;
        detectorBus[1] += reverbWetRight + directInputRight;
        const auto scale = compressorScale (
            p.compressor.sidechain ? sidechainLeft : detectorBus[0],
            p.compressor.sidechain ? sidechainRight : detectorBus[1],
            p.compressor, compressorState, p.compressor.ratio <= 4, true);
        for (auto& stem : stems)
        {
            stem[0] *= scale;
            stem[1] *= scale;
        }
        reverbWetLeft *= scale;
        reverbWetRight *= scale;
        directInputLeft *= scale;
        directInputRight *= scale;
    }

    std::array<std::array<float, 2>, 4> logical {{
        { stems[0][0], stems[0][1] },
        { stems[1][0], stems[1][1] },
        { stems[2][0], stems[2][1] },
        { stems[3][0] + stems[4][0] + reverbWetLeft,
          stems[3][1] + stems[4][1] + reverbWetRight }
    }};

    // Final output stage: one global detector/gain, four independent logical
    // stems. Aux copies are made only after this stage, so routing the same
    // source to another pair can never change Glue or limiter behaviour.
    // The optional stereo audio input keeps its historical bypass of the synth
    // finaliser; it is added to Master after this stage (but still participates
    // in Main Compressor Post when that mode is selected).
    auto finalSum = std::array<float, 2> { 0.0f, 0.0f };
    for (const auto& stem : logical)
    {
        finalSum[0] += stem[0];
        finalSum[1] += stem[1];
    }

    const auto glueInput = std::sqrt (std::max (finalSum[0] * finalSum[0],
                                                finalSum[1] * finalSum[1])
                                      + 0.000000000001f);
    const auto glueCoefficient = glueEnvelope < glueInput
        ? 1.0f / std::max (1.0f, static_cast<float> (0.001 * sampleRate))
        : 1.0f / std::max (1.0f, static_cast<float> (0.004 * sampleRate));
    glueEnvelope += (glueInput - glueEnvelope) * glueCoefficient;
    auto glueGain = 1.0f;
    constexpr auto glueThreshold = 0.055f;
    if (glueEnvelope > glueThreshold)
        glueGain = (glueThreshold + (glueEnvelope - glueThreshold) * 0.5f) / glueEnvelope;

    for (auto& stem : logical)
    {
        stem[0] *= glueGain * 0.4f;
        stem[1] *= glueGain * 0.4f;
    }

    finalSum = { 0.0f, 0.0f };
    for (const auto& stem : logical)
    {
        finalSum[0] += stem[0];
        finalSum[1] += stem[1];
    }

    const auto limiterSample = [] (float sample)
    {
        const auto magnitude = std::abs (sample);
        if (magnitude > 0.98f)
            sample = std::copysign (0.98f + (magnitude - 0.98f) * 0.15f, sample);
        return sample;
    };
    const auto limitedLeft = limiterSample (finalSum[0]);
    const auto limitedRight = limiterSample (finalSum[1]);
    const auto limiterGainLeft = std::abs (finalSum[0]) > 1.0e-20f
        ? limitedLeft / finalSum[0] : 1.0f;
    const auto limiterGainRight = std::abs (finalSum[1]) > 1.0e-20f
        ? limitedRight / finalSum[1] : 1.0f;
    const auto finalGain = d.masterGain * 2.0f;
    for (auto& stem : logical)
    {
        stem[0] *= limiterGainLeft * finalGain;
        stem[1] *= limiterGainRight * finalGain;
    }

    const auto layer1Left = logical[0][0], layer1Right = logical[0][1];
    const auto layer2Left = logical[1][0], layer2Right = logical[1][1];
    const auto noiseLeft = logical[2][0], noiseRight = logical[2][1];
    const auto fxLeft = logical[3][0], fxRight = logical[3][1];

    // 1-2 is an always-present complete Master. Aux pairs are optional copies
    // of the four logical stems and can freely share the same destination.
    left = layer1Left + layer2Left + noiseLeft + fxLeft + directInputLeft;
    right = layer1Right + layer2Right + noiseRight + fxRight + directInputRight;
    aux.fill (0.0f);
    const auto routeAux = [&aux] (int destination, float sourceLeft, float sourceRight)
    {
        if (destination < 1 || destination > 4)
            return;
        const auto offset = static_cast<std::size_t> ((destination - 1) * 2);
        aux[offset] += sourceLeft;
        aux[offset + 1] += sourceRight;
    };
    routeAux (p.auxOutput[0], layer1Left, layer1Right);
    routeAux (p.auxOutput[1], layer2Left, layer2Right);
    routeAux (p.auxOutput[2], noiseLeft, noiseRight);
    routeAux (p.auxOutput[3], fxLeft, fxRight);
}

void SynthEngine::updateEffectCoefficients (const Params& p)
{
    constexpr auto q = 0.707f;
    for (std::size_t band = 0; band < 4; ++band)
    {
        if (! effectCoefficientsReady
            || p.eqFrequency[band] != cachedEqFrequency[band]
            || p.eqGain[band] != cachedEqGain[band])
        {
            eqCoefficients[band] = makePeak (sampleRate, p.eqFrequency[band],
                                             p.eqGain[band], q);
            cachedEqFrequency[band] = p.eqFrequency[band];
            cachedEqGain[band] = p.eqGain[band];
        }
        eqEnabled[band] = std::abs (p.eqGain[band]) > epsilon;
    }
    if (! effectCoefficientsReady
        || p.eqFrequency[4] != cachedEqFrequency[4]
        || p.eqGain[4] != cachedEqGain[4])
    {
        eqCoefficients[4] = makeHighShelf (sampleRate, p.eqFrequency[4], p.eqGain[4], q);
        eqCoefficients[5] = eqCoefficients[4];
        cachedEqFrequency[4] = p.eqFrequency[4];
        cachedEqGain[4] = p.eqGain[4];
    }
    eqEnabled[4] = std::abs (p.eqGain[4]) > epsilon;

    if (! effectCoefficientsReady || p.delayTone != cachedDelayTone)
    {
        delayLpCoefficient = static_cast<float> (std::exp (
            -juce::MathConstants<double>::twoPi * (600.0 + p.delayTone * 7000.0) / sampleRate));
        delayHpCoefficient = static_cast<float> (std::exp (
            -juce::MathConstants<double>::twoPi * (40.0 + p.delayTone * 900.0) / sampleRate));
        cachedDelayTone = p.delayTone;
    }

    updateLwsReverbCoefficients (p);

    const auto reverbCoefficientsChanged = ! effectCoefficientsReady
        || p.reverbDecaySeconds != cachedReverbDecay
        || p.reverbBassMultiplier != cachedReverbBassMultiplier
        || p.reverbXoverHz != cachedReverbXover
        || p.reverbDampingHz != cachedReverbDamping;
    if (! reverbCoefficientsChanged)
    {
        effectCoefficientsReady = true;
        return;
    }

    static constexpr std::array<double, 8> delayTimes {
        0.153129, 0.210389, 0.127837, 0.256891,
        0.174713, 0.192303, 0.125000, 0.219991
    };
    const auto midTime = std::max (0.20, static_cast<double> (p.reverbDecaySeconds));
    const auto lowTime = std::max (0.20, static_cast<double> (
        p.reverbBassMultiplier * p.reverbDecaySeconds));
    const auto lowCoefficient = juce::MathConstants<double>::twoPi
                              * juce::jlimit (40.0, 4000.0,
                                              static_cast<double> (p.reverbXoverHz)) / sampleRate;
    const auto damping = juce::jlimit (500.0, sampleRate * 0.45,
                                       static_cast<double> (p.reverbDampingHz));
    const auto dampingChi = damping > 0.49 * sampleRate
        ? 2.0
        : 1.0 - std::cos (juce::MathConstants<double>::twoPi * damping / sampleRate);
    for (std::size_t line = 0; line < reverbLines.size(); ++line)
    {
        auto& reverbLine = reverbLines[line];
        const auto middleGain = std::pow (0.001, delayTimes[line] / midTime);
        const auto lowGain = std::pow (0.001, delayTimes[line] / lowTime)
                           / middleGain - 1.0;
        const auto highTarget = std::pow (0.001, delayTimes[line] / (0.5 * midTime))
                              / middleGain;
        const auto targetSquared = std::max (0.000000000001, highTarget * highTarget);
        const auto term = (1.0 - targetSquared)
                        / (2.0 * targetSquared * std::max (0.000000000001, dampingChi));
        const auto highCoefficient = std::abs (term) > 0.000000000001
            ? (std::sqrt (std::max (0.0, 1.0 + 4.0 * term)) - 1.0) / (2.0 * term)
            : 1.0;
        reverbLine.midGain = static_cast<float> (middleGain);
        reverbLine.lowGain = static_cast<float> (lowGain);
        reverbLine.lowCoefficient = static_cast<float> (lowCoefficient);
        reverbLine.highCoefficient = static_cast<float> (highCoefficient);
    }
    cachedReverbDecay = p.reverbDecaySeconds;
    cachedReverbBassMultiplier = p.reverbBassMultiplier;
    cachedReverbXover = p.reverbXoverHz;
    cachedReverbDamping = p.reverbDampingHz;
    effectCoefficientsReady = true;
}

void SynthEngine::processChorus (float& left, float& right, const Params& p)
{
    if (chorusBufferLeft.empty())
        return;

    const auto inputMagnitude = std::max (std::abs (left), std::abs (right));
    const auto shouldRun = p.chorusLevel > 0.000001f
                        && (inputMagnitude > epsilon || chorusTail > 0.00002f);
    if (! shouldRun)
    {
        chorusTail *= 0.9995f;
        if (chorusTail < 0.00000001f)
        {
            chorusTail = 0.0f;
            chorusHpXLeft = chorusHpYLeft = chorusHpXRight = chorusHpYRight = 0.0f;
        }
        return;
    }

    chorusRateSmoothed += (p.chorusRate - chorusRateSmoothed) * 0.0005f;
    const auto rateHz = 0.10 + chorusRateSmoothed * 1.40;
    chorusPhase = wrapPhase (chorusPhase + rateHz / sampleRate);
    const auto lfoLeft = std::sin (juce::MathConstants<double>::twoPi * chorusPhase);
    const auto lfoRight = std::sin (juce::MathConstants<double>::twoPi
                                    * wrapPhase (chorusPhase + 0.25));
    constexpr auto minimumDelay = 0.006;
    constexpr auto maximumDelay = 0.018;
    constexpr auto depth = 0.65;
    const auto delayLeft = juce::jlimit (0.001, 0.03,
        minimumDelay + (maximumDelay - minimumDelay) * (0.5 + 0.5 * lfoLeft * depth));
    const auto delayRight = juce::jlimit (0.001, 0.03,
        minimumDelay + (maximumDelay - minimumDelay) * (0.5 + 0.5 * lfoRight * depth));

    const auto readFractional = [] (const std::vector<float>& buffer, int writePosition,
                                    double delaySamples)
    {
        auto readPosition = static_cast<double> (writePosition) - delaySamples;
        while (readPosition < 0.0)
            readPosition += static_cast<double> (buffer.size());
        const auto first = static_cast<std::size_t> (std::floor (readPosition));
        const auto second = (first + 1) % buffer.size();
        const auto fraction = static_cast<float> (readPosition - std::floor (readPosition));
        return buffer[first] + (buffer[second] - buffer[first]) * fraction;
    };

    auto wetLeft = readFractional (chorusBufferLeft, chorusWritePosition,
                                   delayLeft * sampleRate);
    auto wetRight = readFractional (chorusBufferRight, chorusWritePosition,
                                    delayRight * sampleRate);
    chorusBufferLeft[static_cast<std::size_t> (chorusWritePosition)] = left;
    chorusBufferRight[static_cast<std::size_t> (chorusWritePosition)] = right;
    chorusWritePosition = (chorusWritePosition + 1)
                         % static_cast<int> (chorusBufferLeft.size());

    const auto hpInputLeft = wetLeft;
    wetLeft = hpInputLeft - chorusHpXLeft + chorusHpCoefficient * chorusHpYLeft;
    chorusHpXLeft = hpInputLeft;
    chorusHpYLeft = wetLeft;
    const auto hpInputRight = wetRight;
    wetRight = hpInputRight - chorusHpXRight + chorusHpCoefficient * chorusHpYRight;
    chorusHpXRight = hpInputRight;
    chorusHpYRight = wetRight;

    const auto wetGain = p.chorusLevel * (0.5f + 0.5f * p.chorusWidth);
    wetLeft *= wetGain;
    wetRight *= wetGain;
    left += wetLeft;
    right += wetRight;
    const auto currentTail = std::max (inputMagnitude,
                                       std::max (std::abs (wetLeft), std::abs (wetRight)));
    chorusTail = currentTail > chorusTail ? currentTail : chorusTail * 0.9995f;
}


void SynthEngine::resetDelayProcessors()
{
    std::fill (delayBufferLeft.begin(), delayBufferLeft.end(), 0.0f);
    std::fill (delayBufferRight.begin(), delayBufferRight.end(), 0.0f);
    delayWritePosition = 0;
    delayTimeCurrent = 0.0f;
    delayLpLeft = delayLpRight = 0.0f;
    delayHpXLeft = delayHpYLeft = delayHpXRight = delayHpYRight = 0.0f;

    lwsDelayWritePosition = 0;
    lwsDelayMixActive = false;
    lwsDelayTime1 = lwsDelayTime2 = 0.0f;
    lwsDelayWetLp1 = lwsDelayWetLp2 = 0.0f;
    lwsDelayWetHpX1 = lwsDelayWetHpY1 = lwsDelayWetHpX2 = lwsDelayWetHpY2 = 0.0f;
    lwsDelayFbLp1 = lwsDelayFbLp2 = 0.0f;
    lwsDelayFbHpX1 = lwsDelayFbHpY1 = lwsDelayFbHpX2 = lwsDelayFbHpY2 = 0.0f;
    lwsDelayPan1L = 0.9807852804f; lwsDelayPan1R = 0.1950903220f;
    lwsDelayPan2L = 0.1950903220f; lwsDelayPan2R = 0.9807852804f;
    delaySilentSamples = delayBufferLeft.size();
}

void SynthEngine::processDelay (float& left, float& right, const Params& p,
                                float lfo1, float lfo2)
{
    if (p.delayMode != activeDelayMode)
    {
        resetDelayProcessors();
        activeDelayMode = p.delayMode;
    }

    if (p.delayMode == 1)
        processDelay1 (left, right, p, lfo1, lfo2);
    else if (p.delayMode == 2)
        processLwsDelay (left, right, p, lfo1, lfo2);
}

void SynthEngine::processLwsDelay (float& left, float& right, const Params& p,
                                   float lfo1, float lfo2)
{
    if (delayBufferLeft.empty() || delayBufferRight.empty())
        return;

    // Delay 2 now runs as a unity wet engine behind the L1/L2/Noise send
    // matrix. The outer Delay Type selector remains the on/off state.
    const auto mix = p.delayOn ? 1.0f : 0.0f;
    if (mix <= epsilon)
    {
        if (lwsDelayMixActive)
            resetDelayProcessors();
        lwsDelayMixActive = false;
        return;
    }

    if (! lwsDelayMixActive)
    {
        resetDelayProcessors();
        lwsDelayMixActive = true;
    }

    // Delay 2 owns its Time/Sync/LFO clock independently from Delay 1.
    // The two LWS speed controls are independent ratios around that
    // base, so 1.0 / 1.5 recreates the original dual-tape relationship while
    // still allowing completely different tape timings.
    static constexpr std::array<int, 11> divisions { 1, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1 };
    const auto syncIndex = juce::jlimit (0, 10, p.delay2Sync);
    auto baseSeconds = syncIndex > 0
        ? (60.0 / std::max (1.0, p.tempoBpm)) * (4.0 / divisions[static_cast<std::size_t> (syncIndex)])
        : static_cast<double> (p.delay2Time * 1.8f + 0.02f);

    const auto timeModulation = lfo1 * p.delay2Lfo1 + lfo2 * p.delay2Lfo2;
    if (std::abs (timeModulation) > 0.000001f)
        baseSeconds *= std::exp2 (static_cast<double> (timeModulation) * 0.5);

    const auto maximumDelaySamples = static_cast<double> (delayBufferLeft.size() - 4);
    const auto target1 = juce::jlimit (16.0, maximumDelaySamples,
        baseSeconds * static_cast<double> (p.delay2Speed1) * sampleRate);
    const auto target2 = juce::jlimit (16.0, maximumDelaySamples,
        baseSeconds * static_cast<double> (p.delay2Speed2) * sampleRate);

    const auto glideSeconds = std::max (0.001, static_cast<double> (p.delay2GlideMs) * 0.001);
    const auto glideCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / std::max (1.0, glideSeconds * sampleRate)));
    if (lwsDelayTime1 <= 0.0f) lwsDelayTime1 = static_cast<float> (target1);
    if (lwsDelayTime2 <= 0.0f) lwsDelayTime2 = static_cast<float> (target2);
    lwsDelayTime1 += (static_cast<float> (target1) - lwsDelayTime1) * glideCoefficient;
    lwsDelayTime2 += (static_cast<float> (target2) - lwsDelayTime2) * glideCoefficient;

    const auto readLinear = [] (const std::vector<float>& buffer, int writePosition, float delaySamples)
    {
        auto position = static_cast<double> (writePosition) - delaySamples;
        const auto length = static_cast<double> (buffer.size());
        while (position < 0.0) position += length;
        while (position >= length) position -= length;
        const auto first = static_cast<std::size_t> (std::floor (position));
        const auto second = (first + 1) % buffer.size();
        const auto fraction = static_cast<float> (position - std::floor (position));
        return buffer[first] + (buffer[second] - buffer[first]) * fraction;
    };

    const auto tape1Raw = readLinear (delayBufferLeft, lwsDelayWritePosition, lwsDelayTime1);
    const auto tape2Raw = readLinear (delayBufferRight, lwsDelayWritePosition, lwsDelayTime2);

    // Delay 2 Tone L/R mirror the musical behaviour of Delay 1 Tone.
    // 0.50 is an exact neutral point. Below it, only the feedback path is
    // progressively low-passed; above it, only the feedback path is
    // progressively high-passed. Because the filtered signal is written back
    // into the delay line, each successive repeat follows the tone curve more
    // strongly, while the first repeat remains essentially uncoloured.
    const auto toneProcess = [this] (float input, float tone,
                                     float& lowState, float& highX, float& highY)
    {
        tone = juce::jlimit (0.0f, 2.0f, tone);
        if (tone < 0.4999f)
        {
            const auto normalised = juce::jlimit (0.0f, 1.0f, tone / 0.5f);
            const auto cutoffHz = 600.0 + static_cast<double> (normalised) * 7000.0;
            const auto coefficient = static_cast<float> (
                1.0 - std::exp (-juce::MathConstants<double>::twoPi * cutoffHz / sampleRate));
            lowState += (input - lowState) * coefficient;
            highX = lowState;
            highY = lowState;
            return lowState;
        }

        if (tone > 0.5001f)
        {
            const auto normalised = juce::jlimit (0.0f, 1.0f, (tone - 0.5f) / 1.5f);
            const auto cutoffHz = 40.0 + static_cast<double> (normalised) * 1800.0;
            const auto coefficient = static_cast<float> (std::exp (
                -juce::MathConstants<double>::twoPi * cutoffHz / sampleRate));
            const auto output = input - highX + coefficient * highY;
            highX = input;
            highY = output;
            lowState = input;
            return output;
        }

        // Exact centre: no LP or HP coloration. Prime both filter memories so
        // moving away from the centre does not produce a large discontinuity.
        lowState = input;
        highX = input;
        highY = input;
        return input;
    };

    // The audible repeat is not tone-filtered directly. This matches Delay 1:
    // coloration accumulates through the feedback loop on each repeat.
    constexpr auto dcBlock = 0.995f;
    const auto wetDc1 = tape1Raw - lwsDelayWetHpX1 + dcBlock * lwsDelayWetHpY1;
    const auto wetDc2 = tape2Raw - lwsDelayWetHpX2 + dcBlock * lwsDelayWetHpY2;
    lwsDelayWetHpX1 = tape1Raw; lwsDelayWetHpY1 = wetDc1;
    lwsDelayWetHpX2 = tape2Raw; lwsDelayWetHpY2 = wetDc2;

    // Exact LWS-7 shared Drive law. Drive colours only the audible repeat;
    // feedback remains filtered and clean.
    const auto driveAmount = juce::jlimit (0.0f, 1.0f, p.delay2TapeDrive * 0.01f);
    const auto driveSquared = driveAmount * driveAmount;
    const auto driveGain = 1.0f + 79.0f * driveSquared;
    const auto driveBias = 0.185f * driveSquared;
    const auto driveSoftMix = 1.0f - driveSquared;
    const auto driveHardMix = driveSquared;
    const auto zeroSoft = driveBias / (1.0f + std::abs (driveBias));
    const auto zeroHard = juce::jlimit (-1.0f, 1.0f, driveBias);
    const auto driveZero = zeroSoft * driveSoftMix + zeroHard * driveHardMix;

    const auto tapeDistort = [=] (float x)
    {
        auto u = juce::jlimit (-16.0f, 16.0f, x * driveGain + driveBias);
        const auto au = std::abs (u);
        const auto soft = u >= 0.0f ? u / (1.0f + au)
                                    : 0.78f * u / (0.78f + au);
        const auto hard = juce::jlimit (-1.0f, 1.0f, u);
        const auto shaped = soft * driveSoftMix + hard * driveHardMix - driveZero;
        return x + (shaped - x) * driveAmount;
    };

    const auto tape1Wet = tapeDistort (wetDc1);
    const auto tape2Wet = tapeDistort (wetDc2);

    // Tone L/R act only in the feedback path, exactly so the tonal curve
    // compounds from repeat to repeat. Drive remains outside the feedback loop.
    const auto toneFeedback1 = toneProcess (tape1Raw, p.delay2ToneLeft,
                                            lwsDelayFbLp1, lwsDelayFbHpX1, lwsDelayFbHpY1);
    const auto toneFeedback2 = toneProcess (tape2Raw, p.delay2ToneRight,
                                            lwsDelayFbLp2, lwsDelayFbHpX2, lwsDelayFbHpY2);
    const auto feedbackDc1 = toneFeedback1;
    const auto feedbackDc2 = toneFeedback2;

    // Each tape has its own 0..2 feedback control. 1.0 maps to about 0.5 loop
    // gain for a soft Delay-1-like decay; 2.0 reaches ~0.9998 for a sustained
    // loop. The feedback sample remains bounded so the DSP cannot diverge.
    const auto feedback1 = juce::jlimit (0.0f, 2.0f, p.delay2Feedback1) * 0.4999f;
    const auto feedback2 = juce::jlimit (0.0f, 2.0f, p.delay2Feedback2) * 0.4999f;
    const auto feedbackSample1 = juce::jlimit (-2.0f, 2.0f, feedbackDc1 * feedback1);
    const auto feedbackSample2 = juce::jlimit (-2.0f, 2.0f, feedbackDc2 * feedback2);
    const auto monoInput = 0.5f * (left + right);
    delayBufferLeft[static_cast<std::size_t> (lwsDelayWritePosition)]
        = juce::jlimit (-4.0f, 4.0f, monoInput + feedbackSample1);
    delayBufferRight[static_cast<std::size_t> (lwsDelayWritePosition)]
        = juce::jlimit (-4.0f, 4.0f, monoInput + feedbackSample2);

    lwsDelayWritePosition = (lwsDelayWritePosition + 1)
                          % static_cast<int> (delayBufferLeft.size());

    // Delay 2 has its own Stereo/Mono mode; otherwise its dedicated spread is used.
    const auto spread = p.delay2Mono ? 0.0f
                                    : juce::jlimit (-1.0f, 1.0f, p.delay2StereoSpread);
    const auto pan1Angle = (-spread + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    const auto pan2Angle = ( spread + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    const auto target1L = std::cos (pan1Angle), target1R = std::sin (pan1Angle);
    const auto target2L = std::cos (pan2Angle), target2R = std::sin (pan2Angle);
    const auto panCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / std::max (1.0, 0.005 * sampleRate)));
    lwsDelayPan1L += (target1L - lwsDelayPan1L) * panCoefficient;
    lwsDelayPan1R += (target1R - lwsDelayPan1R) * panCoefficient;
    lwsDelayPan2L += (target2L - lwsDelayPan2L) * panCoefficient;
    lwsDelayPan2R += (target2R - lwsDelayPan2R) * panCoefficient;

    constexpr auto equalTapeGain = 0.353553390593274f;
    const auto wetLeft = (tape1Wet * lwsDelayPan1L + tape2Wet * lwsDelayPan2L) * equalTapeGain;
    const auto wetRight = (tape1Wet * lwsDelayPan1R + tape2Wet * lwsDelayPan2R) * equalTapeGain;

    // Send architecture: preserve the input bus and add only the wet return.
    // The source send level is applied before this engine, so Mix now remains
    // unity internally and does not attenuate the dry synth path.
    left += wetLeft * mix;
    right += wetRight * mix;

    const auto activity = std::max ({ std::abs (monoInput), std::abs (tape1Raw),
                                     std::abs (tape2Raw), std::abs (feedbackDc1),
                                     std::abs (feedbackDc2) });
    if (activity > 0.000000001f)
        delaySilentSamples = 0;
    else if (delaySilentSamples < delayBufferLeft.size())
        ++delaySilentSamples;
}

void SynthEngine::processDelay1 (float& left, float& right, const Params& p,
                                 float lfo1, float lfo2)
{
    if (delayBufferLeft.empty())
        return;

    static constexpr std::array<int, 11> divisions { 1, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1 };
    const auto inputLeft = left;
    const auto inputRight = right;
    const auto syncIndex = juce::jlimit (0, 10, p.delaySync);
    auto targetSeconds = syncIndex > 0
        ? (60.0 / p.tempoBpm) * (4.0 / divisions[static_cast<std::size_t> (syncIndex)])
        : static_cast<double> (p.delayTime * 1.8f + 0.02f);
    const auto timeModulation = lfo1 * p.delayLfo1 + lfo2 * p.delayLfo2;
    if (std::abs (timeModulation) > 0.000001f)
        targetSeconds *= std::exp2 (static_cast<double> (timeModulation) * 0.5);

    const auto maximumDelaySamples = static_cast<double> (delayBufferLeft.size() - 2);
    targetSeconds = juce::jlimit (0.0, maximumDelaySamples / sampleRate, targetSeconds);

    // A disabled, fully flushed tape line has no audio work to do. Keeping its
    // head time at the target makes a later enable start from the same settled
    // state as the JSFX block compiler.
    if (! p.delayOn && delaySilentSamples >= delayBufferLeft.size()
        && std::abs (delayLpLeft) <= epsilon && std::abs (delayLpRight) <= epsilon
        && std::abs (delayHpYLeft) <= epsilon && std::abs (delayHpYRight) <= epsilon)
    {
        delayTimeCurrent = static_cast<float> (targetSeconds);
        return;
    }

    delayTimeCurrent += (static_cast<float> (targetSeconds) - delayTimeCurrent) * 0.0005f;
    const auto delaySamples = juce::jlimit (0.0, maximumDelaySamples,
                                            static_cast<double> (delayTimeCurrent) * sampleRate);
    auto readPosition = static_cast<double> (delayWritePosition) - delaySamples;
    while (readPosition < 0.0)
        readPosition += static_cast<double> (delayBufferLeft.size());
    const auto first = static_cast<std::size_t> (std::floor (readPosition));
    const auto second = (first + 1) % delayBufferLeft.size();
    const auto fraction = static_cast<float> (readPosition - std::floor (readPosition));
    const auto delayedLeft = delayBufferLeft[first]
                           + (delayBufferLeft[second] - delayBufferLeft[first]) * fraction;
    const auto delayedRight = delayBufferRight[first]
                            + (delayBufferRight[second] - delayBufferRight[first]) * fraction;

    delayLpLeft += (delayedLeft - delayLpLeft) * (1.0f - delayLpCoefficient);
    delayLpRight += (delayedRight - delayLpRight) * (1.0f - delayLpCoefficient);
    const auto hpInputLeft = delayLpLeft;
    auto feedbackLeft = hpInputLeft - delayHpXLeft + delayHpCoefficient * delayHpYLeft;
    delayHpXLeft = hpInputLeft;
    delayHpYLeft = feedbackLeft;
    const auto hpInputRight = delayLpRight;
    auto feedbackRight = hpInputRight - delayHpXRight + delayHpCoefficient * delayHpYRight;
    delayHpXRight = hpInputRight;
    delayHpYRight = feedbackRight;

    const auto feedback = p.delayOn ? p.delayFeedback : 0.0f;
    feedbackLeft = juce::jlimit (-2.0f, 2.0f, feedbackLeft * feedback * 0.9998f);
    feedbackRight = juce::jlimit (-2.0f, 2.0f, feedbackRight * feedback * 0.9998f);
    const auto monoFeedback = 0.5f * (feedbackLeft + feedbackRight);
    delayBufferLeft[static_cast<std::size_t> (delayWritePosition)] = p.delayOn
        ? left + (p.delayMono ? monoFeedback : feedbackLeft)
        : feedbackLeft;
    delayBufferRight[static_cast<std::size_t> (delayWritePosition)] = p.delayOn
        ? right + (p.delayMono ? monoFeedback : feedbackRight)
        : feedbackRight;
    delayWritePosition = (delayWritePosition + 1)
                       % static_cast<int> (delayBufferLeft.size());

    const auto mix = p.delayOn ? p.delayMix : 0.0f;
    left += delayedLeft * mix;
    right += delayedRight * mix;

    const auto injectedMagnitude = p.delayOn
        ? std::max (std::abs (inputLeft), std::abs (inputRight)) : 0.0f;
    const auto activity = std::max ({ injectedMagnitude,
                                      std::abs (delayedLeft), std::abs (delayedRight),
                                      std::abs (delayLpLeft), std::abs (delayLpRight),
                                      std::abs (delayHpYLeft), std::abs (delayHpYRight) });
    if (activity > 0.00002f)
        delaySilentSamples = 0;
    else if (delaySilentSamples < delayBufferLeft.size())
        ++delaySilentSamples;
}

void SynthEngine::processEqualizer (float& left, float& right, std::size_t stemIndex)
{
    stemIndex = std::min (stemIndex, preReverbStemCount - 1);
    auto& statesLeft = eqStemLeft[stemIndex];
    auto& statesRight = eqStemRight[stemIndex];
    for (std::size_t band = 0; band < 4; ++band)
    {
        if (! eqEnabled[band])
            continue;
        left = processBiquad (left, statesLeft[band], eqCoefficients[band]);
        right = processBiquad (right, statesRight[band], eqCoefficients[band]);
    }
    if (eqEnabled[4])
    {
        left = processBiquad (left, statesLeft[4], eqCoefficients[4]);
        right = processBiquad (right, statesRight[4], eqCoefficients[4]);
        left = processBiquad (left, statesLeft[5], eqCoefficients[5]);
        right = processBiquad (right, statesRight[5], eqCoefficients[5]);
    }
}

float SynthEngine::compressorScale (float detectorLeft, float detectorRight,
                                     const CompressorParameters& parameters,
                                     CompressorState& state, bool legacyCurve,
                                     bool halveCompressedSignal)
{
    constexpr auto dbToLog = 0.1151292546497023;
    constexpr auto logToDb = 8.685889638065037;
    auto ratioPosition = juce::jlimit (0, 9, parameters.ratio);
    if (ratioPosition > 4)
        ratioPosition -= 5;
    static constexpr std::array<float, 5> ratios { 4.0f, 8.0f, 12.0f, 20.0f, 20.0f };
    const auto ratio = ratios[static_cast<std::size_t> (ratioPosition)];
    const auto curveScale = logToDb * (legacyCurve ? 2.08136898 : 1.0);
    const auto thresholdLog = (static_cast<double> (parameters.thresholdDb) - 3.0) * dbToLog;
    const auto detector = std::max (std::abs (detectorLeft), std::abs (detectorRight));
    const auto detectorSquared = detector * detector;
    state.runningAverage = detectorSquared
                         + compressorRmsCoefficient * (state.runningAverage - detectorSquared);
    const auto overDb = static_cast<float> (std::max (0.0,
        curveScale * (0.5 * std::log (std::max (1.0e-18f, state.runningAverage))
                      - thresholdLog)));
    const auto attackSeconds = std::max (0.000001,
        static_cast<double> (parameters.attackMicroseconds) / 1000000.0);
    const auto releaseSeconds = std::max (0.000001,
        static_cast<double> (parameters.releaseMilliseconds) / 1000.0);
    const auto attackCoefficient = static_cast<float> (
        std::exp (-1.0 / (attackSeconds * sampleRate)));
    const auto releaseCoefficient = static_cast<float> (
        std::exp (-1.0 / (releaseSeconds * sampleRate)));
    const auto envelopeCoefficient = overDb > state.runningDb
                                   ? 1.0f - attackCoefficient
                                   : 1.0f - releaseCoefficient;
    state.runningDb += (overDb - state.runningDb) * envelopeCoefficient;

    const auto gainDb = state.runningDb * (1.0f - 1.0f / ratio);
    const auto gain = static_cast<float> (std::exp (-gainDb * dbToLog
        + static_cast<double> (parameters.makeupDb) * dbToLog));
    const auto compressedScale = halveCompressedSignal ? gain * 0.5f : gain;
    const auto mix = juce::jlimit (0.0f, 1.0f, parameters.mix);
    return 1.0f + (compressedScale - 1.0f) * mix;
}

void SynthEngine::processCompressor (float& left, float& right,
                                     float detectorLeft, float detectorRight,
                                     const CompressorParameters& parameters,
                                     CompressorState& state, bool legacyCurve,
                                     bool halveCompressedSignal)
{
    const auto scale = compressorScale (detectorLeft, detectorRight, parameters,
                                        state, legacyCurve, halveCompressedSignal);
    left *= scale;
    right *= scale;
}


void SynthEngine::resetLwsReverbState()
{
    for (auto& buffer : lwsReverbBuffers)
        std::fill (buffer.begin(), buffer.end(), 0.0f);
    lwsReverbIndex = 0;
    lwsReverbMixActive = false;
    lwsReverbFeedbackLp.fill (0.0f);
    lwsReverbSendLpL = lwsReverbSendLpR = 0.0f;
    lwsReverbSendLowL = lwsReverbSendLowR = 0.0f;
    lwsReverbBodyLpL = lwsReverbBodyLpR = 0.0f;
    lwsReverbBodyLowL = lwsReverbBodyLowR = 0.0f;
    lwsReverbMotionTargetA = lwsReverbMotionTargetB = 0.0f;
    lwsReverbMotionA = lwsReverbMotionB = 0.0f;
    lwsReverbMotionCountA = lwsReverbMotionCountB = 0;
    lwsReverbTailAge = 0;
}

void SynthEngine::resetReverbProcessors()
{
    std::fill (reverbPredelayLeft.begin(), reverbPredelayLeft.end(), 0.0f);
    std::fill (reverbPredelayRight.begin(), reverbPredelayRight.end(), 0.0f);
    reverbPredelayWriteLeft = reverbPredelayWriteRight = 0;
    for (auto& line : reverbLines)
    {
        std::fill (line.diffuser.begin(), line.diffuser.end(), 0.0f);
        std::fill (line.delay.begin(), line.delay.end(), 0.0f);
        line.diffuserPosition = line.delayPosition = 0;
        line.lowState = line.highState = 0.0f;
    }
    reverbWetSmoothed = reverbWidthSmoothed = 0.0f;
    reverbEarlyLevelSmoothed = reverbEarlyPanSmoothed = 0.0f;
    reverbEarlyRatioSmoothed = 1.0f;
    reverbOutputLeft = reverbOutputRight = reverbTail = 0.0f;
    reverbCompressorState = {};
    resetLwsReverbState();
}

void SynthEngine::processReverb (float& left, float& right, const Params& p,
                                 float sidechainLeft, float sidechainRight)
{
    if (p.reverbMode != activeReverbMode)
    {
        resetReverbProcessors();
        activeReverbMode = p.reverbMode;
    }

    if (p.reverbMode == 1)
        processReverb1 (left, right, p, sidechainLeft, sidechainRight);
    else if (p.reverbMode == 2)
        processLwsReverb (left, right, p);
}

void SynthEngine::updateLwsReverbCoefficients (const Params& p)
{
    if (sampleRate <= 0.0 || lwsReverbBuffers[0].empty())
        return;

    auto& c = lwsReverbCoefficients;
    const auto bufferLength = static_cast<int> (lwsReverbBuffers[0].size());

    // Reverb 2 reuses the remaining reverb sound controls contextually:
    // Predelay -> Distance, XOver -> Open Sky, Bass Multiplier -> Warmth,
    // Decay -> RT60, Damping -> Tail Tone, Width -> Width,
    // Predelay 2 Level -> Early Reflections, Pan magnitude -> Tail Motion,
    // and Ratio -> Body Volume. Wet amount is now owned by the source sends.
    c.mix = p.reverbOn ? 1.0f : 0.0f;
    c.rt60Seconds = std::max (0.001f, p.reverbDecaySeconds);
    const auto distance = juce::jlimit (0.0f, 3.0f, (p.reverbPredelayMs - 20.0f) / 20.0f);
    const auto openSky = juce::jlimit (0.0f, 1.0f, p.reverbXoverHz / 473.6842f);
    const auto warmth = juce::jlimit (0.0f, 1.0f, (p.reverbBassMultiplier - 0.5f) / 0.7f);
    const auto width = juce::jlimit (0.0f, 1.0f, p.reverbWidth);
    const auto early = juce::jlimit (0.0f, 1.0f, p.reverbEarlyLevel * 3.8f);
    const auto bodyVolume = juce::jlimit (0.0f, 3.0f, p.reverbEarlyRatio * 2.1f);
    const auto tailTone = juce::jlimit (0.0f, 1.0f, p.reverbDampingHz / 10000.0f);
    c.tailMotion = juce::jlimit (0.0f, 1.0f, std::abs (p.reverbEarlyPan));

    const auto earlyScale = 0.75 + 0.75 * distance;
    static constexpr std::array<double, 4> earlySeconds { 0.022, 0.041, 0.077, 0.132 };
    for (std::size_t i = 0; i < c.earlyTaps.size(); ++i)
        c.earlyTaps[i] = juce::jlimit (1, bufferLength - 2,
            static_cast<int> (std::floor (sampleRate * earlySeconds[i] * earlyScale)));

    const auto lateScale = 0.85 + 0.45 * distance;
    static constexpr std::array<double, 8> lateSeconds {
        0.0437, 0.0509, 0.0593, 0.0689, 0.0797, 0.0923, 0.1061, 0.1217
    };
    const auto rt60Ms = std::max (1.0, static_cast<double> (c.rt60Seconds) * 1000.0);
    const auto rt60Log = std::log (0.001) / rt60Ms;
    for (std::size_t i = 0; i < c.delays.size(); ++i)
    {
        const auto seconds = lateSeconds[i] * lateScale;
        c.delays[i] = juce::jlimit (1, bufferLength - 2,
            static_cast<int> (std::floor (sampleRate * seconds)));
        c.feedback[i] = static_cast<float> (
            std::exp (rt60Log * seconds * 1000.0));
    }

    const auto allpassScale = 0.85 + 0.30 * distance;
    static constexpr std::array<double, 4> allpassSeconds { 0.0049, 0.0067, 0.0109, 0.0137 };
    for (std::size_t i = 0; i < c.allpassDelays.size(); ++i)
        c.allpassDelays[i] = juce::jlimit (1, bufferLength - 2,
            static_cast<int> (std::floor (sampleRate * allpassSeconds[i] * allpassScale)));
    c.allpass1 = 0.66f - 0.18f * openSky;
    c.allpass2 = 0.54f - 0.14f * openSky;

    auto sendFc = 16000.0 * std::exp2 (-1.40 * warmth);
    auto dampingFc = 12000.0 * std::exp2 (-2.10 * warmth);
    if (tailTone < 0.5f)
    {
        auto dark = static_cast<double> ((0.5f - tailTone) * 2.0f);
        dark = dark * dark * (3.0 - 2.0 * dark);
        sendFc *= std::exp2 (-2.10 * dark);
        dampingFc *= std::exp2 (-3.20 * dark);
    }
    else
    {
        auto bright = static_cast<double> ((tailTone - 0.5f) * 2.0f);
        bright = bright * bright * (3.0 - 2.0 * bright);
        const auto sendTarget = std::min (sampleRate * 0.43, 19500.0);
        const auto dampingTarget = std::min (sampleRate * 0.40, 17500.0);
        sendFc += (sendTarget - sendFc) * bright;
        dampingFc += (dampingTarget - dampingFc) * bright;
    }
    sendFc = std::max (250.0, sendFc);
    dampingFc = std::max (180.0, dampingFc);

    c.sendLowPass = static_cast<float> (
        1.0 - std::exp (-juce::MathConstants<double>::twoPi
                        * std::min (sampleRate * 0.45, sendFc) / sampleRate));
    static constexpr std::array<double, 8> dampingMultipliers {
        0.88, 0.93, 0.97, 1.01, 1.04, 1.07, 1.10, 1.13
    };
    for (std::size_t i = 0; i < c.damping.size(); ++i)
        c.damping[i] = static_cast<float> (
            1.0 - std::exp (-juce::MathConstants<double>::twoPi
                            * std::min (sampleRate * 0.45,
                                        dampingFc * dampingMultipliers[i]) / sampleRate));

    c.highPass = static_cast<float> (
        1.0 - std::exp (-juce::MathConstants<double>::twoPi * 90.0 / sampleRate));
    c.earlyGain = early * (0.21f + 0.14f * openSky);
    c.lateGain = 0.62f * (1.0f - 0.55f * openSky) * 3.0f * bodyVolume;

    const auto bodyHighHz = 3200.0 * std::exp2 (-0.45 * warmth);
    c.bodyHighPass = static_cast<float> (
        1.0 - std::exp (-juce::MathConstants<double>::twoPi
                        * std::min (sampleRate * 0.45, bodyHighHz) / sampleRate));
    c.bodyLowPass = static_cast<float> (
        1.0 - std::exp (-juce::MathConstants<double>::twoPi * 180.0 / sampleRate));
    c.bodySupport = 0.55f * bodyVolume;
    c.widthGain = width * 1.25f;
}

void SynthEngine::processLwsReverb (float& left, float& right, const Params& p)
{
    if (lwsReverbBuffers[0].empty())
        return;

    const auto& c = lwsReverbCoefficients;
    if (c.mix <= epsilon)
    {
        if (lwsReverbMixActive)
            resetLwsReverbState();
        reverbTail = 0.0f;
        return;
    }
    lwsReverbMixActive = true;

    const auto dryLeft = left;
    const auto dryRight = right;
    const auto sourceLevel = std::max (std::abs (dryLeft), std::abs (dryRight));
    const auto sourceActive = sourceLevel > 0.00000001f;

    if (sourceActive)
        lwsReverbTailAge = 0;
    else
        ++lwsReverbTailAge;

    const auto tailOpenRaw = juce::jlimit (0.0, 1.0,
        static_cast<double> (lwsReverbTailAge) / std::max (1.0, sampleRate * 1.35));
    const auto tailOpen = static_cast<float> (
        tailOpenRaw * tailOpenRaw * (3.0 - 2.0 * tailOpenRaw));

    const auto random01 = [this]()
    {
        return (randomSigned() + 1.0f) * 0.5f;
    };
    if (c.tailMotion > epsilon)
    {
        if (--lwsReverbMotionCountA <= 0)
        {
            lwsReverbMotionTargetA = randomSigned();
            lwsReverbMotionCountA = static_cast<int> (
                sampleRate * (0.85 + random01() * 1.90));
        }
        if (--lwsReverbMotionCountB <= 0)
        {
            lwsReverbMotionTargetB = randomSigned();
            lwsReverbMotionCountB = static_cast<int> (
                sampleRate * (2.10 + random01() * 3.80));
        }
        const auto ka = static_cast<float> (1.0 - std::exp (-1.0 / (0.72 * sampleRate)));
        const auto kb = static_cast<float> (1.0 - std::exp (-1.0 / (1.65 * sampleRate)));
        lwsReverbMotionA += (lwsReverbMotionTargetA - lwsReverbMotionA) * ka;
        lwsReverbMotionB += (lwsReverbMotionTargetB - lwsReverbMotionB) * kb;
    }
    else
    {
        const auto k = static_cast<float> (1.0 - std::exp (-1.0 / (0.35 * sampleRate)));
        lwsReverbMotionA += (0.0f - lwsReverbMotionA) * k;
        lwsReverbMotionB += (0.0f - lwsReverbMotionB) * k;
    }
    const auto motionPan = lwsReverbMotionA * 0.68f + lwsReverbMotionB * 0.32f;
    const auto motionSpread = lwsReverbMotionB * 0.72f - lwsReverbMotionA * 0.28f;

    lwsReverbSendLpL += (dryLeft - lwsReverbSendLpL) * c.sendLowPass;
    lwsReverbSendLpR += (dryRight - lwsReverbSendLpR) * c.sendLowPass;
    lwsReverbSendLowL += (lwsReverbSendLpL - lwsReverbSendLowL) * c.highPass;
    lwsReverbSendLowR += (lwsReverbSendLpR - lwsReverbSendLowR) * c.highPass;
    const auto inputLeft = (lwsReverbSendLpL - lwsReverbSendLowL) * 0.62f;
    const auto inputRight = (lwsReverbSendLpR - lwsReverbSendLowR) * 0.62f;

    auto read = [this] (std::size_t buffer, int delay)
    {
        auto position = lwsReverbIndex - delay;
        if (position < 0)
            position += static_cast<int> (lwsReverbBuffers[buffer].size());
        return lwsReverbBuffers[buffer][static_cast<std::size_t> (position)];
    };

    lwsReverbBuffers[0][static_cast<std::size_t> (lwsReverbIndex)] = inputLeft;
    lwsReverbBuffers[1][static_cast<std::size_t> (lwsReverbIndex)] = inputRight;

    const auto e1 = read (0, c.earlyTaps[0]);
    const auto e2 = read (1, c.earlyTaps[1]);
    const auto e3 = read (0, c.earlyTaps[2]);
    const auto e4 = read (1, c.earlyTaps[3]);
    const auto earlyLeft = e1 * 0.50f + e2 * 0.28f + e3 * 0.22f + e4 * 0.14f;
    const auto earlyRight = e1 * 0.28f + e2 * 0.50f + e3 * 0.14f + e4 * 0.22f;

    auto allpass = [&] (std::size_t buffer, int delay, float gain, float input)
    {
        const auto z = read (buffer, delay);
        const auto y = z - gain * input;
        lwsReverbBuffers[buffer][static_cast<std::size_t> (lwsReverbIndex)] = input + gain * y;
        return y;
    };

    const auto ap1Left = allpass (10, c.allpassDelays[0], c.allpass1, inputLeft);
    const auto ap1Right = allpass (11, c.allpassDelays[1], c.allpass1, inputRight);
    const auto diffLeft = allpass (12, c.allpassDelays[2], c.allpass2, ap1Left);
    const auto diffRight = allpass (13, c.allpassDelays[3], c.allpass2, ap1Right);

    std::array<float, 8> q {};
    for (std::size_t i = 0; i < q.size(); ++i)
    {
        q[i] = read (2 + i, c.delays[i]);
        lwsReverbFeedbackLp[i] += (q[i] - lwsReverbFeedbackLp[i]) * c.damping[i];
    }

    const auto a1 = lwsReverbFeedbackLp[0] + lwsReverbFeedbackLp[1];
    const auto a2 = lwsReverbFeedbackLp[0] - lwsReverbFeedbackLp[1];
    const auto a3 = lwsReverbFeedbackLp[2] + lwsReverbFeedbackLp[3];
    const auto a4 = lwsReverbFeedbackLp[2] - lwsReverbFeedbackLp[3];
    const auto a5 = lwsReverbFeedbackLp[4] + lwsReverbFeedbackLp[5];
    const auto a6 = lwsReverbFeedbackLp[4] - lwsReverbFeedbackLp[5];
    const auto a7 = lwsReverbFeedbackLp[6] + lwsReverbFeedbackLp[7];
    const auto a8 = lwsReverbFeedbackLp[6] - lwsReverbFeedbackLp[7];

    const auto b1 = a1 + a3, b2 = a2 + a4, b3 = a1 - a3, b4 = a2 - a4;
    const auto b5 = a5 + a7, b6 = a6 + a8, b7 = a5 - a7, b8 = a6 - a8;
    constexpr auto hadamardNorm = 0.353553390593274f;
    const std::array<float, 8> matrix {
        (b1 + b5) * hadamardNorm, (b2 + b6) * hadamardNorm,
        (b3 + b7) * hadamardNorm, (b4 + b8) * hadamardNorm,
        (b1 - b5) * hadamardNorm, (b2 - b6) * hadamardNorm,
        (b3 - b7) * hadamardNorm, (b4 - b8) * hadamardNorm
    };

    const auto inputMid = 0.5f * (diffLeft + diffRight);
    const auto inputSide = 0.5f * (diffLeft - diffRight);
    const std::array<float, 8> injection {
        diffLeft * 0.085f + earlyLeft * 0.13f,
        diffRight * 0.085f + earlyRight * 0.13f,
        inputMid * 0.080f + inputSide * 0.045f,
        inputMid * 0.080f - inputSide * 0.045f,
        diffLeft * 0.060f - diffRight * 0.025f + earlyRight * 0.055f,
        diffRight * 0.060f - diffLeft * 0.025f + earlyLeft * 0.055f,
        inputMid * 0.055f + inputSide * 0.060f,
        inputMid * 0.055f - inputSide * 0.060f
    };
    for (std::size_t i = 0; i < q.size(); ++i)
        lwsReverbBuffers[2 + i][static_cast<std::size_t> (lwsReverbIndex)]
            = injection[i] + matrix[i] * c.feedback[i];

    auto lateLeft = q[0] * 0.28f - q[1] * 0.18f + q[2] * 0.24f + q[3] * 0.12f
                  - q[4] * 0.20f + q[5] * 0.18f + q[6] * 0.14f - q[7] * 0.10f;
    auto lateRight = q[1] * 0.28f + q[0] * 0.18f - q[3] * 0.24f + q[2] * 0.12f
                   + q[5] * 0.20f - q[4] * 0.18f - q[7] * 0.14f + q[6] * 0.10f;

    lwsReverbBodyLpL += (lateLeft - lwsReverbBodyLpL) * c.bodyHighPass;
    lwsReverbBodyLpR += (lateRight - lwsReverbBodyLpR) * c.bodyHighPass;
    lwsReverbBodyLowL += (lwsReverbBodyLpL - lwsReverbBodyLowL) * c.bodyLowPass;
    lwsReverbBodyLowR += (lwsReverbBodyLpR - lwsReverbBodyLowR) * c.bodyLowPass;
    lateLeft += (lwsReverbBodyLpL - lwsReverbBodyLowL) * c.bodySupport;
    lateRight += (lwsReverbBodyLpR - lwsReverbBodyLowR) * c.bodySupport;

    const auto tailDecor = (q[0] - q[4] + q[2] - q[6]
                          - q[1] + q[5] - q[3] + q[7]) * 0.16f;
    const auto lateMid = 0.5f * (lateLeft + lateRight);
    auto lateSide = 0.5f * (lateLeft - lateRight);
    auto spreadMultiplier = 1.0f + tailOpen * c.tailMotion
        * (1.35f + motionSpread * 0.32f);
    spreadMultiplier = std::max (1.0f, spreadMultiplier);
    lateSide = lateSide * c.widthGain * spreadMultiplier
             + tailDecor * tailOpen * c.tailMotion
               * (0.55f + 0.35f * juce::jlimit (0.0f, 1.0f, p.reverbWidth));

    const auto tailPan = juce::jlimit (-0.55f, 0.55f,
        motionPan * c.tailMotion * (0.10f + 0.42f * tailOpen));
    auto wetLeft = (lateMid + lateSide) * (1.0f - tailPan);
    auto wetRight = (lateMid - lateSide) * (1.0f + tailPan);
    wetLeft = earlyLeft * c.earlyGain + wetLeft * c.lateGain;
    wetRight = earlyRight * c.earlyGain + wetRight * c.lateGain;

    const auto wetMid = 0.5f * (wetLeft + wetRight);
    const auto wetSide = 0.5f * (wetLeft - wetRight) * c.widthGain;
    wetLeft = wetMid + wetSide;
    wetRight = wetMid - wetSide;

    // LWS-7's approved x2 wet calibration.
    left = dryLeft + wetLeft * c.mix * 2.0f;
    right = dryRight + wetRight * c.mix * 2.0f;

    const auto tailEnergy = std::max ({
        std::abs (q[0]), std::abs (q[1]), std::abs (q[2]), std::abs (q[3]),
        std::abs (q[4]), std::abs (q[5]), std::abs (q[6]), std::abs (q[7]),
        std::abs (earlyLeft), std::abs (earlyRight)
    });
    reverbTail = tailEnergy > reverbTail ? tailEnergy : reverbTail * 0.9995f;

    lwsReverbIndex = (lwsReverbIndex + 1)
                   % static_cast<int> (lwsReverbBuffers[0].size());
}

void SynthEngine::processReverb1 (float& left, float& right, const Params& p,
                                  float sidechainLeft, float sidechainRight)
{
    if (reverbPredelayLeft.empty())
        return;

    // Do not clock an empty eight-line FDN merely to move inaudible controls.
    // This mirrors rvr_param_audio_live/post_chain_live in the JSFX.
    if (! p.reverbOn
        && reverbTail <= 0.00002f
        && std::abs (reverbOutputLeft) <= epsilon
        && std::abs (reverbOutputRight) <= epsilon
        && std::abs (reverbWetSmoothed) <= epsilon
        && std::abs (reverbEarlyLevelSmoothed) <= epsilon
        && reverbCompressorState.runningAverage <= epsilon
        && std::abs (reverbCompressorState.runningDb) <= epsilon)
        return;

    const auto dryLeft = left;
    const auto dryRight = right;
    const auto inputLeft = p.reverbOn ? dryLeft * 1.6f : 0.0f;
    const auto inputRight = p.reverbOn ? dryRight * 1.6f : 0.0f;

    reverbWetSmoothed += ((p.reverbOn ? p.reverbWet : 0.0f) - reverbWetSmoothed) * 0.0005f;
    reverbWidthSmoothed += (p.reverbWidth - reverbWidthSmoothed) * 0.0005f;
    reverbEarlyLevelSmoothed += ((p.reverbOn ? p.reverbEarlyLevel : 0.0f)
                                 - reverbEarlyLevelSmoothed) * 0.0005f;
    reverbEarlyPanSmoothed += (p.reverbEarlyPan - reverbEarlyPanSmoothed) * 0.0005f;
    reverbEarlyRatioSmoothed += (p.reverbEarlyRatio - reverbEarlyRatioSmoothed) * 0.0005f;

    const auto predelaySamples = juce::jlimit (0,
        static_cast<int> (reverbPredelayLeft.size()) - 1,
        juce::roundToInt (std::max (0.0f, p.reverbPredelayMs) * 0.001 * sampleRate));
    reverbPredelayLeft[static_cast<std::size_t> (reverbPredelayWriteLeft)] = inputLeft;
    reverbPredelayRight[static_cast<std::size_t> (reverbPredelayWriteRight)] = inputRight;
    reverbPredelayWriteLeft = (reverbPredelayWriteLeft + 1)
                            % static_cast<int> (reverbPredelayLeft.size());
    reverbPredelayWriteRight = (reverbPredelayWriteRight + 1)
                             % static_cast<int> (reverbPredelayRight.size());
    auto readLeft = reverbPredelayWriteLeft - predelaySamples;
    auto readRight = reverbPredelayWriteRight - predelaySamples;
    if (readLeft < 0) readLeft += static_cast<int> (reverbPredelayLeft.size());
    if (readRight < 0) readRight += static_cast<int> (reverbPredelayRight.size());
    const auto predelayedLeft = reverbPredelayLeft[static_cast<std::size_t> (readLeft)];
    const auto predelayedRight = reverbPredelayRight[static_cast<std::size_t> (readRight)];

    float earlyLeft = 0.0f, earlyRight = 0.0f;
    if (std::abs (reverbEarlyLevelSmoothed) > epsilon)
    {
        const auto earlyDelay = juce::jlimit (0,
            static_cast<int> (reverbPredelayLeft.size()) - 1,
            juce::roundToInt (predelaySamples * reverbEarlyRatioSmoothed));
        auto earlyReadLeft = reverbPredelayWriteLeft - earlyDelay;
        auto earlyReadRight = reverbPredelayWriteRight - earlyDelay;
        if (earlyReadLeft < 0) earlyReadLeft += static_cast<int> (reverbPredelayLeft.size());
        if (earlyReadRight < 0) earlyReadRight += static_cast<int> (reverbPredelayRight.size());
        const auto sourceLeft = reverbPredelayLeft[static_cast<std::size_t> (earlyReadLeft)];
        const auto sourceRight = reverbPredelayRight[static_cast<std::size_t> (earlyReadRight)];
        const auto pan = juce::jlimit (-1.0f, 1.0f, reverbEarlyPanSmoothed);
        earlyLeft = (pan < 0.0f ? sourceLeft + (-pan) * sourceRight
                                : sourceLeft * (1.0f - pan)) * reverbEarlyLevelSmoothed;
        earlyRight = (pan > 0.0f ? sourceRight + pan * sourceLeft
                                 : sourceRight * (1.0f + pan)) * reverbEarlyLevelSmoothed;
    }

    std::array<float, 8> network {};
    for (std::size_t line = 0; line < reverbLines.size(); ++line)
    {
        auto& reverbLine = reverbLines[line];
        const auto diffuserIndex = static_cast<std::size_t> (reverbLine.diffuserPosition);
        const auto delayIndex = static_cast<std::size_t> (reverbLine.delayPosition);
        const auto delayedDiffuser = reverbLine.diffuser[diffuserIndex];
        const auto source = line < 4 ? predelayedLeft : predelayedRight;
        const auto injectionSign = (line % 4) < 2 ? 1.0f : -1.0f;
        auto diffuserInput = reverbLine.delay[delayIndex] + source * 0.22f * injectionSign;
        diffuserInput -= reverbLine.diffuserCoefficient * delayedDiffuser;
        reverbLine.diffuser[diffuserIndex] = diffuserInput;
        reverbLine.diffuserPosition = (reverbLine.diffuserPosition + 1)
                                      % static_cast<int> (reverbLine.diffuser.size());
        network[line] = delayedDiffuser + reverbLine.diffuserCoefficient * diffuserInput;
    }

    const auto butterfly = [&network] (std::size_t a, std::size_t b)
    {
        const auto difference = network[a] - network[b];
        network[a] += network[b];
        network[b] = difference;
    };
    butterfly (0, 1); butterfly (2, 3); butterfly (4, 5); butterfly (6, 7);
    butterfly (0, 2); butterfly (1, 3); butterfly (4, 6); butterfly (5, 7);
    butterfly (0, 4); butterfly (1, 5); butterfly (2, 6); butterfly (3, 7);

    reverbOutputLeft = (network[0] + network[2] + network[5] + network[7]) * 0.25f;
    reverbOutputRight = (network[1] + network[3] + network[4] + network[6]) * 0.25f;
    constexpr auto networkGain = 0.3535533905932738f;
    for (std::size_t line = 0; line < reverbLines.size(); ++line)
    {
        auto& reverbLine = reverbLines[line];
        auto feedback = network[line] * networkGain;
        reverbLine.lowState += reverbLine.lowCoefficient * (feedback - reverbLine.lowState);
        feedback += reverbLine.lowGain * reverbLine.lowState;
        reverbLine.highState += reverbLine.highCoefficient * (feedback - reverbLine.highState);
        feedback = juce::jlimit (-1.2f, 1.2f, reverbLine.midGain * reverbLine.highState);
        reverbLine.delay[static_cast<std::size_t> (reverbLine.delayPosition)] = feedback;
        reverbLine.delayPosition = (reverbLine.delayPosition + 1)
                                  % static_cast<int> (reverbLine.delay.size());
    }

    const auto middle = 0.5f * (reverbOutputLeft + reverbOutputRight);
    const auto side = 0.5f * (reverbOutputLeft - reverbOutputRight);
    const auto widthScale = 0.65f + reverbWidthSmoothed * 0.35f;
    auto wetLeft = (middle + side * widthScale) * 2.0f + earlyLeft;
    auto wetRight = (middle - side * widthScale) * 2.0f + earlyRight;

    if (p.reverbCompressor.mix > epsilon)
    {
        processCompressor (wetLeft, wetRight,
                           p.reverbCompressor.sidechain ? sidechainLeft : wetLeft,
                           p.reverbCompressor.sidechain ? sidechainRight : wetRight,
                           p.reverbCompressor, reverbCompressorState, true, false);
    }
    else
    {
        reverbCompressorState.runningAverage *= 0.999f;
        reverbCompressorState.runningDb *= 0.999f;
    }

    const auto activity = std::max ({ std::abs (inputLeft), std::abs (inputRight),
                                      std::abs (reverbOutputLeft), std::abs (reverbOutputRight) });
    reverbTail = activity > reverbTail ? activity : reverbTail * 0.9995f;
    const auto wet = juce::jlimit (0.0f, 1.0f, reverbWetSmoothed);
    // Send architecture: the source bus remains dry outside the reverb and
    // this processor contributes only an additive wet return.
    left = dryLeft + wetLeft * wet;
    right = dryRight + wetRight * wet;
}

SynthEngine::BiquadCoefficients SynthEngine::makeLowPass (double rate, float frequency, float q)
{
    const auto omega = juce::MathConstants<double>::twoPi * frequency / rate;
    double sine = 0.0, cosine = 0.0;
    filterSinCos (omega, sine, cosine);
    const auto alpha = sine / (2.0 * std::max (0.05f, q));
    const auto inverseA0 = 1.0 / (1.0 + alpha);
    const auto b0 = (1.0 - cosine) * 0.5 * inverseA0;
    return { static_cast<float> (b0), static_cast<float> (2.0 * b0),
             static_cast<float> (b0), static_cast<float> (-2.0 * cosine * inverseA0),
             static_cast<float> ((1.0 - alpha) * inverseA0) };
}

SynthEngine::BiquadCoefficients SynthEngine::makeHighPass (double rate, float frequency, float q)
{
    const auto omega = juce::MathConstants<double>::twoPi * frequency / rate;
    double sine = 0.0, cosine = 0.0;
    filterSinCos (omega, sine, cosine);
    const auto alpha = sine / (2.0 * std::max (0.05f, q));
    const auto inverseA0 = 1.0 / (1.0 + alpha);
    const auto b0 = (1.0 + cosine) * 0.5 * inverseA0;
    return { static_cast<float> (b0), static_cast<float> (-2.0 * b0),
             static_cast<float> (b0), static_cast<float> (-2.0 * cosine * inverseA0),
             static_cast<float> ((1.0 - alpha) * inverseA0) };
}

SynthEngine::BiquadCoefficients SynthEngine::makePeak (double rate, float frequency,
                                                       float gainDb, float q)
{
    const auto safeFrequency = juce::jlimit (10.0, rate * 0.49,
                                              static_cast<double> (frequency));
    const auto omega = juce::MathConstants<double>::twoPi * safeFrequency / rate;
    const auto cosine = std::cos (omega);
    const auto sine = std::sin (omega);
    const auto amplitude = std::pow (10.0, static_cast<double> (gainDb) / 40.0);
    const auto alpha = sine / (2.0 * std::max (0.05f, q));
    const auto inverseA0 = 1.0 / (1.0 + alpha / amplitude);
    return {
        static_cast<float> ((1.0 + alpha * amplitude) * inverseA0),
        static_cast<float> (-2.0 * cosine * inverseA0),
        static_cast<float> ((1.0 - alpha * amplitude) * inverseA0),
        static_cast<float> (-2.0 * cosine * inverseA0),
        static_cast<float> ((1.0 - alpha / amplitude) * inverseA0)
    };
}

SynthEngine::BiquadCoefficients SynthEngine::makeHighShelf (double rate, float frequency,
                                                            float gainDb, float slope)
{
    const auto safeFrequency = juce::jlimit (10.0, rate * 0.49,
                                              static_cast<double> (frequency));
    const auto omega = juce::MathConstants<double>::twoPi * safeFrequency / rate;
    const auto cosine = std::cos (omega);
    const auto sine = std::sin (omega);
    const auto amplitude = std::pow (10.0, static_cast<double> (gainDb) / 40.0);
    const auto safeSlope = std::max (0.001, static_cast<double> (slope));
    const auto rootTerm = (amplitude + 1.0 / amplitude) * (1.0 / safeSlope - 1.0) + 2.0;
    const auto alpha = sine * 0.5 * std::sqrt (std::max (0.0, rootTerm));
    const auto rootAmplitude = std::sqrt (amplitude);
    const auto a0 = (amplitude + 1.0) - (amplitude - 1.0) * cosine
                  + 2.0 * rootAmplitude * alpha;
    const auto inverseA0 = 1.0 / a0;
    return {
        static_cast<float> (amplitude * ((amplitude + 1.0)
            + (amplitude - 1.0) * cosine + 2.0 * rootAmplitude * alpha) * inverseA0),
        static_cast<float> (-2.0 * amplitude * ((amplitude - 1.0)
            + (amplitude + 1.0) * cosine) * inverseA0),
        static_cast<float> (amplitude * ((amplitude + 1.0)
            + (amplitude - 1.0) * cosine - 2.0 * rootAmplitude * alpha) * inverseA0),
        static_cast<float> (2.0 * ((amplitude - 1.0)
            - (amplitude + 1.0) * cosine) * inverseA0),
        static_cast<float> (((amplitude + 1.0)
            - (amplitude - 1.0) * cosine - 2.0 * rootAmplitude * alpha) * inverseA0)
    };
}

SynthEngine::FormantCoefficients SynthEngine::makeFormants (double rate, float position)
{
    struct Vowel
    {
        std::array<double, 3> omega;
        std::array<float, 3> amplitude;
        std::array<float, 3> q;
    };

    static constexpr std::array<Vowel, 6> vowels {{
        {{ 4146.9023027385265, 10681.415022205296, 15079.644737231007 },
         { 6.0f, 1.0606601718f, 1.0606601718f }, { 5.0f, 20.0f, 20.0f }},
        {{ 3330.088212805181, 11623.892818282235, 15707.963267948966 },
         { 6.0f, 1.0606601718f, 2.1213203436f }, { 5.0f, 20.0f, 50.0f }},
        {{ 2513.2741228718346, 12566.370614359172, 16022.122533307946 },
         { 6.0f, 1.0606601718f, 2.1213203436f }, { 5.0f, 20.0f, 50.0f }},
        {{ 1884.9555921538758, 5466.37121724624, 14137.16694115407 },
         { 6.0f, 1.0606601718f, 2.1213203436f }, { 5.0f, 20.0f, 50.0f }},
        {{ 4021.238596594935, 7539.822368615503, 15079.644737231007 },
         { 6.0f, 1.6836930725f, 1.3363480772f }, { 9.0f, 10.0f, 20.0f }},
        {{ 1300.6193585861743, 14451.326206513048, 18849.55592153876 },
         { 6.0f, 1.0606601718f, 2.1213203436f }, { 5.0f, 20.0f, 50.0f }}
    }};

    const auto location = juce::jlimit (0.0f, 5.0f, position);
    const auto lower = std::min (4, static_cast<int> (std::floor (location)));
    const auto fraction = lower == 4 && location >= 5.0f ? 1.0f : location - lower;
    const auto inverse = 1.0f - fraction;
    FormantCoefficients result;

    for (std::size_t band = 0; band < result.bands.size(); ++band)
    {
        const auto omega = vowels[static_cast<std::size_t> (lower)].omega[band] * inverse
                         + vowels[static_cast<std::size_t> (lower + 1)].omega[band] * fraction;
        const auto amplitude = vowels[static_cast<std::size_t> (lower)].amplitude[band] * inverse
                             + vowels[static_cast<std::size_t> (lower + 1)].amplitude[band] * fraction;
        const auto q = vowels[static_cast<std::size_t> (lower)].q[band] * inverse
                     + vowels[static_cast<std::size_t> (lower + 1)].q[band] * fraction;
        const auto w0 = juce::jlimit (0.000001,
                                      juce::MathConstants<double>::pi * 0.95, omega / rate);
        const auto sine = std::sin (w0);
        const auto cosine = std::cos (w0);
        const auto alpha = 0.5 * sine / (2.0 * q);
        const auto inverseA0 = 1.0 / (1.0 + alpha);
        const auto b0 = alpha * inverseA0;
        result.bands[band] = { static_cast<float> (b0), 0.0f, static_cast<float> (-b0),
                               static_cast<float> (-2.0 * cosine * inverseA0),
                               static_cast<float> ((1.0 - alpha) * inverseA0) };
        result.amplitudes[band] = amplitude;
    }
    return result;
}

float SynthEngine::processBiquad (float input, BiquadState& state,
                                  const BiquadCoefficients& coefficients)
{
    const auto output = coefficients.b0 * input
                      + coefficients.b1 * state.x1
                      + coefficients.b2 * state.x2
                      - coefficients.a1 * state.y1
                      - coefficients.a2 * state.y2;
    state.x2 = state.x1;
    state.x1 = input;
    state.y2 = state.y1;
    state.y1 = std::isfinite (output) ? output : 0.0f;
    if (! std::isfinite (output))
        state = {};
    return state.y1;
}

float SynthEngine::polyBlep (double phase, double increment)
{
    if (increment <= 0.0)
        return 0.0f;
    if (phase < increment)
    {
        const auto x = phase / increment;
        return static_cast<float> (x + x - x * x - 1.0);
    }
    if (phase > 1.0 - increment)
    {
        const auto x = (phase - 1.0) / increment;
        return static_cast<float> (x * x + x + x + 1.0);
    }
    return 0.0f;
}

float SynthEngine::oscillator (int wave, double phase, double increment, float pwm,
                               float& triangleState, float metal, float shark, float sync)
{
    if (wave <= 1 && metal > epsilon)
    {
        const auto amount = metal * metal / (1.0 + increment * 80.0);
        const auto modulation = std::sin (juce::MathConstants<double>::twoPi * phase);
        const auto modulatedPhase = phase + modulation * amount * 0.035;
        return static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * modulatedPhase));
    }

    if (wave == 0)
        return static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * phase));
    if (wave == 1)
    {
        const auto triangle = phase < 0.25 ? phase * 4.0
                            : phase < 0.75 ? 2.0 - phase * 4.0
                                           : phase * 4.0 - 4.0;
        return static_cast<float> (triangle);
    }
    if (wave == 2)
    {
        const auto saw = static_cast<float> (2.0 * phase - 1.0) - polyBlep (phase, increment);
        triangleState += saw * static_cast<float> (increment) * 2.0f;
        triangleState -= triangleState * 0.0005f;
        auto triangle = triangleState * 5.65685424f;
        if (shark > epsilon)
        {
            const auto scaledShark = shark * 0.1f;
            const auto amount = scaledShark * scaledShark;
            auto shaped = juce::jlimit (-1.2f, 1.2f, triangle * (1.0f + amount * 1.5f));
            shaped += (shaped - shaped * shaped * shaped / 3.0f) * amount * 2.5f;
            triangle = shaped / (1.0f + amount * 1.2f);
        }
        return triangle;
    }

    auto renderedPhase = phase;
    if (sync > epsilon)
        renderedPhase = wrapPhase (phase * (1.0 + sync * sync * 0.15));
    if (wave == 3)
        return static_cast<float> (2.0 * renderedPhase - 1.0)
             - polyBlep (renderedPhase, increment);

    auto output = renderedPhase < pwm ? 1.0f : -1.0f;
    output += polyBlep (renderedPhase, increment);
    auto second = renderedPhase - pwm;
    if (second < 0.0)
        second += 1.0;
    return output - polyBlep (second, increment);
}

float SynthEngine::morphOscillator (float morph, double phase, double increment, float pwm,
                                    float& triangleState, float metal, float shark, float sync)
{
    const auto scaled = juce::jlimit (0.0f, 1.0f, morph) * 4.0f;
    const auto waveA = juce::jlimit (0, 4, static_cast<int> (std::floor (scaled)));
    const auto waveB = std::min (4, waveA + 1);
    const auto blend = juce::jlimit (0.0f, 1.0f, scaled - waveA);

    float sharkValue = 0.0f;
    const auto needsShark = waveA == 2 || (blend > epsilon && waveB == 2);
    if (needsShark)
        sharkValue = oscillator (2, phase, increment, pwm, triangleState, metal, shark, sync);

    const auto renderWave = [&] (int wave)
    {
        if (wave == 2)
            return sharkValue;
        return oscillator (wave, phase, increment, pwm, triangleState, metal, shark, sync);
    };

    const auto first = renderWave (waveA);
    return blend <= epsilon ? first : first * (1.0f - blend) + renderWave (waveB) * blend;
}

float SynthEngine::superWaveOscillator (const SuperWaveParameters& p, double phase,
                                        double increment, float pwm, float& triangleState,
                                        float metal, float shark, float sync, float length)
{
    static_cast<void> (triangleState);
    length = juce::jlimit (0.125f, 16.0f, length);
    const auto repeatedPhase = wrapPhase (phase / length);
    const auto repeatedIncrement = std::min (0.49, increment / length);
    const auto totalWidth = static_cast<double> (std::accumulate (p.width.begin(), p.width.end(), 0));

    int selected = 4;
    double segmentStart = 0.0;
    double accumulated = 0.0;
    for (int step = 0; step < 5; ++step)
    {
        const auto segmentEnd = accumulated + p.width[static_cast<std::size_t> (step)] / totalWidth;
        if (repeatedPhase < segmentEnd)
        {
            selected = step;
            segmentStart = accumulated;
            break;
        }
        accumulated = segmentEnd;
        segmentStart = accumulated;
    }

    const auto width = p.width[static_cast<std::size_t> (selected)] / totalWidth;
    const auto local = (repeatedPhase - segmentStart) / width;
    const auto pitchMultiplier = std::exp2 (p.pitch[static_cast<std::size_t> (selected)] / 12.0f);
    const auto localPhase = wrapPhase (local * pitchMultiplier);
    const auto localIncrement = std::min (0.49, repeatedIncrement / width * pitchMultiplier);
    const auto wave = p.waves[static_cast<std::size_t> (selected)];

    float output = 0.0f;
    if (wave == 2)
    {
        const auto classicTriangle = localPhase < 0.25 ? localPhase * 4.0
                                   : localPhase < 0.75 ? 2.0 - localPhase * 4.0
                                                       : localPhase * 4.0 - 4.0;
        output = static_cast<float> (classicTriangle * 2.82842712);
        if (shark > epsilon)
        {
            const auto scaledShark = shark * 0.1f;
            const auto amount = scaledShark * scaledShark;
            auto shaped = juce::jlimit (-1.2f, 1.2f, output * (1.0f + amount * 1.5f));
            shaped += (shaped - shaped * shaped * shaped / 3.0f) * amount * 2.5f;
            output = shaped / (1.0f + amount * 1.2f);
        }
    }
    else
    {
        float unusedTriangle = 0.0f;
        output = oscillator (wave, localPhase, localIncrement, pwm, unusedTriangle,
                             metal, shark, sync);
    }

    auto character = juce::jlimit (0.0f, 16.0f, p.character);
    if (character > epsilon)
    {
        auto mix = std::min (character, 1.0f);
        const auto quantised8 = std::floor (output * 8.0f) * 0.125f;
        output = output * (1.0f - mix) + quantised8 * mix;

        const auto extra = std::max (0.0f, character - 1.0f);
        if (extra > epsilon)
        {
            mix = std::min (1.0f, extra / 15.0f);
            const auto steps = std::max (2.0f, 8.0f - 6.0f * mix);
            const auto coarse = std::floor (output * steps) / steps;
            output = output * (1.0f - mix) + coarse * mix;
            const auto drive = 1.0f + 2.5f * mix;
            const auto driven = output * drive;
            output = driven / (1.0f + std::abs (driven));
            output *= (1.0f + drive) / drive;
        }
    }
    return output;
}

float SynthEngine::modulationOscillator (int wave, double phase, float pwm)
{
    if (wave == 0)
        return static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * phase));
    if (wave == 1)
        return phase < 0.25 ? static_cast<float> (phase * 4.0)
             : phase < 0.75 ? static_cast<float> (2.0 - phase * 4.0)
                            : static_cast<float> (phase * 4.0 - 4.0);
    if (wave <= 3)
        return static_cast<float> (2.0 * phase - 1.0);
    return phase < pwm ? 1.0f : -1.0f;
}

float SynthEngine::modulationMorph (float morph, double phase, float pwm)
{
    const auto scaled = juce::jlimit (0.0f, 1.0f, morph) * 4.0f;
    const auto waveA = juce::jlimit (0, 4, static_cast<int> (std::floor (scaled)));
    const auto waveB = std::min (4, waveA + 1);
    const auto blend = juce::jlimit (0.0f, 1.0f, scaled - waveA);
    const auto first = modulationOscillator (waveA, phase, pwm);
    return blend <= epsilon ? first
                            : first * (1.0f - blend)
                            + modulationOscillator (waveB, phase, pwm) * blend;
}

float SynthEngine::modulationSuperWave (const SuperWaveParameters& p, double phase,
                                        double increment, float pwm, float& triangleState,
                                        float metal, float shark, float sync, float length)
{
    // JSFX osc_superwave5 is used directly as the PM source. This deliberately
    // includes the layer's Wave Mod treatment and 8-bit character; using only
    // the raw segment waveform changes hidden-modulator patches such as factory
    // preset 20, Bass Industrial.
    return superWaveOscillator (p, phase, increment, pwm, triangleState,
                                metal, shark, sync, length);
}

float SynthEngine::morphPmGain (float morph)
{
    const auto scaled = juce::jlimit (0.0f, 1.0f, morph) * 4.0f;
    const auto waveA = juce::jlimit (0, 4, static_cast<int> (std::floor (scaled)));
    const auto waveB = std::min (4, waveA + 1);
    const auto blend = juce::jlimit (0.0f, 1.0f, scaled - waveA);
    const auto gain = [] (int wave) { return wave <= 2 ? 1.0f : wave == 3 ? 2.5f : 4.0f; };
    return gain (waveA) * (1.0f - blend) + gain (waveB) * blend;
}

float SynthEngine::superWavePmGain (const SuperWaveParameters& p)
{
    float total = 0.0f;
    for (const auto wave : p.waves)
        total += wave <= 2 ? 1.0f : wave == 3 ? 2.5f : 4.0f;
    return total * 0.2f;
}

float SynthEngine::panGain (float pan, bool left)
{
    pan = juce::jlimit (-1.0f, 1.0f, pan);
    return std::sqrt (0.5f * (left ? 1.0f - pan : 1.0f + pan));
}

void SynthEngine::enterDeepIdle()
{
    // Equivalent to the JSFX fast_idle_active transition. At this point a
    // complete delay-buffer cycle and all audible tails have already fallen
    // below their thresholds, so detector/filter crumbs can be discarded.
    chorusTail = 0.0f;
    chorusHpXLeft = chorusHpYLeft = chorusHpXRight = chorusHpYRight = 0.0f;
    resetDelayProcessors();
    dcXLeft.fill (0.0f); dcYLeft.fill (0.0f);
    dcXRight.fill (0.0f); dcYRight.fill (0.0f);
    eqStemLeft = {};
    eqStemRight = {};
    compressorState = {};
    resetReverbProcessors();
    glueEnvelope = 0.0f;
    pitchArpHeldCount = pitchArpLiveCount = pitchArpLatchCount = 0;
    pitchArpLatchValid = false;
    pitchArpPoolDirty = true;
    pitchArpState1 = {};
    pitchArpState2 = {};
    pitchArpState2.randomSeed = 1000;
    deepIdle = true;
}

float SynthEngine::randomSigned()
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return static_cast<float> (randomState) / static_cast<float> (UINT32_MAX) * 2.0f - 1.0f;
}

std::array<float, 2> SynthEngine::renderNoisePair (Voice& voice, int noiseType,
                                                   float colour,
                                                   double pitchClockIncrement)
{
    noiseType = juce::jlimit (0, 10, noiseType);
    const auto whiteLeft = randomSigned();
    const auto whiteRight = randomSigned();

    // Keep these histories running for every type. Switching back to Classic,
    // Pink or Brown is therefore smooth, and type zero remains the exact
    // pre-selector LJuno noise path.
    pinkLeft += 0.02f * (whiteLeft - pinkLeft);
    pinkRight += 0.02f * (whiteRight - pinkRight);
    brownLeft = (brownLeft + whiteLeft * 0.005f) * 0.995f;
    brownRight = (brownRight + whiteRight * 0.005f) * 0.995f;

    if (noiseType == 0)
    {
        colour = juce::jlimit (-1.0f, 1.0f, colour);
        return colour < 0.0f
            ? std::array<float, 2> { brownLeft * -colour + pinkLeft * (colour + 1.0f),
                                     brownRight * -colour + pinkRight * (colour + 1.0f) }
            : std::array<float, 2> { pinkLeft * (1.0f - colour) + whiteLeft * colour,
                                     pinkRight * (1.0f - colour) + whiteRight * colour };
    }
    if (noiseType == 1)
        return { whiteLeft, whiteRight };
    if (noiseType == 2)
        return { juce::jlimit (-1.0f, 1.0f, pinkLeft * 4.0f),
                 juce::jlimit (-1.0f, 1.0f, pinkRight * 4.0f) };
    if (noiseType == 3)
        return { juce::jlimit (-1.0f, 1.0f, brownLeft * 8.0f),
                 juce::jlimit (-1.0f, 1.0f, brownRight * 8.0f) };

    // Radio and vintage-chip noises are clocked from the current voice pitch.
    // Biscot advances its SID register by oscillator increment * 4; using the
    // voice's already compiled oscillator-1 increment preserves that behaviour
    // through portamento, Arp 2, pitch bend and pitch envelopes.
    voice.chipNoiseClock += juce::jlimit (0.0, 8.0, pitchClockIncrement);
    auto ticks = static_cast<int> (voice.chipNoiseClock);
    voice.chipNoiseClock -= ticks;
    ticks = juce::jlimit (0, 16, ticks);

    if (noiseType >= 4 && noiseType <= 6)
    {
        while (ticks-- > 0)
        {
            voice.chipNoiseLeft = randomSigned();
            voice.chipNoiseRight = randomSigned();
        }
        voice.radioLowLeft += 0.035f * (whiteLeft - voice.radioLowLeft);
        voice.radioLowRight += 0.035f * (whiteRight - voice.radioLowRight);
        const auto highLeft = whiteLeft - voice.radioLowLeft;
        const auto highRight = whiteRight - voice.radioLowRight;
        const auto tunedLeft = voice.chipNoiseLeft - voice.radioPreviousLeft;
        const auto tunedRight = voice.chipNoiseRight - voice.radioPreviousRight;
        voice.radioPreviousLeft = voice.chipNoiseLeft;
        voice.radioPreviousRight = voice.chipNoiseRight;

        if (noiseType == 4) // broadband static with tuned interference grains
            return { juce::jlimit (-1.0f, 1.0f, highLeft * 0.75f
                                                       + voice.chipNoiseLeft * 0.45f),
                     juce::jlimit (-1.0f, 1.0f, highRight * 0.75f
                                                       + voice.chipNoiseRight * 0.45f) };
        if (noiseType == 5) // tuning between stations: narrow, moving hash
            return { juce::jlimit (-1.0f, 1.0f, tunedLeft * 0.7f + highLeft * 0.25f),
                     juce::jlimit (-1.0f, 1.0f, tunedRight * 0.7f + highRight * 0.25f) };
        // impulsive radio crackle over a quieter static bed
        return { (std::abs (voice.chipNoiseLeft) > 0.82f ? voice.chipNoiseLeft : 0.0f)
                    + highLeft * 0.16f,
                 (std::abs (voice.chipNoiseRight) > 0.82f ? voice.chipNoiseRight : 0.0f)
                    + highRight * 0.16f };
    }

    const auto sid = [] (std::uint32_t& state)
    {
        state &= 0x7fffffu;
        if (state == 0)
            state = 0x7fffffu;
        const auto feedback = ((state >> 17u) ^ (state >> 22u)) & 1u;
        state = ((state << 1u) & 0x7fffffu) | feedback;
        const auto output = ((state >> 22u) & 1u) * 128u
                          + ((state >> 20u) & 1u) * 64u
                          + ((state >> 16u) & 1u) * 32u
                          + ((state >> 13u) & 1u) * 16u
                          + ((state >> 11u) & 1u) * 8u
                          + ((state >> 7u) & 1u) * 4u
                          + ((state >> 4u) & 1u) * 2u
                          + ((state >> 2u) & 1u);
        return static_cast<float> (output) / 127.5f - 1.0f;
    };
    if (noiseType == 7)
    {
        while (ticks-- > 0)
        {
            voice.chipNoiseLeft = sid (voice.sidNoiseLeft);
            voice.chipNoiseRight = sid (voice.sidNoiseRight);
        }
        return { voice.chipNoiseLeft, voice.chipNoiseRight };
    }

    const auto ay = [] (std::uint32_t& state)
    {
        state &= 0x1ffffu;
        if (state == 0)
            state = 0x1ffffu;
        const auto output = (state & 1u) != 0u ? 1.0f : -1.0f;
        const auto feedback = ((state >> 0u) ^ (state >> 3u)) & 1u;
        state = (state >> 1u) | (feedback << 16u);
        return output;
    };
    if (noiseType == 8)
    {
        while (ticks-- > 0)
        {
            voice.chipNoiseLeft = ay (voice.ayNoiseLeft);
            voice.chipNoiseRight = ay (voice.ayNoiseRight);
        }
        return { voice.chipNoiseLeft, voice.chipNoiseRight };
    }

    if (noiseType == 9)
    {
        const auto metallic = [] (std::uint32_t& state)
        {
            state &= 0x7fu;
            if (state == 0)
                state = 0x7fu;
            const auto output = (state & 1u) != 0u ? 1.0f : -1.0f;
            const auto feedback = ((state >> 0u) ^ (state >> 1u)) & 1u;
            state = (state >> 1u) | (feedback << 6u);
            return output;
        };
        while (ticks-- > 0)
        {
            voice.chipNoiseLeft = metallic (voice.metallicNoiseLeft);
            voice.chipNoiseRight = metallic (voice.metallicNoiseRight);
        }
        return { voice.chipNoiseLeft, voice.chipNoiseRight };
    }

    // Sparse independent impulses; pitch controls how often a new chance is
    // evaluated, turning it from isolated dust into an 8-bit spray.
    auto dustLeft = 0.0f, dustRight = 0.0f;
    while (ticks-- > 0)
    {
        const auto candidateLeft = randomSigned();
        const auto candidateRight = randomSigned();
        if (std::abs (candidateLeft) > 0.9f) dustLeft = candidateLeft;
        if (std::abs (candidateRight) > 0.9f) dustRight = candidateRight;
    }
    return { dustLeft, dustRight };
}
}
