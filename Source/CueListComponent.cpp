#include "CueListComponent.h"

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
}

CueListComponent::CueListComponent()
{
    addAndMakeVisible(table);
    table.setModel(this);
    table.setRowHeight(29);
    table.setMultipleSelectionEnabled(false);
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
void CueListComponent::undo()
{
    if (undoStack.empty()) return;
    cues = std::move(undoStack.back()); undoStack.pop_back(); rebuildVisibleRows(); normaliseStandby(); table.repaint(); notifyContentChanged();
}
void CueListComponent::addCueFromFile(const juce::File& file, int insert)
{
    if (! file.existsAsFile()) return;
    rememberUndo(); Cue cue; cue.id = nextCueId++; cue.number = juce::String(cues.size() + 1); cue.name = file.getFileNameWithoutExtension(); cue.file = file; cue.continueMode = defaultContinueMode;
    if (durationProvider) cue.durationSeconds = durationProvider(file);
    const auto selected = getSelectedCue(); cue.parentId = selected != nullptr && selected->isGroup() ? selected->id : (selected != nullptr ? selected->parentId : 0);
    if (insert < 0 || insert > cues.size()) cues.add(cue); else cues.insert(insert, cue);
    rebuildVisibleRows(); normaliseStandby(); table.selectRow(findVisibleRow(cue.id)); notifyContentChanged();
}
void CueListComponent::addGroupCue()
{
    rememberUndo(); Cue cue; cue.id = nextCueId++; cue.kind = Cue::Kind::group; cue.number = juce::String(cues.size() + 1); cue.name = "(Untitled Group Cue)";
    if (auto* selected = getSelectedCue()) cue.parentId = selected->parentId;
    cues.add(cue); rebuildVisibleRows(); normaliseStandby(); table.selectRow(findVisibleRow(cue.id)); notifyContentChanged();
}
void CueListComponent::addFadeCue()
{
    rememberUndo();
    Cue cue;
    cue.id = nextCueId++;
    cue.kind = Cue::Kind::fade;
    cue.number = juce::String(cues.size() + 1);
    cue.name = "(Untitled Fade Cue)";
    if (auto* selected = getSelectedCue()) cue.parentId = selected->parentId;
    cues.add(cue);
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
    const auto* selected = getSelectedCue();
    if (selected == nullptr)
        return;

    const auto ids = subtreeIds(selected->id);
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
    const auto* selected = getSelectedCue();
    if (selected == nullptr) return;

    const auto rootId = selected->id;
    const auto ids = subtreeIds(rootId);
    juce::Array<Cue> copies;
    for (const auto& cue : cues)
        if (ids.contains(cue.id))
            copies.add(cue);

    rememberUndo();
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

    auto insertAt = findCueIndex(rootId) + 1;
    while (insertAt < cues.size() && isDescendantOf(cues[insertAt].id, rootId))
        ++insertAt;
    for (const auto& copy : copies)
        cues.insert(insertAt++, copy);

    rebuildVisibleRows(); table.selectRow(findVisibleRow(idMap[rootId])); notifyContentChanged();
}
void CueListComponent::deleteSelectedCue()
{
    if (! editingEnabled) return; auto* selected = getSelectedCue(); if (selected == nullptr) return; rememberUndo(); const auto ids = subtreeIds(selected->id); cues.removeIf([&ids](const Cue& c) { return ids.contains(c.id); }); for (auto& cue : cues) if (cue.isFade()) cue.fadeActions.removeIf([&ids](const Cue::FadeAction& action) { return ids.contains(action.targetCueId); }); rebuildVisibleRows(); normaliseStandby(); if (! visibleRows.isEmpty()) table.selectRow(juce::jmin(table.getSelectedRow(), visibleRows.size() - 1)); notifyContentChanged();
}
void CueListComponent::copySelectedCue()
{
    if (auto* selected = getSelectedCue())
    {
        const auto ids = subtreeIds(selected->id);
        clipboard.clearQuick();
        for (const auto& cue : cues)
            if (ids.contains(cue.id))
                clipboard.add(cue);
    }
}
void CueListComponent::pasteCues()
{
    if (clipboard.isEmpty() || ! editingEnabled) return;
    const auto* selected = getSelectedCue();
    const auto destinationParent = selected == nullptr ? 0 : (selected->isGroup() ? selected->id : selected->parentId);
    const auto rootId = clipboard[0].id;
    rememberUndo();
    std::map<int, int> idMap;
    for (const auto& source : clipboard)
        idMap[source.id] = nextCueId++;
    for (auto copy : clipboard)
    {
        const auto oldId = copy.id;
        copy.id = idMap[oldId];
        copy.parentId = oldId == rootId ? destinationParent : idMap[copy.parentId];
        if (copy.isFade())
            for (auto& action : copy.fadeActions)
                if (const auto target = idMap.find(action.targetCueId); target != idMap.end())
                    action.targetCueId = target->second;
        copy.playCount = 0;
        cues.add(copy);
    }
    rebuildVisibleRows(); table.selectRow(findVisibleRow(idMap[rootId])); notifyContentChanged();
}
void CueListComponent::moveSelectedCue(int delta)
{
    if (! editingEnabled) return; const auto row = table.getSelectedRow(); if (! juce::isPositiveAndBelow(row, visibleRows.size())) return; const auto targetRow = juce::jlimit(0, visibleRows.size() - 1, row + delta); if (targetRow == row) return; rememberUndo(); auto from = visibleRows[row].cueIndex, to = visibleRows[targetRow].cueIndex; cues.move(from, to); rebuildVisibleRows(); table.selectRow(findVisibleRow(cues[to].id)); notifyContentChanged();
}
void CueListComponent::groupSelectedCue()
{
    if (! editingEnabled) return; auto* selected = getSelectedCue(); if (selected == nullptr) return; rememberUndo(); Cue group; group.id = nextCueId++; group.kind = Cue::Kind::group; group.number = selected->number; group.name = "(Untitled Group Cue)"; group.parentId = selected->parentId; const auto index = findCueIndex(selected->id); selected->parentId = group.id; cues.insert(index, group); rebuildVisibleRows(); table.selectRow(findVisibleRow(group.id)); notifyContentChanged();
}
void CueListComponent::toggleSelectedGroup() { if (auto* cue = getSelectedCue(); cue != nullptr && cue->isGroup()) { cue->collapsed = ! cue->collapsed; rebuildVisibleRows(); table.selectRow(findVisibleRow(cue->id)); notifyContentChanged(); } }
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

void CueListComponent::paintCell(juce::Graphics& g, int row, int column, int width, int height, bool)
{
    if (! juce::isPositiveAndBelow(row, visibleRows.size())) return; const auto& vr = visibleRows[row]; const auto& c = cues[vr.cueIndex];
    if (column == 1) { if (c.id == standbyCueId || (c.isGroup() && isDescendantOf(standbyCueId, c.id) && c.collapsed)) { juce::Path triangle; triangle.addTriangle(4.f, (float) height/2-5.f, 4.f, (float) height/2+5.f, 11.f, (float) height/2); g.setColour(juce::Colours::white); g.fillPath(triangle); } drawTypeIcon(g, c.isGroup(), c.isFade(), { 17.f, 7.f, 14.f, (float) height-14.f }, isCueRunning(c.id) ? (c.isGroup() ? groupColour(c) : Palette::standbyGreen) : Palette::textDim); return; }
    juce::String text; auto colour = Palette::textPrimary; auto just = juce::Justification::centredLeft;
    if (column == 2) { text = c.number; just = juce::Justification::centred; }
    if (column == 3)
    {
        const auto indent = 8 + vr.depth * 18;
        if (c.isGroup())
        {
            juce::Path disclosure;
            if (c.collapsed)
                disclosure.addTriangle(static_cast<float>(indent), height * .35f,
                                       static_cast<float>(indent), height * .65f,
                                       static_cast<float>(indent + 6), height * .5f);
            else
                disclosure.addTriangle(static_cast<float>(indent), height * .40f,
                                       static_cast<float>(indent + 8), height * .40f,
                                       static_cast<float>(indent + 4), height * .65f);
            g.setColour(Palette::textDim);
            g.fillPath(disclosure);
        }
        if (! c.armed) colour = Palette::textDim;
        g.setColour(colour);
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
        g.drawText(c.name, indent + (c.isGroup() ? 14 : 10), 0,
                   width - indent - 18, height, juce::Justification::centredLeft, true);
        paintGroupOutline(g, row, width, height);
        return;
    }
    if (column == 4) { text = c.isGroup() ? juce::String() : (c.isFade() ? juce::String(c.fadeActions.size()) + (c.fadeActions.size() == 1 ? " target" : " targets") : (c.target.isNotEmpty() ? c.target : c.file.getFileName())); colour = Palette::textDim; }
    if (column == 5) { text = formatTime(c.preWait); just = juce::Justification::centredRight; }
    if (column == 6) { text = formatTime(c.getEffectiveDuration()); just = juce::Justification::centredRight; }
    if (column == 7) { text = formatTime(c.postWait); just = juce::Justification::centredRight; }
    g.setColour(colour); g.setFont(juce::Font(juce::FontOptions().withHeight(13.f))); g.drawText(text, 6, 0, width - 12, height, just, true);
}
void CueListComponent::cellClicked(int row, int column, const juce::MouseEvent& e) { if (e.mods.isPopupMenu()) { table.selectRow(row); showCueMenu(e.getScreenPosition()); } else if (juce::isPositiveAndBelow(row, visibleRows.size()) && cues[visibleRows[row].cueIndex].isGroup() && ((column == 3 && e.x < 34 + visibleRows[row].depth * 18) || e.getNumberOfClicks() == 2)) { table.selectRow(row); toggleSelectedGroup(); } }
void CueListComponent::selectedRowsChanged(int) { if (standbyLinked) { if (auto* c = getSelectedCue()) standbyCueId = c->id; } table.repaint(); if (onSelectionChanged) onSelectionChanged(); notifyContentChanged(); }
juce::var CueListComponent::getDragSourceDescription(const juce::SparseSet<int>& rows) { return rows.isEmpty() || ! editingEnabled ? juce::var() : juce::var(rows[0]); }
bool CueListComponent::isInterestedInDragSource(const SourceDetails& d) { return editingEnabled && d.sourceComponent == &table; }
void CueListComponent::itemDropped(const SourceDetails& d) { const auto from = (int) d.description; auto local = table.getLocalPoint(this, d.localPosition); auto to = table.getRowContainingPosition(local.x, local.y); if (to < 0) to = visibleRows.size() - 1; if (from == to) return; table.selectRow(from); moveSelectedCue(to - from); }
bool CueListComponent::isInterestedInFileDrag(const juce::StringArray& files) { for (auto& f : files) if (isAudioFile(juce::File(f))) return editingEnabled; return false; }
void CueListComponent::filesDropped(const juce::StringArray& files, int, int) { for (auto& f : files) if (isAudioFile(juce::File(f))) addCueFromFile(juce::File(f)); }
void CueListComponent::showCueMenu(juce::Point<int> p)
{
    juce::PopupMenu menu; auto group = getSelectedCue() != nullptr && getSelectedCue()->isGroup();
    menu.addItem("Preview", [this] { previewSelected(); }); menu.addItem(group ? "Collapse / Expand Group" : "Group selected cue", editingEnabled, false, [this, group] { if (group) toggleSelectedGroup(); else groupSelectedCue(); }); menu.addSeparator();
    menu.addItem("Duplicate", editingEnabled, false, [this] { duplicateSelectedCue(); }); menu.addItem("Move Up", editingEnabled, false, [this] { moveSelectedCue(-1); }); menu.addItem("Move Down", editingEnabled, false, [this] { moveSelectedCue(1); }); menu.addSeparator(); menu.addItem("Delete", editingEnabled, false, [this] { deleteSelectedCue(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ p.x, p.y, 1, 1 }));
}
