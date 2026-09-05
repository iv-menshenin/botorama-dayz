# Handoff: botorama — DayZ AI bot FSM (Phase 0 → Phase 1)

> **Статус: исторический снапшот (устарел).** Актуальный дизайн и этапы — в
> `Reference/fsm-implementation-plan.md`; правила кода — `codeguide.md`; learnings
> Stage 1 — `GrowUp-1.txt`. Этот файл можно удалить.

## Context / Objective

We are building an AI bot "engine" for DayZ (EnfusionScript). The mod is
`botorama` (prefix `dm`), a separate DayZ mod. The bot can already **spawn, sync
server→client, turn its head, and rotate its body with foot-stepping** — that part is
DONE and confirmed working in-game.

The current effort is replacing the hardcoded "stare at the player" behavior with a
**finite state machine (FSM)**. The full plan (with a detailed per-phase TODO) is in
`/home/devalio/dayz/Work/Reference/stage-1.FSL.plan.md` (note: filename has a `FSL`
typo; the user confirmed it should be `FSM` and wants it renamed "at the next
convenient moment" — do it if you touch the file).

## Current state (verified)

- **Git repo**: `/home/devalio/dayz/Work/botorama` (branch `master`), working tree is
  **clean**.
- **Commits** (newest first):
  - `efb5f81` "Config reader/writer" — Phase 0 JSON module (see below).
  - `ad824e0` "Refactoring" — directory restructure + logging `#ifdef` at call sites.
  - `482d322` "Body turn: foot-stepping via native Turn state" (the working turn).
  - `17fc852` "Head animation", `9bfa17f` "Init. Bots skeleton".
- **Phase 0 (JSON config reader) is DONE and committed.** Two new files in
  `core/3_Game/Config/`:
  - `dmJsonFile.c` — generic `dmJsonFile<Class T>`: `Load`/`Save`/`LoadDir`, optional
    version migration, `EnsureDirectory`, static conveniences.
  - `dmJsonConfigBase.c` — base class with `int Version` (serialized as `"Version"`,
    no `m_` prefix — this was an explicit user request for clean JSON), `GetVersion()`,
    `FixVersion()`, `Defaults()`.

## Key artifacts (reference — do not duplicate)

- **FSM plan + TODO**: `/home/devalio/dayz/Work/Reference/stage-1.FSL.plan.md`
  (phases 0–4, weighted-random algorithm, file layout, acceptance criteria).
- **Accumulated learnings/gotchas**: `/home/devalio/dayz/Work/Reference/GrowUp-1.txt`
  (Enfusion quirks, what worked/what crashed, Expansion AI turn mechanics).
- **Directory layout + placement rules**: `/home/devalio/dayz/Work/botorama/fstructure.md`.
- **Reference codebases (read-only, NOT part of botorama)**:
  - `/home/devalio/dayz/Work/DayZ-Script-Diff/` — vanilla DayZ scripts
    (`JsonFileLoader` in `scripts/3_game/tools/jsonfileloader.c`, `FindFile`/`FileAttr`
    in `scripts/1_core/proto/ensystem.c`, `dayzplayer.c` animation APIs).
  - `/home/devalio/dayz/Work/DayZ-Expansion-Scripts/DayZExpansion/` — Expansion AI:
    FSM core (`Core/.../Classes/FSM/ExpansionFSM.c`, `ExpansionState.c`,
    `ExpansionTransition.c`), AI states (`AI/.../Classes/FSM/`), movement
    (`AI/.../Classes/Commands/eAICommandMove.c`), versioned settings
    (`Core/.../Settings/Logging/ExpansionLogSettings.c`).
  - `/home/devalio/dayz/Work/dayz-devaliada/` — the user's other mod project; the
    config reader idea came from `src/mods/OneShot/scripts/Core/3_Game/Config.c`.

## Confirmed decisions (locked in, from the plan)

1. **Transitions**: each has a `weight` (0..1) + optional `Guard()`. Selection is
   weighted-random: `r = RandomFloat01() * Σweight`, walk cumulatively.
2. **JSON config**: one file per FSM preset (`$profile:dmBotorama/fsm_<preset>.json`).
3. **Config**: hardcoded defaults in code + override from `$profile:` at init.
4. **FSM topology**: code-only (Enfusion classes + preset factories); JSON only
   controls parameters.
5. **Config reader** (`dmJsonFile`): lives in botorama (not a shared lib yet — user
   chose "variant A"); no auto-creation of missing files — `Load` returns `false`,
   the owner does `Defaults()` + `Save()`.

## Next steps

The natural next move is to finish Phase 0 verification, then start **Phase 1**.

- **Phase 0 close-out**: write a tiny concrete config struct (inherits
  `dmJsonConfigBase`, has `Version`, `FixVersion`, `Defaults`) and exercise
  `dmJsonFile<T>` on a single key in `$profile:` to confirm read + migration +
  write-back works (log it via `dmBotLog`).
- **Phase 1 (FSM core)**: implement, in `core/4_World/Entities/Bot/FSM/`:
  - `dmBotTransition.c` (`m_To`, `m_Weight`, `Guard()`)
  - `dmBotState.c` (`m_Name`, transitions, `OnEntry`/`OnExit`/`OnUpdate`, EXIT/CONTINUE)
  - `dmBotFSM.c` (`AddState`/`GetState`/`Start`/`Update`, weighted `SelectTransition`)
  Then a throwaway preset of 2–3 states and a tick-loop test to confirm weight
  distribution (frequencies ≈ weights).
- **Phase 2+** (after FSM core): first real states (`Idle`/`Observe`), a
  `dmBotPreset_Default`, wire `dmAISurvivor.OnUpdate` → `m_FSM.Update()` (removing the
  hardcoded `SetLookTarget` flow in `core/5_Mission/MissionServer.c`), then config-
  driven params, then movement (`Wander`/`Drink`/`Eat`/`Flee`).

See the plan file for the full per-phase TODO with checkboxes.

## Key constraints / gotchas (details in GrowUp-1.txt)

- EnfusionScript module load order is `3_Game → 4_World → 5_Mission`; `static const`
  is compile-time and must be declared in an earlier-or-same module (this is why
  `DM_BOTORAMA_VERSION` lives in `cons/3_Game/constants.c`).
- Generics `class dmJsonFile<Class T>` cannot call arbitrary methods on `T`; the
  versioning hook works by casting `T` to `dmJsonConfigBase` via `dmJsonConfigBase.Cast(...)`.
- Logging: wrap the **call site** in `#ifdef DM_BOT_DEBUG` / `DM_BOT_TRACE` (Enfusion
  doesn't optimize empty calls; string concat in args is the expensive part). The
  `dmBotLog.Debug/Trace` methods themselves always `Print`.
- Animation management (head look / body turn) is done on the **server** in
  `CommandHandler`; the client only replays the synced animation. `AnimSetFloat`
  (variables) sync; `AnimCallCommand` triggers a state transition on the server.
- For JSON: `JsonFileLoader<T>.LoadFile/SaveFile`, `FindFile`/`FindNextFile`/
  `CloseFindFile` with `FindFileFlags.DIRECTORIES` for `*.json` directory listing.

## Suggested skills

The next agent should load these via the Skill tool:

- **`dayz-ai-bot`** — primary. Contains the project-specific DayZ AI bot knowledge:
  spawn/sync/head-look/foot-stepping-turn mechanics, custom `.agr` graph variables
  vs commands, Expansion AI (`eAICommandMove`) turn pattern, and the key file map.

(No other skill is currently needed. Only load `customize-opencode` if the work
turns to editing opencode's own config/agents/skills — not expected.)

## Working directories reminder

- `/home/devalio/dayz/Work/` is **not** a git repo (contains all projects + `Reference/`).
- `/home/devalio/dayz/Work/botorama/` is the git repo for this mod.
- Game/server logs live in `botorama/serverLog/` and `botorama/clientLog/` (gitignored).
