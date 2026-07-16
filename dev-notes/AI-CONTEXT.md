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
- Local clone (this machine): `C:\Users\Endurable4847\NFSU2-project\nfsu2-rumble-dev\nfsu2-re`
  — arrived as a file copy, not an actual `git clone` (`.git` was missing);
  re-attached to the real history this session (`git init` + `git remote add`
  + `git fetch` + `git reset <remote-branch>`, which only moves HEAD/index,
  never touches working-tree files, so nothing already on disk was clobbered).
- Branch: `feature/rumble-ff-event-mapping` (pushed to origin, tracked)
- **`SPEED2.idc` is NOT present in this machine's copy** — only `SPEED2.CT`
  is. `docs/funcs.html` is still present (pre-generated) and was usable for
  address lookups, but it has real gaps: a ~36 KB span
  (`0x5BF940`–`0x5C9020`) that contains ALL of the FF call sites this project
  found is entirely undocumented and nearest-symbol lookups against it are
  actively misleading (see Progress Log). Live disassembly via the Cheat
  Engine MCP bridge (see `cheatengine-mcp-bridge/` at the project root) was
  the reliable source of truth, not the generated docs, for this whole
  investigation.
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

## Progress Log (this session)

Game install transferred to this machine at
`C:\Users\Endurable4847\NFSU2-project\NFSU2` (resolves the old Open Item
below). Phase 1–2 of the Plan (Investigation, Correlate) are largely done;
Phase 3 (Extend) has not started yet.

### What was built

- `nfsu2-re-hooks/hook-ff-event-mapping.c` — MinHook-based hooks on
  `IDirectInputDevice8::CreateEffect`, `IDirectInputEffect::Start`,
  `::SetParameters`. Technique: create a throwaway `GUID_SysKeyboard` device
  via `DirectInput8Create`, steal its vtable (shared across all
  `IDirectInputDevice8`/`IDirectInputEffect` instances of the same backend
  class, so hooking via a dummy device also catches the game's real gamepad
  device), `MH_CreateHook`/`MH_EnableHook` on the relevant slots, release the
  dummy objects (hooked code stays resident in the owning DLL, unaffected by
  releasing the instance that revealed it). Logs caller return address +
  `DIEFFECT` fields + first 3 DWORDs of `lpvTypeSpecificParams` (generic dump,
  not decoded per-type) + the call's `HRESULT`.
  - **Important:** the actual `DirectInput8Create`/device-creation call must
    run on a spawned thread (`CreateThread`), not synchronously in `DllMain`.
    Doing it in `DllMain(DLL_PROCESS_ATTACH)` directly hit the Windows loader
    lock and crashed the process on the very first launch.
  - `MinHook` (v1.3.4) vendored under `nfsu2-re-hooks/minhook/` (`include/`,
    `lib/libMinHook.x86.lib` static). Project links it statically; no extra
    DLL needed in the game folder. Known cosmetic issue: `LNK4098` (LIBCMT
    vs the project's `/MDd`) — harmless for what MinHook does (own
    VirtualAlloc-based trampoline allocator, doesn't lean on the CRT), but
    the first thing to suspect if heap corruption ever shows up near this
    code.
- `nfsu2-re-hooks/hook-5BFEE0-ff-dispatcher-caller.c` — **written, currently
  disabled, do not re-enable.** See "Dead end" below.
- Toolchain: this machine's Visual Studio ("18", a newer/preview generation)
  ships **no C++ workload by default** — had to install
  `Microsoft.VisualStudio.Workload.NativeDesktop` via `vs_installer.exe
  modify` before anything would build. Its native `PlatformToolset` is
  `v145`, not `v143`/`v142` — the old VS2005 `.vcproj` can't be silently
  upgraded on this VS generation (the command-line `/Upgrade` path is
  GUI-gated now), so a hand-written `nfsu2-re-hooks.vcxproj` was created
  instead (`PlatformToolset=v145`), targeting `Debug|Win32`, output
  redirected straight to `NFSU2\SCRIPTS\nfsu2-re-hooks.asi`. Only `a_main.c`
  is an actual compiled translation unit; every other `.c` file in the
  project (including the two new hook files) is `ExcludedFromBuild` and
  pulled in via `#include` from `a_main.c` — matches this project's existing
  single-TU convention, not something introduced this session.

### Confirmed working (rumble fires)

- Surface-type transitions (asphalt ↔ off-road) — the original, known-good
  baseline.
- Crashes into public traffic cars, other racing cars — wait, see below,
  only *civilian* traffic and walls/objects confirmed; racer-vs-racer crash
  is unconfirmed/inconsistent, see "Confirmed NOT working."
- Crashes into walls and static objects (highway water barrels, dumpsters).
- Car reset (the "flip car back over" action).
- Two distinct intensity tiers on impacts, driven by impact speed — this is
  real, confirmed via the logged `lpvTypeSpecificParams` first DWORD
  actually varying (observed values from 0 up to ~7000+), not a fixed
  constant. `dwGain` itself is *always* `10000` (max) — the user's
  perception that "overall loudness" feels constant is also correct, that
  scalar never changes; the actual per-hit variation lives in the
  type-specific magnitude field instead.

### Confirmed NOT working (zero FF calls reach DirectInput at all)

- Menu/UI interactions: SMS, map, in-game menu.
- Points of interest (message/event markers on track).
- Nitro activation.
- Confirmed at the deepest level available: a non-intrusive Cheat Engine
  hardware breakpoint on the shared FF-trigger dispatcher (`0x5BFEE0`, see
  below) recorded **zero hits** during several minutes of dedicated testing
  of exactly these interactions, and the regular DirectInput-level log went
  completely silent for the same window. Not "quiet," not "subtle" —
  nothing in the FF call chain executes for these at all in the PC build.
- Racer-vs-racer car crash, and slides/handbrake/360-spins/braking/
  acceleration as dedicated events: no distinct trigger found for any of
  these. What sporadic rumble the user felt during drift/360 attempts traced
  back to already-known channels (continuous terrain/handling effects, or
  incidental curb/wall clips), not anything drift-specific.

### Key structural finding: one shared dispatcher, five effect objects

- Exactly two `CreateEffect` call sites exist in the entire logged session:
  - `0x5C0660` → creates `GUID_Spring` (`13541C27`) and `GUID_Damper`
    (`13541C28`) — DirectInput *condition* effects, used for the continuous
    "road/terrain feel."
  - `0x5C8E95` → creates `GUID_Square` (`13541C22`), `GUID_Sine`
    (`13541C23`), `GUID_Triangle` (`13541C24`) — *periodic* effects, used for
    discrete impact "punch."
- All `Start()` calls on all 5 effects return through **one shared
  dispatcher function at `0x5BFEE0`**: `Dispatch(vehicle_or_context, arg1,
  effectIndex, arg3)` → looks up `effect = table[effectIndex]` → calls
  `effect->Start(1, 0)`.
- `find_call_references` (Cheat Engine MCP bridge) against `0x5BFEE0` found
  **21 distinct call sites**, all clustered in one ~4 KB block
  (`0x5C92D1`–`0x5CA359`) — clearly one big collision/event-reaction
  function, not scattered inline calls. Sampled index arguments across
  several sites: 0, 1, 2, 3 seen (index 4 not directly confirmed but 5
  effect objects exist, so it's presumed to exist too).
- Live (non-intrusive) hardware-breakpoint capture on `0x5BFEE0` during
  normal driving showed: index 0 and index 3 fire continuously (many times
  per second) regardless of specific action — these are the two ongoing
  condition-effect "feel" channels, always live while driving. Index 1 fires
  only sporadically, carrying large `typespecific` magnitude values matching
  known impact numbers from the regular log — this is the discrete
  impact/crash effect.
- This exactly matches the community report at
  [ThirteenAG/WidescreenFixesPack#799](https://github.com/ThirteenAG/WidescreenFixesPack/issues/799)
  ("only ~2 triggers work") — now with hard numbers behind it instead of
  just an anecdote.

### Dead end: do not hook `0x5BFEE0` directly (the dispatcher)

Tried twice, in two forms, to patch the dispatcher's own entry point
(`mkjmp`-style, same technique as this project's other static hooks) to log
the *caller's* return address (the vtable hooks above only ever see
`0x5BFF18`, the return address *inside* this dispatcher after calling
`Start()`, not whoever called the dispatcher itself):

1. First attempt: full `pushad`/`popad` around a C logger call. Crashed the
   game deterministically, every time, at the very first frame where control
   is handed to the player (fault at `0x5BFF0F`, a null effect-pointer deref
   a few instructions into the *original, unmodified* function body).
2. Second attempt: rewrote as a minimal trampoline — only touches
   `EAX`/`EDX` (caller-volatile by convention, nothing relies on their
   incoming value), never writes `ECX`/`EBX`/`ESI`/`EDI`/`EBP`. Still crashed
   at the exact same fault address, on the exact same call
   (`retaddr=0x5C96CF, index=1` — the "control just handed to player" frame).

Since two implementations with very different overhead both fail
identically on the identical call, this isn't a hook-weight/timing-race
issue — something about merely intercepting this function's entry point at
all breaks it on that one specific frame, or per-call precisely.

The address is real, correct, verified via `find_call_references` and
manual disassembly (the vtable-slot arithmetic — `call [ecx+18]` /
`call [ecx+1C]` = slots 6/7 = `SetParameters`/`Start` — checks out
byte-for-byte against the logged behavior), so this is not a "wrong
address" bug. Treat `0x5BFEE0` as **investigate-only via Cheat Engine
hardware breakpoints (`set_breakpoint`, non-intrusive, proven safe across
multiple full sessions with zero crashes) — never patch its code directly.**
`hook-5BFEE0-ff-dispatcher-caller.c` is left in the tree (commented out of
`a_main.c`) for reference/future retry, not deleted.

### External validation: GameCube version

User separately booted the GameCube release of the same game: slides,
braking, nitro, and all crash types fire rumble there, with sensitivity
tiers. Confirms this is purely a **PC-port wiring gap**, not missing
detection logic — the same underlying physics/event-detection almost
certainly still exists in the PC binary (same engine, same game), it just
never got connected to the 21-site dispatcher above. Full GameCube
disassembly (different ISA, would need Dolphin + a PowerPC disassembler) was
considered but is not believed necessary — the plan is to find the missing
PC-side trigger points directly (live memory-scan for nitro/drift state,
`find_call_references` from there) rather than doing comparative RE against
the console build.

### Next steps (not started)

1. Live-locate the nitro-amount (and, separately, drift/slip-angle) memory
   address via Cheat Engine `scan_all`/`next_scan` while the user actually
   plays, then `find_call_references` on whatever writes it to find the
   activation function.
2. From that function, add a **new** call into the existing 5-effect
   dispatcher (reuse an existing effect slot, most likely the periodic
   "impact" one) — this is the safer patch target established above (this
   is a discrete, presumably-once-per-activation function, not the
   every-frame dispatcher itself, so the same crash risk should not apply,
   but confirm the call frequency before patching).
3. Decode `lpvTypeSpecificParams` properly per effect GUID (currently just a
   generic first-3-DWORDs dump) if per-event magnitude tuning is wanted.

## Open item / TODO (resolved)

~~The actual NFSU2 game install... has not been transferred to this dev
machine yet~~ — done; game is installed at
`C:\Users\Endurable4847\NFSU2-project\NFSU2` and has been used for all
testing described above.
