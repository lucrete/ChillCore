#include "AudioTrackerController.h"

#include <cstdio>

#include "UiManager.h"
#include "UiElement.h"
#include "UiPanel.h"
#include "UiText.h"
#include "UiButton.h"
#include "PrintManager.h"
#include "StateMachine.h"
#include "TrackerEngine.h"
#include "SampleLibrary.h"
#include "TrackerProject.h"
#include "TrackerCommandHistory.h"
#include "Commands/AddPatternTrackCommand.h"
#include "Commands/ToggleStepCommand.h"
#include "Commands/AssignSourceToPatternTrackCommand.h"
#include "Commands/RemovePatternTrackCommand.h"
#include "Commands/AddPatternCommand.h"
#include "Commands/RemovePatternCommand.h"
#include "Commands/AddSongTrackCommand.h"
#include "Commands/PlacePatternInSongCommand.h"
#include "Commands/TogglePatternTrackMutedCommand.h"
#include "Commands/ToggleSongTrackMutedCommand.h"
#include "Commands/SetBpmCommand.h"
#include "Commands/SetTimeSignatureCommand.h"
#include "Commands/SetSongLengthCommand.h"
#include "TrackerProjectFile.h"

namespace
{
    const char* SampleStatusClass(CC::SampleStatus status)
    {
        const char* result = "sample-status-ok";
        switch (status)
        {
            case CC::SampleStatus::Ok:       result = "sample-status-ok";       break;
            case CC::SampleStatus::Modified: result = "sample-status-modified"; break;
            case CC::SampleStatus::Broken:   result = "sample-status-broken";   break;
            case CC::SampleStatus::Orphan:   result = "sample-status-orphan";   break;
        }
        return result;
    }

    const char* SampleStatusLabel(CC::SampleStatus status)
    {
        const char* result = "OK";
        switch (status)
        {
            case CC::SampleStatus::Ok:       result = "OK";       break;
            case CC::SampleStatus::Modified: result = "MODIFIED"; break;
            case CC::SampleStatus::Broken:   result = "BROKEN";   break;
            case CC::SampleStatus::Orphan:   result = "ORPHAN";   break;
        }
        return result;
    }
}

AudioTrackerController::AudioTrackerController(CC::TrackerProject* _project)
    : project(_project)
    , prototypeLoopButton(nullptr)
    , samplesListContainer(nullptr)
    , samplesMemoryLabel(nullptr)
    , patternGridContainer(nullptr)
    , patternTabsContainer(nullptr)
    , songGridContainer(nullptr)
    , bpmValueLabel(nullptr)
    , tsNumValueLabel(nullptr)
    , tsDenValueLabel(nullptr)
    , lengthValueLabel(nullptr)
    , isPrototypeLoopPlaying(false)
    , isAuditionActive(false)
    , isSongPlaying(false)
    , lastLibraryVersion(-1)
    , lastProjectVersion(-1)
{
}

AudioTrackerController::~AudioTrackerController()
{
}

void AudioTrackerController::Init()
{
    CC::UiManager* ui = CC::UiManager::Get();

    prototypeLoopButton  = ui->GetElementById("prototypeLoopButton");
    samplesListContainer = ui->GetElementById("samplesList");
    samplesMemoryLabel   = ui->GetElementById("samplesMemoryLabel");
    patternGridContainer = ui->GetElementById("patternGrid");
    patternTabsContainer = ui->GetElementById("patternTabs");
    songGridContainer    = ui->GetElementById("songGrid");
    bpmValueLabel        = ui->GetElementById("bpmValue");
    tsNumValueLabel      = ui->GetElementById("tsNumValue");
    tsDenValueLabel      = ui->GetElementById("tsDenValue");
    lengthValueLabel     = ui->GetElementById("lengthValue");

    auto applyBpmDelta = [this](int delta)
    {
        if (project != nullptr)
        {
            CC::TrackerCommandHistory::Get()->Apply(
                std::unique_ptr<CC::TrackerCommand>(new CC::SetBpmCommand(project->bpm + delta)),
                *project);
        }
    };
    ui->RegisterButtonAction("bpmDown", [applyBpmDelta]() { applyBpmDelta(-1); });
    ui->RegisterButtonAction("bpmUp",   [applyBpmDelta]() { applyBpmDelta(+1); });

    auto applyTimeSignatureDelta = [this](int numDelta, int denDelta)
    {
        if (project != nullptr)
        {
            int newNum = project->timeSignatureNumerator   + numDelta;
            int newDen = project->timeSignatureDenominator + denDelta;
            CC::TrackerCommandHistory::Get()->Apply(
                std::unique_ptr<CC::TrackerCommand>(new CC::SetTimeSignatureCommand(newNum, newDen)),
                *project);
        }
    };
    ui->RegisterButtonAction("tsNumDown", [applyTimeSignatureDelta]() { applyTimeSignatureDelta(-1, 0); });
    ui->RegisterButtonAction("tsNumUp",   [applyTimeSignatureDelta]() { applyTimeSignatureDelta(+1, 0); });
    ui->RegisterButtonAction("tsDenDown", [applyTimeSignatureDelta]() { applyTimeSignatureDelta(0, -1); });
    ui->RegisterButtonAction("tsDenUp",   [applyTimeSignatureDelta]() { applyTimeSignatureDelta(0, +1); });

    auto applyLengthDelta = [this](int delta)
    {
        if (project != nullptr)
        {
            CC::TrackerCommandHistory::Get()->Apply(
                std::unique_ptr<CC::TrackerCommand>(new CC::SetSongLengthCommand(project->songLengthBars + delta)),
                *project);
        }
    };
    ui->RegisterButtonAction("lengthDown", [applyLengthDelta]() { applyLengthDelta(-1); });
    ui->RegisterButtonAction("lengthUp",   [applyLengthDelta]() { applyLengthDelta(+1); });

    ui->RegisterButtonAction("addSongTrack", [this]()
    {
        if (project != nullptr)
        {
            CC::TrackerCommandHistory::Get()->Apply(
                std::unique_ptr<CC::TrackerCommand>(new CC::AddSongTrackCommand()),
                *project);
        }
    });

    ui->RegisterButtonAction("playSong", [this]()
    {
        if (project != nullptr)
        {
            CC::TrackerEngine::Get()->PlaySong(*project);
            isSongPlaying          = true;
            isAuditionActive       = false;
            isPrototypeLoopPlaying = false;
        }
    });

    ui->RegisterButtonAction("stopSong", [this]()
    {
        CC::TrackerEngine::Get()->Stop();
        isSongPlaying          = false;
        isAuditionActive       = false;
        isPrototypeLoopPlaying = false;
    });

    ui->RegisterButtonAction("addPattern", [this]()
    {
        if (project != nullptr)
        {
            CC::TrackerCommandHistory::Get()->Apply(
                std::unique_ptr<CC::TrackerCommand>(new CC::AddPatternCommand()),
                *project);
            // Auto-select the newly-added pattern so the editor jumps
            // to it. Selection is UI state, not undoable, so direct
            // mutation + version bump is the right shape.
            project->currentPatternIndex = (int)project->patterns.size() - 1;
            project->version++;
        }
    });

    ui->RegisterButtonAction("removeCurrentPattern", [this]()
    {
        if (project != nullptr
            && project->currentPatternIndex >= 0
            && (size_t)project->currentPatternIndex < project->patterns.size())
        {
            const std::string& patternId = project->patterns[(size_t)project->currentPatternIndex].id;
            CC::TrackerCommandHistory::Get()->Apply(
                std::unique_ptr<CC::TrackerCommand>(new CC::RemovePatternCommand(patternId)),
                *project);
        }
    });

    ui->RegisterButtonAction("addPatternTrack", [this]()
    {
        if (project != nullptr && !project->patterns.empty())
        {
            const std::string& patternId = project->patterns[(size_t)project->currentPatternIndex].id;
            CC::TrackerCommandHistory::Get()->Apply(
                std::unique_ptr<CC::TrackerCommand>(new CC::AddPatternTrackCommand(patternId)),
                *project);
        }
    });

    ui->RegisterButtonAction("auditionPattern", [this]()
    {
        if (project != nullptr
            && project->currentPatternIndex >= 0
            && (size_t)project->currentPatternIndex < project->patterns.size())
        {
            CC::TrackerEngine::Get()->AuditionPattern(project->patterns[(size_t)project->currentPatternIndex]);
            isAuditionActive       = true;
            isPrototypeLoopPlaying = false;
            isSongPlaying          = false;
        }
    });

    ui->RegisterButtonAction("stopAudition", [this]()
    {
        CC::TrackerEngine::Get()->Stop();
        isAuditionActive       = false;
        isPrototypeLoopPlaying = false;
        isSongPlaying          = false;
    });

    ui->RegisterButtonAction("backToMainMenu", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AudioTracker: backToMainMenu");
        CC::StateMachine::Get()->GotoState("Boot");
    });

    ui->RegisterButtonAction("saveProject", [this]()
    {
        if (project != nullptr)
        {
            CC::TrackerProjectFile::Save(*project, CC::TrackerProjectFile::GetDefaultProjectPath());
        }
    });

    ui->RegisterButtonAction("loadProject", [this]()
    {
        if (project != nullptr)
        {
            // Stop any active transport so audio doesn't reach into the
            // project mid-mutation as the model gets replaced.
            CC::TrackerEngine::Get()->Stop();
            isAuditionActive       = false;
            isSongPlaying          = false;
            isPrototypeLoopPlaying = false;

            if (CC::TrackerProjectFile::Load(CC::TrackerProjectFile::GetDefaultProjectPath(), *project))
            {
                // Undo never crosses a project boundary. The version
                // bump triggers panel + grid + tabs rebuild on the next
                // OnUpdate.
                CC::TrackerCommandHistory::Get()->Clear();
                project->version++;
            }
        }
    });

    ui->RegisterButtonAction("togglePrototypeLoop", [this]()
    {
        CC::TrackerEngine* trackerEngine = CC::TrackerEngine::Get();
        if (isPrototypeLoopPlaying)
        {
            trackerEngine->Stop();
            isPrototypeLoopPlaying = false;
        }
        else
        {
            trackerEngine->StartPrototypeLoop();
            isPrototypeLoopPlaying = true;
            isAuditionActive       = false;
            isSongPlaying          = false;
        }

        if (prototypeLoopButton != nullptr)
        {
            prototypeLoopButton->SetTextContent(isPrototypeLoopPlaying ? "Stop prototype loop" : "Play prototype loop");
        }
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS,
            isPrototypeLoopPlaying ? "AudioTracker: prototype loop ON" : "AudioTracker: prototype loop OFF");
    });

    RebuildSampleList();
}

void AudioTrackerController::OnUpdate()
{
    // The library bumps a version counter on every mutation (import,
    // forget, register, orphan rescan). Polling that counter here
    // catches status-only changes that pure size comparison would miss.
    int currentLibraryVersion = CC::SampleLibrary::Get()->GetVersion();
    if (currentLibraryVersion != lastLibraryVersion)
    {
        RebuildSampleList();
    }

    // TrackerCommandHistory bumps project.version on Apply / Undo /
    // Redo, so the same polling pattern catches every project mutation.
    if (project != nullptr && project->version != lastProjectVersion)
    {
        RebuildPatternTabs();
        RebuildPatternEditor();
        RebuildSongView();

        // Transport labels reflect the project's current values.
        char buffer[16];
        if (bpmValueLabel != nullptr)
        {
            std::snprintf(buffer, sizeof(buffer), "%d", project->bpm);
            bpmValueLabel->SetTextContent(buffer);
        }
        if (tsNumValueLabel != nullptr)
        {
            std::snprintf(buffer, sizeof(buffer), "%d", project->timeSignatureNumerator);
            tsNumValueLabel->SetTextContent(buffer);
        }
        if (tsDenValueLabel != nullptr)
        {
            std::snprintf(buffer, sizeof(buffer), "%d", project->timeSignatureDenominator);
            tsDenValueLabel->SetTextContent(buffer);
        }
        if (lengthValueLabel != nullptr)
        {
            std::snprintf(buffer, sizeof(buffer), "%d", project->songLengthBars);
            lengthValueLabel->SetTextContent(buffer);
        }

        // Live-edit republish: if a transport is currently running,
        // hand the updated state to TrackerEngine. PlaySong /
        // AuditionPattern detect the same-loop-length case and
        // perform a clock-preserving schedule swap so the loop phase
        // carries through.
        if (isAuditionActive
            && project->currentPatternIndex >= 0
            && (size_t)project->currentPatternIndex < project->patterns.size())
        {
            CC::TrackerEngine::Get()->AuditionPattern(project->patterns[(size_t)project->currentPatternIndex]);
        }
        else if (isSongPlaying)
        {
            CC::TrackerEngine::Get()->PlaySong(*project);
        }
    }
}

void AudioTrackerController::RebuildSampleList()
{
    if (samplesListContainer != nullptr)
    {
        CC::UiManager* ui = CC::UiManager::Get();
        samplesListContainer->ClearChildren();

        const std::vector<CC::SampleEntry>& entries = CC::SampleLibrary::Get()->GetEntries();
        if (entries.empty())
        {
            CC::UiText* emptyHint = new CC::UiText(true);
            emptyHint->AddClass("samples-empty");
            emptyHint->SetTextContent("No samples yet.");
            samplesListContainer->AddChild(emptyHint);
        }
        else
        {
            for (size_t i = 0; i < entries.size(); i++)
            {
                const CC::SampleEntry& entry = entries[i];

                CC::UiPanel* row = new CC::UiPanel();
                row->AddClass("sample-row");

                CC::UiPanel* textColumn = new CC::UiPanel();
                textColumn->AddClass("sample-row-text");

                CC::UiText* idText = new CC::UiText(true);
                idText->AddClass("sample-row-id");
                idText->SetTextContent(entry.id.empty() ? entry.fileName : entry.id);
                textColumn->AddChild(idText);

                CC::UiText* fileText = new CC::UiText(true);
                fileText->AddClass("sample-row-file");
                fileText->SetTextContent(entry.fileName);
                textColumn->AddChild(fileText);

                row->AddChild(textColumn);

                CC::UiPanel* actions = new CC::UiPanel();
                actions->AddClass("sample-row-actions");

                CC::UiText* statusBadge = new CC::UiText(false);
                statusBadge->AddClass("sample-row-status");
                statusBadge->AddClass(SampleStatusClass(entry.status));
                statusBadge->SetTextContent(SampleStatusLabel(entry.status));
                actions->AddChild(statusBadge);

                // Per-row action buttons vary by status:
                //   Ok       -> Play
                //   Modified -> Forget   (drop the entry; user re-imports if desired)
                //   Broken   -> Forget   (file gone; entry is dead)
                //   Orphan   -> Register (promote on-disk file to a real entry)
                // Action keys carry the id (or fileName for orphans)
                // so a single registered handler per row routes to the
                // matching sample.
                if (entry.status == CC::SampleStatus::Ok && !entry.id.empty())
                {
                    std::string actionKey = "audition_" + entry.id;
                    std::string sampleId  = entry.id;
                    ui->RegisterButtonAction(actionKey, [sampleId]()
                    {
                        CC::TrackerEngine::Get()->PlayOneShotSample(sampleId);
                    });

                    CC::UiButton* auditionButton = new CC::UiButton();
                    auditionButton->AddClass("sample-audition-button");
                    auditionButton->SetDataAction(actionKey);
                    auditionButton->SetTextContent("Play");
                    actions->AddChild(auditionButton);
                }
                else if ((entry.status == CC::SampleStatus::Modified || entry.status == CC::SampleStatus::Broken) && !entry.id.empty())
                {
                    std::string actionKey = "forget_" + entry.id;
                    std::string sampleId  = entry.id;
                    ui->RegisterButtonAction(actionKey, [sampleId]()
                    {
                        CC::SampleLibrary::Get()->Forget(sampleId);
                    });

                    CC::UiButton* forgetButton = new CC::UiButton();
                    forgetButton->AddClass("sample-forget-button");
                    forgetButton->SetDataAction(actionKey);
                    forgetButton->SetTextContent("Forget");
                    actions->AddChild(forgetButton);
                }
                else if (entry.status == CC::SampleStatus::Orphan)
                {
                    std::string actionKey = "register_" + entry.fileName;
                    std::string fileName  = entry.fileName;
                    ui->RegisterButtonAction(actionKey, [fileName]()
                    {
                        std::string newId;
                        if (CC::SampleLibrary::Get()->RegisterOrphan(fileName, newId))
                        {
                            // Decode immediately so audition is available
                            // without waiting for the next library sync
                            // pass to find it.
                            CC::TrackerEngine::Get()->SyncWithLibrary();
                        }
                    });

                    CC::UiButton* registerButton = new CC::UiButton();
                    registerButton->AddClass("sample-register-button");
                    registerButton->SetDataAction(actionKey);
                    registerButton->SetTextContent("Register");
                    actions->AddChild(registerButton);
                }

                row->AddChild(actions);

                samplesListContainer->AddChild(row);
            }
        }

        // Newly created elements have empty stateStyles; apply the
        // active screen's CSS rules so they pick up the same .sample-*
        // classes the static HTML uses. Without this they render
        // unstyled (no font, no background) and effectively invisible.
        CC::UiManager::Get()->ApplyStylesToDynamicSubtree(samplesListContainer);

        lastLibraryVersion = CC::SampleLibrary::Get()->GetVersion();
        UpdateMemoryLabel();
        CC::UiManager::Get()->InvalidateLayout();
    }
}

void AudioTrackerController::RebuildPatternTabs()
{
    if (patternTabsContainer != nullptr && project != nullptr)
    {
        CC::UiManager* ui = CC::UiManager::Get();
        patternTabsContainer->ClearChildren();

        for (size_t patternIndex = 0; patternIndex < project->patterns.size(); patternIndex++)
        {
            const CC::Pattern& pattern = project->patterns[patternIndex];

            CC::UiButton* tab = new CC::UiButton();
            tab->AddClass("pattern-tab");
            if ((int)patternIndex == project->currentPatternIndex)
            {
                tab->AddClass("pattern-tab-current");
            }
            tab->SetTextContent(pattern.displayName.empty() ? pattern.id : pattern.displayName);

            char keyBuffer[64];
            std::snprintf(keyBuffer, sizeof(keyBuffer), "selectPattern_%zu", patternIndex);
            std::string actionKey = keyBuffer;
            int targetIndex = (int)patternIndex;
            CC::TrackerProject* projectCapture = project;
            tab->SetDataAction(actionKey);
            ui->RegisterButtonAction(actionKey, [projectCapture, targetIndex]()
            {
                if (projectCapture != nullptr
                    && targetIndex >= 0
                    && targetIndex < (int)projectCapture->patterns.size()
                    && projectCapture->currentPatternIndex != targetIndex)
                {
                    // Selection is UI state, not part of the undo
                    // stack. Direct mutation + version bump triggers
                    // the controller's poll-based rebuild + the
                    // live-edit republish if audition is running.
                    projectCapture->currentPatternIndex = targetIndex;
                    projectCapture->version++;
                }
            });

            patternTabsContainer->AddChild(tab);
        }

        CC::UiManager::Get()->ApplyStylesToDynamicSubtree(patternTabsContainer);
    }
}

void AudioTrackerController::RebuildPatternEditor()
{
    if (patternGridContainer != nullptr && project != nullptr)
    {
        CC::UiManager* ui = CC::UiManager::Get();
        patternGridContainer->ClearChildren();

        if (project->patterns.empty()
            || project->currentPatternIndex < 0
            || project->currentPatternIndex >= (int)project->patterns.size())
        {
            CC::UiText* emptyHint = new CC::UiText(true);
            emptyHint->AddClass("pattern-empty");
            emptyHint->SetTextContent("No pattern.");
            patternGridContainer->AddChild(emptyHint);
        }
        else
        {
            const CC::Pattern& pattern    = project->patterns[(size_t)project->currentPatternIndex];
            int                stepCount  = pattern.GetTotalStepCount();
            int                stepsPerBar = pattern.stepsPerBar;

            if (pattern.tracks.empty())
            {
                CC::UiText* emptyHint = new CC::UiText(true);
                emptyHint->AddClass("pattern-empty");
                emptyHint->SetTextContent("No tracks. Click 'Add Track' to start.");
                patternGridContainer->AddChild(emptyHint);
            }
            else
            {
                for (size_t trackIndex = 0; trackIndex < pattern.tracks.size(); trackIndex++)
                {
                    const CC::PatternTrack& track = pattern.tracks[trackIndex];

                    CC::UiPanel* row = new CC::UiPanel();
                    row->AddClass("pattern-track-row");

                    CC::UiText* label = new CC::UiText(false);
                    label->AddClass("pattern-track-label");
                    if (track.source.kind == CC::TrackSourceKind::Sample && !track.source.sampleId.empty())
                    {
                        label->SetTextContent(track.source.sampleId);
                    }
                    else if (track.source.kind == CC::TrackSourceKind::Synth)
                    {
                        label->SetTextContent("(synth)");
                    }
                    else
                    {
                        char buffer[32];
                        std::snprintf(buffer, sizeof(buffer), "Track %zu", trackIndex + 1);
                        label->SetTextContent(buffer);
                    }
                    row->AddChild(label);

                    // Per-row source assignment: clicking the button
                    // cycles to the next Ok-status library sample. v1
                    // first cut — a dropdown is the proper UX once the
                    // engine has UiDropdown support refined enough for
                    // dynamic option lists.
                    CC::UiButton* assignButton = new CC::UiButton();
                    assignButton->AddClass("pattern-track-assign-button");
                    assignButton->SetTextContent("Cycle Source");
                    {
                        char keyBuffer[64];
                        std::snprintf(keyBuffer, sizeof(keyBuffer), "assignSource_%d", (int)trackIndex);
                        std::string actionKey = keyBuffer;
                        std::string patternIdCapture = pattern.id;
                        int         trackIdxCapture  = (int)trackIndex;
                        CC::TrackerProject* projectCapture2 = project;
                        assignButton->SetDataAction(actionKey);
                        ui->RegisterButtonAction(actionKey, [projectCapture2, patternIdCapture, trackIdxCapture]()
                        {
                            if (projectCapture2 == nullptr) { return; }
                            CC::Pattern* p = projectCapture2->FindPattern(patternIdCapture);
                            if (p == nullptr || trackIdxCapture < 0 || (size_t)trackIdxCapture >= p->tracks.size()) { return; }

                            const std::vector<CC::SampleEntry>& entries = CC::SampleLibrary::Get()->GetEntries();
                            std::vector<std::string> okIds;
                            for (size_t k = 0; k < entries.size(); k++)
                            {
                                if (entries[k].status == CC::SampleStatus::Ok && !entries[k].id.empty())
                                {
                                    okIds.push_back(entries[k].id);
                                }
                            }

                            CC::TrackSource newSource;
                            if (okIds.empty())
                            {
                                newSource.kind = CC::TrackSourceKind::None;
                            }
                            else
                            {
                                const std::string& currentId = p->tracks[(size_t)trackIdxCapture].source.sampleId;
                                int currentIdx = -1;
                                for (size_t k = 0; k < okIds.size(); k++)
                                {
                                    if (okIds[k] == currentId)
                                    {
                                        currentIdx = (int)k;
                                        break;
                                    }
                                }
                                int nextIdx = (currentIdx + 1) % (int)okIds.size();
                                newSource.kind     = CC::TrackSourceKind::Sample;
                                newSource.sampleId = okIds[(size_t)nextIdx];
                            }

                            CC::TrackerCommandHistory::Get()->Apply(
                                std::unique_ptr<CC::TrackerCommand>(new CC::AssignSourceToPatternTrackCommand(patternIdCapture, trackIdxCapture, newSource)),
                                *projectCapture2);
                        });
                    }
                    row->AddChild(assignButton);

                    // Mute toggle. Highlights when muted; muted tracks
                    // skip schedule emission in both audition and song
                    // playback.
                    CC::UiButton* muteButton = new CC::UiButton();
                    muteButton->AddClass("pattern-track-mute-button");
                    if (track.isMuted)
                    {
                        muteButton->AddClass("pattern-track-muted");
                    }
                    muteButton->SetTextContent("M");
                    {
                        char keyBuffer[64];
                        std::snprintf(keyBuffer, sizeof(keyBuffer), "muteTrack_%d", (int)trackIndex);
                        std::string actionKey = keyBuffer;
                        std::string patternIdCapture = pattern.id;
                        int         trackIdxCapture  = (int)trackIndex;
                        CC::TrackerProject* projectCapture4 = project;
                        muteButton->SetDataAction(actionKey);
                        ui->RegisterButtonAction(actionKey, [projectCapture4, patternIdCapture, trackIdxCapture]()
                        {
                            if (projectCapture4 != nullptr)
                            {
                                CC::TrackerCommandHistory::Get()->Apply(
                                    std::unique_ptr<CC::TrackerCommand>(new CC::TogglePatternTrackMutedCommand(patternIdCapture, trackIdxCapture)),
                                    *projectCapture4);
                            }
                        });
                    }
                    row->AddChild(muteButton);

                    // Per-row remove button. Sits at the right of the
                    // row label area so it's visually associated with
                    // the track, not with the cells.
                    CC::UiButton* removeButton = new CC::UiButton();
                    removeButton->AddClass("pattern-track-remove-button");
                    removeButton->SetTextContent("X");
                    {
                        char keyBuffer[64];
                        std::snprintf(keyBuffer, sizeof(keyBuffer), "removeTrack_%d", (int)trackIndex);
                        std::string actionKey = keyBuffer;
                        std::string patternIdCapture = pattern.id;
                        int         trackIdxCapture  = (int)trackIndex;
                        CC::TrackerProject* projectCapture3 = project;
                        removeButton->SetDataAction(actionKey);
                        ui->RegisterButtonAction(actionKey, [projectCapture3, patternIdCapture, trackIdxCapture]()
                        {
                            if (projectCapture3 != nullptr)
                            {
                                CC::TrackerCommandHistory::Get()->Apply(
                                    std::unique_ptr<CC::TrackerCommand>(new CC::RemovePatternTrackCommand(patternIdCapture, trackIdxCapture)),
                                    *projectCapture3);
                            }
                        });
                    }
                    row->AddChild(removeButton);

                    CC::UiPanel* cells = new CC::UiPanel();
                    cells->AddClass("pattern-track-cells");

                    const std::string& patternId = pattern.id;
                    int                trackIdxCaptured = (int)trackIndex;
                    CC::TrackerProject* projectCapture = project;

                    for (int stepIndex = 0; stepIndex < stepCount; stepIndex++)
                    {
                        // UiButton (not UiPanel) so the input handler
                        // routes hover and click states through the
                        // existing button machinery.
                        CC::UiButton* cell = new CC::UiButton();
                        cell->AddClass("pattern-step-cell");

                        bool isOn = (stepIndex < (int)track.velocities.size())
                            && (track.velocities[(size_t)stepIndex] > 0.0f);
                        if (isOn)
                        {
                            cell->AddClass("pattern-step-on");
                        }

                        bool isBarBoundary = (stepsPerBar > 0) && ((stepIndex % stepsPerBar) == 0) && (stepIndex > 0);
                        if (isBarBoundary)
                        {
                            cell->AddClass("pattern-step-bar-start");
                        }

                        // Per-cell action key carries pattern id +
                        // track + step so a single registered handler
                        // per cell routes to the correct command.
                        // Action map overwrites by key on each rebuild,
                        // so the cost is bounded regardless of how
                        // many rebuilds the user triggers.
                        char keyBuffer[64];
                        std::snprintf(keyBuffer, sizeof(keyBuffer), "step_%d_%d", trackIdxCaptured, stepIndex);
                        std::string actionKey = keyBuffer;

                        cell->SetDataAction(actionKey);
                        ui->RegisterButtonAction(actionKey, [projectCapture, patternId, trackIdxCaptured, stepIndex]()
                        {
                            if (projectCapture != nullptr)
                            {
                                CC::TrackerCommandHistory::Get()->Apply(
                                    std::unique_ptr<CC::TrackerCommand>(new CC::ToggleStepCommand(patternId, trackIdxCaptured, stepIndex)),
                                    *projectCapture);
                            }
                        });

                        cells->AddChild(cell);
                    }

                    row->AddChild(cells);
                    patternGridContainer->AddChild(row);
                }
            }
        }

        CC::UiManager::Get()->ApplyStylesToDynamicSubtree(patternGridContainer);

        lastProjectVersion = project->version;
        CC::UiManager::Get()->InvalidateLayout();
    }
}

void AudioTrackerController::RebuildSongView()
{
    if (songGridContainer != nullptr && project != nullptr)
    {
        CC::UiManager* ui = CC::UiManager::Get();
        songGridContainer->ClearChildren();

        if (project->songTracks.empty())
        {
            CC::UiText* emptyHint = new CC::UiText(true);
            emptyHint->AddClass("song-empty");
            emptyHint->SetTextContent("No song tracks. Click 'Add Song Track' to start.");
            songGridContainer->AddChild(emptyHint);
        }
        else
        {
            for (size_t songTrackIndex = 0; songTrackIndex < project->songTracks.size(); songTrackIndex++)
            {
                const CC::SongTrack& songTrack = project->songTracks[songTrackIndex];

                CC::UiPanel* row = new CC::UiPanel();
                row->AddClass("song-track-row");

                CC::UiText* label = new CC::UiText(false);
                label->AddClass("song-track-label");
                label->SetTextContent(songTrack.displayName.empty() ? "Track" : songTrack.displayName);
                row->AddChild(label);

                CC::UiButton* songMuteButton = new CC::UiButton();
                songMuteButton->AddClass("song-track-mute-button");
                if (songTrack.isMuted)
                {
                    songMuteButton->AddClass("song-track-muted");
                }
                songMuteButton->SetTextContent("M");
                {
                    char keyBuffer[64];
                    std::snprintf(keyBuffer, sizeof(keyBuffer), "muteSongTrack_%zu", songTrackIndex);
                    std::string actionKey = keyBuffer;
                    int songTrackIdxCapture = (int)songTrackIndex;
                    CC::TrackerProject* projectMuteCapture = project;
                    songMuteButton->SetDataAction(actionKey);
                    ui->RegisterButtonAction(actionKey, [projectMuteCapture, songTrackIdxCapture]()
                    {
                        if (projectMuteCapture != nullptr)
                        {
                            CC::TrackerCommandHistory::Get()->Apply(
                                std::unique_ptr<CC::TrackerCommand>(new CC::ToggleSongTrackMutedCommand(songTrackIdxCapture)),
                                *projectMuteCapture);
                        }
                    });
                }
                row->AddChild(songMuteButton);

                CC::UiPanel* bars = new CC::UiPanel();
                bars->AddClass("song-track-bars");

                for (size_t barIndex = 0; barIndex < songTrack.patternIdPerBar.size(); barIndex++)
                {
                    const std::string& placedId = songTrack.patternIdPerBar[barIndex];

                    CC::UiButton* cell = new CC::UiButton();
                    cell->AddClass("song-bar-cell");
                    if (!placedId.empty())
                    {
                        cell->AddClass("song-bar-cell-filled");
                    }
                    cell->SetTextContent(placedId.empty() ? "—" : placedId);

                    char keyBuffer[64];
                    std::snprintf(keyBuffer, sizeof(keyBuffer), "songCell_%zu_%zu", songTrackIndex, barIndex);
                    std::string actionKey = keyBuffer;
                    int songTrackIdxCapture = (int)songTrackIndex;
                    int barIdxCapture       = (int)barIndex;
                    CC::TrackerProject* projectCapture = project;
                    cell->SetDataAction(actionKey);
                    ui->RegisterButtonAction(actionKey, [projectCapture, songTrackIdxCapture, barIdxCapture]()
                    {
                        if (projectCapture == nullptr) { return; }
                        if (songTrackIdxCapture < 0 || (size_t)songTrackIdxCapture >= projectCapture->songTracks.size()) { return; }
                        const CC::SongTrack& track = projectCapture->songTracks[(size_t)songTrackIdxCapture];
                        if (barIdxCapture < 0 || (size_t)barIdxCapture >= track.patternIdPerBar.size()) { return; }

                        // Cycle: empty -> P1 -> P2 -> ... -> empty.
                        // Building the option list inline is cheap; the
                        // pattern count is small.
                        std::vector<std::string> options;
                        options.push_back(std::string());
                        for (size_t k = 0; k < projectCapture->patterns.size(); k++)
                        {
                            options.push_back(projectCapture->patterns[k].id);
                        }

                        const std::string& current = track.patternIdPerBar[(size_t)barIdxCapture];
                        int currentOption = 0;
                        for (size_t k = 0; k < options.size(); k++)
                        {
                            if (options[k] == current)
                            {
                                currentOption = (int)k;
                                break;
                            }
                        }
                        int nextOption = (currentOption + 1) % (int)options.size();

                        CC::TrackerCommandHistory::Get()->Apply(
                            std::unique_ptr<CC::TrackerCommand>(new CC::PlacePatternInSongCommand(songTrackIdxCapture, barIdxCapture, options[(size_t)nextOption])),
                            *projectCapture);
                    });

                    bars->AddChild(cell);
                }

                row->AddChild(bars);
                songGridContainer->AddChild(row);
            }
        }

        CC::UiManager::Get()->ApplyStylesToDynamicSubtree(songGridContainer);
    }
}

void AudioTrackerController::UpdateMemoryLabel()
{
    if (samplesMemoryLabel != nullptr)
    {
        size_t totalBytes = CC::TrackerEngine::Get()->GetDiskSamplesByteCount();
        float  totalMb    = (float)totalBytes / (1024.0f * 1024.0f);

        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "Memory: %.1f MB", totalMb);
        samplesMemoryLabel->SetTextContent(buffer);
    }
}
