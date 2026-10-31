# Research: источники лута для Exploration (wrecks / трупы / контейнеры / гейт energy-water)

Статус: исследование API. Ведёт субагент `dayz-research`.
Дополняет `docs/research/dynamic-events.md` (конфиги событий), `world-poi.md` (классификация),
`inventory-enumeration.md` (перечисление инвентаря), `loot.md` (взятие/дроп), `food-consume.md`
(энергия/вода). НЕ дублирует уже известное из этих заметок.

## Цель

Дать разработчику готовые сигнатуры + каноничные предикаты для четырёх расширений лутинга:
(а) детект крушения/военного врека/конвоя; (б) детект трупа (игрок/зомби) + перечисление его лута;
(в) открытие + перечисление + извлечение из контейнера (бочка/ящик/сундук); (г) гейт energy/water
для похода к приоритетному событию.

---

## 1. Динамические события (helicrash / convoy)

### 1.1 Иерархия классов — ВСЕ wrecks это `House`

| Класс | Базовый класс | file:line |
|---|---|---|
| `class CrashBase extends House` | `House` | `scripts/4_world/entities/building/crashbase.c:1` |
| `class Wreck_UH1Y extends CrashBase` | `CrashBase` | `scripts/4_world/entities/building/wrecks/wreck_uh1y.c:1` |
| `class Wreck_Mi8_Crashed extends CrashBase` | `CrashBase` | `wrecks/wreck_mi8.c:2` |
| `class Wreck_Mi8 extends CrashBase` | `CrashBase` | `wrecks/wreck_mi8.c:10` |
| `class Wreck_SantasSleigh extends CrashBase` | `CrashBase` | `wrecks/wreck_santassleigh.c:2` |
| `class BuildingBase : Building` | `Building` | `scripts/4_world/entities/game/super/building.c:1` |
| `class House : BuildingBase` | `BuildingBase` | `building.c:85` |

**Конвой/военные вреки — `HouseNoDestruct`** (все в `DZ/structures/wrecks/config.cpp`):
- `Land_Wreck_*`, `Land_wreck_truck01_*_DE`, `StaticObj_Wreck_*` → **все** объявлены
  `: HouseNoDestruct` (напр. `Land_Wreck_Uaz: HouseNoDestruct` :17, `Land_Wreck_V3S: HouseNoDestruct` :580,
  `StaticObj_Wreck_BMP2: HouseNoDestruct` :1597, `StaticObj_Wreck_T72_Chassis_DE: HouseNoDestruct` :1784).
- `HouseNoDestruct` — **движковый (C++) класс**, в скриптах только forward-декларация
  (`DZ/structures/wrecks/config.cpp:16` — `class HouseNoDestruct;`); он наследует `House`
  (это подтверждено `cfgeconomycore.xml` `<rootclass name="HouseNoDestruct" .../>` и research
  `dynamic-events.md` §4.2).
- Поездные контейнеры `Land_Container_1Mo/1Moh/1Bo/1Aoh[_DE]` — тоже `HouseNoDestruct`
  (`DZ/structures/industrial/config.cpp:2258+`).

**Вывод:** любой crash/wreck (вертолёт, конвой, поезд) — это `House` → конструктор
`modded class House` (`src/reg/4_World/modded_House.c`) уже регистрирует их в
`dmLiveBuildingRegistry`. Отдельного события-«не-House» в классе `LandEventBase`/`EventBase`/
`DynamicEventBase` **в ванили нет** (grep пуст; только sound-event-классы).

### 1.2 Есть ли динамические event-сущности НЕ `House`

- **Целые машины** (`CivilianSedan`/`Hatchback_02`/… из Vehicle*-событий) — `CarScript : Transport`,
  **НЕ** `House` → в реестр зданий не попадают. Это «ездящие» машины, не wreck.
- `Wreck_SantasSleigh` при спавне плодит мёртвых оленей `Animal_RangiferTarandus`
  (`wreck_santassleigh.c` `SpawnRandomDeers`) — это `AnimalBase`, не House (лут — отдельная тема,
  не этой задачи).
- Детектить не-House события на сервере — только world-бокс-скан (`SceneGetEntitiesInBox`,
  `QueryFlags.STATIC`/`DYNAMIC`) + классификация по классу/`IsInherited`; CE-API списка активных
  событий не даёт (см. `dynamic-events.md` §4.1 — `GetCEApi()` только диагностика/спавн).

### 1.3 Каноничный предикат «крушение / военный врек / конвой»

```c
// (а) вертолётные крушения — надёжно по классу:
bool isCrash = CrashBase.Cast(building) != null;                 // Wreck_UH1Y/Mi8/Mi8_Crashed/SantasSleigh

// (б) статические военные вреки — по интерьер-карте:
bool isMilWreck = dmBuildingInteriorMap.Get().GetType(building.GetType()) == dmWorldPOIType.MILITARY_WRECK;
```

**Оба предиката вместе** (`isCrash || isMilWreck`) дают «военный/крушение-лут». `CrashBase.Cast`
надёжен для вертолётов; `GetType(...)==MILITARY_WRECK` — для уже замапленных статик-вреков
(`StaticObj_Wreck_*`, `Land_Wreck_C130J`, `Land_Wreck_V3S`).

### 1.4 Гэпы классификации (важно — проверить перед релизом)

`data/map/buildings_interior.json` (генерится `tools/generate_map_data.py`, префикс-классификатор
MILITARY_WRECK на строках 29-30: `Land_Wreck_C130J`, `Land_Wreck_V3S`, `StaticObj_Wreck`,
`Wreck_Mi8`, `Wreck_UH1Y`):

- **Вертолёты `Wreck_UH1Y`/`Wreck_Mi8`/`Wreck_Mi8_Crashed` — ЕСТЬ в JSON с типом
  `MILITARY_WRECK`** (проверено по `data/map/buildings_interior.json`: `Wreck_UH1Y`,
  `Wreck_Mi8`, `Wreck_Mi8_Crashed` → MILITARY_WRECK; `Wreck_SantasSleigh` → GENERIC).
  Т.е. `GetType()==MILITARY_WRECK` покрывает вертолётные крушения **без** `CrashBase.Cast`.
  (`CrashBase.Cast` — необязательный belt-and-suspenders для вертолётов.)
- **Конвойные гражданские вреки** (`Land_Wreck_Ikarus_DE`, `Land_wreck_truck01_*_DE`,
  `Land_Wreck_offroad02_*_DE`, `Land_Wreck_hb01_*_police_DE`, `Land_Wreck_Volha_Police`) в JSON
  помечены **GENERIC**, а `Land_Wreck_Uaz_DE` — вообще отсутствует. `GetType()==MILITARY_WRECK`
  для них **false**.
- Реально `MILITARY_WRECK` в JSON — 17 классов (включая `Wreck_UH1Y/Mi8/Mi8_Crashed`,
  `Land_Wreck_V3S_DE`, `Land_Wreck_C130J_Cargo`, `StaticObj_Wreck_BMP1/2_DE`, `StaticObj_Wreck_BRDM_DE`,
  `StaticObj_Wreck_HMMWV_DE`, `StaticObj_Wreck_T72_Chassis_DE`, `StaticObj_Wreck_Train_*_DE`).

**Рекомендация:** если нужен полный охват конвоя — добавить фолбэк-эвристику по имени класса
(`GetType()` префикс `Land_Wreck_`/`Land_wreck_truck01`/`StaticObj_Wreck_` + суффикс `_DE`), либо
расширить классификатор `generate_map_data.py` и перегенерить JSON. `[нужно проверить]` — какие
конкретно конвойные классы нужны для Ливонии (миссия бота — `dayzOffline.enoch`, см. dynamic-events.md).

---

## 2. Трупы (зомби и игрок) — труп ИГРОКА это `PlayerBase` (НЕ трансформируется)

### 2.1 Труп игрока остаётся `PlayerBase` с `IsAlive()==false`

| Сигнатура / факт | file:line |
|---|---|
| `int m_CorpseState` (поле `PlayerBase`) | `scripts/4_world/entities/manbase/playerbase.c:148` |
| `RegisterNetSyncVariableInt("m_CorpseState", -CORPSE_STATE_DECAYED, CORPSE_STATE_DECAYED)` | `playerbase.c:563` |
| `override void EEKilled(Object killer)` | `playerbase.c:1183` |
| в `EEKilled`: `g_Game.GetMission().InsertCorpse(this)` (условие `m_CorpseState == 0`) | `playerbase.c:1207` |
| `class CorpseData { PlayerBase m_Player; ... }` | `scripts/4_world/classes/corpsedata.c:1,16-27` |
| `MissionServer.ref array<ref CorpseData> m_DeadPlayersArray` | `scripts/5_mission/mission/missionserver.c:8` |
| `MissionServer.InsertCorpse(Man)` → `new CorpseData(PlayerBase.Cast(player), ...)` | `missionserver.c:774-777` |
| `UpdateCorpseStatesServer()` (прогресс гниения, ~30 c тик) | `missionserver.c:779-811` |
| `CORPSE_STATE_FRESH=0 / MEDIUM=1 / DECAYED=2` | `scripts/3_game/playerconstants.c:213-215` |

**Ответ на ключевой вопрос:** мёртвый игрок в мире — это **тот же `PlayerBase`** (конкретно
`SurvivorBase : PlayerBase`), **без трансформации класса**. Признак смерти — `IsAlive() == false`
(нативный, health-based), а не тип класса. `GetType()` вернёт `"SurvivorM_*"`/`"SurvivorF_*"` и т.п.
Совет «трупы будут не PlayerBase» **по скриптам НЕ подтверждается** — см. п.7-1 `[нужно подтвердить]`.

`m_CorpseState` **не подходит** как предикат смерти: `FRESH == 0` совпадает со значением у живого
(конструктор `m_CorpseState = 0`, `playerbase.c:370`; `EEKilled` потому и гейтится `== 0`). Его смысл —
степень гниения (0..2), отрицательное значение = «принудительно живой» (`flies.c:41/66`).

### 2.2 `IsAlive()` — нативный health-based на `Man`; базовый скриптовый — `!IsDamageDestroyed()`

| Сигнатура | file:line |
|---|---|
| `bool IsAlive() { return !IsDamageDestroyed(); }` (база `Object`) | `scripts/3_game/entities/object.c:523-526` |
| `proto native Man GetHierarchyRootPlayer()` | `scripts/3_game/entities/entityai.c:877` |
| `proto native EntityAI GetHierarchyRoot()` | `entityai.c:874` |
| `proto native EntityAI GetHierarchyParent()` | `entityai.c:880` |
| `DeleteSafe()`: `if (GetHierarchyRootPlayer()==null \|\| !GetHierarchyRootPlayer().IsAlive()) Delete();` | `entityai.c:786-799` |

`Man`/`PlayerBase`/`ZombieBase` переопределяют `IsAlive()` **нативно** (в скриптах переопределения
нет — `Object.IsAlive()` единственная скриптовая реализация). Для пешки `IsAlive()` — живое/мёртвое
(health > 0). Ключевое доказательство: `DeleteSafe()` отдельно проверяет `IsAlive()` у рута — значит
`GetHierarchyRootPlayer()` **не фильтрует** по «жив», и у вещи на трупе возвращает мёртвого `Man` (не null).

### 2.3 `GetHierarchyRootPlayer()`/`GetHierarchyRoot()` для предмета на трупе

| Ситуация | `GetHierarchyRoot()` | `GetHierarchyRootPlayer()` |
|---|---|---|
| Лужный предмет на земле (CE-лут, в т.ч. у трупа зомби) | сам предмет | `null` |
| Предмет в одежде/карго трупа игрока | труп (PlayerBase) | **мёртвый Man (не null)** |

**Влияние на мод:**
- `dmLoot.ScanNearbyItems` фильтр `!item.GetHierarchyRootPlayer()` (`dmLoot.c:290`) **исключает**
  лут на трупе игрока (корень = труп-`Man`, не null). Лут трупа игрока этим сканом НЕ виден.
- `dmBotIntent_PickUp` ждёт `m_Item.GetHierarchyRootPlayer() != null` как признак «вещь легла»
  (`dmBotIntent_PickUp.c:161,174`). Если вещь взята С ТРУПА, этот признак true **уже в источнике**
  (корень = труп) → интент ложно «завершится» сразу. Для трупного лута нужен отдельный путь/признак
  завершения (напр. сравнить `GetHierarchyRootPlayer() == bot.GetPawn()`).

### 2.4 Труп зомби

- `class ZombieBase extends DayZInfected` (`scripts/4_world/entities/creatures/infected/zombiebase.c:1`);
  `override bool IsZombie() → true` (`zombiebase.c:103`).
- **Ванильный лут зомби НЕ лежит в его инвентаре** — это loose-предметы, спавнимые CE на земле вокруг
  трупа (в скриптах `EEKilled`/`ProcessLoot`/`SpawnLoot` у `ZombieBase`/`DayZInfected` нет — всё нативно).
  Поэтому лут зомби уже ловится `ScanNearbyItems` (лужный предмет → `GetHierarchyRootPlayer()==null`).
- `GetInventory()`/`EnumerateInventory` на мёртвом зомби **работают** (это обычный `EntityAI` с валидным
  инвентарём), но у ванильного зомби карго пустое/незначимое — перечислять труп зомби бесполезно.
- Мёртвый зомби удаляется из `dmEntityRegistry.GetZombies()` (Cleanup, `dmEntityRegistry.c:40`), т.е.
  через реестр зомби его не найти — только world-скан, но это не нужно (лут — на земле).

### 2.5 Каноничный предикат «мёртвое тело с лутом» + перечисление

```c
// Труп игрока (в т.ч. мёртвый AI-выживший — dmAISurvivorBase : PlayerBase):
PlayerBase corpse = PlayerBase.Cast(entity);
if (corpse && !corpse.IsAlive()) { /* труп игрока */ }

// Труп зомби (редко нужен — лут на земле):
ZombieBase z = ZombieBase.Cast(entity);
if (z && !z.IsAlive()) { /* труп зомби */ }

// Перечисление ВСЕГО лута трупа игрока — тот же приём, что inventory-enumeration.md:
array<EntityAI> items = new array<EntityAI>();
corpse.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);
// + предмет в руках отдельно: corpse.GetHumanInventory().GetEntityInHands();
```

`EnumerateInventory` работает на трупе (это `PlayerBase`), возвращает одетую одежду + всё карго +
оружие в слотах; предмет в руках — отдельно (`GetEntityInHands()`). Для фильтра «дропаемое карго»
см. `dmLoot.CollectCargoFiltered`/`CollectCargoRecursive` (инвентарь трупа ничем не отличается от
инвентаря живой пешки). Извлечение вещи из трупа — обычный `dmLoot.TakeIntoCargo`/`TakeIntoDestination`
(`LocalTakeToDst`), т.к. одежда/карго трупа — обычные `InventoryLocation`.

**Не путать с `IsCorpse()`** (`object.c:734`, override `edible_base.c:372` → true) — это **съедобная
туша** (курица/кролик), НЕ труп игрока/зомби.

---

## 3. Контейнеры (Barrel_ColorBase / WoodenCrate / SeaChest) — бочку нужно открыть

### 3.1 Иерархия + API открытия

| Сигнатура | file:line |
|---|---|
| `class Container_Base : ItemBase` | `scripts/4_world/entities/itembase/container_base.c:1` |
| `class DeployableContainer_Base : Container_Base` | `container_base.c:32` |
| `class Barrel_ColorBase : DeployableContainer_Base` | `scripts/4_world/entities/itembase/barrel_colorbase.c:1` |
| `class SeaChest : DeployableContainer_Base` | `scripts/4_world/entities/itembase/gear/camping/camping.c:5` |
| `class WoodenCrate : DeployableContainer_Base` | `camping.c:17` |
| `void Open(); void Close(); bool IsOpen() { return true; }` (база `ItemBase`) | `scripts/4_world/entities/itembase.c:4146-4152` |
| `override bool CanDisplayCargo() { return IsOpen(); }` | `itembase.c:4153-4156` |
| `class OpenableBehaviour { Open()/Close()/IsOpened() }` | `scripts/4_world/entities/itembase/openablebehaviour.c:1` |

`Open()`/`Close()` в базе `ItemBase` объявлены **без тела** («Implementations only») — реализуются
только у классов, которые переопределяют `IsOpen()`. `IsOpen()` по умолчанию **true**.

**Barrel_ColorBase** (переопределения, точные строки `barrel_colorbase.c`):
| Метод | file:line | Поведение |
|---|---|---|
| `RegisterNetSyncVariableBool("m_Openable.m_IsOpened")` | :21 | сетевая синхронизация открытости |
| `override void Open()` | :87 | `m_Openable.Open()` + `SetTakeable(false)` + визуал |
| `override void Close()` | :106 | `m_Openable.Close()` + `SetTakeable(true)` |
| `override bool IsOpen()` | :125 | `m_Openable.IsOpened()` |
| `bool IsLocked()` / `void Lock(float)` / `void Unlock()` | :82 / :165 / :171 | замок (время крафта) |
| `override bool CanReceiveItemIntoCargo(item)` | :512 | `IsOpen()` → иначе false |
| `override bool CanReleaseCargo(cargo)` | :520 | `IsOpen()` |
| `override bool CanPutInCargo(parent)` | :490 | только пустую и закрытую |

**SeaChest / WoodenCrate** НЕ переопределяют `Open/Close/IsOpen` → `IsOpen()` всегда **true**, карго
**всегда доступно** (открывать не нужно; «крышка» у них чисто визуальная).

### 3.2 Каноничный способ открыть контейнер ИИ-боту (без action-manager)

```c
if (!container.IsOpen())
    container.Open();   // ItemBase.Open() — ровно это делает ActionOpen.OnExecuteServer (actionopen.c:50)
```

Зеркалит ванильный `ActionOpen.OnExecuteServer` → `action_data.m_MainItem.Open()` (`actionopen.c:45-50`)
и `ActionClose.OnExecuteServer` → `m_MainItem.Close()` (`actionclose.c:54-56`). Вызов `Open()` безопасен
только когда `!IsOpen()` (иначе у класса без реализации `Open()` — crash) — ровно это условие
`ActionOpen.ActionCondition` (`actionopen.c:24-33`). Для SeaChest/WoodenCrate `IsOpen()==true` → `Open()`
никогда не вызовется.

### 3.3 Блокирует ли `LocalTakeToDst` извлечение из ЗАКРЫТОЙ бочки — ДА

`LocalTakeToDst` → нативный `LocationSyncMoveEntity` (`inventory.c:567,1067`) внутри вызывает
**движковые скрипт-условия** (см. doc-комментарии `entityai.c`):

| Скрипт-условие | Кто | Где переопределено у бочки | file:line |
|---|---|---|---|
| `CanReleaseCargo(cargo)` | контейнер-источник | `return IsOpen();` | `entityai.c:1608` / `barrel_colorbase.c:520` |
| `CanRemoveFromCargo(parent)` | сам предмет | (база true) | `entityai.c:1619` |
| `CanReceiveItemIntoCargo(item)` | контейнер-приёмник | `return IsOpen()? super…:false;` | `entityai.c:1551` / `barrel_colorbase.c:512` |

Т.е. **извлечение из карго закрытой бочки вернёт `false`** (блокирует `CanReleaseCargo`→`IsOpen()`),
и **вставка в закрытую бочку** — тоже (`CanReceiveItemIntoCargo`). Что блокирует — именно
`CanReleaseCargo`/`CanReceiveItemIntoCargo` (по `IsOpen()`), а **НЕ** `AreChildrenAccessible()`
(тот — про глубину вложенности/достижимость: `entityai.c:1657`, возвращает false только если сущность
лежит в CARGO/PROXYCARGO; к открытию/закрытию крышки отношения не имеет).

### 3.4 Перечисление карго контейнера — НЕ требует открытия

`GetInventory().GetCargo()` / `EnumerateInventory` — **только чтение**, гейты `IsOpen()` стоят на
`CanRelease/CanReceive`, а не на перечислении. Открытость для ЧТЕНИЯ списка предметов не нужна.
(На клиенте `CanDisplayCargo()` → `IsOpen()` гейтит UI-отображение, но серверное перечисление
`GetCargo().GetItemCount()/GetItem(i)` работает и на закрытой бочке.)

### 3.5 `GetPosition()` предмета внутри карго контейнера

`[нужно проверить]` — возвращает ли валидную мировую позицию (у родителя-контейнера) или устаревшую
(последнюю мировую позицию до укладки). Для `PickUp.m_Goal` это критично: если позиция == позиция
контейнера — бот идёт к бочке (норм); если устаревшая — маршрут неверный. Probe: `spawnobj` бочку с
предметом внутри, `botdump` → `GetPosition()` предмета против `GetPosition()` бочки.

---

## 4. Энергия/вода (гейт приоритета событий)

### 4.1 Подтверждённые сигнатуры

| Сигнатура | file:line |
|---|---|
| `PlayerStat<float> GetStatWater()` | `scripts/4_world/entities/manbase/playerbase.c:7843` |
| `PlayerStat<float> GetStatEnergy()` | `playerbase.c:7861` |
| `class PlayerStat<Class T> : PlayerStatBase` — `Get()` / `Set(T,system)` / `Add(T,system)` / `GetMax()` / `GetMin()` / `GetNormalized()` | `scripts/4_world/classes/playerstats/playerstatbase.c` (`Set:85`, `Add:129`, `GetMax:144`, `GetMin:149`) |
| `SL_ENERGY_CRITICAL=0 / LOW=300 / NORMAL=800 / HIGH=3500 / MAX=5000` | `scripts/3_game/playerconstants.c:40-44` |
| `SL_WATER_CRITICAL=0 / LOW=300 / NORMAL=800 / HIGH=3500 / MAX=5000` | `playerconstants.c:46-50` |
| `METABOLIC_SPEED_ENERGY_{BASAL=0.01,WALK=0.1,JOG=0.3,SPRINT=0.6}` | `playerconstants.c:75-79` |
| `METABOLIC_SPEED_WATER_{...}` (те же значения) | `playerconstants.c:81-85` |
| мод: `DM_EAT_DRINK_ENERGY = 2500`, `DM_EAT_DRINK_WATER = 2500` | `src/cons/4_World/constants.c:969,971` |
| мод использует `pawn.GetStatEnergy().Get()` / `GetStatWater().Get()` | `dmBotIntent_EatDrink.c:75,85` |

**Диапазон: 0..5000** (`SL_ENERGY_MAX`/`SL_WATER_MAX`). `GetStatEnergy().GetMax()` вернёт 5000
(используется в `GetStatBordersEnergy`, `playerbase.c:5249-5256`).

### 4.2 Пороги «достаточно» и оценка бюджета на 1500 м

Ванильные смысловые уровни: **NORMAL=800** (ниже — статы влияют на здоровье), **HIGH=3500**.
Мод ест/пьёт ниже **2500** (половина запаса).

Расход: JOG 0.3/с, SPRINT 0.6/с, WALK 0.1/с (+BASAL 0.01/с). 1500 м jog (~4 м/с ≈ 375 с) ≈
**~115 energy** + **~115 water**; sprint (~7 м/с ≈ 215 с) ≈ **~130**. Т.е. даже «едва выше LOW»
(300) хватит на 1500 м, а «выше NORMAL» (800) — с запасом.

**Рекомендация для гейта** (предикат «у бота достаточно energy/water идти к событию до 1500 м»):

```c
bool ReadyForPriorityEvent(dmAISurvivor bot) {
    PlayerBase pawn = bot.GetPawn();
    // минимум — не деградирует здоровье по пути (NORMAL=800); комфорт — не ниже порога еды (2500).
    return pawn.GetStatEnergy().Get() >= DM_PRIORITY_EVENT_MIN_ENERGY   // напр. 800 (или 2500)
        && pawn.GetStatWater().Get()  >= DM_PRIORITY_EVENT_MIN_WATER;
}
```

- **Консервативно**: порог `>= DM_EAT_DRINK_ENERGY` (2500) — «полный» бот, идёт без остановок на еду.
- **Либерально**: порог `>= 800` (`SL_ENERGY_NORMAL`) — «не умрёт по пути», может перекусить после.
Выбор — за дизайном; для «приоритетное событие» разумно `>= 2500` (полный запас → не отвлекается),
а при 800..2500 — идти, но разрешить EatDrink-интенту параллельно (он и так PARALLEL/IDLE,
`dmBotIntent_EatDrink.c:16-18`).

---

## Открытые вопросы / гипотезы

1. **[ПОДТВЕРЖДЕНО, E2E v3.406] Труп игрока действительно `PlayerBase` (не трансформируется), `IsAlive()==false`.**
   `corpsedump` после `setstat health 0` бота: `corpse type=dmAI_SurvivorM_Denis alive=false player=1 zombie=0`.
   Совет «трупы будут не PlayerBase» — опровергнут (прокси = труп бота `dmAISurvivorBase : PlayerBase`; полная
   валидация на живом игроке — только с наблюдателем).
2. **[ПОДТВЕРЖДЕНО, E2E v3.406] `GetHierarchyRootPlayer()` у предмета на трупе — не null (мёртвый Man).**
   `corpseItem i=0 type=GorkaPants_Autumn rootPlayer=dmAI_SurvivorM_Denis alive=false`.
3. **[ПОДТВЕРЖДЕНО косвенно, Atom 1 E2E] Извлечение из закрытой бочки требует `Open()`.** Сценарий
   `explore-container-loot` — бот сам открыл закрытую бочку (`Открыл контейнер Barrel_Green`) и забрал содержимое.
4. **[ПОДТВЕРЖДЕНО, Atom 1 E2E] Перечисление карго закрытой бочки работает** (бот увидел содержимое до/после открытия).
5. **[ПОДТВЕРЖДЕНО, Atom 1 E2E] `GetPosition()` предмета в карго** — решено якорем на позицию контейнера (m_Goal),
   работает (бот идёт к бочке).
6. **[нужно проверить] Какие конвойные классы реально спавнятся на Ливонии** и нужно ли их до-классифицировать
   как MILITARY_WRECK (п.1.4) — `SpawnDE`/серверный лог + `scanbox` вокруг точки конвоя.
7. **[ПОДТВЕРЖДЕНО, Atom 1 E2E] SeaChest/WoodenCrate `IsOpen()==true` всегда** — `Open()` для них не вызывается.

### Эмпирика (hypothesis-прогон v3.406, ручка `corpsedump`)

- **Труп ЗОМБИ**: `corpse type=ZmbM_PatrolNormal_Autumn alive=false zombie=1`, НО `GetInventory()==null` →
  ручка не перечислила лут (ни `corpseItem`, ни `corpseEmpty`). Лут зомби **недоступен через GameInventory**
  у bare-спавна (`meleefight` → `CreateObject` без CE-init). Реальный CE-спавненный зомби может иметь инвентарь
  (ваниль: зомби носит одежду/лут) — НЕ подтверждено. `[нужно подтвердить]` на CE-спавненном зомби.
- **Cargo-предметы игрока при смерти пропадают** (эмпирика): `givecargo` Apple×2+BakedBeansCan → после смерти в
  трупе только одежда (`GorkaPants_Autumn`), `scanbox` — только сам труп. ВАЖНО: вероятный артефакт
  `EnumerateInventory(LEVELORDER)` (null-записи для вложенного карго, см. inventory-enumeration.md §7-2, решение B);
  строго НЕ подтверждено, что это реальная механика смерти. Для стрипа трупа — ручная рекурсия (вариант B),
  НЕ LEVELORDER.

---

## Существенные развилки и принятые решения (для docs/decisions.md)

1. **Детект «крушение/врек» строить на `GetType()==MILITARY_WRECK`, `CrashBase.Cast` — опционально.**
   Почему: вертолёты (`Wreck_UH1Y/Mi8/Mi8_Crashed`) **уже** в `buildings_interior.json` с
   `MILITARY_WRECK`, поэтому одного `GetType()==MILITARY_WRECK` достаточно для крушений + статик-вреков.
   `CrashBase.Cast` — belt-and-suspenders; конвойные гражданские вреки (GENERIC) им всё равно не покрыть
   (нужен фолбэк по префиксу имени, если понадобятся).
2. **Труп игрока = `PlayerBase` с `IsAlive()==false`, НЕ отдельный класс.** Почему: вся ваниль
   (`CorpseData`, `EEKilled`→`InsertCorpse`, `m_CorpseState`) держит труп как тот же `PlayerBase`.
   Значит лут трупа перечисляется стандартным `EnumerateInventory`, а не особым API. Действует пока: E2E
   (п.7-1) не покажет обратного.
3. **Открытие контейнера — прямой `ItemBase.Open()` (после `!IsOpen()`), а не action-manager.** Почему:
   `ActionOpen.OnExecuteServer` сводится к `item.Open()`; SeaChest/WoodenCrate и так всегда «открыты»
   (`IsOpen()==true`), а бочка гейтится `CanReleaseCargo`/`CanReceiveItemIntoCargo` → `IsOpen()`. Действует
   пока: не появится контейнер с нестандартным open/close, где `Open()` требует анимации/условий.

## Источники (file:line)

- Ваниль `DayZ Projects/scripts/`:
  - `4_world/entities/building/crashbase.c:1` — `CrashBase : House`.
  - `4_world/entities/building/wrecks/{wreck_uh1y.c:1, wreck_mi8.c:2/10, wreck_santassleigh.c:2}`.
  - `4_world/entities/game/super/building.c:1,85` — `BuildingBase : Building`, `House : BuildingBase`.
  - `3_game/entities/object.c:523-526` (`IsAlive` база), `:734` (`IsCorpse`).
  - `3_game/entities/entityai.c:874/877/880` (`GetHierarchyRoot*`), `:786-799` (`DeleteSafe`),
    `:1547/1566/1577/1608/1619` (`Can*Cargo`-условия), `:1657` (`AreChildrenAccessible`).
  - `3_game/systems/inventory/inventory.c:442-448` (`AreChildrenAccessible`), `:567` (`LocationSyncMoveEntity`),
    `:1056-1068` (`TakeToDst`).
  - `4_world/entities/manbase/playerbase.c:148,563,1183,1207` (труп), `:7843/7861` (`GetStatWater/Energy`),
    `:5249-5256` (`GetStatBorders*` → `GetMax`).
  - `4_world/classes/corpsedata.c:1-27`; `5_mission/mission/missionserver.c:8,774-811`.
  - `3_game/playerconstants.c:40-50` (SL_*), `:75-85` (METABOLIC_*), `:213-215` (CORPSE_STATE_*).
  - `4_world/entities/itembase.c:4146-4156` (`Open/Close/IsOpen/CanDisplayCargo`).
  - `4_world/entities/itembase/{container_base.c:1,32, barrel_colorbase.c:1/21/82/87/106/125/165/171/490/512/520, openablebehaviour.c:1, gear/camping/camping.c:5,17}`.
  - `4_world/classes/useractionscomponent/actions/singleuse/{actionopen.c:45-50, actionclose.c:54-56}`.
  - `4_world/classes/playerstats/playerstatbase.c` (`Set:85`, `Add:129`, `GetMax:144`, `GetMin:149`).
  - `4_world/entities/creatures/infected/zombiebase.c:1,103`; `3_game/entities/dayzinfected.c:103`.
- Ваниль `DayZ Projects/DZ/`:
  - `structures/wrecks/config.cpp` (`Land_Wreck_*`/`StaticObj_Wreck_*` → `HouseNoDestruct`; `:16` forward-декларация).
  - `structures/industrial/config.cpp:2258+` (`Land_Container_*` → `HouseNoDestruct`).
  - `gear/containers/config.cpp:19,95` (`Barrel_ColorBase`, `itemsCargoSize[]={10,15}`).
  - `gear/camping/config.cpp:9820/9846` (`SeaChest`, `{10,10}`), `:10074/10180` (`WoodenCrate`, `{10,5}`).
- Мод botorama:
  - `src/reg/4_World/modded_House.c` (хук регистрации House), `dmLiveBuildingRegistry.c`, `dmBuildingInteriorMap.c:60/123-136`.
  - `src/reg/4_World/dmEntityRegistry.c:31-53` (Cleanup: зомби/животные дропаются по `!IsAlive()`, игроки — нет).
  - `src/core/4_World/Entities/Bot/Loot/dmLoot.c:273-294` (`ScanNearbyItems`), `:298-354` (CollectCargo).
  - `src/core/4_World/Entities/Bot/Intent/dmBotIntent_PickUp.c:161,174` (завершение по `GetHierarchyRootPlayer()!=null`).
  - `src/core/4_World/Entities/Bot/Intent/dmBotIntent_EatDrink.c:75,85`; `src/cons/4_World/constants.c:969,971`; `src/cons/3_Game/constants.c:78-91` (`dmWorldPOIType`).
  - `tools/generate_map_data.py:29-30` (классификатор MILITARY_WRECK), `data/map/buildings_interior.json`.
