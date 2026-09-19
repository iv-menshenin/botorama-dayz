# Research: динамические события, блокирующие дороги (для объезда водителем)

Статус: исследование конфигов миссии + ванильного CE API. Ведёт `dayz-research`.

## Цель

Бот-водитель (`dmBotIntent_Drive`) едет по дорожному графу и встречает на осевой
«динамические события» (разбитые машины `Land_Wreck_*`, `StaticObj_Wreck_*`). Нужно
понять, как события описаны в конфигах миссии/игры, какова геометрия «заблокированного
пятна» и как его объехать, а также можно ли в рантайме узнать, где стоит событие.

---

## Где лежат конфиги (важно: источники в WORKSPACE, а не `/mnt/deep-space`)

Запрошенный путь `/mnt/deep-space/Steam/.../dayzOffline.chernarusplus/` **вне доступных
источников**. Реальная миссия бота — **Livonia** (`dayzOffline.enoch`), и она есть в
workspace. Полный комплект конфигов нашёлся в двух местах:

- Миссия бота (Livonia): `/home/devalio/dayz/Work/dayz-devaliada/Server/mpmissions/dayzOffline.enoch/`
  (и копия в `deploy/mission/dayzOffline.enoch/`). Файлы: `cfgeventspawns.xml`,
  `cfgeconomycore.xml`, `db/{globals,messages,types}.xml`, `expansion_ce/expansion_events.xml`.
- **Ванильный `db/events.xml` (Chernarus)** — полная копия в
  `/home/devalio/dayz/Work/DayZ-CommunityOnlineTools/Missions/COT.ChernarusPlus/db/events.xml`
  (усечённая версия: без групповых событий-групп, см. ниже).
- **Серверный консольный лог с фактическим спавном событий** (авторитетнее конфигов):
  `/home/devalio/dayz/Work/dayz-devaliada/deploy/logs_backup_*/server_console.log`.

**Важно:** у миссии `dayzOffline.enoch` **НЕТ `db/events.xml`** — значит сервер
использует **ванильный** `events.xml` (лежит в `dta/` инсталляции DayZServer, вне
workspace). Поэтому типы объектов брал из COT-копии events.xml + из серверного лога.

---

## 1. Какие события ставят объекты НА дорогу (релевантны водителю)

Источники: `cfgeventspawns.xml` (enoch), серверный лог, COT `db/events.xml`.

| Событие | Что спавнит (типы из лога/events.xml) | Характер |
|---|---|---|
| **StaticPoliceCar** | `Land_Wreck_Volha_Police` (1 объект) | одна брошенная полицейская машина на дороге |
| **StaticPoliceSituation** | кластер `Land_Wreck_hb01_aban1_police_DE`, `Land_Wreck_hb01/02_aban2_*_DE`, `Land_Wreck_sed01/02_aban*_*_DE`, `Land_wreck_truck01_aban1_firetruck_DE`, `Land_Wreck_Tractor_DE`, `Land_Wreck_Ikarus_DE` (автобус), `Land_Wreck_Trailer_Closed_DE`, `Land_wreck_truck01_aban1_orange/blue_DE` | **полицейский roadblock / traffic**: много машин в кучу на дороге |
| **StaticMilitaryConvoy** | `StaticObj_Wreck_T72_Chassis_DE`, `StaticObj_Wreck_BMP2_DE`, `StaticObj_Wreck_BMP1_NoPlacement_DE`, `StaticObj_Wreck_Ural_DE`, `StaticObj_Wreck_Uaz_DE`, `Land_wreck_truck01_aban1/2_green_DE`, `Land_Wreck_offroad02_aban1/2_DE`, `Land_Wreck_V3S_DE` | **военная колонна** на дороге |
| **StaticTrain** | `StaticObj_Wreck_Train_742_*_DE`, `Land_Train_Wagon_Box*_DE`, `StaticObj_Wreck_Train_Wagon_*_DE`, `Land_Container_1Mo*/1Moh*/1Bo/1Aoh_DE`, + у части групп `Land_wreck_truck01_aban1_firetruck_DE` | составы на рельсах; `Train_Accident_*` группы могут пересекать дорогу |
| Vehicle* (CivilianSedan/Hatchback02/OffroadHatchback/Sedan02/Truck01/TransitBus) | целые машины (`CivilianSedan`, `Hatchback_02`, ...) | **ездящие** машины на дороге (не wreck, но тоже препятствие) |
| StaticHeliCrash | `Wreck_Mi8` / `Wreck_UH1Y` | в полях, вне дорог; малорелевантен |
| StaticContaminatedArea | газ (не объект) | не препятствие для геометрии |

**Вывод для водителя:** главные блокираторы дороги — `StaticPoliceCar`,
`StaticPoliceSituation`, `StaticMilitaryConvoy` (и, слабее, `StaticTrain`-Accident-группы).
Все их объекты — классы с суффиксом `_DE` (`Land_Wreck_*_DE`, `StaticObj_Wreck_*_DE`)
или `Land_Wreck_Volha_Police` — это `HouseNoDestruct` (см. §4), т.е. **статические
здания с коллизией**.

---

## 2. Как описано размещение объектов события

### 2.1 `events.xml` (типы/структура события)

Формат (из COT `db/events.xml`, парсер Expansion `ExpansionCEEvents.c`):

```xml
<event name="StaticPoliceCar">
    <nominal>10</nominal>      <!-- целевое кол-во активных событий -->
    <min>0</min> <max>0</max>
    <lifetime>2500</lifetime>
    <restock>0</restock>
    <saferadius>500</saferadius>
    <distanceradius>500</distanceradius>
    <cleanupradius>200</cleanupradius>
    <flags deletable="1" init_random="0" remove_damaged="0"/>
    <position>fixed</position>
    <limit>child</limit>
    <active>1</active>
    <children>
        <child lootmax="5" lootmin="3" max="10" min="10" type="Land_Wreck_Volha_Police"/>
    </children>
</event>
```

**Ключевой факт:** `<child>` несёт только `type` + `min`/`max`/`lootmin`/`lootmax`.
**Оффсетов `pos`/`rot` НЕТ** — ни в COT events.xml, ни в парсере `ExpansionCEEvents.c`
(он читает только эти 4 атрибута, строки 213–220). То есть **размещение объектов внутри
события в events.xml НЕ описано как явные offset'ы** — объекты спавнятся в точке события,
а для групповых событий раскладку задаёт нативный код CE (см. §2.3).

### 2.2 `cfgeventspawns.xml` (где и как ставятся)

Фрагменты из enoch `cfgeventspawns.xml` (полный файл 754 строки):

```xml
<event name="StaticPoliceCar">
    <pos x="6488.11377" z="11279.327148" a="331.097137" />
    <pos x="6359.906" z="10970.853" a="67.064" />
    ...
</event>

<event name="StaticMilitaryConvoy">
    <zone smin="0" smax="0" dmin="1" dmax="1" r="1" />
    <pos x="8772.478" z="8563.994" a="0" y="269.798" group="Abandoned_Borek"/>
    <pos x="4756.113" z="3061.519" a="0" y="413.901" group="Abandoned_Nadbor"/>
    <pos x="3273.258" z="4830.391" a="0" y="395.621" group="Ambushed_Adamow"/>
    ...
</event>

<event name="StaticPoliceSituation">
    <zone smin="0" smax="0" dmin="1" dmax="2" r="20" />
    <pos x="10260.424" z="4227.363" a="0" y="339.232" group="Block_Gieraltow"/>
    <pos x="2740.029" z="6913.033" a="0" y="257.667" group="Traffic_Adamow"/>
    ...
</event>
```

Атрибуты `<pos>`:
- `x`, `z` — мировые 2D-координаты (плоскость XZ, Y = высота).
- `a` — угол поворота события в градусах.
- `y` — опциональная высота (при её отсутствии CE снэпит на землю).
- `group` — имя «DE-группы» (для групповых событий: конвой/блок/поезд).

Атрибуты `<zone>`:
- `smin`/`smax` — мин/макс одновременно активных событий этого типа.
- `dmin`/`dmax` — мин/макс дистанция размещения (м).
- `r` — радиус разброса объектов события вокруг точки (для `StaticPoliceSituation` = **20 м**,
  `StaticMilitaryConvoy` = 1 м, `StaticHeliCrash` = 45 м, `StaticTrain` = 20 м).

### 2.3 Групповые события: раскладка объектов — НАТИВНАЯ, не в конфиге

`group="Abandoned_Borek"` / `group="Traffic_Adamow"` / `group="Block_*"` ссылаются на
«DE-группы» — списки объектов с относительными позициями. **Эти определения групп в
workspace НЕ нашлись**: их нет в COT events.xml (он усечён — без `StaticMilitaryConvoy`/
`StaticPoliceSituation`/`StaticTrain`), а ванильный полный events.xml лежит в `dta/`
DayZServer (вне доступа). Из серверного лога известны только **типы** объектов группы
(§1), но не их смещения.

**Следствие для геометрии:** точные offset'ы объектов внутри конвоя/блока недоступны
статически. Можно оценить лишь размер пятна (§5) по типу события и радиусу `r`.

---

## 3. Где события спавнятся

- Позиции **явные, рукописные** (`x`/`z` + `a` + опц. `y`) — **никакого «снэпа к дороге»/
  маркера «на дороге» в XML нет**. Автор просто вручную ставил точки на дорогах.
- `cfgeconomycore.xml` даёт дефолты `<defaults>` для `cfgeventspawns.xml`:
  ```xml
  <default name="dyn_radius" value="30" />
  <default name="dyn_smin"  value="0" />
  <default name="dyn_smax"  value="0" />
  <default name="dyn_dmin"  value="1" />
  <default name="dyn_dmax"  value="5" />
  ```
  → события **без явного `<zone r>`** (напр. `StaticPoliceCar` в enoch) получают
  `r=30` (дефолтный радиус разброса) — `[нужно проверить]` точную семантику `r`
  (разброс объектов vs радиус «окна» события).
- **Только подмножество позиций реально активно:** `nominal`/`min`/`max` задают целевое
  число активных событий (`StaticPoliceCar` nominal=10 при ~145 позициях в enoch;
  `StaticMilitaryConvoy`/`StaticPoliceSituation` nominal=0 → активны только те, что
  определены группами). → **нельзя заранее считать каждую позицию из cfgeventspawns.xml
  «заблокированной»** — там, где событие не заспавнилось, дорога свободна.

---

## 4. Рантайм-обнаружение событий

### 4.1 API Central Economy (ваниль, `3_game/ce/centraleconomy.c`)

- `proto native CEApi GetCEApi();` (`centraleconomy.c:745`) — **только на сервере**
  («Client does not have Hive when connected to a server, only the server does»).
- `proto native void SpawnDE(string sEvName, vector vPos, float fAngle = -1);` (`:363`)
  и `SpawnDEEx(name, pos, angle, flags)` (`:378`) — принудительный спавн события
  (помечены DEVELOPER/DIAG ONLY).
- `DynamicEventSpawn()` / `ToggleDynamicEventStatus` / `ToggleDynamicEventVisualisation` /
  `DynamicEventExport()` (`:710-716`) — только диагностика/визуализация/выгрузка в файл.

**НЕТ API «перечислить активные события» или «где стоят объекты события».** CE полностью
нативный (C++), скрипт видит только `CEApi` выше. `DynamicEventExport()` пишет файл,
а не возвращает данные в скрипт.

### 4.2 Объекты события — обычные здания в мире

`cfgeconomycore.xml:12` прямо подтверждает:
```xml
<rootclass name="HouseNoDestruct" reportMemoryLOD="no" /> <!-- houses, wrecks -->
```
→ все `Land_Wreck_*` / `StaticObj_Wreck_*` — это **`HouseNoDestruct`** (подкласс
`House → BuildingBase → Building → EntityAI`), обычные статические объекты мира с
физической коллизией. Иерархия подтверждена:
`BuildingBase : Building` (`4_world/entities/game/super/building.c:1`),
`House : BuildingBase` (`:85`).

**Значит рантайм-детект = обычный world-запрос, а не CE:**
- Райкаст вперёд (`DayZPhysics.RaycastRV`, `CollisionFlags.ALLOBJECTS`, `ObjIntersectGeom`)
  уже ловит эти здания (как и деревья/дома в `ProbeAhead`).
- Скан-бокс/сфера (`QueryFlags.STATIC`) ловит `Building`-объекты.
- Опознание «это wreck динамического события» — по имени класса: префикс
  `Land_Wreck_`/`StaticObj_Wreck_` + суффикс `_DE` (или `Land_Wreck_Volha_Police`), либо
  `Building`/`House`-каст. Отдельного «isDynamicEvent»-флага у объекта нет.

### 4.3 Когда событие заспавнено (критично для детекта)

Лог показывает: список событий печатается на старте сервера (`4:05:06`), но
`[CE][SpawnRandomLoot] (StaticPoliceSituation)` идут позже и повторяются
(`4:09:25`, `4:31:26`, `6:09:44`, ...) — то есть **события спавнятся динамически, когда
кто-то (игрок/бот) приближается в радиус `distanceradius`** (500 м для полиции/конвоя,
1000 м для вертолёта). До этого wreck'а на месте нет.

**Следствие для водителя:** заранее «прогреть» карту блокировок нельзя (издалека пятно
пустое, при подъезде — заспавнится). Детект должен быть **рантаймовым**: райкаст/скан
вперёд по ходу движения, с запасом не менее `distanceradius`-радиуса (≈500 м) или по
факту появления препятствия на пути.

---

## 5. Формула объезда (черновая прикидка по найденным числам)

Точные offset'ы объектов внутри группы недоступны (§2.3), поэтому оценки — по радиусу
`r`, типам объектов и здравому смыслу; всё помечено `[нужно проверить]`.

| Событие | `r` | Оценка размера пятна | Зазор объезда (черновой) |
|---|---|---|---|
| StaticPoliceCar | 30 (дефолт) | 1 машина ≈ 4.5×2 м | сойти с осевой на **4–6 м** вбок, вернуться через **10–15 м** |
| StaticMilitaryConvoy | 1 | линия техники вдоль дороги (колонна) ≈ 30–100+ м | уйти вбок на **10–15 м**, вернуться через **50–120 м** |
| StaticPoliceSituation | 20 | кластер машин в радиусе ~20 м → пятно до ~40 м диаметром | уйти вбок на **25–30 м**, вернуться через **40–60 м** |
| StaticTrain (Accident) | 20 | состав вдоль рельсов, длина десятки м | не на дороге; если пересекает — как конвой |

Общие цифры, опорные для констант:
- дефолтный радиус события `dyn_radius = 30` (cfgeconomycore.xml).
- блокирующая единица — машина ≈ 4.5×2 м (легковая) / до 7×2.5 м (грузовик/автобус).
- полицейский блок (r=20) — самый широкий «поперёк», военный конвой (r=1) — самый
  длинный «вдоль».

---

## Открытые вопросы / [нужно проверить]

1. **[нужно проверить] Семантика `r` в `<zone>`**: радиус разброса ОБЪЕКТОВ события вокруг
   точки, или радиус «окна» спавна события, или радиус разброса самой точки? Влияет на
   ширину пятна блокировки (для `StaticPoliceSituation` это разница между ~20 м и ~40 м).
2. **[нужно проверить] Раскладка объектов внутри DE-группы** (`Abandoned_*`/`Traffic_*`/
   `Block_*`/`Train_*`): точные offset'ы/ориентация недоступны статически (ванильный
   events.xml в `dta/`, вне workspace). Проверяемо только эмпирически: заспавнить событие
   (`SpawnDE`) и просканить объекты (`scanbox`/`botdump`) вокруг точки.
3. **[нужно проверить] `distanceradius`-спавн**: событие появляется при входе игрока/бота в
   500 м (полиция/конвой)? Или быстрее/медленнее? Определяет, на каком удалении бот должен
   начинать сканировать дорогу.
4. **[нужно проверить] Сколько из `nominal` позиций реально активно** на Livonia-сервере
   бота (`StaticPoliceCar` nominal=10 из ~145 точек, выбор случайный/по близости?).
5. **[нужно подтвердить]** Райкаст вперёд ловит `HouseNoDestruct`-wreck на высоте капота
   машины и на нужной дистанции (не проскакивает низкий `Land_Wreck_sed*`).

## Вывод

- **Детект — только рантаймовый.** CE-API не отдаёт список активных событий; события
  спавнятся нативным CE при приближении (distanceradius). Поэтому объезд строим на
  world-запросе (райкаст/скан) вперёд по дороге, а не на предвычислении блокировок из
  конфига (тем более что активна лишь часть позиций из cfgeventspawns.xml).
- **Опознание препятствия**: `Land_Wreck_*` / `StaticObj_Wreck_*` (+`_DE`) → `HouseNoDestruct`
  (здание с коллизией). Каста хватает, отдельного DE-флага нет.
- **Геометрия/зазор** (черновая): один wreck ≈ 4.5–7 м вдоль; полицейский блок (r=20) —
  пятно ~40 м; военный конвой — длинная линия вдоль дороги. Объезд «человеческим» способом:
  сместиться поперёк осевой на 5–30 м (по типу события), пройти вдоль 10–120 м, вернуться.
  Точные числа — после эмпирики п.1–2.

---

## Существенные развилки / принятые решения (для docs/decisions.md)

1. **Объезд дорожных событий строить на рантайм-детекте, а не на предвычислении из
   cfgeventspawns.xml.** Почему: (а) CE-API не даёт списка активных событий;
   (б) `nominal/min/max` активируют лишь подмножество позиций (предвычисление даёт ложные
   блокировки); (в) события спавнятся нативно при приближении (distanceradius) — издалека
   пятна нет. Решение: детект через райкаст/скан вперёд (`HouseNoDestruct`-объекты).
2. **Не тратить время на парсинг ванильного events.xml ради offset'ов групп.** Почему:
   полный events.xml вне workspace (`dta/`), а в доступных копиях/парсере offset'ы объектов
   не представлены — раскладка групп нативная. Решение: геометрию пятна оценивать по
   радиусу `r` события и эмпирике (`SpawnDE` + scanbox), а не по конфигу.
