// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <vector>
#include <functional>
#include <utility>
#include "PluginProcessor.h"

class LJunoAboutTextEditor final : public juce::TextEditor
{
public:
    using KeyHandler = std::function<bool (const juce::KeyPress&)>;

    explicit LJunoAboutTextEditor (const juce::String& name)
        : juce::TextEditor (name) {}

    void setKeyHandler (KeyHandler handlerToUse)
    {
        keyHandler = std::move (handlerToUse);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (keyHandler && keyHandler (key))
            return true;
        return juce::TextEditor::keyPressed (key);
    }

private:
    KeyHandler keyHandler;
};

class LJunoParameterComboBox final : public juce::ComboBox
{
public:
    void focusGained (FocusChangeType cause) override
    {
        // Include the shortcut whenever keyboard focus enters the parameter grid,
        // whether through Alt+L, Tab/Shift+Tab, or another focus transfer.
        // The editor clears it before navigation inside the grid.
        setDescription ("Alt+L");
        juce::ComboBox::focusGained (cause);
    }
};

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
    LJunoParameterComboBox parameterSelector;
    juce::Slider parameterValue;
    juce::TextButton sequencerButton { "Sequencer" };
    juce::TextButton resetParameter { "Reset parameter" };
    juce::TextButton initializeSynth { "Initialize synth" };
    juce::TextButton previousPreset { "Previous preset" };
    juce::TextButton nextPreset { "Next preset" };
    juce::TextButton loadPreset { "Browser" };
    juce::TextButton savePreset { "Save preset" };
    juce::TextButton help { "Help" };
    juce::TextButton aboutButton { "About" };
    LJunoAboutTextEditor aboutInfo { "About LJuno-116" };
    juce::TextButton aboutProject { "Project" };
    juce::TextButton aboutContact { "Contact" };
    juce::TextButton aboutClose { "Close" };
    juce::Label sequencerEditorPanel;
    juce::Label sequencerParameterPickerPanel;
    juce::ComboBox sequencerParameterPicker;

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
    bool aboutOpen = false;
    bool sequencerEditorOpen = false;
    int sequencerEditorBlock = 0;
    int sequencerEditorCurrentStep = 0;
    ljuno::SequencerLayer sequencerEditorLayer = ljuno::SequencerLayer::note;
    bool sequencerEditorLaunchPage = false;
    int sequencerEditorValueStepIndex = 0;
    int sequencerEditorParameterIndex = 0;
    bool sequencerParameterPickerOpen = false;
    std::vector<int> sequencerParameterCatalogIndices;
    std::vector<juce::String> sequencerParameterNames;
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
    bool selectNextParameterStartingWith (juce::juce_wchar, bool backwards = false);
    void buildSequencerParameterPickerList();
    void openSequencerParameterPicker();
    void closeSequencerParameterPicker (bool returnToEditor = true);
    bool handleSequencerParameterPickerKey (const juce::KeyPress&);
    void moveSequencerParameterPicker (int rowDelta, int columnDelta);
    void setSequencerParameterPickerIndex (int);
    bool selectSequencerParameterStartingWith (juce::juce_wchar, bool backwards);
    void addSelectedSequencerParameter (bool keepPickerOpen);
    void changeSequencerEditorParameterSelection (int direction);
    void removeSequencerEditorParameter();
    juce::String currentSequencerParameterText() const;
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
    void openAbout();
    void closeAbout();
    void openContactEmail();
    bool navigateAboutText (const juce::KeyPress&);
    void openProjectPage();
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
