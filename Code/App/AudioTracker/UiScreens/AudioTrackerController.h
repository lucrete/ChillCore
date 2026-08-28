#ifndef AUDIOTRACKERCONTROLLER_H
#define AUDIOTRACKERCONTROLLER_H

#include <cstddef>

#include "UiScreenController.h"

namespace CC
{
    class UiElement;
    struct TrackerProject;
}

class AudioTrackerController : public CC::UiScreenController
{
public:
    explicit AudioTrackerController(CC::TrackerProject* project);
    ~AudioTrackerController() override;

    void Init() override;
    void OnUpdate() override;

private:
    void RebuildSampleList();
    void UpdateMemoryLabel();
    void RebuildPatternEditor();
    void RebuildPatternTabs();
    void RebuildSongView();

    CC::TrackerProject* project;

    CC::UiElement* prototypeLoopButton;
    CC::UiElement* samplesListContainer;
    CC::UiElement* samplesMemoryLabel;
    CC::UiElement* patternGridContainer;
    CC::UiElement* patternTabsContainer;
    CC::UiElement* songGridContainer;
    CC::UiElement* bpmValueLabel;
    CC::UiElement* tsNumValueLabel;
    CC::UiElement* tsDenValueLabel;
    CC::UiElement* lengthValueLabel;
    bool           isPrototypeLoopPlaying;

    // True between Audition and Stop clicks. Drives the live-edit
    // republish: when the project version changes while this is true,
    // OnUpdate calls AuditionPattern again so the running loop reflects
    // the edit on its next iteration.
    bool           isAuditionActive;

    // Same live-republish role for full-song playback. Mutually
    // exclusive with isAuditionActive and isPrototypeLoopPlaying —
    // each transport entry-point clears the others.
    bool           isSongPlaying;

    int            lastLibraryVersion;
    int            lastProjectVersion;
};

#endif // AUDIOTRACKERCONTROLLER_H
