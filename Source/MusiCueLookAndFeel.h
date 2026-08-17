#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "Palette.h"

class MusiCueLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MusiCueLookAndFeel()
    {
        setColourScheme(getDarkColourScheme());
        setColour(juce::PopupMenu::backgroundColourId, Palette::panelBg);
        setColour(juce::PopupMenu::textColourId, Palette::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::selection);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId, Palette::standbyGreen);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int height) override
    {
        return juce::Font(juce::FontOptions().withHeight(juce::jlimit(12.0f, 15.0f,
                                                                      static_cast<float>(height) * 0.43f)));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& background, bool highlighted,
                              bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto tabButton = static_cast<bool>(button.getProperties().getWithDefault("tabButton", false));
        if (tabButton)
        {
            g.setColour((highlighted || down) ? Palette::controlBg : Palette::panelBg);
            g.fillRect(button.getLocalBounds());
            if (static_cast<bool>(button.getProperties().getWithDefault("active", false)))
            {
                g.setColour(Palette::standbyGreen);
                g.fillRect(0, button.getHeight() - 2, button.getWidth(), 2);
            }
            return;
        }

        auto fill = background;
        if (! button.isEnabled()) fill = fill.withMultipliedAlpha(0.35f);
        else if (down) fill = fill.brighter(0.16f);
        else if (highlighted) fill = fill.brighter(0.08f);

        if (! fill.isTransparent())
        {
            g.setColour(fill);
            g.fillRoundedRectangle(bounds, 7.0f);
        }

        if (button.hasKeyboardFocus(true))
        {
            g.setColour(Palette::standbyGreen.withAlpha(0.9f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.5f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        const auto on = button.getToggleState();
        g.setColour(button.findColour(on ? juce::TextButton::textColourOnId
                                        : juce::TextButton::textColourOffId)
                           .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.42f));
        g.setFont(getTextButtonFont(button, button.getHeight()));
        const auto leftAligned = static_cast<bool>(button.getProperties().getWithDefault("alignLeft", false));
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().reduced(leftAligned ? 14 : 8, 2),
                         leftAligned ? juce::Justification::centredLeft
                                     : juce::Justification::centred,
                         1);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool highlighted, bool down) override
    {
        auto track = juce::Rectangle<float>(2.0f, (static_cast<float>(button.getHeight()) - 22.0f) * 0.5f,
                                            40.0f, 22.0f);
        const auto active = button.getToggleState();
        auto trackColour = active ? Palette::standbyGreen.darker(0.25f) : Palette::controlBg;
        if (highlighted) trackColour = trackColour.brighter(0.08f);
        if (down) trackColour = trackColour.brighter(0.14f);
        if (! button.isEnabled()) trackColour = trackColour.withMultipliedAlpha(0.45f);
        g.setColour(trackColour);
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);

        const auto thumbX = active ? track.getRight() - 19.0f : track.getX() + 3.0f;
        g.setColour(active ? juce::Colours::white : Palette::textDim.brighter(0.25f));
        g.fillEllipse(thumbX, track.getY() + 3.0f, 16.0f, 16.0f);

        g.setColour(button.findColour(juce::ToggleButton::textColourId)
                           .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.45f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
        g.drawText(button.getButtonText(), 52, 0, button.getWidth() - 52,
                   button.getHeight(), juce::Justification::centredLeft, true);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool down,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width),
                                             static_cast<float>(height)).reduced(0.5f);
        g.setColour((down ? Palette::controlDown : Palette::controlBg)
                        .withMultipliedAlpha(box.isEnabled() ? 1.0f : 0.45f));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(Palette::divider.brighter(0.38f));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        const float cx = static_cast<float>(width) - 18.0f;
        const float cy = static_cast<float>(height) * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath(cx - 5.0f, cy - 2.0f);
        chevron.lineTo(cx, cy + 3.0f);
        chevron.lineTo(cx + 5.0f, cy - 2.0f);
        g.setColour(Palette::textPrimary.withMultipliedAlpha(box.isEnabled() ? 1.0f : 0.4f));
        g.strokePath(chevron, juce::PathStrokeType(1.8f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(juce::FontOptions().withHeight(14.0f));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(12, 1, box.getWidth() - 40, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                                   static_cast<float>(x),
                                                   static_cast<float>(x + width), style, slider);
            return;
        }

        const auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const auto startX = static_cast<float>(x + 7);
        const auto endX = static_cast<float>(x + width - 7);
        g.setColour(Palette::fieldBg);
        g.drawLine(startX, centreY, endX, centreY, 5.0f);
        g.setColour(Palette::standbyGreen.darker(0.1f));
        g.drawLine(startX, centreY, sliderPos, centreY, 5.0f);
        g.setColour(slider.isEnabled() ? Palette::textPrimary : Palette::textDim);
        g.fillEllipse(sliderPos - 6.5f, centreY - 6.5f, 13.0f, 13.0f);
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->setColour(juce::Label::backgroundColourId, Palette::fieldBg);
        label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour(juce::Label::textColourId, Palette::textPrimary);
        return label;
    }

    int getTabButtonOverlap(int) override { return 0; }

    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics& g,
                                      int width, int height) override
    {
        // JUCE paints this layer after inactive tab buttons. Keep it transparent
        // or it covers every tab except the active front button.
        g.setColour(Palette::divider);
        g.fillRect(0, height - 1, width, 1);
    }

    void drawTabButton(juce::TabBarButton& button, juce::Graphics& g,
                       bool highlighted, bool down) override
    {
        const auto active = button.getIndex() == button.getTabbedButtonBar().getCurrentTabIndex();
        auto bounds = button.getLocalBounds();
        if (highlighted || down)
        {
            g.setColour(Palette::controlBg.withAlpha(down ? 0.95f : 0.6f));
            g.fillRect(bounds);
        }
        if (active)
        {
            g.setColour(Palette::standbyGreen);
            g.fillRect(bounds.removeFromBottom(2));
        }
        g.setColour(active ? Palette::textPrimary : Palette::textDim);
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.5f)));
        g.drawText(button.getButtonText(), button.getLocalBounds().reduced(12, 0),
                   juce::Justification::centred, true);
    }

    int getTabButtonBestWidth(juce::TabBarButton& button, int) override
    {
        return juce::jmax(88, static_cast<int>(button.getButtonText().length()) * 8 + 30);
    }

    void drawTableHeaderBackground(juce::Graphics& g,
                                   juce::TableHeaderComponent& header) override
    {
        g.fillAll(Palette::panelBg);
        g.setColour(Palette::divider);
        g.fillRect(0, header.getHeight() - 1, header.getWidth(), 1);
    }

    void drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent&,
                               const juce::String& name, int, int width, int height,
                               bool highlighted, bool down, int) override
    {
        if (highlighted || down)
        {
            g.setColour(Palette::controlBg);
            g.fillRect(0, 0, width, height);
        }
        g.setColour(Palette::textDim);
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        g.drawText(name, 9, 0, width - 14, height,
                   name == "#" ? juce::Justification::centred
                               : juce::Justification::centredLeft,
                   true);
        g.setColour(Palette::divider);
        g.fillRect(width - 1, 6, 1, height - 12);
    }
};
