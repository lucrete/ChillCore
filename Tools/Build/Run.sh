#!/usr/bin/env bash
#
# Launch the built desktop application and collect its log.
#
# The application prints to stdout, which is lost if it faults, and it must
# start in Code/App for its Data-relative asset paths to resolve. This script
# handles both, and runs under the Windows debugger when one is available so a
# crash yields a stack rather than an exit code.
#
# See DevDocs/ChillCoreDevDocs/docs/Planning/BuildPipelinePlan.md.

set -u

EXIT_MISSING_TOOLCHAIN=3

scriptDirectory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
repositoryRoot="$(cd "$scriptDirectory/../.." && pwd -P)"

configuration="Debug"
runSeconds=10
isInteractive=false
doPause=""

PrintUsage()
{
    cat <<'USAGE'
Usage: Run.sh [options]

  --debug            Run the Debug build. (default)
  --release          Run the Release build.
  --seconds <n>      Stop the application after n seconds. (default 10)
  --pause            Wait for a keypress before exiting.
  --no-pause         Exit immediately.
  -h, --help         Print this message.

The log is written to Build/Logs/Run.log and echoed. A crash adds the faulting
stack to the same file.
USAGE
}

if [ $# -eq 0 ] && [ -t 0 ] && [ -t 1 ]
then
    isInteractive=true
fi

while [ $# -gt 0 ]
do
    case "$1" in
        --debug)    configuration="Debug" ;;
        --release)  configuration="Release" ;;
        --seconds)  shift; runSeconds="${1:-10}" ;;
        --pause)    doPause=true ;;
        --no-pause) doPause=false ;;
        -h|--help)  PrintUsage; exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            PrintUsage >&2
            exit 2
            ;;
    esac
    shift
done

if [ -z "$doPause" ]
then
    doPause=$isInteractive
fi

Finish()
{
    if [ "$doPause" = true ]
    then
        echo ""
        read -r -n 1 -s -p "Press any key to close."
        echo ""
    fi

    exit "$1"
}

executablePath="$repositoryRoot/Build/x64/$configuration/ChillCore.exe"
workingDirectory="$repositoryRoot/Code/App"
logDirectory="$repositoryRoot/Build/Logs"
logPath="$logDirectory/Run.log"

if [ ! -f "$executablePath" ]
then
    echo "No $configuration build found at $executablePath." >&2
    echo "  Run Tools/Build/Build.sh first." >&2
    Finish $EXIT_MISSING_TOOLCHAIN
fi

mkdir -p "$logDirectory"

# The debugger is optional. Without it a fault still ends the run, but the
# reason is only an exit code.
debuggerPath="/mnt/c/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"

if [ ! -f "$debuggerPath" ]
then
    debuggerPath="C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"
fi

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

cd "$workingDirectory" || Finish $EXIT_MISSING_TOOLCHAIN

echo "Running $configuration for ${runSeconds}s from $workingDirectory"
echo ""

if [ -f "$debuggerPath" ]
then
    # 'g' runs until the application exits or faults; a fault breaks back in
    # and the stack is dumped before quitting. The debugger has no run-for-n-
    # seconds command, so a clean run is ended by the outer timeout instead —
    # which is why timing out here is success, not failure.
    timeout "$runSeconds" "$debuggerPath" -g -G \
        -c "g; .echo ---FAULT-STACK---; kn 12; q" \
        "$(ToWindowsPath "$executablePath")" > "$logPath" 2>&1
    runExitCode=$?

    if [ "$runExitCode" -eq 124 ]
    then
        runExitCode=0
    fi
else
    timeout "$runSeconds" "$executablePath" > "$logPath" 2>&1
    runExitCode=$?
fi

# The debugger's module-load chatter is noise; the application's own output is
# what the log is for.
grep -v "^ModLoad\|^NatVis script" "$logPath" > "$logPath.tmp" && mv "$logPath.tmp" "$logPath"

cat "$logPath"

echo ""
faultCount="$(grep -c "Access violation\|Assertion failed\|---FAULT-STACK---" "$logPath")"

if [ "$faultCount" -gt 0 ]
then
    echo "Faulted. Stack is in $logPath"
else
    echo "Ran without faulting. Log: $logPath"
fi

Finish "$runExitCode"
