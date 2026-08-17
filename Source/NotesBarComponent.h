#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"

class NotesBarComponent : public juce::Component
{
public:
    NotesBarComponent()
    {
        editor.setMultiLine(false);
        editor.setReturnKeyStartsNewLine(false);
        editor.setScrollbarsShown(false);
        editor.setOpaque(false);
        editor.setTextToShowWhenEmpty("Notes", Palette::textDim);
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        editor.setColour(juce::TextEditor::textColourId, Palette::textPrimary);
        editor.setFont(juce::Font(juce::FontOptions().withHeight(15.0f)));
        editor.setIndents(10, 0);
        addAndMakeVisible(editor);
    }

    void resized() override
    {
        editor.setBounds(getLocalBounds().reduced(2, 0));
    }

    void setText(const juce::String& text) { editor.setText(text, false); }
    juce::String getText() const { return editor.getText(); }

    void paint(juce::Graphics& g) override
    {
        g.setColour(Palette::fieldBg);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    }

private:
    juce::TextEditor editor;
};
