#include "Shortcuts.h"

namespace
{
    bool sameDescription(const juce::String& a, const juce::String& b)
    {
        return a.trim().equalsIgnoreCase(b.trim());
    }

    juce::KeyPress commandKey(int keyCode, juce::juce_wchar character)
    {
        return { keyCode, juce::ModifierKeys::commandModifier, character };
    }

    juce::StringArray descriptionsFromKeys(const juce::Array<juce::KeyPress>& keys)
    {
        juce::StringArray descriptions;

        for (const auto& key : keys)
            descriptions.add(key.getTextDescription());

        return descriptions;
    }
}

const juce::Array<ShortcutInfo>& ShortcutBindings::catalog()
{
    static const juce::Array<ShortcutInfo> items = []
    {
        const ShortcutInfo data[] = {
            { ShortcutId::go,              "go",              "Playback",   "GO standby cue" },
            { ShortcutId::panic,           "panic",           "Playback",   "Panic (stop all)" },
            { ShortcutId::preview,         "preview",         "Playback",   "Preview selected cue" },
            { ShortcutId::stopSelected,    "stopSelected",    "Playback",   "Stop selected cue" },
            { ShortcutId::pause,           "pause",           "Playback",   "Pause / resume" },
            { ShortcutId::resetStandby,    "resetStandby",    "Playback",   "Reset standby to top" },
            { ShortcutId::undo,            "undo",            "Edit",       "Undo" },
            { ShortcutId::deleteSelected,  "deleteSelected",  "Edit",       "Delete selected cue" },
            { ShortcutId::duplicate,       "duplicate",       "Edit",       "Duplicate selected cue" },
            { ShortcutId::copy,            "copy",            "Edit",       "Copy selected cue" },
            { ShortcutId::paste,           "paste",           "Edit",       "Paste cues" },
            { ShortcutId::indent,          "indent",          "Edit",       "Indent cue" },
            { ShortcutId::outdent,         "outdent",         "Edit",       "Outdent cue" },
            { ShortcutId::moveCueUp,       "moveCueUp",       "Edit",       "Move cue up" },
            { ShortcutId::moveCueDown,     "moveCueDown",     "Edit",       "Move cue down" },
            { ShortcutId::groupSelected,   "groupSelected",   "Edit",       "Group selected cues" },
            { ShortcutId::ungroupSelected, "ungroupSelected", "Edit",       "Ungroup" },
            { ShortcutId::selectPrevious,  "selectPrevious",  "Navigation", "Select previous cue" },
            { ShortcutId::selectNext,      "selectNext",      "Navigation", "Select next cue" },
            { ShortcutId::collapseGroup,   "collapseGroup",   "Navigation", "Collapse group" },
            { ShortcutId::expandGroup,     "expandGroup",     "Navigation", "Expand group" },
            { ShortcutId::open,            "open",            "Workspace",  "Open workspace" },
            { ShortcutId::save,            "save",            "Workspace",  "Save workspace" },
            { ShortcutId::saveAs,          "saveAs",          "Workspace",  "Save workspace as" },
            { ShortcutId::toggleInspector, "toggleInspector", "Workspace",  "Show / hide inspector" },
            { ShortcutId::toggleShowMode,  "toggleShowMode",  "Workspace",  "Toggle show / edit mode" },
            { ShortcutId::openSettings,    "openSettings",    "Workspace",  "Open settings" },
            { ShortcutId::addAudioCue,     "addAudioCue",     "Workspace",  "Add audio cue" },
            { ShortcutId::addGroupCue,     "addGroupCue",     "Workspace",  "Add group cue" },
            { ShortcutId::addFadeCue,      "addFadeCue",      "Workspace",  "Add fade cue" }
        };

        juce::Array<ShortcutInfo> list;
        list.addArray(data, static_cast<int>(sizeof(data) / sizeof(data[0])));
        return list;
    }();

    return items;
}

bool ShortcutBindings::allowedWhileTyping(ShortcutId id)
{
    return id == ShortcutId::panic
        || id == ShortcutId::save
        || id == ShortcutId::saveAs
        || id == ShortcutId::open
        || id == ShortcutId::openSettings;
}

juce::StringArray ShortcutBindings::defaultDescriptions(ShortcutId id)
{
    const auto command = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;

    switch (id)
    {
        case ShortcutId::go:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::spaceKey) });
        case ShortcutId::panic:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::escapeKey) });
        case ShortcutId::preview:
            return descriptionsFromKeys({ commandKey('p', 'p') });
        case ShortcutId::stopSelected:
            return descriptionsFromKeys({ commandKey('.', '.') });
        case ShortcutId::pause:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::spaceKey, shift, 0) });
        case ShortcutId::resetStandby:
            return {};
        case ShortcutId::undo:
            return descriptionsFromKeys({ commandKey('z', 'z') });
        case ShortcutId::deleteSelected:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::backspaceKey),
                                          juce::KeyPress(juce::KeyPress::deleteKey) });
        case ShortcutId::duplicate:
            return descriptionsFromKeys({ commandKey('d', 'd') });
        case ShortcutId::copy:
            return descriptionsFromKeys({ commandKey('c', 'c') });
        case ShortcutId::paste:
            return descriptionsFromKeys({ commandKey('v', 'v') });
        case ShortcutId::indent:
            return descriptionsFromKeys({ commandKey(']', ']') });
        case ShortcutId::outdent:
            return descriptionsFromKeys({ commandKey('[', '[') });
        case ShortcutId::moveCueUp:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::upKey, command, 0) });
        case ShortcutId::moveCueDown:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::downKey, command, 0) });
        case ShortcutId::groupSelected:
            return descriptionsFromKeys({ commandKey('g', 'g') });
        case ShortcutId::ungroupSelected:
            return descriptionsFromKeys({ juce::KeyPress('g', command | shift, 'g') });
        case ShortcutId::selectPrevious:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::upKey) });
        case ShortcutId::selectNext:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::downKey) });
        case ShortcutId::collapseGroup:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::leftKey) });
        case ShortcutId::expandGroup:
            return descriptionsFromKeys({ juce::KeyPress(juce::KeyPress::rightKey) });
        case ShortcutId::open:
            return descriptionsFromKeys({ commandKey('o', 'o') });
        case ShortcutId::save:
            return descriptionsFromKeys({ commandKey('s', 's') });
        case ShortcutId::saveAs:
            return descriptionsFromKeys({ juce::KeyPress('s', command | shift, 's') });
        case ShortcutId::toggleInspector:
            return descriptionsFromKeys({ commandKey('i', 'i') });
        case ShortcutId::toggleShowMode:
            return {};
        case ShortcutId::openSettings:
            return descriptionsFromKeys({ commandKey(',', ',') });
        case ShortcutId::addAudioCue:
        case ShortcutId::addGroupCue:
        case ShortcutId::addFadeCue:
            return {};
    }

    return {};
}

juce::String ShortcutBindings::propertyKey(const ShortcutInfo& info)
{
    return juce::String("shortcut.") + info.storageKey;
}

void ShortcutBindings::resetToDefaults()
{
    bindings.clear();

    for (const auto& info : catalog())
        bindings[info.id] = defaultDescriptions(info.id);
}

void ShortcutBindings::load(const juce::PropertiesFile& props)
{
    resetToDefaults();

    for (const auto& info : catalog())
    {
        const auto key = propertyKey(info);

        if (! props.containsKey(key))
            continue;

        const auto value = props.getValue(key);
        juce::StringArray descriptions;

        if (value.isNotEmpty())
            descriptions = juce::StringArray::fromTokens(value, "|", "");

        descriptions.trim();
        descriptions.removeEmptyStrings();
        bindings[info.id] = std::move(descriptions);
    }
}

void ShortcutBindings::save(juce::PropertiesFile& props) const
{
    for (const auto& info : catalog())
        props.setValue(propertyKey(info), getDescriptions(info.id).joinIntoString("|"));
}

juce::StringArray ShortcutBindings::getDescriptions(ShortcutId id) const
{
    const auto it = bindings.find(id);
    return it != bindings.end() ? it->second : defaultDescriptions(id);
}

juce::String ShortcutBindings::describe(const juce::String& description)
{
    auto desc = description.replace(" + ", "+");
    juce::StringArray parts;
    parts.addTokens(desc, "+", "");

    for (auto& part : parts)
    {
        part = part.trim();

        if (part.isNotEmpty())
            part = part.substring(0, 1).toUpperCase() + part.substring(1);
    }

    return parts.joinIntoString("+");
}

juce::String ShortcutBindings::describe(const juce::KeyPress& key)
{
    return describe(key.getTextDescription());
}

juce::String ShortcutBindings::getDisplayString(ShortcutId id) const
{
    juce::StringArray parts;

    for (const auto& description : getDescriptions(id))
        parts.add(describe(description));

    return parts.joinIntoString(" or ");
}

juce::String ShortcutBindings::tooltipSuffix(ShortcutId id) const
{
    const auto text = getDisplayString(id);
    return text.isEmpty() ? juce::String() : " (" + text + ")";
}

void ShortcutBindings::assign(ShortcutId id, juce::KeyPress key)
{
    const auto desc = key.getTextDescription();

    for (auto& entry : bindings)
    {
        if (entry.first == id)
            continue;

        for (int i = entry.second.size(); --i >= 0;)
            if (sameDescription(entry.second[i], desc))
                entry.second.remove(i);
    }

    bindings[id] = juce::StringArray { desc };
}

void ShortcutBindings::clear(ShortcutId id)
{
    bindings[id] = {};
}

const ShortcutInfo* ShortcutBindings::find(const juce::KeyPress& key) const
{
    const auto desc = key.getTextDescription();

    for (const auto& info : catalog())
        for (const auto& bound : getDescriptions(info.id))
            if (sameDescription(bound, desc))
                return &info;

    return nullptr;
}

const ShortcutInfo* ShortcutBindings::findConflict(const juce::KeyPress& key, ShortcutId except) const
{
    const auto desc = key.getTextDescription();

    for (const auto& info : catalog())
    {
        if (info.id == except)
            continue;

        for (const auto& bound : getDescriptions(info.id))
            if (sameDescription(bound, desc))
                return &info;
    }

    return nullptr;
}
