# Titr8

Bodoque V1 (RP2040) acid groovebox firmware, forked from MaquinaAcida. PlatformIO + Mozzi.

**Before doing anything, read the full context:**
`DOC\Titr8-Refactor.md` (in this folder)
(ecosystem overview: `DOC\Bodoque-Ecosistema.md`)

Key rules:
- The repo lives at `C:\code\pico\Bodoque-Ecosistema` (moved from F: on 2026-11-10). Never put it back on drive F:: its disk stalls and froze git, copies and PlatformIO.
- This `Titr8` folder is the only working copy (`C:\Titr8` was retired on 2026-11-10). Build and upload from here.
- Do NOT run PlatformIO builds from Claude's shell — the user builds/uploads with the VS Code PlatformIO buttons. Ask them to build and paste errors.
- Refactor rule: keep behavior identical unless the user asks for a change. The volume bug (`mix + delayedPadBass << 6` in voices.cpp) is deliberately deferred.
- User prefers direct, concise answers.
