#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"

class GoButtonComponent : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;

    void setArmed(bool shouldBeArmed)
    {
        if (armed != shouldBeArmed)
        {
            armed = shouldBeArmed;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(1);

        auto fill = armed ? Palette::standbyGreen.darker(0.18f) : Palette::controlBg;
        if (isMouseOver())
            fill = fill.brighter(0.08f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

        g.setColour(armed ? juce::Colours::white : Palette::textDim);
        g.setFont(juce::Font(juce::FontOptions().withHeight(juce::jmin(64.0f, (float) bounds.getHeight() * 0.55f))));
        g.drawText("GO", bounds, juce::Justification::centred);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains(e.getPosition()) && onClick != nullptr)
            onClick();
    }

private:
    bool armed = false;
};
