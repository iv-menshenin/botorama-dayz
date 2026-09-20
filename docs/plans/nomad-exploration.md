# Nomad Exploration — руководство реализации

Кочевничество (Nomad): бот обходит здания внутри локации (`dmBotState_Exploration`) и
кочует между локациями (`dmBotState_Travel`). Источник поведения — `docs/nomad-proposal.md`.

Статус: **в работе**. Ветка: `feature/nomad-exploration`.

---

## 0. Цель и ключевые решения

- **Цели кочёвки** — все 306 локаций из `world_poi.json` (не только населённые пункты).
- **Лут при исследовании** — оппортунистический подбор (`PickUp`) и выброс при переполнении
  остаются без изменений; меняется только механика обнаружения/обхода зданий.
- **Жажда→колодец** — старое PREEMPTIVE-поведение `Travel` блокируется/удаляется. Будущее
  «пустая ёмкость → поиск колодца» — в `docs/techdebt.md`.
- **Условия завершения Exploration** — в JSON `$profile:dmBotorama/settings/looting.json`.
- **Здания привязываются к локациям** через хук `House` (реестр зданий), не через радиус-скан.
- **Сканбоксы зданий в Explorer убраны полностью**; остаются только маленькие лут-сканбоксы
  внутри зданий (`dmLoot.ScanNearbyItems`).

---

## 1. Архитектура модулей (после рефактора)

| Модуль | requiredAddons | примечание |
|---|---|---|
| cons | `[]` | + `dmJsonFile`/`dmJsonConfigBase` (перенесены из reg) |
| map | `[cons]` | только `dmWorldPoiConfig` + `dmWorldPOIRegistry` (автономный) |
| reg | `[cons, map]` | реестр зданий + привязка к локациям |
| roads | `[cons]` | без изменений (спец-зависимости от reg НЕ создаём) |
| loadout | `[cons, reg]` | убрана ложная зависимость от core |
| core | `[cons, roads, reg, loadout, map]` | боты, спавн-менеджер, состояния |
| test | всё | без изменений |

Правило слоёв: `reg` видит `map` (хук зданий привязывает их к локациям), `core` видит `map`
(состояния кочёвки читают реестр POI). `roads` остаётся минимальным на `cons`.

---

## 2. Вехи (порядок строгий)

Каждая веха = атомарный коммит(ы) + **ревью «придиры»** (`dayz-reviewer`) + **тест**
(`dayz-tester`). Веха считается завершённой только после обоих гейтов.

### Веха A — рефактор модулей

- `src/reg/3_Game/Config/dmJsonConfigBase.c` → `src/cons/3_Game/Config/`.
- `src/reg/3_Game/Config/dmJsonFile.c` → `src/cons/3_Game/Config/`.
- `src/map/5_Mission/System/dmBotSpawnManager.c` → `src/core/5_Mission/System/`.
- `src/map/3_Game/Config/dmSpawnConfig.c` → `src/core/3_Game/Config/`.
- `src/map/5_Mission/MissionServer.c` удалить; тики `dmBotSpawnManager` +
  `dmRoadDiscoveryManager` + `dmRoadGraphManager` перенести в `src/core/5_Mission/MissionServer.c`
  (рядом с `dmAISurvivor.TickAll`).
- `requiredAddons`: loadout `[cons,reg]`; map `[cons]`; reg `[cons,map]`; core
  `[cons,roads,reg,loadout,map]`.
- Из `src/map/config.cpp` убрать `missionScriptModule` (папка 5_Mission исчезает).
- Гейт: дымовой тест (сборка → деплой → сервер → все 7 PBO + «Botorama initialized»).

### Веха B — реестр зданий + привязка к локациям

- `dmWorldPoiLocation` (map/3_Game/Config): добавить `int Id` (присваивается при `Load()`, 0..N-1).
- `dmWorldPOIRegistry` (map): `m_Locations` (все 306) рядом с `m_Settlements`; `GetLocation(id)`,
  `LocationCount()`, `GetLocationAt(pos)` (ближайшая в радиусе присутствия типа, `LengthSq`);
  `static GetPresenceRadius(type)` = Capital 1200 / City 600 / Village 300 / прочие 200;
  `static GetArrivalRadius(type)` = Capital 500 / City 150 / Village 75 / прочие 50.
- `dmLiveBuildingRegistry` (reg): обёртка `dmRegisteredBuilding { Building m_Building; int m_LocationId; }`;
  `ref array<ref dmRegisteredBuilding> m_Buildings` + `ref map<int, ref array<Building>> m_ByLocation`;
  `Register` привязывает к локации (`GetLocationAt`), `Unregister` снимает;
  `GetBuildingsForLocation(loc, out array<Building>)`; `GetNearestBuildingWithDoors(pos, radius, exclude)`.
  `excludedBuildings` переносится из `dmExplorer` в реестр (не привязываем исключённые типы).
- Гейт: тест «ленивый стрим» (спавн бота у локации → реестр наполняется, здания привязаны) + профайлинг.

### Веха C — `dmExplorer` без сканбоксов

- Удалить: `OnUpdate`-скан (`SceneGetEntitiesInBox STATIC`), `ForgetFar`, `GetNearest`,
  `GetNearestBuildingWithDoors`, `FindBuilding`, `IsExcludedBuilding`, радиус-поля; вызов
  `m_Explorer.OnUpdate` из `dmAISurvivor.OnUpdate`.
- Новые поля: `ref array<int> m_VisitedLocations`, `ref dmWorldPoiLocation m_CurrentLocation`,
  `ref dmWorldPoiLocation m_Destination`, `ref array<ref dmExploredBuilding> m_LocationBuildings`,
  `int m_LocationTotal`, `bool m_NothingToDo`, `bool m_InTransit`, `float m_TimeInLocation`.
- Методы: `HasVisited(id)/RememberLocation(id)`, `ArriveAtLocation(bot, loc)` (идемпотентно),
  `GetNextUnvisitedBuilding`, `MarkLocationBuildingVisited`, `LocationUnvisitedCount/VisitedCount`,
  `IsNothingToDo/Set`, `IsInTransit/Set`, `Get/Set/ClearDestination`, `TickLocationTime/GetTimeInLocation`,
  `GetNearestUnvisitedLocation(bot)` (перебор реестра, пропуск visited — **слои: это core, не map**).
- Hunting: `GetNearestBuildingWithDoors` → `dmLiveBuildingRegistry`, локальный `m_Checked` в состоянии.

### Веха D — обход зданий + `looting.json`

- `src/core/3_Game/Config/dmLootingConfig.c`: `dmExplorationConfig` (вложенный объект
  `Exploration`) с полями `ExitVisitedCount`, `ExitVisitedPercent`, `ExitVisitedPercentTime`,
  `ExitTimePercentSeconds`, `ExitTimeSeconds`; `dmLootingConfig : dmJsonConfigBase` c `Defaults()`.
- `src/core/4_World/Entities/Bot/Loot/dmLootingSettings.c`: ленивый синглтон, автосоздание
  файла с дефолтами при первом запуске.
- Константы: `DM_SETTINGS_DIR` = `$profile:dmBotorama/settings`, `DM_LOOTING_SETTINGS_FILE` =
  `$profile:dmBotorama/settings/looting.json`.
- Переписать `dmBotState_Exploration`: `CanEnter() = !IsNothingToDo()`; обход
  «ближайшее непосещённое → интерьер-точки `GetRoamWorldPoints` → якорь → пометить посещённым»;
  выходы ставят `SetNothingToDo(true)` (список пуст / счётчик / процент / процент+время / время).
- Условия `dmBotCondition_InTransit`/`dmBotCondition_NothingToDo` + фабрики.
- `dmBotPreset_Nomad`: `.BlockWhen(InTransit)` на входах в Travel, `.BlockWhen(NothingToDo)` на входах в Exploration.
- Probe-op `explorerdump` в `dmE2EBridge` (локация, visited, флаги, таймер, текущее здание).
- Гейт: сценарии «5 зданий» / «5 минут» / процент; уборка зомби; профайлинг.

### Веха E — рабочий `dmBotState_Travel`

- Переписать `dmBotState_Travel`: `INTERRUPTIBLE`; `CanEnter()` = `!IsInTransit() &&
  (IsNothingToDo() || GetLocationAt(pos)==null) && GetNearestUnvisitedLocation(bot)!=null`.
- `OnEntry`: взять `m_Destination` (или выбрать), `SetInTransit(true)`, снап назначения на
  навмеш (`bot.SampleNavmesh`, фолбэк `SurfaceY`), `MoveTo` с `m_ReachDistance = GetArrivalRadius(type)`.
- `OnUpdate`: угроза→EXIT; `MoveTo` finished → сброс `inTransit`/`nothingToDo`, `ArriveAtLocation`, EXIT;
  `MoveTo` failed → `RememberLocation(id)` (анти-цикл), сброс флагов, EXIT.
- `OnExit`: `m_Move.Finish()` + `SetInTransit(false)` (прерывание боем не залипает).
- Удалить «жажду»: `GetNearest(WATER)`, `dmBotCondition_Thirsty`, `DM_TRAVEL_WATER_THRESHOLD`,
  `DM_TRAVEL_POI_SEARCH_RADIUS`, `DM_TRAVEL_REACH_DISTANCE`.
- Гейт: сквозной сценарий (спавн вне локации → Travel → прибытие → Exploration → лут →
  «нечего делать» → Travel → цикл).

---

## 3. Процесс (обязательный)

- **Мелкие куски**: в каждой вехе — маленькие под-шаги, каждый подтверждается тестировщиком;
  не решать большой этап разом.
- **Гейт вехи**: только после ревью «придиры» (критично/важно → rework у `dayz-dev`;
  косметика → `docs/techdebt.md`) И прохождения теста.
- **Сомнения по API/ванили** → `dayz-research` (гипотезы `[нужно проверить]` → `dayz-tester`).
- Бюджет ≤3 rework/delegate-циклов на задачу; probe-ручки — батчем заранее.
- Рефлексия ошибок по классификации из `AGENTS.md`; развилки → `docs/decisions.md`.
- Bump `DM_BOTORAMA_VERSION` в каждом коммите вехи.

## 4. Константы vs настройки

- **Константы** (компиляционные, геометрия/движок) — в `src/cons/4_World/constants.c`
  (`DM_EXPLORE_INTERIOR_REACH` и т.п.).
- **Настройки поведения** (пороги завершения Exploration) — в JSON
  `$profile:dmBotorama/settings/looting.json`, читаются `dmLootingSettings`.

## 5. Каталоги (доступ)

- `src/cons/3_Game/Config/` — `dmJsonConfigBase.c`, `dmJsonFile.c` (создать папку).
- `src/core/3_Game/Config/` — `dmLootingConfig.c`, `dmSpawnConfig.c` (создать папку).
- `src/core/5_Mission/System/` — `dmBotSpawnManager.c` (создать папку).
- `src/core/4_World/Entities/Bot/Loot/` — `dmLootingSettings.c` (рядом с `dmExplorer.c`).
- Runtime: `$profile:dmBotorama/settings/looting.json` (автосоздание),
  `$profile:dmBotorama/map/{world_poi,buildings_interior,spawn}.json`.
- Probe-ops: `src/test/4_World/dmE2EBridge.c` (`buildingdump`, `poidump`, `explorerdump`).

## 6. Git

- Ветка `feature/nomad-exploration` от `master`.
- Коммитить только exploration-файлы; хвосты вождения (`bugreport.txt`, `pics/*`,
  `tools/e2e/drive-*.json`) не трогать.
