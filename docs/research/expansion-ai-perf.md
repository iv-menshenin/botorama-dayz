# Research: за счёт чего Expansion AI легче по производительности

Статус: исходное исследование (perf-развилка: «оптимизировать наш скрипт vs менять класс сущности»).

## Цель

Понять, почему Expansion держит ~180 ИИ-ботов без тормозов, а botorama (~30 ботов,
`dmAISurvivorBase : PlayerBase`) уже грузит сервер. Ключевой вопрос: дело в **классе
сущности** (Expansion использует что-то легче `PlayerBase`?) или в **организации тиков**
(скриптовый ранбук: троттлинг, time-slicing, LOD, деспавн)?

**Короткий ответ (TL;DR):** Expansion использует **тот же класс сущности, что и мы**
(`eAIBase : PlayerBase`, спавн `CreateObject`) и **ту же полную симуляцию** в
`CommandHandler` (`super.CommandHandler`). Разница — НЕ в классе, а в **системе тиков**:
тяжёлая ИИ-логика (FSM/паффайндинг/таргетинг/обход препятствий) вынесена из
`CommandHandler` в **time-sliced round-robin scheduler** с бюджетом, а вдали от игроков
частота падает в 5× и вплоть до полного `DisableSimulation`. Отсюда вывод для botorama —
см. «Выводы для botorama» в конце.

---

## 1. Класс сущности — ТОТ ЖЕ `PlayerBase`, что и у нас

- `class eAIBase: PlayerBase` — `eAIBase.c:20`. Никакого lightweight-класса нет.
- `class eAINPCBase: eAIBase {}` (пустой) — `eAINPCBase.c:1`.
- `class eAINPCPassive: eAINPCBase` — пассивный NPC, только выставляет `m_eAI_FactionType` — `eAINPCBase.c:3-15`.
- Модельные классы — пустые маркеры типов: `class eAI_SurvivorM_Mirek: eAIBase {}` и т.д. — `eAISurvivor.c:97-131`.
- `eAISurvivor` — **не** сущность, а статический хелпер имён моделей (`GetQuasiRandom*`) — `eAISurvivor.c:1-90`.

### Спавн
`SpawnAIEx` → `Class.CastTo(ai, g_Game.CreateObject(GetRandomAI(), pos))` — `eaicommandmanagerimpl.c:370-378`.
Тот же `CreateObject`, что у botorama; явного `INSTANCETYPE`/`EconomyProfile` **нет** в скрипте.
`INSTANCETYPE_AI_SERVER` выставляет движок (для `PlayerBase`, созданного на сервере без клиента).

### INSTANCETYPE
- `DayZPlayerInstanceType`: `INSTANCETYPE_SERVER/CLIENT/REMOTE/ AI_SERVER/AI_REMOTE/AI_SINGLEPLAYER` — `dayzplayer.c:1069-1078` (ваниль).
- Expansion читает `GetInstanceType() == INSTANCETYPE_AI_REMOTE` только в одной точке — `OnPlayerLoaded`
  (`eAIBase.c:619`) для `DelayedValidateAndRepair` оружия (клиентская репрезентация ИИ).
- `EOnInit` → `OnSelectPlayer` (тот же путь, что у игрока) — `eAIBase.c:697-704`.
- **Вывод:** класс сущности и способ спавна идентичны botorama. «Поменять класс на lightweight» — не тот рычаг.

---

## 2. Симуляция — полная `super.CommandHandler`, но ИИ-логика вынесена наружу

### CommandHandler (переопределён)
`override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)` — `eAIBase.c:7077`.

- Зовёт **`super.CommandHandler(...)`** (полная симуляция PlayerBase: анимационный граф,
  `HeadingModel`, стамина) — `eAIBase.c:7128`.
- После super — свои `GetPlayerSoundManagerServer().Update(); ShockRefill(pDt); FreezeCheck();` (комментарий:
  «vanilla only checks for INSTANCETYPE_SERVER») — `eAIBase.c:7197-7199`.
- Свой `eAI_HandleWeapons(...)` вместо ванильного оружейного конвейера — `eAIBase.c:7246, 9503`.

**Ключевой разрыв с botorama:** при `s_eAI_FTO > 0` (дефолт `2`, `eAIBase.c:31`) **тяжёлая ИИ-логика НЕ вызывается из
`CommandHandler`** — `eAI_OnUpdate(pDt)` и `eAI_UpdateFSM(...)` гейтятся `if (s_eAI_FTO == 0)`:
- `eAIBase.c:7208-7211` (`if (s_eAI_FTO == 0) eAI_OnUpdate(pDt);`)
- `eAIBase.c:7286-7287` (`if (s_eAI_FTO == 0) eAI_UpdateFSM(pDt, simulationPrecision);`)

Вместо этого `eAI_OnUpdate` + `eAI_UpdateFSM` + `AvoidObstacles` + `eAI_OnMovementUpdate` гонит **scheduler**
(см. §3). То есть: `CommandHandler` = только «тело» (нативная симуляция человека), «мозг» — отдельным
дросселируемым циклом.

### OnScheduledTick (тело-системы)
`override void OnScheduledTick(float deltaTime)` — `eAIBase.c:8752`: вручную тикает
`m_ModifiersManager.OnScheduledTick(deltaTime)`, `GetBleedingManagerServer().OnTick(deltaTime)`,
`m_Environment.Update(deltaTime)` (ваниль гейтит `OnScheduledTick` по `IsPlayerSelected`). Это **ровно то,
что уже делает botorama** (см. skill «Системы тела»). У Expansion дополнительно троттлится
`Expansion_UpdateVisibility()` — `m_eAI_UpdateVisibilityTimeThreshold = 5.0` с — `eAIBase.c:353-354, 8814-8820`.

### Кастомный animation graph — НЕТ
Отдельного «лёгкого» графа для ИИ нет; eAIBase наследует стандартный граф `PlayerBase`. Разгружают они
не граф, а **сам факт тика** (Dormant/`DisableSimulation`, §3).

---

## 3. LOD по дистанции — time-sliced scheduler + Dormant/DisableSimulation

Это центральный механизм. Три слоя.

### (a) Scheduler: round-robin с бюджетом (всегда)
`ExpansionWorld.eAI_OnUpdate(bool doSim, float timeslice)` — `ExpansionWorld.c:197-293`.

Константы (`ExpansionWorld.c:3-6`):
- `AI_UPDATE_INTERVAL = 0.05` с (номинал 20 Гц);
- `AI_UPDATE_BUDGET_FACTOR = 0.3` (30 % бюджета кадра);
- `MAX_AI_UPDATE_TIME_MS = 5` мс (потолок на один слайс);
- `MAX_AI_UPDATE_BATCH = 20` ИИ за слайс.

Вход: `DayZGame.OnUpdate(doSim, timeslice)` → `m_ExpansionGame.eAI_OnUpdate(doSim, timeslice)` — `DayZGame.c:101-104`.

Логика:
- AI в **двусвязном списке** `eAIBase.s_eAI_Alive` (`CF_DoublyLinkedNodes_WeakRef`) — `eAIBase.c:28`;
  итерация через `m_CurrentAliveAI` (курсор round-robin) — `ExpansionWorld.c:236-242`.
- **FPS-адаптив**: `f = LinearConversion(30,35, serverFPS, 5, 1)` — при падении ниже 35 FPS интервал
  растёт до 5× — `ExpansionWorld.c:208-209`.
- **Бюджет**: `budget = min((1000/serverFPS)·0.3, 5ms)`; `maxUpdateTime = LinearConversion(30,35, serverFPS, t·2, budget)`;
  `aiMax = min(ceil(maxUpdateTime/t), min(aiCount, 20))` — **максимум 20 ИИ за слайс**, прерывание по
  `costTotal >= maxUpdateTime` — `ExpansionWorld.c:226-228, 275-277`.
- Каждый ИИ обновляется **не чаще своего интервала**: `if (pDt >= updateIntervalCurrent)` — `ExpansionWorld.c:252-259`.

### (b) Дальние ИИ — интервал ×5
`if (s_eAI_FTO >= 2 && ai.m_eAI_PlayersWithinVisibilityDistanceLimit.Count() == 0)
  updateIntervalCurrent = AI_UPDATE_INTERVAL * 5;` — `ExpansionWorld.c:248-250`.

Т.е. ИИ, у которого нет ни одного игрока в радиусе видимости, тикает раз в `0.25 с` вместо `0.05 с`.
`m_eAI_PlayersWithinVisibilityDistanceLimit` — набор игроков в радиусе `eAI_GetVisibilityDistanceLimit`
(до ~1100·visibility метров, лимит ночью `min(ThreatDistanceLimit, 250)`), пересчитывается в `eAI_Targeting`
по одному игроку за тик — `eAIBase.c:3076-3094, 9487-9496`.

### (c) Dormant: полное отключение симуляции
`eAIState_Dormant` — `eaistate_dormant.c`:
- `OnEntry`: `unit.DisableSimulation(true)` — `eaistate_dormant.c:8`;
- `OnExit`: `unit.DisableSimulation(false)` — `eaistate_dormant.c:16`;
- `Guard()` выводит из сна, если: fighting / `AcuteDangerTargetCount>0` / hit <10 с / игрок в радиусе
  видимости / команда ≠ MOVE,SWIM / скорость >0 / есть вейпоинты / leader не ИИ / ROAMING-режим —
  `eaistate_dormant.c:31-75`.

`DisableSimulation`/`GetIsSimulationDisabled` — нативные (`entity.c:3,6`). В scheduler
`isDisabled = ai.GetIsSimulationDisabled()` — если движок выключил симуляцию, пропускается
`eAI_OnUpdate`/`AvoidObstacles`/`eAI_OnMovementUpdate`, остаётся только лёгкое
`eAI_SetHasProjectileWeaponInHands` + `eAI_Targeting(pDt, ..., false)` — `ExpansionWorld.c:256-269`.

> `[нужно подтвердить]` Точный порог/условие входа в Dormant — это FSM-переход из `eAIState_Idle`
> (скриптовый guard, не дистанционный хардкод). Дистанция задаётся через
> `m_eAI_PlayersWithinVisibilityDistanceLimit` (пусто ⇒ можно спать). Насколько агрессивно FSM
> реально уводит в Dormant в проде — проверить на тест-сервере с `botdump`.

---

## 4. Райкасты / зрение — троттлинг + кэш + FOV-гейт

- **LOS-райкаст троттлится и кэшируется**: `LOS_CHECK_INTERVAL = 0.15` — `eAIBase.c:24`; аккумулятор
  `m_eAI_LOSCheckDT`, результат кэшируется в `state.m_LOS` и возвращается без луча, пока интервал не истёк
  (и цель не сменилась) — `eAIBase.c:8080-8096`.
- **Интервал масштабируется частотой тика**: `interval = LOS_CHECK_INTERVAL · (pDt / AI_UPDATE_INTERVAL)`
  (дальний ИИ с `pDt=0.25` получает интервал `0.75 с`) — `eAIBase.c:8088-8092`.
- **FOV-гейт до луча**: если цель вне горизонтального полуугла FOV — `state.m_LOS=false` БЕЗ райкаста —
  `eAIBase.c:8130-8185`.
- **Двойной луч** «сквозь стекло/окна»: сначала `ObjIntersectView`, при хите здания — повтор `ObjIntersectFire`
  — `eAIBase.c:8194, 8228`.
- **Скан сущностей в боксе** (30 м) — раз в `RandomFloat(0.1, 0.2)` с, `SceneGetEntitiesInBox` с
  `QueryFlags.DYNAMIC`; cover-объекты (статику/деревья/камни) — только каждый 33-й скан
  (`m_eAI_UpdateNearTargetsCount % 33 == 0`) — `eAIBase.c:2963-3050`.
- **Игроки в дальнем радиусе (1000 м) — по одному игроку за тик** (round-robin по `s_Expansion_AllPlayers`) —
  комментарий «check one player per tick (30 players per second)» — `eAIBase.c:3067-3094`.
- **Обработка целей — по одной сущности за вызов**: `while (m_eAI_CurrentPotentialTargetIndex < Count())`
  инкрементит индекс (не перебирает весь массив за тик) — `eAIBase.c:3149-3151`.

---

## 5. Pathfinding — ванильный `AIWorld.FindPath`, обёрнутый и сильно дросселированный

- Свой класс `ExpansionPathHandler` (обёртка, не замена движка) — `ExpansionPathHandler.c:1`.
- `m_AIWorld = g_Game.GetWorld().GetAIWorld()` — **тот же ванильный `AIWorld`** — `ExpansionPathHandler.c:72`.
- `PGFilter` берётся из синглтона `ExpansionPathFilters` (include/exclude флагов, jump/climb, swimming) —
  `ExpansionPathHandler.c:66-70`.
- Сам поиск: `m_Target.FindPath(this, m_Points)` → `pathFinding.m_AIWorld.FindPath(startPos, Position, filter, path)`
  (плюс `NavMesh.FindPath` для спец-кейсов) — `ExpansionPathPoint.c:120, 189`.
- `m_AIWorld.SampleNavmeshPosition(inPos, maxDistance=1.0, m_AllFilter, inPos)` для валидации цели —
  `ExpansionPathHandler.c:612`.
- **Частота пересчёта сильно дросселирована**:
  - `PATH_RECALCULATE_THRESHOLD = 5.0` — `ExpansionPathHandler.c:3`;
  - `timeUntilNextUpdate = (pSimulationPrecision + 1.0) · 2.0` (мин. ~2 с между рекалками на суше) —
    `ExpansionPathHandler.c:564-579`;
  - **cost-based минимум**: `m_MinTimeUntilNextUpdate = pDt + m_TimeIt.GetElapsed() · 0.0002` (чем дольше
    шёл прошлый FindPath, тем дольше пауза) — `ExpansionPathHandler.c:874`.
- Свой A* (`AStar.c`/`PathNode.c`/`ExpansionPriorityQueue.c`) — **не для пешего ИИ**, а для
  `eAIRoadNetwork` (вождение, отдельная тема; см. decisions.md «drive: road-graph»).

---

## 6. Прочие перф-хаки

- **Спавн/деспавн по дистанции (динамические патрули)** — `eAIDynamicPatrol`:
  `m_MinimumRadius/m_MaximumRadius` (радиус спавна), `m_DespawnRadius` + `m_DespawnTime`
  (если ВСЕ игроки вне `m_DespawnRadius` → тик таймера → `ObjectDelete` патруля) —
  `eAIDynamicPatrol.c:22-30, 167-185`; условие `AvoidPlayer(patrolPos, m_DespawnRadius) || m_Group.m_ForcePatrolDespawn` — `eAIDynamicPatrol.c:749`.
- **Load balancing по онлайну**: `LoadBalancing_UpdateAll` пересчитывает `MaxPatrols` по
  `MinPlayers/MaxPlayers` категорий, по подключению/отключению игрока (отложенно 10 с) —
  `eAIDynamicPatrol.c:473-542`.
- **Инвентарь не синкается на клиент**: сущность `AI_SERVER` (нет клиентской репрезентации), поэтому
  содержимое рюкзака на клиент не уезжает. `m_eAI_IsInventoryVisible` (net-var) + RPC
  `RPC_eAI_SetIsInventoryVisible` включают видимость только когда игрок лутит труп —
  `eAIBase.c:189, 590, 9847-9906`. `IsInventoryVisible()` гейтится ещё и `m_Expansion_CanBeLooted` — `eAIBase.c:9847-9855`.
- **Спавн патруля отложенный по кадрам** (`SpawnAI_Deferred` по одному ИИ через `CallQueue(CALL_CATEGORY_SYSTEM)`)
  — избегает спайка на одновременный спавн формации — `eAIDynamicPatrol.c:650-670`.
- **Persist групп по CE**: `OnCEIterate` сохраняет по одной группе за CE-интервал (троттлинг `s_eAI_LastCEUpdateTime` > 3 с) — `eAIBase.c:779-842`.
- `InsertAgent(WOUND_AGENT)` — заглушка `return` (отказ от агента ранений) — `eAIBase.c:845-853`.

---

## Выводы для botorama

1. **Менять класс сущности не нужно** — Expansion работает на том же `PlayerBase` + `CreateObject`.
   Выигрыш не в классе.
2. **Главный рычаг — вынос «мозга» из per-frame цикла в дросселируемый scheduler.** У нас сейчас
   тяжёлая логика (интенты/FSM/паффайндинг/`dmVision`) тикает в `TickAll` → `OnUpdate` каждый кадр на
   30 ботов. Expansion: 20 ИИ/слайс, 20 Гц, 5× дальше, `DisableSimulation` у спящих.
3. **LOD по дистанции** = `m_eAI_PlayersWithinVisibilityDistanceLimit` (пусто ⇒ ×5) + `eAIState_Dormant`
   (`DisableSimulation(true)`). У нас аналога `DisableSimulation` нет — ближайший кандидат.
4. **Pathfinding** — у нас `FindPath` по рекалку чаще (MoveTo пересчитывает по прогресс-монитору);
   Expansion держит мин. 2 с между рекалками + cost-based паузу.

---

## Открытые вопросы / [нужно проверить]

1. `[нужно проверить]` **Кто дороже per-frame при одинаковом числе активных ботов: наш `CommandHandler` vs
   Expansion `super.CommandHandler` + `eAI_HandleWeapons`?** Судя по коду — оба гонят полную нативную
   симуляцию `PlayerBase`; разница должна быть в скриптовой части мозга (у нас — интенты каждый кадр).
2. `[нужно подтвердить]` `DisableSimulation(true)` реально останавливает анимационный граф/скелет для
   `AI_SERVER`-сущности, или только физику? (Косвенно — `CommandHandler` не тикается при disabled, но
   подтвердить по `botdump`/FPS.)
3. `[нужно проверить]` Порог `eAI_GetVisibilityDistanceLimit` на проде (до 1100 м днём) означает, что
   «пусто в радиусе видимости» почти никогда не наступает на плотном сервере — тогда ×5-ветка редкая, и
   основной эффект даёт `Dormant` + бюджет. Стоит замерить на нашем сетапе.
4. `[нужно подтвердить]` Насколько `eAIState_Dormant` реально достижим в проде (guard'ы строгие:
   нет вейпоинтов, leader ИИ, не ROAMING) — т.е. работает ли он для settlement-ботов botorama (у нас
   постоянный idle-сетап, у Expansion — формации/патрули).

## Источники

- `DayZ-Expansion-Scripts/DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/AI/eAIBase.c`
- `.../Classes/ExpansionWorld.c`, `.../Classes/PathFinding/ExpansionPathHandler.c`, `.../PathFinding/ExpansionPathPoint.c`
- `.../Classes/FSM/states/eaistate_dormant.c`, `.../Classes/Patrols/eAIDynamicPatrol.c`
- `.../3_Game/DayZExpansion_AI/DayZGame.c`, `.../5_Mission/DayZExpansion_AI/eaicommandmanagerimpl.c`
- Ваниль: `3_game/dayzplayer.c` (DayZPlayerInstanceType), `3_game/entities/entity.c` (DisableSimulation/GetIsSimulationDisabled).

---

## Существенные развилки и принятые решения (для decisions.md)

1. **Рычаг — не класс сущности, а scheduler.** Колебался между «исследовать, какой lightweight-класс
   спавнит Expansion» и «сравнить организацию тиков». Код показал: класс одинаковый (`PlayerBase`),
   вся разница — в time-sliced scheduler + LOD. Это определяет направление: не переписывать сущность,
   а портировать бюджетный/дросселируемый цикл.
2. **Что считать «главным выигрышем».** В репо четыре независимых механизма (бюджетный round-robin,
   ×5 по дистанции, Dormant/DisableSimulation, дроссель паффайндинга/LOS). Открытым остаётся, какой из
   них доминирует на нашем сетапе — это эмпирический вопрос для `dayz-tester`, а не решается из кода.
