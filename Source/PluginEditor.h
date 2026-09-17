// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <vector>
#include "PluginProcessor.h"

class LJuno116AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::KeyListener,
                                           private juce::ListBoxModel,
                                           private juce::AsyncUpdater
{
public:
    explicit LJuno116AudioProcessorEditor (LJuno116AudioProcessor&);
    ~LJuno116AudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void focusGained (FocusChangeType) override;
    void visibilityChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    LJuno116AudioProcessor& processor;
    std::unique_ptr<juce::LookAndFeel_V4> c64LookAndFeel;
    std::unique_ptr<juce::LookAndFeel_V4> helpMenuLookAndFeel;
    juce::Label title;
    juce::Label status;
    juce::ComboBox pageSelector;
    juce::ComboBox parameterSelector;
    juce::Slider parameterValue;
    juce::TextButton sequencerButton { "Sequencer" };
    juce::TextButton resetParameter { "Reset parameter" };
    juce::TextButton initializeSynth { "Initialize synth" };
    juce::TextButton previousPreset { "Previous preset" };
    juce::TextButton nextPreset { "Next preset" };
    juce::TextButton loadPreset { "Browser" };
    juce::TextButton savePreset { "Save preset" };
    juce::TextButton help { "Help" };
    juce::Label sequencerEditorPanel;
    juce::Label parameterLockBrowserTitle;
    juce::ListBox parameterLockBrowser { "All parameters", this };

    juce::Label presetBrowserPath;
    juce::ListBox presetBrowser { "Preset browser", this };
    juce::TextButton presetBrowserBack { "Back" };
    juce::TextButton presetBrowserClose { "Close" };
    juce::Label presetSaveLabel;
    juce::TextEditor presetSaveName;
    juce::TextButton presetSaveConfirm { "Save" };
    juce::TextButton presetSaveCancel { "Close" };
    juce::Label presetOverwriteLabel;
    juce::TextButton presetOverwriteYes { "Yes" };
    juce::TextButton presetOverwriteNo { "No" };
    juce::TextButton presetOverwriteClose { "Close" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> valueAttachment;
    std::vector<int> visibleParameterIndices;
    std::vector<juce::String> visibleParameterNames;
    std::vector<ljuno::PresetManager::BrowserEntry> presetBrowserEntries;
    juce::File presetBrowserDirectory;
    juce::File presetBrowserPreviewFile;
    ljuno::PresetManager::PatchSnapshot presetBrowserOriginalPatch;
    bool presetBrowserOpen = false;
    bool presetBrowserHasPreview = false;
    bool suppressPresetBrowserAnnouncement = false;
    bool presetSaveOpen = false;
    bool presetOverwriteConfirmationOpen = false;
    juce::File presetOverwriteFile;
    juce::File presetSaveDirectory;
    bool presetDeleteConfirmationOpen = false;
    juce::File presetDeleteFile;
    int presetDeleteRow = -1;
    bool initialFocusTransferPending = false;
    int initialFocusTransferAttempts = 0;
    int stepWidthIndex = 0;
    bool valueTextEditorShortcutActive = false;
    juce::Component* valueTextEditorAnnouncementSource = nullptr;
    juce::Component* pendingShortcutFocusTarget = nullptr;
    juce::Component* overlayReturnFocusTarget = nullptr;
    std::vector<int> lastParameterIndexByPage;
    int displayedPageIndex = -1;
    int displayedDelayMode = -1;
    int displayedReverbMode = -1;
    bool effectParameterRefreshPending = false;
    bool sequencerEditorOpen = false;
    int sequencerEditorBlock = 0;
    int sequencerEditorCurrentStep = 0;
    ljuno::SequencerLayer sequencerEditorLayer = ljuno::SequencerLayer::note;
    bool sequencerEditorLaunchPage = false;
    bool sequencerEditorParameterPage = false;
    int sequencerEditorSelectedLockSlider = -1;
    std::array<std::array<int, ljuno::SequencerState::stepsPerSequence>,
               ljuno::SequencerState::maximumSequences> sequencerEditorSelectedLocksByStep {};
    int sequencerEditorValueStepIndex = 0;
    bool parameterLockBrowserOpen = false;
    bool suppressParameterLockBrowserAnnouncement = false;
    std::vector<int> parameterLockBrowserCatalogIndices;
    std::array<bool, ljuno::SequencerState::stepsPerSequence> sequencerEditorSelectedSteps {};
    int sequencerEditorLastStepKey = -1;
    double sequencerEditorLastStepTimeMs = 0.0;

    void selectRelativePage (int delta);
    void selectPageByInitial (juce::juce_wchar, bool focusParameterGrid = true);
    void announcePage();
    void updateParameterList();
    void selectParameter();
    void resetSelectedParameter();
    void initializeAllParameters();
    void updateCurrentParameterLabel();
    void notifySelectedValueChanged();
    void setParameterListIndex (int);
    void changeStepWidth (int);
    void changeSelectedValue (int direction, bool pageStep);
    void setSelectedValueToBoundary (bool maximum);
    void announceMessage (const juce::String&);
    void announceMessageFrom (juce::Component&, const juce::String&);
    void scheduleInitialFocusTransfer();
    void performInitialFocusTransfer();
    void handleAsyncUpdate() override;
    void requestShortcutFocus (juce::Component&);
    juce::Component* normaliseFocusTarget (juce::Component*) noexcept;
    void rememberOverlayReturnFocus();
    void restoreOverlayReturnFocus (juce::Component& fallback);
    void rememberCurrentPageAndParameter();
    juce::Rectangle<int> getC64ScreenBounds() const;
    void moveParameterInGrid (int rowDelta, int columnDelta);
    bool selectNextParameterStartingWith (juce::juce_wchar);
    void openSequencerEditor();
    void closeSequencerEditor();
    bool handleSequencerEditorKey (const juce::KeyPress&);
    void refreshSequencerEditorPanel (bool announce = false);
    void announceSequencerStep();
    void selectSequencerEditorStep (int localStep);
    void changeSequencerEditorStepValue (int direction);
    void changeSequencerEditorLayer (int direction);
    void changeSequencerEditorBlock (int direction);
    void changeSequencerEditorSequence (int direction);
    juce::String sequencerLayerName() const;
    juce::String sequencerStepValueText (int sequence, int step) const;
    void openParameterLockBrowser();
    void closeParameterLockBrowser (bool restoreFocus = true);
    void refreshParameterLockBrowser (int selectedRow = -1);
    void announceParameterLockBrowserRow (int row);
    void assignParameterLockBrowserRow (int row, bool closeAfter);
    void toggleParameterLockBrowserRow (int row);
    void moveParameterLockBrowserInGrid (int rowDelta, int columnDelta, bool assignedOnly);
    bool selectNextParameterLockBrowserStartingWith (juce::juce_wchar);
    void moveSelectedParameterLock (int direction);
    void rememberSelectedParameterLockForCurrentStep (int sliderNumber);
    int getParameterLockSliderForRow (int row) const noexcept;
    void validateSelectedParameterLock();
    void changePreset (int direction);
    void togglePresetBrowser();
    void openPresetBrowser();
    void closePresetBrowser (bool restoreOriginalPatch = true,
                             bool restoreFocus = true);
    void refreshPresetBrowser (int selectedRow = 0);
    void focusPresetBrowserAndAnnounce();
    void selectPresetBrowserRow (int row);
    void announcePresetBrowserRow (int row, bool includeFolder);
    bool previewPresetBrowserRow (int row);
    void activatePresetBrowserRow (int row);
    void goToParentPresetFolder();
    void showPresetSave();
    void closePresetSave (bool restoreFocus = true);
    void commitPresetSave();
    void showPresetOverwriteConfirmation (const juce::File&);
    void dismissPresetOverwriteConfirmation();
    void confirmPresetOverwrite();
    void showPresetDeleteConfirmation();
    void dismissPresetDeleteConfirmation();
    void confirmPresetDelete();
    void showHelpLanguageMenu();
    void openHelp (const juce::String& languageCode);
    void refreshAfterPresetChange();
    void setMainControlsEnabled (bool);

    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    juce::String getNameForRow (int) override;
    void selectedRowsChanged (int) override;
    void returnKeyPressed (int) override;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LJuno116AudioProcessorEditor)
};
