#include "TrackerProjectFile.h"

#include <cstdio>
#include <cstdint>

#include <rapidyaml-0.10.0.hpp>

#include "TrackerProject.h"
#include "Pattern.h"
#include "TrackSource.h"
#include "TrackerPaths.h"
#include "PlatformFileSystem.h"
#include "PrintManager.h"

namespace CC
{
    namespace TrackerProjectFile
    {
        // ========================
        // Helpers
        // ========================

        static std::string NodeToString(ryml::ConstNodeRef node)
        {
            std::string result;
            if (node.has_val())
            {
                c4::csubstr value = node.val();
                result = std::string(value.data(), value.size());
            }
            return result;
        }

        static int NodeToInt(ryml::ConstNodeRef node, int fallback)
        {
            int result = fallback;
            if (node.has_val())
            {
                int parsed = 0;
                node >> parsed;
                result = parsed;
            }
            return result;
        }

        static float NodeToFloat(ryml::ConstNodeRef node, float fallback)
        {
            float result = fallback;
            if (node.has_val())
            {
                float parsed = 0.0f;
                node >> parsed;
                result = parsed;
            }
            return result;
        }

        static bool NodeToBool(ryml::ConstNodeRef node, bool fallback)
        {
            bool result = fallback;
            if (node.has_val())
            {
                std::string text = NodeToString(node);
                if (text == "true" || text == "True" || text == "1")
                {
                    result = true;
                }
                else if (text == "false" || text == "False" || text == "0")
                {
                    result = false;
                }
            }
            return result;
        }

        static const char* SourceKindString(TrackSourceKind kind)
        {
            const char* result = "none";
            switch (kind)
            {
                case TrackSourceKind::None:   result = "none";   break;
                case TrackSourceKind::Sample: result = "sample"; break;
                case TrackSourceKind::Synth:  result = "synth";  break;
            }
            return result;
        }

        static TrackSourceKind ParseSourceKind(const std::string& text)
        {
            TrackSourceKind result = TrackSourceKind::None;
            if (text == "sample")
            {
                result = TrackSourceKind::Sample;
            }
            else if (text == "synth")
            {
                result = TrackSourceKind::Synth;
            }
            return result;
        }

        // Hand-rolled emitter mirroring SampleLibrary's pattern. Output
        // is a strict subset of YAML that ryml can re-parse on load.
        // Indentation is two spaces per level.
        static std::string EmitProjectYaml(const TrackerProject& project)
        {
            std::string result;
            char        buffer[64];

            std::snprintf(buffer, sizeof(buffer), "version: %d\n", TRACKER_PROJECT_FILE_VERSION);
            result += buffer;
            std::snprintf(buffer, sizeof(buffer), "bpm: %d\n", project.bpm);
            result += buffer;
            std::snprintf(buffer, sizeof(buffer), "timeSignatureNumerator: %d\n", project.timeSignatureNumerator);
            result += buffer;
            std::snprintf(buffer, sizeof(buffer), "timeSignatureDenominator: %d\n", project.timeSignatureDenominator);
            result += buffer;
            std::snprintf(buffer, sizeof(buffer), "songLengthBars: %d\n", project.songLengthBars);
            result += buffer;
            std::snprintf(buffer, sizeof(buffer), "currentPatternIndex: %d\n", project.currentPatternIndex);
            result += buffer;

            result += "patterns:\n";
            for (size_t patternIndex = 0; patternIndex < project.patterns.size(); patternIndex++)
            {
                const Pattern& pattern = project.patterns[patternIndex];
                result += "  - id: ";
                result += pattern.id;
                result += "\n    displayName: ";
                result += pattern.displayName;
                result += "\n";
                std::snprintf(buffer, sizeof(buffer), "    barCount: %d\n", pattern.barCount);
                result += buffer;
                std::snprintf(buffer, sizeof(buffer), "    stepsPerBar: %d\n", pattern.stepsPerBar);
                result += buffer;
                result += "    tracks:\n";
                for (size_t trackIndex = 0; trackIndex < pattern.tracks.size(); trackIndex++)
                {
                    const PatternTrack& track = pattern.tracks[trackIndex];

                    result += "      - source:\n";
                    result += "          kind: ";
                    result += SourceKindString(track.source.kind);
                    result += "\n";
                    if (track.source.kind == TrackSourceKind::Sample)
                    {
                        result += "          sampleId: ";
                        result += track.source.sampleId;
                        result += "\n";
                    }
                    // Synth source params will be persisted when synth
                    // tracks become editable.

                    std::snprintf(buffer, sizeof(buffer), "        gainLinear: %g\n", track.gainLinear);
                    result += buffer;
                    result += "        isMuted: ";
                    result += (track.isMuted ? "true" : "false");
                    result += "\n";

                    result += "        velocities: [";
                    for (size_t velocityIndex = 0; velocityIndex < track.velocities.size(); velocityIndex++)
                    {
                        if (velocityIndex > 0)
                        {
                            result += ", ";
                        }
                        std::snprintf(buffer, sizeof(buffer), "%g", track.velocities[velocityIndex]);
                        result += buffer;
                    }
                    result += "]\n";
                }
            }

            result += "songTracks:\n";
            for (size_t songTrackIndex = 0; songTrackIndex < project.songTracks.size(); songTrackIndex++)
            {
                const SongTrack& songTrack = project.songTracks[songTrackIndex];
                result += "  - displayName: ";
                result += songTrack.displayName;
                result += "\n";
                std::snprintf(buffer, sizeof(buffer), "    gainLinear: %g\n", songTrack.gainLinear);
                result += buffer;
                result += "    isMuted: ";
                result += (songTrack.isMuted ? "true" : "false");
                result += "\n";

                result += "    patternIdPerBar: [";
                for (size_t barIndex = 0; barIndex < songTrack.patternIdPerBar.size(); barIndex++)
                {
                    if (barIndex > 0)
                    {
                        result += ", ";
                    }
                    if (songTrack.patternIdPerBar[barIndex].empty())
                    {
                        result += "\"\"";
                    }
                    else
                    {
                        result += songTrack.patternIdPerBar[barIndex];
                    }
                }
                result += "]\n";
            }

            return result;
        }

        // ========================
        // Public API
        // ========================

        std::string GetDefaultProjectPath()
        {
            return TrackerPaths::GetProjectsPath() + "/song.cctrack";
        }

        bool Save(const TrackerProject& project, const std::string& filePath)
        {
            std::string yamlText  = EmitProjectYaml(project);
            bool        succeeded = PlatformFileSystem::Get()->WriteFileTextAtomic(filePath.c_str(), yamlText);
            if (!succeeded)
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerProjectFile::Save failed to write %s", filePath.c_str());
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerProjectFile::Save wrote %s", filePath.c_str());
            }
            return succeeded;
        }

        bool Load(const std::string& filePath, TrackerProject& outProject)
        {
            bool succeeded = false;

            if (!PlatformFileSystem::Get()->FileExists(filePath.c_str()))
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerProjectFile::Load: file not found: %s", filePath.c_str());
            }
            else
            {
                std::string yamlText;
                if (!PlatformFileSystem::Get()->ReadFileText(filePath.c_str(), yamlText))
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerProjectFile::Load: failed to read %s", filePath.c_str());
                }
                else
                {
                    ryml::Tree tree;
                    bool       parseOk = true;
                    try
                    {
                        tree = ryml::parse_in_arena(ryml::to_csubstr(yamlText));
                    }
                    catch (const std::exception& exception)
                    {
                        CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerProjectFile::Load: ryml parse failed: %s", exception.what());
                        parseOk = false;
                    }

                    if (parseOk)
                    {
                        ryml::ConstNodeRef root = tree.rootref();

                        int fileVersion = root.has_child("version") ? NodeToInt(root["version"], 0) : 0;
                        if (fileVersion != TRACKER_PROJECT_FILE_VERSION)
                        {
                            CCPrint(PrintManager::CHANNEL_ALWAYS,
                                "TrackerProjectFile::Load: version mismatch (got %d, expected %d)", fileVersion, TRACKER_PROJECT_FILE_VERSION);
                        }
                        else
                        {
                            TrackerProject loaded;
                            loaded.bpm                      = NodeToInt  (root["bpm"],                      90);
                            loaded.timeSignatureNumerator   = NodeToInt  (root["timeSignatureNumerator"],   4);
                            loaded.timeSignatureDenominator = NodeToInt  (root["timeSignatureDenominator"], 4);
                            loaded.songLengthBars           = NodeToInt  (root["songLengthBars"],           8);
                            loaded.currentPatternIndex      = NodeToInt  (root["currentPatternIndex"],      0);

                            if (root.has_child("patterns") && root["patterns"].is_seq())
                            {
                                for (ryml::ConstNodeRef patternNode : root["patterns"].children())
                                {
                                    Pattern pattern;
                                    pattern.id           = NodeToString(patternNode["id"]);
                                    pattern.displayName  = NodeToString(patternNode["displayName"]);
                                    pattern.barCount     = NodeToInt   (patternNode["barCount"],    1);
                                    pattern.stepsPerBar  = NodeToInt   (patternNode["stepsPerBar"], 16);

                                    if (patternNode.has_child("tracks") && patternNode["tracks"].is_seq())
                                    {
                                        for (ryml::ConstNodeRef trackNode : patternNode["tracks"].children())
                                        {
                                            PatternTrack track;
                                            if (trackNode.has_child("source"))
                                            {
                                                ryml::ConstNodeRef sourceNode = trackNode["source"];
                                                track.source.kind = ParseSourceKind(NodeToString(sourceNode["kind"]));
                                                if (track.source.kind == TrackSourceKind::Sample && sourceNode.has_child("sampleId"))
                                                {
                                                    track.source.sampleId = NodeToString(sourceNode["sampleId"]);
                                                }
                                            }
                                            track.gainLinear = NodeToFloat(trackNode["gainLinear"], 1.0f);
                                            track.isMuted    = NodeToBool (trackNode["isMuted"],    false);

                                            if (trackNode.has_child("velocities") && trackNode["velocities"].is_seq())
                                            {
                                                for (ryml::ConstNodeRef velocityNode : trackNode["velocities"].children())
                                                {
                                                    track.velocities.push_back(NodeToFloat(velocityNode, 0.0f));
                                                }
                                            }

                                            pattern.tracks.push_back(track);
                                        }
                                    }

                                    loaded.patterns.push_back(pattern);
                                }
                            }

                            if (root.has_child("songTracks") && root["songTracks"].is_seq())
                            {
                                for (ryml::ConstNodeRef songTrackNode : root["songTracks"].children())
                                {
                                    SongTrack songTrack;
                                    songTrack.displayName = NodeToString(songTrackNode["displayName"]);
                                    songTrack.gainLinear  = NodeToFloat (songTrackNode["gainLinear"], 1.0f);
                                    songTrack.isMuted     = NodeToBool  (songTrackNode["isMuted"],    false);

                                    if (songTrackNode.has_child("patternIdPerBar") && songTrackNode["patternIdPerBar"].is_seq())
                                    {
                                        for (ryml::ConstNodeRef barNode : songTrackNode["patternIdPerBar"].children())
                                        {
                                            songTrack.patternIdPerBar.push_back(NodeToString(barNode));
                                        }
                                    }

                                    loaded.songTracks.push_back(songTrack);
                                }
                            }

                            // Sanity: clamp currentPatternIndex against
                            // the loaded patterns vector.
                            if (loaded.currentPatternIndex >= (int)loaded.patterns.size())
                            {
                                loaded.currentPatternIndex = (int)loaded.patterns.size() - 1;
                            }
                            if (loaded.currentPatternIndex < 0 && !loaded.patterns.empty())
                            {
                                loaded.currentPatternIndex = 0;
                            }

                            outProject = loaded;
                            succeeded = true;
                            CCPrint(PrintManager::CHANNEL_ALWAYS, "TrackerProjectFile::Load loaded %s", filePath.c_str());
                        }
                    }
                }
            }

            return succeeded;
        }
    }
}
