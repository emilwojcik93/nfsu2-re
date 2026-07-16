# AI Context — NFSU2 Rumble/Force-Feedback Event Mapping Project

This file exists so a fresh Claude Code (or any AI assistant) session on the dev
machine can pick up this project with full context, without re-deriving
everything from scratch.

## Goal

Need for Speed Underground 2 (PC port) has working DirectInput force-feedback
rumble, but it only triggers on **surface-type transitions** (asphalt ↔
off-road). Crashes, drifts, nitro use, wall scrapes, and other events that
plausibly should trigger rumble do not. Goal: find out why, and extend it.

## How we got here (prior session, different machine)

1. Player has a ROG Ally X handheld running a 1:1 copy of NFSU2 (scp'd from a
   host called `fractal`), installed at `C:\Games\NFSU2`.
2. Followed a Steam Community guide (originally written for Euro Truck
   Simulator 2, id 165238623) for enabling DirectInput force-feedback with
   Xbox-style pads via **"Force Feedback Driver for XInput" by Masahiko
   Morii** — a small usermode COM DLL (`xiffd.dll`, CLSID
   `{FFB10360-5623-49AA-BD51-B321DB9625CE}`), NOT a kernel driver. No driver
   signing is involved anywhere in this mechanism.
3. Root cause of "rumble not working on Ally X" was diagnosed as a missing
   registry whitelist entry: DirectInput looks up
   `HKLM\SYSTEM\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_xxxx&PID_xxxx`
   per controller VID/PID to find which COM object handles force feedback.
   Ally X's built-in pad enumerates as `VID_0B05&PID_1B4C` (ASUS), which
   wasn't in Morii's whitelist (only Microsoft/HORI/Mad Catz/Saitek VIDs
   were). Fix: cloned the registry subtree from the genuine "Xbox 360
   Controller for Windows" entry (`VID_045E&PID_028E`) onto
   `VID_0B05&PID_1B4C` — every whitelisted device's registry block turned out
   to be a byte-for-byte identical template, so this was a safe, mechanical,
   fully reversible fix (just delete the cloned key to undo). No reboot
   needed — DirectInput reads this registry key fresh per enumeration.
4. Result: rumble now works in NFSU2 on the Ally X, confirmed by the user —
   **but only for the surface-type event**, which is the actual subject of
   this project.

**Why this matters for the current project:** the FF plumbing (game →
DirectInput → xiffd.dll → physical motor) is confirmed fully functional
end-to-end. The remaining problem is entirely about **which in-game events
call into that plumbing** — this is a game-binary/event-wiring question, not
a driver/OS/registry question. Nothing about drivers or signing is relevant
to the rest of this project.

## Research findings (this is what justifies the whole approach)

- **xan1242** (maintainer of `NFS-XtendedInput` / `NFSU-XtendedInput`, the
  community XInput-support mods for Black Box-era NFS games), on
  [NFS-XtendedInput#10](https://github.com/xan1242/NFS-XtendedInput/issues/10),
  2022-04-30: *"Right now all I know is that PC version has rumble code. How
  it behaves, I have no idea... If necessary I'll code it up from scratch. It
  doesn't seem very difficult to get right."* — confirms the PC binary
  genuinely contains rumble/FF code paths (inherited from wheel support),
  it's not stripped out entirely.
- Neither `NFS-XtendedInput` nor `NFSU-XtendedInput` ever implemented rumble
  (both READMEs say "console control feature parity, except rumble") — dead
  end as reference code, but confirms nobody's solved this publicly yet.
- **[ThirteenAG/WidescreenFixesPack#799](https://github.com/ThirteenAG/WidescreenFixesPack/issues/799)**
  (NFS Most Wanted, same Black Box engine family): identical symptom —
  only ~2 triggers work (high-speed crash impact, grass surface — "wrong
  motor"), closed `wontfix`. Same pattern across multiple Black Box PC ports,
  strongly suggesting a franchise-wide, not NFSU2-specific, PC-port
  limitation in how rumble events were wired (likely: console versions had
  many more rumble call sites wired to their native pad rumble APIs; PC port
  only wired a couple of events to the DirectInput-FF path built primarily
  for wheel owners).
- **[yugecin/nfsu2-re](https://github.com/yugecin/nfsu2-re)** — the
  community IDA-based reverse-engineering project for NFSU2 (this is the
  repo forked for this project, see below). Grepped `docs/funcs.html`,
  `vars.html`, `structs.html`, `enums.html` for `rumble` / `forcefeedback` /
  `ffb` / `vibrat` — **zero hits**. Nobody has publicly named or mapped the
  FF trigger call sites in this codebase yet. This is genuinely
  undocumented/unexplored territory, not a solved problem waiting to be
  flipped on.

## Repo setup

- Upstream: https://github.com/yugecin/nfsu2-re (hobby RE project, ~310
  commits, IDA-derived symbol DB generated from `SPEED2.idc`, C hook
  injection files compiled as a VS2005-era project)
- Fork: https://github.com/emilwojcik93/nfsu2-re (`origin` remote)
- Local clone: `C:\Users\ewojcik\dev\nfsu2-re`
- Branch: `feature/rumble-ff-event-mapping` (pushed to origin, tracked)
- Relevant existing structure:
  - `nfsu2-re-hooks/` — ~40 existing hook `.c` files (UI, filesystem,
    networking, hashing patches) built via `nfsu2-re-hooks.vcproj`. No
    existing hook touches input/FF — this project would add a new one here,
    e.g. `nfsu2-re-hooks/hook-ff-event-mapping.c`.
  - `d3d9-dinput-stuff/main.c` — NOT a hook; it's a tiny helper that just
    prints DirectX/DirectInput constant values, used to help build the IDA
    symbol table. Not directly useful beyond that.
  - `docs/` — generated documentation site (funcs/vars/structs/enums +
    blog), built via `docs/Makefile` from the IDA `.idc` export.
  - `SPEED2.CT` (repo root) — a Cheat Engine table, existing starting point
    for locating live vehicle-state structures in memory.
  - `SPEED2.idc` — the IDA symbol database export; load this into IDA to get
    the same named functions/structs the `docs/` site is generated from.

## Plan

1. **Investigation** — hook DirectInput's FF entry points
   (`IDirectInputEffect::Start`, `::SetParameters`,
   `IDirectInputDevice8::CreateEffect`) from an injected DLL (or a Frida
   script first, for faster iteration without recompiling), logging the
   return address (call site) and parameters every time one fires during
   real gameplay across many scenarios: surface transitions (known-working,
   baseline), high-speed crashes, drifts, nitro activation, wall scrapes,
   other collisions.
2. **Correlate** — cross-reference logged call-site addresses against the
   IDA `SPEED2.idc` symbol database (`funcs.html`) to identify which game
   function each call site lives in. This tells us whether it's a single
   generic dispatcher (easy to extend) or scattered inline calls (more work,
   but still tractable).
3. **Extend** — add new hook call sites (or patch the existing dispatcher)
   for the missing events, reusing the same FF COM interface/CLSID already
   confirmed working end-to-end via the registry fix above. Tune effect
   magnitude per event.
4. **Regression test** — confirm existing surface-change rumble still works
   post-patch, no crashes, reasonable per-event magnitude.
5. **Package** — as an ASI-loaded DLL via Ultimate ASI Loader, matching the
   existing `nfsu2-re-hooks` build conventions, so it's a drop-in file next
   to the game exe rather than requiring manual injection.

See `dev-notes/SETUP-CHECKLIST.md` for the tool list to install before
starting Phase 1.

## Open item / TODO

The actual NFSU2 game install (`C:\Games\NFSU2` on the original machine, a
1:1 copy scp'd from `fractal`) has **not** been transferred to this dev
machine yet — testing requires the real game running here. This needs to
happen separately (same scp-from-fractal approach, or copy from the Ally
X/original machine) before Phase 1 investigation can start for real.
