#include "InspectorComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
    juce::String formatInspectorTime(double seconds)
    {
        const auto mins = static_cast<int>(seconds / 60.0);
        const auto secs = seconds - static_cast<double>(mins) * 60.0;
        return juce::String::formatted("%02d:%06.3f", mins, secs);
    }

    double parseTimeText(const juce::String& text)
    {
        const auto trimmed = text.trim();

        if (trimmed.containsChar(':'))
        {
            const auto mins = trimmed.upToFirstOccurrenceOf(":", false, false).getDoubleValue();
            const auto secs = trimmed.fromFirstOccurrenceOf(":", false, false).getDoubleValue();
            return mins * 60.0 + secs;
        }

        return trimmed.getDoubleValue();
    }

    void styleLabel(juce::Label& label, const juce::String& text, bool rightAlign = true)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(rightAlign ? juce::Justification::centredRight
                                              : juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, Palette::textDim);
        label.setFont(juce::Font(juce::FontOptions().withHeight(12.5f)));
    }

    void styleEditor(juce::TextEditor& editor, bool readOnly = false)
    {
        editor.setMultiLine(false);
        editor.setReadOnly(readOnly);
        editor.setScrollbarsShown(false);
        editor.setColour(juce::TextEditor::backgroundColourId, Palette::fieldBg);
        editor.setColour(juce::TextEditor::outlineColourId, Palette::divider);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, Palette::controlDown);
        editor.setColour(juce::TextEditor::textColourId, Palette::textPrimary);
        editor.setIndents(6, 0);
        editor.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    }

    class PlaceholderTab : public juce::Component
    {
    public:
        explicit PlaceholderTab(const juce::String& text) : message(text) {}

        void paint(juce::Graphics& g) override
        {
            g.fillAll(Palette::panelBg);
            g.setColour(Palette::textDim);
            g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
            g.drawText(message, getLocalBounds(), juce::Justification::centred);
        }

    private:
        juce::String message;
    };

    class WaveformView : public juce::Component
    {
    public:
        std::function<void(double, double)> onTrimChanged;

        void setFile(const juce::File& file, double newTrimStart, double newTrimEnd)
        {
            trimStart = newTrimStart;
            trimEnd = newTrimEnd;
            lengthSeconds = 0.0;
            peaks.clear();

            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();

            if (auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file));
                reader != nullptr)
            {
                lengthSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;

                const int columns = 960;
                peaks.assign(static_cast<size_t>(columns) * 2, 0.0f);

                const auto total = reader->lengthInSamples;
                juce::AudioBuffer<float> chunk(static_cast<int>(juce::jmin(2u, reader->numChannels)),
                                               16384);

                for (juce::int64 start = 0; start < total; start += 16384)
                {
                    const auto toRead = static_cast<int>(juce::jmin<juce::int64>(16384, total - start));
                    reader->read(&chunk, 0, toRead, start, true, true);

                    for (int i = 0; i < toRead; ++i)
                    {
                        auto sample = chunk.getSample(0, i);

                        if (chunk.getNumChannels() > 1)
                            sample = 0.5f * (sample + chunk.getSample(1, i));

                        const auto column = static_cast<size_t>((start + i) * columns / total);
                        peaks[column * 2] = juce::jmin(peaks[column * 2], sample);
                        peaks[column * 2 + 1] = juce::jmax(peaks[column * 2 + 1], sample);
                    }
                }
            }

            if (trimEnd <= trimStart || trimEnd > lengthSeconds)
                trimEnd = lengthSeconds;

            repaint();
        }

        void setTrim(double newTrimStart, double newTrimEnd)
        {
            trimStart = newTrimStart;
            trimEnd = (newTrimEnd <= newTrimStart || newTrimEnd > lengthSeconds)
                          ? lengthSeconds
                          : newTrimEnd;
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();

            g.setColour(Palette::fieldBg);
            g.fillRoundedRectangle(bounds, 6.0f);

            auto waveArea = bounds.reduced(6.0f, 8.0f);

            if (peaks.empty() || lengthSeconds <= 0.0)
            {
                g.setColour(Palette::textDim);
                g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
                g.drawText("No audio loaded", waveArea, juce::Justification::centred);
                return;
            }

            const auto columns = static_cast<int>(peaks.size() / 2);
            const auto width = static_cast<int>(waveArea.getWidth());
            const auto midY = waveArea.getCentreY();
            const auto halfHeight = waveArea.getHeight() * 0.5f;

            g.setColour(Palette::standbyGreen.withAlpha(0.75f));

            for (int x = 0; x < width; ++x)
            {
                const auto column = juce::jmin(columns - 1, x * columns / width);
                const auto bottom = midY - peaks[column * 2] * halfHeight;
                const auto top = midY - peaks[column * 2 + 1] * halfHeight;
                g.drawVerticalLine(static_cast<int>(waveArea.getX()) + x, top,
                                   juce::jmax(top + 1.0f, bottom));
            }

            const auto startX = timeToX(trimStart);
            const auto endX = timeToX(trimEnd);

            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRect(waveArea.getX(), waveArea.getY(), startX - waveArea.getX(),
                       waveArea.getHeight());
            g.fillRect(endX, waveArea.getY(), waveArea.getRight() - endX, waveArea.getHeight());

            g.setColour(juce::Colours::white);
            g.fillRect(startX - 1.5f, waveArea.getY(), 3.0f, waveArea.getHeight());
            g.fillRect(endX - 1.5f, waveArea.getY(), 3.0f, waveArea.getHeight());
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            const auto x = static_cast<float>(e.x);

            if (std::abs(x - timeToX(trimStart)) < 8.0f)
                dragging = 1;
            else if (std::abs(x - timeToX(trimEnd)) < 8.0f)
                dragging = 2;
            else
                dragging = 0;

            if (dragging != 0)
                mouseDrag(e);
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (dragging == 0 || lengthSeconds <= 0.0)
                return;

            const auto time = xToTime(static_cast<float>(e.x));

            if (dragging == 1)
                trimStart = juce::jlimit(0.0, trimEnd - 0.01, time);
            else
                trimEnd = juce::jlimit(trimStart + 0.01, lengthSeconds, time);

            repaint();

            if (onTrimChanged != nullptr)
                onTrimChanged(trimStart, trimEnd);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            dragging = 0;
        }

    private:
        float timeToX(double time) const
        {
            return 6.0f + static_cast<float>(time / juce::jmax(0.001, lengthSeconds))
                        * (static_cast<float>(getWidth()) - 12.0f);
        }

        double xToTime(float x) const
        {
            const auto proportion = (x - 6.0f) / (static_cast<float>(getWidth()) - 12.0f);
            return juce::jlimit(0.0, lengthSeconds, static_cast<double>(proportion) * lengthSeconds);
        }

        std::vector<float> peaks; // min/max pairs
        double lengthSeconds = 0.0;
        double trimStart = 0.0;
        double trimEnd = 0.0;
        int dragging = 0;
    };
}

class InspectorComponent::BasicsTab : public juce::Component
{
public:
    BasicsTab()
    {
        auto addLabel = [this](juce::Label& label, const juce::String& text)
        {
            styleLabel(label, text);
            addAndMakeVisible(label);
        };

        addLabel(numberLabel, "Number:");
        addLabel(durationLabel, "Duration:");
        addLabel(preWaitLabel, "Pre-Wait:");
        addLabel(postWaitLabel, "Post-Wait:");
        addLabel(fadeInLabel, "Fade In:");
        addLabel(fadeOutLabel, "Fade Out:");
        addLabel(continueLabel, "Continue:");
        addLabel(nameLabel, "Name:");
        addLabel(targetLabel, "Target:");
        addLabel(notesLabel, "Notes:");
        notesLabel.setJustificationType(juce::Justification::topRight);
        addLabel(colorLabel, "Color:  Default");
        addLabel(flaggedLabel, "Flagged:");
        addLabel(autoLoadLabel, "Auto-load:");
        addLabel(armedLabel, "Armed:");
        addLabel(skipLabel, "Skip if disarmed:");
        colorLabel.setJustificationType(juce::Justification::centredLeft);

        auto addEditor = [this](juce::TextEditor& editor, bool readOnly)
        {
            styleEditor(editor, readOnly);
            addAndMakeVisible(editor);
        };

        addEditor(numberEditor, false);
        addEditor(durationEditor, true);
        addEditor(preWaitEditor, false);
        addEditor(postWaitEditor, false);
        addEditor(fadeInEditor, false);
        addEditor(fadeOutEditor, false);
        addEditor(nameEditor, false);
        addEditor(targetEditor, true);
        addEditor(notesEditor, false);
        notesEditor.setMultiLine(true);
        notesEditor.setReturnKeyStartsNewLine(true);
        notesEditor.setScrollbarsShown(true);

        auto applyAndRelease = [this](juce::TextEditor& editor)
        {
            applyEdits();
            editor.giveAwayKeyboardFocus();
        };

        for (auto* editor : { &numberEditor, &preWaitEditor, &postWaitEditor,
                              &fadeInEditor, &fadeOutEditor, &nameEditor })
        {
            editor->onReturnKey = [this, applyAndRelease, editor] { applyAndRelease(*editor); };
            editor->onFocusLost = [this] { applyEdits(); };
        }

        notesEditor.onFocusLost = [this] { applyEdits(); };

        continueBox.addItem("Do not continue", 1);
        continueBox.addItem("Auto-continue", 2);
        continueBox.addItem("Auto-follow", 3);
        continueBox.onChange = [this] { applyEdits(); };
        addAndMakeVisible(continueBox);

        auto setupToggle = [this](juce::ToggleButton& toggle)
        {
            toggle.setWantsKeyboardFocus(false);
            toggle.setMouseClickGrabsKeyboardFocus(false);
            toggle.onClick = [this] { applyEdits(); };
            addAndMakeVisible(toggle);
        };

        setupToggle(flaggedToggle);
        setupToggle(autoLoadToggle);
        setupToggle(armedToggle);
        setupToggle(skipToggle);
    }

    void setCue(Cue* cue)
    {
        currentCue = cue;

        if (currentCue == nullptr)
            return;

        numberEditor.setText(cue->number, false);
        durationEditor.setText(formatInspectorTime(cue->getEffectiveDuration()), false);
        preWaitEditor.setText(formatInspectorTime(cue->preWait), false);
        postWaitEditor.setText(formatInspectorTime(cue->postWait), false);
        fadeInEditor.setText(formatInspectorTime(cue->fadeIn), false);
        fadeOutEditor.setText(formatInspectorTime(cue->fadeOut), false);
        nameEditor.setText(cue->name, false);
        targetEditor.setText(cue->file.getFullPathName(), false);
        notesEditor.setText(cue->notes, false);
        continueBox.setSelectedId(cue->continueMode + 1, juce::dontSendNotification);
        flaggedToggle.setToggleState(cue->flagged, juce::dontSendNotification);
        autoLoadToggle.setToggleState(cue->autoLoad, juce::dontSendNotification);
        armedToggle.setToggleState(cue->armed, juce::dontSendNotification);
        skipToggle.setToggleState(cue->skipIfDisarmed, juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::panelBg);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(14);
        r.removeFromTop(4);

        auto col1 = r.removeFromLeft(196);
        auto col3 = r.removeFromRight(260);
        r.removeFromLeft(28);
        r.removeFromRight(28);
        auto col2 = r;

        auto fieldRow = [](juce::Rectangle<int>& area, juce::Label& label, int labelWidth,
                           juce::Component& field)
        {
            auto line = area.removeFromTop(22);
            label.setBounds(line.removeFromLeft(labelWidth));
            line.removeFromLeft(8);
            field.setBounds(line);
            area.removeFromTop(7);
        };

        fieldRow(col1, numberLabel, 72, numberEditor);
        fieldRow(col1, durationLabel, 72, durationEditor);
        fieldRow(col1, preWaitLabel, 72, preWaitEditor);
        fieldRow(col1, postWaitLabel, 72, postWaitEditor);
        fieldRow(col1, fadeInLabel, 72, fadeInEditor);
        fieldRow(col1, fadeOutLabel, 72, fadeOutEditor);
        fieldRow(col1, continueLabel, 72, continueBox);

        fieldRow(col2, nameLabel, 60, nameEditor);
        fieldRow(col2, targetLabel, 60, targetEditor);

        auto notesRow = col2;
        auto notesLabelArea = notesRow.removeFromLeft(68);
        notesLabel.setBounds(notesLabelArea.removeFromTop(22));
        notesEditor.setBounds(notesRow);

        colorLabel.setBounds(col3.removeFromTop(22));
        col3.removeFromTop(7);

        auto toggleRow = [](juce::Rectangle<int>& area, juce::Label& label, juce::Component& field)
        {
            auto line = area.removeFromTop(22);
            field.setBounds(line.removeFromRight(44));
            line.removeFromRight(6);
            label.setBounds(line);
            area.removeFromTop(7);
        };

        toggleRow(col3, flaggedLabel, flaggedToggle);
        toggleRow(col3, autoLoadLabel, autoLoadToggle);
        toggleRow(col3, armedLabel, armedToggle);
        toggleRow(col3, skipLabel, skipToggle);
    }

    std::function<void()> onEdited;

private:
    void applyEdits()
    {
        if (currentCue == nullptr)
            return;

        currentCue->number = numberEditor.getText().trim();
        currentCue->name = nameEditor.getText().trim();
        currentCue->preWait = parseTimeText(preWaitEditor.getText());
        currentCue->postWait = parseTimeText(postWaitEditor.getText());
        currentCue->fadeIn = parseTimeText(fadeInEditor.getText());
        currentCue->fadeOut = parseTimeText(fadeOutEditor.getText());
        currentCue->notes = notesEditor.getText();
        currentCue->continueMode = juce::jlimit(0, 2, continueBox.getSelectedId() - 1);
        currentCue->flagged = flaggedToggle.getToggleState();
        currentCue->autoLoad = autoLoadToggle.getToggleState();
        currentCue->armed = armedToggle.getToggleState();
        currentCue->skipIfDisarmed = skipToggle.getToggleState();

        if (onEdited != nullptr)
            onEdited();
    }

    juce::Label numberLabel, durationLabel, preWaitLabel, postWaitLabel;
    juce::Label fadeInLabel, fadeOutLabel, continueLabel;
    juce::Label nameLabel, targetLabel, notesLabel;
    juce::Label colorLabel, flaggedLabel, autoLoadLabel, armedLabel, skipLabel;
    juce::TextEditor numberEditor, durationEditor, preWaitEditor, postWaitEditor;
    juce::TextEditor fadeInEditor, fadeOutEditor;
    juce::TextEditor nameEditor, targetEditor, notesEditor;
    juce::ComboBox continueBox;
    juce::ToggleButton flaggedToggle, autoLoadToggle, armedToggle, skipToggle;
    Cue* currentCue = nullptr;
};

class InspectorComponent::TriggersTab : public juce::Component
{
public:
    TriggersTab()
    {
        styleLabel(infoLabel, "Hotkey triggers this cue independently of the standby position:", false);
        addAndMakeVisible(infoLabel);

        styleLabel(hotkeyCaption, "Hotkey:");
        addAndMakeVisible(hotkeyCaption);

        styleLabel(hotkeyValue, "None", false);
        hotkeyValue.setColour(juce::Label::textColourId, Palette::textPrimary);
        addAndMakeVisible(hotkeyValue);

        assignButton.setButtonText("Assign...");
        clearButton.setButtonText("Clear");
        assignButton.setWantsKeyboardFocus(false);
        clearButton.setWantsKeyboardFocus(false);

        assignButton.onClick = [this]
        {
            capturing = true;
            assignButton.setButtonText("Press a key...");
            grabKeyboardFocus();
        };

        clearButton.onClick = [this]
        {
            if (currentCue != nullptr)
            {
                currentCue->hotkey.clear();
                refresh();
                notifyEdited();
            }
        };

        addAndMakeVisible(assignButton);
        addAndMakeVisible(clearButton);

        styleLabel(noteLabel, "MIDI, OSC and wall-clock triggers are planned for a future version.",
                   false);
        addAndMakeVisible(noteLabel);

        setWantsKeyboardFocus(true);
    }

    void setCue(Cue* cue)
    {
        currentCue = cue;
        refresh();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (! capturing)
            return false;

        capturing = false;
        assignButton.setButtonText("Assign...");

        if (key != juce::KeyPress::escapeKey && currentCue != nullptr)
        {
            currentCue->hotkey = key.getTextDescription();
            notifyEdited();
        }

        refresh();
        return true;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::panelBg);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(20);
        r.removeFromTop(6);

        infoLabel.setBounds(r.removeFromTop(22));
        r.removeFromTop(14);

        auto row = r.removeFromTop(24);
        hotkeyCaption.setBounds(row.removeFromLeft(70));
        hotkeyValue.setBounds(row.removeFromLeft(120));
        row.removeFromLeft(12);
        assignButton.setBounds(row.removeFromLeft(100));
        row.removeFromLeft(8);
        clearButton.setBounds(row.removeFromLeft(80));

        r.removeFromTop(20);
        noteLabel.setBounds(r.removeFromTop(22));
    }

    std::function<void()> onEdited;

private:
    void refresh()
    {
        hotkeyValue.setText(currentCue != nullptr && currentCue->hotkey.isNotEmpty()
                                ? currentCue->hotkey
                                : "None",
                            juce::dontSendNotification);
    }

    void notifyEdited()
    {
        if (onEdited != nullptr)
            onEdited();
    }

    juce::Label infoLabel, hotkeyCaption, hotkeyValue, noteLabel;
    juce::TextButton assignButton, clearButton;
    Cue* currentCue = nullptr;
    bool capturing = false;
};

class InspectorComponent::IOTab : public juce::Component
{
public:
    IOTab()
    {
        styleLabel(outputCaption, "Output:");
        styleLabel(outputValue, "-", false);
        outputValue.setColour(juce::Label::textColourId, Palette::textPrimary);
        styleLabel(channelsCaption, "Channels:");
        styleLabel(channelsValue, "1 - 2 (stereo)", false);
        channelsValue.setColour(juce::Label::textColourId, Palette::textPrimary);
        styleLabel(noteLabel, "Multi-output routing is planned for a future version.", false);

        for (auto* label : { &outputCaption, &outputValue, &channelsCaption, &channelsValue,
                             &noteLabel })
            addAndMakeVisible(*label);
    }

    void refresh(const juce::String& deviceName)
    {
        outputValue.setText(deviceName, juce::dontSendNotification);
    }

    void setCue(Cue*) {}

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::panelBg);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(20);
        r.removeFromTop(6);

        auto row = r.removeFromTop(22);
        outputCaption.setBounds(row.removeFromLeft(70));
        outputValue.setBounds(row);
        r.removeFromTop(10);

        auto row2 = r.removeFromTop(22);
        channelsCaption.setBounds(row2.removeFromLeft(70));
        channelsValue.setBounds(row2);
        r.removeFromTop(20);

        noteLabel.setBounds(r.removeFromTop(22));
    }

    std::function<void()> onEdited;

private:
    juce::Label outputCaption, outputValue, channelsCaption, channelsValue, noteLabel;
};

class InspectorComponent::TimeLoopsTab : public juce::Component
{
public:
    TimeLoopsTab()
    {
        loopToggle.setButtonText("Loop until stopped");
        loopToggle.setColour(juce::ToggleButton::textColourId, Palette::textPrimary);
        loopToggle.onClick = [this]
        {
            if (currentCue != nullptr)
            {
                currentCue->loop = loopToggle.getToggleState();
                notifyEdited();
            }
        };
        addAndMakeVisible(loopToggle);

        styleLabel(fileDurationCaption, "File duration:");
        styleLabel(fileDurationValue, "-", false);
        fileDurationValue.setColour(juce::Label::textColourId, Palette::textPrimary);
        styleLabel(effectiveCaption, "Playing duration:");
        styleLabel(effectiveValue, "-", false);
        effectiveValue.setColour(juce::Label::textColourId, Palette::textPrimary);
        styleLabel(noteLabel, "With trim points set, the loop wraps within the trimmed region.",
                   false);

        for (auto* label : { &fileDurationCaption, &fileDurationValue, &effectiveCaption,
                             &effectiveValue, &noteLabel })
            addAndMakeVisible(*label);
    }

    void setCue(Cue* cue)
    {
        currentCue = cue;

        if (cue == nullptr)
            return;

        loopToggle.setToggleState(cue->loop, juce::dontSendNotification);
        fileDurationValue.setText(formatInspectorTime(cue->durationSeconds),
                                  juce::dontSendNotification);
        effectiveValue.setText(formatInspectorTime(cue->getEffectiveDuration()),
                               juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::panelBg);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(20);
        r.removeFromTop(6);

        loopToggle.setBounds(r.removeFromTop(24));
        r.removeFromTop(14);

        auto row = r.removeFromTop(22);
        fileDurationCaption.setBounds(row.removeFromLeft(110));
        fileDurationValue.setBounds(row);
        r.removeFromTop(10);

        auto row2 = r.removeFromTop(22);
        effectiveCaption.setBounds(row2.removeFromLeft(110));
        effectiveValue.setBounds(row2);
        r.removeFromTop(20);

        noteLabel.setBounds(r.removeFromTop(22));
    }

    std::function<void()> onEdited;

private:
    void notifyEdited()
    {
        if (onEdited != nullptr)
            onEdited();
    }

    juce::ToggleButton loopToggle;
    juce::Label fileDurationCaption, fileDurationValue, effectiveCaption, effectiveValue, noteLabel;
    Cue* currentCue = nullptr;
};

class InspectorComponent::LevelsTab : public juce::Component
{
public:
    LevelsTab()
    {
        styleLabel(gainCaption, "Gain:");
        addAndMakeVisible(gainCaption);

        gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
        gainSlider.setTextValueSuffix(" dB");
        gainSlider.setRange(-60.0, 6.0, 0.1);
        gainSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff777777));
        gainSlider.setColour(juce::Slider::thumbColourId, Palette::textPrimary);
        gainSlider.onValueChange = [this]
        {
            if (currentCue != nullptr)
            {
                currentCue->gainDb = gainSlider.getValue();
                notifyEdited();
            }
        };
        addAndMakeVisible(gainSlider);

        styleLabel(panCaption, "Pan:");
        addAndMakeVisible(panCaption);

        panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
        panSlider.setRange(-1.0, 1.0, 0.01);
        panSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff777777));
        panSlider.setColour(juce::Slider::thumbColourId, Palette::textPrimary);
        panSlider.onValueChange = [this]
        {
            if (currentCue != nullptr)
            {
                currentCue->pan = panSlider.getValue();
                notifyEdited();
            }
        };
        addAndMakeVisible(panSlider);

        styleLabel(noteLabel, "Changes apply to running instances of this cue immediately.", false);
        addAndMakeVisible(noteLabel);
    }

    void setCue(Cue* cue)
    {
        currentCue = cue;

        if (cue == nullptr)
            return;

        gainSlider.setValue(cue->gainDb, juce::dontSendNotification);
        panSlider.setValue(cue->pan, juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::panelBg);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(20);
        r.removeFromTop(6);

        auto row = r.removeFromTop(24);
        gainCaption.setBounds(row.removeFromLeft(50));
        gainSlider.setBounds(row.removeFromLeft(320));
        r.removeFromTop(14);

        auto row2 = r.removeFromTop(24);
        panCaption.setBounds(row2.removeFromLeft(50));
        panSlider.setBounds(row2.removeFromLeft(320));
        r.removeFromTop(20);

        noteLabel.setBounds(r.removeFromTop(22));
    }

    std::function<void()> onEdited;

private:
    void notifyEdited()
    {
        if (onEdited != nullptr)
            onEdited();
    }

    juce::Label gainCaption, panCaption, noteLabel;
    juce::Slider gainSlider, panSlider;
    Cue* currentCue = nullptr;
};

class InspectorComponent::TrimTab : public juce::Component
{
public:
    TrimTab()
    {
        addAndMakeVisible(waveform);
        waveform.onTrimChanged = [this](double start, double end)
        {
            if (currentCue != nullptr)
            {
                currentCue->trimStart = start;
                currentCue->trimEnd = end;
                refreshEditors();
                notifyEdited();
            }
        };

        styleLabel(startCaption, "Start:");
        styleLabel(endCaption, "End:");
        addAndMakeVisible(startCaption);
        addAndMakeVisible(endCaption);

        styleEditor(startEditor);
        styleEditor(endEditor);
        addAndMakeVisible(startEditor);
        addAndMakeVisible(endEditor);
        endEditor.setTextToShowWhenEmpty("file end", Palette::textDim);

        auto apply = [this]
        {
            if (currentCue != nullptr)
            {
                currentCue->trimStart = juce::jmax(0.0, parseTimeText(startEditor.getText()));
                currentCue->trimEnd = parseTimeText(endEditor.getText());
                waveform.setTrim(currentCue->trimStart,
                                 currentCue->trimEnd > 0.0 ? currentCue->trimEnd : 1.0e9);
                notifyEdited();
            }
        };

        startEditor.onReturnKey = [this, apply] { apply(); startEditor.giveAwayKeyboardFocus(); };
        endEditor.onReturnKey = [this, apply] { apply(); endEditor.giveAwayKeyboardFocus(); };
        startEditor.onFocusLost = [apply] { apply(); };
        endEditor.onFocusLost = [apply] { apply(); };
    }

    void setCue(Cue* cue)
    {
        currentCue = cue;

        if (cue == nullptr)
            return;

        waveform.setFile(cue->file, cue->trimStart,
                         cue->trimEnd > 0.0 ? cue->trimEnd : 1.0e9);
        refreshEditors();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::panelBg);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(14);
        r.removeFromTop(4);

        auto fields = r.removeFromBottom(24);
        r.removeFromBottom(8);
        waveform.setBounds(r);

        startCaption.setBounds(fields.removeFromLeft(50));
        startEditor.setBounds(fields.removeFromLeft(120));
        fields.removeFromLeft(16);
        endCaption.setBounds(fields.removeFromLeft(40));
        endEditor.setBounds(fields.removeFromLeft(120));
    }

    std::function<void()> onEdited;

private:
    void refreshEditors()
    {
        if (currentCue == nullptr)
            return;

        startEditor.setText(formatInspectorTime(currentCue->trimStart), false);
        endEditor.setText(currentCue->trimEnd > currentCue->trimStart
                              ? formatInspectorTime(currentCue->trimEnd)
                              : juce::String(),
                          false);
    }

    void notifyEdited()
    {
        if (onEdited != nullptr)
            onEdited();
    }

    WaveformView waveform;
    juce::Label startCaption, endCaption;
    juce::TextEditor startEditor, endEditor;
    Cue* currentCue = nullptr;
};

InspectorComponent::InspectorComponent()
    : tabs(juce::TabbedButtonBar::TabsAtTop)
{
    auto wireEdited = [this](auto* tab)
    {
        tab->onEdited = [this]
        {
            if (onCueEdited != nullptr)
                onCueEdited();
        };
    };

    auto basics = std::make_unique<BasicsTab>();
    basicsTab = basics.get();
    wireEdited(basicsTab);

    auto triggers = std::make_unique<TriggersTab>();
    triggersTab = triggers.get();
    wireEdited(triggersTab);

    auto io = std::make_unique<IOTab>();
    ioTab = io.get();

    auto timeLoops = std::make_unique<TimeLoopsTab>();
    timeLoopsTab = timeLoops.get();
    wireEdited(timeLoopsTab);

    auto levels = std::make_unique<LevelsTab>();
    levelsTab = levels.get();
    wireEdited(levelsTab);

    auto trim = std::make_unique<TrimTab>();
    trimTab = trim.get();
    wireEdited(trimTab);

    tabs.setTabBarDepth(26);
    tabs.setColour(juce::TabbedComponent::backgroundColourId, Palette::panelBg);
    tabs.setColour(juce::TabbedComponent::outlineColourId, Palette::divider);

    tabs.addTab("Basics", Palette::panelBg, basics.release(), true);
    tabs.addTab("Triggers", Palette::panelBg, triggers.release(), true);
    tabs.addTab("I/O", Palette::panelBg, io.release(), true);
    tabs.addTab("Time & Loops", Palette::panelBg, timeLoops.release(), true);
    tabs.addTab("Levels", Palette::panelBg, levels.release(), true);
    tabs.addTab("Trim", Palette::panelBg, trim.release(), true);
    tabs.setCurrentTabIndex(0);
    addAndMakeVisible(tabs);

    emptyLabel.setText("No Cue Selected", juce::dontSendNotification);
    emptyLabel.setJustificationType(juce::Justification::centred);
    emptyLabel.setColour(juce::Label::textColourId, Palette::textDim);
    emptyLabel.setFont(juce::Font(juce::FontOptions().withHeight(17.0f)));
    addAndMakeVisible(emptyLabel);

    setCue(nullptr);
}

InspectorComponent::~InspectorComponent() = default;

void InspectorComponent::setCue(Cue* cue)
{
    basicsTab->setCue(cue);
    triggersTab->setCue(cue);
    timeLoopsTab->setCue(cue);
    levelsTab->setCue(cue);
    trimTab->setCue(cue);

    if (outputInfoProvider != nullptr)
        ioTab->refresh(outputInfoProvider());

    const auto hasCue = cue != nullptr;
    tabs.setVisible(hasCue);
    emptyLabel.setVisible(! hasCue);
}

void InspectorComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::panelBg);
}

void InspectorComponent::resized()
{
    tabs.setBounds(getLocalBounds());
    emptyLabel.setBounds(getLocalBounds());
}
