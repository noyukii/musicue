#include "InspectorComponent.h"
#include "FadeEditorComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <map>
#include <memory>

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
        editor.setSelectAllWhenFocused(false);
    }

    class PlaceholderTab : public juce::Component
    {
    public:
        explicit PlaceholderTab(const juce::String& text) : message(text) {}

        void paint(juce::Graphics& g) override
        {
            g.fillAll(Palette::inspectorBg);
            g.setColour(Palette::textDim);
            g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
            g.drawText(message, getLocalBounds(), juce::Justification::centred);
        }

    private:
        juce::String message;
    };

    struct WaveformPeaks
    {
        std::vector<float> peaks;
        double lengthSeconds = 0.0;
    };

    WaveformPeaks computeWaveformPeaks(const juce::File& file, const std::shared_ptr<std::atomic<bool>>& cancelled)
    {
        WaveformPeaks result;
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file));
        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
            return result;

        result.lengthSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;

        const int columns = 512;
        result.peaks.assign(static_cast<size_t>(columns) * 2, 0.0f);

        const auto total = reader->lengthInSamples;
        constexpr int maxWindow = 1024;
        juce::AudioBuffer<float> chunk(1, maxWindow);

        auto scanWindow = [&](juce::int64 pos, int numSamples, float& mn, float& mx)
        {
            if (pos >= total || numSamples <= 0)
                return;

            const int toRead = static_cast<int>(juce::jmin<juce::int64>(numSamples, total - pos));
            reader->read(&chunk, 0, toRead, pos, true, false);
            const float* samples = chunk.getReadPointer(0);

            for (int i = 0; i < toRead; ++i)
            {
                mn = juce::jmin(mn, samples[i]);
                mx = juce::jmax(mx, samples[i]);
            }
        };

        for (int col = 0; col < columns; ++col)
        {
            if ((col & 7) == 0 && cancelled != nullptr && cancelled->load())
                return {};

            const auto colStart = static_cast<juce::int64>(col) * total / columns;
            const auto colEnd = static_cast<juce::int64>(col + 1) * total / columns;
            const auto colLen = juce::jmax<juce::int64>(1, colEnd - colStart);
            const int window = static_cast<int>(juce::jmin<juce::int64>(maxWindow, colLen));

            float mn = 0.0f;
            float mx = 0.0f;
            scanWindow(colStart, window, mn, mx);

            if (colLen > maxWindow * 2)
                scanWindow(colStart + colLen / 2, window, mn, mx);

            result.peaks[static_cast<size_t>(col) * 2] = mn;
            result.peaks[static_cast<size_t>(col) * 2 + 1] = mx;
        }

        return result;
    }

    class WaveformView : public juce::Component
    {
    public:
        std::function<void(double, double, bool)> onTrimChanged;

        WaveformView() = default;

        ~WaveformView() override
        {
            cancelLoad();
        }

        void setFile(const juce::File& file, double newTrimStart, double newTrimEnd)
        {
            pendingTrimStart = newTrimStart;
            pendingTrimEnd = newTrimEnd;

            if (file == requestedFile && hasPeaks)
            {
                applyPendingTrim();
                repaint();
                return;
            }

            requestedFile = file;
            cancelLoad();
            clearPeaks();
            trimStart = pendingTrimStart;
            trimEnd = pendingTrimEnd;

            if (requestedFile == juce::File())
            {
                repaint();
                return;
            }

            if (const auto* entry = findCached(requestedFile))
            {
                adoptPeaks(entry->peaks, entry->lengthSeconds);
                applyPendingTrim();
                repaint();
                return;
            }

            if (isShowing())
                startBackgroundLoad();
            else
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
                g.drawText(requestedFile != juce::File() ? "Loading waveform..." : "No audio loaded",
                           waveArea, juce::Justification::centred);
                return;
            }

            if (wavePath.isEmpty())
                rebuildWavePath(waveArea);

            g.setColour(Palette::standbyGreen.withAlpha(0.75f));
            g.fillPath(wavePath);

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

        void resized() override
        {
            wavePath.clear();
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

            trimDirty = true;
            repaint();

            if (onTrimChanged != nullptr)
                onTrimChanged(trimStart, trimEnd, false);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (trimDirty && onTrimChanged != nullptr)
                onTrimChanged(trimStart, trimEnd, true);

            trimDirty = false;
            dragging = 0;
        }

        void visibilityChanged() override
        {
            if (isShowing() && requestedFile != juce::File() && ! hasPeaks && loadCancelled == nullptr)
                startBackgroundLoad();
        }

    private:
        struct Cache
        {
            juce::int64 modTime = 0;
            std::vector<float> peaks;
            double lengthSeconds = 0.0;
        };

        void cancelLoad()
        {
            if (loadCancelled != nullptr)
                loadCancelled->store(true);
            loadCancelled.reset();
        }

        void clearPeaks()
        {
            peaks.clear();
            wavePath.clear();
            hasPeaks = false;
            lengthSeconds = 0.0;
        }

        void adoptPeaks(const std::vector<float>& newPeaks, double newLength)
        {
            peaks = newPeaks;
            lengthSeconds = newLength;
            hasPeaks = ! peaks.empty();
            wavePath.clear();
        }

        void applyPendingTrim()
        {
            trimStart = pendingTrimStart;
            trimEnd = (pendingTrimEnd <= trimStart || pendingTrimEnd > lengthSeconds)
                          ? lengthSeconds
                          : pendingTrimEnd;
        }

        const Cache* findCached(const juce::File& file) const
        {
            const auto it = peakCache.find(file.getFullPathName());
            if (it == peakCache.end())
                return nullptr;
            if (it->second.modTime != file.getLastModificationTime().toMilliseconds())
                return nullptr;
            return &it->second;
        }

        void rebuildWavePath(juce::Rectangle<float> area)
        {
            wavePath.clear();

            const int columns = static_cast<int>(peaks.size() / 2);
            const int width = static_cast<int>(area.getWidth());
            if (columns <= 0 || width <= 0)
                return;

            const auto midY = area.getCentreY();
            const auto halfHeight = area.getHeight() * 0.5f;
            const auto x0 = area.getX();

            wavePath.startNewSubPath(x0, midY - peaks[1] * halfHeight);

            for (int x = 0; x < width; ++x)
            {
                const auto column = juce::jmin(columns - 1, x * columns / width);
                wavePath.lineTo(x0 + static_cast<float>(x), midY - peaks[column * 2 + 1] * halfHeight);
            }

            for (int x = width - 1; x >= 0; --x)
            {
                const auto column = juce::jmin(columns - 1, x * columns / width);
                const auto top = midY - peaks[column * 2 + 1] * halfHeight;
                const auto bottom = midY - peaks[column * 2] * halfHeight;
                wavePath.lineTo(x0 + static_cast<float>(x), juce::jmax(top + 1.0f, bottom));
            }

            wavePath.closeSubPath();
        }

        void startBackgroundLoad()
        {
            cancelLoad();
            loadCancelled = std::make_shared<std::atomic<bool>>(false);
            auto cancelled = loadCancelled;
            const auto file = requestedFile;
            const auto modTime = file.getLastModificationTime().toMilliseconds();
            auto safeThis = juce::Component::SafePointer<WaveformView>(this);

            juce::Thread::launch([safeThis, file, cancelled, modTime]
            {
                auto computed = computeWaveformPeaks(file, cancelled);
                if (cancelled->load())
                    return;

                juce::MessageManager::callAsync([safeThis, file, cancelled, modTime, computed = std::move(computed)]() mutable
                {
                    if (safeThis == nullptr || cancelled->load() || file != safeThis->requestedFile)
                        return;

                    safeThis->adoptPeaks(computed.peaks, computed.lengthSeconds);
                    safeThis->peakCache[file.getFullPathName()] = { modTime, safeThis->peaks,
                                                                    safeThis->lengthSeconds };
                    safeThis->applyPendingTrim();
                    safeThis->loadCancelled.reset();
                    safeThis->repaint();
                });
            });
        }

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
        juce::Path wavePath;
        std::map<juce::String, Cache> peakCache;
        juce::File requestedFile;
        std::shared_ptr<std::atomic<bool>> loadCancelled;
        double lengthSeconds = 0.0;
        double trimStart = 0.0;
        double trimEnd = 0.0;
        double pendingTrimStart = 0.0;
        double pendingTrimEnd = 0.0;
        int dragging = 0;
        bool hasPeaks = false;
        bool trimDirty = false;
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
        targetEditor.setText(cue->isGroup() ? "(not applicable)" : (cue->isFade() ? "(fade targets)" : cue->file.getFullPathName()), false);
        notesEditor.setText(cue->notes, false);
        continueBox.setSelectedId(cue->continueMode + 1, juce::dontSendNotification);
        flaggedToggle.setToggleState(cue->flagged, juce::dontSendNotification);
        autoLoadToggle.setToggleState(cue->autoLoad, juce::dontSendNotification);
        armedToggle.setToggleState(cue->armed, juce::dontSendNotification);
        skipToggle.setToggleState(cue->skipIfDisarmed, juce::dontSendNotification);

        for (auto* component : { static_cast<juce::Component*>(&fadeInLabel),
                                 static_cast<juce::Component*>(&fadeInEditor),
                                 static_cast<juce::Component*>(&fadeOutLabel),
                                 static_cast<juce::Component*>(&fadeOutEditor) })
            component->setVisible(cue->isAudio());
        resized();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::inspectorBg);
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
        if (currentCue == nullptr || currentCue->isAudio())
        {
            fieldRow(col1, fadeInLabel, 72, fadeInEditor);
            fieldRow(col1, fadeOutLabel, 72, fadeOutEditor);
        }
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
        if (currentCue->isAudio())
        {
            currentCue->fadeIn = parseTimeText(fadeInEditor.getText());
            currentCue->fadeOut = parseTimeText(fadeOutEditor.getText());
        }
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
        g.fillAll(Palette::inspectorBg);
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
        g.fillAll(Palette::inspectorBg);
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
        g.fillAll(Palette::inspectorBg);
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
        g.fillAll(Palette::inspectorBg);
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

class InspectorComponent::ModeTab : public juce::Component
{
public:
    ModeTab()
    {
        styleLabel(introLabel,
                   "Group mode controls what starts and where standby moves when this cue is triggered.",
                   false);
        addAndMakeVisible(introLabel);

        const juce::String names[] = { "Timeline", "Playlist", "Start first and enter",
                                       "Start first", "Start random" };
        for (int i = 0; i < 5; ++i)
        {
            options[i].setButtonText(names[i]);
            options[i].setRadioGroupId(7271);
            options[i].setClickingTogglesState(true);
            options[i].setColour(juce::ToggleButton::textColourId, Palette::textPrimary);
            options[i].onClick = [this, i]
            {
                if (currentCue == nullptr)
                    return;
                currentCue->groupMode = static_cast<Cue::GroupMode>(i);
                refreshDescription();
                if (onEdited != nullptr)
                    onEdited();
            };
            addAndMakeVisible(options[i]);
        }

        titleLabel.setColour(juce::Label::textColourId, Palette::textPrimary);
        titleLabel.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
        titleLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(titleLabel);

        descriptionLabel.setColour(juce::Label::textColourId, Palette::textDim);
        descriptionLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.5f)));
        descriptionLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(descriptionLabel);
    }

    void setCue(Cue* cue)
    {
        currentCue = cue;
        if (cue == nullptr)
            return;
        const auto index = juce::jlimit(0, 4, static_cast<int>(cue->groupMode));
        options[index].setToggleState(true, juce::dontSendNotification);
        refreshDescription();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::inspectorBg);
        g.setColour(Palette::divider);
        g.fillRect(260, 62, 1, juce::jmax(0, getHeight() - 82));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(20);
        introLabel.setBounds(r.removeFromTop(28));
        r.removeFromTop(10);

        auto choices = r.removeFromLeft(220);
        r.removeFromLeft(42);
        for (auto& option : options)
        {
            option.setBounds(choices.removeFromTop(30));
            choices.removeFromTop(3);
        }

        titleLabel.setBounds(r.removeFromTop(28));
        descriptionLabel.setBounds(r.removeFromTop(110));
    }

    std::function<void()> onEdited;

private:
    void refreshDescription()
    {
        if (currentCue == nullptr)
            return;

        const juce::String titles[] = { "Timeline", "Playlist", "Start first and enter",
                                        "Start first", "Start random" };
        const juce::String descriptions[] = {
            "Starts all children simultaneously. Standby advances to the first cue after the group. Child pre-waits count from the group start.",
            "Starts the first child, then plays children sequentially as each one finishes. Standby advances to the first cue after the group.",
            "Starts the first child and moves standby to the next child. After the final child, standby leaves the group.",
            "Starts only the first child and advances standby to the first cue after the group. Child cue sequences can continue independently.",
            "Starts an armed, idle child using round-robin random selection, then advances standby past the group."
        };
        const auto index = juce::jlimit(0, 4, static_cast<int>(currentCue->groupMode));
        titleLabel.setText(titles[index], juce::dontSendNotification);
        descriptionLabel.setText(descriptions[index], juce::dontSendNotification);
    }

    juce::Label introLabel, titleLabel, descriptionLabel;
    juce::ToggleButton options[5];
    Cue* currentCue = nullptr;
};

class InspectorComponent::TrimTab : public juce::Component
{
public:
    TrimTab()
    {
        addAndMakeVisible(waveform);
        waveform.onTrimChanged = [this](double start, double end, bool finished)
        {
            if (currentCue != nullptr)
            {
                currentCue->trimStart = start;
                currentCue->trimEnd = end;
                refreshEditors();
                if (finished)
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
        {
            waveform.setFile({}, 0.0, 0.0);
            return;
        }

        waveform.setFile(cue->file, cue->trimStart,
                         cue->trimEnd > 0.0 ? cue->trimEnd : 1.0e9);
        refreshEditors();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::inspectorBg);
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

class InspectorComponent::FadeTab : public juce::Component
{
public:
    FadeTab()
    {
        editor.onChange = [this] { commit(); };
        editor.onIncomingSelected = [this](int cueId)
        {
            if (onPreviewCue != nullptr)
                onPreviewCue(cueId);
        };
        editor.playheadProvider = [this]() -> double
        {
            if (currentCue == nullptr || fadeProgressProvider == nullptr)
                return -1.0;
            return fadeProgressProvider(currentCue->id);
        };
        addAndMakeVisible(editor);
    }

    void setAvailableCues(const juce::Array<Cue>& cues)
    {
        availableCues = cues;
        editor.setAvailableCues(cues);
    }

    void setCue(Cue* cue)
    {
        currentCue = cue;
        updating = true;
        if (currentCue != nullptr && currentCue->isFade())
            editor.setSetup(Cue::FadeSetup::fromActions(currentCue->fadeActions, currentCue->fadeStopPolicy));
        else
            editor.setSetup({});
        updating = false;
    }

    std::function<void()> onEdited;
    std::function<void(int cueId)> onPreviewCue;
    std::function<double(int cueId)> fadeProgressProvider;

    void paint(juce::Graphics& g) override { g.fillAll(Palette::inspectorBg); }

    void resized() override { editor.setBounds(getLocalBounds()); }

private:
    bool isAutoName(const juce::String& name) const
    {
        return name.startsWith("(Untitled Fade Cue") || name.startsWith("Crossfade to ")
            || name.startsWith("Fade out ");
    }

    void commit()
    {
        if (updating || currentCue == nullptr || ! currentCue->isFade())
            return;

        const auto setup = editor.getSetup();
        currentCue->fadeStopPolicy = setup.stopPolicy;
        currentCue->fadeActions = setup.toActions(setup.fromCueId, setup.toCueId);
        if (currentCue->name.isEmpty() || isAutoName(currentCue->name))
            currentCue->name = Cue::makeFadeName(availableCues, setup.fromCueId, setup.toCueId);

        if (onEdited != nullptr)
            onEdited();
    }

    FadeEditorComponent editor { FadeEditorComponent::Mode::inspector };
    juce::Array<Cue> availableCues;
    Cue* currentCue = nullptr;
    bool updating = false;
};

class InspectorComponent::InspectorViewport : public juce::Component
{
public:
    InspectorViewport(juce::Component& contentToScroll, int minimumContentHeight)
        : content(contentToScroll), minHeight(minimumContentHeight)
    {
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
        viewport.setScrollBarThickness(8);
        viewport.setColour(juce::ScrollBar::thumbColourId, Palette::controlDown);
        addAndMakeVisible(viewport);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::inspectorBg);
    }

    void resized() override
    {
        viewport.setBounds(getLocalBounds());

        const auto height = juce::jmax(minHeight, viewport.getMaximumVisibleHeight());
        content.setSize(viewport.getMaximumVisibleWidth(), height);

        const auto widthAfterScrollBars = viewport.getMaximumVisibleWidth();
        if (widthAfterScrollBars != content.getWidth())
            content.setSize(widthAfterScrollBars, height);
    }

private:
    juce::Viewport viewport;
    juce::Component& content;
    int minHeight = 0;
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

    basicsTab = std::make_unique<BasicsTab>();
    wireEdited(basicsTab.get());

    triggersTab = std::make_unique<TriggersTab>();
    wireEdited(triggersTab.get());

    ioTab = std::make_unique<IOTab>();

    timeLoopsTab = std::make_unique<TimeLoopsTab>();
    wireEdited(timeLoopsTab.get());

    levelsTab = std::make_unique<LevelsTab>();
    wireEdited(levelsTab.get());

    trimTab = std::make_unique<TrimTab>();
    wireEdited(trimTab.get());

    modeTab = std::make_unique<ModeTab>();
    wireEdited(modeTab.get());

    fadeTab = std::make_unique<FadeTab>();
    wireEdited(fadeTab.get());
    fadeTab->onPreviewCue = [this](int cueId)
    {
        if (onPreviewCue != nullptr)
            onPreviewCue(cueId);
    };
    fadeTab->fadeProgressProvider = [this](int cueId)
    {
        return fadeProgressProvider != nullptr ? fadeProgressProvider(cueId) : -1.0;
    };

    basicsHost = std::make_unique<InspectorViewport>(*basicsTab, 236);
    triggersHost = std::make_unique<InspectorViewport>(*triggersTab, 148);
    ioHost = std::make_unique<InspectorViewport>(*ioTab, 142);
    timeLoopsHost = std::make_unique<InspectorViewport>(*timeLoopsTab, 180);
    levelsHost = std::make_unique<InspectorViewport>(*levelsTab, 150);
    trimHost = std::make_unique<InspectorViewport>(*trimTab, 200);
    modeHost = std::make_unique<InspectorViewport>(*modeTab, 378);
    fadeHost = std::make_unique<InspectorViewport>(*fadeTab, 448);

    tabs.setTabBarDepth(26);
    tabs.setColour(juce::TabbedComponent::backgroundColourId, Palette::inspectorBg);
    tabs.setColour(juce::TabbedComponent::outlineColourId, Palette::inspectorBorder);

    rebuildTabs(false, false);
    addAndMakeVisible(tabs);

    emptyLabel.setText("No Cue Selected", juce::dontSendNotification);
    emptyLabel.setJustificationType(juce::Justification::centred);
    emptyLabel.setColour(juce::Label::textColourId, Palette::textDim);
    emptyLabel.setFont(juce::Font(juce::FontOptions().withHeight(17.0f)));
    addAndMakeVisible(emptyLabel);

    setCue(nullptr);
}

InspectorComponent::~InspectorComponent() = default;

void InspectorComponent::rebuildTabs(bool groupSelected, bool fadeSelected)
{
    tabs.clearTabs();
    tabs.addTab("Basics", Palette::inspectorBg, basicsHost.get(), false);
    tabs.addTab("Triggers", Palette::inspectorBg, triggersHost.get(), false);
    if (groupSelected)
    {
        tabs.addTab("Mode", Palette::inspectorBg, modeHost.get(), false);
    }
    else if (fadeSelected)
    {
        tabs.addTab("Fade", Palette::inspectorBg, fadeHost.get(), false);
    }
    else
    {
        tabs.addTab("I/O", Palette::inspectorBg, ioHost.get(), false);
        tabs.addTab("Time & Loops", Palette::inspectorBg, timeLoopsHost.get(), false);
        tabs.addTab("Levels", Palette::inspectorBg, levelsHost.get(), false);
        tabs.addTab("Trim", Palette::inspectorBg, trimHost.get(), false);
    }
    if (tabs.getCurrentTabIndex() != 0)
        tabs.setCurrentTabIndex(0);
    showingGroupTabs = groupSelected;
    showingFadeTabs = fadeSelected;
}

void InspectorComponent::setCue(Cue* cue)
{
    const auto groupSelected = cue != nullptr && cue->isGroup();
    const auto fadeSelected = cue != nullptr && cue->isFade();
    if (groupSelected != showingGroupTabs || fadeSelected != showingFadeTabs)
        rebuildTabs(groupSelected, fadeSelected);

    basicsTab->setCue(cue);
    triggersTab->setCue(cue);
    if (groupSelected)
    {
        modeTab->setCue(cue);
    }
    else if (fadeSelected)
    {
        fadeTab->setCue(cue);
    }
    else
    {
        timeLoopsTab->setCue(cue);
        levelsTab->setCue(cue);
        trimTab->setCue(cue);
    }

    if (outputInfoProvider != nullptr)
        ioTab->refresh(outputInfoProvider());

    const auto hasCue = cue != nullptr;
    tabs.setVisible(hasCue);
    emptyLabel.setVisible(! hasCue);

    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (isParentOf(focused))
            focused->giveAwayKeyboardFocus();
}

void InspectorComponent::setAvailableCues(const juce::Array<Cue>& cues)
{
    fadeTab->setAvailableCues(cues);
}

void InspectorComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::inspectorBg);
}

void InspectorComponent::resized()
{
    tabs.setBounds(getLocalBounds());
    emptyLabel.setBounds(getLocalBounds());
}
