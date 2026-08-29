# Handoff: botorama — DayZ AI bot FSM + intents + goals

> **Статус: исторический снапшот (устарел).** Описанная тут незакоммиченная работа
> уже закоммичена (`93f4907`, `a4a3180`). Актуальный дизайн/этапы — в
> `Reference/fsm-implementation-plan.md`. Этот файл можно удалить.

## Objective

Building a server-side AI bot "engine" for DayZ (EnfusionScript), mod `botorama`
(prefix `dm`). The bot spawns/syncs/looks/turns/moves; behaviour is driven by an
FSM that commands atomic "intents" (look/move/turn). We are mid-implementation of
the FSM↔intent wiring and the hybrid (cooperative + preemptive) transition model.

## Repo / working dirs

- Git repo: `/home/devalio/dayz/Work/botorama` (branch `master`). Working tree has
  **uncommitted changes** (see below). Do NOT commit unless asked.
- `/home/devalio/dayz/Work/` is NOT a repo (holds all projects + `Reference/` docs).
- Logs: `botorama/serverLog/`, `botorama/clientLog/` (gitignored).
- Reference codebases (read-only): `/home/devalio/dayz/Work/DayZ-Script-Diff/`
  (vanilla), `/home/devalio/dayz/Work/DayZ-Expansion-Scripts/DayZExpansion/`
  (Expansion AI — FSM/target/movement reference), `/home/devalio/dayz/Work/dayz-devaliada/`
  (user's other mod; source of the JSON config reader idea).

## Commits (newest first)

- `a4a3180` "Test commands: /bot intent lookAt|lookAtMe|goto|clear"
- `9b2949b` "Fix scoupe" (hoisted a variable — no block scoping)
- `916d70e` "FSM: states, transitions and bot intents"
- `efb5f81` "Config reader/writer" (JSON), `ad824e0` "Refactoring",
  `482d322` "Body turn: foot-stepping", `17fc852` "Head animation",
  `9bfa17f` "Init. Bots skeleton".

## Uncommitted work (current session — read these files, do not redo blindly)

The working tree contains the following, all NOT yet committed:

- **Preemption model** (just implemented, not yet compiled/tested by user):
  - `FSM/dmBotState.c` — added `enum dmBotStateKind { NORMAL, INTERRUPTIBLE, PREEMPTIVE }`
    + `GetKind()` (default NORMAL, overridden per state).
  - `FSM/dmBotFSM.c` — hybrid `Update()`: cooperative EXIT → `SelectTransition`;
    if current is `INTERRUPTIBLE`, every `s_PreemptInterval` (= `DM_FSM_PREEMPT_INTERVAL`
    0.25s, static `SetPreemptInterval()` overridable) run `SelectPreemptive` (only
    `PREEMPTIVE` destinations; if any eligible → MUST transition). Refactored shared
    `RollWeighted` / `TransitionTo`.
  - `States/dmBotState_Patrol.c` — `GetKind() = INTERRUPTIBLE`.
  - `cons/4_World/constants.c` — `DM_FSM_PREEMPT_INTERVAL = 0.25`.
- **Idle/Patrol states + orientation intents** (previous turns, uncommitted):
  - `States/dmBotState_Idle.c` — now EXITs after random 30–90s (was the bug: never
    EXITed, so FSM never left Idle); does random glance/turn every 15–60s.
  - `States/dmBotState_Patrol.c` — `CanEnter` = has patrol points; captures route;
    MoveTo each point, `≤1m + 5s` dwell → next; last → EXIT.
  - `Intent/dmBotIntent_Glance.c` (head-only look), `Intent/dmBotIntent_Turn.c`
    (body turn, FINISH on reach).
  - `dmTarget.c` — goal target (type ACQUIRE/DESTROY, `m_Entity` nullable,
    `m_ClassEntity`, `m_Priority`, `m_LastPosition`). Skeleton only.
  - `dmAISurvivor.c` — added `LookAtYaw/GetYawTo`, patrol-point methods
    (`GetPatrolPoints/AddPatrolPoint/ClearPatrolPoints`), target methods
    (`AddTarget/ClearTargets/GetTargets`), `ClearCommandIntents/ClearPersonalityIntents`.
  - `MissionServer.c` — `/fsm new|add idle|patrol|apply` (draft FSM builder,
    auto-connect all pairs) + `/bot patrol add|clear`.
  - `cons/5_Mission/constants.c` — `/fsm` + `/bot patrol` command words.
- `override` keyword was added to all `dmBotIntent_*` OnStart/OnUpdate/OnCancel
  (was `FIX-ME: Overriding function ... not marked as 'override'` warnings).

## Key artifacts (reference — do NOT duplicate)

- **Plan + design**: `/home/devalio/dayz/Work/Reference/fsm-implementation-plan.md`
  (FSM core, intents, Idle/Patrol/goals, hybrid transition model, test commands,
  commits+lessons). This is the authoritative design doc.
- **Enfusion coding rules**: `/home/devalio/dayz/Work/Reference/codeguide.md`
  (what's NOT supported, module ordering, ref/generics/JSON, etc.).
- **Phased TODO**: `/home/devalio/dayz/Work/Reference/stage-1.FSL.plan.md`
  (filename has FSL typo; user wants it renamed to FSM at some point).
- **Accumulated learnings**: `/home/devalio/dayz/Work/Reference/GrowUp-1.txt`.

## Current architecture (for orientation)

```
FSM (dmBotFSM/dmBotState/dmBotTransition/dmBotCondition) — decisions
  └─ sets intents via bot.AddFSMIntent(...)
Intent (dmBotIntent + pool, dmBotIntent_*) — atomic behaviours, 3 pools
  (m_FSMIntents / m_PersonalityIntents / m_CommandIntents), arbitration each tick
  └─ calls brain primitives
dmAISurvivor (brain) — LookAt*/SetWalk/FacePoint/UpdateLook/patrol/targets
  └─ drives pawn
dmAISurvivorBase (pawn) — head look + foot-step body turn (graph vars/commands)
```

Key facts: intent priority `{IDLE, DESIRABLE, CRITICAL}` + concurrency
`{PARALLEL, EXCLUSIVE}`; arbitration = reset channels (`LookForward`+`SetWalk(false)`)
→ tick pools → EXCLUSIVE silences all, else PARALLEL ascending priority. Body facing
is DERIVED (moving → movement direction; idle + FULL/AUTO → look). See the plan doc.

## Enfusion gotchas (details in codeguide.md — critical to avoid repeat mistakes)

- **No ternary** `?:`, **no line breaks in an unfinished statement** (chains must be
  one line), **no block scoping** (a name can be declared once per function — hoist
  `int i;` and use `for (i = 0; ...)`).
- **`static const` module order**: `3_Game → 4_World → 5_Mission`; a constant used by
  `core/4_World` must live in `cons/4_World` (NOT `cons/5_Mission`). Caused two
  `Can't find variable 'DM_...'` errors already — see codeguide "static const и
  порядок модулей".
- `GetLookDirection()`/`GetAimDirection()` are **Expansion-only**; vanilla is
  `MiscGameplayFunctions.GetHeadingVector(PlayerBase)` + `DayZPhysics.RaycastRV`.
- Logging: wrap the **call site** in `#ifdef DM_BOT_DEBUG` / `DM_BOT_TRACE`.
- `ref` for non-managed class fields/containers; parameterless constructors +
  setters for state/intent setup (avoid constructor-inheritance ambiguity).

## Next steps (suggested)

1. **Compile/test** the uncommitted preemption model + Idle/Patrol changes. Expect
   `[FSM] enter Idle` → `[FSM] transition Idle -> Patrol` → `[FSM] Patrol.entry
   routePoints=N` → `[FSM] Patrol -> point 1/N` … and the bot walking.
2. If transition happens but the bot doesn't move → the movement (`SetWalk` via
   `OverrideMovementSpeed/Angle`) is the untested risk (was flagged as "verify in
   tests"; may need a custom movement command like Expansion `eAICommandMove`).
3. Preemption is currently inert (no `PREEMPTIVE` state). To exercise it, add e.g.
   a `Fight` state with `GetKind() = PREEMPTIVE` + `CanEnter() = "enemy present"`.
4. "Resumable" (resume interrupted state from where it left) is deferred — currently
   preemption evicts and the state re-enters fresh.

## Suggested skills

The next agent should load via the Skill tool:

- **`dayz-ai-bot`** — primary. Contains the DayZ AI bot domain knowledge: spawn/sync,
  head look, foot-stepping body turn, custom `.agr` graph vars vs commands, the
  Expansion AI reference points (`eAICommandMove` turn, `eAITargetInformation`), and
  the botorama file map.
