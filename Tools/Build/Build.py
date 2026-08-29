"""ChillCore desktop build pipeline.

Drives the CMake presets, captures the log, parses diagnostics, and writes the
per-build report folder. Invoked by Build.sh; see
DevDocs/ChillCoreDevDocs/docs/Planning/BuildPipelinePlan.md.

Exit codes:
    0  success
    1  compile or link errors
    2  invalid usage
    3  missing toolchain
    4  internal script failure
"""

import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import time

EXIT_SUCCESS = 0
EXIT_BUILD_FAILED = 1
EXIT_INVALID_USAGE = 2
EXIT_MISSING_TOOLCHAIN = 3
EXIT_INTERNAL_FAILURE = 4

CONFIGURE_PRESET = "Desktop"
BUILD_PRESETS = {"Debug": "DesktopDebug", "Release": "DesktopRelease"}
REQUIRED_GENERATOR = "Visual Studio 18 2026"
UNLABELLED = "Unlabelled"
REPORT_ERROR_LIMIT = 20

SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
REPOSITORY_ROOT = os.path.abspath(os.path.join(SCRIPT_DIRECTORY, "..", ".."))
CODE_ROOT = os.path.join(REPOSITORY_ROOT, "Code")
BUILD_ROOT = os.path.join(REPOSITORY_ROOT, "Build")
LOGS_ROOT = os.path.join(BUILD_ROOT, "Logs")
CMAKE_BINARY_DIRECTORY = os.path.join(BUILD_ROOT, "CMake", "Desktop")
COUNTER_PATH = os.path.join(BUILD_ROOT, "BuildCounter.json")
RUNTIME_DATA_SOURCE = os.path.join(CODE_ROOT, "App", "Data")
RUNTIME_DATA_FOLDER_NAME = "Data"
GENERATED_HEADER_PATH = os.path.join(CODE_ROOT, "Core", "BuildInfoGenerated.h")


# ============================================================================
# Toolchain resolution
# ============================================================================

def FindVisualStudioInstallation():
    """Newest Visual Studio installation path, or None."""
    installationPath = None
    programFiles = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswherePath = os.path.join(programFiles, "Microsoft Visual Studio", "Installer", "vswhere.exe")

    if os.path.isfile(vswherePath):
        completed = subprocess.run(
            [vswherePath, "-latest", "-prerelease", "-products", "*",
             "-property", "installationPath"],
            capture_output=True, text=True)

        if completed.returncode == 0:
            firstLine = completed.stdout.strip().splitlines()
            if firstLine:
                installationPath = firstLine[0].strip()

    return installationPath


def SupportsRequiredGenerator(cmakePath):
    supports = False
    try:
        completed = subprocess.run([cmakePath, "--help"], capture_output=True, text=True)
        supports = completed.returncode == 0 and REQUIRED_GENERATOR in completed.stdout
    except OSError:
        supports = False

    return supports


def ResolveCMake():
    """CMake that knows the required generator.

    Visual Studio's bundled CMake is preferred: a CMake on PATH is often older
    than the installed Visual Studio and does not know its generator.
    """
    resolved = None
    candidates = []

    installationPath = FindVisualStudioInstallation()
    if installationPath is not None:
        candidates.append(os.path.join(
            installationPath, "Common7", "IDE", "CommonExtensions", "Microsoft",
            "CMake", "CMake", "bin", "cmake.exe"))

    pathCMake = shutil.which("cmake")
    if pathCMake is not None:
        candidates.append(pathCMake)

    for candidate in candidates:
        if resolved is None and os.path.isfile(candidate) and SupportsRequiredGenerator(candidate):
            resolved = candidate

    return resolved


def ValidateToolchain():
    """Resolved CMake path, or None after reporting what is missing."""
    cmakePath = None
    installationPath = FindVisualStudioInstallation()

    if installationPath is None:
        print("Missing toolchain: no Visual Studio installation found.")
        print("  Install Visual Studio 2026 with the Desktop development with C++ workload.")
    else:
        cmakePath = ResolveCMake()

        if cmakePath is None:
            print("Missing toolchain: no CMake found that supports the "
                  "'" + REQUIRED_GENERATOR + "' generator.")
            print("  Install the C++ CMake tools component in the Visual Studio installer,")
            print("  or put a CMake new enough to know that generator on PATH.")

    return cmakePath


# ============================================================================
# Build identifier
# ============================================================================

def RunGit(arguments):
    """Trimmed stdout of a git command, or None when git is unavailable."""
    output = None
    try:
        completed = subprocess.run(
            ["git", "-C", REPOSITORY_ROOT] + arguments,
            capture_output=True, text=True)

        if completed.returncode == 0:
            output = completed.stdout.strip()
    except OSError:
        output = None

    return output


def GetCommitHash():
    commitHash = RunGit(["rev-parse", "--short", "HEAD"])

    if commitHash is None or commitHash == "":
        commitHash = "Unknown"

    return commitHash


def IsWorkingTreeDirty():
    status = RunGit(["status", "--porcelain"])
    return status is not None and status != ""


def NextDailyCounter(dateStamp):
    """Counter for today, incremented and persisted."""
    counter = 1
    state = {}

    if os.path.isfile(COUNTER_PATH):
        try:
            with open(COUNTER_PATH, "r", encoding="utf-8") as counterFile:
                state = json.load(counterFile)
        except (OSError, ValueError):
            state = {}

    if state.get("date") == dateStamp:
        counter = int(state.get("counter", 0)) + 1

    os.makedirs(BUILD_ROOT, exist_ok=True)
    with open(COUNTER_PATH, "w", encoding="utf-8") as counterFile:
        json.dump({"date": dateStamp, "counter": counter}, counterFile, indent=4)

    return counter


def GenerateBuildId():
    """Date, daily counter, commit hash, and a dirty marker when unclean."""
    dateStamp = datetime.datetime.now().strftime("%Y_%m_%d")
    counter = NextDailyCounter(dateStamp)
    buildId = dateStamp + "_" + str(counter) + "_g" + GetCommitHash()

    if IsWorkingTreeDirty():
        buildId = buildId + "_dirty"

    return buildId


def WriteGeneratedHeader(buildId, commitHash, configuration, timestamp):
    """Write BuildInfoGenerated.h, but only when its content changes.

    Rewriting it unconditionally would recompile BuildInfo.cpp on every
    incremental build for no reason.
    """
    lines = [
        "// Generated by Tools/Build/Build.py. Not checked in.",
        "",
        "#ifndef BUILDINFOGENERATED_H",
        "#define BUILDINFOGENERATED_H",
        "",
        '#define CC_BUILD_ID "' + buildId + '"',
        '#define CC_BUILD_COMMIT_HASH "' + commitHash + '"',
        '#define CC_BUILD_CONFIGURATION "' + configuration + '"',
        '#define CC_BUILD_TIMESTAMP "' + timestamp + '"',
        "",
        "#endif // BUILDINFOGENERATED_H",
        "",
    ]
    content = "\n".join(lines)
    existing = None

    if os.path.isfile(GENERATED_HEADER_PATH):
        try:
            with open(GENERATED_HEADER_PATH, "r", encoding="utf-8") as headerFile:
                existing = headerFile.read()
        except OSError:
            existing = None

    if existing != content:
        with open(GENERATED_HEADER_PATH, "w", encoding="utf-8") as headerFile:
            headerFile.write(content)


# ============================================================================
# Running CMake
# ============================================================================

def RunCapturing(command, logLines):
    """Run a command, echoing and collecting its combined output."""
    exitCode = EXIT_INTERNAL_FAILURE

    logLines.append("> " + " ".join(command))

    try:
        process = subprocess.Popen(
            command, cwd=CODE_ROOT, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, encoding="utf-8",
            errors="replace", bufsize=1)

        for line in process.stdout:
            line = line.rstrip("\n")
            print(line)
            logLines.append(line)

        process.wait()
        exitCode = process.returncode
    except OSError as error:
        message = "Failed to run " + command[0] + ": " + str(error)
        print(message)
        logLines.append(message)

    logLines.append("")

    return exitCode


# ============================================================================
# Runtime data
#
# The executable resolves assets relative to its working directory, so a build
# output directory is only runnable if the data tree sits beside the
# executable under its original folder name.
#
# This runs in the pipeline rather than in the build. A build started from the
# IDE deliberately does not copy: the debugger starts in the application
# directory and reads the source tree, so the interactive loop would be paying
# for a copy it never reads — a cost that grows with the asset tree.
# ============================================================================

def GetOutputDirectory(configuration):
    return os.path.join(BUILD_ROOT, "x64", configuration)


def IsCopyNeeded(sourcePath, destinationPath):
    """True when the destination is missing or does not match the source."""
    isNeeded = True

    if os.path.isfile(destinationPath):
        sourceInfo = os.stat(sourcePath)
        destinationInfo = os.stat(destinationPath)
        isNeeded = (sourceInfo.st_size != destinationInfo.st_size
                    or sourceInfo.st_mtime > destinationInfo.st_mtime)

    return isNeeded


def CopyRuntimeData(configuration, logLines):
    """Mirror the runtime data tree beside the executable.

    Returns counts of what was copied and what was pruned. Pruning matters:
    an asset deleted from source but left in the output keeps working until
    it does not, which is worse than one that is plainly missing.
    """
    summary = {"copied": 0, "pruned": 0, "unchanged": 0}
    destinationRoot = os.path.join(GetOutputDirectory(configuration),
                                   RUNTIME_DATA_FOLDER_NAME)
    sourceFiles = set()

    for directoryPath, directoryNames, fileNames in os.walk(RUNTIME_DATA_SOURCE):
        relativeDirectory = os.path.relpath(directoryPath, RUNTIME_DATA_SOURCE)
        destinationDirectory = os.path.join(destinationRoot, relativeDirectory)
        os.makedirs(destinationDirectory, exist_ok=True)

        for fileName in fileNames:
            sourcePath = os.path.join(directoryPath, fileName)
            destinationPath = os.path.join(destinationDirectory, fileName)
            sourceFiles.add(os.path.normpath(destinationPath))

            if IsCopyNeeded(sourcePath, destinationPath):
                shutil.copy2(sourcePath, destinationPath)
                summary["copied"] += 1
            else:
                summary["unchanged"] += 1

    # Anything in the destination with no counterpart in source is stale.
    for directoryPath, directoryNames, fileNames in os.walk(destinationRoot, topdown=False):
        for fileName in fileNames:
            destinationPath = os.path.normpath(os.path.join(directoryPath, fileName))

            if destinationPath not in sourceFiles:
                os.remove(destinationPath)
                summary["pruned"] += 1

        if not os.listdir(directoryPath) and directoryPath != destinationRoot:
            os.rmdir(directoryPath)

    message = ("Runtime data: " + str(summary["copied"]) + " copied, " +
               str(summary["pruned"]) + " pruned, " +
               str(summary["unchanged"]) + " unchanged -> " + destinationRoot)
    print(message)
    logLines.append(message)

    return summary


# ============================================================================
# Diagnostic parsing
# ============================================================================

# file.cpp(123,45): error C2065: message [project.vcxproj]
LOCATED_DIAGNOSTIC = re.compile(
    r"^\s*(?P<file>[^()]+?)\((?P<line>\d+)(?:,(?P<column>\d+))?\)\s*:\s*"
    r"(?P<severity>fatal error|error|warning)\s+(?P<code>[A-Za-z]+\d+)\s*:\s*"
    r"(?P<message>.*?)\s*(?:\[[^\]]*\])?$")

# LINK : fatal error LNK1104: message
UNLOCATED_DIAGNOSTIC = re.compile(
    r"^\s*(?P<origin>[^:]+?)\s*:\s*"
    r"(?P<severity>fatal error|error|warning)\s+(?P<code>[A-Za-z]+\d+)\s*:\s*"
    r"(?P<message>.*?)\s*(?:\[[^\]]*\])?$")

CMAKE_DIAGNOSTIC = re.compile(r"^CMake (?P<severity>Error|Warning)\b(?P<message>.*)$")


def NormaliseSeverity(rawSeverity):
    normalised = "warning"

    if rawSeverity.lower().endswith("error"):
        normalised = "error"

    return normalised


def RelativeToRepository(path):
    relative = path

    try:
        # Only absolute paths are rewritten. A relative one would be resolved
        # against this script's working directory rather than the compiler's.
        if os.path.isabs(path) and path.lower().startswith(REPOSITORY_ROOT.lower()):
            relative = os.path.relpath(path, REPOSITORY_ROOT).replace("\\", "/")
    except (OSError, ValueError):
        relative = path

    return relative


def ParseDiagnostics(logLines):
    """One entry per distinct diagnostic, in the order first seen."""
    diagnostics = []
    seen = set()

    for line in logLines:
        entry = None
        match = LOCATED_DIAGNOSTIC.match(line)

        if match is not None:
            entry = {
                "severity": NormaliseSeverity(match.group("severity")),
                "file": RelativeToRepository(match.group("file").strip()),
                "line": int(match.group("line")),
                "column": int(match.group("column")) if match.group("column") else 0,
                "code": match.group("code"),
                "message": match.group("message").strip(),
            }
        else:
            match = UNLOCATED_DIAGNOSTIC.match(line)

            if match is not None:
                entry = {
                    "severity": NormaliseSeverity(match.group("severity")),
                    "file": match.group("origin").strip(),
                    "line": 0,
                    "column": 0,
                    "code": match.group("code"),
                    "message": match.group("message").strip(),
                }
            else:
                match = CMAKE_DIAGNOSTIC.match(line)

                if match is not None:
                    entry = {
                        "severity": "error" if match.group("severity") == "Error" else "warning",
                        "file": "CMake",
                        "line": 0,
                        "column": 0,
                        "code": "CMAKE",
                        "message": match.group("message").strip(" :"),
                    }

        if entry is not None:
            key = (entry["severity"], entry["file"], entry["line"], entry["column"],
                   entry["code"], entry["message"])

            if key not in seen:
                seen.add(key)
                diagnostics.append(entry)

    return diagnostics


# ============================================================================
# Reporting
# ============================================================================

def DescribeLocation(diagnostic):
    location = diagnostic["file"]

    if diagnostic["line"] > 0:
        location = location + ":" + str(diagnostic["line"])

        if diagnostic["column"] > 0:
            location = location + ":" + str(diagnostic["column"])

    return location


def WriteReport(reportPath, summary, diagnostics):
    errors = [entry for entry in diagnostics if entry["severity"] == "error"]
    warnings = [entry for entry in diagnostics if entry["severity"] == "warning"]

    lines = []
    lines.append("# Build Report — " + summary["result"])
    lines.append("")
    lines.append("| | |")
    lines.append("| --- | --- |")
    lines.append("| Build id | `" + summary["buildId"] + "` |")
    lines.append("| Configuration | " + summary["configuration"] + " |")
    lines.append("| Mode | " + ("Clean" if summary["isClean"] else "Incremental") + " |")
    lines.append("| Commit | `" + summary["commitHash"] + "` |")
    lines.append("| Started | " + summary["startedUtc"] + " |")
    lines.append("| Duration | " + format(summary["durationSeconds"], ".1f") + "s |")
    lines.append("| Errors | " + str(len(errors)) + " |")
    lines.append("| Warnings | " + str(len(warnings)) + " |")
    lines.append("| Output | `" + summary["outputDirectory"] + "` |")

    runtimeData = summary["runtimeData"]
    lines.append("| Runtime data | " + str(runtimeData["copied"]) + " copied, " +
                 str(runtimeData["pruned"]) + " pruned, " +
                 str(runtimeData["unchanged"]) + " unchanged |")
    lines.append("")

    if errors:
        lines.append("## Errors")
        lines.append("")

        for diagnostic in errors[:REPORT_ERROR_LIMIT]:
            lines.append("- **" + DescribeLocation(diagnostic) + "** — " +
                         diagnostic["code"] + ": " + diagnostic["message"])

        if len(errors) > REPORT_ERROR_LIMIT:
            lines.append("- … " + str(len(errors) - REPORT_ERROR_LIMIT) + " more.")

        lines.append("")

    if warnings:
        lines.append("## Warnings")
        lines.append("")

        for diagnostic in warnings[:REPORT_ERROR_LIMIT]:
            lines.append("- " + DescribeLocation(diagnostic) + " — " +
                         diagnostic["code"] + ": " + diagnostic["message"])

        if len(warnings) > REPORT_ERROR_LIMIT:
            lines.append("- … " + str(len(warnings) - REPORT_ERROR_LIMIT) + " more.")

        lines.append("")

    lines.append("Full output: `Build.log`. Machine-readable diagnostics: `Diagnostics.json`.")
    lines.append("")

    with open(reportPath, "w", encoding="utf-8") as reportFile:
        reportFile.write("\n".join(lines))


# ============================================================================
# Entry point
# ============================================================================

def ParseArguments():
    parser = argparse.ArgumentParser(
        prog="Build.py", description="Build the ChillCore desktop target.")
    parser.add_argument("--configuration", choices=["Debug", "Release"], default="Debug")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--build-id", dest="useBuildId", action="store_true")

    return parser.parse_args()


def Run():
    arguments = ParseArguments()
    exitCode = EXIT_SUCCESS

    cmakePath = ValidateToolchain()

    if cmakePath is None:
        exitCode = EXIT_MISSING_TOOLCHAIN
    else:
        startedUtc = datetime.datetime.now(datetime.timezone.utc)
        startTime = time.monotonic()

        commitHash = GetCommitHash()
        buildId = GenerateBuildId() if arguments.useBuildId else UNLABELLED
        timestamp = startedUtc.strftime("%Y-%m-%d %H:%M:%S UTC")

        WriteGeneratedHeader(buildId, commitHash, arguments.configuration,
                             timestamp if arguments.useBuildId else "Unknown")

        outputDirectory = os.path.join(LOGS_ROOT, buildId)

        if os.path.isdir(outputDirectory):
            shutil.rmtree(outputDirectory, ignore_errors=True)

        os.makedirs(outputDirectory, exist_ok=True)

        if arguments.clean and os.path.isdir(CMAKE_BINARY_DIRECTORY):
            print("Clean: removing " + CMAKE_BINARY_DIRECTORY)
            shutil.rmtree(CMAKE_BINARY_DIRECTORY, ignore_errors=True)

        logLines = []
        logLines.append("ChillCore build " + buildId)
        logLines.append("Configuration: " + arguments.configuration)
        logLines.append("CMake: " + cmakePath)
        logLines.append("")

        exitCode = RunCapturing([cmakePath, "--preset", CONFIGURE_PRESET], logLines)

        if exitCode == 0:
            exitCode = RunCapturing(
                [cmakePath, "--build", "--preset", BUILD_PRESETS[arguments.configuration]],
                logLines)

        if exitCode != 0:
            exitCode = EXIT_BUILD_FAILED

        # Only a successful build has an executable worth placing data beside.
        dataSummary = {"copied": 0, "pruned": 0, "unchanged": 0}

        if exitCode == EXIT_SUCCESS:
            dataSummary = CopyRuntimeData(arguments.configuration, logLines)

        durationSeconds = time.monotonic() - startTime
        diagnostics = ParseDiagnostics(logLines)
        errorCount = len([entry for entry in diagnostics if entry["severity"] == "error"])
        warningCount = len(diagnostics) - errorCount

        summary = {
            "buildId": buildId,
            "isLabelled": arguments.useBuildId,
            "commitHash": commitHash,
            "configuration": arguments.configuration,
            "isClean": arguments.clean,
            "result": "Success" if exitCode == EXIT_SUCCESS else "Failed",
            "exitCode": exitCode,
            "startedUtc": timestamp,
            "durationSeconds": round(durationSeconds, 2),
            "errorCount": errorCount,
            "warningCount": warningCount,
            "cmakePath": cmakePath,
            "configurePreset": CONFIGURE_PRESET,
            "buildPreset": BUILD_PRESETS[arguments.configuration],
            "outputDirectory": GetOutputDirectory(arguments.configuration),
            "runtimeData": dataSummary,
        }

        with open(os.path.join(outputDirectory, "Build.log"), "w",
                  encoding="utf-8", errors="replace") as logFile:
            logFile.write("\n".join(logLines))

        diagnosticsDocument = dict(summary)
        diagnosticsDocument["diagnostics"] = diagnostics

        with open(os.path.join(outputDirectory, "Diagnostics.json"), "w",
                  encoding="utf-8") as diagnosticsFile:
            json.dump(diagnosticsDocument, diagnosticsFile, indent=4)

        WriteReport(os.path.join(outputDirectory, "Report.md"), summary, diagnostics)

        print("")
        print(summary["result"] + ": " + str(errorCount) + " errors, " +
              str(warningCount) + " warnings in " + format(durationSeconds, ".1f") + "s.")
        print("Report: " + os.path.join(outputDirectory, "Report.md"))

    return exitCode


if __name__ == "__main__":
    try:
        sys.exit(Run())
    except KeyboardInterrupt:
        sys.exit(EXIT_INTERNAL_FAILURE)
