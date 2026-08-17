#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include "Palette.h"

namespace Icons
{
    inline std::unique_ptr<juce::Drawable> load(const void* data, size_t size, juce::Colour tint)
    {
        auto drawable = juce::Drawable::createFromImageData(data, static_cast<int>(size));

        if (drawable != nullptr)
            drawable->replaceColour(juce::Colours::black, tint);

        return drawable;
    }

    struct IconButton
    {
        std::unique_ptr<juce::DrawableButton> button;
        std::unique_ptr<juce::Drawable> normal, over;
    };

    inline IconButton makeButton(const juce::String& name, const void* data, size_t size,
                                 juce::Colour tint, const juce::String& tooltip)
    {
        IconButton result;
        result.normal = load(data, size, tint);
        result.over = load(data, size, tint.brighter(0.3f));

        result.button = std::make_unique<juce::DrawableButton>(name, juce::DrawableButton::ImageOnButtonBackground);
        result.button->setImages(result.normal.get(), result.over.get(), result.over.get());
        result.button->setEdgeIndent(6);
        result.button->setWantsKeyboardFocus(false);
        result.button->setMouseClickGrabsKeyboardFocus(false);
        result.button->setColour(juce::DrawableButton::backgroundColourId, Palette::controlBg);
        result.button->setColour(juce::DrawableButton::backgroundOnColourId, Palette::controlDown);
        result.button->setTooltip(tooltip);

        return result;
    }
}
