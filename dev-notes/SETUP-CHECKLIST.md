# Dev Environment Setup Checklist

Manual installs needed before development starts. NFSU2 is a **32-bit**
game — every tool/library below must be the 32-bit build where it matters.

- [x] **Visual Studio 2022 Community** — Desktop C++ workload. NOT installed
      by default on this machine's VS ("18" generation) despite being
      present — had to run `vs_installer.exe modify --add
      Microsoft.VisualStudio.Workload.NativeDesktop` explicitly. Its native
      toolset is `PlatformToolset=v145`, not `v143`/`v142`; the old
      `.vcproj` couldn't be silently upgraded (command-line `/Upgrade` is
      GUI-gated on this VS generation), so a hand-written `.vcxproj` was
      used instead (see `AI-CONTEXT.md` Progress Log).
- [x] **MinHook** — https://github.com/TsudaKageyu/minhook — vendored under
      `nfsu2-re-hooks/minhook/` (v1.3.4, static `libMinHook.x86.lib`).
      In use by `hook-ff-event-mapping.c`.
- [ ] **x32dbg** — the 32-bit variant from the x64dbg suite,
      https://x64dbg.com — installed via `winget` on this machine, but its
      CLI aliases didn't register (needs a fresh shell to pick up PATH
      changes from the winget shim, or manual PATH entry). Not used yet this
      session — Cheat Engine's live disassembly covered everything needed
      so far.
- [x] **Cheat Engine** — https://cheatengine.org — used extensively this
      session via `cheatengine-mcp-bridge` (see project root) for live
      disassembly, `find_call_references`, and non-intrusive hardware
      breakpoints. **Critical**: Settings → Extra → disable "Query memory
      region routines" (BSOD risk with DBVM) — verify this is off before any
      `scan_all`/`aob_scan` work.
- [ ] **IDA Free** — https://hex-rays.com/ida-free — installed, but
      **`SPEED2.idc` is missing from this machine's copy of the repo**
      (only `SPEED2.CT` came over). Never actually used this session; live
      Cheat Engine disassembly substituted for it. `docs/funcs.html` (the
      pre-generated static docs) was used for lookups instead, with the
      caveat that it has real undocumented gaps (see `AI-CONTEXT.md`).
- [x] **Ultimate ASI Loader** —
      https://github.com/ThirteenAG/Ultimate-ASI-Loader — already correctly
      installed on this machine's game copy (`dinput8.dll`, v9.7.2, in
      `NFSU2\`), loading from `NFSU2\SCRIPTS\`. `nfsu2-re-hooks.asi` builds
      straight into that folder now.
- [ ] *(Optional, not used)* **Frida** — https://frida.re — installed
      (`pip install frida-tools`) but not needed; the compiled MinHook path
      plus Cheat Engine covered the whole investigation without it.
- [x] **Git** + **GitHub CLI (`gh`)** — both present, `gh auth status` shows
      logged in as `emilwojcik93` (matches the fork owner). **Caveat**: this
      machine's copy of `nfsu2-re` was a plain file copy, NOT an actual
      `git clone` — no `.git` directory existed. Re-attached to the real
      history via `git init` + `git remote add origin` + `git fetch` +
      `git reset <remote-branch>` (mixed reset only moves HEAD/index, never
      touches working-tree files — safe to do without losing any local
      changes already on disk). If setting up fresh elsewhere, just
      `git clone git@github.com:emilwojcik93/nfsu2-re.git` properly instead.
- [x] **NFSU2 game install itself** — done, at
      `C:\Users\Endurable4847\NFSU2-project\NFSU2` (see `AI-CONTEXT.md` Open
      Item, now resolved).

## Repo already prepared (in this archive)

- Fork: https://github.com/emilwojcik93/nfsu2-re
- Upstream: https://github.com/yugecin/nfsu2-re
- Branch: `feature/rumble-ff-event-mapping` (checked out, pushed, tracked)
- Read `dev-notes/AI-CONTEXT.md` first for full background before starting
  any work.
