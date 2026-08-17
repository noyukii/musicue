#include "WorkspaceFile.h"

bool WorkspaceFile::save(const juce::File& targetFile, const WorkspaceData& data)
{
    auto xml = std::make_unique<juce::XmlElement>("musicue-workspace");
    xml->setAttribute("version", 2);
    xml->setAttribute("masterGain", static_cast<double>(data.masterGain));
    xml->setAttribute("standbyCueId", data.standbyCueId);
    xml->setAttribute("notes", data.notes);

    auto* cuesXml = xml->createNewChildElement("cues");

    juce::ZipFile::Builder builder;
    juce::StringPairArray pathToEntry;
    int fileIndex = 0;

    for (const auto& cue : data.cues)
    {
        auto* cueXml = cuesXml->createNewChildElement("cue");
        cueXml->setAttribute("id", cue.id);
        cueXml->setAttribute("parentId", cue.parentId);
        cueXml->setAttribute("kind", cue.isGroup() ? "group" : "audio");
        cueXml->setAttribute("number", cue.number);
        cueXml->setAttribute("name", cue.name);
        cueXml->setAttribute("duration", cue.durationSeconds);
        cueXml->setAttribute("preWait", cue.preWait);
        cueXml->setAttribute("postWait", cue.postWait);
        cueXml->setAttribute("continueMode", cue.continueMode);
        cueXml->setAttribute("armed", cue.armed ? 1 : 0);
        cueXml->setAttribute("flagged", cue.flagged ? 1 : 0);
        cueXml->setAttribute("autoLoad", cue.autoLoad ? 1 : 0);
        cueXml->setAttribute("skipIfDisarmed", cue.skipIfDisarmed ? 1 : 0);
        cueXml->setAttribute("notes", cue.notes);
        cueXml->setAttribute("gainDb", cue.gainDb);
        cueXml->setAttribute("pan", cue.pan);
        cueXml->setAttribute("trimStart", cue.trimStart);
        cueXml->setAttribute("trimEnd", cue.trimEnd);
        cueXml->setAttribute("fadeIn", cue.fadeIn);
        cueXml->setAttribute("fadeOut", cue.fadeOut);
        cueXml->setAttribute("loop", cue.loop ? 1 : 0);
        cueXml->setAttribute("hotkey", cue.hotkey);
        cueXml->setAttribute("collapsed", cue.collapsed ? 1 : 0);
        cueXml->setAttribute("target", cue.target);

        if (cue.isAudio() && cue.file.existsAsFile())
        {
            const auto fullPath = cue.file.getFullPathName();
            auto entryName = pathToEntry.getValue(fullPath, {});

            if (entryName.isEmpty())
            {
                entryName = "audio/" + juce::String(fileIndex++) + "_" + cue.file.getFileName();
                builder.addFile(cue.file, 6, entryName);
                pathToEntry.set(fullPath, entryName);
            }

            cueXml->setAttribute("audioEntry", entryName);
            cueXml->setAttribute("originalPath", fullPath);
        }
    }

    const auto tempXml = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("musicue_" + juce::Uuid().toString() + ".xml");
    tempXml.replaceWithText(xml->toString());
    builder.addFile(tempXml, 6, "workspace.xml");

    // Build beside destination, then replace. Existing workspace survives failed writes.
    const auto temporary = targetFile.getSiblingFile(targetFile.getFileName() + ".tmp");
    temporary.deleteFile();
    juce::FileOutputStream output(temporary);

    if (! output.openedOk())
    {
        tempXml.deleteFile();
        return false;
    }

    output.setPosition(0);
    output.truncate();
    const auto ok = builder.writeToStream(output, nullptr);
    tempXml.deleteFile();

    if (! ok)
    {
        temporary.deleteFile();
        return false;
    }

    return temporary.replaceFileIn(targetFile);
}

bool WorkspaceFile::load(const juce::File& sourceFile, WorkspaceData& data,
                         const juce::File& extractFolder)
{
    juce::ZipFile zip(sourceFile);

    const auto xmlIndex = zip.getIndexOfFileName("workspace.xml");
    if (xmlIndex < 0)
        return false;

    const auto* xmlEntry = zip.getEntry(xmlIndex);
    if (xmlEntry == nullptr)
        return false;

    std::unique_ptr<juce::InputStream> input(zip.createStreamForEntry(*xmlEntry));
    if (input == nullptr)
        return false;

    auto xml = juce::XmlDocument::parse(input->readEntireStreamAsString());
    if (xml == nullptr || xml->getTagName() != "musicue-workspace")
        return false;

    data.masterGain = static_cast<float>(xml->getDoubleAttribute("masterGain", 0.8));
    if (xml->getIntAttribute("version", 0) < 2)
        return false;

    data.standbyCueId = xml->getIntAttribute("standbyCueId", 0);
    data.notes = xml->getStringAttribute("notes");

    extractFolder.deleteRecursively();
    extractFolder.createDirectory();

    data.cues.clear();

    if (auto* cuesXml = xml->getChildByName("cues"))
    {
        for (auto* cueXml : cuesXml->getChildIterator())
        {
            Cue cue;
            cue.id = cueXml->getIntAttribute("id");
            cue.parentId = cueXml->getIntAttribute("parentId", 0);
            cue.kind = cueXml->getStringAttribute("kind") == "group"
                           ? Cue::Kind::group : Cue::Kind::audio;
            cue.number = cueXml->getStringAttribute("number");
            cue.name = cueXml->getStringAttribute("name");
            cue.durationSeconds = cueXml->getDoubleAttribute("duration", 0.0);
            cue.preWait = cueXml->getDoubleAttribute("preWait", 0.0);
            cue.postWait = cueXml->getDoubleAttribute("postWait", 0.0);
            cue.continueMode = cueXml->getIntAttribute("continueMode", 0);
            cue.armed = cueXml->getIntAttribute("armed", 1) != 0;
            cue.flagged = cueXml->getIntAttribute("flagged", 0) != 0;
            cue.autoLoad = cueXml->getIntAttribute("autoLoad", 0) != 0;
            cue.skipIfDisarmed = cueXml->getIntAttribute("skipIfDisarmed", 0) != 0;
            cue.notes = cueXml->getStringAttribute("notes");
            cue.gainDb = cueXml->getDoubleAttribute("gainDb", 0.0);
            cue.pan = cueXml->getDoubleAttribute("pan", 0.0);
            cue.trimStart = cueXml->getDoubleAttribute("trimStart", 0.0);
            cue.trimEnd = cueXml->getDoubleAttribute("trimEnd", 0.0);
            cue.fadeIn = cueXml->getDoubleAttribute("fadeIn", 0.0);
            cue.fadeOut = cueXml->getDoubleAttribute("fadeOut", 0.0);
            cue.loop = cueXml->getIntAttribute("loop", 0) != 0;
            cue.hotkey = cueXml->getStringAttribute("hotkey");
            cue.collapsed = cueXml->getIntAttribute("collapsed", 0) != 0;
            cue.target = cueXml->getStringAttribute("target", "Main Output");

            const auto entryName = cueXml->getStringAttribute("audioEntry");

            if (cue.isAudio() && entryName.isNotEmpty())
            {
                const auto audioIndex = zip.getIndexOfFileName(entryName);

                if (audioIndex >= 0
                    && zip.uncompressEntry(audioIndex, extractFolder, true).wasOk())
                    cue.file = extractFolder.getChildFile(entryName);
            }

            if (! cue.file.existsAsFile())
            {
                const auto original = cueXml->getStringAttribute("originalPath");

                if (original.isNotEmpty() && juce::File(original).existsAsFile())
                    cue.file = juce::File(original);
            }

            data.cues.add(cue);
        }
    }

    return true;
}
