#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

enum class ShortcutId
{
    go,
    panic,
    preview,
    stopSelected,
    pause,
    resetStandby,
    undo,
    deleteSelected,
    duplicate,
    copy,
    paste,
    indent,
    outdent,
    moveCueUp,
    moveCueDown,
    groupSelected,
    ungroupSelected,
    selectPrevious,
    selectNext,
    selectAll,
    collapseGroup,
    expandGroup,
    open,
    save,
    saveAs,
    toggleInspector,
    toggleShowMode,
    openSettings,
    addAudioCue,
    addGroupCue,
    addFadeCue
};

struct ShortcutInfo
{
    ShortcutId id = ShortcutId::go;
    const char* storageKey = "";
    const char* category = "";
    const char* name = "";
};

class ShortcutBindings
{
public:
    static const juce::Array<ShortcutInfo>& catalog();
    static bool allowedWhileTyping(ShortcutId id);

    void load(const juce::PropertiesFile& props);
    void save(juce::PropertiesFile& props) const;
    void resetToDefaults();

    juce::StringArray getDescriptions(ShortcutId id) const;
    juce::String getDisplayString(ShortcutId id) const;
    juce::String tooltipSuffix(ShortcutId id) const;

    void assign(ShortcutId id, juce::KeyPress key);
    void clear(ShortcutId id);

    const ShortcutInfo* find(const juce::KeyPress& key) const;
    const ShortcutInfo* findConflict(const juce::KeyPress& key, ShortcutId except) const;

    static juce::String describe(const juce::KeyPress& key);
    static juce::String describe(const juce::String& description);

private:
    static juce::String propertyKey(const ShortcutInfo& info);
    static juce::StringArray defaultDescriptions(ShortcutId id);

    std::map<ShortcutId, juce::StringArray> bindings;
};
