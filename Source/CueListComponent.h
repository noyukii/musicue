#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include "Palette.h"
#include "Cue.h"

class CueListComponent : public juce::Component,
                         private juce::TableListBoxModel,
                         public juce::DragAndDropTarget,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    CueListComponent();
    void resized() override;

    void addCueFromFile(const juce::File& file);
    void addGroupCue();
    void addFadeCue();
    void addCrossfadeCue(int toCueId);
    void showBulkFadeEditor();
    void previewCue(int cueId);
    int firstPlayingAudioCueId(int excludeId) const;
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
    void selectAdjacentCue(int delta);
    void selectAllCues();
    void groupSelectedCue();
    void ungroupSelectedCue();
    void indentSelectedCue();
    void outdentSelectedCue();
    void toggleSelectedGroup();
    void expandSelectedGroup();
    void collapseSelectedGroup();
    void expandAllGroups();
    void collapseAllGroups();
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
    juce::Array<int> getSelectedCueIds() const;
    const Cue* getStandbyCue() const;
    int getStandbyCueId() const { return standbyCueId; }
    int getCueCount() const { return cues.size(); }
    bool hasStandby() const;
    void setEditingEnabled(bool enabled);

    std::function<double(const juce::File&)> durationProvider;
    std::function<double(int cueId)> playheadProvider;
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
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void paintOverChildren(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void timerCallback() override;

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
    double playProgressFor(const Cue&) const;
    bool hasPlayingCues() const;
    void startPlayheadUpdates();
    void onPlayheadTick();
    void notifyContentChanged();
    void rememberUndo();
    void discardLastUndo();
    void normaliseStandby();
    void showCueMenu(juce::Point<int>);
    void applyFadeSetup(const Cue::FadeSetup& setup, const juce::Array<int>& fromIds);
    juce::Array<int> selectedAudioCueIds() const;
    static bool isAudioFile(const juce::File&);
    static juce::String formatTime(double);

    // Selection and tree-order move engine. Sibling order is the flat-array
    // order among cues sharing a parentId; subtrees need not be contiguous.
    juce::Array<int> canonicalIds(const juce::Array<int>& ids) const;
    int childCount(int parentId) const;
    int siblingIndexOf(const Cue&) const;
    int insertionIndexFor(int parentId, int siblingIndex) const;
    bool canMoveCueSubtrees(const juce::Array<int>& rootIds, int newParentId) const;
    bool moveCueSubtrees(const juce::Array<int>& rootIds, int newParentId, int siblingIndex);
    int createAudioCue(const juce::File& file, int parentId, int siblingIndex);
    void setGroupCollapsed(int cueId, bool collapsed);
    void selectRowsWithIds(const juce::Array<int>& ids);

    // Disclosure chevron hit-testing and hover feedback
    juce::Rectangle<float> disclosureCellBounds(int row) const;
    int columnX(int columnId) const;

    // Drop targeting: maps a mouse position to (parentId, siblingIndex)
    enum class DropZone { none, before, after, inside };
    DropZone dropZoneForRow(int row, int yInRow, int rowHeight) const;
    bool computeDropTarget(juce::Point<int> tablePos, int& parentId, int& siblingIndex,
                           DropZone& zone, int& indicatorRow, int& indicatorDepth) const;
    void updateDropTarget(juce::Point<int> tablePos);
    void clearDropTarget();
    static juce::Array<int> idsFromDragDescription(const juce::var& description);

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

    std::unique_ptr<juce::Drawable> chevronRight, chevronDown;
    std::unique_ptr<juce::Drawable> chevronRightHover, chevronDownHover;
    int hoveredDisclosureRow = -1;

    int dropIndicatorRow = -1;
    int dropIndicatorDepth = 0;
    DropZone dropIndicatorZone = DropZone::none;
    int dragScrollDirection = 0;
    juce::Point<int> lastDragTablePos { -1, -1 };
    int autoExpandCueId = 0;
    juce::uint32 autoExpandStartMs = 0;

    struct PlayheadTimer : juce::Timer
    {
        explicit PlayheadTimer(CueListComponent& o) : owner(o) {}
        void timerCallback() override { owner.onPlayheadTick(); }
        CueListComponent& owner;
    };
    PlayheadTimer playheadTimer { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CueListComponent)
};
