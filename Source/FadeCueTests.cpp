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

        beginTest("Builds a crossfade action pair");
        const auto pair = Cue::makeCrossfadeActions(3, 7, 2.5, Cue::FadeCurve::sCurve);
        expectEquals(pair.size(), 2);
        expectEquals(pair[0].targetCueId, 3);
        expect(pair[0].fadeGain);
        expectWithinAbsoluteError(pair[0].targetGainDb, -60.0, 0.0001);
        expect(pair[0].stopAtEnd);
        expect(! pair[0].startIfStopped);
        expectEquals(pair[1].targetCueId, 7);
        expectWithinAbsoluteError(pair[1].targetGainDb, 0.0, 0.0001);
        expect(pair[1].startIfStopped);
        expectWithinAbsoluteError(pair[1].startGainDb, -60.0, 0.0001);
        expect(! pair[1].stopAtEnd);
        for (const auto& action : pair)
        {
            expectWithinAbsoluteError(action.durationSeconds, 2.5, 0.0001);
            expectEquals(static_cast<int>(action.curve), static_cast<int>(Cue::FadeCurve::sCurve));
        }

        beginTest("Builds fade-in only when nothing is playing");
        const auto fadeInOnly = Cue::makeCrossfadeActions(0, 7);
        expectEquals(fadeInOnly.size(), 1);
        expectEquals(fadeInOnly[0].targetCueId, 7);
        expect(fadeInOnly[0].startIfStopped);
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
