// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GeneratedParameters.h"
#include "LinkedFilterCutoff.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>

// REAPER VST3 host extension, ported from LSampler-24 TEST187.  We query
// envelope ACTIVE (not VISIBLE): hiding a lane is not deactivation.
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wnon-virtual-dtor")
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
JUCE_END_IGNORE_WARNINGS_GCC_LIKE

namespace ljuno_reaper
{
using namespace Steinberg;
class IReaperHostApplication : public FUnknown
{
public:
    virtual void* PLUGIN_API getReaperApi(CStringA name) = 0;
    virtual void* PLUGIN_API getReaperParent(uint32 type) = 0;
    virtual void* PLUGIN_API reaperExtended(uint32 call, void*, void*, void*) = 0;
    static const FUID iid;
};
DECLARE_CLASS_IID(IReaperHostApplication, 0x79655E36, 0x77EE4267, 0xA573FEF7, 0x4912C27C)
DEF_CLASS_IID(IReaperHostApplication)
}

class LJuno116ReaperHostExtension final : public juce::VST3ClientExtensions
{
public:
    ~LJuno116ReaperHostExtension() override
    {
        if (host != nullptr) host->release();
    }
    void setIHostApplication(Steinberg::FUnknown* application) override
    {
        if (host != nullptr) { host->release(); host = nullptr; }
        if (application == nullptr) return;
        void* result = nullptr;
        if (application->queryInterface(ljuno_reaper::IReaperHostApplication::iid, &result)
            == Steinberg::kResultOk)
            host = static_cast<ljuno_reaper::IReaperHostApplication*>(result);
    }
    bool available() const noexcept { return host != nullptr; }
    int getActive(const LJuno116AudioProcessor& processor, juce::AudioProcessorParameter* param) const
    {
        const auto fx = findFocusedFx(processor, param);
        if (!fx) return -1;
        const auto getEnvelope = getApi<void* (*)(void*, int, int, bool)>("GetFXEnvelope");
        const auto attribute = getApi<bool (*)(void*, const char*, char*, bool)>("GetSetEnvelopeInfo_String");
        if (!getEnvelope || !attribute) return -1;
        auto* env = getEnvelope(fx.track, fx.index, fx.parameter, false);
        if (env == nullptr) return 0;
        char text[32] = {};
        if (!attribute(env, "ACTIVE", text, false)) return -1;
        return text[0] == '1' ? 1 : text[0] == '0' ? 0 : -1;
    }
    bool toggle(LJuno116AudioProcessor& processor, juce::AudioProcessorParameter* param) const
    {
        const auto fx = findFocusedFx(processor, param);
        if (!fx) return false;
        const auto getEnvelope = getApi<void* (*)(void*, int, int, bool)>("GetFXEnvelope");
        const auto attribute = getApi<bool (*)(void*, const char*, char*, bool)>("GetSetEnvelopeInfo_String");
        if (!getEnvelope || !attribute) return false;
        void* env = getEnvelope(fx.track, fx.index, fx.parameter, false);
        bool wasActive = false;
        if (env != nullptr)
        {
            char active[32] = {};
            if (!attribute(env, "ACTIVE", active, false)
                || (active[0] != '0' && active[0] != '1')) return false;
            wasActive = active[0] == '1';
        }
        if (!wasActive && env == nullptr)
            env = getEnvelope(fx.track, fx.index, fx.parameter, true);
        if (env == nullptr) return false;
        auto set = [attribute, env](const char* name, const char* value)
        {
            char text[2] { value[0], 0 };
            return attribute(env, name, text, true);
        };
        if (!set("ACTIVE", wasActive ? "0" : "1")) return false;
        if (wasActive)
        {
            set("ARM", "0");
            set("VISIBLE", "0");
        }
        else
        {
            set("VISIBLE", "1");
            set("ARM", "1");
        }
        if (const auto adjust = getApi<void (*)(bool)>("TrackList_AdjustWindows")) adjust(false);
        if (const auto update = getApi<void (*)()>("UpdateArrange")) update();
        return true;
    }
private:
    template <class T> T getApi(const char* name) const
    {
        return host != nullptr ? reinterpret_cast<T>(host->getReaperApi(name)) : nullptr;
    }
    struct FocusedFx
    {
        void* track = nullptr;
        int index = -1, parameter = -1;
        explicit operator bool() const { return track != nullptr && index >= 0 && parameter >= 0; }
    };
    FocusedFx findFocusedFx(const LJuno116AudioProcessor& processor, juce::AudioProcessorParameter* param) const
    {
        if (host == nullptr || param == nullptr) return {};
        // Guard against touching another instance, project, take, or track.
        auto* owner = host->getReaperParent(1);
        if (owner == nullptr) return {};
        const auto focused = getApi<bool (*)(int, int*, int*, int*, int*, int*)>("GetTouchedOrFocusedFX");
        const auto getTrack = getApi<void* (*)(void*, int)>("GetTrack");
        const auto getMaster = getApi<void* (*)(void*)>("GetMasterTrack");
        const auto getParamName = getApi<bool (*)(void*, int, int, char*, int)>("TrackFX_GetParamName");
        if (!focused || !getTrack || !getMaster || !getParamName) return {};
        int trackIdx = -2, itemIdx = -2, takeIdx = -2, fxIdx = -1, ignored = -1;
        if (!focused(1, &trackIdx, &itemIdx, &takeIdx, &fxIdx, &ignored)
            || itemIdx != -1 || fxIdx < 0) return {};
        void* focusedTrack = trackIdx == -1 ? getMaster(nullptr) : getTrack(nullptr, trackIdx);
        if (focusedTrack == nullptr || focusedTrack != owner) return {};
        const int parameter = processor.getParameters().indexOf(param);
        if (parameter < 0) return {};
        char name[512] = {};
        if (!getParamName(owner, fxIdx, parameter, name, int(sizeof(name)))) return {};
        if (!juce::String::fromUTF8(name).trim().equalsIgnoreCase(param->getName(512).trim())) return {};
        return { owner, fxIdx, parameter };
    }
    ljuno_reaper::IReaperHostApplication* host = nullptr;
};

juce::VST3ClientExtensions* LJuno116AudioProcessor::getVST3ClientExtensions()
{
    return reaperHostExtension.get();
}
bool LJuno116AudioProcessor::hasReaperEnvelopeApi() const noexcept
{
    return reaperHostExtension != nullptr && reaperHostExtension->available();
}
int LJuno116AudioProcessor::getReaperEnvelopeActive(juce::AudioProcessorParameter* p) const
{
    return reaperHostExtension != nullptr ? reaperHostExtension->getActive(*this, p) : -1;
}
bool LJuno116AudioProcessor::toggleReaperEnvelopeActive(juce::AudioProcessorParameter* p)
{
    return reaperHostExtension != nullptr && reaperHostExtension->toggle(*this, p);
}


LJuno116AudioProcessor::LJuno116AudioProcessor (juce::File presetLibraryRoot)
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
          .withOutput ("Master 1-2", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Aux 3-4", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Aux 5-6", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Aux 7-8", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Aux 9-10", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "LJuno116State", ljuno::createParameterLayout()),
      presetManager (parameters, std::move (presetLibraryRoot),
                     [this] { return combinedSequencerData(); },
                     [this] (const juce::String& data) { restoreSequencerData (data); })
{
    reaperHostExtension = std::make_unique<LJuno116ReaperHostExtension>();
    for (const auto& parameter : ljuno::generated::parameters)
        parameters.addParameterListener (parameter.id, this);
    syncSequencerBankToParameters();
    syncLinkedFilterCutoff (false);
}

LJuno116AudioProcessor::~LJuno116AudioProcessor()
{
    for (const auto& parameter : ljuno::generated::parameters)
        parameters.removeParameterListener (parameter.id, this);
}

void LJuno116AudioProcessor::parameterChanged (const juce::String& id, float newValue)
{
    parameterRevision.fetch_add (1, std::memory_order_relaxed);

    if (id == "slider015" || id == "slider017" || id == "slider466")
    {
        syncLinkedFilterCutoff (id == "slider466", newValue);
        return;
    }

    if (synchronisingSequencerBank.load (std::memory_order_relaxed))
        return;

    const auto sliderNumber = id.startsWith ("slider") ? id.substring (6).getIntValue() : -1;
    if ((sliderNumber >= 313 && sliderNumber <= 335) || (sliderNumber == 364 || sliderNumber == 452)
        || (sliderNumber >= 443 && sliderNumber <= 445))
    {
        handleSequencerParameterChanged (id, newValue);
        return;
    }

    if (synchronisingMorph.exchange (true))
        return;

    const auto setPlainValue = [this] (const char* targetId, float plainValue)
    {
        if (auto* target = parameters.getParameter (targetId))
            target->setValueNotifyingHost (target->convertTo0to1 (plainValue));
    };

    if (id == "slider243" && newValue >= 0.5f)
    {
        // LArp's own one-shot Init mirrors the defaults in the fused JSFX.
        // LArp Routing and MIDI Channel deliberately remain unchanged.
        const std::array<std::pair<const char*, float>, 35> defaults {{
            { "slider203", 4.0f }, { "slider204", 0.0f },
            { "slider205", 0.0f }, { "slider206", 0.0f },
            { "slider207", 0.5f }, { "slider208", 0.0f },
            { "slider209", 0.0f }, { "slider210", 0.0f },
            { "slider211", 0.0f }, { "slider212", 0.0f },
            { "slider213", 0.0f }, { "slider214", 1.0f },
            { "slider215", 1.0f }, { "slider216", 0.0f },
            { "slider217", 0.0f }, { "slider218", 0.0f },
            { "slider219", 0.0f }, { "slider234", 0.0f },
            { "slider235", 0.5f }, { "slider236", 0.0f },
            { "slider237", 0.0f }, { "slider238", 0.0f },
            { "slider239", 0.0f }, { "slider240", 0.0f },
            { "slider241", 0.5f }, { "slider242", 1.0f },
            { "slider244", 0.0f }, { "slider247", 0.0f },
            { "slider248", 0.0f }, { "slider249", 0.0f },
            { "slider250", 0.0f }, { "slider251", 0.0f },
            { "slider252", 0.0f }, { "slider253", 0.0f },
            { "slider254", 0.0f }
        }};
        for (const auto& setting : defaults)
            setPlainValue (setting.first, setting.second);
        setPlainValue ("slider243", 0.0f);
    }
    else if (id == "slider003" && newValue < 4.5f)
        setPlainValue ("slider097", juce::roundToInt (newValue) * 0.25f);
    else if (id == "slider052" && newValue < 4.5f)
        setPlainValue ("slider098", juce::roundToInt (newValue) * 0.25f);
    else if (id == "slider097")
    {
        if (const auto* wave = parameters.getRawParameterValue ("slider003");
            wave != nullptr && wave->load() < 4.5f)
            setPlainValue ("slider003", static_cast<float> (juce::roundToInt (
                juce::jlimit (0.0f, 1.0f, newValue) * 4.0f)));
    }
    else if (id == "slider098")
    {
        if (const auto* wave = parameters.getRawParameterValue ("slider052");
            wave != nullptr && wave->load() < 4.5f)
            setPlainValue ("slider052", static_cast<float> (juce::roundToInt (
                juce::jlimit (0.0f, 1.0f, newValue) * 4.0f)));
    }

    synchronisingMorph.store (false);
}


void LJuno116AudioProcessor::syncLinkedFilterCutoff (bool move, float target)
{
    if (synchronisingLinkedFilter.exchange (true))
        return;
    const auto rate = getSampleRate();
    const ljuno::linkedFilter::Scale scale (rate > 0.0 ? rate : 48000.0);
    const auto current = [this] { return ljuno::linkedFilter::Cutoffs {
        getPlainParameterValue ("slider015"), getPlainParameterValue ("slider017") }; };
    const auto set = [this] (const char* id, float plain)
    {
        if (auto* parameter = parameters.getParameter (id))
        {
            const auto normalised = parameter->convertTo0to1 (plain);
            if (std::abs (parameter->getValue() - normalised) > 0.000001f)
                parameter->setValueNotifyingHost (normalised);
        }
    };
    if (move)
    {
        // Retain sub-step precision between macro moves so repeated small
        // moves cannot accumulate the quantisation error of the old sliders.
        const auto next = scale.move ({ linkedFilterAnchorLp, linkedFilterAnchorHp }, target);
        linkedFilterAnchorLp = next.lp;
        linkedFilterAnchorHp = next.hp;
        set ("slider015", next.lp);
        set ("slider017", next.hp);
    }
    else
    {
        const auto pair = current();
        linkedFilterAnchorLp = pair.lp;
        linkedFilterAnchorHp = pair.hp;
    }
    // Re-read the actual snapped host values, not the requested position.
    set ("slider466", scale.centre (current()));
    synchronisingLinkedFilter.store (false);
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

namespace
{
using SeqParam = ljuno::SequencerState::ConfigParameter;

bool sequencerConfigParameterForId (const juce::String& id, SeqParam& parameter)
{
    if (id == "slider315") parameter = SeqParam::startStep;
    else if (id == "slider316") parameter = SeqParam::endStep;
    else if (id == "slider317") parameter = SeqParam::bpmDivision;
    else if (id == "slider318") parameter = SeqParam::playbackMode;
    else if (id == "slider319") parameter = SeqParam::shuffle;
    else if (id == "slider320") parameter = SeqParam::noteSkipProbability;
    else if (id == "slider321") parameter = SeqParam::noteLengthRandomDepth;
    else if (id == "slider322") parameter = SeqParam::legato;
    else if (id == "slider323") parameter = SeqParam::noteRandomDepth;
    else if (id == "slider324") parameter = SeqParam::velocityRandomDepth;
    else if (id == "slider325") parameter = SeqParam::repeatRandomDepth;
    else if (id == "slider326") parameter = SeqParam::octaveShift;
    else if (id == "slider327") parameter = SeqParam::semitoneShift;
    else if (id == "slider328") parameter = SeqParam::globalStepLength;
    else if (id == "slider329") parameter = SeqParam::globalStepVelocity;
    else if (id == "slider330") parameter = SeqParam::globalStepRepeat;
    else if (id == "slider331") parameter = SeqParam::globalStepShift;
    else if (id == "slider332") parameter = SeqParam::midiInputMode;
    else if (id == "slider333") parameter = SeqParam::midiChannel;
    else if (id == "slider334") parameter = SeqParam::launchStep;
    else if (id == "slider335") parameter = SeqParam::launchOffsetMs;
    else return false;
    return true;
}

const std::array<std::pair<const char*, SeqParam>, 21> sequencerBankParameters {{
    { "slider315", SeqParam::startStep },
    { "slider316", SeqParam::endStep },
    { "slider317", SeqParam::bpmDivision },
    { "slider318", SeqParam::playbackMode },
    { "slider319", SeqParam::shuffle },
    { "slider320", SeqParam::noteSkipProbability },
    { "slider321", SeqParam::noteLengthRandomDepth },
    { "slider322", SeqParam::legato },
    { "slider323", SeqParam::noteRandomDepth },
    { "slider324", SeqParam::velocityRandomDepth },
    { "slider325", SeqParam::repeatRandomDepth },
    { "slider326", SeqParam::octaveShift },
    { "slider327", SeqParam::semitoneShift },
    { "slider328", SeqParam::globalStepLength },
    { "slider329", SeqParam::globalStepVelocity },
    { "slider330", SeqParam::globalStepRepeat },
    { "slider331", SeqParam::globalStepShift },
    { "slider332", SeqParam::midiInputMode },
    { "slider333", SeqParam::midiChannel },
    { "slider334", SeqParam::launchStep },
    { "slider335", SeqParam::launchOffsetMs }
}};
}

void LJuno116AudioProcessor::setPlainParameterValue (const char* parameterId, float value)
{
    if (auto* parameter = parameters.getParameter (parameterId))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

int LJuno116AudioProcessor::getAvailableSequencerCount() const noexcept
{
    // LJuno exposes three fixed sequencer lanes: Sequence 1 drives Layer 1,
    // Sequence 2 drives Layer 2, and Sequence 3 drives the Noise engine.
    // The Voices parameter remains only the synth polyphony control.
    return 3;
}

int LJuno116AudioProcessor::getSelectedSequencerIndex() const noexcept
{
    return juce::jlimit (0, getAvailableSequencerCount() - 1,
                         sequencerState.getSelectedSequence());
}

int LJuno116AudioProcessor::getSequencerPattern (int track) const noexcept
{
    return sequencerState.getSelectedPattern (track);
}

int LJuno116AudioProcessor::getActiveSequenceSlot (int track) const noexcept
{
    return sequencerState.getActiveSequenceIndex (track);
}

void LJuno116AudioProcessor::selectSequencerPatternFromEditor (int track, int pattern)
{
    static constexpr std::array<const char*, 3> ids { "slider443", "slider444", "slider445" };
    const auto lane = juce::jlimit (0, 2, track);
    setPlainParameterValue (ids[static_cast<std::size_t> (lane)],
                            static_cast<float> (juce::jlimit (1, ljuno::SequencerState::patternsPerTrack, pattern)));
}

ljuno::SequencerConfig LJuno116AudioProcessor::getSequencerConfig (int sequence) const noexcept
{
    return sequencerState.getConfig (getActiveSequenceSlot (sequence));
}

float LJuno116AudioProcessor::getSequencerStepValue (int sequence, int step,
                                                      ljuno::SequencerLayer layer) const noexcept
{
    return sequencerState.getStepValue (getActiveSequenceSlot (sequence), step, layer);
}

void LJuno116AudioProcessor::setSequencerStepValue (int sequence, int step,
                                                     ljuno::SequencerLayer layer,
                                                     float value) noexcept
{
    sequencerState.setStepValue (getActiveSequenceSlot (sequence), step, layer, value);
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::addSequencerStepDelta (int sequence, int step,
                                                     ljuno::SequencerLayer layer,
                                                     float delta) noexcept
{
    sequencerState.addStepDelta (getActiveSequenceSlot (sequence), step, layer, delta);
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

int LJuno116AudioProcessor::getSequencerStepParameterCount (int sequence, int step) const noexcept
{
    return sequencerState.getStepParameterCount (getActiveSequenceSlot (sequence), step);
}

ljuno::SequencerParameterLock LJuno116AudioProcessor::getSequencerStepParameterLock (
    int sequence, int step, int lockIndex) const noexcept
{
    return sequencerState.getStepParameterLock (getActiveSequenceSlot (sequence), step, lockIndex);
}

int LJuno116AudioProcessor::findSequencerStepParameterLock (int sequence, int step,
                                                             int sliderNumber) const noexcept
{
    return sequencerState.findStepParameterLock (getActiveSequenceSlot (sequence), step, sliderNumber);
}

int LJuno116AudioProcessor::addSequencerStepParameterLock (int sequence, int step,
                                                            int sliderNumber, float value) noexcept
{
    const auto index = sequencerState.addStepParameterLock (getActiveSequenceSlot (sequence), step, sliderNumber, value);
    if (index >= 0)
        parameterRevision.fetch_add (1, std::memory_order_relaxed);
    return index;
}

bool LJuno116AudioProcessor::setSequencerStepParameterLockValue (int sequence, int step,
                                                                 int lockIndex, float value) noexcept
{
    const auto changed = sequencerState.setStepParameterLockValue (getActiveSequenceSlot (sequence), step, lockIndex, value);
    if (changed)
        parameterRevision.fetch_add (1, std::memory_order_relaxed);
    return changed;
}

bool LJuno116AudioProcessor::removeSequencerStepParameterLock (int sequence, int step,
                                                               int lockIndex) noexcept
{
    const auto changed = sequencerState.removeStepParameterLock (getActiveSequenceSlot (sequence), step, lockIndex);
    if (changed)
        parameterRevision.fetch_add (1, std::memory_order_relaxed);
    return changed;
}

float LJuno116AudioProcessor::getPlainParameterValue (const char* parameterId) const noexcept
{
    if (const auto* raw = parameters.getRawParameterValue (parameterId))
        return raw->load();
    return 0.0f;
}

void LJuno116AudioProcessor::selectSequencerFromEditor (int sequence)
{
    const auto clamped = juce::jlimit (0, getAvailableSequencerCount() - 1, sequence);
    setPlainParameterValue ("slider314", static_cast<float> (clamped + 1));
}

void LJuno116AudioProcessor::requestSequencerPreview (int sequence, bool start) noexcept
{
    synthEngine.requestSequencerPreview (sequence, start);
    // Wake processBlock even when the synth was in deep idle.
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::requestSequencerStepAudition (
    int lane, ljuno::StepAuditionRange range) noexcept
{
    if (isHostTransportPlaying()) return;
    synthEngine.requestSequencerStepAudition (lane, range);
    // Wake the audio thread even when the synth was deep-idle.
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

LJuno116AudioProcessor::SequencerEditSnapshot
LJuno116AudioProcessor::captureSequencerEditSnapshot() const
{
    SequencerEditSnapshot snapshot;
    snapshot.data = sequencerState.serialiseToBase64 (false);
    static constexpr std::array<const char*, 3> sourceIds { "slider392", "slider393", "slider394" };
    for (std::size_t i = 0; i < sourceIds.size(); ++i)
        snapshot.noteSources[i] = getPlainParameterValue (sourceIds[i]);
    return snapshot;
}

bool LJuno116AudioProcessor::sequencerSnapshotsEqual (const SequencerEditSnapshot& a,
                                                        const SequencerEditSnapshot& b) noexcept
{
    if (a.data != b.data)
        return false;
    for (std::size_t i = 0; i < a.noteSources.size(); ++i)
        if (std::abs (a.noteSources[i] - b.noteSources[i]) > 1.0e-7f)
            return false;
    return true;
}

void LJuno116AudioProcessor::trimSequencerHistory (std::vector<SequencerEditSnapshot>& history)
{
    if (history.size() > maximumSequencerUndoStates)
        history.erase (history.begin(),
                       history.begin() + static_cast<std::ptrdiff_t> (history.size() - maximumSequencerUndoStates));
}

void LJuno116AudioProcessor::commitSequencerEditSnapshot (SequencerEditSnapshot before)
{
    const auto after = captureSequencerEditSnapshot();
    if (sequencerSnapshotsEqual (before, after))
        return;

    if (sequencerUndoHistory.empty()
        || ! sequencerSnapshotsEqual (sequencerUndoHistory.back(), before))
        sequencerUndoHistory.push_back (std::move (before));
    trimSequencerHistory (sequencerUndoHistory);
    sequencerRedoHistory.clear();
}

bool LJuno116AudioProcessor::restoreSequencerEditSnapshot (const SequencerEditSnapshot& snapshot)
{
    const auto selected = getSelectedSequencerIndex();
    if (! sequencerState.restoreFromBase64 (snapshot.data))
        return false;

    sequencerState.setSelectedSequence (selected);
    static constexpr std::array<const char*, 3> sourceIds { "slider392", "slider393", "slider394" };
    for (std::size_t i = 0; i < sourceIds.size(); ++i)
        setPlainParameterValue (sourceIds[i], snapshot.noteSources[i]);

    syncSequencerBankToParameters();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
    return true;
}

bool LJuno116AudioProcessor::undoSequencerEdit()
{
    if (sequencerUndoHistory.empty())
        return false;

    auto current = captureSequencerEditSnapshot();
    auto target = std::move (sequencerUndoHistory.back());
    sequencerUndoHistory.pop_back();
    sequencerRedoHistory.push_back (std::move (current));
    trimSequencerHistory (sequencerRedoHistory);
    if (restoreSequencerEditSnapshot (target))
        return true;

    sequencerRedoHistory.pop_back();
    sequencerUndoHistory.push_back (std::move (target));
    return false;
}

bool LJuno116AudioProcessor::redoSequencerEdit()
{
    if (sequencerRedoHistory.empty())
        return false;

    auto current = captureSequencerEditSnapshot();
    auto target = std::move (sequencerRedoHistory.back());
    sequencerRedoHistory.pop_back();
    sequencerUndoHistory.push_back (std::move (current));
    trimSequencerHistory (sequencerUndoHistory);
    if (restoreSequencerEditSnapshot (target))
        return true;

    sequencerUndoHistory.pop_back();
    sequencerRedoHistory.push_back (std::move (target));
    return false;
}

namespace
{
const juce::String ljunoPatternClipboardHeader = "LJUNO116-PATTERN-V1:";
}

bool LJuno116AudioProcessor::hasSequencerClipboard() const
{
    return juce::SystemClipboard::getTextFromClipboard()
        .startsWith (ljunoPatternClipboardHeader);
}

bool LJuno116AudioProcessor::copyCurrentSequencer()
{
    // Only ONE pattern from the active track is staged in slot 0.
    // This deliberately transfers only the active pattern, not the other 63.
    auto staged = std::make_unique<ljuno::SequencerState>();
    staged->copySequenceFrom (sequencerState,
                              getActiveSequenceSlot (getSelectedSequencerIndex()), 0);
    juce::SystemClipboard::copyTextToClipboard (
        ljunoPatternClipboardHeader + staged->serialiseToBase64 (false));
    return true;
}

bool LJuno116AudioProcessor::cutCurrentSequencer()
{
    if (! copyCurrentSequencer())
        return false;
    auto before = captureSequencerEditSnapshot();
    const auto sequence = getSelectedSequencerIndex();
    // Cutting pattern notes must not change the independent MIDI keyboard mode.
    const auto oldPolyphony = sequencerState.getConfigValue (
        getActiveSequenceSlot (sequence), ljuno::SequencerState::ConfigParameter::midiInputPolyphony);
    sequencerState.resetSequence (getActiveSequenceSlot (sequence));
    if (sequence < 2)
        sequencerState.setConfigValue (getActiveSequenceSlot (sequence),
            ljuno::SequencerState::ConfigParameter::midiInputPolyphony, oldPolyphony);
    syncSequencerBankToParameters();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
    commitSequencerEditSnapshot (std::move (before));
    return true;
}

bool LJuno116AudioProcessor::pasteCurrentSequencer()
{
    const auto text = juce::SystemClipboard::getTextFromClipboard();
    if (! text.startsWith (ljunoPatternClipboardHeader) || text.length() > 4 * 1024 * 1024)
        return false;

    // Validate the entire incoming clipboard BEFORE mutating the destination.
    auto staged = std::make_unique<ljuno::SequencerState>();
    if (! staged->restoreFromBase64 (
            text.substring (ljunoPatternClipboardHeader.length()).trim()))
        return false;

    auto before = captureSequencerEditSnapshot();
    const auto destination = getSelectedSequencerIndex();
    sequencerState.copySequenceFrom (*staged, 0, getActiveSequenceSlot (destination));
    syncSequencerBankToParameters();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
    commitSequencerEditSnapshot (std::move (before));
    return true;
}

bool LJuno116AudioProcessor::nudgeSequencerPageParameter (const juce::String& parameterId,
                                                          float delta)
{
    if (auto* parameter = parameters.getParameter (parameterId))
    {
        const auto currentNormalised = parameter->getValue();
        const auto current = parameter->convertFrom0to1 (currentNormalised);
        const auto targetNormalised = juce::jlimit (0.0f, 1.0f,
            parameter->convertTo0to1 (current + delta));

        if (std::abs (targetNormalised - currentNormalised) <= 1.0e-7f)
            return false;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (targetNormalised);
        parameter->endChangeGesture();
        return true;
    }

    return false;
}

void LJuno116AudioProcessor::resetSequencerState()
{
    sequencerState.reset();
    sequencerUndoHistory.clear();
    sequencerRedoHistory.clear();
    syncSequencerBankToParameters();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}


void LJuno116AudioProcessor::resetSequencerSequence (int sequence)
{
    sequencerState.resetSequence (getActiveSequenceSlot (sequence));
    if (sequence == getSelectedSequencerIndex())
        syncSequencerBankToParameters();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::resetSequencerStep (int sequence, int step)
{
    sequencerState.resetStep (getActiveSequenceSlot (sequence), step);
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

ljuno::SequencerStep LJuno116AudioProcessor::getSequencerStep (int sequence, int step) const noexcept
{
    return sequencerState.getStep (getActiveSequenceSlot (sequence), step);
}

void LJuno116AudioProcessor::replaceSequencerStep (int sequence, int step,
                                                   const ljuno::SequencerStep& replacement) noexcept
{
    sequencerState.replaceStep (getActiveSequenceSlot (sequence), step, replacement);
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

juce::String LJuno116AudioProcessor::combinedSequencerData() const
{
    return sequencerState.serialiseToBase64() + "|LJUNO-SONG-1|" + songs.serialise();
}

void LJuno116AudioProcessor::restoreSequencerData (const juce::String& data)
{
    // Preset/project restoration starts a new edit history.  The runtime undo
    // stack deliberately survives editor close/reopen, but never crosses a
    // loaded preset or restored DAW state.
    sequencerUndoHistory.clear();
    sequencerRedoHistory.clear();

    // Routing is an automatable VST parameter and is therefore authoritative.
    // SequencerData also contains a historical copy; preserve the parameter so
    // legacy data cannot overwrite the migrated 3-choice routing value.
    const auto routingMode = parameters.getRawParameterValue ("slider313") != nullptr
        ? juce::roundToInt (parameters.getRawParameterValue ("slider313")->load())
        : sequencerState.getRoutingMode();
    const auto split = data.indexOf ("|LJUNO-SONG-1|");
    const auto stepData = split < 0 ? data : data.substring (0, split);
    if (! sequencerState.restoreFromBase64 (stepData))
        sequencerState.reset();
    if (split < 0 || ! songs.restore (data.substring (split + 14)))
        songs.reset();
    sequencerState.setRoutingMode (routingMode);
    const auto maximum = getAvailableSequencerCount();
    if (sequencerState.getSelectedSequence() >= maximum)
        sequencerState.setSelectedSequence (maximum - 1);
    syncSequencerBankToParameters();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::syncSequencerBankToParameters()
{
    synchronisingSequencerBank.store (true, std::memory_order_relaxed);

    const auto maximum = getAvailableSequencerCount();
    currentSequenceBankIndex = juce::jlimit (0, maximum - 1,
                                             sequencerState.getSelectedSequence());
    sequencerState.setSelectedSequence (currentSequenceBankIndex);

    setPlainParameterValue ("slider313", static_cast<float> (sequencerState.getRoutingMode()));
    setPlainParameterValue ("slider314", static_cast<float> (currentSequenceBankIndex + 1));
    static constexpr std::array<const char*, 3> patternIds { "slider443", "slider444", "slider445" };
    for (int lane = 0; lane < 3; ++lane)
        setPlainParameterValue (patternIds[static_cast<std::size_t> (lane)],
                                static_cast<float> (sequencerState.getSelectedPattern (lane)));
    for (const auto& bank : sequencerBankParameters)
        setPlainParameterValue (bank.first,
                                sequencerState.getConfigValue (getActiveSequenceSlot (currentSequenceBankIndex),
                                                               bank.second));
    // These are fixed, automatable per-layer controls, never track-banked.
    setPlainParameterValue ("slider364", sequencerState.getConfigValue (
        getActiveSequenceSlot (0), SeqParam::midiInputPolyphony));
    setPlainParameterValue ("slider452", sequencerState.getConfigValue (
        getActiveSequenceSlot (1), SeqParam::midiInputPolyphony));
    synchronisingSequencerBank.store (false, std::memory_order_relaxed);
}

void LJuno116AudioProcessor::handleSequencerParameterChanged (const juce::String& id,
                                                               float newValue)
{
    if (id == "slider313")
    {
        sequencerState.setRoutingMode (juce::roundToInt (newValue));
        return;
    }

    if (id == "slider314")
    {
        const auto requested = juce::roundToInt (newValue) - 1;
        const auto clamped = juce::jlimit (0, getAvailableSequencerCount() - 1, requested);
        sequencerState.setSelectedSequence (clamped);
        currentSequenceBankIndex = clamped;
        syncSequencerBankToParameters();
        return;
    }

    if (id == "slider443" || id == "slider444" || id == "slider445")
    {
        const auto lane = id == "slider443" ? 0 : id == "slider444" ? 1 : 2;
        sequencerState.setSelectedPattern (lane, juce::roundToInt (newValue));
        if (lane == currentSequenceBankIndex)
            syncSequencerBankToParameters();
        parameterRevision.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    if (id == "slider364" || id == "slider452")
    {
        const auto lane = id == "slider364" ? 0 : 1;
        sequencerState.setConfigValue (getActiveSequenceSlot (lane),
                                      SeqParam::midiInputPolyphony, newValue);
        return;
    }

    SeqParam parameter;
    if (! sequencerConfigParameterForId (id, parameter))
        return;

    sequencerState.setConfigValue (getActiveSequenceSlot (currentSequenceBankIndex), parameter, newValue);

    // These controls can clamp a paired value: Start/End constrain each other,
    // and Launch Step 1 deliberately forbids a negative millisecond offset.
    if (id == "slider315" || id == "slider316" || id == "slider334" || id == "slider335")
        syncSequencerBankToParameters();
}

void LJuno116AudioProcessor::prepareToPlay (double sampleRate, int maximumBlockSize)
{
    synthEngine.prepare (sampleRate);
    syncLinkedFilterCutoff (false);
    hostPlayLfoReset.clear();
    sidechainScratch.setSize (2, std::max (1, maximumBlockSize), false, false, true);
    sidechainScratch.clear();
    lastProcessedParameterRevision = parameterRevision.load (std::memory_order_relaxed);
}
void LJuno116AudioProcessor::releaseResources() {}

bool LJuno116AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto sidechain = layouts.getChannelSet (true, 1);
    if (! (mainInput.isDisabled() || mainInput == juce::AudioChannelSet::stereo())
        || ! (sidechain.isDisabled() || sidechain == juce::AudioChannelSet::stereo()))
        return false;

    // Master must remain stereo. Auxiliary buses may be disabled by the host,
    // but when enabled each is always stereo, matching LR-608's host-friendly
    // multichannel layout handling.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (int bus = 1; bus < getBusCount (false); ++bus)
    {
        const auto channels = layouts.getChannelSet (false, bus);
        if (! (channels.isDisabled() || channels == juce::AudioChannelSet::stereo()))
            return false;
    }
    return true;
}

void LJuno116AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto hasStereoInput = getBusCount (true) > 0
                             && getChannelCountOfBus (true, 0) >= 2;
    const auto hasSidechain = getBusCount (true) > 1
                           && getChannelCountOfBus (true, 1) >= 2;
    const float* sidechainLeft = nullptr;
    const float* sidechainRight = nullptr;
    if (hasSidechain)
    {
        const auto sidechain = getBusBuffer (buffer, true, 1);
        if (sidechainScratch.getNumSamples() < buffer.getNumSamples())
            sidechainScratch.setSize (2, buffer.getNumSamples(), false, false, true);
        sidechainScratch.copyFrom (0, 0, sidechain, 0, 0, buffer.getNumSamples());
        sidechainScratch.copyFrom (1, 0, sidechain, 1, 0, buffer.getNumSamples());
        sidechainLeft = sidechainScratch.getReadPointer (0);
        sidechainRight = sidechainScratch.getReadPointer (1);
    }

    auto inputIsSilent = true;
    if (hasStereoInput)
    {
        const auto mainInput = getBusBuffer (buffer, true, 0);
        for (int channel = 0; channel < std::min (2, mainInput.getNumChannels()); ++channel)
            inputIsSilent = inputIsSilent
                         && mainInput.getMagnitude (channel, 0, mainInput.getNumSamples()) == 0.0f;
    }

    // Acquire MIDI Step Entry notes before dispatching any MIDI to the synth.
    // Forward all non-note events unchanged; the editor consumes the lock-free
    // queue on the message thread and performs undoable pattern edits there.
    if (midiStepEntry.needsFiltering() && ! midi.isEmpty())
    {
        juce::MidiBuffer remainingMidi;
        for (const auto event : midi)
            if (! midiStepEntry.consume (event.getMessage()))
                remainingMidi.addEvent (event.getMessage(), event.samplePosition);
        midi.swapWith (remainingMidi);
    }
    // TEST121: detect DAW Play before the deep-idle return, otherwise a silent
    // start could be missed. No transport is inferred from MIDI or the preview.
    auto tempoBpm = 120.0;
    hostTransportPlaying.store (false, std::memory_order_relaxed);
    if (auto* currentPlayHead = getPlayHead())
        if (const auto position = currentPlayHead->getPosition())
        {
            hostTransportPlaying.store (position->getIsPlaying(), std::memory_order_relaxed);
            if (const auto bpm = position->getBpm())
                tempoBpm = *bpm;
            std::optional<std::int64_t> samplePosition;
            if (const auto samples = position->getTimeInSamples())
                samplePosition = static_cast<std::int64_t> (*samples);
            if (hostPlayLfoReset.update (position->getIsPlaying(), samplePosition,
                                         buffer.getNumSamples()))
            {
                const auto free1 = getPlainParameterValue ("slider025") < 0.5f;
                const auto free2 = getPlainParameterValue ("slider035") < 0.5f;
                synthEngine.restartFreeLfosOnHostPlay (free1, free2);
            }
        }

    const auto revisionBefore = parameterRevision.load (std::memory_order_relaxed);
    if (synthEngine.isDeepIdle() && midi.isEmpty() && inputIsSilent
        && revisionBefore == lastProcessedParameterRevision)
    {
        buffer.clear();
        return;
    }

    std::array<float*, 8> auxOutputs {};
    // Real hosts provide a buffer large enough for the active output layout.
    // Some offline unit tests call processBlock with only the main stereo pair;
    // in that case simply omit aux writes rather than touching nonexistent data.
    if (buffer.getNumChannels() >= getTotalNumOutputChannels())
    {
        for (int bus = 1; bus < std::min (5, getBusCount (false)); ++bus)
        {
            if (! getBus (false, bus)->isEnabled())
                continue;
            auto destination = getBusBuffer (buffer, false, bus);
            const auto offset = static_cast<std::size_t> ((bus - 1) * 2);
            auxOutputs[offset] = destination.getWritePointer (0);
            auxOutputs[offset + 1] = destination.getWritePointer (1);
        }
    }

    auto mainOutput = getBusBuffer (buffer, false, 0);
    synthEngine.process (mainOutput, midi, parameters, sequencerState, songs, tempoBpm, hasStereoInput,
                         sidechainLeft, sidechainRight, auxOutputs, revisionBefore);
    lastProcessedParameterRevision = revisionBefore;
}

void LJuno116AudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty ("noiseColorRange01", true, nullptr);
    state.setProperty ("routingModeRevision", 2, nullptr);
    state.setProperty ("sequencerData", combinedSequencerData(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void LJuno116AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            if (! state.hasProperty ("noiseColorRange01") && state.hasProperty ("slider051"))
            {
                const auto legacy = static_cast<float> (state.getProperty ("slider051"));
                state.setProperty ("slider051", juce::jlimit (0.0f, 1.0f,
                                                              (legacy + 1.0f) * 0.5f),
                                   nullptr);
            }
            state.setProperty ("noiseColorRange01", true, nullptr);

            // Multi-output migration: old projects did not contain aux routing.
            // Keep all four auxiliary stems Off so 1-2 remains the only audible
            // output exactly as before until the user explicitly routes a stem.
            if (! state.hasProperty ("slider374"))
                for (const auto* id : { "slider374", "slider375", "slider376", "slider377" })
                    state.setProperty (id, 0.0f, nullptr);

            // Source MIDI routing migration: old projects received direct notes
            // on every MIDI channel. Omni keeps that behaviour for all three sources.
            if (! state.hasProperty ("slider378"))
                for (const auto* id : { "slider378", "slider379", "slider380" })
                    state.setProperty (id, 0.0f, nullptr);

            // Per-source performance migration. Older projects had one Portamento
            // and one Velocity Volume control. Copy them to the new source controls
            // so old projects retain their sound until edited.
            if (! state.hasProperty ("slider381"))
            {
                const auto legacyPortamento = state.hasProperty ("slider009")
                    ? static_cast<float> (state.getProperty ("slider009")) : 0.0f;
                state.setProperty ("slider381", legacyPortamento, nullptr);
                state.setProperty ("slider382", legacyPortamento, nullptr);
            }
            if (! state.hasProperty ("slider386"))
            {
                const auto l1Portamento = state.hasProperty ("slider381")
                    ? static_cast<float> (state.getProperty ("slider381")) : 0.0f;
                const auto l2Portamento = state.hasProperty ("slider382")
                    ? static_cast<float> (state.getProperty ("slider382")) : l1Portamento;
                // Before Noise had its own control it shared the combined voice
                // glide whenever L1/L2 matched; with split glides it was instant.
                const auto noisePortamento = std::abs (l1Portamento - l2Portamento) <= 0.000001f
                    ? l1Portamento : 0.0f;
                state.setProperty ("slider386", noisePortamento, nullptr);
            }
            if (! state.hasProperty ("slider383"))
            {
                const auto legacyVelocity = state.hasProperty ("slider057")
                    ? static_cast<float> (state.getProperty ("slider057")) : 0.0f;
                for (const auto* id : { "slider383", "slider384", "slider385" })
                    state.setProperty (id, legacyVelocity, nullptr);
            }

            // Per-source pitch-bend range and voice-mode migration. The original
            // single range is copied to all three sources. Voice Mode defaults to
            // Follow Global so older projects preserve the historical Voices=1
            // monophonic behaviour and Voices>1 polyphonic behaviour exactly.
            if (! state.hasProperty ("slider387"))
            {
                const auto legacyBendRange = state.hasProperty ("slider078")
                    ? static_cast<float> (state.getProperty ("slider078")) : 2.0f;
                for (const auto* id : { "slider387", "slider388", "slider389" })
                    state.setProperty (id, legacyBendRange, nullptr);
            }
            if (! state.hasProperty ("slider390"))
            {
                state.setProperty ("slider390", 0.0f, nullptr);
                state.setProperty ("slider391", 0.0f, nullptr);
            }

            // Noise Pan was added after the original L1/L2 pan controls.
            // Old project states are always centred.
            if (! state.hasProperty ("slider395"))
                state.setProperty ("slider395", 0.0f, nullptr);

            // Independent local pre-main filters were added after 0.99.51.
            // Neutral values are true bypass, so old projects retain the exact
            // historical main-filter path until a local filter is edited.
            if (! state.hasProperty ("slider396"))
            {
                for (const auto* id : { "slider396", "slider400", "slider404" })
                    state.setProperty (id, 1.0f, nullptr);
                for (const auto* id : { "slider397", "slider398", "slider399",
                                        "slider401", "slider402", "slider403",
                                        "slider405", "slider406", "slider407" })
                    state.setProperty (id, 0.0f, nullptr);
            }
            if (! state.hasProperty ("slider408"))
            {
                const auto legacyLocalSlope = state.hasProperty ("slider023")
                    ? static_cast<float> (state.getProperty ("slider023")) : 0.0f;
                for (const auto* id : { "slider408", "slider409", "slider410" })
                    state.setProperty (id, legacyLocalSlope, nullptr);
            }

            // Per-source note-generator migration. Before these controls existed,
            // the enabled Sequencer owned the synth before LArp. Recreate that
            // audible routing for older projects; new projects default to Direct.
            if (! state.hasProperty ("slider392"))
            {
                const auto sequencerMode = state.hasProperty ("slider313")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider313"))) : 0;
                const auto larpMode = state.hasProperty ("slider202")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider202"))) : 0;
                const auto source = (sequencerMode == 1 || sequencerMode == 3) ? 1.0f
                                  : (larpMode == 1 || larpMode == 3) ? 2.0f
                                                                    : 0.0f;
                for (const auto* id : { "slider392", "slider393", "slider394" })
                    state.setProperty (id, source, nullptr);
            }

            // Routing mode revision 2 removes the historical Off/Direct choice.
            // Global Note Source now provides the neutral Direct path. Preserve
            // the three musical routing functions and reorder them as:
            // 0 Synth+MIDI, 1 Synth + MIDI Direct, 2 MIDI Only.
            if (! state.hasProperty ("routingModeRevision"))
            {
                const auto remapRouting = [] (int oldMode)
                {
                    if (oldMode == 3) return 1;
                    if (oldMode == 2) return 2;
                    return 0;
                };
                const auto oldLArp = state.hasProperty ("slider202")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider202"))) : 0;
                const auto oldSequencer = state.hasProperty ("slider313")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider313"))) : 0;
                state.setProperty ("slider202", remapRouting (oldLArp), nullptr);
                state.setProperty ("slider313", remapRouting (oldSequencer), nullptr);
            }
            state.setProperty ("routingModeRevision", 2, nullptr);

            // 0.99.3 source-send migration. Older projects had one global wet
            // level per effect. Copy that value to all three source sends so
            // recalling an old project keeps the same overall FX amount until
            // the user chooses different L1/L2/Noise sends.
            if (! state.hasProperty ("slider365"))
            {
                const auto chorus = state.hasProperty ("slider060")
                    ? static_cast<float> (state.getProperty ("slider060")) : 0.4f;
                const auto delayMode = state.hasProperty ("slider100")
                    ? juce::roundToInt (static_cast<float> (state.getProperty ("slider100"))) : 1;
                const auto delay = delayMode == 2 && state.hasProperty ("slider309")
                    ? static_cast<float> (state.getProperty ("slider309"))
                    : (state.hasProperty ("slider103")
                        ? static_cast<float> (state.getProperty ("slider103")) : 0.25f);
                const auto reverb = state.hasProperty ("slider147")
                    ? static_cast<float> (state.getProperty ("slider147")) : 0.2f;
                for (const auto* id : { "slider365", "slider366", "slider367" })
                    state.setProperty (id, juce::jlimit (0.0f, 1.0f, chorus), nullptr);
                for (const auto* id : { "slider368", "slider369", "slider370" })
                    state.setProperty (id, juce::jlimit (0.0f, 1.0f, delay), nullptr);
                for (const auto* id : { "slider371", "slider372", "slider373" })
                    state.setProperty (id, juce::jlimit (0.0f, 1.0f, reverb), nullptr);
            }

            // TEST89: earlier projects have no intensity sliders. Their
            // saturation modes must retain the exact TEST87/TEST88 tone.
            for (const auto* id : { "slider430", "slider431", "slider432" })
                if (! state.hasProperty (id))
                    state.setProperty (id, 50.0f, nullptr);

            const auto sequencerData = state.getProperty ("sequencerData").toString();
            // The physical cutoff pair is authoritative when restoring projects.
            // Suppress the derived macro while APVTS loads individual values.
            for (const auto& descriptor : ljuno::generated::parameters)
            {
                if (descriptor.sliderNumber < 465)
                    continue;
                if (! state.getChildWithProperty ("id", descriptor.id).isValid())
                {
                    juce::ValueTree child ("PARAM");
                    child.setProperty ("id", descriptor.id, nullptr);
                    child.setProperty ("value", descriptor.defaultValue, nullptr);
                    state.addChild (child, -1, nullptr);
                }
            }
            synchronisingLinkedFilter.store (true);
            parameters.replaceState (state);
            synchronisingLinkedFilter.store (false);
            syncLinkedFilterCutoff (false);
            restoreSequencerData (sequencerData);
        }
}

juce::AudioProcessorEditor* LJuno116AudioProcessor::createEditor()
{
    return new LJuno116AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LJuno116AudioProcessor();
}
