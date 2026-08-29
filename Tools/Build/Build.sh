#!/usr/bin/env bash
#
# ChillCore build entry point.
#
# Double-clicked from Windows Explorer, this prompts for the build parameters
# and holds the window open. Given any argument, it runs unattended for Claude
# or a build runner.
#
# See DevDocs/ChillCoreDevDocs/docs/Planning/BuildPipelinePlan.md.

set -u

EXIT_INVALID_USAGE=2
EXIT_MISSING_TOOLCHAIN=3

scriptDirectory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"

useBuildId=false
configuration="Debug"
isClean=false
isInteractive=false
doPause=""

# ============================================================================
# Usage
# ============================================================================

PrintUsage()
{
    cat <<'USAGE'
Usage: Build.sh [options]

  --no-id            Do not label this build. Output overwrites
                     Build/Logs/Unlabelled. (default)
  --build-id         Label this build. Output goes to Build/Logs/<buildId>
                     and the identifier is compiled into the binary.

  --debug            Debug configuration. (default)
  --release          Release configuration.

  --incremental      Build only what changed. (default)
  --clean            Discard the build tree and rebuild everything.

  --pause            Wait for a keypress before exiting.
  --no-pause         Exit immediately.
  -h, --help         Print this message.

With no options, every parameter is prompted for. With any option, nothing is
prompted for and unspecified parameters take their defaults.
USAGE
}

# ============================================================================
# Argument parsing
# ============================================================================

ParseArguments()
{
    while [ $# -gt 0 ]
    do
        case "$1" in
            --no-id)        useBuildId=false ;;
            --build-id)     useBuildId=true ;;
            --debug)        configuration="Debug" ;;
            --release)      configuration="Release" ;;
            --incremental)  isClean=false ;;
            --clean)        isClean=true ;;
            --pause)        doPause=true ;;
            --no-pause)     doPause=false ;;
            -h|--help)      PrintUsage; exit 0 ;;
            *)
                echo "Unknown option: $1" >&2
                echo "" >&2
                PrintUsage >&2
                exit $EXIT_INVALID_USAGE
                ;;
        esac
        shift
    done
}

# ============================================================================
# Prompting
# ============================================================================

# Prompt with a default. Answering nothing takes the default.
AskYesNo()
{
    local question="$1"
    local defaultAnswer="$2"
    local hint="y/N"
    local reply=""

    if [ "$defaultAnswer" = "y" ]
    then
        hint="Y/n"
    fi

    read -r -p "$question [$hint] " reply
    reply="${reply:-$defaultAnswer}"

    case "$reply" in
        [Yy]*) return 0 ;;
        *)     return 1 ;;
    esac
}

PromptForParameters()
{
    echo "ChillCore build"
    echo ""

    if AskYesNo "Label this build with a build id?" "n"
    then
        useBuildId=true
    fi

    if AskYesNo "Release configuration?" "n"
    then
        configuration="Release"
    fi

    if AskYesNo "Clean rebuild?" "n"
    then
        isClean=true
    fi

    echo ""
}

# ============================================================================
# Python resolution
#
# The build needs the *Windows* interpreter: it drives Windows CMake and
# Visual Studio. The .exe names are tried first so that a WSL shell, where a
# Linux python3 is also on PATH, still reaches the Windows one through interop.
# CMake and the compiler are validated by Build.py.
# ============================================================================

ResolvePython()
{
    local candidate=""

    for candidate in python.exe py.exe python python3 py
    do
        if command -v "$candidate" >/dev/null 2>&1
        then
            if "$candidate" -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" >/dev/null 2>&1
            then
                echo "$candidate"
                return 0
            fi
        fi
    done

    return 1
}

# Convert a shell path to a Windows path for the Windows interpreter. Git Bash
# usually converts arguments on the way out, but not reliably for every
# invocation form, and WSL does not convert at all.
ToWindowsPath()
{
    local path="$1"

    if command -v cygpath >/dev/null 2>&1
    then
        cygpath -w "$path"
    elif command -v wslpath >/dev/null 2>&1
    then
        wslpath -w "$path"
    else
        echo "$path"
    fi
}

# ============================================================================
# Main
# ============================================================================

if [ $# -eq 0 ] && [ -t 0 ] && [ -t 1 ]
then
    isInteractive=true
fi

ParseArguments "$@"

if [ -z "$doPause" ]
then
    doPause=$isInteractive
fi

if [ "$isInteractive" = true ]
then
    PromptForParameters
fi

# Hold the window open regardless of how the build ended, so a double-click
# leaves the result on screen.
Finish()
{
    local code="$1"

    if [ "$doPause" = true ]
    then
        echo ""
        read -r -n 1 -s -p "Press any key to close."
        echo ""
    fi

    exit "$code"
}

pythonCommand="$(ResolvePython)" || {
    echo "Missing toolchain: Python 3.8 or newer was not found." >&2
    echo "  Install it from python.org and make sure it is on PATH." >&2
    Finish $EXIT_MISSING_TOOLCHAIN
}

buildArguments=(--configuration "$configuration")

if [ "$useBuildId" = true ]
then
    buildArguments+=(--build-id)
fi

if [ "$isClean" = true ]
then
    buildArguments+=(--clean)
fi

# ============================================================================
# Virtual environment
#
# Build.py imports only the standard library today, so the environment buys
# isolation for what comes later — a report template engine, a test runner —
# rather than solving a problem that exists now. Creating it costs a few
# seconds once; using it costs nothing measurable per build, because the
# interpreter and its import search are the same either way.
# ============================================================================

virtualEnvironment="$scriptDirectory/.venv"

if [ ! -f "$virtualEnvironment/Scripts/python.exe" ] && [ ! -f "$virtualEnvironment/bin/python" ]
then
    echo "Creating the build virtual environment (first run only)."

    if ! "$pythonCommand" -m venv "$(ToWindowsPath "$virtualEnvironment")"
    then
        echo "Failed to create the virtual environment at $virtualEnvironment." >&2
        Finish $EXIT_MISSING_TOOLCHAIN
    fi
fi

if [ -f "$virtualEnvironment/Scripts/python.exe" ]
then
    pythonCommand="$virtualEnvironment/Scripts/python.exe"
elif [ -f "$virtualEnvironment/bin/python" ]
then
    pythonCommand="$virtualEnvironment/bin/python"
fi

buildScript="$scriptDirectory/Build.py"

buildScript="$(ToWindowsPath "$buildScript")"

"$pythonCommand" "$buildScript" "${buildArguments[@]}"
Finish $?
