#pragma once

#include <juce_core/juce_core.h>
#include "CueListComponent.h"

struct WorkspaceData
{
    juce::Array<Cue> cues;
    int standbyCueId = 0;
    float masterGain = 0.8f;
    juce::String notes;
};

class WorkspaceFile
{
public:
    static bool save(const juce::File& targetFile, const WorkspaceData& data);
    static bool load(const juce::File& sourceFile, WorkspaceData& data,
                     const juce::File& extractFolder);
};
