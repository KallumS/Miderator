#!/usr/bin/env python3
"""
Every C++ string literal with a character beyond ASCII - an ellipsis, a dash,
a curly quote, a key symbol - must be read through ui() (decision 0054):
juce::String takes a plain literal as ASCII and garbles the rest (and asserts
in a Debug build). This finds any that are not.

    python3 tools/check_ui_text.py      # from the top of the repository
"""
import pathlib, re, sys

literal = re.compile(r'"(?:[^"\\\n]|\\.)*"')
bad = []
for path in sorted(pathlib.Path("Source").rglob("*.[ch]*")):
    text = path.read_text(encoding="utf-8")
    for m in literal.finditer(text):
        if all(ord(c) < 128 for c in m.group()):
            continue
        # Inside an open ui ( ... ) or fromUTF8 ( ... )?
        before = text[: m.start()]
        call = max(before.rfind("ui ("), before.rfind("fromUTF8 ("))
        depth = 0 if call < 0 else before[call:].count("(") - before[call:].count(")")
        if call < 0 or depth <= 0:
            line = text.count("\n", 0, m.start()) + 1
            bad.append(f"{path}:{line}: {m.group()}")
for b in bad:
    print(b)
print(f"{len(bad)} literal(s) with non-ASCII text outside ui()" if bad else "All non-ASCII text goes through ui().")
sys.exit(1 if bad else 0)
