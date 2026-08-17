#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "Cue.h"

class CueListComponent : public juce::Component,
                         private juce::TableListBoxModel,
                         public juce::DragAndDropTarget,
                         public juce::FileDragAndDropTarget
{
public:
    CueListComponent();
    void resized() override;

    void addCueFromFile(const juce::File& file, int insertIndex = -1);
    void addGroupCue();
    void setCues(juce::Array<Cue> newCues, int standbyCueId);
    const juce::Array<Cue>& getCues() const { return cues; }

    bool triggerStandby();
    void previewSelected();
    void resetStandby();
    void duplicateSelectedCue();
    void deleteSelectedCue();
    void copySelectedCue();
    void pasteCues();
    void moveSelectedCue(int delta);
    void groupSelectedCue();
    void toggleSelectedGroup();
    void undo();

    void markCueFinished(int cueId);
    void markAllIdle();
    void refreshDisplay();
    bool triggerHotkey(const juce::KeyPress& key);
    void handleAutoContinue(int cueId);
    void setStandbyLinked(bool linked) { standbyLinked = linked; }
    void setDefaultContinueMode(int mode) { defaultContinueMode = mode; }

    Cue* getSelectedCue();
    const Cue* getStandbyCue() const;
    int getStandbyCueId() const { return standbyCueId; }
    int getCueCount() const { return cues.size(); }
    bool hasStandby() const;
    void setEditingEnabled(bool enabled);

    std::function<double(const juce::File&)> durationProvider;
    std::function<void(const Cue&)> onPlayCue;
    std::function<void()> onSelectionChanged;
    std::function<void()> onContentChanged;

private:
    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int, int, int, bool) override;
    void paintCell(juce::Graphics&, int, int, int, int, bool) override;
    void cellClicked(int, int, const juce::MouseEvent&) override;
    void selectedRowsChanged(int) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

    void rebuildVisibleRows();
    void appendVisibleChildren(int parentId, int depth);
    int findCueIndex(int id) const;
    int findVisibleRow(int id) const;
    int nextSiblingId(const Cue&) const;
    void runCue(int index, bool advanceStandby);
    void runGroup(int groupId);
    void advanceStandby(const Cue& cue);
    void notifyContentChanged();
    void rememberUndo();
    void normaliseStandby();
    void showCueMenu(juce::Point<int>);
    static bool isAudioFile(const juce::File&);
    static juce::String formatTime(double);

    struct VisibleRow { int cueIndex = -1; int depth = 0; };
    juce::TableListBox table;
    juce::Array<Cue> cues, clipboard;
    juce::Array<VisibleRow> visibleRows;
    std::vector<juce::Array<Cue>> undoStack;
    int standbyCueId = 0;
    int nextCueId = 1;
    bool standbyLinked = true;
    bool editingEnabled = true;
    int defaultContinueMode = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CueListComponent)
};
