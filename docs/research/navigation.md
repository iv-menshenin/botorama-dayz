# Research: навигация (vault / doors / ladders)

Статус: база navmesh готова (`dmBotPathfinder`, `FindPath`/`SamplePosition`).
Задачи T7/T8/T9 плана. Ведёт субагент `dayz-research`.

## Цель

Довести `MoveTo` до прохождения препятствий: перепрыгивание/влезание (vault/climb),
открывание дверей, подъём/спуск по лестницам.

## Известные стартовые точки

- `AIWorld` = `GetGame().GetWorld().GetAIWorld()`: `FindPath(from, to, PGFilter, out TVectorArray)`,
  `RaycastNavMesh(from, to, filter, out hitPos, out hitNormal)`,
  `SampleNavmeshPosition(pos, maxDist, filter, out sampled)`.
- `PGPolyFlags`: `WALK/DOOR/INSIDE/LADDER/SPECIAL/JUMP/CLIMB/CRAWL/CROUCH/SWIM/UNREACHABLE/ALL`.
- `PGFilter`: `SetFlags(include, exclude, exclusive)`, `SetCost(PGAreaType, cost)`.
- Клаймб/лестница (`4_world/entities/manbase/playerbase.c`): `IsClimbing()`, `IsClimbingLadder()`,
  `GetCommand_Climb()`/`GetCommand_Ladder()`, `m_JumpClimb` (обработчик прыжка/клаймба).
- Команды движения: `COMMANDID_LADDER` (`3_game/dayzplayer.c`), `HumanCommandLadder`,
  `HumanCommandClimb`.

## Открытые вопросы (research)

1. **Vault/climb**: как инициировать перепрыгивание/влезание для ИИ — через
   `m_JumpClimb`? какая команда/состояние движения? Детект ребра через `RaycastNavMesh`
   + `PGPolyFlags.SPECIAL/JUMP/CLIMB`.
2. **Двери**: как найти дверь (`DoorBase`/`ActionOpenDoors`), как открыть серверно
   (`Open()`? интеракция без ActionManager у ИИ?), стоимость `DOOR_CLOSED`/`DOOR_OPENED`
   в `PGFilter.SetCost`.
3. **Лестницы**: entry/exit точки, `PGPolyFlags.LADDER`, состояние подъёма
   (`m_eAI_IsOnLadder` у Expansion), `SamplePosition` к лестнице.
4. Recovery при зависании (шаг назад/вбок) — приоритетная доводка `MoveTo` (техдолг F).

## Источники

- `DayZ Projects/scripts/3_game/ai/aiworld.c`
- `DayZ Projects/scripts/3_game/ai/pgfilter.c`, `pgpolyflags` (enums)
- `DayZ Projects/scripts/4_world/entities/manbase/playerbase.c` (m_JumpClimb, IsClimbing)
- `DayZ Projects/scripts/3_game/human.c` (HumanCommandLadder/Climb)
- Expansion: `ExpansionPathHandler.c`, `eAICommandMove.c`
