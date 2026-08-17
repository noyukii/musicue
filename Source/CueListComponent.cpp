#include "CueListComponent.h"

namespace
{
void drawTypeIcon(juce::Graphics& g, bool group, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour(colour);
    if (group)
    {
        g.drawRoundedRectangle(area.reduced(1.5f), 2.0f, 1.5f);
        g.drawLine(area.getX() + 3.0f, area.getCentreY(), area.getRight() - 3.0f, area.getCentreY(), 1.5f);
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
void CueListComponent::appendVisibleChildren(int parentId, int depth)
{
    for (int i = 0; i < cues.size(); ++i) if (cues[i].parentId == parentId)
    {
        visibleRows.add({ i, depth });
        if (cues[i].isGroup() && ! cues[i].collapsed) appendVisibleChildren(cues[i].id, depth + 1);
    }
}
void CueListComponent::rebuildVisibleRows() { visibleRows.clearQuick(); appendVisibleChildren(0, 0); table.updateContent(); }
void CueListComponent::normaliseStandby()
{
    if (findCueIndex(standbyCueId) < 0 || (findCueIndex(standbyCueId) >= 0 && cues[findCueIndex(standbyCueId)].parentId != 0))
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
    rememberUndo(); Cue cue; cue.id = nextCueId++; cue.kind = Cue::Kind::group; cue.number = juce::String(cues.size() + 1); cue.name = "New Group";
    if (auto* selected = getSelectedCue()) cue.parentId = selected->parentId;
    cues.add(cue); rebuildVisibleRows(); normaliseStandby(); table.selectRow(findVisibleRow(cue.id)); notifyContentChanged();
}
void CueListComponent::setCues(juce::Array<Cue> source, int standbyId)
{
    cues = std::move(source); nextCueId = 1;
    for (auto& cue : cues) { cue.id = cue.id > 0 ? cue.id : nextCueId; nextCueId = juce::jmax(nextCueId, cue.id + 1); cue.playCount = 0; if (cue.isAudio() && durationProvider && cue.file.existsAsFile()) cue.durationSeconds = durationProvider(cue.file); }
    standbyCueId = standbyId; undoStack.clear(); rebuildVisibleRows(); normaliseStandby();
    if (! visibleRows.isEmpty()) table.selectRow(0); else if (onSelectionChanged) onSelectionChanged(); notifyContentChanged();
}
void CueListComponent::runGroup(int groupId)
{
    for (auto& cue : cues) if (cue.parentId == groupId)
    {
        if (cue.isGroup()) runGroup(cue.id);
        else if (cue.armed || ! cue.skipIfDisarmed) { ++cue.playCount; if (onPlayCue) onPlayCue(cue); }
    }
}
void CueListComponent::advanceStandby(const Cue& cue)
{
    const auto row = findVisibleRow(standbyCueId);
    if (row < 0)
        return;

    auto next = row + 1;
    if (cue.isGroup())
        while (next < visibleRows.size() && visibleRows[next].depth > visibleRows[row].depth)
            ++next;

    if (next < visibleRows.size())
        standbyCueId = cues[visibleRows[next].cueIndex].id;
}
void CueListComponent::runCue(int index, bool advance)
{
    if (! juce::isPositiveAndBelow(index, cues.size())) return;
    auto& cue = cues.getReference(index);
    if (cue.isGroup()) runGroup(cue.id);
    else if (cue.armed || ! cue.skipIfDisarmed) { ++cue.playCount; if (onPlayCue) onPlayCue(cue); }
    if (advance) advanceStandby(cue); table.repaint(); notifyContentChanged();
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
        runCue(index, true);
        started = true;
        if (! autoFollow)
            break;
    }

    if (standbyLinked)
        table.selectRow(findVisibleRow(standbyCueId));
    return started;
}
void CueListComponent::previewSelected() { if (auto* c = getSelectedCue()) runCue(findCueIndex(c->id), false); }
void CueListComponent::resetStandby() { standbyCueId = visibleRows.isEmpty() ? 0 : cues[visibleRows[0].cueIndex].id; if (standbyLinked) table.selectRow(0); table.repaint(); notifyContentChanged(); }
void CueListComponent::duplicateSelectedCue() { if (! editingEnabled) return; if (auto* selected = getSelectedCue()) { rememberUndo(); auto copy = *selected; copy.id = nextCueId++; copy.name += " copy"; copy.playCount = 0; cues.insert(findCueIndex(selected->id) + 1, copy); rebuildVisibleRows(); table.selectRow(findVisibleRow(copy.id)); notifyContentChanged(); } }
void CueListComponent::deleteSelectedCue()
{
    if (! editingEnabled) return; auto* selected = getSelectedCue(); if (selected == nullptr) return; rememberUndo(); const int id = selected->id; cues.removeIf([id](const Cue& c) { return c.id == id || c.parentId == id; }); rebuildVisibleRows(); normaliseStandby(); if (! visibleRows.isEmpty()) table.selectRow(juce::jmin(table.getSelectedRow(), visibleRows.size() - 1)); notifyContentChanged();
}
void CueListComponent::copySelectedCue() { if (auto* selected = getSelectedCue()) { clipboard.clearQuick(); clipboard.add(*selected); } }
void CueListComponent::pasteCues() { if (clipboard.isEmpty() || ! editingEnabled) return; rememberUndo(); for (auto copy : clipboard) { copy.id = nextCueId++; copy.playCount = 0; cues.add(copy); } rebuildVisibleRows(); notifyContentChanged(); }
void CueListComponent::moveSelectedCue(int delta)
{
    if (! editingEnabled) return; const auto row = table.getSelectedRow(); if (! juce::isPositiveAndBelow(row, visibleRows.size())) return; const auto targetRow = juce::jlimit(0, visibleRows.size() - 1, row + delta); if (targetRow == row) return; rememberUndo(); auto from = visibleRows[row].cueIndex, to = visibleRows[targetRow].cueIndex; cues.move(from, to); rebuildVisibleRows(); table.selectRow(findVisibleRow(cues[to].id)); notifyContentChanged();
}
void CueListComponent::groupSelectedCue()
{
    if (! editingEnabled) return; auto* selected = getSelectedCue(); if (selected == nullptr) return; rememberUndo(); Cue group; group.id = nextCueId++; group.kind = Cue::Kind::group; group.number = selected->number; group.name = "Group - " + selected->name; group.parentId = selected->parentId; const auto index = findCueIndex(selected->id); selected->parentId = group.id; cues.insert(index, group); rebuildVisibleRows(); table.selectRow(findVisibleRow(group.id)); notifyContentChanged();
}
void CueListComponent::toggleSelectedGroup() { if (auto* cue = getSelectedCue(); cue != nullptr && cue->isGroup()) { cue->collapsed = ! cue->collapsed; rebuildVisibleRows(); table.selectRow(findVisibleRow(cue->id)); notifyContentChanged(); } }
void CueListComponent::markCueFinished(int id) { if (auto i = findCueIndex(id); i >= 0) cues.getReference(i).playCount = juce::jmax(0, cues[i].playCount - 1); table.repaint(); notifyContentChanged(); }
void CueListComponent::markAllIdle() { for (auto& c : cues) c.playCount = 0; table.repaint(); notifyContentChanged(); }
void CueListComponent::refreshDisplay() { rebuildVisibleRows(); table.repaint(); }
int CueListComponent::nextSiblingId(const Cue& cue) const { bool seen = false; for (auto& other : cues) { if (other.parentId != cue.parentId) continue; if (seen) return other.id; if (other.id == cue.id) seen = true; } return 0; }
void CueListComponent::handleAutoContinue(int id) { const auto i = findCueIndex(id); if (i < 0 || cues[i].continueMode != 1) return; const auto next = nextSiblingId(cues[i]); const auto delay = (int) juce::jmax(0.0, cues[i].postWait * 1000.0); if (next != 0) juce::Timer::callAfterDelay(delay, [this, next] { runCue(findCueIndex(next), false); }); }
bool CueListComponent::triggerHotkey(const juce::KeyPress& key) { for (int i = 0; i < cues.size(); ++i) if (cues[i].hotkey == key.getTextDescription()) { runCue(i, false); return true; } return false; }
Cue* CueListComponent::getSelectedCue() { const auto row = table.getSelectedRow(); return juce::isPositiveAndBelow(row, visibleRows.size()) ? &cues.getReference(visibleRows[row].cueIndex) : nullptr; }
const Cue* CueListComponent::getStandbyCue() const { const auto i = findCueIndex(standbyCueId); return i >= 0 ? &cues.getReference(i) : nullptr; }
bool CueListComponent::hasStandby() const { return getStandbyCue() != nullptr; }
void CueListComponent::setEditingEnabled(bool enabled) { editingEnabled = enabled; }
void CueListComponent::notifyContentChanged() { if (onContentChanged) onContentChanged(); }
int CueListComponent::getNumRows() { return visibleRows.size(); }
void CueListComponent::paintRowBackground(juce::Graphics& g, int row, int, int, bool selected) { g.fillAll(selected ? Palette::selection : (row % 2 == 0 ? Palette::rowA : Palette::rowB)); }
void CueListComponent::paintCell(juce::Graphics& g, int row, int column, int width, int height, bool)
{
    if (! juce::isPositiveAndBelow(row, visibleRows.size())) return; const auto& vr = visibleRows[row]; const auto& c = cues[vr.cueIndex];
    if (column == 1) { if (c.id == standbyCueId) { juce::Path triangle; triangle.addTriangle(4.f, (float) height/2-5.f, 4.f, (float) height/2+5.f, 11.f, (float) height/2); g.setColour(juce::Colours::white); g.fillPath(triangle); } drawTypeIcon(g, c.isGroup(), { 17.f, 7.f, 14.f, (float) height-14.f }, c.playCount ? Palette::standbyGreen : Palette::textDim); return; }
    juce::String text; auto colour = Palette::textPrimary; auto just = juce::Justification::centredLeft;
    if (column == 2) { text = c.number; just = juce::Justification::centred; }
    if (column == 3) { text = (c.isGroup() ? (c.collapsed ? "> " : "v ") : "") + c.name; text = juce::String::repeatedString("    ", vr.depth) + text; if (! c.armed) colour = Palette::textDim; }
    if (column == 4) { text = c.isGroup() ? "Container" : (c.target.isNotEmpty() ? c.target : c.file.getFileName()); colour = Palette::textDim; }
    if (column == 5) { text = formatTime(c.preWait); just = juce::Justification::centredRight; }
    if (column == 6) { text = c.isGroup() ? "-" : formatTime(c.getEffectiveDuration()); just = juce::Justification::centredRight; }
    if (column == 7) { text = formatTime(c.postWait); just = juce::Justification::centredRight; }
    g.setColour(colour); g.setFont(juce::Font(juce::FontOptions().withHeight(13.f))); g.drawText(text, 6, 0, width - 12, height, just, true);
}
void CueListComponent::cellClicked(int row, int, const juce::MouseEvent& e) { if (e.mods.isPopupMenu()) { table.selectRow(row); showCueMenu(e.getScreenPosition()); } else if (e.getNumberOfClicks() == 2 && juce::isPositiveAndBelow(row, visibleRows.size()) && cues[visibleRows[row].cueIndex].isGroup()) toggleSelectedGroup(); }
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
