// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "GeneratedParameters.h"
#include "GeneratedPages.h"
#include "ScreenReaderAnnouncer.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
constexpr int parametersPerColumn = 8;
constexpr int parameterListPageStep = 5;
constexpr int valuePageStep = 40;
constexpr int stepWidths[] { 1, 5, 10, 15, 20 };
constexpr auto ljunoVersion = "0.99.5";
constexpr auto ljunoReleaseDate = "21 September 2026";
constexpr auto ljunoProjectUrl = "https://github.com/Lo-lo78/LJuno-116";
constexpr auto ljunoContactEmail = "vmanolo301@gmail.com";
constexpr auto ljunoLicense = "GNU Affero General Public License v3 or later (AGPL-3.0-or-later)";
const juce::Identifier stepWidthState { "editorStepWidthIndex" };
const juce::Identifier selectedPageState { "editorSelectedPage" };
const juce::Identifier presetBrowserDirectoryState { "presetBrowserDirectory" };
const juce::Identifier presetBrowserSelectionState { "presetBrowserSelection" };
const juce::Identifier presetBrowserRowState { "presetBrowserRow" };
const juce::Identifier deletedValueCharacter { "deletedValueCharacter" };

juce::Identifier selectedParameterState (int pageIndex)
{
    return juce::Identifier ("editorSelectedParameterPage" + juce::String (pageIndex));
}

// Power-on colours 6 and 14 from the VIC-II palette, using the commonly
// documented PAL RGB conversion. The original signal was YUV, so no RGB pair
// can be universally exact across all CRTs.
const auto c64Blue = juce::Colour::fromRGB (0x50, 0x45, 0x9b);
const auto c64LightBlue = juce::Colour::fromRGB (0x88, 0x7e, 0xcb);

using ValueEditorShortcut = std::function<bool (const juce::KeyPress&, juce::Component*)>;

class ShortcutValueTextEditor final : public juce::TextEditor
{
public:
    ShortcutValueTextEditor (const juce::String& name, ValueEditorShortcut shortcutToUse)
        : juce::TextEditor (name), shortcut (std::move (shortcutToUse)) {}

    bool keyPressed (const juce::KeyPress& key) override
    {
        // This runs before TextEditor::keyPressed, which otherwise inserts
        // Alt+letters before a KeyListener can see the global shortcuts.
        if (key.getModifiers().isAltDown() && shortcut)
        {
            const auto keyCode = key.getKeyCode();
            const auto isValueNavigation = keyCode == juce::KeyPress::upKey
                                        || keyCode == juce::KeyPress::downKey
                                        || keyCode == juce::KeyPress::leftKey
                                        || keyCode == juce::KeyPress::rightKey
                                        || keyCode == juce::KeyPress::pageUpKey
                                        || keyCode == juce::KeyPress::pageDownKey
                                        || keyCode == juce::KeyPress::homeKey
                                        || keyCode == juce::KeyPress::endKey;
            const auto updatesValue = isValueNavigation
                                   && keyCode != juce::KeyPress::leftKey
                                   && keyCode != juce::KeyPress::rightKey;
            const juce::Component::SafePointer<ShortcutValueTextEditor> safeThis (this);
            const auto descriptionBefore = getDescription();
            if (shortcut (key, this))
            {
                if (updatesValue && safeThis != nullptr)
                    if (auto* slider = safeThis->findParentComponentOfClass<juce::Slider>())
                    {
                        safeThis->setText (slider->getTextFromValue (slider->getValue()), false);
                        safeThis->selectAll();
                    }
                if (safeThis != nullptr && safeThis->getDescription().isNotEmpty()
                    && (isValueNavigation
                        || safeThis->getDescription() != descriptionBefore))
                    ljuno::announceToActiveScreenReader (*safeThis,
                                                         safeThis->getDescription());
                return true;
            }
        }

        // Enter is handled by the editor owner so it can commit this temporary
        // field and raise a fresh accessible focus event on the parameter grid.
        if (key.getKeyCode() == juce::KeyPress::returnKey && shortcut)
            if (shortcut (key, this))
                return true;

        const auto keyCode = key.getKeyCode();
        const auto character = key.getTextCharacter();
        const auto modifiers = key.getModifiers();
        const auto isNumericInput = ! modifiers.isAltDown() && ! modifiers.isCtrlDown()
                                 && ! modifiers.isCommandDown()
                                 && (juce::CharacterFunctions::isDigit (character)
                                     || character == '.');

        if (keyCode == juce::KeyPress::backspaceKey)
        {
            juce::juce_wchar deleted = 0;
            const auto selectedRange = getHighlightedRegion();
            if (! selectedRange.isEmpty())
            {
                const auto selectedText = getTextInRange (selectedRange);
                if (selectedText.isNotEmpty())
                    deleted = selectedText.getLastCharacter();
            }
            else if (getCaretPosition() > 0)
            {
                const auto beforeCaret = getTextInRange (
                    { getCaretPosition() - 1, getCaretPosition() });
                if (beforeCaret.isNotEmpty())
                    deleted = beforeCaret.getLastCharacter();
            }

            const auto handled = juce::TextEditor::keyPressed (key);
            if (deleted != 0 && shortcut)
            {
                getProperties().set (deletedValueCharacter,
                                     juce::String::charToString (deleted));
                shortcut (key, this);
                getProperties().remove (deletedValueCharacter);
                if (getDescription().isNotEmpty())
                    ljuno::announceToActiveScreenReader (*this, getDescription());
            }
            return handled;
        }

        const auto handled = juce::TextEditor::keyPressed (key);
        if (isNumericInput && shortcut)
        {
            shortcut (key, this);
            if (getDescription().isNotEmpty())
                ljuno::announceToActiveScreenReader (*this, getDescription());
        }
        return handled;
    }

private:
    ValueEditorShortcut shortcut;
};

class ShortcutSliderLabel final : public juce::Label
{
public:
    explicit ShortcutSliderLabel (ValueEditorShortcut shortcutToUse)
        : shortcut (std::move (shortcutToUse)) {}

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return createIgnoredAccessibilityHandler (*this);
    }

protected:
    juce::TextEditor* createEditorComponent() override
    {
        auto* textEditor = new ShortcutValueTextEditor (getName(), shortcut);
        textEditor->setInputRestrictions (0, "0123456789.");
        textEditor->applyFontToAllText (getLookAndFeel().getLabelFont (*this));
        textEditor->setColour (juce::TextEditor::textColourId,
                           findColour (juce::TextEditor::textColourId));
        textEditor->setColour (juce::TextEditor::backgroundColourId,
                           findColour (juce::TextEditor::backgroundColourId));
        textEditor->setColour (juce::TextEditor::outlineColourId,
                           findColour (juce::TextEditor::outlineColourId));
        textEditor->setColour (juce::TextEditor::highlightColourId,
                           findColour (juce::TextEditor::highlightColourId));
        return textEditor;
    }

private:
    ValueEditorShortcut shortcut;
};

juce::Font c64Font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                          height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

class C64LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit C64LookAndFeel (ValueEditorShortcut shortcutToUse,
                             juce::String popupWindowTitleToUse = {})
        : valueEditorShortcut (std::move (shortcutToUse)),
          popupWindowTitle (std::move (popupWindowTitleToUse))
    {
        setColour (juce::TextButton::buttonColourId, c64Blue);
        setColour (juce::TextButton::buttonOnColourId, c64LightBlue);
        setColour (juce::TextButton::textColourOffId, c64LightBlue);
        setColour (juce::TextButton::textColourOnId, c64Blue);

        setColour (juce::ComboBox::backgroundColourId, c64Blue);
        setColour (juce::ComboBox::textColourId, c64LightBlue);
        setColour (juce::ComboBox::outlineColourId, c64LightBlue);
        setColour (juce::ComboBox::buttonColourId, c64Blue);
        setColour (juce::ComboBox::arrowColourId, c64LightBlue);
        setColour (juce::ComboBox::focusedOutlineColourId, c64LightBlue);

        setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, c64LightBlue);
        setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::backgroundWhenEditingColourId, c64Blue);
        setColour (juce::Label::textWhenEditingColourId, c64LightBlue);
        setColour (juce::Label::outlineWhenEditingColourId, c64LightBlue);

        setColour (juce::Slider::backgroundColourId, c64Blue);
        setColour (juce::Slider::trackColourId, c64LightBlue);
        setColour (juce::Slider::thumbColourId, c64LightBlue);
        setColour (juce::Slider::textBoxTextColourId, c64LightBlue);
        setColour (juce::Slider::textBoxBackgroundColourId, c64Blue);
        setColour (juce::Slider::textBoxHighlightColourId, c64LightBlue);
        setColour (juce::Slider::textBoxOutlineColourId, c64LightBlue);

        setColour (juce::ListBox::backgroundColourId, c64Blue);
        setColour (juce::ListBox::outlineColourId, c64LightBlue);
        setColour (juce::ListBox::textColourId, c64LightBlue);

        setColour (juce::PopupMenu::backgroundColourId, c64Blue);
        setColour (juce::PopupMenu::textColourId, c64LightBlue);
        setColour (juce::PopupMenu::headerTextColourId, c64LightBlue);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c64LightBlue);
        setColour (juce::PopupMenu::highlightedTextColourId, c64Blue);

        setColour (juce::TextEditor::backgroundColourId, c64Blue);
        setColour (juce::TextEditor::textColourId, c64LightBlue);
        setColour (juce::TextEditor::highlightColourId, c64LightBlue);
        setColour (juce::TextEditor::highlightedTextColourId, c64Blue);
        setColour (juce::TextEditor::outlineColourId, c64LightBlue);
        setColour (juce::TextEditor::focusedOutlineColourId, c64LightBlue);
        setColour (juce::CaretComponent::caretColourId, c64LightBlue);
        setColour (juce::ScrollBar::backgroundColourId, c64Blue);
        setColour (juce::ScrollBar::thumbColourId, c64LightBlue);
        setColour (juce::ScrollBar::trackColourId, c64Blue);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int height) override
    {
        return c64Font (juce::jlimit (13.0f, 17.0f, height * 0.43f), true);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return c64Font (16.0f, true); }
    juce::Font getLabelFont (juce::Label& label) override
    {
        if (label.getFont().getHeight() > 20.0f)
            return label.getFont();
        return c64Font (16.0f, true);
    }
    juce::Font getPopupMenuFont() override { return c64Font (16.0f, true); }

    void preparePopupMenuWindow (juce::Component& window) override
    {
        juce::LookAndFeel_V4::preparePopupMenuWindow (window);

        if (popupWindowTitle.isNotEmpty())
        {
            // PopupMenu otherwise exposes the host/plugin window title to UIA.
            // Give this particular popup a useful, role-independent name.
            window.setName (popupWindowTitle);
            window.setTitle (popupWindowTitle);
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = new ShortcutSliderLabel (valueEditorShortcut);
        label->setJustificationType (juce::Justification::centred);
        label->setKeyboardType (juce::TextInputTarget::decimalKeyboard);
        label->setColour (juce::Label::textColourId,
                          slider.findColour (juce::Slider::textBoxTextColourId));
        label->setColour (juce::Label::backgroundColourId,
                          slider.findColour (juce::Slider::textBoxBackgroundColourId));
        label->setColour (juce::Label::outlineColourId,
                          slider.findColour (juce::Slider::textBoxOutlineColourId));
        label->setColour (juce::TextEditor::textColourId,
                          slider.findColour (juce::Slider::textBoxTextColourId));
        label->setColour (juce::TextEditor::backgroundColourId,
                          slider.findColour (juce::Slider::textBoxBackgroundColourId));
        label->setColour (juce::TextEditor::outlineColourId,
                          slider.findColour (juce::Slider::textBoxOutlineColourId));
        label->setColour (juce::TextEditor::highlightColourId,
                          slider.findColour (juce::Slider::textBoxHighlightColourId));
        return label;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool highlighted, bool down) override
    {
        const auto inverse = highlighted || down || button.hasKeyboardFocus (false);
        g.setColour ((inverse ? c64LightBlue : c64Blue)
                         .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.45f));
        g.fillRect (button.getLocalBounds());
        g.setColour ((inverse ? c64Blue : c64LightBlue)
                         .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.45f));
        g.drawRect (button.getLocalBounds(), inverse ? 3 : 2);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool highlighted, bool down) override
    {
        const auto inverse = highlighted || down || button.hasKeyboardFocus (false);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.setColour ((inverse ? c64Blue : c64LightBlue)
                         .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.45f));
        g.drawFittedText (button.getButtonText().toUpperCase(),
                          button.getLocalBounds().reduced (6, 2),
                          juce::Justification::centred, 1, 0.85f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        const auto focused = box.hasKeyboardFocus (true);
        g.setColour (down ? c64LightBlue : c64Blue);
        g.fillRect (0, 0, width, height);
        g.setColour (down ? c64Blue : c64LightBlue);
        g.drawRect (0, 0, width, height, focused ? 3 : 2);
        g.drawLine (static_cast<float> (buttonX), 2.0f,
                    static_cast<float> (buttonX), static_cast<float> (height - 2), 2.0f);

        juce::Path arrow;
        const auto cx = buttonX + buttonW * 0.5f;
        const auto cy = buttonY + buttonH * 0.52f;
        arrow.addTriangle (cx - 6.0f, cy - 3.0f,
                           cx + 6.0f, cy - 3.0f,
                           cx, cy + 5.0f);
        g.fillPath (arrow);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal
            && style != juce::Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                              0.0f, 0.0f, style, slider);
            return;
        }

        const auto track = juce::Rectangle<float> (static_cast<float> (x),
                                                    y + height * 0.36f,
                                                    static_cast<float> (width),
                                                    juce::jmax (8.0f, height * 0.28f));
        g.setColour (c64Blue);
        g.fillRect (track);
        g.setColour (c64LightBlue);
        g.drawRect (track, 2.0f);
        g.fillRect (track.withRight (juce::jlimit (track.getX(), track.getRight(), sliderPos)));

        const auto thumb = juce::Rectangle<float> (12.0f, height * 0.72f)
                               .withCentre ({ sliderPos, y + height * 0.5f });
        g.setColour (c64LightBlue);
        g.fillRect (thumb);
        g.setColour (c64Blue);
        g.drawRect (thumb.reduced (3.0f), 1.0f);
    }

private:
    ValueEditorShortcut valueEditorShortcut;
    juce::String popupWindowTitle;
};

}

LJuno116AudioProcessorEditor::LJuno116AudioProcessorEditor (LJuno116AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    c64LookAndFeel = std::make_unique<C64LookAndFeel> (
        [this] (const juce::KeyPress& key, juce::Component* source)
        {
            const juce::ScopedValueSetter shortcutScope (valueTextEditorShortcutActive, true);
            const juce::ScopedValueSetter sourceScope (valueTextEditorAnnouncementSource, source);
            return keyPressed (key, source);
        });
    helpMenuLookAndFeel = std::make_unique<C64LookAndFeel> (
        ValueEditorShortcut {}, "Choose Help language");
    setLookAndFeel (c64LookAndFeel.get());

    title.setText ("**** LJUNO-116 SYNTHESIZER ****", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (c64Font (22.0f, true));
    title.setAccessible (false);
    addAndMakeVisible (title);

    pageSelector.setTitle ("Parameter page");
    pageSelector.setDescription (
        "Selects a group of synthesizer parameters. Type a letter or number to move to the next page beginning with it. Alt+D");
    for (int i = 0; i < static_cast<int> (std::size (ljuno::generated::pages)); ++i)
        pageSelector.addItem (ljuno::generated::pages[static_cast<size_t> (i)].name, i + 1);
    const auto pageCount = static_cast<int> (std::size (ljuno::generated::pages));
    const auto rememberedPage = juce::jlimit (0, pageCount - 1,
        static_cast<int> (processor.parameters.state.getProperty (selectedPageState, 0)));
    pageSelector.setSelectedId (rememberedPage + 1, juce::dontSendNotification);
    pageSelector.onChange = [this]
    {
        updateParameterList();
    };
    pageSelector.setExplicitFocusOrder (1);
    addAndMakeVisible (pageSelector);

    parameterSelector.setTitle ("Parameter");
    parameterSelector.setDescription (
        "Selects the synthesizer parameter to edit. Type a letter or number to move to the next parameter beginning with it");
    parameterSelector.setTextWhenNothingSelected ("Select a parameter");
    parameterSelector.setExplicitFocusOrder (2);
    parameterSelector.onChange = [this] { selectParameter(); };
    addAndMakeVisible (parameterSelector);

    parameterValue.setSliderStyle (juce::Slider::LinearHorizontal);
    parameterValue.setTextBoxStyle (juce::Slider::TextBoxRight, false, 150, 32);
    // Slider constructs its label before this editor has a parent LookAndFeel.
    // Assign explicitly so that the label and its TextEditor are rebuilt using
    // ShortcutSliderLabel rather than JUCE's consuming default editor.
    parameterValue.setLookAndFeel (c64LookAndFeel.get());
    parameterValue.setTitle ("Parameter value");
    parameterValue.setDescription ("Edits the value of the selected synthesizer parameter");
    parameterValue.setWantsKeyboardFocus (true);
    parameterValue.setExplicitFocusOrder (3);
    parameterValue.onValueChange = [this]
    {
        updateCurrentParameterLabel();

        const auto index = parameterSelector.getSelectedItemIndex();
        if (! juce::isPositiveAndBelow (index, static_cast<int> (visibleParameterIndices.size())))
            return;

        const auto catalogIndex = visibleParameterIndices[static_cast<std::size_t> (index)];
        const auto& descriptor = ljuno::generated::parameters[static_cast<std::size_t> (catalogIndex)];
        const auto changedId = juce::String (descriptor.id);
        if (changedId != "slider100" && changedId != "slider140")
            return;

        auto delayMode = processor.parameters.getRawParameterValue ("slider100") != nullptr
            ? juce::jlimit (0, 2, juce::roundToInt (
                processor.parameters.getRawParameterValue ("slider100")->load()))
            : 0;
        auto reverbMode = processor.parameters.getRawParameterValue ("slider140") != nullptr
            ? juce::jlimit (0, 2, juce::roundToInt (
                processor.parameters.getRawParameterValue ("slider140")->load()))
            : 0;

        // The SliderAttachment may publish the APVTS value just after the
        // Slider's own onValueChange callback. Use the value the user has just
        // selected for the engine selector so we never miss the refresh by
        // comparing against one stale APVTS sample.
        if (changedId == "slider100")
            delayMode = juce::jlimit (0, 2, juce::roundToInt (parameterValue.getValue()));
        else if (changedId == "slider140")
            reverbMode = juce::jlimit (0, 2, juce::roundToInt (parameterValue.getValue()));
        if ((delayMode == displayedDelayMode && reverbMode == displayedReverbMode)
            || effectParameterRefreshPending)
            return;

        effectParameterRefreshPending = true;
        const auto restoreValueFocus = parameterValue.hasKeyboardFocus (true);
        const auto restoreParameterFocus = parameterSelector.hasKeyboardFocus (true);
        juce::Component::SafePointer<LJuno116AudioProcessorEditor> safeThis (this);
        juce::MessageManager::callAsync ([safeThis, restoreValueFocus, restoreParameterFocus]
        {
            if (safeThis == nullptr)
                return;

            safeThis->effectParameterRefreshPending = false;
            safeThis->updateParameterList();

            // Delay/Reverb engine changes alter the FX parameter grid itself.
            // Notify accessibility clients explicitly. updateParameterList()
            // restores the selected parameter by stable parameter ID rather
            // than by its old row, so the cursor follows the parameter when
            // controls are inserted or removed around it.
            if (auto* handler = safeThis->parameterSelector.getAccessibilityHandler())
                handler->notifyAccessibilityEvent (juce::AccessibilityEvent::structureChanged);

            // Preserve the user's editing context as well as the selected
            // parameter. If the value slider owned focus, keep editing the
            // same parameter; if the parameter grid owned focus, stay there.
            // Fall back to the grid for any unusual programmatic change.
            auto* focusTarget = restoreValueFocus
                ? static_cast<juce::Component*> (&safeThis->parameterValue)
                : static_cast<juce::Component*> (&safeThis->parameterSelector);
            if (! restoreValueFocus && ! restoreParameterFocus)
                focusTarget = &safeThis->parameterSelector;

            safeThis->parameterValue.hideTextBox (false);
            juce::AccessibilityHandler::clearCurrentlyFocusedHandler();
            focusTarget->grabKeyboardFocus();
            if (auto* handler = focusTarget->getAccessibilityHandler())
                handler->grabFocus();
        });
    };
    addAndMakeVisible (parameterValue);

    sequencerButton.setDescription ("Opens the Sequencer step editor. Shortcut Alt Q");
    sequencerButton.setExplicitFocusOrder (4);
    sequencerButton.onClick = [this] { openSequencerEditor(); };
    addAndMakeVisible (sequencerButton);
    sequencerButton.setVisible (false);

    resetParameter.setDescription ("Restores the selected parameter to its initial value. Shortcut Alt R");
    resetParameter.setExplicitFocusOrder (5);
    resetParameter.onClick = [this] { resetSelectedParameter(); };
    addAndMakeVisible (resetParameter);

    initializeSynth.setDescription ("Restores every synthesizer and LArp parameter to the Lua Init patch. Shortcut Alt I");
    initializeSynth.setExplicitFocusOrder (6);
    initializeSynth.onClick = [this] { initializeAllParameters(); };
    addAndMakeVisible (initializeSynth);

    previousPreset.setDescription ("Loads the previous preset. Shortcut Alt minus");
    previousPreset.setExplicitFocusOrder (7);
    previousPreset.onClick = [this] { changePreset (-1); };
    addAndMakeVisible (previousPreset);

    nextPreset.setDescription ("Loads the next preset. Shortcut Alt plus");
    nextPreset.setExplicitFocusOrder (8);
    nextPreset.onClick = [this] { changePreset (1); };
    addAndMakeVisible (nextPreset);

    loadPreset.setDescription ("Opens the accessible preset browser. Shortcut Alt B");
    loadPreset.setExplicitFocusOrder (9);
    loadPreset.onClick = [this] { togglePresetBrowser(); };
    addAndMakeVisible (loadPreset);

    savePreset.setDescription ("Saves the current patch with a name. Shortcut Alt S");
    savePreset.setExplicitFocusOrder (10);
    savePreset.onClick = [this] { showPresetSave(); };
    addAndMakeVisible (savePreset);

    help.setDescription ("Opens the HTML help language menu. Shortcut Alt+H");
    help.setExplicitFocusOrder (11);
    help.onClick = [this] { showHelpLanguageMenu(); };
    addAndMakeVisible (help);

    aboutButton.setDescription ("Opens information about LJuno-116. Shortcut Alt A");
    aboutButton.setExplicitFocusOrder (12);
    aboutButton.onClick = [this] { openAbout(); };
    addAndMakeVisible (aboutButton);

    const auto aboutText =
        juce::String ("LJuno-116\nVersion: ") + ljunoVersion
        + "\nRelease date: " + ljunoReleaseDate
        + "\nLicense: " + ljunoLicense
        + "\nProject: " + ljunoProjectUrl
        + "\nContact: " + ljunoContactEmail;

    aboutInfo.setTitle ("About LJuno-116");
    aboutInfo.setMultiLine (true, true);
    aboutInfo.setReturnKeyStartsNewLine (false);
    aboutInfo.setReadOnly (true);
    aboutInfo.setScrollbarsShown (true);
    aboutInfo.setCaretVisible (true);
    aboutInfo.setPopupMenuEnabled (false);
    aboutInfo.setTabKeyUsedAsCharacter (false);
    aboutInfo.setText (aboutText, false);
    // NVDA announces the real About information when focus enters the panel.
    // Do not expose keyboard instructions here: the user hears the actual
    // version, release date, license, project URL and contact address instead.
    aboutInfo.setDescription (aboutText);
    aboutInfo.setJustification (juce::Justification::centredLeft);
    aboutInfo.setFont (c64Font (18.0f, true));
    aboutInfo.setColour (juce::TextEditor::backgroundColourId, c64Blue);
    aboutInfo.setColour (juce::TextEditor::textColourId, c64LightBlue);
    aboutInfo.setColour (juce::TextEditor::outlineColourId, c64LightBlue);
    aboutInfo.setColour (juce::TextEditor::focusedOutlineColourId, c64LightBlue);
    aboutInfo.setBorder (juce::BorderSize<int> (12));
    aboutInfo.setWantsKeyboardFocus (true);
    aboutInfo.setExplicitFocusOrder (1);
    aboutInfo.setKeyHandler ([this] (const juce::KeyPress& key)
    {
        return keyPressed (key, &aboutInfo);
    });
    aboutInfo.onEscapeKey = [this] { closeAbout(); };
    addAndMakeVisible (aboutInfo);
    aboutInfo.setVisible (false);

    aboutProject.setButtonText (juce::String ("Project: ") + ljunoProjectUrl);
    aboutProject.setDescription ("LJuno-116 GitHub project URL. Press Enter to open in the default browser.");
    aboutProject.setWantsKeyboardFocus (true);
    aboutProject.setExplicitFocusOrder (2);
    aboutProject.onClick = [this] { openProjectPage(); };
    aboutProject.addKeyListener (this);
    addAndMakeVisible (aboutProject);
    aboutProject.setVisible (false);

    aboutContact.setButtonText (juce::String ("Contact: ") + ljunoContactEmail);
    aboutContact.setDescription ("LJuno-116 contact email. Press Enter to open the default email application.");
    aboutContact.setWantsKeyboardFocus (true);
    aboutContact.setExplicitFocusOrder (3);
    aboutContact.onClick = [this] { openContactEmail(); };
    aboutContact.addKeyListener (this);
    addAndMakeVisible (aboutContact);
    aboutContact.setVisible (false);

    aboutClose.setDescription ("Closes About. Shortcut Alt C");
    aboutClose.setWantsKeyboardFocus (true);
    aboutClose.setExplicitFocusOrder (4);
    aboutClose.onClick = [this] { closeAbout(); };
    aboutClose.addKeyListener (this);
    addAndMakeVisible (aboutClose);
    aboutClose.setVisible (false);

    sequencerEditorPanel.setTitle ("Sequencer step editor");
    sequencerEditorPanel.setDescription (
        "Alt Q sequencer editor. Q to I and A to K select the sixteen visible steps. "
        "1 to 5 select Note, Length, Velocity, Repeat and Shift. 6 selects Parameters. "
        "On Parameters, Enter opens the synth parameter grid and Home End move through locks in the current step. "
        "8 selects Launch Step. 9 and 0 change the sixteen-step block. "
        "Page Up and Page Down change BPM Division. M toggles Legato and P changes Playback Mode. "
        "Left and Right choose the value step 1, 5, 10 through 40. Up and Down edit by that step. "
        "Z X edit Start, C V edit End, "
        "Home End change Sequence. Escape closes.");
    sequencerEditorPanel.setJustificationType (juce::Justification::topLeft);
    sequencerEditorPanel.setFont (c64Font (17.0f, true));
    sequencerEditorPanel.setColour (juce::Label::backgroundColourId, c64Blue);
    sequencerEditorPanel.setColour (juce::Label::textColourId, c64LightBlue);
    sequencerEditorPanel.setColour (juce::Label::outlineColourId, c64LightBlue);
    sequencerEditorPanel.setBorderSize (juce::BorderSize<int> (10));
    sequencerEditorPanel.setWantsKeyboardFocus (true);
    sequencerEditorPanel.addKeyListener (this);
    addAndMakeVisible (sequencerEditorPanel);
    sequencerEditorPanel.setVisible (false);

    sequencerParameterPickerPanel.setTitle ("Sequencer parameter grid");
    sequencerParameterPickerPanel.setDescription (
        "Parameter grid for the current sequencer step. Eight rows per column. "
        "Arrow keys navigate, Home End move within a column, Page Up Page Down move five items, "
        "Control Home End move to first or last. Letters and numbers cycle forward; "
        "Shift plus a letter or number cycles backward. Enter adds and closes. Alt Enter adds and stays open. Escape cancels.");
    sequencerParameterPickerPanel.setJustificationType (juce::Justification::topLeft);
    sequencerParameterPickerPanel.setFont (c64Font (17.0f, true));
    sequencerParameterPickerPanel.setColour (juce::Label::backgroundColourId, c64Blue);
    sequencerParameterPickerPanel.setColour (juce::Label::textColourId, c64LightBlue);
    sequencerParameterPickerPanel.setColour (juce::Label::outlineColourId, c64LightBlue);
    sequencerParameterPickerPanel.setBorderSize (juce::BorderSize<int> (10));
    addAndMakeVisible (sequencerParameterPickerPanel);
    sequencerParameterPickerPanel.setVisible (false);

    sequencerParameterPicker.setTitle ("Sequencer parameter grid");
    sequencerParameterPicker.setWantsKeyboardFocus (true);
    sequencerParameterPicker.addKeyListener (this);
    addAndMakeVisible (sequencerParameterPicker);
    sequencerParameterPicker.setVisible (false);
    buildSequencerParameterPickerList();

    for (auto* control : std::array<juce::Component*, 12> {
             &pageSelector,
             &parameterSelector, &parameterValue, &sequencerButton, &resetParameter, &initializeSynth,
             &previousPreset, &nextPreset, &loadPreset, &savePreset, &help, &aboutButton })
    {
        control->addKeyListener (this);
    }

    presetBrowserPath.setTitle ("Preset browser folder");
    presetBrowserPath.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetBrowserPath);
    presetBrowser.setTitle ("Preset browser");
    presetBrowser.setDescription (
        "Folders and valid LJuno-116 presets. Selecting a preset previews it immediately. Enter opens a folder or confirms a preset. Delete asks before deleting a preset. Backspace goes to the parent folder. Alt C cancels and restores the previous sound.");
    presetBrowser.setAccessible (true);
    presetBrowser.setMultipleSelectionEnabled (false);
    presetBrowser.setRowHeight (32);
    presetBrowser.setOutlineThickness (1);
    presetBrowser.addKeyListener (this);
    addAndMakeVisible (presetBrowser);
    presetBrowserBack.setDescription ("Goes to the parent preset folder. Shortcut Backspace");
    presetBrowserBack.onClick = [this] { goToParentPresetFolder(); };
    presetBrowserBack.addKeyListener (this);
    addAndMakeVisible (presetBrowserBack);
    presetBrowserClose.setDescription (
        "Closes the preset browser and restores the sound from before it was opened. Shortcut Alt C");
    presetBrowserClose.onClick = [this] { closePresetBrowser(); };
    presetBrowserClose.addKeyListener (this);
    addAndMakeVisible (presetBrowserClose);

    presetSaveLabel.setText ("Preset name", juce::dontSendNotification);
    presetSaveLabel.setTitle ("Save preset");
    addAndMakeVisible (presetSaveLabel);
    presetSaveName.setTitle ("Preset name");
    presetSaveName.setDescription ("Type a name for the current patch");
    presetSaveName.setReturnKeyStartsNewLine (false);
    presetSaveName.onReturnKey = [this] { commitPresetSave(); };
    presetSaveName.onEscapeKey = [this] { closePresetSave(); };
    presetSaveName.addKeyListener (this);
    addAndMakeVisible (presetSaveName);
    presetSaveConfirm.onClick = [this] { commitPresetSave(); };
    presetSaveConfirm.addKeyListener (this);
    addAndMakeVisible (presetSaveConfirm);
    presetSaveCancel.setDescription ("Closes Save preset without saving. Shortcut Alt C");
    presetSaveCancel.onClick = [this] { closePresetSave(); };
    presetSaveCancel.addKeyListener (this);
    addAndMakeVisible (presetSaveCancel);

    presetOverwriteLabel.setTitle ("Overwrite preset confirmation");
    presetOverwriteLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (presetOverwriteLabel);
    presetOverwriteYes.setDescription ("Overwrites the existing preset");
    presetOverwriteYes.onClick = [this]
    {
        if (presetDeleteConfirmationOpen)
            confirmPresetDelete();
        else
            confirmPresetOverwrite();
    };
    presetOverwriteYes.addKeyListener (this);
    addAndMakeVisible (presetOverwriteYes);
    presetOverwriteNo.setDescription (
        "Does not overwrite and returns to the preset name. Default choice");
    presetOverwriteNo.onClick = [this]
    {
        if (presetDeleteConfirmationOpen)
            dismissPresetDeleteConfirmation();
        else
            dismissPresetOverwriteConfirmation();
    };
    presetOverwriteNo.addKeyListener (this);
    addAndMakeVisible (presetOverwriteNo);
    presetOverwriteClose.setDescription (
        "Closes Save preset and cancels the save. Shortcut Alt C");
    presetOverwriteClose.onClick = [this]
    {
        if (presetDeleteConfirmationOpen)
            dismissPresetDeleteConfirmation();
        else
            closePresetSave();
    };
    presetOverwriteClose.addKeyListener (this);
    addAndMakeVisible (presetOverwriteClose);

    for (auto* component : std::array<juce::Component*, 12> {
             &presetBrowserPath, &presetBrowser, &presetBrowserBack, &presetBrowserClose,
             &presetSaveLabel, &presetSaveName, &presetSaveConfirm, &presetSaveCancel,
             &presetOverwriteLabel, &presetOverwriteYes, &presetOverwriteNo,
             &presetOverwriteClose })
        component->setVisible (false);

    status.setText ("READY.", juce::dontSendNotification);
    status.setJustificationType (juce::Justification::centred);
    status.setAccessible (true);
    status.setTitle ("Migration status");
    addAndMakeVisible (status);

    setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);
    setWantsKeyboardFocus (true);
    setSize (720, 540);

    stepWidthIndex = juce::jlimit (
        0, static_cast<int> (std::size (stepWidths)) - 1,
        static_cast<int> (processor.parameters.state.getProperty (stepWidthState, 0)));

    lastParameterIndexByPage.resize (static_cast<std::size_t> (pageCount));
    for (int page = 0; page < pageCount; ++page)
        lastParameterIndexByPage[static_cast<std::size_t> (page)] = std::max (0,
            static_cast<int> (processor.parameters.state.getProperty (
                selectedParameterState (page), 0)));

    updateParameterList();
}

LJuno116AudioProcessorEditor::~LJuno116AudioProcessorEditor()
{
    cancelPendingUpdate();
    pendingShortcutFocusTarget = nullptr;
    // Closing the editor is also a cancellation unless Enter already committed
    // the preview. This preserves an unsaved sound when the plug-in UI closes.
    if (presetBrowserOpen && presetBrowserHasPreview
        && presetBrowserOriginalPatch.isValid())
        processor.presetManager.restorePatchSnapshot (presetBrowserOriginalPatch);
    parameterValue.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void LJuno116AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (c64LightBlue);
    const auto screen = getC64ScreenBounds();
    g.setColour (c64Blue);
    g.fillRect (screen);

    // Very light scan lines suggest a composite CRT without reducing legibility.
    g.setColour (juce::Colours::black.withAlpha (0.035f));
    for (int y = screen.getY() + 1; y < screen.getBottom(); y += 3)
        g.drawHorizontalLine (y, static_cast<float> (screen.getX()),
                             static_cast<float> (screen.getRight()));

    if (presetBrowserOpen || presetSaveOpen || aboutOpen)
    {
        g.setColour (c64Blue);
        g.fillRect (getLocalBounds().reduced (28));
        g.setColour (c64LightBlue);
        g.drawRect (getLocalBounds().reduced (28), 4);
    }

}

juce::Rectangle<int> LJuno116AudioProcessorEditor::getC64ScreenBounds() const
{
    return getLocalBounds().reduced (20);
}

void LJuno116AudioProcessorEditor::requestShortcutFocus (juce::Component& target)
{
    pendingShortcutFocusTarget = &target;
    triggerAsyncUpdate();
}

juce::Component* LJuno116AudioProcessorEditor::normaliseFocusTarget (
    juce::Component* source) noexcept
{
    if (source == nullptr || ! isParentOf (source))
        return nullptr;
    if (source == &parameterValue || parameterValue.isParentOf (source))
        return &parameterValue;
    for (auto* control : std::array<juce::Component*, 12> {
             &pageSelector, &parameterSelector, &parameterValue, &sequencerButton, &resetParameter,
             &initializeSynth, &previousPreset, &nextPreset, &loadPreset,
             &savePreset, &help, &aboutButton })
        if (source == control || control->isParentOf (source))
            return control;
    return nullptr;
}

void LJuno116AudioProcessorEditor::rememberOverlayReturnFocus()
{
    if (auto* target = normaliseFocusTarget (
            juce::Component::getCurrentlyFocusedComponent()))
        overlayReturnFocusTarget = target;
}

void LJuno116AudioProcessorEditor::restoreOverlayReturnFocus (
    juce::Component& fallback)
{
    auto* target = overlayReturnFocusTarget;
    overlayReturnFocusTarget = nullptr;
    if (target == nullptr || ! isParentOf (target) || ! target->isShowing()
        || ! target->isEnabled())
        target = &fallback;
    requestShortcutFocus (*target);
}

void LJuno116AudioProcessorEditor::rememberCurrentPageAndParameter()
{
    if (! juce::isPositiveAndBelow (displayedPageIndex,
                                    static_cast<int> (lastParameterIndexByPage.size())))
        return;
    const auto parameterIndex = parameterSelector.getSelectedItemIndex();
    if (parameterIndex < 0)
        return;
    lastParameterIndexByPage[static_cast<std::size_t> (displayedPageIndex)] = parameterIndex;
    processor.parameters.state.setProperty (
        selectedParameterState (displayedPageIndex), parameterIndex, nullptr);
}

void LJuno116AudioProcessorEditor::handleAsyncUpdate()
{
    auto* target = pendingShortcutFocusTarget;
    pendingShortcutFocusTarget = nullptr;
    if (target == nullptr || ! isParentOf (target) || ! target->isShowing())
        return;

    // The numeric field is a temporary child of the slider. Close it only after
    // its key event has returned, then reset JUCE's logical accessibility focus.
    // This prevents grabFocus() from treating the destination slider as an
    // already-focused ancestor and guarantees a fresh UIA focus-changed event.
    parameterValue.hideTextBox (false);
    juce::AccessibilityHandler::clearCurrentlyFocusedHandler();
    target->grabKeyboardFocus();
    if (auto* handler = target->getAccessibilityHandler())
        handler->grabFocus();
}

void LJuno116AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (24);
    title.setBounds (area.removeFromTop (42));
    area.removeFromTop (16);

    const auto showSequencerButton = sequencerButton.isVisible();

    pageSelector.setBounds (area.removeFromTop (36));

    area.removeFromTop (showSequencerButton ? 14 : 24);
    parameterSelector.setBounds (area.removeFromTop (38));
    area.removeFromTop (showSequencerButton ? 8 : 16);
    parameterValue.setBounds (area.removeFromTop (42));
    area.removeFromTop (showSequencerButton ? 8 : 16);
    if (showSequencerButton)
    {
        sequencerButton.setBounds (area.removeFromTop (36).withSizeKeepingCentre (180, 36));
        area.removeFromTop (8);
    }
    else
    {
        sequencerButton.setBounds ({ });
    }
    resetParameter.setBounds (area.removeFromTop (36).withSizeKeepingCentre (180, 36));
    area.removeFromTop (showSequencerButton ? 8 : 12);
    initializeSynth.setBounds (area.removeFromTop (36).withSizeKeepingCentre (180, 36));
    area.removeFromTop (showSequencerButton ? 12 : 18);
    auto presetButtons = area.removeFromTop (36);
    const auto presetButtonWidth = (presetButtons.getWidth() - 30) / 4;
    previousPreset.setBounds (presetButtons.removeFromLeft (presetButtonWidth));
    presetButtons.removeFromLeft (10);
    nextPreset.setBounds (presetButtons.removeFromLeft (presetButtonWidth));
    presetButtons.removeFromLeft (10);
    loadPreset.setBounds (presetButtons.removeFromLeft (presetButtonWidth));
    presetButtons.removeFromLeft (10);
    savePreset.setBounds (presetButtons);
    area.removeFromTop (showSequencerButton ? 10 : 18);
    status.setBounds (area.removeFromTop (42));
    area.removeFromTop (showSequencerButton ? 4 : 8);
    auto infoButtons = area.removeFromTop (36).withSizeKeepingCentre (300, 36);
    help.setBounds (infoButtons.removeFromLeft (140));
    infoButtons.removeFromLeft (20);
    aboutButton.setBounds (infoButtons.removeFromLeft (140));

    auto aboutArea = getLocalBounds().withSizeKeepingCentre (700, 430);
    auto aboutCloseArea = aboutArea.removeFromBottom (42);
    aboutArea.removeFromBottom (10);
    auto aboutContactArea = aboutArea.removeFromBottom (38);
    aboutArea.removeFromBottom (8);
    auto aboutProjectArea = aboutArea.removeFromBottom (38);
    aboutArea.removeFromBottom (10);
    aboutInfo.setBounds (aboutArea);
    aboutProject.setBounds (aboutProjectArea);
    aboutContact.setBounds (aboutContactArea);
    aboutClose.setBounds (aboutCloseArea.withSizeKeepingCentre (140, 36));

    auto sequencerParameterArea = getLocalBounds().reduced (36);
    sequencerParameterPickerPanel.setBounds (sequencerParameterArea);
    auto sequencerPickerBounds = sequencerParameterArea.removeFromTop (54).reduced (12, 8);
    sequencerParameterPicker.setBounds (sequencerPickerBounds);

    auto overlay = getLocalBounds().reduced (36);
    presetBrowserPath.setBounds (overlay.removeFromTop (38));
    overlay.removeFromTop (10);
    auto browserButtons = overlay.removeFromBottom (36);
    presetBrowserBack.setBounds (browserButtons.removeFromLeft (130));
    presetBrowserClose.setBounds (browserButtons.removeFromRight (130));
    overlay.removeFromBottom (10);
    presetBrowser.setBounds (overlay);

    auto saveArea = getLocalBounds().withSizeKeepingCentre (520, 190);
    presetSaveLabel.setBounds (saveArea.removeFromTop (36));
    saveArea.removeFromTop (8);
    presetSaveName.setBounds (saveArea.removeFromTop (38));
    saveArea.removeFromTop (20);
    auto saveButtons = saveArea.removeFromTop (36);
    presetSaveConfirm.setBounds (saveButtons.removeFromLeft (150));
    presetSaveCancel.setBounds (saveButtons.removeFromRight (150));

    auto overwriteArea = getLocalBounds().withSizeKeepingCentre (560, 190);
    presetOverwriteLabel.setBounds (overwriteArea.removeFromTop (76));
    overwriteArea.removeFromTop (18);
    auto overwriteButtons = overwriteArea.removeFromTop (38);
    presetOverwriteYes.setBounds (overwriteButtons.removeFromLeft (140));
    overwriteButtons.removeFromLeft (20);
    presetOverwriteClose.setBounds (overwriteButtons.removeFromRight (140));
    overwriteButtons.removeFromRight (20);
    presetOverwriteNo.setBounds (overwriteButtons);

    sequencerEditorPanel.setBounds (getLocalBounds().reduced (36));
}

void LJuno116AudioProcessorEditor::focusGained (FocusChangeType)
{
    scheduleInitialFocusTransfer();
}

void LJuno116AudioProcessorEditor::visibilityChanged()
{
    if (isVisible())
        scheduleInitialFocusTransfer();
}

std::unique_ptr<juce::AccessibilityHandler>
LJuno116AudioProcessorEditor::createAccessibilityHandler()
{
    // REAPER already exposes the plug-in window title.  Keeping the JUCE editor
    // root in the accessibility tree as another named, focusable element makes
    // screen readers repeat that title before reaching the first real control.
    // An ignored root still exposes all focusable children and routes an initial
    // accessibility focus request to the page selector.
    return createIgnoredAccessibilityHandler (*this);
}

void LJuno116AudioProcessorEditor::selectRelativePage (int delta)
{
    const auto count = static_cast<int> (std::size (ljuno::generated::pages));
    const auto next = juce::jlimit (1, count, pageSelector.getSelectedId() + delta);
    if (next == pageSelector.getSelectedId())
        return;
    // For the shortcuts, rebuild silently and send one ordered screen-reader
    // announcement. Separate ComboBox valueChanged events can otherwise cause
    // the parameter event to replace the page event in NVDA.
    pageSelector.setSelectedId (next, juce::dontSendNotification);
    updateParameterList();
    announcePage();
    requestShortcutFocus (parameterSelector);
}

void LJuno116AudioProcessorEditor::selectPageByInitial (juce::juce_wchar initial,
                                                         bool focusParameterGrid)
{
    const auto wanted = juce::CharacterFunctions::toLowerCase (initial);
    std::vector<int> matches;
    for (int page = 0; page < static_cast<int> (std::size (ljuno::generated::pages)); ++page)
    {
        const auto name = juce::String (ljuno::generated::pages[static_cast<std::size_t> (page)].name);
        if (name.isNotEmpty()
            && juce::CharacterFunctions::toLowerCase (name[0]) == wanted)
            matches.push_back (page + 1);
    }

    if (matches.empty())
        return;

    const auto current = pageSelector.getSelectedId();
    auto target = matches.front();
    const auto currentMatch = std::find (matches.begin(), matches.end(), current);
    if (currentMatch != matches.end())
    {
        const auto next = std::next (currentMatch);
        target = next == matches.end() ? matches.front() : *next;
    }

    pageSelector.setSelectedId (target, juce::dontSendNotification);
    updateParameterList();
    announcePage();
    if (focusParameterGrid)
        // Changing the ComboBox selection and announcing the page is not enough
        // for UIA: JUCE may still regard the old accessibility object as focused.
        // Use the same explicit focus reset as Alt+L so Alt+Shift+G (and every
        // other direct-page shortcut) really lands in that page's parameter grid.
        requestShortcutFocus (parameterSelector);
    else
        pageSelector.grabKeyboardFocus();
}

void LJuno116AudioProcessorEditor::announcePage()
{
    const auto pageIndex = pageSelector.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (pageIndex,
                                    static_cast<int> (std::size (ljuno::generated::pages))))
        return;

    auto message = juce::String ("Page ")
                 + ljuno::generated::pages[static_cast<std::size_t> (pageIndex)].name;
    const auto parameterIndex = parameterSelector.getSelectedItemIndex();
    if (juce::isPositiveAndBelow (parameterIndex,
                                  static_cast<int> (visibleParameterIndices.size())))
    {
        const auto catalogIndex = visibleParameterIndices[static_cast<std::size_t> (parameterIndex)];
        const auto& descriptor = ljuno::generated::parameters[static_cast<std::size_t> (catalogIndex)];
        message += ". " + visibleParameterNames[static_cast<std::size_t> (parameterIndex)];
        if (auto* parameter = processor.parameters.getParameter (descriptor.id))
            message += ", " + parameter->getCurrentValueAsText();
    }
    announceMessage (message);
}

void LJuno116AudioProcessorEditor::updateParameterList()
{
    const auto pageIndex = pageSelector.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (pageIndex, static_cast<int> (std::size (ljuno::generated::pages))))
        return;

    // A contextual rebuild may insert/remove controls before the current one.
    // Remember the stable parameter ID before clearing the list so selection
    // follows that parameter to its new row instead of staying at the old row.
    juce::String selectedParameterId;
    if (pageIndex == displayedPageIndex)
    {
        const auto currentIndex = parameterSelector.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (currentIndex,
                                      static_cast<int> (visibleParameterIndices.size())))
        {
            const auto catalogIndex = visibleParameterIndices[static_cast<std::size_t> (currentIndex)];
            selectedParameterId = ljuno::generated::parameters[static_cast<std::size_t> (catalogIndex)].id;
        }
    }

    rememberCurrentPageAndParameter();
    processor.parameters.state.setProperty (selectedPageState, pageIndex, nullptr);
    valueAttachment.reset();
    visibleParameterIndices.clear();
    visibleParameterNames.clear();
    parameterSelector.clear (juce::dontSendNotification);

    const auto& page = ljuno::generated::pages[static_cast<size_t> (pageIndex)];
    const auto sequencerPageSelected = juce::String (page.name) == "Sequencer";
    if (sequencerButton.isVisible() != sequencerPageSelected)
    {
        sequencerButton.setVisible (sequencerPageSelected);
        resized();
    }
    const auto delayMode = processor.parameters.getRawParameterValue ("slider100") != nullptr
        ? juce::jlimit (0, 2, juce::roundToInt (
            processor.parameters.getRawParameterValue ("slider100")->load()))
        : 0;
    displayedDelayMode = delayMode;
    const auto reverbMode = processor.parameters.getRawParameterValue ("slider140") != nullptr
        ? juce::jlimit (0, 2, juce::roundToInt (
            processor.parameters.getRawParameterValue ("slider140")->load()))
        : 0;
    displayedReverbMode = reverbMode;

    const auto isDelay1Control = [] (const juce::String& id)
    {
        return id == "slider101" || id == "slider102" || id == "slider103"
            || id == "slider104" || id == "slider105" || id == "slider106"
            || id == "slider107" || id == "slider108";
    };
    const auto isDelay2Control = [] (const juce::String& id)
    {
        const auto number = id.substring (6).getIntValue();
        return number >= 298 && number <= 312;
    };
    const auto isDelaySendControl = [] (const juce::String& id)
    {
        const auto number = id.substring (6).getIntValue();
        return number >= 368 && number <= 370;
    };
    const auto isReverbSendControl = [] (const juce::String& id)
    {
        const auto number = id.substring (6).getIntValue();
        return number >= 371 && number <= 373;
    };
    const auto isReverbControl = [] (const juce::String& id)
    {
        const auto number = id.substring (6).getIntValue();
        return (number >= 141 && number <= 156) || number == 161;
    };

    for (std::size_t pageParameter = 0; pageParameter < page.parameterCount; ++pageParameter)
    {
        const auto id = juce::String (page.parameterIds[pageParameter]);
        if (juce::String (page.name) == "FX")
        {
            if (delayMode == 0 && (isDelay1Control (id) || isDelay2Control (id)
                                   || isDelaySendControl (id)))
                continue;
            if (delayMode == 1 && isDelay2Control (id))
                continue;
            if (delayMode == 2 && isDelay1Control (id))
                continue;
            if (reverbMode == 0 && (isReverbControl (id) || isReverbSendControl (id)))
                continue;
        }
        for (int catalogIndex = 0;
             catalogIndex < static_cast<int> (std::size (ljuno::generated::parameters));
             ++catalogIndex)
        {
            const auto& descriptor = ljuno::generated::parameters[static_cast<size_t> (catalogIndex)];
            if (id == descriptor.id)
            {
                visibleParameterIndices.push_back (catalogIndex);
                auto displayName = juce::String (page.parameterNames[pageParameter]);
                visibleParameterNames.emplace_back (displayName);
                auto label = visibleParameterNames.back();
                if (auto* parameter = processor.parameters.getParameter (descriptor.id))
                    label += ", " + parameter->getCurrentValueAsText();
                parameterSelector.addItem (label,
                                           static_cast<int> (visibleParameterIndices.size()));
                break;
            }
        }
    }

    displayedPageIndex = pageIndex;
    const auto rememberedIndex = lastParameterIndexByPage.empty() ? 0
        : lastParameterIndexByPage[static_cast<std::size_t> (pageIndex)];

    auto targetIndex = -1;
    if (selectedParameterId.isNotEmpty())
    {
        for (int index = 0; index < static_cast<int> (visibleParameterIndices.size()); ++index)
        {
            const auto catalogIndex = visibleParameterIndices[static_cast<std::size_t> (index)];
            if (selectedParameterId == ljuno::generated::parameters[static_cast<std::size_t> (catalogIndex)].id)
            {
                targetIndex = index;
                break;
            }
        }
    }

    if (targetIndex < 0)
        targetIndex = juce::jlimit (
            0, std::max (0, static_cast<int> (visibleParameterIndices.size()) - 1),
            rememberedIndex);

    parameterSelector.setSelectedItemIndex (targetIndex, juce::dontSendNotification);
    selectParameter();
}

void LJuno116AudioProcessorEditor::selectParameter()
{
    const auto index = parameterSelector.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (index, static_cast<int> (visibleParameterIndices.size())))
        return;

    if (juce::isPositiveAndBelow (displayedPageIndex,
                                  static_cast<int> (lastParameterIndexByPage.size())))
    {
        lastParameterIndexByPage[static_cast<std::size_t> (displayedPageIndex)] = index;
        processor.parameters.state.setProperty (
            selectedParameterState (displayedPageIndex), index, nullptr);
    }

    const auto catalogIndex = visibleParameterIndices[static_cast<size_t> (index)];
    const auto& descriptor = ljuno::generated::parameters[static_cast<size_t> (catalogIndex)];
    const auto& displayName = visibleParameterNames[static_cast<size_t> (index)];
    valueAttachment.reset();

    parameterValue.setTitle (displayName);
    parameterValue.setDescription (juce::String ("Value for ") + displayName
                                   + ". Alt+V. Enter returns to Parameter. Backspace resets");
    valueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, descriptor.id, parameterValue);

    parameterSelector.setDescription (
        juce::String ("Parameter, row ") + juce::String (index % parametersPerColumn + 1)
        + ", column " + juce::String (index / parametersPerColumn + 1)
        + ". Alt+navigation keys, Enter for Value, or Backspace to reset");
    updateCurrentParameterLabel();
}

void LJuno116AudioProcessorEditor::resetSelectedParameter()
{
    const auto index = parameterSelector.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (index, static_cast<int> (visibleParameterIndices.size())))
        return;

    const auto catalogIndex = visibleParameterIndices[static_cast<size_t> (index)];
    const auto& descriptor = ljuno::generated::parameters[static_cast<size_t> (catalogIndex)];
    if (auto* parameter = processor.parameters.getParameter (descriptor.id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (
            ljuno::initialPatchValue (descriptor.sliderNumber, descriptor.defaultValue)));
        parameter->endChangeGesture();

        updateCurrentParameterLabel();
        notifySelectedValueChanged();
    }
}

void LJuno116AudioProcessorEditor::initializeAllParameters()
{
    auto* returnFocus = normaliseFocusTarget (juce::Component::getCurrentlyFocusedComponent());
    valueAttachment.reset();
    for (const auto& descriptor : ljuno::generated::parameters)
    {
        if (auto* parameter = processor.parameters.getParameter (descriptor.id))
        {
            const auto initial = ljuno::initialPatchValue (descriptor.sliderNumber,
                                                           descriptor.defaultValue);
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (initial));
            parameter->endChangeGesture();
        }
    }

    processor.resetSequencerState();
    selectParameter();
    updateCurrentParameterLabel();
    announceMessage ("Synth and sequencer initialized");
    if (returnFocus != nullptr)
        requestShortcutFocus (*returnFocus);
}

void LJuno116AudioProcessorEditor::updateCurrentParameterLabel()
{
    const auto index = parameterSelector.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (index, static_cast<int> (visibleParameterIndices.size())))
        return;

    const auto catalogIndex = visibleParameterIndices[static_cast<size_t> (index)];
    const auto& descriptor = ljuno::generated::parameters[static_cast<size_t> (catalogIndex)];
    auto label = visibleParameterNames[static_cast<size_t> (index)];
    if (auto* parameter = processor.parameters.getParameter (descriptor.id))
    {
        label += ", " + parameter->getCurrentValueAsText();
        if (juce::String (descriptor.id) == "slider314")
        {
            const auto sequence = processor.getSelectedSequencerIndex();
            const auto destination = sequence == 0 ? juce::String ("Layer 1")
                                   : sequence == 1 ? juce::String ("Layer 2")
                                                   : juce::String ("Noise");
            label += " of " + juce::String (processor.getAvailableSequencerCount())
                  + ", " + destination;
        }
    }

    // changeItemText updates only the popup-menu item. JUCE deliberately regards
    // the ComboBox as unselected when the visible label and the selected item text
    // differ, so refresh the visible label as well without firing onChange.
    parameterSelector.changeItemText (index + 1, label);
    parameterSelector.setSelectedId (index + 1, juce::dontSendNotification);
}

void LJuno116AudioProcessorEditor::notifySelectedValueChanged()
{
    const auto index = parameterSelector.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (index, static_cast<int> (visibleParameterIndices.size())))
        return;

    const auto catalogIndex = visibleParameterIndices[static_cast<size_t> (index)];
    const auto& descriptor = ljuno::generated::parameters[static_cast<size_t> (catalogIndex)];

    if (valueTextEditorShortcutActive && valueTextEditorAnnouncementSource != nullptr)
    {
        if (auto* parameter = processor.parameters.getParameter (descriptor.id))
            announceMessageFrom (*valueTextEditorAnnouncementSource,
                                 parameter->getCurrentValueAsText());
        return;
    }

    if (parameterValue.hasKeyboardFocus (true))
    {
        if (auto* handler = parameterValue.getAccessibilityHandler())
            handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
        return;
    }

    if (auto* parameter = processor.parameters.getParameter (descriptor.id))
        announceMessage (parameter->getCurrentValueAsText());
}

void LJuno116AudioProcessorEditor::setParameterListIndex (int target)
{
    if (visibleParameterIndices.empty())
        return;
    parameterSelector.setSelectedItemIndex (
        juce::jlimit (0, static_cast<int> (visibleParameterIndices.size()) - 1, target),
        juce::sendNotificationSync);
}

void LJuno116AudioProcessorEditor::changeStepWidth (int direction)
{
    const auto next = juce::jlimit (
        0, static_cast<int> (std::size (stepWidths)) - 1,
        stepWidthIndex + direction);
    if (next == stepWidthIndex)
        return;
    stepWidthIndex = next;
    processor.parameters.state.setProperty (stepWidthState, stepWidthIndex, nullptr);
    const auto message = "Step " + juce::String (stepWidths[stepWidthIndex]);
    if (valueTextEditorShortcutActive && valueTextEditorAnnouncementSource != nullptr)
        announceMessageFrom (*valueTextEditorAnnouncementSource, message);
    else
        announceMessage (message);
}

void LJuno116AudioProcessorEditor::changeSelectedValue (int direction, bool pageStep)
{
    const auto index = parameterSelector.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (index, static_cast<int> (visibleParameterIndices.size())))
        return;

    const auto catalogIndex = visibleParameterIndices[static_cast<size_t> (index)];
    const auto& descriptor = ljuno::generated::parameters[static_cast<size_t> (catalogIndex)];
    const auto isChoice = juce::String (descriptor.choices).isNotEmpty();
    const auto multiplier = isChoice ? 1.0
                                     : static_cast<double> (stepWidths[stepWidthIndex]
                                         * (pageStep ? valuePageStep : 1));
    if (auto* parameter = processor.parameters.getParameter (descriptor.id))
    {
        const auto current = static_cast<double> (
            parameter->convertFrom0to1 (parameter->getValue()));
        const auto delta = descriptor.step * multiplier * direction;
        auto next = juce::jlimit (static_cast<double> (descriptor.minimum),
                                  static_cast<double> (descriptor.maximum),
                                  current + delta);
        next = descriptor.minimum
             + std::round ((next - descriptor.minimum) / descriptor.step) * descriptor.step;
        next = juce::jlimit (static_cast<double> (descriptor.minimum),
                             static_cast<double> (descriptor.maximum), next);

        if (std::abs (next - current) < descriptor.step * 0.25)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (
            parameter->convertTo0to1 (static_cast<float> (next)));
        parameter->endChangeGesture();

        updateCurrentParameterLabel();
        notifySelectedValueChanged();
    }
}

void LJuno116AudioProcessorEditor::setSelectedValueToBoundary (bool maximum)
{
    const auto index = parameterSelector.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (index, static_cast<int> (visibleParameterIndices.size())))
        return;
    const auto catalogIndex = visibleParameterIndices[static_cast<size_t> (index)];
    const auto& descriptor = ljuno::generated::parameters[static_cast<size_t> (catalogIndex)];
    if (auto* parameter = processor.parameters.getParameter (descriptor.id))
    {
        const auto target = maximum ? descriptor.maximum : descriptor.minimum;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (target));
        parameter->endChangeGesture();

        updateCurrentParameterLabel();
        notifySelectedValueChanged();
    }
}

void LJuno116AudioProcessorEditor::announceMessage (const juce::String& message)
{
    if (valueTextEditorShortcutActive && valueTextEditorAnnouncementSource != nullptr)
    {
        announceMessageFrom (*valueTextEditorAnnouncementSource, message);
        return;
    }

    status.setText (message, juce::dontSendNotification);
    ljuno::announceToActiveScreenReader (status, message);
}

void LJuno116AudioProcessorEditor::announceMessageFrom (juce::Component& source,
                                                         const juce::String& message)
{
    status.setText (message, juce::dontSendNotification);
    source.setDescription (message);
    if (! valueTextEditorShortcutActive)
        ljuno::announceToActiveScreenReader (source, message);
}

void LJuno116AudioProcessorEditor::scheduleInitialFocusTransfer()
{
    if (initialFocusTransferPending)
        return;

    initialFocusTransferPending = true;
    juce::Timer::callAfterDelay (150, [safeThis = juce::Component::SafePointer (this)]
    {
        if (safeThis == nullptr)
            return;
        safeThis->initialFocusTransferPending = false;
        safeThis->performInitialFocusTransfer();
    });
}

void LJuno116AudioProcessorEditor::performInitialFocusTransfer()
{
    if (! isShowing() || presetBrowserOpen || presetSaveOpen)
        return;

    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    if (focused != nullptr && focused != this && focused != &pageSelector
        && isParentOf (focused))
        return; // The user has already moved to another editor control.

    auto* peer = getPeer();
    auto* handler = pageSelector.getAccessibilityHandler();
    const auto nativeAccessibilityReady = handler != nullptr
        && handler->getNativeImplementation() != nullptr;
    if (peer == nullptr || ! peer->isFocused() || ! nativeAccessibilityReady)
    {
        if (++initialFocusTransferAttempts < 8)
            scheduleInitialFocusTransfer();
        return;
    }

    initialFocusTransferAttempts = 0;

    // If focus was assigned before the native accessibility tree existed,
    // JUCE would otherwise treat the next grab as a no-op. A real logical focus
    // loss/gain recreates the native UIA focus event without synthesising Tab.
    if (pageSelector.hasKeyboardFocus (false))
        pageSelector.giveAwayKeyboardFocus();

    pageSelector.grabKeyboardFocus();
    if (pageSelector.hasKeyboardFocus (false) && handler != nullptr)
        handler->grabFocus();

    if (! pageSelector.hasKeyboardFocus (false)
        && ++initialFocusTransferAttempts < 8)
    {
        scheduleInitialFocusTransfer();
    }
}

void LJuno116AudioProcessorEditor::moveParameterInGrid (int rowDelta, int columnDelta)
{
    const auto current = parameterSelector.getSelectedItemIndex();
    const auto count = static_cast<int> (visibleParameterIndices.size());
    if (! juce::isPositiveAndBelow (current, count))
        return;

    const auto row = current % parametersPerColumn;
    auto target = current;

    if (rowDelta < 0 && row > 0)
        --target;
    else if (rowDelta > 0 && row < parametersPerColumn - 1 && current + 1 < count)
        ++target;
    else if (columnDelta < 0 && current >= parametersPerColumn)
        target -= parametersPerColumn;
    else if (columnDelta > 0 && current + parametersPerColumn < count)
        target += parametersPerColumn;

    if (target != current)
        parameterSelector.setSelectedItemIndex (target, juce::sendNotificationSync);
}

bool LJuno116AudioProcessorEditor::selectNextParameterStartingWith (juce::juce_wchar character,
                                                                     bool backwards)
{
    const auto initial = juce::CharacterFunctions::toLowerCase (character);
    if (! juce::CharacterFunctions::isLetterOrDigit (initial)
        || visibleParameterIndices.empty())
        return false;

    const auto count = static_cast<int> (visibleParameterIndices.size());
    const auto current = parameterSelector.getSelectedItemIndex();
    const auto start = current >= 0 ? current : (backwards ? 0 : -1);
    const auto direction = backwards ? -1 : 1;
    for (int offset = 1; offset <= count; ++offset)
    {
        const auto target = (start + direction * offset + count * 2) % count;
        const auto name = visibleParameterNames[static_cast<std::size_t> (target)].trimStart();
        if (name.isNotEmpty()
            && juce::CharacterFunctions::toLowerCase (name[0]) == initial)
        {
            setParameterListIndex (target);
            return true;
        }
    }

    // Prevent the ComboBox from performing a different action for a valid
    // type-navigation key when the current page has no matching parameter.
    return true;
}

void LJuno116AudioProcessorEditor::buildSequencerParameterPickerList()
{
    sequencerParameterCatalogIndices.clear();
    sequencerParameterNames.clear();
    sequencerParameterPicker.clear (juce::dontSendNotification);

    const auto isExcludedPage = [] (const juce::String& name)
    {
        return name == "Global" || name == "Sequencer"
            || name == "Arp" || name == "Arp Modulation";
    };

    // Exclude pages rather than globally excluding their parameter IDs. If a
    // parameter is intentionally exposed on an allowed synth page as well as an
    // excluded page, it remains available from the allowed page.
    juce::StringArray includedIds;
    for (const auto& page : ljuno::generated::pages)
    {
        if (isExcludedPage (page.name))
            continue;
        for (std::size_t i = 0; i < page.parameterCount; ++i)
        {
            const juce::String id (page.parameterIds[i]);
            if (includedIds.contains (id))
                continue;

            for (std::size_t catalogIndex = 0;
                 catalogIndex < std::size (ljuno::generated::parameters); ++catalogIndex)
            {
                const auto& descriptor = ljuno::generated::parameters[catalogIndex];
                if (id != descriptor.id)
                    continue;
                includedIds.add (id);
                sequencerParameterCatalogIndices.push_back (static_cast<int> (catalogIndex));
                sequencerParameterNames.emplace_back (descriptor.displayName);
                sequencerParameterPicker.addItem (descriptor.displayName,
                                                  static_cast<int> (sequencerParameterCatalogIndices.size()));
                break;
            }
        }
    }
}

void LJuno116AudioProcessorEditor::setSequencerParameterPickerIndex (int index)
{
    if (sequencerParameterCatalogIndices.empty())
        return;
    index = juce::jlimit (0, static_cast<int> (sequencerParameterCatalogIndices.size()) - 1, index);
    sequencerParameterPicker.setSelectedItemIndex (index, juce::dontSendNotification);
    const auto& name = sequencerParameterNames[static_cast<std::size_t> (index)];
    // The picker behaves like the main 8-row parameter grid, but while choosing
    // a lock the screen reader should announce only the parameter name.
    sequencerParameterPicker.setDescription (name);
    announceMessageFrom (sequencerParameterPicker, name);
}

void LJuno116AudioProcessorEditor::moveSequencerParameterPicker (int rowDelta, int columnDelta)
{
    const auto count = static_cast<int> (sequencerParameterCatalogIndices.size());
    const auto current = sequencerParameterPicker.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (current, count))
        return;
    const auto row = current % parametersPerColumn;
    auto target = current;
    if (rowDelta < 0 && row > 0)
        --target;
    else if (rowDelta > 0 && row < parametersPerColumn - 1 && current + 1 < count)
        ++target;
    else if (columnDelta < 0 && current >= parametersPerColumn)
        target -= parametersPerColumn;
    else if (columnDelta > 0 && current + parametersPerColumn < count)
        target += parametersPerColumn;
    if (target != current)
        setSequencerParameterPickerIndex (target);
}

bool LJuno116AudioProcessorEditor::selectSequencerParameterStartingWith (juce::juce_wchar character,
                                                                         bool backwards)
{
    const auto initial = juce::CharacterFunctions::toLowerCase (character);
    if (! juce::CharacterFunctions::isLetterOrDigit (initial)
        || sequencerParameterNames.empty())
        return false;
    const auto count = static_cast<int> (sequencerParameterNames.size());
    const auto current = sequencerParameterPicker.getSelectedItemIndex();
    const auto start = current >= 0 ? current : (backwards ? 0 : -1);
    const auto direction = backwards ? -1 : 1;
    for (int offset = 1; offset <= count; ++offset)
    {
        const auto target = (start + direction * offset + count * 2) % count;
        const auto name = sequencerParameterNames[static_cast<std::size_t> (target)].trimStart();
        if (name.isNotEmpty()
            && juce::CharacterFunctions::toLowerCase (name[0]) == initial)
        {
            setSequencerParameterPickerIndex (target);
            return true;
        }
    }
    return true;
}

void LJuno116AudioProcessorEditor::openSequencerParameterPicker()
{
    if (! sequencerEditorOpen || sequencerEditorLayer != ljuno::SequencerLayer::parameters
        || sequencerParameterPickerOpen || sequencerParameterCatalogIndices.empty())
        return;

    const auto sequence = processor.getSelectedSequencerIndex();
    const auto count = processor.getSequencerStepParameterCount (sequence, sequencerEditorCurrentStep);
    auto pickerIndex = 0;
    if (count > 0)
    {
        sequencerEditorParameterIndex = juce::jlimit (0, count - 1, sequencerEditorParameterIndex);
        const auto lock = processor.getSequencerStepParameterLock (
            sequence, sequencerEditorCurrentStep, sequencerEditorParameterIndex);
        for (int i = 0; i < static_cast<int> (sequencerParameterCatalogIndices.size()); ++i)
            if (ljuno::generated::parameters[static_cast<std::size_t> (
                    sequencerParameterCatalogIndices[static_cast<std::size_t> (i)])].sliderNumber
                == lock.sliderNumber)
            {
                pickerIndex = i;
                break;
            }
    }

    sequencerParameterPickerOpen = true;
    sequencerParameterPickerPanel.setVisible (true);
    sequencerParameterPicker.setVisible (true);
    sequencerParameterPickerPanel.toFront (true);
    sequencerParameterPicker.toFront (true);
    sequencerParameterPickerPanel.setText (
        "SEQUENCER PARAMETER GRID\n\nEnter adds and closes.  Alt+Enter adds and stays open.\n"
        "Arrows navigate an 8-row grid.  Letters/numbers cycle forward; Shift cycles backward.",
        juce::dontSendNotification);
    sequencerParameterPicker.setSelectedItemIndex (pickerIndex, juce::dontSendNotification);
    sequencerParameterPicker.grabKeyboardFocus();
    juce::AccessibilityHandler::clearCurrentlyFocusedHandler();
    if (auto* handler = sequencerParameterPicker.getAccessibilityHandler())
        handler->grabFocus();
    setSequencerParameterPickerIndex (pickerIndex);
}

void LJuno116AudioProcessorEditor::closeSequencerParameterPicker (bool returnToEditor)
{
    if (! sequencerParameterPickerOpen)
        return;
    sequencerParameterPickerOpen = false;
    sequencerParameterPicker.setVisible (false);
    sequencerParameterPickerPanel.setVisible (false);
    if (returnToEditor && sequencerEditorOpen)
    {
        refreshSequencerEditorPanel (false);
        sequencerEditorPanel.grabKeyboardFocus();
        juce::AccessibilityHandler::clearCurrentlyFocusedHandler();
        if (auto* handler = sequencerEditorPanel.getAccessibilityHandler())
            handler->grabFocus();
    }
}

void LJuno116AudioProcessorEditor::addSelectedSequencerParameter (bool keepPickerOpen)
{
    const auto pickerIndex = sequencerParameterPicker.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow (pickerIndex,
                                    static_cast<int> (sequencerParameterCatalogIndices.size())))
        return;
    const auto catalogIndex = sequencerParameterCatalogIndices[static_cast<std::size_t> (pickerIndex)];
    const auto& descriptor = ljuno::generated::parameters[static_cast<std::size_t> (catalogIndex)];
    const auto sequence = processor.getSelectedSequencerIndex();
    auto lockIndex = processor.findSequencerStepParameterLock (
        sequence, sequencerEditorCurrentStep, descriptor.sliderNumber);
    const auto alreadyPresent = lockIndex >= 0;
    if (! alreadyPresent)
        lockIndex = processor.addSequencerStepParameterLock (
            sequence, sequencerEditorCurrentStep, descriptor.sliderNumber,
            processor.getPlainParameterValue (descriptor.id));
    if (lockIndex < 0)
    {
        announceMessageFrom (sequencerParameterPicker,
                             "Parameter limit reached for this step");
        return;
    }
    sequencerEditorParameterIndex = lockIndex;
    const auto count = processor.getSequencerStepParameterCount (sequence, sequencerEditorCurrentStep);
    const auto message = juce::String (alreadyPresent ? "Selected " : "Added ")
                       + descriptor.displayName + ", " + juce::String (count) + " parameters";
    if (keepPickerOpen)
        announceMessageFrom (sequencerParameterPicker, message);
    else
    {
        closeSequencerParameterPicker (true);
        refreshSequencerEditorPanel (false);
        announceMessageFrom (sequencerEditorPanel, currentSequencerParameterText());
    }
}

bool LJuno116AudioProcessorEditor::handleSequencerParameterPickerKey (const juce::KeyPress& key)
{
    const auto keyCode = key.getKeyCode();
    if (keyCode == juce::KeyPress::escapeKey)
    {
        closeSequencerParameterPicker (true);
        return true;
    }
    if (keyCode == juce::KeyPress::returnKey)
    {
        addSelectedSequencerParameter (key.getModifiers().isAltDown());
        return true;
    }
    if (key.getModifiers().isCtrlDown() && keyCode == juce::KeyPress::homeKey)
    { setSequencerParameterPickerIndex (0); return true; }
    if (key.getModifiers().isCtrlDown() && keyCode == juce::KeyPress::endKey)
    { setSequencerParameterPickerIndex (static_cast<int> (sequencerParameterCatalogIndices.size()) - 1); return true; }
    if (keyCode == juce::KeyPress::homeKey)
    {
        const auto current = sequencerParameterPicker.getSelectedItemIndex();
        setSequencerParameterPickerIndex ((current / parametersPerColumn) * parametersPerColumn);
        return true;
    }
    if (keyCode == juce::KeyPress::endKey)
    {
        const auto current = sequencerParameterPicker.getSelectedItemIndex();
        const auto columnStart = (current / parametersPerColumn) * parametersPerColumn;
        setSequencerParameterPickerIndex (juce::jmin (
            columnStart + parametersPerColumn - 1,
            static_cast<int> (sequencerParameterCatalogIndices.size()) - 1));
        return true;
    }
    if (keyCode == juce::KeyPress::pageUpKey)
    { setSequencerParameterPickerIndex (sequencerParameterPicker.getSelectedItemIndex() - parameterListPageStep); return true; }
    if (keyCode == juce::KeyPress::pageDownKey)
    { setSequencerParameterPickerIndex (sequencerParameterPicker.getSelectedItemIndex() + parameterListPageStep); return true; }
    if (keyCode == juce::KeyPress::upKey) { moveSequencerParameterPicker (-1, 0); return true; }
    if (keyCode == juce::KeyPress::downKey) { moveSequencerParameterPicker (1, 0); return true; }
    if (keyCode == juce::KeyPress::leftKey) { moveSequencerParameterPicker (0, -1); return true; }
    if (keyCode == juce::KeyPress::rightKey) { moveSequencerParameterPicker (0, 1); return true; }

    if (! key.getModifiers().isAltDown() && ! key.getModifiers().isCtrlDown()
        && ! key.getModifiers().isCommandDown())
    {
        auto character = key.getTextCharacter();
        if (key.getModifiers().isShiftDown()
            && ! juce::CharacterFunctions::isLetterOrDigit (character)
            && keyCode >= '0' && keyCode <= '9')
            character = static_cast<juce::juce_wchar> (keyCode);
        if (juce::CharacterFunctions::isLetterOrDigit (character))
            return selectSequencerParameterStartingWith (character,
                                                          key.getModifiers().isShiftDown());
    }
    return true;
}


void LJuno116AudioProcessorEditor::setMainControlsEnabled (bool enabled)
{
    for (auto* control : std::array<juce::Component*, 12> {
             &pageSelector,
             &parameterSelector, &parameterValue, &sequencerButton, &resetParameter, &initializeSynth,
             &previousPreset, &nextPreset, &loadPreset, &savePreset, &help, &aboutButton })
        control->setEnabled (enabled);
}

void LJuno116AudioProcessorEditor::openAbout()
{
    if (aboutOpen)
        return;

    rememberOverlayReturnFocus();
    aboutOpen = true;
    setMainControlsEnabled (false);
    aboutInfo.setVisible (true);
    aboutProject.setVisible (true);
    aboutContact.setVisible (true);
    aboutClose.setVisible (true);
    aboutInfo.toFront (false);
    aboutProject.toFront (false);
    aboutContact.toFront (false);
    aboutClose.toFront (false);
    repaint();

    aboutInfo.setCaretPosition (0);
    juce::AccessibilityHandler::clearCurrentlyFocusedHandler();
    aboutInfo.grabKeyboardFocus();
    if (auto* handler = aboutInfo.getAccessibilityHandler())
        handler->grabFocus();
}

void LJuno116AudioProcessorEditor::closeAbout()
{
    if (! aboutOpen)
        return;

    aboutOpen = false;
    aboutInfo.setVisible (false);
    aboutProject.setVisible (false);
    aboutContact.setVisible (false);
    aboutClose.setVisible (false);
    setMainControlsEnabled (true);
    repaint();
    restoreOverlayReturnFocus (aboutButton);
}

void LJuno116AudioProcessorEditor::openProjectPage()
{
    if (juce::URL (ljunoProjectUrl).launchInDefaultBrowser())
        announceMessageFrom (aboutProject, "Opening LJuno-116 GitHub project page");
    else
        announceMessageFrom (aboutProject, "Cannot open the LJuno-116 GitHub project page");
}

void LJuno116AudioProcessorEditor::openContactEmail()
{
    const auto mailUrl = juce::String ("mailto:") + ljunoContactEmail;
    if (juce::URL (mailUrl).launchInDefaultBrowser())
        announceMessageFrom (aboutContact, "Opening email to " + juce::String (ljunoContactEmail));
    else
        announceMessageFrom (aboutContact, "Cannot open the email application");
}

bool LJuno116AudioProcessorEditor::navigateAboutText (const juce::KeyPress& key)
{
    const auto keyCode = key.getKeyCode();
    const auto text = aboutInfo.getText();
    const auto length = text.length();
    auto caret = juce::jlimit (0, length, aboutInfo.getCaretPosition());

    const auto lineStartFor = [&text] (int position)
    {
        auto p = juce::jlimit (0, text.length(), position);
        while (p > 0 && text[p - 1] != '\n' && text[p - 1] != '\r')
            --p;
        return p;
    };

    const auto lineEndFor = [&text] (int position)
    {
        auto p = juce::jlimit (0, text.length(), position);
        while (p < text.length() && text[p] != '\n' && text[p] != '\r')
            ++p;
        return p;
    };

    const auto moveVertical = [&] (int position, int direction)
    {
        const auto currentStart = lineStartFor (position);
        const auto currentEnd = lineEndFor (position);
        const auto column = juce::jlimit (0, currentEnd - currentStart, position - currentStart);

        if (direction < 0)
        {
            if (currentStart <= 0)
                return 0;
            auto previousEnd = currentStart - 1;
            while (previousEnd > 0 && (text[previousEnd] == '\n' || text[previousEnd] == '\r'))
                --previousEnd;
            const auto previousStart = lineStartFor (previousEnd);
            const auto previousLineEnd = lineEndFor (previousStart);
            return previousStart + juce::jmin (column, previousLineEnd - previousStart);
        }

        if (currentEnd >= length)
            return length;
        auto nextStart = currentEnd;
        while (nextStart < length && (text[nextStart] == '\n' || text[nextStart] == '\r'))
            ++nextStart;
        const auto nextEnd = lineEndFor (nextStart);
        return nextStart + juce::jmin (column, nextEnd - nextStart);
    };

    int target = caret;
    if (keyCode == juce::KeyPress::leftKey)
        target = juce::jmax (0, caret - 1);
    else if (keyCode == juce::KeyPress::rightKey)
        target = juce::jmin (length, caret + 1);
    else if (keyCode == juce::KeyPress::upKey)
        target = moveVertical (caret, -1);
    else if (keyCode == juce::KeyPress::downKey)
        target = moveVertical (caret, 1);
    else if (keyCode == juce::KeyPress::homeKey)
        target = key.getModifiers().isCtrlDown() ? 0 : lineStartFor (caret);
    else if (keyCode == juce::KeyPress::endKey)
        target = key.getModifiers().isCtrlDown() ? length : lineEndFor (caret);
    else if (keyCode == juce::KeyPress::pageUpKey)
    {
        for (int i = 0; i < 6; ++i)
            target = moveVertical (target, -1);
    }
    else if (keyCode == juce::KeyPress::pageDownKey)
    {
        for (int i = 0; i < 6; ++i)
            target = moveVertical (target, 1);
    }
    else
    {
        return false;
    }

    aboutInfo.setCaretPosition (target);

    if (keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey)
    {
        if (target >= length)
        {
            announceMessageFrom (aboutInfo, "End of text");
        }
        else
        {
            const auto character = text[target];
            if (character == '\n' || character == '\r')
                announceMessageFrom (aboutInfo, "New line");
            else if (character == ' ' || character == '\t')
                announceMessageFrom (aboutInfo, "Space");
            else
                announceMessageFrom (aboutInfo, juce::String::charToString (character));
        }
    }
    else
    {
        const auto start = lineStartFor (target);
        const auto end = lineEndFor (target);
        const auto line = text.substring (start, end).trimEnd();
        announceMessageFrom (aboutInfo, line.isEmpty() ? "Blank line" : line);
    }

    return true;
}

void LJuno116AudioProcessorEditor::showHelpLanguageMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (helpMenuLookAndFeel.get());
    menu.addSectionHeader ("Help language");
    menu.addItem (1, "English");
    menu.addItem (2, "Italiano");
    menu.addItem (3, juce::String::fromUTF8 ("Español"));
    menu.addItem (4, juce::String::fromUTF8 ("Português"));
    menu.addItem (5, juce::String::fromUTF8 ("Français"));
    menu.addItem (6, juce::String::fromUTF8 ("Русский"));
    menu.addItem (7, juce::String::fromUTF8 ("中文"));
    menu.addItem (8, juce::String::fromUTF8 ("日本語"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&help),
        [safeThis = juce::Component::SafePointer (this)] (int result)
        {
            if (safeThis == nullptr)
                return;
            if (result == 0)
            {
                // PopupMenu temporarily owns the native/UIA focus. Restore it
                // only after the popup has fully closed, then use the normal
                // shortcut-focus path to force a fresh accessibility event.
                juce::Timer::callAfterDelay (50,
                    [safeThis]
                    {
                        if (safeThis != nullptr)
                            safeThis->requestShortcutFocus (safeThis->help);
                    });
                return;
            }
            static constexpr const char* codes[] {
                "en", "it", "es", "pt", "fr", "ru", "zh", "ja"
            };
            if (juce::isPositiveAndBelow (result - 1, static_cast<int> (std::size (codes))))
                safeThis->openHelp (codes[result - 1]);
        });
}

void LJuno116AudioProcessorEditor::openHelp (const juce::String& languageCode)
{
    int dataSize = 0;
    const auto* data = BinaryData::getNamedResource ("LJuno116Help_html", dataSize);
    if (data == nullptr || dataSize <= 0)
    {
        announceMessage ("Help file unavailable");
        return;
    }

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("LJuno-116 Help");
    if (folder.createDirectory().failed())
    {
        announceMessage ("Cannot create help folder");
        return;
    }

    auto html = juce::String::fromUTF8 (data, dataSize);
    const auto safeLanguage = juce::StringArray {
        "en", "it", "es", "pt", "fr", "ru", "zh", "ja"
    }
                                  .contains (languageCode) ? languageCode : "en";
    html = html.replace ("const supported=",
                         "const requestedLanguage='" + safeLanguage
                             + "';const supported=");
    html = html.replace (":'en';document.querySelectorAll",
                         ":requestedLanguage;document.querySelectorAll");

    const auto file = folder.getChildFile ("LJuno-116 Help " + safeLanguage + ".html");
    if (! file.replaceWithText (html, false, false, "\n"))
    {
        announceMessage ("Cannot write help file");
        return;
    }

    // Match the reliable CF_ShellExecute approach used by the original Lua:
    // pass Windows a real .html file, not a file:// URL containing an anchor.
    if (! file.startAsProcess())
        announceMessage ("Cannot open help in the default browser");
}

void LJuno116AudioProcessorEditor::refreshAfterPresetChange()
{
    const auto selectedParameter = parameterSelector.getSelectedItemIndex();
    updateParameterList();
    if (! visibleParameterIndices.empty())
        parameterSelector.setSelectedItemIndex (juce::jlimit (
            0, static_cast<int> (visibleParameterIndices.size()) - 1, selectedParameter),
            juce::sendNotificationSync);
}


juce::String LJuno116AudioProcessorEditor::sequencerLayerName() const
{
    if (sequencerEditorLaunchPage)
        return "Launch Step";

    switch (sequencerEditorLayer)
    {
        case ljuno::SequencerLayer::note:     return "Note";
        case ljuno::SequencerLayer::length:   return "Length";
        case ljuno::SequencerLayer::velocity: return "Velocity";
        case ljuno::SequencerLayer::repeat:   return "Repeat";
        case ljuno::SequencerLayer::shift:      return "Shift";
        case ljuno::SequencerLayer::parameters: return "Parameters";
        default:                                return "Note";
    }
}

juce::String LJuno116AudioProcessorEditor::sequencerStepValueText (int sequence,
                                                                    int step) const
{
    const auto value = processor.getSequencerStepValue (sequence, step,
                                                         sequencerEditorLayer);
    if (sequencerEditorLayer == ljuno::SequencerLayer::note
        && juce::roundToInt (value) == 0)
        return "Pause";
    if (sequencerEditorLayer == ljuno::SequencerLayer::shift)
    {
        const auto signedShift = (value - 0.5f) * 2.0f;
        return juce::String (signedShift, 2);
    }
    if (sequencerEditorLayer == ljuno::SequencerLayer::parameters)
    {
        const auto count = processor.getSequencerStepParameterCount (sequence, step);
        return juce::String (count) + (count == 1 ? " param" : " params");
    }
    return juce::String (juce::roundToInt (value));
}

juce::String LJuno116AudioProcessorEditor::currentSequencerParameterText() const
{
    const auto sequence = processor.getSelectedSequencerIndex();
    const auto count = processor.getSequencerStepParameterCount (sequence, sequencerEditorCurrentStep);
    if (count <= 0)
        return "0 parameters. Press Enter to add a synth parameter";

    const auto index = juce::jlimit (0, count - 1, sequencerEditorParameterIndex);
    const auto lock = processor.getSequencerStepParameterLock (
        sequence, sequencerEditorCurrentStep, index);
    juce::String name = "Unknown parameter";
    juce::String valueText (lock.value, 4);
    for (const auto& descriptor : ljuno::generated::parameters)
    {
        if (descriptor.sliderNumber != lock.sliderNumber)
            continue;
        name = descriptor.displayName;
        valueText = ljuno::valueToText (descriptor.sliderNumber, lock.value);
        break;
    }
    return juce::String (count) + (count == 1 ? " parameter, " : " parameters, ")
         + "parameter " + juce::String (index + 1) + " of " + juce::String (count)
         + ", " + name + ", " + valueText;
}

void LJuno116AudioProcessorEditor::changeSequencerEditorParameterSelection (int direction)
{
    const auto sequence = processor.getSelectedSequencerIndex();
    const auto count = processor.getSequencerStepParameterCount (sequence, sequencerEditorCurrentStep);
    if (count <= 0)
    {
        sequencerEditorParameterIndex = 0;
        announceMessageFrom (sequencerEditorPanel, currentSequencerParameterText());
        return;
    }
    sequencerEditorParameterIndex = juce::jlimit (0, count - 1,
                                                  sequencerEditorParameterIndex + direction);
    refreshSequencerEditorPanel (false);
    announceMessageFrom (sequencerEditorPanel, currentSequencerParameterText());
}

void LJuno116AudioProcessorEditor::removeSequencerEditorParameter()
{
    const auto sequence = processor.getSelectedSequencerIndex();
    const auto count = processor.getSequencerStepParameterCount (sequence, sequencerEditorCurrentStep);
    if (count <= 0)
        return;
    sequencerEditorParameterIndex = juce::jlimit (0, count - 1, sequencerEditorParameterIndex);
    const auto lock = processor.getSequencerStepParameterLock (
        sequence, sequencerEditorCurrentStep, sequencerEditorParameterIndex);
    juce::String removedName = "Parameter";
    for (const auto& descriptor : ljuno::generated::parameters)
        if (descriptor.sliderNumber == lock.sliderNumber)
        {
            removedName = descriptor.displayName;
            break;
        }
    if (! processor.removeSequencerStepParameterLock (
            sequence, sequencerEditorCurrentStep, sequencerEditorParameterIndex))
        return;
    const auto remaining = processor.getSequencerStepParameterCount (sequence, sequencerEditorCurrentStep);
    sequencerEditorParameterIndex = remaining > 0
        ? juce::jlimit (0, remaining - 1, sequencerEditorParameterIndex) : 0;
    refreshSequencerEditorPanel (false);
    announceMessageFrom (sequencerEditorPanel,
                         "Removed " + removedName + ". " + currentSequencerParameterText());
}

void LJuno116AudioProcessorEditor::refreshSequencerEditorPanel (bool announce)
{
    if (! sequencerEditorOpen)
        return;

    const auto sequence = processor.getSelectedSequencerIndex();
    const auto maximum = processor.getAvailableSequencerCount();
    const auto first = sequencerEditorBlock * 16;
    const auto config = processor.getSequencerConfig (sequence);
    const auto destination = sequence == 0 ? juce::String ("Layer 1")
                           : sequence == 1 ? juce::String ("Layer 2")
                                           : juce::String ("Noise");
    juce::String text;
    text << "SEQUENCER  " << (sequence + 1) << " OF " << maximum
         << "    " << destination.toUpperCase()
         << "    STEPS " << (first + 1) << "-" << (first + 16)
         << "    PAGE " << sequencerLayerName() << "\n\n";

    if (sequencerEditorLaunchPage)
    {
        text << "Launch Step " << config.launchStep << "\n\n";
        text << "Up Down changes Launch Step.  1-6 returns to step layers.\n";
    }
    else
    {
        for (int row = 0; row < 2; ++row)
        {
            for (int column = 0; column < 8; ++column)
            {
                const auto step = first + row * 8 + column;
                const auto current = step == sequencerEditorCurrentStep;
                const auto selected = sequencerEditorSelectedSteps[static_cast<std::size_t> (step)];
                if (current) text << ">";
                else if (selected) text << "*";
                else text << " ";
                text << (step + 1) << ":" << sequencerStepValueText (sequence, step);
                if (column != 7) text << "   ";
            }
            text << "\n";
        }
    }

    static constexpr std::array<int, 9> valueSteps { 1, 5, 10, 15, 20, 25, 30, 35, 40 };
    text << "\nStart " << config.startStep << "   End " << config.endStep
         << "   Launch " << config.launchStep << "   Offset "
         << juce::String (config.launchOffsetMs, 0) << " ms"
         << "   BPM " << juce::String (config.bpmDivision, 4)
         << "   Value Step "
         << valueSteps[static_cast<std::size_t> (sequencerEditorValueStepIndex)];
    sequencerEditorPanel.setText (text, juce::dontSendNotification);
    repaint();

    if (announce)
    {
        juce::String message;
        message << "Sequence " << (sequence + 1) << " of " << maximum
                << ", " << destination
                << ", steps " << (first + 1) << " to " << (first + 16)
                << ", page " << sequencerLayerName();
        if (sequencerEditorLaunchPage)
            message << ", " << config.launchStep;
        message << ", value step "
                << valueSteps[static_cast<std::size_t> (sequencerEditorValueStepIndex)];
        announceMessageFrom (sequencerEditorPanel, message);
    }
}

void LJuno116AudioProcessorEditor::announceSequencerStep()
{
    const auto sequence = processor.getSelectedSequencerIndex();
    if (sequencerEditorLaunchPage)
    {
        const auto config = processor.getSequencerConfig (sequence);
        announceMessageFrom (sequencerEditorPanel,
                             "Launch Step " + juce::String (config.launchStep));
        return;
    }
    juce::String message;
    message << "Step " << (sequencerEditorCurrentStep + 1);
    if (sequencerEditorSelectedSteps[static_cast<std::size_t> (sequencerEditorCurrentStep)])
        message << ", selected";
    if (sequencerEditorLayer == ljuno::SequencerLayer::parameters)
        message << ", Parameters, " << currentSequencerParameterText();
    else
        message << ", " << sequencerLayerName() << ", "
                << sequencerStepValueText (sequence, sequencerEditorCurrentStep);
    announceMessageFrom (sequencerEditorPanel, message);
}

void LJuno116AudioProcessorEditor::openSequencerEditor()
{
    if (presetBrowserOpen || presetSaveOpen || presetOverwriteConfirmationOpen
        || presetDeleteConfirmationOpen)
        return;
    if (sequencerEditorOpen)
        return;

    rememberOverlayReturnFocus();
    sequencerEditorOpen = true;
    sequencerEditorBlock = juce::jlimit (0, 7, sequencerEditorCurrentStep / 16);
    sequencerEditorPanel.setVisible (true);
    sequencerEditorPanel.toFront (true);
    refreshSequencerEditorPanel (false);
    sequencerEditorPanel.grabKeyboardFocus();
    juce::AccessibilityHandler::clearCurrentlyFocusedHandler();
    if (auto* handler = sequencerEditorPanel.getAccessibilityHandler())
        handler->grabFocus();
    refreshSequencerEditorPanel (true);
}

void LJuno116AudioProcessorEditor::closeSequencerEditor()
{
    if (! sequencerEditorOpen)
        return;
    if (sequencerParameterPickerOpen)
        closeSequencerParameterPicker (false);
    sequencerEditorOpen = false;
    sequencerEditorPanel.setVisible (false);
    updateParameterList();
    restoreOverlayReturnFocus (parameterSelector);
}

void LJuno116AudioProcessorEditor::selectSequencerEditorStep (int localStep)
{
    localStep = juce::jlimit (0, 15, localStep);
    const auto step = sequencerEditorBlock * 16 + localStep;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto doublePress = step == sequencerEditorLastStepKey
                          && now - sequencerEditorLastStepTimeMs <= 300.0;
    sequencerEditorCurrentStep = step;
    if (sequencerEditorLayer == ljuno::SequencerLayer::parameters)
    {
        const auto count = processor.getSequencerStepParameterCount (
            processor.getSelectedSequencerIndex(), sequencerEditorCurrentStep);
        sequencerEditorParameterIndex = count > 0
            ? juce::jlimit (0, count - 1, sequencerEditorParameterIndex) : 0;
    }

    if (doublePress)
    {
        auto& selected = sequencerEditorSelectedSteps[static_cast<std::size_t> (step)];
        selected = ! selected;
        sequencerEditorLastStepKey = -1;
        sequencerEditorLastStepTimeMs = 0.0;
        refreshSequencerEditorPanel (false);
        announceMessageFrom (sequencerEditorPanel,
            "Step " + juce::String (step + 1) + (selected ? ", selected" : ", deselected"));
        return;
    }

    sequencerEditorLastStepKey = step;
    sequencerEditorLastStepTimeMs = now;
    refreshSequencerEditorPanel (false);
    announceSequencerStep();
}

void LJuno116AudioProcessorEditor::changeSequencerEditorStepValue (int direction)
{
    static constexpr std::array<int, 9> valueSteps { 1, 5, 10, 15, 20, 25, 30, 35, 40 };
    const auto amount = valueSteps[static_cast<std::size_t> (sequencerEditorValueStepIndex)];
    const auto sequence = processor.getSelectedSequencerIndex();
    if (sequencerEditorLaunchPage)
    {
        if (processor.nudgeSequencerPageParameter ("slider334", static_cast<float> (direction * amount)))
        {
            refreshSequencerEditorPanel (false);
            announceMessageFrom (sequencerEditorPanel,
                "Launch Step " + juce::String (processor.getSequencerConfig (sequence).launchStep));
        }
        return;
    }
    if (sequencerEditorLayer == ljuno::SequencerLayer::parameters)
    {
        const auto count = processor.getSequencerStepParameterCount (sequence, sequencerEditorCurrentStep);
        if (count <= 0)
        {
            announceMessageFrom (sequencerEditorPanel, currentSequencerParameterText());
            return;
        }
        sequencerEditorParameterIndex = juce::jlimit (0, count - 1, sequencerEditorParameterIndex);
        const auto lock = processor.getSequencerStepParameterLock (
            sequence, sequencerEditorCurrentStep, sequencerEditorParameterIndex);
        for (const auto& descriptor : ljuno::generated::parameters)
        {
            if (descriptor.sliderNumber != lock.sliderNumber)
                continue;
            const auto unit = descriptor.step > 0.0f ? descriptor.step
                                                     : (descriptor.maximum - descriptor.minimum) / 100.0f;
            const auto target = juce::jlimit (descriptor.minimum, descriptor.maximum,
                                               lock.value + direction * amount * unit);
            if (processor.setSequencerStepParameterLockValue (
                    sequence, sequencerEditorCurrentStep, sequencerEditorParameterIndex, target))
            {
                refreshSequencerEditorPanel (false);
                announceMessageFrom (sequencerEditorPanel,
                    juce::String (descriptor.displayName) + ", "
                    + ljuno::valueToText (descriptor.sliderNumber, target));
            }
            return;
        }
        return;
    }

    const auto before = processor.getSequencerStepValue (sequence, sequencerEditorCurrentStep,
                                                         sequencerEditorLayer);
    float delta = static_cast<float> (direction * amount);
    if (sequencerEditorLayer == ljuno::SequencerLayer::shift)
        delta *= 0.01f;

    for (int step = 0; step < ljuno::SequencerState::stepsPerSequence; ++step)
        if (sequencerEditorSelectedSteps[static_cast<std::size_t> (step)]
            || step == sequencerEditorCurrentStep)
            processor.addSequencerStepDelta (sequence, step, sequencerEditorLayer, delta);

    const auto after = processor.getSequencerStepValue (sequence, sequencerEditorCurrentStep,
                                                        sequencerEditorLayer);
    refreshSequencerEditorPanel (false);
    if (std::abs (after - before) > 1.0e-7f)
        announceMessageFrom (sequencerEditorPanel,
                             sequencerStepValueText (sequence, sequencerEditorCurrentStep));
}

void LJuno116AudioProcessorEditor::changeSequencerEditorLayer (int direction)
{
    constexpr auto count = static_cast<int> (ljuno::SequencerLayer::count);
    auto layer = static_cast<int> (sequencerEditorLayer);
    layer = (layer + direction + count) % count;
    sequencerEditorLayer = static_cast<ljuno::SequencerLayer> (layer);
    refreshSequencerEditorPanel (false);
    announceMessageFrom (sequencerEditorPanel, "Layer " + sequencerLayerName());
}

void LJuno116AudioProcessorEditor::changeSequencerEditorBlock (int direction)
{
    const auto target = juce::jlimit (0, 7, sequencerEditorBlock + direction);
    if (target == sequencerEditorBlock)
        return;
    const auto local = sequencerEditorCurrentStep % 16;
    sequencerEditorBlock = target;
    sequencerEditorCurrentStep = sequencerEditorBlock * 16 + local;
    refreshSequencerEditorPanel (false);
    announceMessageFrom (sequencerEditorPanel,
        "Steps " + juce::String (sequencerEditorBlock * 16 + 1) + " to "
        + juce::String (sequencerEditorBlock * 16 + 16));
}

void LJuno116AudioProcessorEditor::changeSequencerEditorSequence (int direction)
{
    const auto old = processor.getSelectedSequencerIndex();
    const auto target = juce::jlimit (0, processor.getAvailableSequencerCount() - 1,
                                      old + direction);
    if (target == old)
        return;
    processor.selectSequencerFromEditor (target);
    refreshSequencerEditorPanel (false);
    const auto destination = target == 0 ? juce::String ("Layer 1")
                           : target == 1 ? juce::String ("Layer 2")
                                         : juce::String ("Noise");
    announceMessageFrom (sequencerEditorPanel,
        "Sequence " + juce::String (target + 1) + " of "
        + juce::String (processor.getAvailableSequencerCount())
        + ", " + destination);
}

bool LJuno116AudioProcessorEditor::handleSequencerEditorKey (const juce::KeyPress& key)
{
    const auto keyCode = key.getKeyCode();
    const auto character = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (keyCode == juce::KeyPress::escapeKey)
    {
        closeSequencerEditor();
        return true;
    }
    if (keyCode == juce::KeyPress::returnKey
        && ! sequencerEditorLaunchPage
        && sequencerEditorLayer == ljuno::SequencerLayer::parameters)
    {
        openSequencerParameterPicker();
        return true;
    }
    if ((keyCode == juce::KeyPress::deleteKey || keyCode == juce::KeyPress::backspaceKey)
        && ! sequencerEditorLaunchPage
        && sequencerEditorLayer == ljuno::SequencerLayer::parameters)
    {
        removeSequencerEditorParameter();
        return true;
    }
    if (keyCode == juce::KeyPress::upKey)   { changeSequencerEditorStepValue (1); return true; }
    if (keyCode == juce::KeyPress::downKey) { changeSequencerEditorStepValue (-1); return true; }
    if (keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey)
    {
        static constexpr std::array<int, 9> valueSteps { 1, 5, 10, 15, 20, 25, 30, 35, 40 };
        const auto direction = keyCode == juce::KeyPress::rightKey ? 1 : -1;
        const auto target = juce::jlimit (0, static_cast<int> (valueSteps.size()) - 1,
                                          sequencerEditorValueStepIndex + direction);
        if (target == sequencerEditorValueStepIndex)
            return true;
        sequencerEditorValueStepIndex = target;
        announceMessageFrom (sequencerEditorPanel,
                             "Value Step " + juce::String (valueSteps[static_cast<std::size_t> (target)]));
        return true;
    }
    if (keyCode == juce::KeyPress::homeKey)
    {
        if (! sequencerEditorLaunchPage
            && sequencerEditorLayer == ljuno::SequencerLayer::parameters)
            changeSequencerEditorParameterSelection (-1);
        else
            changeSequencerEditorSequence (-1);
        return true;
    }
    if (keyCode == juce::KeyPress::endKey)
    {
        if (! sequencerEditorLaunchPage
            && sequencerEditorLayer == ljuno::SequencerLayer::parameters)
            changeSequencerEditorParameterSelection (1);
        else
            changeSequencerEditorSequence (1);
        return true;
    }
    if (keyCode == juce::KeyPress::pageUpKey || keyCode == juce::KeyPress::pageDownKey)
    {
        const auto delta = keyCode == juce::KeyPress::pageUpKey ? 0.0625f : -0.0625f;
        if (processor.nudgeSequencerPageParameter ("slider317", delta))
        {
            refreshSequencerEditorPanel (false);
            const auto config = processor.getSequencerConfig (processor.getSelectedSequencerIndex());
            announceMessageFrom (sequencerEditorPanel,
                                 "BPM Division " + juce::String (config.bpmDivision, 4));
        }
        return true;
    }
    if (keyCode == juce::KeyPress::tabKey)
    {
        refreshSequencerEditorPanel (true);
        return true;
    }

    if (character >= '1' && character <= '6')
    {
        sequencerEditorLaunchPage = false;
        sequencerEditorLayer = static_cast<ljuno::SequencerLayer> (character - '1');
        if (sequencerEditorLayer == ljuno::SequencerLayer::parameters)
        {
            const auto count = processor.getSequencerStepParameterCount (
                processor.getSelectedSequencerIndex(), sequencerEditorCurrentStep);
            sequencerEditorParameterIndex = count > 0
                ? juce::jlimit (0, count - 1, sequencerEditorParameterIndex) : 0;
        }
        refreshSequencerEditorPanel (false);
        if (sequencerEditorLayer == ljuno::SequencerLayer::parameters)
            announceMessageFrom (sequencerEditorPanel,
                                 "Page Parameters, " + currentSequencerParameterText());
        else
            announceMessageFrom (sequencerEditorPanel, "Page " + sequencerLayerName());
        return true;
    }
    if (character == '8')
    {
        sequencerEditorLaunchPage = true;
        refreshSequencerEditorPanel (false);
        const auto config = processor.getSequencerConfig (processor.getSelectedSequencerIndex());
        announceMessageFrom (sequencerEditorPanel,
                             "Page Launch Step, " + juce::String (config.launchStep));
        return true;
    }
    if (character == '9') { changeSequencerEditorBlock (-1); return true; }
    if (character == '0') { changeSequencerEditorBlock (1); return true; }
    if (character == 'm')
    {
        const auto config = processor.getSequencerConfig (processor.getSelectedSequencerIndex());
        const auto next = config.legato ? 0.0f : 1.0f;
        if (processor.nudgeSequencerPageParameter ("slider322",
                                                   next - (config.legato ? 1.0f : 0.0f)))
        {
            refreshSequencerEditorPanel (false);
            announceMessageFrom (sequencerEditorPanel,
                                 juce::String ("Legato ") + juce::String (next > 0.5f ? "On" : "Off"));
        }
        return true;
    }
    if (character == 'p')
    {
        auto config = processor.getSequencerConfig (processor.getSelectedSequencerIndex());
        const auto next = (config.playbackMode + 1) % 5;
        if (processor.nudgeSequencerPageParameter ("slider318",
                                                   static_cast<float> (next - config.playbackMode)))
        {
            static constexpr std::array<const char*, 5> names {
                "Cyclic", "Free", "Reverse", "Pendulum", "Random" };
            refreshSequencerEditorPanel (false);
            announceMessageFrom (sequencerEditorPanel,
                                 "Playback Mode " + juce::String (names[static_cast<std::size_t> (next)]));
        }
        return true;
    }
    if (character == 'z')
    {
        if (processor.nudgeSequencerPageParameter ("slider315", -1.0f))
        {
            refreshSequencerEditorPanel (false);
            announceMessageFrom (sequencerEditorPanel,
                "Start Step " + juce::String (processor.getSequencerConfig (
                    processor.getSelectedSequencerIndex()).startStep));
        }
        return true;
    }
    if (character == 'x')
    {
        if (processor.nudgeSequencerPageParameter ("slider315", 1.0f))
        {
            refreshSequencerEditorPanel (false);
            announceMessageFrom (sequencerEditorPanel,
                "Start Step " + juce::String (processor.getSequencerConfig (
                    processor.getSelectedSequencerIndex()).startStep));
        }
        return true;
    }
    if (character == 'c')
    {
        if (processor.nudgeSequencerPageParameter ("slider316", -1.0f))
        {
            refreshSequencerEditorPanel (false);
            announceMessageFrom (sequencerEditorPanel,
                "End Step " + juce::String (processor.getSequencerConfig (
                    processor.getSelectedSequencerIndex()).endStep));
        }
        return true;
    }
    if (character == 'v')
    {
        if (processor.nudgeSequencerPageParameter ("slider316", 1.0f))
        {
            refreshSequencerEditorPanel (false);
            announceMessageFrom (sequencerEditorPanel,
                "End Step " + juce::String (processor.getSequencerConfig (
                    processor.getSelectedSequencerIndex()).endStep));
        }
        return true;
    }

    static constexpr std::array<char, 8> firstRow { 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i' };
    static constexpr std::array<char, 8> secondRow { 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k' };
    for (int i = 0; i < 8; ++i)
    {
        if (character == firstRow[static_cast<std::size_t> (i)])
        {
            selectSequencerEditorStep (i);
            return true;
        }
        if (character == secondRow[static_cast<std::size_t> (i)])
        {
            selectSequencerEditorStep (8 + i);
            return true;
        }
    }
    return true; // dedicated editor deliberately swallows every other key
}

void LJuno116AudioProcessorEditor::changePreset (int direction)
{
    juce::String name;
    auto number = 0;
    const auto result = processor.presetManager.loadRelativePreset (direction, name, number);
    if (result.failed())
    {
        announceMessage (result.getErrorMessage());
        return;
    }
    refreshAfterPresetChange();
    if (presetBrowserOpen)
        closePresetBrowser (false);
    if (presetSaveOpen)
        closePresetSave();
    announceMessage ((number > 0 ? juce::String (number) + ": " : juce::String()) + name);
}

void LJuno116AudioProcessorEditor::togglePresetBrowser()
{
    if (presetBrowserOpen)
        closePresetBrowser();
    else
        openPresetBrowser();
}

void LJuno116AudioProcessorEditor::openPresetBrowser()
{
    if (presetBrowserOpen)
        return;

    const auto result = processor.presetManager.ensureLibraryExists();
    if (result.failed())
    {
        announceMessage (result.getErrorMessage());
        return;
    }
    if (presetSaveOpen)
        closePresetSave (false);
    else
        rememberOverlayReturnFocus();
    presetBrowserOriginalPatch = processor.presetManager.capturePatchSnapshot();
    presetBrowserPreviewFile = {};
    presetBrowserHasPreview = false;
    const auto root = processor.presetManager.getLibraryRoot();
    presetBrowserDirectory = root;
    const auto rememberedDirectory = processor.parameters.state.getProperty (
        presetBrowserDirectoryState).toString();
    if (rememberedDirectory.isNotEmpty() && rememberedDirectory != ".")
    {
        const auto candidate = root.getChildFile (rememberedDirectory);
        if (candidate.isDirectory() && processor.presetManager.isInsideLibrary (candidate))
            presetBrowserDirectory = candidate;
    }
    presetBrowserOpen = true;
    setMainControlsEnabled (false);
    for (auto* component : std::array<juce::Component*, 4> {
             &presetBrowserPath, &presetBrowser, &presetBrowserBack, &presetBrowserClose })
    {
        component->setVisible (true);
        component->toFront (false);
    }
    const auto rememberedRow = static_cast<int> (processor.parameters.state.getProperty (
        presetBrowserRowState, 0));
    refreshPresetBrowser (rememberedRow);
    const auto rememberedSelection = processor.parameters.state.getProperty (
        presetBrowserSelectionState).toString();
    if (rememberedSelection.isNotEmpty())
    {
        const auto rememberedFile = root.getChildFile (rememberedSelection);
        for (int row = 0; row < static_cast<int> (presetBrowserEntries.size()); ++row)
            if (presetBrowserEntries[static_cast<std::size_t> (row)].file == rememberedFile)
            {
                const juce::ScopedValueSetter suppressAnnouncements (
                    suppressPresetBrowserAnnouncement, true);
                presetBrowser.selectRow (row);
                presetBrowser.scrollToEnsureRowIsOnscreen (row);
                break;
            }
    }
    repaint();
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this)]
    {
        if (safeThis != nullptr && safeThis->presetBrowserOpen)
            safeThis->focusPresetBrowserAndAnnounce();
    });
}

void LJuno116AudioProcessorEditor::closePresetBrowser (bool restoreOriginalPatch,
                                                        bool restoreFocus)
{
    if (! presetBrowserOpen)
        return;

    const auto root = processor.presetManager.getLibraryRoot();
    processor.parameters.state.setProperty (
        presetBrowserDirectoryState,
        presetBrowserDirectory == root
            ? juce::String (".")
            : presetBrowserDirectory.getRelativePathFrom (root), nullptr);
    const auto selectedRow = presetBrowser.getSelectedRow();
    processor.parameters.state.setProperty (presetBrowserRowState,
                                             std::max (0, selectedRow), nullptr);
    if (juce::isPositiveAndBelow (selectedRow,
                                  static_cast<int> (presetBrowserEntries.size())))
        processor.parameters.state.setProperty (
            presetBrowserSelectionState,
            presetBrowserEntries[static_cast<std::size_t> (selectedRow)].file
                .getRelativePathFrom (root), nullptr);
    else
        processor.parameters.state.removeProperty (presetBrowserSelectionState, nullptr);

    if (restoreOriginalPatch && presetBrowserHasPreview
        && presetBrowserOriginalPatch.isValid())
        processor.presetManager.restorePatchSnapshot (presetBrowserOriginalPatch);

    presetDeleteConfirmationOpen = false;
    presetDeleteFile = {};
    presetDeleteRow = -1;
    for (auto* component : std::array<juce::Component*, 4> {
             &presetOverwriteLabel, &presetOverwriteYes, &presetOverwriteNo,
             &presetOverwriteClose })
        component->setVisible (false);
    presetBrowserOriginalPatch = {};
    presetBrowserPreviewFile = {};
    presetBrowserHasPreview = false;
    presetBrowserOpen = false;
    for (auto* component : std::array<juce::Component*, 4> {
             &presetBrowserPath, &presetBrowser, &presetBrowserBack, &presetBrowserClose })
    {
        component->setEnabled (true);
        component->setVisible (false);
    }
    setMainControlsEnabled (true);
    refreshAfterPresetChange();
    repaint();
    if (restoreFocus)
        restoreOverlayReturnFocus (loadPreset);
}

void LJuno116AudioProcessorEditor::refreshPresetBrowser (int selectedRow)
{
    const juce::ScopedValueSetter suppressAnnouncements (
        suppressPresetBrowserAnnouncement, true);
    presetBrowserEntries = processor.presetManager.listDirectory (presetBrowserDirectory);
    const auto relative = presetBrowserDirectory.getRelativePathFrom (
        processor.presetManager.getLibraryRoot());
    presetBrowserPath.setText (relative == "."
        ? "Preset browser. LJuno-116"
        : "Preset browser. Folder " + relative,
        juce::dontSendNotification);
    presetBrowser.updateContent();
    if (presetBrowserEntries.empty())
    {
        presetBrowser.deselectAllRows();
        announceMessage ("Empty folder");
    }
    else
    {
        presetBrowser.selectRow (juce::jlimit (
            0, static_cast<int> (presetBrowserEntries.size()) - 1, selectedRow));
        presetBrowser.scrollToEnsureRowIsOnscreen (presetBrowser.getSelectedRow());
    }
}

void LJuno116AudioProcessorEditor::focusPresetBrowserAndAnnounce()
{
    if (! presetBrowserOpen)
        return;
    presetBrowser.grabKeyboardFocus();
    const auto row = presetBrowser.getSelectedRow();
    if (previewPresetBrowserRow (row))
        announcePresetBrowserRow (row, true);
}

void LJuno116AudioProcessorEditor::selectPresetBrowserRow (int row)
{
    const auto count = static_cast<int> (presetBrowserEntries.size());
    if (count <= 0)
        return;
    const auto target = juce::jlimit (0, count - 1, row);
    presetBrowser.grabKeyboardFocus();
    presetBrowser.selectRow (target);
    presetBrowser.scrollToEnsureRowIsOnscreen (target);
}

void LJuno116AudioProcessorEditor::announcePresetBrowserRow (int row, bool includeFolder)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (presetBrowserEntries.size())))
        return;
    auto message = getNameForRow (row);
    if (includeFolder)
    {
        const auto relative = presetBrowserDirectory.getRelativePathFrom (
            processor.presetManager.getLibraryRoot());
        message = (relative == "." ? juce::String ("Preset browser. LJuno-116")
                                    : juce::String ("Preset browser. Folder ") + relative)
                + ". " + message;
    }
    announceMessage (message);
}

bool LJuno116AudioProcessorEditor::previewPresetBrowserRow (int row)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (presetBrowserEntries.size())))
        return false;

    const auto& entry = presetBrowserEntries[static_cast<std::size_t> (row)];
    if (entry.isDirectory || entry.file == presetBrowserPreviewFile)
        return true;

    const auto result = processor.presetManager.previewPreset (entry.file);
    if (result.failed())
    {
        announceMessage (result.getErrorMessage());
        return false;
    }

    presetBrowserPreviewFile = entry.file;
    presetBrowserHasPreview = true;
    return true;
}

void LJuno116AudioProcessorEditor::activatePresetBrowserRow (int row)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (presetBrowserEntries.size())))
        return;
    const auto& entry = presetBrowserEntries[static_cast<std::size_t> (row)];
    if (entry.isDirectory)
    {
        presetBrowserDirectory = entry.file;
        refreshPresetBrowser();
        focusPresetBrowserAndAnnounce();
        return;
    }
    if (! previewPresetBrowserRow (row))
        return;

    const auto result = processor.presetManager.commitPresetPreview (entry.file);
    if (result.failed())
    {
        announceMessage (result.getErrorMessage());
        return;
    }

    closePresetBrowser (false);
    announcePage();
}

void LJuno116AudioProcessorEditor::goToParentPresetFolder()
{
    const auto root = processor.presetManager.getLibraryRoot();
    if (presetBrowserDirectory == root)
        return;
    const auto parent = presetBrowserDirectory.getParentDirectory();
    if (! processor.presetManager.isInsideLibrary (parent))
        return;
    const auto previousDirectoryName = presetBrowserDirectory.getFileName();
    presetBrowserDirectory = parent;
    {
        const juce::ScopedValueSetter suppressAnnouncements (
            suppressPresetBrowserAnnouncement, true);
        refreshPresetBrowser();
        for (int row = 0; row < static_cast<int> (presetBrowserEntries.size()); ++row)
            if (presetBrowserEntries[static_cast<std::size_t> (row)].isDirectory
                && presetBrowserEntries[static_cast<std::size_t> (row)].name == previousDirectoryName)
            {
                presetBrowser.selectRow (row);
                break;
            }
    }
    focusPresetBrowserAndAnnounce();
}

void LJuno116AudioProcessorEditor::showPresetSave()
{
    if (presetSaveOpen)
        return;
    const auto result = processor.presetManager.ensureLibraryExists();
    if (result.failed())
    {
        announceMessage (result.getErrorMessage());
        return;
    }
    if (presetBrowserOpen)
        closePresetBrowser (true, false);
    else
        rememberOverlayReturnFocus();
    presetSaveOpen = true;
    presetOverwriteConfirmationOpen = false;
    presetOverwriteFile = {};
    const auto currentPreset = processor.presetManager.getCurrentPresetFile();
    presetSaveDirectory = currentPreset.existsAsFile()
                        ? currentPreset.getParentDirectory()
                        : processor.presetManager.getLibraryRoot();
    setMainControlsEnabled (false);
    for (auto* component : std::array<juce::Component*, 4> {
             &presetSaveLabel, &presetSaveName, &presetSaveConfirm, &presetSaveCancel })
    {
        component->setVisible (true);
        component->toFront (false);
    }
    presetSaveName.setText (currentPreset.existsAsFile()
                                ? currentPreset.getFileNameWithoutExtension()
                                : juce::String(), false);
    repaint();
    presetSaveName.grabKeyboardFocus();
    presetSaveName.selectAll();
}

void LJuno116AudioProcessorEditor::closePresetSave (bool restoreFocus)
{
    if (! presetSaveOpen)
        return;
    presetSaveOpen = false;
    presetOverwriteConfirmationOpen = false;
    presetOverwriteFile = {};
    presetSaveDirectory = {};
    for (auto* component : std::array<juce::Component*, 8> {
             &presetSaveLabel, &presetSaveName, &presetSaveConfirm, &presetSaveCancel,
             &presetOverwriteLabel, &presetOverwriteYes, &presetOverwriteNo,
             &presetOverwriteClose })
        component->setVisible (false);
    setMainControlsEnabled (true);
    repaint();
    if (restoreFocus)
        restoreOverlayReturnFocus (savePreset);
}

void LJuno116AudioProcessorEditor::commitPresetSave()
{
    if (presetOverwriteConfirmationOpen)
        return;

    juce::File savedFile;
    const auto result = processor.presetManager.savePreset (
        presetSaveName.getText(), presetSaveDirectory, savedFile);
    if (result.failed())
    {
        if (savedFile.existsAsFile())
        {
            showPresetOverwriteConfirmation (savedFile);
            return;
        }
        announceMessage (result.getErrorMessage());
        presetSaveName.grabKeyboardFocus();
        return;
    }
    const auto name = savedFile.getFileNameWithoutExtension();
    closePresetSave();
    announceMessage ("Preset saved. " + name);
}

void LJuno116AudioProcessorEditor::showPresetOverwriteConfirmation (
    const juce::File& existingFile)
{
    presetOverwriteConfirmationOpen = true;
    presetOverwriteFile = existingFile;
    for (auto* component : std::array<juce::Component*, 4> {
             &presetSaveLabel, &presetSaveName, &presetSaveConfirm, &presetSaveCancel })
        component->setVisible (false);

    const auto name = existingFile.getFileNameWithoutExtension();
    presetOverwriteLabel.setText (
        "Preset " + name + " already exists. Overwrite?",
        juce::dontSendNotification);
    presetOverwriteLabel.setTitle ("Overwrite preset confirmation");
    presetOverwriteYes.setDescription ("Overwrites the existing preset");
    presetOverwriteNo.setDescription (
        "Does not overwrite and returns to the preset name. Default choice");
    presetOverwriteClose.setDescription (
        "Closes Save preset and cancels the save. Shortcut Alt C");
    for (auto* component : std::array<juce::Component*, 4> {
             &presetOverwriteLabel, &presetOverwriteYes, &presetOverwriteNo,
             &presetOverwriteClose })
    {
        component->setVisible (true);
        component->toFront (false);
    }
    repaint();

    // No is deliberately the default. Moving focus after the accessibility
    // tree has updated lets NVDA, JAWS and Narrator announce the question and
    // its safe choice reliably.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this)]
    {
        if (safeThis != nullptr && safeThis->presetOverwriteConfirmationOpen)
        {
            safeThis->presetOverwriteNo.grabKeyboardFocus();
            safeThis->announceMessage (
                safeThis->presetOverwriteLabel.getText() + " No");
        }
    });
}

void LJuno116AudioProcessorEditor::dismissPresetOverwriteConfirmation()
{
    if (! presetOverwriteConfirmationOpen)
        return;
    presetOverwriteConfirmationOpen = false;
    presetOverwriteFile = {};
    for (auto* component : std::array<juce::Component*, 4> {
             &presetOverwriteLabel, &presetOverwriteYes, &presetOverwriteNo,
             &presetOverwriteClose })
        component->setVisible (false);
    for (auto* component : std::array<juce::Component*, 4> {
             &presetSaveLabel, &presetSaveName, &presetSaveConfirm, &presetSaveCancel })
    {
        component->setVisible (true);
        component->toFront (false);
    }
    repaint();
    presetSaveName.grabKeyboardFocus();
    presetSaveName.selectAll();
    announceMessage ("Preset not overwritten. Enter another name");
}

void LJuno116AudioProcessorEditor::confirmPresetOverwrite()
{
    if (! presetOverwriteConfirmationOpen || ! presetOverwriteFile.existsAsFile())
    {
        dismissPresetOverwriteConfirmation();
        return;
    }

    juce::File savedFile;
    const auto result = processor.presetManager.savePreset (
        presetSaveName.getText(), presetSaveDirectory, savedFile, true);
    if (result.failed())
    {
        announceMessage (result.getErrorMessage());
        return;
    }
    const auto name = savedFile.getFileNameWithoutExtension();
    closePresetSave();
    announceMessage ("Preset overwritten. " + name);
}

void LJuno116AudioProcessorEditor::showPresetDeleteConfirmation()
{
    if (! presetBrowserOpen || presetDeleteConfirmationOpen)
        return;

    const auto row = presetBrowser.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, static_cast<int> (presetBrowserEntries.size()))
        || presetBrowserEntries[static_cast<std::size_t> (row)].isDirectory)
    {
        announceMessage ("Select a preset to delete");
        return;
    }

    presetDeleteConfirmationOpen = true;
    presetDeleteFile = presetBrowserEntries[static_cast<std::size_t> (row)].file;
    presetDeleteRow = row;
    for (auto* component : std::array<juce::Component*, 4> {
             &presetBrowserPath, &presetBrowser, &presetBrowserBack, &presetBrowserClose })
        component->setEnabled (false);

    const auto name = presetBrowserEntries[static_cast<std::size_t> (row)].name;
    presetOverwriteLabel.setTitle ("Delete preset confirmation");
    presetOverwriteLabel.setText (
        "Delete preset " + name + "?", juce::dontSendNotification);
    presetOverwriteYes.setDescription ("Deletes the selected preset");
    presetOverwriteNo.setDescription (
        "Does not delete and returns to the preset browser. Default choice");
    presetOverwriteClose.setDescription (
        "Cancels deletion and returns to the preset browser. Shortcut Alt C");
    for (auto* component : std::array<juce::Component*, 4> {
             &presetOverwriteLabel, &presetOverwriteYes, &presetOverwriteNo,
             &presetOverwriteClose })
    {
        component->setVisible (true);
        component->toFront (false);
    }
    repaint();

    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this)]
    {
        if (safeThis != nullptr && safeThis->presetDeleteConfirmationOpen)
        {
            safeThis->presetOverwriteNo.grabKeyboardFocus();
            safeThis->announceMessage (
                safeThis->presetOverwriteLabel.getText() + " No");
        }
    });
}

void LJuno116AudioProcessorEditor::dismissPresetDeleteConfirmation()
{
    if (! presetDeleteConfirmationOpen)
        return;

    presetDeleteConfirmationOpen = false;
    presetDeleteFile = {};
    presetDeleteRow = -1;
    for (auto* component : std::array<juce::Component*, 4> {
             &presetOverwriteLabel, &presetOverwriteYes, &presetOverwriteNo,
             &presetOverwriteClose })
        component->setVisible (false);
    for (auto* component : std::array<juce::Component*, 4> {
             &presetBrowserPath, &presetBrowser, &presetBrowserBack, &presetBrowserClose })
        component->setEnabled (true);
    repaint();
    presetBrowser.grabKeyboardFocus();
    const auto row = presetBrowser.getSelectedRow();
    announceMessage ("Preset not deleted. " + getNameForRow (row));
}

void LJuno116AudioProcessorEditor::confirmPresetDelete()
{
    if (! presetDeleteConfirmationOpen)
        return;

    const auto file = presetDeleteFile;
    const auto deletedRow = presetDeleteRow;
    const auto name = juce::isPositiveAndBelow (
        deletedRow, static_cast<int> (presetBrowserEntries.size()))
        ? presetBrowserEntries[static_cast<std::size_t> (deletedRow)].name
        : file.getFileNameWithoutExtension();
    const auto result = processor.presetManager.deletePreset (file);
    if (result.failed())
    {
        announceMessage (result.getErrorMessage());
        return;
    }

    const auto root = processor.presetManager.getLibraryRoot();
    if (presetBrowserOriginalPatch.hadCurrentPreset
        && root.getChildFile (presetBrowserOriginalPatch.currentPresetRelativePath) == file)
    {
        presetBrowserOriginalPatch.hadCurrentPreset = false;
        presetBrowserOriginalPatch.currentPresetRelativePath.clear();
    }
    if (presetBrowserPreviewFile == file)
        presetBrowserPreviewFile = {};

    presetDeleteConfirmationOpen = false;
    presetDeleteFile = {};
    presetDeleteRow = -1;
    for (auto* component : std::array<juce::Component*, 4> {
             &presetOverwriteLabel, &presetOverwriteYes, &presetOverwriteNo,
             &presetOverwriteClose })
        component->setVisible (false);
    for (auto* component : std::array<juce::Component*, 4> {
             &presetBrowserPath, &presetBrowser, &presetBrowserBack, &presetBrowserClose })
        component->setEnabled (true);

    refreshPresetBrowser (deletedRow);
    const auto row = presetBrowser.getSelectedRow();
    if (row >= 0)
        previewPresetBrowserRow (row);
    repaint();
    presetBrowser.grabKeyboardFocus();
    announceMessage ("Preset deleted. " + name
        + (row >= 0 ? ". " + getNameForRow (row) : ". Empty folder"));
}

int LJuno116AudioProcessorEditor::getNumRows()
{
    return static_cast<int> (presetBrowserEntries.size());
}

void LJuno116AudioProcessorEditor::paintListBoxItem (int row, juce::Graphics& g,
                                                      int width, int height,
                                                      bool selected)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (presetBrowserEntries.size())))
        return;
    if (selected)
        g.fillAll (c64LightBlue);
    g.setFont (c64Font (16.0f, true));
    g.setColour (selected ? c64Blue : c64LightBlue);
    g.drawText (getNameForRow (row), 8, 0, width - 16, height,
                juce::Justification::centredLeft, true);
}

juce::String LJuno116AudioProcessorEditor::getNameForRow (int row)
{
    if (! juce::isPositiveAndBelow (row, static_cast<int> (presetBrowserEntries.size())))
        return {};
    const auto& entry = presetBrowserEntries[static_cast<std::size_t> (row)];
    return juce::String (row + 1) + ". "
         + (entry.isDirectory ? "Category " : "") + entry.name;
}

void LJuno116AudioProcessorEditor::selectedRowsChanged (int row)
{
    if (! presetBrowserOpen || suppressPresetBrowserAnnouncement)
        return;

    if (previewPresetBrowserRow (row) && presetBrowser.hasKeyboardFocus (true))
        announcePresetBrowserRow (row, false);
}

void LJuno116AudioProcessorEditor::returnKeyPressed (int row)
{
    activatePresetBrowserRow (row);
}

bool LJuno116AudioProcessorEditor::keyPressed (const juce::KeyPress& key,
                                                juce::Component* originatingComponent)
{
    const auto keyCode = key.getKeyCode();

    if (aboutOpen)
    {
        const auto aboutCharacter = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
        if (keyCode == juce::KeyPress::escapeKey
            || (key.getModifiers().isAltDown() && aboutCharacter == 'c'))
        {
            closeAbout();
            return true;
        }

        if (keyCode == juce::KeyPress::tabKey)
        {
            const bool backwards = key.getModifiers().isShiftDown();
            juce::Component* target = nullptr;
            if (! backwards)
            {
                if (originatingComponent == &aboutInfo) target = &aboutProject;
                else if (originatingComponent == &aboutProject) target = &aboutContact;
                else if (originatingComponent == &aboutContact) target = &aboutClose;
                else target = &aboutInfo;
            }
            else
            {
                if (originatingComponent == &aboutInfo) target = &aboutClose;
                else if (originatingComponent == &aboutClose) target = &aboutContact;
                else if (originatingComponent == &aboutContact) target = &aboutProject;
                else target = &aboutInfo;
            }

            if (target != nullptr)
            {
                juce::AccessibilityHandler::clearCurrentlyFocusedHandler();
                target->grabKeyboardFocus();
                if (auto* handler = target->getAccessibilityHandler())
                    handler->grabFocus();
            }
            return true;
        }

        if (originatingComponent == &aboutInfo && navigateAboutText (key))
            return true;

        if (keyCode == juce::KeyPress::returnKey && originatingComponent == &aboutInfo)
        {
            closeAbout();
            return true;
        }

        if (keyCode == juce::KeyPress::returnKey || key.getTextCharacter() == ' ')
        {
            if (originatingComponent == &aboutProject)
            {
                openProjectPage();
                return true;
            }
            if (originatingComponent == &aboutContact)
            {
                openContactEmail();
                return true;
            }
            if (originatingComponent == &aboutClose)
            {
                closeAbout();
                return true;
            }
        }

        // About is modal. Alt shortcuts and unrelated keys are swallowed so
        // they cannot act on the controls behind the About panel.
        if (key.getModifiers().isAltDown())
            return true;

        return true;
    }

    if (auto* editor = dynamic_cast<juce::TextEditor*> (originatingComponent);
        editor != nullptr && ! key.getModifiers().isAltDown()
        && ! key.getModifiers().isCtrlDown() && ! key.getModifiers().isCommandDown())
    {
        const auto character = key.getTextCharacter();
        if (juce::CharacterFunctions::isDigit (character) || character == '.')
        {
            // Pass punctuation verbatim. The active screen reader then chooses
            // its pronunciation from the user's language and punctuation rules.
            announceMessageFrom (*editor, juce::String::charToString (character));
            return false;
        }
    }

    if (keyCode == juce::KeyPress::backspaceKey)
        if (auto* editor = dynamic_cast<juce::TextEditor*> (originatingComponent))
        {
            const auto deleted = editor->getProperties()[deletedValueCharacter].toString();
            if (deleted.isNotEmpty())
                announceMessageFrom (*editor, deleted);
            return false;
        }

    if (keyCode == juce::KeyPress::returnKey)
        if (auto* editor = dynamic_cast<juce::TextEditor*> (originatingComponent);
            editor != nullptr && parameterValue.isParentOf (editor))
        {
            requestShortcutFocus (parameterSelector);
            return true;
        }

    const auto focusControl = [this, originatingComponent] (juce::Component& target)
    {
        if (dynamic_cast<juce::TextEditor*> (originatingComponent) == nullptr)
        {
            target.grabKeyboardFocus();
            return;
        }
        requestShortcutFocus (target);
    };

    const auto lowerCharacter = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());


    if (presetOverwriteConfirmationOpen || presetDeleteConfirmationOpen)
    {
        const auto character = juce::CharacterFunctions::toLowerCase (
            key.getTextCharacter());
        const auto cancelConfirmation = [this]
        {
            if (presetDeleteConfirmationOpen)
                dismissPresetDeleteConfirmation();
            else
                closePresetSave();
        };
        if (keyCode == juce::KeyPress::escapeKey
            || (key.getModifiers().isAltDown() && character == 'c'))
        {
            cancelConfirmation();
            return true;
        }
        if (keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::upKey
            || (! key.getModifiers().isAltDown() && character == 'y'))
        {
            presetOverwriteYes.grabKeyboardFocus();
            return true;
        }
        if (keyCode == juce::KeyPress::rightKey || keyCode == juce::KeyPress::downKey
            || (! key.getModifiers().isAltDown() && character == 'n'))
        {
            presetOverwriteNo.grabKeyboardFocus();
            return true;
        }
        if (keyCode == juce::KeyPress::returnKey)
        {
            if (originatingComponent == &presetOverwriteYes)
            {
                if (presetDeleteConfirmationOpen)
                    confirmPresetDelete();
                else
                    confirmPresetOverwrite();
            }
            else if (originatingComponent == &presetOverwriteClose)
                cancelConfirmation();
            else
            {
                if (presetDeleteConfirmationOpen)
                    dismissPresetDeleteConfirmation();
                else
                    dismissPresetOverwriteConfirmation();
            }
            return true;
        }
        return false;
    }

    // The preset browser is keyboard-modal. Handle it before any of the
    // plugin-wide Alt/page shortcuts so focus can never escape behind the
    // browser and leave screen-reader accessibility in an inconsistent state.
    if (presetBrowserOpen)
    {
        const auto count = static_cast<int> (presetBrowserEntries.size());
        const auto character = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

        // Keep the historical browser close shortcuts, but suppress every
        // other Alt combination while the browser is open.
        if (key.getModifiers().isAltDown())
        {
            if (character == 'b' || character == 'c')
                closePresetBrowser();
            return true;
        }

        if (keyCode == juce::KeyPress::escapeKey) { closePresetBrowser(); return true; }
        if (keyCode == juce::KeyPress::backspaceKey) { goToParentPresetFolder(); return true; }
        if (keyCode == juce::KeyPress::deleteKey)
        { showPresetDeleteConfirmation(); return true; }
        if (keyCode == juce::KeyPress::returnKey)
        { activatePresetBrowserRow (presetBrowser.getSelectedRow()); return true; }
        if (count > 0 && keyCode == juce::KeyPress::upKey)
        { selectPresetBrowserRow (presetBrowser.getSelectedRow() - 1); return true; }
        if (count > 0 && keyCode == juce::KeyPress::downKey)
        { selectPresetBrowserRow (presetBrowser.getSelectedRow() + 1); return true; }
        if (count > 0 && keyCode == juce::KeyPress::pageUpKey)
        { selectPresetBrowserRow (presetBrowser.getSelectedRow() - 10); return true; }
        if (count > 0 && keyCode == juce::KeyPress::pageDownKey)
        { selectPresetBrowserRow (presetBrowser.getSelectedRow() + 10); return true; }
        if (count > 0 && keyCode == juce::KeyPress::homeKey)
        { selectPresetBrowserRow (0); return true; }
        if (count > 0 && keyCode == juce::KeyPress::endKey)
        { selectPresetBrowserRow (count - 1); return true; }

        // No other key is allowed to leak to the underlying editor/host while
        // the browser is open. Escape/Enter/navigation above remain active.
        return true;
    }

    if (sequencerParameterPickerOpen)
        return handleSequencerParameterPickerKey (key);

    if (key.getModifiers().isAltDown() && lowerCharacter == 'q')
    {
        if (sequencerEditorOpen)
            closeSequencerEditor();
        else
            openSequencerEditor();
        return true;
    }

    if (sequencerEditorOpen)
        return handleSequencerEditorKey (key);

    if (key.getModifiers().isAltDown()
        && juce::CharacterFunctions::toLowerCase (key.getTextCharacter()) == 'c'
        && presetSaveOpen)
    {
        closePresetSave();
        return true;
    }

    if (key.getModifiers().isAltDown()
        && juce::CharacterFunctions::toLowerCase (key.getTextCharacter()) == 'a')
    {
        openAbout();
        return true;
    }

    if (key.getModifiers().isAltDown()
        && juce::CharacterFunctions::toLowerCase (key.getTextCharacter()) == 'h')
    {
        showHelpLanguageMenu();
        return true;
    }

    if (originatingComponent == &pageSelector
        && ! key.getModifiers().isAltDown()
        && ! key.getModifiers().isCtrlDown()
        && ! key.getModifiers().isCommandDown())
    {
        const auto pageCount = pageSelector.getNumItems();
        const auto currentPage = pageSelector.getSelectedItemIndex();
        auto targetPage = currentPage;

        if (keyCode == juce::KeyPress::homeKey)
            targetPage = 0;
        else if (keyCode == juce::KeyPress::endKey)
            targetPage = pageCount - 1;
        else if (keyCode == juce::KeyPress::pageUpKey)
            targetPage = juce::jmax (0, currentPage - parameterListPageStep);
        else if (keyCode == juce::KeyPress::pageDownKey)
            targetPage = juce::jmin (pageCount - 1,
                                     currentPage + parameterListPageStep);

        if (targetPage != currentPage && juce::isPositiveAndBelow (targetPage, pageCount))
        {
            // Handle these explicitly because native ComboBox implementations do
            // not agree on Home/End direction when the popup is closed. Follow
            // the same ordering as the parameter grid: Home starts, End finishes.
            pageSelector.setSelectedItemIndex (targetPage, juce::dontSendNotification);
            updateParameterList();
            announcePage();
            return true;
        }

        if (keyCode == juce::KeyPress::homeKey || keyCode == juce::KeyPress::endKey
            || keyCode == juce::KeyPress::pageUpKey || keyCode == juce::KeyPress::pageDownKey)
            return true;

        const auto character = key.getTextCharacter();
        if (juce::CharacterFunctions::isLetterOrDigit (character))
        {
            // Keep focus in the page selector so repeated initials cycle,
            // e.g. L alternates between LFO 1 and LFO 2.
            selectPageByInitial (character, false);
            return true;
        }
    }

    if (key.getModifiers().isAltDown() && key.getModifiers().isShiftDown()
        && ! key.getModifiers().isCtrlDown())
    {
        const auto character = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
        if (juce::CharacterFunctions::isLetter (character))
        {
            selectPageByInitial (character);
            return true;
        }
    }

    if (key.getModifiers().isAltDown())
    {
        const auto character = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
        if (character == '+' || keyCode == juce::KeyPress::numberPadAdd
            || (keyCode == '=' && key.getModifiers().isShiftDown()))
        {
            changePreset (1);
            return true;
        }

        if (character == '-' || keyCode == '-' || keyCode == juce::KeyPress::numberPadSubtract)
        {
            changePreset (-1);
            return true;
        }

        if (character == 'b')
        {
            togglePresetBrowser();
            return true;
        }

        if (character == 'c' && presetBrowserOpen)
        {
            closePresetBrowser();
            return true;
        }

        if (character == 's')
        {
            showPresetSave();
            return true;
        }
        if (character == 'p')
        {
            selectRelativePage (-1);
            return true;
        }

        if (character == 'n')
        {
            selectRelativePage (1);
            return true;
        }

        if (character == 'l')
        {
            focusControl (parameterSelector);
            return true;
        }

        if (character == 'd')
        {
            focusControl (pageSelector);
            return true;
        }

        if (character == 'v')
        {
            focusControl (parameterValue);
            return true;
        }

        if (character == 'e')
        {
            parameterValue.showTextBox();
            return true;
        }

        if (character == 'r')
        {
            resetSelectedParameter();
            return true;
        }

        if (character == 'i')
        {
            initializeAllParameters();
            return true;
        }

        if (keyCode == juce::KeyPress::upKey)    { changeSelectedValue (1, false); return true; }
        if (keyCode == juce::KeyPress::downKey)  { changeSelectedValue (-1, false); return true; }
        if (keyCode == juce::KeyPress::leftKey)  { changeStepWidth (-1); return true; }
        if (keyCode == juce::KeyPress::rightKey) { changeStepWidth (1); return true; }
        if (keyCode == juce::KeyPress::homeKey)  { setSelectedValueToBoundary (true); return true; }
        if (keyCode == juce::KeyPress::endKey)   { setSelectedValueToBoundary (false); return true; }
        if (keyCode == juce::KeyPress::pageUpKey)   { changeSelectedValue (1, true); return true; }
        if (keyCode == juce::KeyPress::pageDownKey) { changeSelectedValue (-1, true); return true; }

        // The Slider's temporary TextEditor would otherwise insert the letter
        // for an unassigned Alt combination into the numeric value.
        if (dynamic_cast<juce::TextEditor*> (originatingComponent) != nullptr)
            return true;
    }

    if (presetSaveOpen)
    {
        if (keyCode == juce::KeyPress::escapeKey) { closePresetSave(); return true; }
        if (keyCode == juce::KeyPress::returnKey) { commitPresetSave(); return true; }
        return false;
    }

    if (originatingComponent == &parameterSelector)
    {
        if (keyCode == juce::KeyPress::backspaceKey)
        {
            resetSelectedParameter();
            return true;
        }
        if (keyCode == juce::KeyPress::returnKey)
        {
            parameterValue.grabKeyboardFocus();
            return true;
        }
        if (key.getModifiers().isCtrlDown() && keyCode == juce::KeyPress::homeKey)
        { setParameterListIndex (0); return true; }
        if (key.getModifiers().isCtrlDown() && keyCode == juce::KeyPress::endKey)
        { setParameterListIndex (static_cast<int> (visibleParameterIndices.size()) - 1); return true; }
        if (keyCode == juce::KeyPress::homeKey)
        {
            const auto current = parameterSelector.getSelectedItemIndex();
            setParameterListIndex ((current / parametersPerColumn) * parametersPerColumn);
            return true;
        }
        if (keyCode == juce::KeyPress::endKey)
        {
            const auto current = parameterSelector.getSelectedItemIndex();
            const auto columnStart = (current / parametersPerColumn) * parametersPerColumn;
            setParameterListIndex (juce::jmin (columnStart + parametersPerColumn - 1,
                                               static_cast<int> (visibleParameterIndices.size()) - 1));
            return true;
        }
        if (keyCode == juce::KeyPress::pageUpKey)
        { setParameterListIndex (parameterSelector.getSelectedItemIndex() - parameterListPageStep); return true; }
        if (keyCode == juce::KeyPress::pageDownKey)
        { setParameterListIndex (parameterSelector.getSelectedItemIndex() + parameterListPageStep); return true; }

        if (! key.getModifiers().isAltDown()
            && ! key.getModifiers().isCtrlDown()
            && ! key.getModifiers().isCommandDown())
        {
            auto character = key.getTextCharacter();
            if (key.getModifiers().isShiftDown()
                && ! juce::CharacterFunctions::isLetterOrDigit (character)
                && keyCode >= '0' && keyCode <= '9')
                character = static_cast<juce::juce_wchar> (keyCode);
            if (selectNextParameterStartingWith (character, key.getModifiers().isShiftDown()))
                return true;
        }

        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            moveParameterInGrid (-1, 0);
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            moveParameterInGrid (1, 0);
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::leftKey)
        {
            moveParameterInGrid (0, -1);
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            moveParameterInGrid (0, 1);
            return true;
        }
    }

    if (originatingComponent == &parameterValue)
    {
        if (keyCode == juce::KeyPress::backspaceKey)
        {
            resetSelectedParameter();
            return true;
        }
        if (keyCode == juce::KeyPress::returnKey)
        {
            parameterSelector.grabKeyboardFocus();
            return true;
        }
        if (keyCode == juce::KeyPress::leftKey)  { changeStepWidth (-1); return true; }
        if (keyCode == juce::KeyPress::rightKey) { changeStepWidth (1); return true; }
        if (keyCode == juce::KeyPress::upKey)    { changeSelectedValue (1, false); return true; }
        if (keyCode == juce::KeyPress::downKey)  { changeSelectedValue (-1, false); return true; }
        if (keyCode == juce::KeyPress::pageUpKey)   { changeSelectedValue (1, true); return true; }
        if (keyCode == juce::KeyPress::pageDownKey) { changeSelectedValue (-1, true); return true; }
        if (keyCode == juce::KeyPress::homeKey) { setSelectedValueToBoundary (true); return true; }
        if (keyCode == juce::KeyPress::endKey)  { setSelectedValueToBoundary (false); return true; }
    }

    return false;
}
