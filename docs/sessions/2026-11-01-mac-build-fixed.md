# 2026-11-01: the Mac build mended after a merge

The Build workflow failed on `main` after PR #6 (`a0b9f93`): the Mac and
Linux app builds stopped at `varyReplaces`, `run (bool)` and `more` - names
from code that should no longer have been there.

## What happened

`claude/blissful-bohr-lqwr1j` held both sessions' work (0049-0057) at
`58bcba8`. Meanwhile `main` took PR #8 (the older copy of
`claude/adoring-feynman-bihxp2`, which already had 0048 merged in) and PR #7
(AutoCC 1.3). Merging that `main` back into the branch (`dfa20a8`) kept
`main`'s side of every Miderator file both had touched: the More button
(gone in 0052) came back, `varyReplaces` (0055) and Clear All (0057) went,
the wording went back to before 0054, and the docs lost 0049-0057. The rest
of 0055 still used `varyReplaces`, so it no longer compiled. The CI run on
`dfa20a8` had been cancelled, so nothing showed it before PR #6 merged.

## The mend

- Every file the merge got wrong put back as it was at `58bcba8`:
  `Source/App/` (BlocksPanel, Controller, GeneratorPanel, MainComponent,
  Panels), `CLAUDE.md`, `README.md`, `docs/ARCHITECTURE.md`, `docs/SHARED.md`
  and both READMEs under `docs/`.
- AutoCC 1.3, `main`'s only new work, kept as it came: `AutoCC.*`,
  `Perform.cpp`, `TestScore.cpp` and the decision. Its entries added back to
  the docs. Its decision was numbered 0049, as the Escape decision is: it is
  0058 now. The shared `AutoCC.h` still says "decision 0049" until Noterator
  renumbers it and the file is synced.

## Not done yet

- Noterator probably has the same 0049 clash (its PR #9 used 0049-0055, its
  PR #11 AutoCC 0049): renumber there, then sync `AutoCC.h`.
- Lesson: when a merge of `main` has conflicts in files both sides changed,
  diff the result against the branch before it (`git diff <branch-before>
  <merge>`) - anything there that `main` did not mean to bring is a revert.
