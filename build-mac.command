#!/bin/bash
# Builds Miderator on this Mac and puts it in Applications (decision 0045).
#
# Double-click this file in the Finder. The first time, macOS may say it
# cannot check it: right-click it, choose Open, then Open again.
#
# It keeps its own copy of the code in ~/Developer/Miderator, brings it up to
# date from GitHub each time, builds the app there and copies it into
# Applications. Nothing is sent anywhere; GitHub's build minutes are not used.
#
# What it needs, once: Apple's Command Line Tools (it offers to install them)
# and Homebrew's cmake and ninja (it installs those if Homebrew is there).

APP="Miderator"
REPO="https://github.com/KallumS/Miderator.git"
SOURCE="$HOME/Developer/$APP"

set -u
say() { printf '\n\033[1m%s\033[0m\n' "$*"; }
fail() { printf '\n\033[31m%s\033[0m\n\n' "$*"; read -r -p "Press Return to close this window. " _; exit 1; }

if [ "$(uname)" != "Darwin" ]; then fail "This builds the Mac app, so it has to run on a Mac."; fi
if [ "$(uname -m)" != "arm64" ]; then fail "$APP is built for Apple silicon Macs (M1 or later); this Mac has an Intel processor."; fi

say "Building $APP on this Mac"

# 1. Apple's Command Line Tools: the compiler, and git.
if ! xcode-select -p >/dev/null 2>&1; then
    xcode-select --install >/dev/null 2>&1
    fail "macOS is now offering to install Apple's Command Line Tools. Click Install, wait for it to finish (it can take a while), then double-click this file again."
fi

# 2. cmake and ninja, from Homebrew.
for brewDir in /opt/homebrew/bin /usr/local/bin; do
    [ -x "$brewDir/brew" ] && PATH="$brewDir:$PATH"
done
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    if ! command -v brew >/dev/null 2>&1; then
        open "https://brew.sh"
        fail "This needs Homebrew, which installs cmake and ninja. Its page is open in your browser: copy the one line under \"Install Homebrew\" into Terminal and press Return. When it has finished, double-click this file again."
    fi
    say "Installing cmake and ninja with Homebrew..."
    brew install cmake ninja || fail "Homebrew could not install cmake and ninja (see above)."
fi

# 3. The code: fetched the first time, brought up to date after that.
BRANCH="main"
echo
read -r -p "Which version? Press Return for the latest ($BRANCH), or type a branch name: " answer
[ -n "$answer" ] && BRANCH="$answer"

if [ -d "$SOURCE/.git" ]; then
    say "Bringing the code up to date ($BRANCH)..."
    git -C "$SOURCE" fetch --quiet origin "$BRANCH" || fail "Could not fetch '$BRANCH' from GitHub. Is the name right, and is the Mac online?"
    git -C "$SOURCE" checkout --quiet --force -B "$BRANCH" "origin/$BRANCH" || fail "Could not switch to '$BRANCH'."
else
    say "Fetching the code ($BRANCH)..."
    mkdir -p "$(dirname "$SOURCE")"
    git clone --quiet --branch "$BRANCH" "$REPO" "$SOURCE" || fail "Could not fetch '$BRANCH' from GitHub. Is the name right, and is the Mac online?"
fi
echo "Version: $(git -C "$SOURCE" log -1 --format='%h, %cd - %s' --date=format:'%d %b %Y %H:%M')"

# 4. Build. The first time also downloads JUCE; it takes a while. Later
#    builds only redo what changed.
say "Building (the first time takes 10 to 30 minutes)..."
cd "$SOURCE" || fail "Could not open $SOURCE."
cmake -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 || fail "The build could not be set up (see above)."
cmake --build build-mac --target "$APP" || fail "The build failed (see above). Copy the last lines into a message to Claude."

# 5. Into Applications, in place of the one there.
BUILT="build-mac/${APP}_artefacts/Release/$APP.app"
[ -d "$BUILT" ] || fail "The build finished but $BUILT is missing."
codesign --force --deep --sign - "$BUILT" >/dev/null 2>&1
osascript -e "if application \"$APP\" is running then tell application \"$APP\" to quit" >/dev/null 2>&1
rm -rf "/Applications/$APP.app"
cp -R "$BUILT" /Applications/ || fail "Could not copy $APP into Applications."

say "Done: $APP is in Applications, built from $BRANCH."
read -r -p "Press Return to open it, or close this window. " _
open "/Applications/$APP.app"
