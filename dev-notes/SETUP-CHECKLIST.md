# Dev Environment Setup Checklist

Manual installs needed before development starts. NFSU2 is a **32-bit**
game — every tool/library below must be the 32-bit build where it matters.

- [ ] **Visual Studio 2022 Community** — Desktop C++ workload. This repo's
      project files are old `.vcproj` (VS2005-era); VS2022 will offer to
      upgrade them on first open. If the upgrade wizard complains about a
      missing platform toolset, also install the **MSVC v141/v142** build
      tools component (VS2017/2019 toolsets) for compatibility.
- [ ] **MinHook** — https://github.com/TsudaKageyu/minhook — x86 inline
      hooking library, used to intercept the DirectInput FF calls. (Detours
      is a heavier alternative; MinHook is simpler for this scope.)
- [ ] **x32dbg** — the 32-bit variant from the x64dbg suite,
      https://x64dbg.com — for live debugging: breakpoints on `dinput8.dll`
      exports, call-stack inspection while playing.
- [ ] **Cheat Engine** — https://cheatengine.org — live memory inspection;
      the repo already ships `SPEED2.CT` as a starting cheat table for
      vehicle-state structures.
- [ ] **IDA Free** — https://hex-rays.com/ida-free — the repo's symbol
      database (`SPEED2.idc`) is IDA-format; loading it gives named
      functions/structs to cross-reference hook call-site addresses against.
- [ ] **Ultimate ASI Loader** —
      https://github.com/ThirteenAG/Ultimate-ASI-Loader — standard, clean
      way to auto-load a custom DLL into NFSU2 without manual injection.
      Drop a proxy DLL into the game folder; pick a free proxy-DLL slot —
      check what the game/other mods already use before choosing (e.g. on
      the original install, `dinput8.dll` was already taken by ReShade;
      `winmm.dll` or `version.dll` are common free choices).
- [ ] *(Optional, faster iteration)* **Frida** — https://frida.re — dynamic
      instrumentation; hook/log the FF calls from a Python script without
      recompiling C++ each time. Good for the initial "which call sites
      exist" investigation phase before committing to the compiled MinHook
      DLL.
- [ ] **Git** + **GitHub CLI (`gh`)** — repo is already cloned and configured
      on this machine if you're reading this from the prepared archive; if
      setting up fresh elsewhere, `gh auth login` then
      `git clone git@github.com:emilwojcik93/nfsu2-re.git`.
- [ ] **NFSU2 game install itself** — not included in this archive. Needs to
      be copied separately onto this dev machine before Phase 1 testing can
      start (see `AI-CONTEXT.md` Open Item).

## Repo already prepared (in this archive)

- Fork: https://github.com/emilwojcik93/nfsu2-re
- Upstream: https://github.com/yugecin/nfsu2-re
- Branch: `feature/rumble-ff-event-mapping` (checked out, pushed, tracked)
- Read `dev-notes/AI-CONTEXT.md` first for full background before starting
  any work.
