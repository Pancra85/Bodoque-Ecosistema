# Titr8

Bodoque V1 (RP2040) acid groovebox firmware, forked from MaquinaAcida. PlatformIO + Mozzi.

**Before doing anything, read the full context:**
`F:\[WORKING]\PICO\Bodoque-Ecosistema\Bodoque-Documentation\Titr8-Refactor.md`
(ecosystem overview: `Bodoque-Ecosistema.md` in the same folder)

Key rules:
- `C:\Titr8` is the working copy (build/upload here). The repo copy at `F:\[WORKING]\PICO\Bodoque-Ecosistema\Titr8` is for committing: sync it by copying everything EXCEPT `.pio/` and `.vscode/` (copying `.pio` froze the PC once).
- Do NOT run PlatformIO builds from Claude's shell — the user builds/uploads with the VS Code PlatformIO buttons. Ask them to build and paste errors.
- `[WORKING]` brackets break PowerShell paths: use `-LiteralPath` or Bash.
- Refactor rule: keep behavior identical unless the user asks for a change. The volume bug (`mix + delayedPadBass << 6` in voices.cpp) is deliberately deferred.
- User prefers direct, concise answers.
