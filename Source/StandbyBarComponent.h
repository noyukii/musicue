#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"

class StandbyBarComponent : public juce::Component
{
public:
    void setText(const juce::String& newText)
    {
        if (currentText != newText)
        {
            currentText = newText;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(Palette::barBg);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

        g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));

        if (currentText.isEmpty())
        {
            g.setColour(Palette::textDim);
            g.drawText("[no cue on standby]", 14, 0, getWidth() - 28, getHeight(),
                       juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(Palette::textPrimary);
            g.drawText(currentText, 14, 0, getWidth() - 28, getHeight(),
                       juce::Justification::centredLeft);
        }
    }

private:
    juce::String currentText;
};
