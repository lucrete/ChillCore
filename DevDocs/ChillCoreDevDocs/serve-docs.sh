#!/usr/bin/env bash
#
# Serve the ChillCore dev docs locally.
#
# Creates a Python virtual environment on first run, installs the pinned
# requirements into it, then starts the mkdocs development server. Later
# runs reuse the environment and start immediately.
#
# Written for a bash shell on Windows (Git Bash / MSYS2). Also runs on a
# POSIX shell if the docs are ever served from Linux or macOS.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR=".venv"
REQUIREMENTS="requirements.txt"
STAMP="$VENV_DIR/.requirements-stamp"
SERVE_ADDRESS="127.0.0.1:8000"

# Work from the script's own directory. Every path below is relative, which
# keeps Windows Python from having to interpret MSYS-style paths.
cd "$SCRIPT_DIR"

Fail()
{
    echo ""
    echo "ERROR: $1" >&2
    echo ""
    read -r -p "Press Enter to close..." _ || true
    exit 1
}

# ---------------------------------------------------------------
# Locate Python
# ---------------------------------------------------------------
# Windows installs expose the "py" launcher and "python"; "python3" usually
# does not exist. POSIX systems are the other way round. Try in that order.

PYTHON=()

if command -v py >/dev/null 2>&1 && py -3 --version >/dev/null 2>&1
then
    PYTHON=(py -3)
elif command -v python >/dev/null 2>&1 && python --version >/dev/null 2>&1
then
    PYTHON=(python)
elif command -v python3 >/dev/null 2>&1
then
    PYTHON=(python3)
else
    Fail "no Python found. Install Python 3 and make sure it is on your PATH."
fi

echo "Using $("${PYTHON[@]}" --version 2>&1)"

# ---------------------------------------------------------------
# Virtual environment
# ---------------------------------------------------------------
# Windows venvs put executables in Scripts/, POSIX venvs in bin/.

VenvPython()
{
    if [ -x "$VENV_DIR/Scripts/python.exe" ]
    then
        echo "$VENV_DIR/Scripts/python.exe"
    elif [ -x "$VENV_DIR/bin/python" ]
    then
        echo "$VENV_DIR/bin/python"
    fi
}

if [ -z "$(VenvPython)" ]
then
    echo "Creating virtual environment..."
    rm -rf "$VENV_DIR"

    if ! "${PYTHON[@]}" -m venv "$VENV_DIR"
    then
        # Some Linux distributions ship venv without ensurepip. Create the
        # environment without pip and bootstrap pip into it instead.
        echo "  ensurepip unavailable, bootstrapping pip..."
        rm -rf "$VENV_DIR"
        "${PYTHON[@]}" -m venv --without-pip "$VENV_DIR" \
            || Fail "could not create a virtual environment."

        command -v curl >/dev/null 2>&1 || Fail "curl not found, and pip could not be bootstrapped without it."
        curl -fsSL https://bootstrap.pypa.io/get-pip.py -o "$VENV_DIR/get-pip.py" \
            || Fail "could not download the pip bootstrap. Check your network connection."
        "$(VenvPython)" "$VENV_DIR/get-pip.py" --quiet || Fail "pip bootstrap failed."
        rm -f "$VENV_DIR/get-pip.py"
    fi

    [ -n "$(VenvPython)" ] || Fail "virtual environment was created but has no Python executable."
fi

VENV_PYTHON="$(VenvPython)"

# ---------------------------------------------------------------
# Requirements
# ---------------------------------------------------------------
# Reinstall only when requirements.txt has changed since the last run.

REQUIREMENTS_HASH="$(sha1sum "$REQUIREMENTS" | cut -d' ' -f1)"

if [ ! -f "$STAMP" ] || [ "$(cat "$STAMP")" != "$REQUIREMENTS_HASH" ]
then
    echo "Installing documentation requirements..."
    "$VENV_PYTHON" -m pip install --quiet --upgrade pip
    "$VENV_PYTHON" -m pip install --quiet -r "$REQUIREMENTS" \
        || Fail "requirements install failed. Check your network connection."
    echo "$REQUIREMENTS_HASH" > "$STAMP"
fi

# ---------------------------------------------------------------
# Serve
# ---------------------------------------------------------------

echo ""
echo "Serving docs at http://$SERVE_ADDRESS"
echo "Edits to files under docs/ reload automatically. Press Ctrl+C to stop."
echo ""

exec "$VENV_PYTHON" -m mkdocs serve --dev-addr "$SERVE_ADDRESS"
