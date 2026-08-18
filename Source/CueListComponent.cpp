#include "CueListComponent.h"
#include <BinaryData.h>
#include "Icons.h"

namespace
{
void drawTypeIcon(juce::Graphics& g, bool group, bool fade, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour(colour);
    if (group)
    {
        g.drawRoundedRectangle(area.reduced(1.5f), 2.0f, 1.5f);
        g.drawLine(area.getX() + 3.0f, area.getCentreY(), area.getRight() - 3.0f, area.getCentreY(), 1.5f);
    }
    else if (fade)
    {
        juce::Path p;
        p.startNewSubPath(area.getX(), area.getBottom() - 2.0f);
        p.cubicTo(area.getX() + area.getWidth() * .28f, area.getBottom() - 2.0f,
                  area.getX() + area.getWidth() * .50f, area.getY() + 2.0f,
                  area.getRight(), area.getY() + 2.0f);
        g.strokePath(p, juce::PathStrokeType(1.8f));
    }
    else
    {
        juce::Path p;
        p.addRectangle(area.getX(), area.getY() + area.getHeight() * .33f, area.getWidth() * .28f, area.getHeight() * .34f);
        p.addTriangle(area.getX() + area.getWidth() * .25f, area.getY() + area.getHeight() * .30f,
                      area.getX() + area.getWidth() * .25f, area.getY() + area.getHeight() * .70f,
                      area.getX() + area.getWidth() * .68f, area.getY() + area.getHeight() * .1f);
        g.fillPath(p);
    }
}

// Flat-array index at which a new child of parentId should be inserted so that
// it becomes sibling number siblingIndex. Sibling order is the flat-array order
// among cues sharing a parentId; subtrees need not be contiguous.
int insertionIndexIn(const juce::Array<Cue>& list, int parentId, int siblingIndex)
{
    int childrenSeen = 0;
    int lastChildFlat = -1;
    int parentFlat = -1;

    for (int i = 0; i < list.size(); ++i)
    {
        if (list[i].id == parentId)
            parentFlat = i;
        if (list[i].parentId != parentId)
            continue;
        if (childrenSeen == siblingIndex)
            return i;
        ++childrenSeen;
        lastChildFlat = i;
    }

    if (lastChildFlat >= 0)
        return lastChildFlat + 1;
    if (parentFlat >= 0)
        return parentFlat + 1;
    return list.size();
}
}

CueListComponent::CueListComponent()
{
    addAndMakeVisible(table);
    table.setModel(this);
    table.setRowHeight(29);
    table.setMultipleSelectionEnabled(true);
    table.setColour(juce::ListBox::backgroundColourId, Palette::windowBg);
    table.setColour(juce::TableHeaderComponent::backgroundColourId, Palette::panelBg);
    table.setColour(juce::TableHeaderComponent::textColourId, Palette::textDim);
    table.setColour(juce::TableHeaderComponent::outlineColourId, Palette::divider);
    auto& h = table.getHeader(); h.setPopupMenuActive(false);
    const auto fixed = juce::TableHeaderComponent::visible;
    const auto flex = juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable | juce::TableHeaderComponent::draggable;
    h.addColumn("", 1, 34, 34, 34, fixed);
    h.addColumn("#", 2, 58, 42, 110, flex);
    h.addColumn("Cue", 3, 360, 150, -1, flex);
    h.addColumn("Target", 4, 180, 90, -1, flex);
    h.addColumn("Pre", 5, 76, 60, 120, flex);
    h.addColumn("Duration", 6, 90, 65, 140, flex);
    h.addColumn("Post", 7, 76, 60, 120, flex);
    h.setStretchToFitActive(true);
    table.addMouseListener(this, true);

    chevronRight = Icons::load(BinaryData::chevron_right_svg, BinaryData::chevron_right_svgSize, Palette::textDim);
    chevronDown = Icons::load(BinaryData::expand_more_svg, BinaryData::expand_more_svgSize, Palette::textDim);
    chevronRightHover = Icons::load(BinaryData::chevron_right_svg, BinaryData::chevron_right_svgSize, Palette::textPrimary);
    chevronDownHover = Icons::load(BinaryData::expand_more_svg, BinaryData::expand_more_svgSize, Palette::textPrimary);
}

void CueListComponent::resized() { table.setBounds(getLocalBounds()); }
bool CueListComponent::isAudioFile(const juce::File& f) { return f.hasFileExtension(".wav;.aif;.aiff;.mp3;.flac;.ogg"); }
juce::String CueListComponent::formatTime(double s) { return juce::String::formatted("%02d:%05.2f", (int) (s / 60.0), s - 60.0 * (int) (s / 60.0)); }

int CueListComponent::findCueIndex(int id) const { for (int i = 0; i < cues.size(); ++i) if (cues[i].id == id) return i; return -1; }
int CueListComponent::findVisibleRow(int id) const { for (int i = 0; i < visibleRows.size(); ++i) if (cues[visibleRows[i].cueIndex].id == id) return i; return -1; }
int CueListComponent::firstChildId(int parentId) const { for (const auto& cue : cues) if (cue.parentId == parentId) return cue.id; return 0; }

bool CueListComponent::isDescendantOf(int cueId, int ancestorId) const
{
    auto index = findCueIndex(cueId);
    while (index >= 0 && cues[index].parentId != 0)
    {
        if (cues[index].parentId == ancestorId)
            return true;
        index = findCueIndex(cues[index].parentId);
    }
    return false;
}

juce::Array<int> CueListComponent::subtreeIds(int rootId) const
{
    juce::Array<int> result { rootId };
    for (int i = 0; i < result.size(); ++i)
        for (const auto& cue : cues)
            if (cue.parentId == result[i])
                result.addIfNotAlreadyThere(cue.id);
    return result;
}

bool CueListComponent::isCueRunning(int cueId) const
{
    const auto index = findCueIndex(cueId);
    if (index < 0)
        return false;
    if (cues[index].playCount > 0)
        return true;
    for (const auto& cue : cues)
        if (cue.parentId == cueId && isCueRunning(cue.id))
            return true;
    return false;
}

double CueListComponent::calculateGroupDuration(int groupId) const
{
    const auto groupIndex = findCueIndex(groupId);
    if (groupIndex < 0)
        return 0.0;

    const auto durationFor = [this](const Cue& child)
    {
        const auto action = child.isGroup() ? calculateGroupDuration(child.id)
                                            : child.getEffectiveDuration();
        return child.preWait + action + child.postWait;
    };

    double duration = 0.0;
    bool foundChild = false;
    for (const auto& child : cues)
    {
        if (child.parentId != groupId)
            continue;

        const auto childDuration = durationFor(child);
        const auto mode = cues[groupIndex].groupMode;
        if (mode == Cue::GroupMode::playlist)
            duration += childDuration;
        else if (mode == Cue::GroupMode::timeline || mode == Cue::GroupMode::startRandom)
            duration = juce::jmax(duration, childDuration);
        else if (! foundChild)
            duration = childDuration;
        foundChild = true;
    }
    return duration;
}

void CueListComponent::updateGroupDurations()
{
    for (auto& cue : cues)
        if (cue.isGroup())
            cue.durationSeconds = calculateGroupDuration(cue.id);
}
void CueListComponent::appendVisibleChildren(int parentId, int depth)
{
    for (int i = 0; i < cues.size(); ++i) if (cues[i].parentId == parentId)
    {
        visibleRows.add({ i, depth });
        if (cues[i].isGroup() && ! cues[i].collapsed) appendVisibleChildren(cues[i].id, depth + 1);
    }
}
void CueListComponent::rebuildVisibleRows() { updateGroupDurations(); visibleRows.clearQuick(); appendVisibleChildren(0, 0); table.updateContent(); }
void CueListComponent::normaliseStandby()
{
    if (findCueIndex(standbyCueId) < 0)
        standbyCueId = visibleRows.isEmpty() ? 0 : cues[visibleRows[0].cueIndex].id;
}
void CueListComponent::rememberUndo()
{
    undoStack.push_back(cues); if (undoStack.size() > 40) undoStack.erase(undoStack.begin());
}
void CueListComponent::discardLastUndo()
{
    if (! undoStack.empty()) undoStack.pop_back();
}
void CueListComponent::undo()
{
    if (undoStack.empty()) return;
    cues = std::move(undoStack.back()); undoStack.pop_back(); rebuildVisibleRows(); normaliseStandby(); table.repaint(); notifyContentChanged();
}

//==============================================================================
// Selection helpers and the tree-order move engine

juce::Array<int> CueListComponent::getSelectedCueIds() const
{
    juce::Array<int> ids;
    const auto rows = table.getSelectedRows();
    for (int i = 0; i < rows.size(); ++i)
        if (juce::isPositiveAndBelow(rows[i], visibleRows.size()))
            ids.add(cues[visibleRows[rows[i]].cueIndex].id);
    return ids;
}

juce::Array<int> CueListComponent::canonicalIds(const juce::Array<int>& ids) const
{
    juce::Array<int> roots;
    for (const auto& cue : cues)
    {
        if (! ids.contains(cue.id))
            continue;

        auto coveredByAncestor = false;
        for (const auto other : ids)
            if (other != cue.id && isDescendantOf(cue.id, other))
            {
                coveredByAncestor = true;
                break;
            }

        if (! coveredByAncestor)
            roots.add(cue.id);
    }
    return roots;
}

int CueListComponent::childCount(int parentId) const
{
    auto count = 0;
    for (const auto& cue : cues)
        if (cue.parentId == parentId)
            ++count;
    return count;
}

int CueListComponent::siblingIndexOf(const Cue& cue) const
{
    auto index = 0;
    for (const auto& other : cues)
    {
        if (other.parentId != cue.parentId)
            continue;
        if (other.id == cue.id)
            return index;
        ++index;
    }
    return -1;
}

int CueListComponent::insertionIndexFor(int parentId, int siblingIndex) const
{
    return insertionIndexIn(cues, parentId, siblingIndex);
}

bool CueListComponent::canMoveCueSubtrees(const juce::Array<int>& rootIds, int newParentId) const
{
    if (rootIds.isEmpty())
        return false;
    if (newParentId != 0 && findCueIndex(newParentId) < 0)
        return false;
    for (const auto rootId : rootIds)
        if (newParentId == rootId || isDescendantOf(newParentId, rootId))
            return false;
    return true;
}

bool CueListComponent::moveCueSubtrees(const juce::Array<int>& rootIds, int newParentId, int siblingIndex)
{
    juce::Array<int> movingIds;
    for (const auto rootId : rootIds)
        for (const auto id : subtreeIds(rootId))
            movingIds.addIfNotAlreadyThere(id);

    const auto originalStart = findCueIndex(rootIds.getFirst());

    juce::Array<Cue> block, reduced;
    for (const auto& cue : cues)
        (movingIds.contains(cue.id) ? block : reduced).add(cue);

    const auto insertFlat = insertionIndexIn(reduced, newParentId, siblingIndex);

    if (rootIds.size() == 1 && cues[originalStart].parentId == newParentId && insertFlat == originalStart)
        return false;

    for (auto& cue : block)
        if (rootIds.contains(cue.id))
            cue.parentId = newParentId;

    cues.clearQuick();
    for (int i = 0; i < insertFlat; ++i)
        cues.add(reduced[i]);
    for (const auto& cue : block)
        cues.add(cue);
    for (int i = insertFlat; i < reduced.size(); ++i)
        cues.add(reduced[i]);
    return true;
}

void CueListComponent::selectRowsWithIds(const juce::Array<int>& ids)
{
    juce::SparseSet<int> rows;
    for (const auto id : ids)
        if (const auto row = findVisibleRow(id); row >= 0)
            rows.addRange(juce::Range<int>(row, row + 1));
    table.setSelectedRows(rows);
}

int CueListComponent::createAudioCue(const juce::File& file, int parentId, int siblingIndex)
{
    if (! file.existsAsFile())
        return 0;

    Cue cue;
    cue.id = nextCueId++;
    cue.number = juce::String(cues.size() + 1);
    cue.name = file.getFileNameWithoutExtension();
    cue.file = file;
    cue.continueMode = defaultContinueMode;
    cue.parentId = parentId;
    if (durationProvider)
        cue.durationSeconds = durationProvider(file);

    cues.insert(insertionIndexFor(parentId, siblingIndex), cue);
    return cue.id;
}

void CueListComponent::addCueFromFile(const juce::File& file)
{
    if (! file.existsAsFile()) return;
    const auto* selected = getSelectedCue();
    const auto parentId = selected == nullptr ? 0
                        : (selected->isGroup() ? selected->id : selected->parentId);
    const auto siblingIndex = selected == nullptr ? childCount(0)
                            : (selected->isGroup() ? childCount(selected->id) : siblingIndexOf(*selected) + 1);

    rememberUndo();
    const auto id = createAudioCue(file, parentId, siblingIndex);
    if (id == 0)
    {
        discardLastUndo();
        return;
    }
    rebuildVisibleRows(); normaliseStandby(); table.selectRow(findVisibleRow(id)); notifyContentChanged();
}
void CueListComponent::addGroupCue()
{
    rememberUndo();
    Cue cue;
    cue.id = nextCueId++;
    cue.kind = Cue::Kind::group;
    cue.number = juce::String(cues.size() + 1);
    cue.name = "(Untitled Group Cue)";
    auto insertAt = cues.size();
    if (auto* selected = getSelectedCue())
    {
        cue.parentId = selected->parentId;
        insertAt = insertionIndexFor(selected->parentId, siblingIndexOf(*selected) + 1);
    }
    cues.insert(insertAt, cue);
    rebuildVisibleRows(); normaliseStandby(); table.selectRow(findVisibleRow(cue.id)); notifyContentChanged();
}
void CueListComponent::addFadeCue()
{
    rememberUndo();
    Cue cue;
    cue.id = nextCueId++;
    cue.kind = Cue::Kind::fade;
    cue.number = juce::String(cues.size() + 1);
    cue.name = "(Untitled Fade Cue)";
    auto insertAt = cues.size();
    if (auto* selected = getSelectedCue())
    {
        cue.parentId = selected->parentId;
        insertAt = insertionIndexFor(selected->parentId, siblingIndexOf(*selected) + 1);
    }
    cues.insert(insertAt, cue);
    rebuildVisibleRows(); normaliseStandby(); table.selectRow(findVisibleRow(cue.id)); notifyContentChanged();
}
void CueListComponent::setCues(juce::Array<Cue> source, int standbyId)
{
    cues = std::move(source); nextCueId = 1;
    for (auto& cue : cues) { cue.id = cue.id > 0 ? cue.id : nextCueId; nextCueId = juce::jmax(nextCueId, cue.id + 1); cue.playCount = 0; if (cue.isAudio() && durationProvider && cue.file.existsAsFile()) cue.durationSeconds = durationProvider(cue.file); }
    standbyCueId = standbyId; undoStack.clear(); activePlaylistChildren.clear(); randomHistory.clear(); rebuildVisibleRows(); normaliseStandby();
    if (! visibleRows.isEmpty()) table.selectRow(0); else if (onSelectionChanged) onSelectionChanged(); notifyContentChanged();
}

int CueListComponent::cueAfterSubtree(const Cue& cue) const
{
    auto current = &cue;
    while (current != nullptr)
    {
        if (const auto sibling = nextSiblingId(*current); sibling != 0)
            return sibling;
        const auto parentIndex = findCueIndex(current->parentId);
        current = parentIndex >= 0 ? &cues.getReference(parentIndex) : nullptr;
    }
    return 0;
}

int CueListComponent::chooseRandomChild(Cue& group)
{
    juce::Array<int> candidates;
    for (const auto& child : cues)
        if (child.parentId == group.id && child.armed && ! isCueRunning(child.id))
            candidates.add(child.id);

    if (candidates.isEmpty())
        return 0;

    auto& history = randomHistory[group.id];
    juce::Array<int> available;
    for (const auto id : candidates)
        if (! history.contains(id))
            available.add(id);

    if (available.isEmpty())
    {
        history.clearQuick();
        available = candidates;
    }

    const auto chosen = available[juce::Random::getSystemRandom().nextInt(available.size())];
    history.add(chosen);
    return chosen;
}

bool CueListComponent::runGroup(Cue& group, bool advance)
{
    bool started = false;
    const auto first = firstChildId(group.id);

    if (group.groupMode == Cue::GroupMode::timeline)
    {
        for (int i = 0; i < cues.size(); ++i)
            if (cues[i].parentId == group.id)
                started = runCue(i, false) || started;
    }
    else if (group.groupMode == Cue::GroupMode::startRandom)
    {
        const auto chosen = chooseRandomChild(group);
        if (chosen != 0)
            started = runCue(findCueIndex(chosen), false);
    }
    else if (first != 0)
    {
        started = runCue(findCueIndex(first), false);
        if (group.groupMode == Cue::GroupMode::playlist && started)
            activePlaylistChildren[group.id] = first;
    }

    if (advance)
    {
        if (group.groupMode == Cue::GroupMode::startFirstAndEnter && first != 0)
        {
            const auto firstIndex = findCueIndex(first);
            standbyCueId = firstIndex >= 0 ? cueAfterSubtree(cues[firstIndex]) : cueAfterSubtree(group);
        }
        else
        {
            standbyCueId = cueAfterSubtree(group);
        }
    }

    return started;
}

void CueListComponent::advanceStandby(const Cue& cue)
{
    standbyCueId = cueAfterSubtree(cue);
}

bool CueListComponent::runCue(int index, bool advance)
{
    if (! juce::isPositiveAndBelow(index, cues.size())) return false;
    auto& cue = cues.getReference(index);
    if (! cue.armed)
    {
        if (advance)
            advanceStandby(cue);
        table.repaint();
        return false;
    }

    bool started = false;
    if (cue.isGroup())
        started = runGroup(cue, advance);
    else
    {
        started = onPlayCue != nullptr && onPlayCue(cue);
        if (started)
        {
            ++cue.playCount;
            if (advance) advanceStandby(cue);
        }
    }
    table.repaint(); notifyContentChanged();
    return started;
}
bool CueListComponent::triggerStandby()
{
    bool started = false;
    for (int guard = 0; guard < cues.size(); ++guard)
    {
        const auto index = findCueIndex(standbyCueId);
        if (index < 0)
            break;

        const auto autoFollow = cues[index].continueMode == 2;
        started = runCue(index, true) || started;
        if (! autoFollow)
            break;
    }

    if (standbyLinked)
        if (const auto row = findVisibleRow(standbyCueId); row >= 0)
            table.selectRow(row);
    return started;
}
void CueListComponent::previewSelected() { if (auto* c = getSelectedCue()) runCue(findCueIndex(c->id), false); }
void CueListComponent::stopSelectedCue()
{
    juce::Array<int> ids;
    for (const auto rootId : getSelectedCueIds())
        for (const auto id : subtreeIds(rootId))
            ids.addIfNotAlreadyThere(id);

    for (const auto id : ids)
        if (onStopCue != nullptr)
            onStopCue(id);

    for (auto it = activePlaylistChildren.begin(); it != activePlaylistChildren.end();)
    {
        if (ids.contains(it->first) || ids.contains(it->second))
            it = activePlaylistChildren.erase(it);
        else
            ++it;
    }
}
void CueListComponent::resetStandby() { standbyCueId = visibleRows.isEmpty() ? 0 : cues[visibleRows[0].cueIndex].id; if (standbyLinked) table.selectRow(0); table.repaint(); notifyContentChanged(); }
void CueListComponent::duplicateSelectedCue()
{
    if (! editingEnabled) return;
    const auto roots = canonicalIds(getSelectedCueIds());
    if (roots.isEmpty()) return;

    rememberUndo();
    juce::Array<int> newRoots;
    for (int r = roots.size() - 1; r >= 0; --r)
    {
        const auto rootId = roots[r];
        const auto ids = subtreeIds(rootId);
        juce::Array<Cue> copies;
        for (const auto& cue : cues)
            if (ids.contains(cue.id))
                copies.add(cue);

        std::map<int, int> idMap;
        for (auto& copy : copies)
        {
            idMap[copy.id] = nextCueId;
            copy.id = nextCueId++;
            copy.playCount = 0;
        }
        for (auto& copy : copies)
        {
            if (const auto parent = idMap.find(copy.parentId); parent != idMap.end())
                copy.parentId = parent->second;
            if (copy.isFade())
                for (auto& action : copy.fadeActions)
                    if (const auto target = idMap.find(action.targetCueId); target != idMap.end())
                        action.targetCueId = target->second;
            if (copy.id == idMap[rootId])
                copy.name += " copy";
        }

        const auto& source = cues[findCueIndex(rootId)];
        auto insertAt = insertionIndexFor(source.parentId, siblingIndexOf(source) + 1);
        for (const auto& copy : copies)
            cues.insert(insertAt++, copy);
        newRoots.insert(0, idMap[rootId]);
    }

    rebuildVisibleRows(); selectRowsWithIds(newRoots); notifyContentChanged();
}
void CueListComponent::deleteSelectedCue()
{
    if (! editingEnabled) return;
    const auto roots = canonicalIds(getSelectedCueIds());
    if (roots.isEmpty()) return;

    juce::Array<int> ids;
    for (const auto rootId : roots)
        for (const auto id : subtreeIds(rootId))
            ids.addIfNotAlreadyThere(id);

    const auto selectedRows = table.getSelectedRows();
    const auto anchorRow = selectedRows.size() > 0 ? selectedRows[0] : 0;

    rememberUndo();
    cues.removeIf([&ids](const Cue& c) { return ids.contains(c.id); });
    for (auto& cue : cues)
        if (cue.isFade())
            cue.fadeActions.removeIf([&ids](const Cue::FadeAction& action) { return ids.contains(action.targetCueId); });
    for (auto it = activePlaylistChildren.begin(); it != activePlaylistChildren.end();)
    {
        if (ids.contains(it->first) || ids.contains(it->second))
            it = activePlaylistChildren.erase(it);
        else
            ++it;
    }

    rebuildVisibleRows(); normaliseStandby();
    if (! visibleRows.isEmpty()) table.selectRow(juce::jmin(anchorRow, visibleRows.size() - 1));
    notifyContentChanged();
}
void CueListComponent::copySelectedCue()
{
    const auto roots = canonicalIds(getSelectedCueIds());
    if (roots.isEmpty()) return;

    juce::Array<int> ids;
    for (const auto rootId : roots)
        for (const auto id : subtreeIds(rootId))
            ids.addIfNotAlreadyThere(id);

    clipboard.clearQuick();
    for (const auto& cue : cues)
        if (ids.contains(cue.id))
            clipboard.add(cue);
}
void CueListComponent::pasteCues()
{
    if (clipboard.isEmpty() || ! editingEnabled) return;

    juce::Array<int> clipboardIds;
    for (const auto& cue : clipboard)
        clipboardIds.add(cue.id);
    juce::Array<int> clipRoots;
    for (const auto& cue : clipboard)
        if (! clipboardIds.contains(cue.parentId))
            clipRoots.add(cue.id);

    const auto* selected = getSelectedCue();
    const auto destinationParent = selected == nullptr ? 0
        : (selected->isGroup() ? selected->id : selected->parentId);
    const auto siblingIndex = selected == nullptr ? childCount(0)
        : (selected->isGroup() ? childCount(selected->id) : siblingIndexOf(*selected) + 1);

    rememberUndo();
    std::map<int, int> idMap;
    for (const auto& source : clipboard)
        idMap[source.id] = nextCueId++;

    auto insertAt = insertionIndexFor(destinationParent, siblingIndex);
    juce::Array<int> newRoots;
    for (auto copy : clipboard)
    {
        const auto oldId = copy.id;
        copy.id = idMap[oldId];
        copy.parentId = clipRoots.contains(oldId) ? destinationParent : idMap[copy.parentId];
        if (copy.isFade())
            for (auto& action : copy.fadeActions)
                if (const auto target = idMap.find(action.targetCueId); target != idMap.end())
                    action.targetCueId = target->second;
        copy.playCount = 0;
        if (clipRoots.contains(oldId))
            newRoots.add(copy.id);
        cues.insert(insertAt++, copy);
    }

    rebuildVisibleRows(); selectRowsWithIds(newRoots); notifyContentChanged();
}
void CueListComponent::moveSelectedCue(int delta)
{
    if (! editingEnabled || delta == 0) return;
    const auto roots = canonicalIds(getSelectedCueIds());
    if (roots.isEmpty()) return;

    const auto firstIndex = findCueIndex(roots.getFirst());
    const auto lastIndex = findCueIndex(roots.getLast());
    if (firstIndex < 0 || lastIndex < 0) return;

    auto targetParent = cues[firstIndex].parentId;
    const auto firstSibling = siblingIndexOf(cues[firstIndex]);
    const auto siblings = childCount(targetParent);
    auto target = juce::jlimit(0, juce::jmax(0, siblings - (int) roots.size()), firstSibling + delta);

    if (target == firstSibling && roots.size() == 1 && targetParent != 0)
    {
        // At the edge of a group: pop out to the grandparent level.
        const auto parentIndex = findCueIndex(targetParent);
        target = siblingIndexOf(cues[parentIndex]) + (delta > 0 ? 1 : 0);
        targetParent = cues[parentIndex].parentId;
    }
    else if (target == firstSibling)
    {
        return;
    }

    if (! canMoveCueSubtrees(roots, targetParent))
        return;

    rememberUndo();
    if (moveCueSubtrees(roots, targetParent, target))
    {
        rebuildVisibleRows(); selectRowsWithIds(roots); notifyContentChanged();
    }
    else
    {
        discardLastUndo();
    }
}
void CueListComponent::groupSelectedCue()
{
    if (! editingEnabled) return;
    const auto roots = canonicalIds(getSelectedCueIds());
    if (roots.isEmpty()) return;

    rememberUndo();
    const auto firstIndex = findCueIndex(roots.getFirst());
    Cue group;
    group.id = nextCueId++;
    group.kind = Cue::Kind::group;
    group.number = cues[firstIndex].number;
    group.name = "(Untitled Group Cue)";
    group.parentId = cues[firstIndex].parentId;
    cues.insert(firstIndex, group);
    moveCueSubtrees(roots, group.id, 0);
    rebuildVisibleRows(); table.selectRow(findVisibleRow(group.id)); notifyContentChanged();
}
void CueListComponent::ungroupSelectedCue()
{
    if (! editingEnabled) return;

    juce::Array<int> groups;
    for (const auto id : canonicalIds(getSelectedCueIds()))
        if (const auto index = findCueIndex(id); index >= 0 && cues[index].isGroup())
            groups.add(id);
    if (groups.isEmpty()) return;

    rememberUndo();
    juce::Array<int> reselect;
    for (const auto groupId : groups)
    {
        const auto newParent = cues[findCueIndex(groupId)].parentId;
        for (auto& cue : cues)
            if (cue.parentId == groupId)
            {
                cue.parentId = newParent;
                reselect.add(cue.id);
            }
        for (auto& cue : cues)
            if (cue.isFade())
                cue.fadeActions.removeIf([groupId](const Cue::FadeAction& action) { return action.targetCueId == groupId; });
        activePlaylistChildren.erase(groupId);
        randomHistory.erase(groupId);
        cues.removeIf([groupId](const Cue& c) { return c.id == groupId; });
    }

    rebuildVisibleRows(); normaliseStandby(); selectRowsWithIds(reselect); notifyContentChanged();
}
void CueListComponent::indentSelectedCue()
{
    if (! editingEnabled) return;
    const auto roots = canonicalIds(getSelectedCueIds());
    if (roots.isEmpty()) return;

    const auto firstIndex = findCueIndex(roots.getFirst());
    const auto parentId = cues[firstIndex].parentId;

    auto previousSibling = 0;
    for (int i = 0; i < firstIndex; ++i)
        if (cues[i].parentId == parentId)
            previousSibling = cues[i].id;

    if (previousSibling == 0)
        return;
    const auto targetIndex = findCueIndex(previousSibling);
    if (! cues[targetIndex].isGroup())
        return;

    rememberUndo();
    cues.getReference(targetIndex).collapsed = false;
    moveCueSubtrees(roots, previousSibling, childCount(previousSibling));
    rebuildVisibleRows(); selectRowsWithIds(roots); notifyContentChanged();
}
void CueListComponent::outdentSelectedCue()
{
    if (! editingEnabled) return;

    juce::Array<int> roots;
    for (const auto id : canonicalIds(getSelectedCueIds()))
        if (const auto index = findCueIndex(id); index >= 0 && cues[index].parentId != 0)
            roots.add(id);
    if (roots.isEmpty()) return;

    rememberUndo();
    for (int r = roots.size() - 1; r >= 0; --r)
    {
        const auto index = findCueIndex(roots[r]);
        const auto parentIndex = findCueIndex(cues[index].parentId);
        const auto& parent = cues[parentIndex];
        moveCueSubtrees({ roots[r] }, parent.parentId, siblingIndexOf(parent) + 1);
    }
    rebuildVisibleRows(); selectRowsWithIds(roots); notifyContentChanged();
}
void CueListComponent::setGroupCollapsed(int cueId, bool collapsed)
{
    const auto index = findCueIndex(cueId);
    if (index < 0 || ! cues[index].isGroup() || cues[index].collapsed == collapsed)
        return;

    const auto selection = getSelectedCueIds();
    cues.getReference(index).collapsed = collapsed;
    rebuildVisibleRows(); selectRowsWithIds(selection); table.repaint(); notifyContentChanged();
}
void CueListComponent::toggleSelectedGroup()
{
    if (auto* cue = getSelectedCue(); cue != nullptr && cue->isGroup())
    {
        const auto id = cue->id;
        setGroupCollapsed(id, ! cue->collapsed);
        table.selectRow(findVisibleRow(id));
    }
}
void CueListComponent::expandSelectedGroup()
{
    auto* cue = getSelectedCue();
    if (cue == nullptr || ! cue->isGroup())
        return;

    const auto id = cue->id;
    if (cue->collapsed)
    {
        setGroupCollapsed(id, false);
        table.selectRow(findVisibleRow(id));
    }
    else if (const auto first = firstChildId(id); first != 0)
    {
        table.selectRow(findVisibleRow(first));
    }
}
void CueListComponent::collapseSelectedGroup()
{
    auto* cue = getSelectedCue();
    if (cue == nullptr)
        return;

    if (cue->isGroup() && ! cue->collapsed)
    {
        const auto id = cue->id;
        setGroupCollapsed(id, true);
        table.selectRow(findVisibleRow(id));
    }
    else if (cue->parentId != 0)
    {
        table.selectRow(findVisibleRow(cue->parentId));
    }
}
void CueListComponent::expandAllGroups()
{
    const auto selection = getSelectedCueIds();
    for (auto& cue : cues)
        if (cue.isGroup())
            cue.collapsed = false;
    rebuildVisibleRows(); selectRowsWithIds(selection); table.repaint(); notifyContentChanged();
}
void CueListComponent::collapseAllGroups()
{
    const auto selection = getSelectedCueIds();
    for (auto& cue : cues)
        if (cue.isGroup())
            cue.collapsed = true;
    rebuildVisibleRows(); selectRowsWithIds(selection); table.repaint(); notifyContentChanged();
}
void CueListComponent::markCueFinished(int id) { if (auto i = findCueIndex(id); i >= 0) cues.getReference(i).playCount = juce::jmax(0, cues[i].playCount - 1); table.repaint(); notifyContentChanged(); }
void CueListComponent::markCueStarted(int id) { if (auto i = findCueIndex(id); i >= 0) ++cues.getReference(i).playCount; table.repaint(); notifyContentChanged(); }
void CueListComponent::markAllIdle() { for (auto& c : cues) c.playCount = 0; activePlaylistChildren.clear(); table.repaint(); notifyContentChanged(); }
void CueListComponent::refreshDisplay() { rebuildVisibleRows(); table.repaint(); }
int CueListComponent::nextSiblingId(const Cue& cue) const { bool seen = false; for (auto& other : cues) { if (other.parentId != cue.parentId) continue; if (seen) return other.id; if (other.id == cue.id) seen = true; } return 0; }
void CueListComponent::handleAutoContinue(int id)
{
    const auto index = findCueIndex(id);
    if (index < 0) return;
    const auto cue = cues[index];
    const auto parentIndex = findCueIndex(cue.parentId);

    if (parentIndex >= 0 && cues[parentIndex].isGroup()
        && cues[parentIndex].groupMode == Cue::GroupMode::playlist)
    {
        const auto groupId = cues[parentIndex].id;
        const auto active = activePlaylistChildren.find(groupId);
        if (active == activePlaylistChildren.end() || active->second != id)
            return;

        const auto next = nextSiblingId(cue);
        if (next == 0)
        {
            activePlaylistChildren.erase(active);
            table.repaint();
            return;
        }

        const auto delay = static_cast<int>(juce::jmax(0.0, cue.postWait * 1000.0));
        juce::Timer::callAfterDelay(delay, [this, groupId, id, next]
        {
            const auto current = activePlaylistChildren.find(groupId);
            if (current == activePlaylistChildren.end() || current->second != id)
                return;
            current->second = next;
            if (! runCue(findCueIndex(next), false))
                activePlaylistChildren.erase(groupId);
        });
        return;
    }

    if (cue.continueMode != 1) return;
    const auto next = nextSiblingId(cue);
    const auto delay = static_cast<int>(juce::jmax(0.0, cue.postWait * 1000.0));
    if (next != 0) juce::Timer::callAfterDelay(delay, [this, next] { runCue(findCueIndex(next), false); });
}
bool CueListComponent::triggerHotkey(const juce::KeyPress& key) { for (int i = 0; i < cues.size(); ++i) if (cues[i].hotkey == key.getTextDescription()) { runCue(i, false); return true; } return false; }
Cue* CueListComponent::getSelectedCue() { const auto row = table.getSelectedRow(); return juce::isPositiveAndBelow(row, visibleRows.size()) ? &cues.getReference(visibleRows[row].cueIndex) : nullptr; }
const Cue* CueListComponent::getStandbyCue() const { const auto i = findCueIndex(standbyCueId); return i >= 0 ? &cues.getReference(i) : nullptr; }
bool CueListComponent::hasStandby() const { return getStandbyCue() != nullptr; }
void CueListComponent::setEditingEnabled(bool enabled) { editingEnabled = enabled; }
void CueListComponent::notifyContentChanged() { if (onContentChanged) onContentChanged(); }
int CueListComponent::getNumRows() { return visibleRows.size(); }
void CueListComponent::paintRowBackground(juce::Graphics& g, int row, int, int, bool selected) { g.fillAll(selected ? Palette::selection : (row % 2 == 0 ? Palette::rowA : Palette::rowB)); }

juce::Colour CueListComponent::groupColour(const Cue& cue) const
{
    switch (cue.groupMode)
    {
        case Cue::GroupMode::timeline: return juce::Colour(0xff00c83c);
        case Cue::GroupMode::playlist: return juce::Colour(0xffff9f0a);
        case Cue::GroupMode::startFirstAndEnter:
        case Cue::GroupMode::startFirst: return juce::Colour(0xff3a9cff);
        case Cue::GroupMode::startRandom: return juce::Colour(0xffaf52de);
    }
    return Palette::standbyGreen;
}

void CueListComponent::paintGroupOutline(juce::Graphics& g, int row, int width, int height) const
{
    if (! juce::isPositiveAndBelow(row, visibleRows.size()))
        return;

    auto cueId = cues[visibleRows[row].cueIndex].id;
    while (cueId != 0)
    {
        const auto cueIndex = findCueIndex(cueId);
        if (cueIndex < 0)
            break;
        const auto& cue = cues[cueIndex];
        if (cue.isGroup())
        {
            const auto groupRow = findVisibleRow(cue.id);
            if (groupRow >= 0)
            {
                const auto groupDepth = visibleRows[groupRow].depth;
                auto lastRow = groupRow;
                while (lastRow + 1 < visibleRows.size()
                       && visibleRows[lastRow + 1].depth > groupDepth)
                    ++lastRow;

                const auto left = 1.0f + static_cast<float>(groupDepth * 14);
                const auto right = static_cast<float>(width) - 1.0f;
                g.setColour(groupColour(cue));
                g.drawVerticalLine(static_cast<int>(left), 0.0f, static_cast<float>(height));
                g.drawVerticalLine(static_cast<int>(right), 0.0f, static_cast<float>(height));
                if (row == groupRow)
                    g.drawHorizontalLine(1, left, right);
                if (row == lastRow)
                    g.drawHorizontalLine(height - 2, left, right);
            }
        }
        cueId = cue.parentId;
    }
}

int CueListComponent::columnX(int columnId) const
{
    const auto index = table.getHeader().getIndexOfColumnId(columnId, true);
    return index >= 0 ? table.getHeader().getColumnPosition(index).getX() : 0;
}

juce::Rectangle<float> CueListComponent::disclosureCellBounds(int row) const
{
    if (! juce::isPositiveAndBelow(row, visibleRows.size()))
        return {};
    if (! cues[visibleRows[row].cueIndex].isGroup())
        return {};
    const auto indent = 8 + visibleRows[row].depth * 18;
    return { (float) indent, ((float) table.getRowHeight() - 16.0f) * 0.5f, 16.0f, 16.0f };
}

void CueListComponent::paintCell(juce::Graphics& g, int row, int column, int width, int height, bool)
{
    if (! juce::isPositiveAndBelow(row, visibleRows.size())) return; const auto& vr = visibleRows[row]; const auto& c = cues[vr.cueIndex];
    if (column == 1) { if (c.id == standbyCueId || (c.isGroup() && isDescendantOf(standbyCueId, c.id) && c.collapsed)) { juce::Path triangle; triangle.addTriangle(4.f, (float) height/2-5.f, 4.f, (float) height/2+5.f, 11.f, (float) height/2); g.setColour(juce::Colours::white); g.fillPath(triangle); } drawTypeIcon(g, c.isGroup(), c.isFade(), { 17.f, 7.f, 14.f, (float) height-14.f }, isCueRunning(c.id) ? (c.isGroup() ? groupColour(c) : Palette::standbyGreen) : Palette::textDim); return; }
    juce::String text; auto colour = Palette::textPrimary; auto just = juce::Justification::centredLeft;
    if (column == 2) { text = c.number; just = juce::Justification::centred; }
    if (column == 3)
    {
        const auto indent = 8 + vr.depth * 18;
        auto textX = indent + 10;

        if (c.isGroup())
        {
            const auto& icon = c.collapsed ? (row == hoveredDisclosureRow ? chevronRightHover : chevronRight)
                                           : (row == hoveredDisclosureRow ? chevronDownHover : chevronDown);
            if (icon != nullptr)
                icon->drawWithin(g, { (float) indent, ((float) height - 16.0f) * 0.5f, 16.0f, 16.0f },
                                 juce::RectanglePlacement::centred, 1.0f);
            textX = indent + 20;
        }

        if (! c.armed) colour = Palette::textDim;
        const juce::Font font(juce::FontOptions().withHeight(13.0f));
        g.setFont(font);
        g.setColour(colour);
        g.drawText(c.name, textX, 0, width - textX - 18, height, juce::Justification::centredLeft, true);

        if (c.isGroup() && c.collapsed)
        {
            const auto badgeX = textX + font.getStringWidth(c.name) + 6;
            if (badgeX < width - 24)
            {
                g.setColour(Palette::textDim);
                g.drawText("(" + juce::String(childCount(c.id)) + ")", badgeX, 0,
                           width - badgeX - 6, height, juce::Justification::centredLeft, true);
            }
        }

        paintGroupOutline(g, row, width, height);
        return;
    }
    if (column == 4) { text = c.isGroup() ? juce::String() : (c.isFade() ? juce::String(c.fadeActions.size()) + (c.fadeActions.size() == 1 ? " target" : " targets") : (c.target.isNotEmpty() ? c.target : c.file.getFileName())); colour = Palette::textDim; }
    if (column == 5) { text = formatTime(c.preWait); just = juce::Justification::centredRight; }
    if (column == 6) { text = formatTime(c.getEffectiveDuration()); just = juce::Justification::centredRight; }
    if (column == 7) { text = formatTime(c.postWait); just = juce::Justification::centredRight; }
    g.setColour(colour); g.setFont(juce::Font(juce::FontOptions().withHeight(13.f))); g.drawText(text, 6, 0, width - 12, height, just, true);
}

void CueListComponent::paintOverChildren(juce::Graphics& g)
{
    if (dropIndicatorZone == DropZone::none
        || ! juce::isPositiveAndBelow(dropIndicatorRow, visibleRows.size()))
        return;

    const auto accent = juce::Colour(0xff3a9cff);
    const auto origin = getLocalPoint(&table, juce::Point<int>());

    if (dropIndicatorZone == DropZone::inside)
    {
        auto rect = table.getRowPosition(dropIndicatorRow, true).toFloat();
        rect.translate((float) origin.x, (float) origin.y);
        rect = rect.reduced(1.0f);
        g.setColour(accent.withAlpha(0.16f));
        g.fillRoundedRectangle(rect, 4.0f);
        g.setColour(accent);
        g.drawRoundedRectangle(rect, 4.0f, 1.6f);
        return;
    }

    auto lineRow = dropIndicatorRow;
    if (dropIndicatorZone == DropZone::after)
    {
        const auto depth = visibleRows[lineRow].depth;
        while (lineRow + 1 < visibleRows.size() && visibleRows[lineRow + 1].depth > depth)
            ++lineRow;
    }

    const auto rect = table.getRowPosition(lineRow, true);
    const auto y = (float) origin.y + (float) (dropIndicatorZone == DropZone::before ? rect.getY() : rect.getBottom());
    const auto x = (float) (origin.x + columnX(3) + 8 + dropIndicatorDepth * 18);

    g.setColour(accent);
    g.drawLine(x, y, (float) getWidth() - 2.0f, y, 2.0f);
    juce::Path marker;
    marker.addTriangle(x, y - 4.0f, x, y + 4.0f, x + 6.0f, y);
    g.fillPath(marker);
}

void CueListComponent::cellClicked(int row, int column, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow(row, visibleRows.size()))
        return;

    if (e.mods.isPopupMenu())
    {
        if (! table.isRowSelected(row))
            table.selectRow(row);
        showCueMenu(e.getScreenPosition());
        return;
    }

    const auto& cue = cues[visibleRows[row].cueIndex];
    if (! cue.isGroup())
        return;

    if (column == 3)
    {
        const auto bounds = disclosureCellBounds(row);
        const auto cellX = (float) (e.x - columnX(3));
        if (cellX >= bounds.getX() - 2.0f && cellX <= bounds.getRight() + 2.0f)
        {
            setGroupCollapsed(cue.id, ! cue.collapsed);
            return;
        }
    }

    if (e.getNumberOfClicks() == 2)
        setGroupCollapsed(cue.id, ! cue.collapsed);
}
void CueListComponent::selectedRowsChanged(int)
{
    if (standbyLinked && table.getNumSelectedRows() == 1)
        if (auto* c = getSelectedCue())
            standbyCueId = c->id;
    table.repaint(); if (onSelectionChanged) onSelectionChanged(); notifyContentChanged();
}

//==============================================================================
// Drag & drop

juce::var CueListComponent::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    if (rows.isEmpty() || ! editingEnabled)
        return {};

    juce::Array<juce::var> ids;
    for (int i = 0; i < rows.size(); ++i)
        if (juce::isPositiveAndBelow(rows[i], visibleRows.size()))
            ids.add(cues[visibleRows[rows[i]].cueIndex].id);
    return ids.isEmpty() ? juce::var() : juce::var(ids);
}

juce::Array<int> CueListComponent::idsFromDragDescription(const juce::var& description)
{
    juce::Array<int> ids;
    if (const auto* array = description.getArray())
        for (const auto& value : *array)
            ids.add((int) value);
    return ids;
}

bool CueListComponent::isInterestedInDragSource(const SourceDetails& d)
{
    return editingEnabled && d.sourceComponent == &table && d.description.isArray();
}

CueListComponent::DropZone CueListComponent::dropZoneForRow(int row, int yInRow, int rowHeight) const
{
    if (cues[visibleRows[row].cueIndex].isGroup())
    {
        if (yInRow < rowHeight / 4)
            return DropZone::before;
        if (yInRow >= rowHeight * 3 / 4)
            return DropZone::after;
        return DropZone::inside;
    }
    return yInRow < rowHeight / 2 ? DropZone::before : DropZone::after;
}

bool CueListComponent::computeDropTarget(juce::Point<int> pos, int& parentId, int& siblingIndex,
                                         DropZone& zone, int& indicatorRow, int& indicatorDepth) const
{
    parentId = 0;
    siblingIndex = 0;
    zone = DropZone::none;
    indicatorRow = -1;
    indicatorDepth = 0;

    if (visibleRows.isEmpty())
        return true;

    const auto row = table.getRowContainingPosition(pos.x, pos.y);

    if (row < 0)
    {
        if (pos.y < table.getRowPosition(0, true).getY())
        {
            const auto& first = cues[visibleRows.getFirst().cueIndex];
            parentId = first.parentId;
            siblingIndex = siblingIndexOf(first);
            zone = DropZone::before;
            indicatorRow = 0;
            indicatorDepth = visibleRows.getFirst().depth;
        }
        else
        {
            siblingIndex = childCount(0);
            zone = DropZone::after;
            indicatorRow = visibleRows.size() - 1;
        }
        return true;
    }

    const auto& vr = visibleRows[row];
    const auto& cue = cues[vr.cueIndex];
    const auto rowRect = table.getRowPosition(row, true);
    zone = dropZoneForRow(row, pos.y - rowRect.getY(), rowRect.getHeight());

    if (zone == DropZone::inside)
    {
        if (cue.isGroup())
        {
            parentId = cue.id;
            siblingIndex = childCount(cue.id);
        }
        else
        {
            zone = DropZone::after;
        }
    }

    if (zone != DropZone::inside)
    {
        parentId = cue.parentId;
        siblingIndex = siblingIndexOf(cue) + (zone == DropZone::after ? 1 : 0);
    }

    indicatorRow = row;
    indicatorDepth = vr.depth;
    return true;
}

void CueListComponent::updateDropTarget(juce::Point<int> tablePos)
{
    int parentId = 0, siblingIndex = 0, row = -1, depth = 0;
    DropZone zone = DropZone::none;
    computeDropTarget(tablePos, parentId, siblingIndex, zone, row, depth);

    if (row != dropIndicatorRow || zone != dropIndicatorZone)
    {
        dropIndicatorRow = row;
        dropIndicatorZone = zone;
        dropIndicatorDepth = depth;
        repaint();
    }

    auto expandCandidate = 0;
    if (zone == DropZone::inside && row >= 0)
    {
        const auto& cue = cues[visibleRows[row].cueIndex];
        if (cue.isGroup() && cue.collapsed)
            expandCandidate = cue.id;
    }
    if (expandCandidate != autoExpandCueId)
    {
        autoExpandCueId = expandCandidate;
        autoExpandStartMs = juce::Time::getMillisecondCounter();
    }
}

void CueListComponent::clearDropTarget()
{
    dropIndicatorRow = -1;
    dropIndicatorZone = DropZone::none;
    dragScrollDirection = 0;
    lastDragTablePos = { -1, -1 };
    autoExpandCueId = 0;
    stopTimer();
    repaint();
}

void CueListComponent::itemDragEnter(const SourceDetails& d)
{
    itemDragMove(d);
}

void CueListComponent::itemDragMove(const SourceDetails& d)
{
    const auto tablePos = table.getLocalPoint(this, d.localPosition);
    lastDragTablePos = tablePos;
    updateDropTarget(tablePos);

    constexpr auto margin = 24;
    dragScrollDirection = tablePos.y < margin ? -1
                        : tablePos.y > table.getHeight() - margin ? 1 : 0;

    if (dragScrollDirection != 0 || autoExpandCueId != 0)
    {
        if (! isTimerRunning())
            startTimer(40);
    }
    else
    {
        stopTimer();
    }
}

void CueListComponent::itemDragExit(const SourceDetails&)
{
    clearDropTarget();
}

void CueListComponent::itemDropped(const SourceDetails& d)
{
    const auto ids = canonicalIds(idsFromDragDescription(d.description));

    int parentId = 0, siblingIndex = 0, indicatorRow = -1, indicatorDepth = 0;
    DropZone zone = DropZone::none;
    const auto tablePos = table.getLocalPoint(this, d.localPosition);

    if (! ids.isEmpty()
        && computeDropTarget(tablePos, parentId, siblingIndex, zone, indicatorRow, indicatorDepth)
        && canMoveCueSubtrees(ids, parentId)
        && ! (indicatorRow >= 0 && ids.contains(cues[visibleRows[indicatorRow].cueIndex].id)))
    {
        rememberUndo();
        if (moveCueSubtrees(ids, parentId, siblingIndex))
        {
            rebuildVisibleRows();
            selectRowsWithIds(ids);
            notifyContentChanged();
        }
        else
        {
            discardLastUndo();
        }
    }
    clearDropTarget();
}

void CueListComponent::timerCallback()
{
    if (dragScrollDirection != 0)
    {
        auto* viewport = table.getViewport();
        viewport->setViewPosition(viewport->getViewPosition() + juce::Point<int>(0, dragScrollDirection * 14));
        if (lastDragTablePos.x >= 0)
            updateDropTarget(lastDragTablePos);
    }

    if (autoExpandCueId != 0
        && juce::Time::getMillisecondCounter() - autoExpandStartMs >= 700)
    {
        if (const auto index = findCueIndex(autoExpandCueId); index >= 0)
        {
            cues.getReference(index).collapsed = false;
            rebuildVisibleRows();
            if (lastDragTablePos.x >= 0)
                updateDropTarget(lastDragTablePos);
        }
        autoExpandCueId = 0;
    }

    if (dragScrollDirection == 0 && autoExpandCueId == 0)
        stopTimer();
}

bool CueListComponent::isInterestedInFileDrag(const juce::StringArray& files) { for (auto& f : files) if (isAudioFile(juce::File(f))) return editingEnabled; return false; }
void CueListComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    int parentId = 0, siblingIndex = 0, indicatorRow = -1, indicatorDepth = 0;
    DropZone zone = DropZone::none;
    computeDropTarget(table.getLocalPoint(this, juce::Point<int>(x, y)), parentId, siblingIndex, zone, indicatorRow, indicatorDepth);

    rememberUndo();
    auto lastId = 0;
    for (auto& f : files)
        if (isAudioFile(juce::File(f)))
            if (const auto id = createAudioCue(juce::File(f), parentId, siblingIndex); id != 0)
            {
                lastId = id;
                ++siblingIndex;
            }

    if (lastId == 0)
    {
        discardLastUndo();
        return;
    }
    rebuildVisibleRows(); normaliseStandby(); table.selectRow(findVisibleRow(lastId)); notifyContentChanged();
}

//==============================================================================
// Disclosure chevron hover feedback

void CueListComponent::mouseMove(const juce::MouseEvent& e)
{
    const auto pos = e.getEventRelativeTo(&table).getPosition();
    const auto row = table.getRowContainingPosition(pos.x, pos.y);

    auto newHover = -1;
    if (row >= 0)
    {
        const auto bounds = disclosureCellBounds(row);
        const auto rowRect = table.getRowPosition(row, true);
        const juce::Rectangle<float> tableBounds(bounds.getX() + (float) columnX(3),
                                                 bounds.getY() + (float) rowRect.getY(),
                                                 bounds.getWidth(), bounds.getHeight());
        if (tableBounds.contains(pos.toFloat()))
            newHover = row;
    }

    if (newHover != hoveredDisclosureRow)
    {
        const auto old = hoveredDisclosureRow;
        hoveredDisclosureRow = newHover;
        if (old >= 0) table.repaintRow(old);
        if (newHover >= 0) table.repaintRow(newHover);
    }
}

void CueListComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoveredDisclosureRow >= 0)
    {
        table.repaintRow(hoveredDisclosureRow);
        hoveredDisclosureRow = -1;
    }
}

void CueListComponent::showCueMenu(juce::Point<int> p)
{
    const auto roots = canonicalIds(getSelectedCueIds());

    auto anyGroup = false;
    auto singleGroup = roots.size() == 1;
    for (const auto id : roots)
        if (const auto index = findCueIndex(id); index >= 0 && cues[index].isGroup())
            anyGroup = true;
    singleGroup = singleGroup && anyGroup;

    juce::PopupMenu menu;
    menu.addItem("Preview", [this] { previewSelected(); });
    menu.addSeparator();
    menu.addItem("Group Selected Cues", editingEnabled && ! roots.isEmpty(), false, [this] { groupSelectedCue(); });
    menu.addItem("Ungroup", editingEnabled && anyGroup, false, [this] { ungroupSelectedCue(); });
    menu.addItem("Collapse / Expand Group", singleGroup, false, [this] { toggleSelectedGroup(); });
    menu.addItem("Expand All Groups", [this] { expandAllGroups(); });
    menu.addItem("Collapse All Groups", [this] { collapseAllGroups(); });
    menu.addSeparator();
    menu.addItem("Duplicate", editingEnabled, false, [this] { duplicateSelectedCue(); });
    menu.addItem("Move Up", editingEnabled, false, [this] { moveSelectedCue(-1); });
    menu.addItem("Move Down", editingEnabled, false, [this] { moveSelectedCue(1); });
    menu.addItem("Indent", editingEnabled, false, [this] { indentSelectedCue(); });
    menu.addItem("Outdent", editingEnabled, false, [this] { outdentSelectedCue(); });
    menu.addSeparator();
    menu.addItem("Delete", editingEnabled, false, [this] { deleteSelectedCue(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ p.x, p.y, 1, 1 }));
}
