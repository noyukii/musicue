#include "WorkspaceFile.h"

namespace
{
class FadeCueModelTest final : public juce::UnitTest
{
public:
    FadeCueModelTest() : juce::UnitTest("Fade cue model", "MusiCue") {}

    void runTest() override
    {
        beginTest("Uses latest action endpoint as cue duration");
        Cue cue;
        cue.kind = Cue::Kind::fade;
        cue.fadeActions.add({ 1, 0.25, 1.5 });
        cue.fadeActions.add({ 2, 2.0, 0.75 });
        expectWithinAbsoluteError(cue.getEffectiveDuration(), 2.75, 0.0001);
    }
};

class FadeCueWorkspaceTest final : public juce::UnitTest
{
public:
    FadeCueWorkspaceTest() : juce::UnitTest("Fade cue workspace", "MusiCue") {}

    void runTest() override
    {
        beginTest("Round trips actions and stop policy");
        const auto root = juce::File("/private/tmp")
                              .getChildFile("MusiCueTests_" + juce::Uuid().toString());
        const auto workspace = root.getChildFile("fade.musicue");
        const auto extracted = root.getChildFile("extracted");
        root.createDirectory();

        WorkspaceData data;
        Cue fade;
        fade.id = 11;
        fade.kind = Cue::Kind::fade;
        fade.name = "Crossfade";
        fade.fadeStopPolicy = Cue::FadeStopPolicy::stopTargets;
        Cue::FadeAction action;
        action.targetCueId = 7;
        action.delaySeconds = 0.25;
        action.durationSeconds = 1.5;
        action.fadeGain = true;
        action.targetGainDb = -12.0;
        action.fadePan = true;
        action.targetPan = 0.4;
        action.startIfStopped = true;
        action.startGainDb = -60.0;
        action.stopAtEnd = true;
        action.curve = Cue::FadeCurve::sCurve;
        fade.fadeActions.add(action);
        data.cues.add(fade);

        expect(WorkspaceFile::save(workspace, data));
        WorkspaceData loaded;
        expect(WorkspaceFile::load(workspace, loaded, extracted));
        expectEquals(loaded.cues.size(), 1);
        if (! loaded.cues.isEmpty())
        {
            const auto& loadedFade = loaded.cues[0];
            expect(loadedFade.isFade());
            expectEquals(static_cast<int>(loadedFade.fadeStopPolicy),
                         static_cast<int>(Cue::FadeStopPolicy::stopTargets));
            expectEquals(loadedFade.fadeActions.size(), 1);
            if (! loadedFade.fadeActions.isEmpty())
            {
                const auto& loadedAction = loadedFade.fadeActions[0];
                expectEquals(loadedAction.targetCueId, 7);
                expectWithinAbsoluteError(loadedAction.durationSeconds, 1.5, 0.0001);
                expect(loadedAction.fadePan);
                expect(loadedAction.startIfStopped);
                expect(loadedAction.stopAtEnd);
                expectEquals(static_cast<int>(loadedAction.curve), static_cast<int>(Cue::FadeCurve::sCurve));
            }
        }

        root.deleteRecursively();
    }
};

FadeCueModelTest fadeCueModelTest;
FadeCueWorkspaceTest fadeCueWorkspaceTest;
}

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();
    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (const auto* result = runner.getResult(i))
            failures += result->failures;
    return failures == 0 ? 0 : 1;
}
