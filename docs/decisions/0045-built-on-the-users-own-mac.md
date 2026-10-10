# 0045 - The Mac app can be built on the user's own Mac

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike). Adds to 0012, which
  still holds: CI builds and tests every push.

## Context

GitHub's Mac build minutes are limited (they ran out once, while the
repositories were private), and the user asked whether the app could be
built without GitHub.

## Decision

`build-mac.command` at the top of the repository, double-clicked in the
Finder (right-click > Open the first time). It:

1. checks for Apple's Command Line Tools and offers to install them;
2. installs cmake and ninja with Homebrew, or opens brew.sh if Homebrew is
   missing - Homebrew's installer asks for the password, so it is the one
   step left to the user;
3. keeps its own clone in `~/Developer/<App>` and brings it up to date,
   asking which branch (Return for `main`), so a version on a branch can be
   tried before it is merged;
4. builds the app as CI does (Release, arm64) - only the app, not the tests;
5. replaces the app in Applications, ad-hoc signed. Built on the Mac, it
   carries no "downloaded" mark, so it opens without right-click > Open.

## Consequences

- The first build downloads JUCE and takes a while; later ones only redo
  what changed.
- The tests are not run on the user's Mac: CI is still where they run.
- Rehearsed on Linux with stand-ins for the Mac's own tools (a fake `uname`,
  `xcode-select`, `cmake`...): first fetch, a branch, a wrong branch name.
  Not yet run on a Mac.
