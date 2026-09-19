// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SequencerState.h"

namespace ljuno
{
class SynthEngine
{
public:
    void prepare (double);
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&,
                  juce::AudioProcessorValueTreeState&, const SequencerState&,
                  double tempoBpm,
                  bool includeStereoInput, const float* sidechainLeft,
                  const float* sidechainRight,
                  const std::array<float*, 8>& auxOutputs,
                  std::uint64_t parameterRevision);
    bool isDeepIdle() const noexcept { return deepIdle; }

private:
    enum class Stage { idle, attack, decay, sustain, release, steal };

    struct BiquadState
    {
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    };

    struct BiquadCoefficients
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    };

    struct FormantCoefficients
    {
        std::array<BiquadCoefficients, 3> bands {};
        std::array<float, 3> amplitudes {};
    };

    struct RoutedFilterState
    {
        BiquadState lowPassLeft, lowPassRight;
        BiquadState lowPass2Left, lowPass2Right;
        BiquadState highPassLeft, highPassRight;
        BiquadCoefficients lowPassCoefficients, highPassCoefficients;
        float lowPassCoefficientFrequency = -1.0f, lowPassCoefficientQ = -1.0f;
        float highPassCoefficientFrequency = -1.0f, highPassCoefficientQ = -1.0f;
        std::array<BiquadState, 3> formantLeft, formantRight;
    };

    struct Voice
    {
        bool active = false, held = false;
        bool pending = false;
        int note = -1;
        int pendingNote = -1;
        // Bit 0 = Layer 1, bit 1 = Layer 2, bit 2 = Noise.
        // Normal MIDI uses all three bits.
        int layerMask = 7;
        int pendingLayerMask = 7;
        float pendingVelocity = 0.0f;
        float velocity = 0.0f, velocitySmoothed = 0.0f, velocityFilterSmoothed = 0.0f;
        float keyFollowVolumeGain = 1.0f;
        float keyFollowLowPass = 1.0f, keyFollowLowPassTarget = 1.0f;
        float keyFollowOctavesTarget = 0.0f;
        float envelope = 0.0f, envelope2 = 0.0f;
        float pitchEnvelope = 0.0f, releasePitch = 0.0f, releasePitch2 = 0.0f;
        double phase1 = 0.0, phase2 = 0.0;
        float triangleState1 = 0.0f, triangleState2 = 0.0f;
        std::array<double, 15> unisonPhase1 {}, unisonPhase2 {};
        std::array<float, 15> unisonTriangle1 {}, unisonTriangle2 {};
        float pwm1 = 0.5f, pwm2 = 0.5f;
        float drift = 0.0f, ping = 1.0f;
        float microMotionCommon = 0.0f;
        float microMotionIndependent1 = 0.0f, microMotionIndependent2 = 0.0f;
        float microMotionOut1 = 0.0f, microMotionOut2 = 0.0f;
        float microMotionPitchMultiplier1 = 1.0f, microMotionPitchMultiplier2 = 1.0f;
        double frequency = 0.0, targetFrequency = 0.0;
        double cachedPitchFrequency = -1.0;
        float cachedPerformancePitch = std::numeric_limits<float>::max();
        double cachedIncrement1 = 0.0, cachedIncrement2 = 0.0;
        Stage stage = Stage::idle, stage2 = Stage::idle;
        std::uint64_t age = 0;
        BiquadState lowPassLeft, lowPassRight;
        BiquadState lowPass2Left, lowPass2Right;
        BiquadState highPassLeft, highPassRight;
        BiquadCoefficients lowPassCoefficients, highPassCoefficients;
        float lowPassCoefficientFrequency = -1.0f, lowPassCoefficientQ = -1.0f;
        float highPassCoefficientFrequency = -1.0f, highPassCoefficientQ = -1.0f;
        std::array<BiquadState, 3> formantLeft, formantRight;
        std::array<RoutedFilterState, 3> routedFilters;
        FormantCoefficients formantCoefficients;
        float formantPosition = -1.0f;
        int formantControlCounter = 0;
        bool formantWasEnabled = false;
        std::uint32_t sidNoiseLeft = 0x7fffffu, sidNoiseRight = 0x5a5a5au;
        std::uint32_t ayNoiseLeft = 0x1ffffu, ayNoiseRight = 0x15555u;
        std::uint32_t metallicNoiseLeft = 0x7fu, metallicNoiseRight = 0x5du;
        double chipNoiseClock = 0.0;
        float chipNoiseLeft = 0.0f, chipNoiseRight = 0.0f;
        float radioLowLeft = 0.0f, radioLowRight = 0.0f;
        float radioPreviousLeft = 0.0f, radioPreviousRight = 0.0f;
        float noiseColourLowLeft = 0.0f, noiseColourLowRight = 0.0f;
        float noiseColourPreviousLeft = 0.0f, noiseColourPreviousRight = 0.0f;
    };

    struct LfoParameters
    {
        int mode = 0, wave = 0;
        float rate = 1.0f, squarePwm = 0.5f, smooth = 0.0f;
        float envelopeRate = 0.0f, crossRate = 0.0f, delay = 0.0f;
        float phase = 0.0f, oneShotPercent = 100.0f;
        float upperSquash = 0.0f, lowerSquash = 0.0f;
        bool oneShot = false;
    };

    struct LfoState
    {
        double phase = 0.0, oneShotPosition = 0.0;
        float previousPhase = 0.0f, coreValue = 0.0f;
        float smoothState = 0.0f, sampleHold = 0.0f, delayEnvelope = 0.0f;
        bool oneShotDone = false;
    };

    struct SuperWaveParameters
    {
        std::array<int, 5> waves { 0, 1, 2, 3, 4 };
        std::array<float, 5> pitch { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<int, 5> width { 1, 1, 1, 1, 1 };
        float length = 1.0f, character = 0.0f;
        float lfo1Length = 0.0f, lfo2Length = 0.0f;
        bool invertOddVoices = false;
    };

    struct PitchArpParameters
    {
        int mode = 0, octaveUp = 0, octaveDown = 0;
        float rate = 16.0f, glide = 0.0f, volumeDepth = 0.0f, pwmDepth = 0.0f;
        bool pitchMovement = true;
    };

    struct PitchArpState
    {
        std::array<int, 320> pool {};
        std::array<int, 128> sidSequence {};
        int poolCount = 0, rootNote = 60, stepIndex = -1;
        int sidSequenceLength = 0;
        double stepClock = 0.0;
        float pitchTarget = 0.0f, pitchCurrent = 0.0f;
        std::uint64_t randomSeed = 0;
    };

    struct LArpParameters
    {
        int state = 0, pattern = 0, lengthRandomMode = 0;
        int octaveUp = 0, octaveDown = 0, octaveMode = 1;
        int repeat = 1, legatoPattern = 0, microShiftMode = 0;
        int modeSwitch = 0, ratePattern = 0, sustainQuantize = 0;
        int midiChannel = 1;
        float division = 4.0f, shuffle = 0.0f, skipProbability = 0.0f;
        float length = 0.5f, repeatRandomDepth = 0.0f;
        float pitchRandomDepth = 0.0f, velocityRandomDepth = 0.0f;
        float shuffleVelocity = 0.0f, shuffleLength = 0.0f;
        float microShiftDepth = 0.5f, microShiftVelocityDepth = 0.0f;
        float microShiftLengthDepth = 0.0f, ratePatternSpeed = 0.5f;
        float volumeDepth = 0.0f, lowPassDepth = 0.0f, panDepth = 0.0f;
        float pitchDepth = 0.0f, pwmDepth = 0.0f, highPassDepth = 0.0f;
        float lfo1RateDepth = 0.0f, lfo2RateDepth = 0.0f;
        bool chordHold = false, adaptiveDivision = false;
        bool freeShuffle = true, resetSustain = false;
    };

    struct LArpState
    {
        std::array<bool, 128> held {}, keyDown {}, pendingRelease {}, outputActive {};
        std::array<int, 128> velocity {}, age {};
        std::array<int, 64> chord {}, chordVelocity {};
        int heldCount = 0, chordCount = 0, previousChordCount = 0;
        int currentNote = -1, arpStep = -1, arpIndex = 0, direction = 1;
        int repeatCounter = 0, shufflePhase = 0, previousMode = 0;
        int outputChannel = 1, ageCounter = 1;
        double sequenceTimer = 0.0, noteTimer = 0.0, activeDuration = 0.0;
        double ratePatternPhase = 0.0, microShiftSamples = 0.0;
        float modulation = 0.0f, panStage = 0.0f;
        float ratePatternRandom = 1.0f;
        float brownianLength = 1.0f, fractalPosition = 0.0f, fractalVelocity = 1.0f;
        std::uint32_t randomSeed = 0x6c617270u;
        bool sustain = false, chordDirty = true, forceTrigger = false;
        bool newChordPending = false, currentLegato = false, nextLegato = false;
        bool resetSustainWasDown = false;
    };

    struct SequencerRuntime
    {
        std::array<bool, 128> held {};
        std::array<int, 128> heldVelocity {};
        std::array<std::uint64_t, 128> heldAge {};
        std::uint64_t heldAgeCounter = 1;
        int heldCount = 0;
        int inputNote = -1;
        int previousInputNote = -1;
        int triggerVelocity = 100;
        int baseTranspose = 0;
        int position = 0;
        int direction = 1;
        int repeatCounter = 0;
        int activeRepeatTarget = 1;
        int shufflePhase = 0;
        int currentNote = -1;
        // Poly sequencer keeps one generated note for each physically held input key.
        std::array<bool, 128> activePolyVoice {};
        std::array<int, 128> activePolyOutputNote {};
        int outputChannel = 1;
        double stepTimer = 0.0;
        double noteTimer = 0.0;
        double activeDuration = 0.0;
        double launchRemaining = 0.0;
        double stepInterval = 0.0;
        bool running = false;
        bool waitingForLaunch = false;
        bool activeLegato = false;
        std::uint32_t randomSeed = 0x51e90001u;
    };

    struct CompressorParameters
    {
        float thresholdDb = -11.0f, makeupDb = 7.0f;
        float attackMicroseconds = 1000.0f, releaseMilliseconds = 250.0f;
        float mix = 0.5f;
        int ratio = 5;
        bool sidechain = false;
    };

    struct CompressorState
    {
        float runningAverage = 0.0f, runningDb = 0.0f;
    };

    struct ReverbLine
    {
        std::vector<float> diffuser, delay;
        int diffuserPosition = 0, delayPosition = 0;
        float diffuserCoefficient = 0.0f;
        float lowState = 0.0f, highState = 0.0f;
        float midGain = 0.0f, lowGain = 0.0f;
        float lowCoefficient = 0.0f, highCoefficient = 0.0f;
    };

    struct LwsReverbCoefficients
    {
        std::array<int, 4> earlyTaps {};
        std::array<int, 8> delays {};
        std::array<int, 4> allpassDelays {};
        std::array<float, 8> feedback {};
        std::array<float, 8> damping {};
        float allpass1 = 0.0f, allpass2 = 0.0f;
        float sendLowPass = 0.0f, highPass = 0.0f;
        float earlyGain = 0.0f, lateGain = 0.0f;
        float bodyHighPass = 0.0f, bodyLowPass = 0.0f, bodySupport = 0.0f;
        float widthGain = 1.0f, mix = 0.0f, rt60Seconds = 1.0f, tailMotion = 0.0f;
    };

    struct Params
    {
        int voiceCount = 8, polyMode = 0, wave1 = 3, wave2 = 3;
        int octave1 = 0, octave2 = 0, semitone1 = 0, semitone2 = 0;
        int lowPassSlope = 0, keyFollowFilterMode = 1;
        int monoNoteMode = 1, monoPortamentoMode = 0, monoUnisonVoices = 8;
        float balance = 0.5f, level1 = 1.0f, level2 = 1.0f, detune2 = 10.0f;
        float pan1 = -0.8f, pan2 = 0.8f, pwm = 0.5f;
        float velocityVolume = 0.0f, velocityFilter = 0.0f;
        float pitchBendRange = 2.0f, pitchEnvelopeAmount = 0.0f;
        float drift = 0.02f, portamento = 0.0f;
        float noiseLevel = 0.0f, noiseColor = 0.5f, noisePitch = 0.0f;
        float noiseStereo = 0.0f;
        float lfo1NoisePitch = 0.0f, lfo2NoisePitch = 0.0f;
        int noiseType = 0;
        float attack = 0.0003f, decay = 0.15f, sustain = 0.75f, release = 0.25f;
        float attack2 = 0.0003f, decay2 = 0.15f, sustain2 = 0.75f, release2 = 0.25f;
        float ampBlend1 = 0.0f, ampBlend2 = 0.0f, noiseBlend = 0.0f;
        float pitchAdsr2Blend = 0.0f, panAdsr2Blend = 0.0f, pitchReleaseDirection = 0.5f;
        bool filterUsesAdsr2 = false;
        bool filterLayerRouting = false;
        bool lowPassLayer1 = true, lowPassLayer2 = true, lowPassNoise = true;
        bool highPassLayer1 = true, highPassLayer2 = true, highPassNoise = true;
        bool formantLayer1 = true, formantLayer2 = true, formantNoise = true;
        float lowPassCutoff = 1.0f, lowPassResonance = 0.0f;
        float highPassCutoff = 0.0f, highPassResonance = 0.0f;
        float filterEnvelope = 0.0f, keyFollowFilter = 0.5f, keyFollowVolume = 0.5f;
        float panEnvelope = 0.0f, noteScalePan = 0.0f, pingPongPan = 0.0f;
        float voicePanAlternate = 0.0f;
        float monoUnisonDetune = 1.0f;
        float splitWidth = 0.0f, splitNote = 60.0f;
        bool splitInverted = false;
        float inputGainDb = 0.0f, masterVolumeDb = 0.0f, masterToneSemitones = 0.0f;
        float chorusLevel = 1.0f, chorusRate = 0.3f, chorusWidth = 1.0f;
        std::array<float, 3> chorusSend { 0.4f, 0.4f, 0.4f };
        std::array<float, 3> delaySend { 0.25f, 0.25f, 0.25f };
        std::array<float, 3> reverbSend { 0.2f, 0.2f, 0.2f };
        // 0 = Off, 1 = 3-4, 2 = 5-6, 3 = 7-8, 4 = 9-10.
        std::array<int, 4> auxOutput { 0, 0, 0, 0 };
        int delayMode = 1;
        bool delayOn = true, delayMono = false;
        int delaySync = 6;
        float delayTime = 0.2f, delayFeedback = 0.5f, delayMix = 1.0f;
        float delayTone = 0.5f, delayLfo1 = 0.0f, delayLfo2 = 0.0f;
        float delay2GlideMs = 180.0f;
        float delay2Speed1 = 1.0f, delay2Speed2 = 1.5f;
        float delay2Feedback1 = 1.0f, delay2Feedback2 = 1.0f;
        float delay2ToneLeft = 0.5f, delay2ToneRight = 0.5f;
        float delay2StereoSpread = 0.75f, delay2TapeDrive = 20.0f;
        int delay2Sync = 6;
        bool delay2Mono = false;
        float delay2Time = 0.2f, delay2Mix = 1.0f;
        float delay2Lfo1 = 0.0f, delay2Lfo2 = 0.0f;
        std::array<float, 5> eqFrequency { 40.0f, 110.0f, 250.0f, 850.0f, 12000.0f };
        std::array<float, 5> eqGain {};
        CompressorParameters compressor;
        int compressorPosition = 0;
        int reverbMode = 0;
        bool reverbOn = false;
        float reverbPredelayMs = 40.0f, reverbXoverHz = 360.0f;
        float reverbBassMultiplier = 1.2f, reverbDecaySeconds = 2.0f;
        float reverbDampingHz = 8000.0f, reverbWidth = 1.0f, reverbWet = 1.0f;
        float reverbEarlyLevel = 0.2f, reverbEarlyPan = -1.0f;
        float reverbEarlyRatio = 1.2f;
        CompressorParameters reverbCompressor;
        float metal1 = 0.0f, metal2 = 0.0f, shark1 = 0.0f, shark2 = 0.0f;
        float sync1 = 0.0f, sync2 = 0.0f, phaseMod12 = 0.0f, phaseMod21 = 0.0f;
        float waveModLfo1 = 0.0f, waveModLfo2 = 0.0f;
        // Legacy global depths remain readable for old projects/automation but
        // are hidden from the current UI. They are added to each source depth.
        float legacyLfoVolume1 = 0.0f, legacyLfoVolume2 = 0.0f;
        float legacyLfoLowPass1 = 0.0f, legacyLfoLowPass2 = 0.0f;
        float legacyLfoHighPass1 = 0.0f, legacyLfoHighPass2 = 0.0f;
        float legacyLfoPan1 = 0.0f, legacyLfoPan2 = 0.0f;
        float legacyLfoPitch1 = 0.0f, legacyLfoPitch2 = 0.0f;
        float legacyLfoPwm1 = 0.0f, legacyLfoPwm2 = 0.0f;
        float lfo1PitchL1 = 0.0f, lfo1PitchL2 = 0.0f;
        float lfo2PitchL1 = 0.0f, lfo2PitchL2 = 0.0f;
        float lfo1VolumeL1 = 0.0f, lfo1VolumeL2 = 0.0f, lfo1VolumeNoise = 0.0f;
        float lfo2VolumeL1 = 0.0f, lfo2VolumeL2 = 0.0f, lfo2VolumeNoise = 0.0f;
        float lfo1PanL1 = 0.0f, lfo1PanL2 = 0.0f, lfo1PanNoise = 0.0f;
        float lfo2PanL1 = 0.0f, lfo2PanL2 = 0.0f, lfo2PanNoise = 0.0f;
        float lfo1LowPassL1 = 0.0f, lfo1LowPassL2 = 0.0f, lfo1LowPassNoise = 0.0f;
        float lfo2LowPassL1 = 0.0f, lfo2LowPassL2 = 0.0f, lfo2LowPassNoise = 0.0f;
        float lfo1HighPassL1 = 0.0f, lfo1HighPassL2 = 0.0f, lfo1HighPassNoise = 0.0f;
        float lfo2HighPassL1 = 0.0f, lfo2HighPassL2 = 0.0f, lfo2HighPassNoise = 0.0f;
        float lfo1PwmL1 = 0.0f, lfo1PwmL2 = 0.0f;
        float lfo2PwmL1 = 0.0f, lfo2PwmL2 = 0.0f;
        float lfo1ModWheelAmount = 0.0f, lfo2AftertouchAmount = 0.0f;
        float morph1 = 0.75f, morph2 = 0.75f;
        float lfo1Morph1 = 0.0f, lfo1Morph2 = 0.0f;
        float lfo2Morph1 = 0.0f, lfo2Morph2 = 0.0f;
        bool formantEnabled = false;
        float formantMorph = 0.0f, lfo1FormantMorph = 0.0f;
        float lfo2FormantMorph = 0.0f, formantEnvelope = 0.0f;
        float microMotionSpeed = 0.833333f;
        float microMotionPitch1 = 0.0f, microMotionPitch2 = 0.0f;
        float microMotionPwm1 = 0.0f, microMotionPwm2 = 0.0f;
        float microMotionPan1 = 0.0f, microMotionPan2 = 0.0f;
        float microMotionLowPass = 0.0f, microMotionHighPass = 0.0f;
        float microMotionFormant = 0.0f;
        double tempoBpm = 120.0;
        LfoParameters lfo1, lfo2;
        SuperWaveParameters superWave1, superWave2;
        PitchArpParameters pitchArp1, pitchArp2;
        float pitchArpLowPassDepth = 0.0f, pitchArpHighPassDepth = 0.0f;
        float pitchArpPanDepth = 0.0f;
        LArpParameters larp;
    };

    struct RenderConstants
    {
        LfoParameters lfo1, lfo2;
        float balance1 = 0.0f, balance2 = 0.0f;
        float layer1PanLeft = 0.0f, layer1PanRight = 0.0f;
        float layer2PanLeft = 0.0f, layer2PanRight = 0.0f;
        float pwmCoefficient = 0.0f;
        float attackIncrement1 = 0.0f, decayIncrement1 = 0.0f, releaseIncrement1 = 0.0f;
        float attackIncrement2 = 0.0f, decayIncrement2 = 0.0f, releaseIncrement2 = 0.0f;
        float releaseShape = 0.0f, portamentoAmount = 0.0f;
        float stealCoefficient = 0.0f;
        float velocityCoefficient = 0.0f, velocityFilterCoefficient = 0.0f;
        float keyFollowCoefficient = 0.0f;
        double oscillatorTuning1 = 1.0, oscillatorTuning2 = 1.0;
        int staticWave1 = 0, staticWave2 = 0;
        float staticWaveBlend1 = 0.0f, staticWaveBlend2 = 0.0f;
        float lpBase = 0.0f, lpCoefficientMaximum = 0.0f, lpQ = 0.0f;
        float keyFollowSlope = 0.0f;
        float hpBase = 0.0f, hpCoefficientMaximum = 0.0f, hpQ = 0.0f;
        float inputGain = 1.0f, masterGain = 1.0f, centrePanGain = 0.0f;
        float microMotionAlpha = 0.0f;
        std::array<float, 128> noteVolumeGain {}, noteKeyFollowOctaves {}, noteHpKeyFollow {};
        bool adsr2Used = false, delayNeedsLfo = false, highPassEnabled = false;
        bool lfo1Needed = false, lfo2Needed = false;
        bool morph1Dynamic = false, morph2Dynamic = false;
        bool continuousPitchModulation = false;
        bool lowPassStatic = false, highPassStatic = false;
        bool needsEnvelope1 = false, needsEnvelope2 = false, centredFinalPan = false;
        bool velocityRuntimeNeeded = false;
        bool microMotionAny = false;
    };

    Params cachedParams {};
    RenderConstants cachedRenderConstants {};
    std::uint64_t cachedParameterRevision = 0;
    bool parameterCacheReady = false;

    // Normal MIDI uses the first 16 musical voices.  The sequencer can address
    // Layer 1 and Layer 2 independently, so it needs a second 16-slot register
    // bank to preserve the public Voices=1..16 meaning for each layer.  Noise is
    // intentionally monophonic and owns one dedicated sequencer slot.
    static constexpr int maximumVoiceCount = 16;
    static constexpr int layer2VoiceBankStart = maximumVoiceCount;
    static constexpr int noiseSequencerVoiceIndex = maximumVoiceCount * 2;
    std::array<Voice, maximumVoiceCount * 2 + 1> voices {};
    double sampleRate = 44100.0;
    std::uint64_t ageCounter = 0;
    int rolandVoice = 0;
    std::array<int, 3> sequencerRolandVoice {};
    bool sustainPedal = false;
    float pitchBend = 0.0f, modWheel = 0.0f, channelAftertouch = 0.0f;
    float globalPitchEnvelope1 = 0.0f, globalPitchEnvelope2 = 0.0f;
    float pinkLeft = 0.0f, pinkRight = 0.0f;
    float brownLeft = 0.0f, brownRight = 0.0f;
    std::uint32_t randomState = 0x1165a17u;
    LfoState lfoState1, lfoState2;
    std::array<int, 128> monoNoteStack {};
    std::array<float, 128> monoVelocityStack {};
    int monoNoteCount = 0;

    std::array<int, 16> pitchArpHeldNotes {}, pitchArpLiveSorted {}, pitchArpLiveFree {};
    std::array<int, 16> pitchArpLatchSorted {}, pitchArpLatchFree {};
    std::array<bool, 128> pitchArpKeyDown {}, pitchArpSustained {};
    int pitchArpHeldCount = 0, pitchArpLiveCount = 0, pitchArpLatchCount = 0;
    int pitchArpLiveSortedRoot = 60, pitchArpLiveFreeRoot = 60;
    int pitchArpLatchSortedRoot = 60, pitchArpLatchFreeRoot = 60;
    bool pitchArpLatchValid = false, pitchArpPoolDirty = true;
    PitchArpState pitchArpState1 {}, pitchArpState2 {};
    LArpState larpState {};
    std::array<SequencerRuntime, SequencerState::maximumSequences> sequencerRuntime {};
    int previousSequencerMode = 0;

    std::vector<float> chorusBufferLeft, chorusBufferRight;
    std::vector<float> delayBufferLeft, delayBufferRight;
    int chorusWritePosition = 0, delayWritePosition = 0;
    int activeDelayMode = -1, lwsDelayWritePosition = 0;
    bool lwsDelayMixActive = false;
    float lwsDelayTime1 = 0.0f, lwsDelayTime2 = 0.0f;
    float lwsDelayWetLp1 = 0.0f, lwsDelayWetLp2 = 0.0f;
    float lwsDelayWetHpX1 = 0.0f, lwsDelayWetHpY1 = 0.0f;
    float lwsDelayWetHpX2 = 0.0f, lwsDelayWetHpY2 = 0.0f;
    float lwsDelayFbLp1 = 0.0f, lwsDelayFbLp2 = 0.0f;
    float lwsDelayFbHpX1 = 0.0f, lwsDelayFbHpY1 = 0.0f;
    float lwsDelayFbHpX2 = 0.0f, lwsDelayFbHpY2 = 0.0f;
    float lwsDelayPan1L = 0.9807852804f, lwsDelayPan1R = 0.1950903220f;
    float lwsDelayPan2L = 0.1950903220f, lwsDelayPan2R = 0.9807852804f;
    double chorusPhase = 0.0;
    float chorusRateSmoothed = 0.0f, chorusTail = 0.0f;
    float chorusHpXLeft = 0.0f, chorusHpYLeft = 0.0f;
    float chorusHpXRight = 0.0f, chorusHpYRight = 0.0f;
    float delayTimeCurrent = 0.0f;
    float delayLpLeft = 0.0f, delayLpRight = 0.0f;
    float delayHpXLeft = 0.0f, delayHpYLeft = 0.0f;
    float delayHpXRight = 0.0f, delayHpYRight = 0.0f;
    float chorusHpCoefficient = 0.0f;
    float delayLpCoefficient = 0.0f, delayHpCoefficient = 0.0f;
    float dcCoefficient = 0.0f;
    // L1, L2, Noise, Chorus wet and Delay wet remain independent through
    // the linear pre-reverb global stage so they can later feed aux outputs.
    static constexpr std::size_t preReverbStemCount = 5;
    std::array<float, preReverbStemCount> dcXLeft {}, dcYLeft {}, dcXRight {}, dcYRight {};
    std::array<BiquadCoefficients, 6> eqCoefficients {};
    std::array<bool, 5> eqEnabled {};
    std::array<float, 5> cachedEqFrequency {}, cachedEqGain {};
    float cachedDelayTone = -1.0f;
    float cachedReverbDecay = -1.0f, cachedReverbBassMultiplier = -1.0f;
    float cachedReverbXover = -1.0f, cachedReverbDamping = -1.0f;
    bool effectCoefficientsReady = false;
    std::array<std::array<BiquadState, 6>, preReverbStemCount> eqStemLeft {}, eqStemRight {};
    CompressorState compressorState, reverbCompressorState;
    float compressorRmsCoefficient = 0.0f;
    float glueEnvelope = 0.0f;
    std::vector<float> reverbPredelayLeft, reverbPredelayRight;
    int reverbPredelayWriteLeft = 0, reverbPredelayWriteRight = 0;
    std::array<ReverbLine, 8> reverbLines {};

    // LWS-7 Open Courtyard reverb: two early-reflection buffers, eight FDN
    // lines and four short allpass diffusers sharing one circular index.
    std::array<std::vector<float>, 14> lwsReverbBuffers {};
    int lwsReverbIndex = 0, activeReverbMode = -1;
    bool lwsReverbMixActive = false;
    LwsReverbCoefficients lwsReverbCoefficients {};
    std::array<float, 8> lwsReverbFeedbackLp {};
    float lwsReverbSendLpL = 0.0f, lwsReverbSendLpR = 0.0f;
    float lwsReverbSendLowL = 0.0f, lwsReverbSendLowR = 0.0f;
    float lwsReverbBodyLpL = 0.0f, lwsReverbBodyLpR = 0.0f;
    float lwsReverbBodyLowL = 0.0f, lwsReverbBodyLowR = 0.0f;
    float lwsReverbMotionTargetA = 0.0f, lwsReverbMotionTargetB = 0.0f;
    float lwsReverbMotionA = 0.0f, lwsReverbMotionB = 0.0f;
    int lwsReverbMotionCountA = 0, lwsReverbMotionCountB = 0;
    std::uint64_t lwsReverbTailAge = 0;
    float reverbWetSmoothed = 0.0f, reverbWidthSmoothed = 0.0f;
    float reverbEarlyLevelSmoothed = 0.0f, reverbEarlyPanSmoothed = 0.0f;
    float reverbEarlyRatioSmoothed = 1.0f;
    float reverbOutputLeft = 0.0f, reverbOutputRight = 0.0f, reverbTail = 0.0f;
    std::size_t delaySilentSamples = 0;
    bool deepIdle = true;

    static float value (juce::AudioProcessorValueTreeState&, const char*);
    static Params readParams (juce::AudioProcessorValueTreeState&, double tempoBpm);
    static bool usesAdsr2 (const Params&);
    static void setVoicePerformanceTargets (Voice&, int note, float velocity,
                                            const Params&, bool instant);
    RenderConstants makeRenderConstants (const Params&) const;
    void handleMidi (const juce::MidiMessage&, const Params&, int layerMask = 7);
    void randomizeUnisonPhases (Voice&);
    void seedVoiceMicroMotion (Voice&);
    void advanceVoiceMicroMotion (Voice&, const Params&, const RenderConstants&);
    void handleMonoNoteOn (int note, float velocity, const Params&);
    void handleMonoNoteOff (int note, const Params&);
    void removeMonoNote (int note);
    void trackPitchArpMidi (const juce::MidiMessage&, const Params&);
    void removePitchArpNote (int note);
    void refreshPitchArpLiveSets();
    void buildPitchArpPool (PitchArpState&, const PitchArpParameters&);
    static void recordPitchArpSidNote (PitchArpState&, int note, bool firstNote);
    void advancePitchArps (const Params&);
    bool handleSequencerInput (const juce::MidiMessage&, int, juce::MidiBuffer&,
                               const Params&, const SequencerState&,
                               const std::array<SequencerConfig, SequencerState::maximumSequences>&,
                               int activeSequenceCount, int routingMode);
    void advanceSequencers (int sampleOffset, juce::MidiBuffer&, const Params&,
                            const SequencerState&,
                            const std::array<SequencerConfig, SequencerState::maximumSequences>&,
                            int activeSequenceCount, int routingMode);
    void triggerSequencerStep (int sequenceIndex, int sampleOffset, juce::MidiBuffer&,
                               const Params&, const SequencerState&,
                               const SequencerConfig&, int routingMode, bool previousLegato);
    void startSequencer (int sequenceIndex, int note, int velocity,
                         const SequencerConfig&);
    void stopSequencer (int sequenceIndex, int sampleOffset, juce::MidiBuffer&,
                        const Params&, int routingMode, bool clearHeld);
    void releaseSequencerNote (int sequenceIndex, int sampleOffset, juce::MidiBuffer&,
                               const Params&, int routingMode);
    void emitSequencerMessage (int sequenceIndex, const juce::MidiMessage&, int,
                               juce::MidiBuffer&, const Params&, int routingMode);
    float sequencerRandom (SequencerRuntime&) noexcept;
    int nextSequencerPosition (SequencerRuntime&, const SequencerConfig&, bool commit);
    double sequencerStepSamples (const SequencerConfig&, const Params&) const noexcept;

    void handleLArpInput (const juce::MidiMessage&, int, juce::MidiBuffer&,
                          const Params&);
    void advanceLArp (int, juce::MidiBuffer&, const Params&);
    void emitLArp (const juce::MidiMessage&, int, juce::MidiBuffer&, const Params&);
    void releaseLArpOutput (int, juce::MidiBuffer&, const Params&, bool);
    void resetLArpState (bool);
    void rebuildLArpChord (const LArpParameters&);
    int chooseLArpIndex (int, int);
    bool isLArpLegatoStep (int, int);
    float larpRandom();
    Voice* allocate (const Params&, int layerMask = 7);
    float advanceEnvelope (Voice&, const Params&, float attackIncrement,
                           float decayIncrement, float releaseIncrement,
                           float stealCoefficient);
    float advanceEnvelope2 (Voice&, const Params&, bool adsr2Used,
                            float attackIncrement, float decayIncrement,
                            float releaseIncrement) const;
    float advanceLfo (LfoState&, const LfoParameters&, float envelopeSource,
                      float crossSource);
    void retriggerLfos (const Params&);
    void render (float&, float&, std::array<float, 8>& aux,
                 const Params&, const RenderConstants&,
                 float inputLeft, float inputRight,
                 float sidechainLeft, float sidechainRight);
    void updateEffectCoefficients (const Params&);
    void processChorus (float&, float&, const Params&);
    void processDelay (float&, float&, const Params&, float lfo1, float lfo2);
    void processDelay1 (float&, float&, const Params&, float lfo1, float lfo2);
    void processLwsDelay (float&, float&, const Params&, float lfo1, float lfo2);
    void resetDelayProcessors();
    void processEqualizer (float&, float&, std::size_t stemIndex);
    float compressorScale (float detectorLeft, float detectorRight,
                           const CompressorParameters&, CompressorState&,
                           bool legacyCurve, bool halveCompressedSignal);
    void processCompressor (float&, float&, float detectorLeft, float detectorRight,
                            const CompressorParameters&, CompressorState&,
                            bool legacyCurve, bool halveCompressedSignal);
    void processReverb (float&, float&, const Params&, float sidechainLeft,
                        float sidechainRight);
    void processReverb1 (float&, float&, const Params&, float sidechainLeft,
                         float sidechainRight);
    void processLwsReverb (float&, float&, const Params&);
    void resetReverbProcessors();
    void resetLwsReverbState();
    void updateLwsReverbCoefficients (const Params&);

    static BiquadCoefficients makeLowPass (double sampleRate, float frequency, float q);
    static BiquadCoefficients makeHighPass (double sampleRate, float frequency, float q);
    static BiquadCoefficients makePeak (double sampleRate, float frequency,
                                        float gainDb, float q);
    static BiquadCoefficients makeHighShelf (double sampleRate, float frequency,
                                             float gainDb, float slope);
    static FormantCoefficients makeFormants (double sampleRate, float position);
    static float processBiquad (float input, BiquadState&, const BiquadCoefficients&);
    static float polyBlep (double, double);
    static float oscillator (int, double, double, float, float&, float, float, float);
    static float morphOscillator (float, double, double, float, float&, float, float, float);
    static float superWaveOscillator (const SuperWaveParameters&, double, double, float,
                                      float&, float, float, float, float);
    static float modulationOscillator (int, double, float);
    static float modulationMorph (float, double, float);
    static float modulationSuperWave (const SuperWaveParameters&, double, double, float,
                                      float&, float, float, float, float);
    static float morphPmGain (float);
    static float superWavePmGain (const SuperWaveParameters&);
    static float panGain (float, bool);
    void enterDeepIdle();
    float randomSigned();
    std::array<float, 2> renderNoisePair (Voice&, int noiseType, float colour,
                                          double pitchClockIncrement);
};
}
