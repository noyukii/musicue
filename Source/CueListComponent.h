#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
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
    void addFadeCue();
    void setCues(juce::Array<Cue> newCues, int standbyCueId);
    const juce::Array<Cue>& getCues() const { return cues; }

    bool triggerStandby();
    void previewSelected();
    void stopSelectedCue();
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
    void markCueStarted(int cueId);
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
    std::function<bool(const Cue&)> onPlayCue;
    std::function<void(int)> onStopCue;
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
    bool runCue(int index, bool advanceStandby);
    bool runGroup(Cue& group, bool advanceStandby);
    void advanceStandby(const Cue& cue);
    int cueAfterSubtree(const Cue& cue) const;
    int firstChildId(int parentId) const;
    int chooseRandomChild(Cue& group);
    bool isCueRunning(int cueId) const;
    bool isDescendantOf(int cueId, int ancestorId) const;
    juce::Array<int> subtreeIds(int rootId) const;
    void updateGroupDurations();
    double calculateGroupDuration(int groupId) const;
    void paintGroupOutline(juce::Graphics&, int row, int width, int height) const;
    juce::Colour groupColour(const Cue&) const;
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
    std::map<int, int> activePlaylistChildren;
    std::map<int, juce::Array<int>> randomHistory;
    int standbyCueId = 0;
    int nextCueId = 1;
    bool standbyLinked = true;
    bool editingEnabled = true;
    int defaultContinueMode = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CueListComponent)
};
