# World POI (точки интереса) — research-заметка

**Цель**: выяснить, что ваниль DayZ даёт для построения реестра POI: именованные
населённые пункты (имя+позиция), категории зданий, водопои, и референс «спавна по
городам». Домен: `world-poi`.

Источники: ванильные скрипты (`/home/devalio/dayz/Work/DayZ Projects/scripts`),
референс Expansion (`/home/devalio/dayz/Work/DayZ-Expansion-Scripts`).

---

## Ключевой вывод (кратко)

| Что нужно | Ваниль даёт? | Как |
|---|---|---|
| Именованные локации (города/деревни) + имя + позиция | **ДА** | конфиг `CfgWorlds <WorldName> Names` (читать через `g_Game.ConfigGet*`) |
| Категория здания (полиция/пожарка/больница/военные/жилые) | **НЕТ** (метода нет) | рантайм-скан статики + классификация по классу (`GetType()`/`IsInherited`) |
| Перечислить все здания карты | **ДА** | `DayZPlayerUtils.SceneGetEntitiesInBox` на всю карту |
| Водопои-колодцы (wells) | **ДА** | скан статики + `IsWell()` / `GetWaterSourceObjectType()` |
| Водопои-пруды/озёра (ponds) | **НЕТ** (не сущности) | поверхность террейна через `SurfaceInfo.GetLiquidType()` — нужен грид-скан |
| Спавн-поинты CE / «по городам» | **НЕТ** нативного API | читать mission XML (`$mission:cfgplayerspawnpoints.xml`, `env\*_territories.xml`) файлами |

---

## 1. Населённые пункты / именованные локации — **ДА, через конфиг `CfgWorlds`**

**НЕ `CfgLocationTypes`/`cfgLocationTypes`** — этих классов/конфигов в ванильных
скриптах нет (grep по всему дереву пуст). Правильный источник — базовый игровой
конфиг `CfgWorlds <worldName> Names`.

### Подтверждённая структура конфига (референс Expansion)

`DayZ-Expansion-Scripts/DayZExpansion/Core/Scripts/3_Game/DayZExpansion_Core/locations/expansionlocation.c:91-177`
(`ExpansionLocationT<Class T>::GetWorldLocations`):

```c
string worldName = g_Game.GetWorldName();
string location_config_path = "CfgWorlds " + worldName + " Names";
int classNamesCount = g_Game.ConfigGetChildrenCount(location_config_path);
for (int l = 0; l < classNamesCount; ++l) {
    g_Game.ConfigGetChildName(location_config_path, l, location_class_name);
    g_Game.ConfigGetText(location_class_name_path + " type", location_type);     // "Capital"/"City"/"Village"/"Local"/"Camp"/"Ruin"/"Marine"/"Hill"/"ViewPoint"/"Suburb"
    g_Game.ConfigGetText(location_class_name_path + " name", location_name);     // имя
    g_Game.ConfigGetFloatArray(location_class_name_path + " position", pos);     // float[2] = {x, z}
}
```

Итого у каждой именованной локации есть **имя**, **тип** (`Capital`/`City`/`Village`/
`Local`/`Suburb`/`Camp`/…), **позиция** (`float[2]`, X/Z, высота не задана). Expansion
добавляет радиус эвристикой по типу (`expansionlocation.c:48-69`: Capital=1000м,
City=500м, Village/Suburb=200м, Camp/Local=100м).

### Подтверждение, что `CfgWorlds` читается ванилью

- `3_game/global/game.c:931-938` — `GetWorldName(out string)` / `GetWorldName()`.
- `4_world/entities/core/inherited/inventoryitem.c:1060` — `"CfgWorlds " + g_Game.GetWorldName()` → читает `mapDisplayNameKey`, `mapTextureClosed` и т.д.
- `5_mission/gui/mapmenu.c:92` — `g_Game.ConfigGetVector("CfgWorlds %1 centerPosition", g_Game.GetWorldName())`.
- `4_world/entities/itembase/mapnavigationbehaviour.c:17` — `GRID_SIZE_CFG_PATH = "CfgWorlds %1 Grid Zoom1 stepX"`.

### Config API (нативный, на `Game`)

`3_game/global/game.c:448-614`:
- `proto bool ConfigGetText(string path, out string value)` (:448), `ConfigGetTextRaw` (:457)
- `proto native vector ConfigGetVector(string path)` (:531)
- `proto native float ConfigGetFloat(string path)` (:523), `proto native int ConfigGetInt(string path)` (:538)
- `proto native void ConfigGetFloatArray(string path, out TFloatArray values)` (:577), `ConfigGetIntArray` (:584), `ConfigGetTextArray` (:557)
- `proto bool ConfigGetChildName(string path, int index, out string name)` (:593)
- `proto native int ConfigGetChildrenCount(string path)` (:610), `proto native bool ConfigIsExisting(string path)` (:611)
- `proto native void ConfigGetFullPath(...)` (:613-614)

Классы в пути разделяются пробелом; опционально `configFile`/`missionConfigFile` как первый элемент.

### Вывод

**Можно**: получить список городов/деревень с именем и позицией из ванили без
рантайм-скана — через `CfgWorlds <WorldName> Names`. Это ровно тот список, что
рисуется на карте. Радиус POI — эвристика по типу (референс Expansion выше).
Высота не дана — дозаливать через `Game.SurfaceGetType3D`/`SampleNavmeshPosition`
при использовании.

**Готча**: имя — локализационный ключ (`#str_...`), нужен `FormatRawConfigStringKeys`
(`game.c:476`) / перевод. `CfgWorlds` — базовый конфиг игры, данные живут не в
`.c`-файлах, а в игровых данных (недоступны здесь для проверки структуры
построчно — но читаемость подтверждена ванильным и Expansion-кодом).

---

## 2. Категории зданий — **НЕТ метода, нужна классификация по классу**

### Что есть на `Building` / `Object`

- `3_game/entities/building.c:10-273` — класс `Building : EntityAI`. Методы только
  про двери (`GetDoorCount`, `OpenDoor`, …), `IsBuilding()` → `true` (:241),
  `CanObstruct()`, `IsHealthVisible()`. **Нет `GetBuildingCategory` / лутабельного класса.**
- `3_game/entities/object.c:517-771` — предикаты-категории, доступные на любом Object:
  `IsBuilding()` (:648), `IsWell()` (:665), `IsFuelStation()` (:676), `IsTransport()`,
  `IsTree()`, `IsRock()`, `IsBush()`, `IsFireplace()`, `IsContainer()`, `IsInventoryItem()`,
  `IsParticle()`, `IsHologram()` и т.д. **Нет `IsMilitary`/`IsHospital`/`IsPolice`/`IsResidential`/`IsIndustrial`.**
- Классификация по типу/наследованию:
  - `string GetType()` (`object.c:473`) — конфиг-класс (`"Land_Mil_Barracks1"`, `"Land_House_1W01"`).
  - `bool IsKindOf(string type)` (`object.c:517`), `bool IsInherited(typename)` (native на Class, `1_core/proto/enscript.c:23`), `bool IsAnyInherited(array<typename>)` (`object.c:945`).

### Как ваниль/Expansion классифицируют здания

- Ваниль группирует здания **по каталогу и классу** (не по рантайм-методу):
  `4_world/entities/building/{military,residential,industrial,specific,well,fuelstation,wrecks,signs,underground}`.
  Классы военных: `Land_Mil_*` (`4_world/entities/building/military/houses/mil_barracks1.c` и др.),
  жилых: `Land_House_*`, промышленных: `Land_..._Industrial` и т.д.
- Expansion (`DayZExpansion/AI/.../ExpansionWorld.c:156-159`) классифицирует по **префиксу класса**:
  ```c
  string type = candidate.GetType(); type.ToLower();
  if (ExpansionString.StartsWithAny(type, settings.ExcludedRoamingBuildings)) continue;
  ```

### CE-конфиг категорий зданий — НЕ доступен из скриптов

`cfgBuildingCategory` / `cfgLoot` / `cfgLootTable` / `mapgrouppos.xml` / `cfglimitsdefinition.xml` —
это **XML-данные CE в миссии**, нативного скриптового API к ним нет. `GetCEApi()`
(`3_game/ce/centraleconomy.c:226-737`) даёт только debug/диагностику спавна и
`EconomyMap`/`EconomyOutput` (логи/картинки), но **не** перечисление категорий зданий.
Expansion читает эти XML сам файловым парсером `CF_XML` (см. §4).

### Как перечислить ВСЕ здания карты

`DayZPlayerUtils.SceneGetEntitiesInBox` (`4_world/entities/dayzplayerutils.c:75`):

```c
static proto native void SceneGetEntitiesInBox(vector min, vector max, notnull out array<EntityAI> entList, int flags = QueryFlags.DYNAMIC);
```

- `enum QueryFlags` (`dayzplayerutils.c:1-9`): `NONE, STATIC, DYNAMIC, ORIGIN_DISTANCE, ONLY_ROADWAYS`.
  ⚠️ В скрипте значения идут подряд (0..4), но в коде применяются через `|`
  (`STATIC|DYNAMIC`, `STATIC|ORIGIN_DISTANCE`) — похоже, нативно трактуются как битовые
  флаги (STATIC=1, DYNAMIC=2, ORIGIN_DISTANCE=4, ONLY_ROADWAYS=8). Проверить на живом сервере.
- Полный охват карты — референс Expansion `GenerateRoamingLocations` (`ExpansionWorld.c:129-136`):
  ```c
  int worldSize = g_Game.GetWorld().GetWorldSize();   // World.GetWorldSize(): 3_game/global/world.c:85
  vector min = Vector(0, -1000, 0);
  vector max = Vector(worldSize, 1000, worldSize);
  DayZPlayerUtils.SceneGetEntitiesInBox(min, max, candidates, QueryFlags.STATIC | QueryFlags.ORIGIN_DISTANCE);
  ```
  затем фильтр `candidate.IsBuilding()` (`ExpansionWorld.c:148`).
- `GetWorldSize()`: `3_game/global/world.c:85` (`proto int GetWorldSize()`). Chernarus = 15360×15360.
- `GetPosition()`: `3_game/entities/object.c:293` (`proto native vector GetPosition()`).

**Вывод: нужен рантайм-скан.** Категорию здания можно дать только (а) по префиксу/классу
`GetType()`, (б) по `IsInherited` с собственным списком типов, или (в) разово собрать
собственную таблицу «класс → категория» и сериализовать. Готового реестра категорий в
скриптах нет.

---

## 3. Водопои — колодцы **ДА** (сущности), пруды **НЕТ** (поверхность)

### Колодцы (wells) — сущности-здания

- `4_world/entities/building/well.c` — `class Well extends BuildingSuper`:
  - `override bool IsWell()` → `GetWaterSourceObjectType() == EWaterSourceObjectType.WELL` (:10)
  - `override EWaterSourceObjectType GetWaterSourceObjectType()` → `WELL` (:13)
  - `override int GetLiquidSourceType()` → `LIQUID_CLEANWATER` (:18)
- Базовый `Object` (`3_game/entities/object.c`):
  - `EWaterSourceObjectType GetWaterSourceObjectType()` → `NONE` (:653)
  - `bool IsWell()` → `false` (:665) — «DEPRECATED by GetWaterSourceObjectType»
  - `int GetLiquidSourceType()` → `LIQUID_NONE` (:658)
- `enum EWaterSourceObjectType` (`3_game/enums/ewatersourceobjecttype.c`): `NONE=-1, WELL=0, THROUGH=1`.
- Прочие источники: `Land_Misc_Through_Static` (`4_world/entities/building/industrial/misc/land_misc_through_static.c`) → `THROUGH`; `Land_WaterSpring_Sakhal` (`specific/`); `Land_Underground_WaterReservoir` (`underground/water/`).

**Перечисление**: `SceneGetEntitiesInBox` по всей карте + фильтр `IsWell()`
(или `GetWaterSourceObjectType() == EWaterSourceObjectType.WELL`).

### Пруды/озёра — НЕ сущности, это поверхность террейна

- Определяются по типу поверхности, не по объекту:
  - `Game.SurfaceGetType(float x, float z, out string type)` — `3_game/global/game.c:1166`; `SurfaceGetType3D` (:1168).
  - `SurfaceInfo.GetByName(name).GetLiquidType()` — `3_game/surfaceinfo.c:16,45`.
  - `Surface.CheckLiquidSource(pHeight, pSurface, mask)` — `4_world/static/surface.c:54-72` (проверка бита `LIQUID_*`).
- Жидкие типы (`3_game/constants.c:544-560`): `LIQUID_FRESHWATER=524288` (реки/пруды),
  `LIQUID_STILLWATER=1048576`, `LIQUID_SALTWATER=262144`, `LIQUID_GROUP_DRINKWATER` (маска),
  `LIQUID_CLEANWATER=4194304` (колодцы/дождь).
- Классов `WellBase`/`WaterPondBase`/`EnvWaterSource` **в ванили нет** (grep пуст) —
  пруд не существует как объект с позицией.

**Вывод: нужен грид-скан террейна** (`SurfaceGetType` по сетке с шагом, напр. 50–100 м,
фильтр `GetLiquidType() & LIQUID_FRESHWATER|LIQUID_STILLWATER`). Готового реестра прудов нет.

---

## 4. Прочее по спавну — «спавн по городам» нативного API НЕТ

### CEApi

`3_game/ce/centraleconomy.c` — полный API `class CEApi` (:226-737), доступ через
`proto native CEApi GetCEApi()` (:745). Есть: debug-спавн (`SpawnGroup` :346,
`SpawnDE` :363, `SpawnLoot` :393, `SpawnEntity` :455, `SpawnSingleEntity` :467,
`SpawnBuilding` :440, `SpawnVehicles` :424), `ExportSpawnData()` (:238 — пишет
`storage/spawnpoints.bin`), `ExportProxyData`/`ExportClusterData`/`ExportProxyProto`
(дебаг-экспорт XML в `storage/export/`), Avoidance API (`AvoidPlayer` :618,
`AvoidVehicle` :629, `CountPlayersWithinRange` :639), `GetCEGlobal*` (:584-598).
**Нет методов перечисления спавн-локаций/городов/точек.**

### Спавн-поинты игроков и территории — XML-файлы миссии, читаются файлами

Референс Expansion:
- `DayZExpansion/Core/Scripts/3_Game/DayZExpansion_Core/ce/expansionce.c`:
  - `LoadPlayerSpawnpoints()` → `"$mission:cfgplayerspawnpoints.xml"` (:89)
  - `LoadTerritories()` → `ExpansionStatic.FindFilesInLocation("$mission:env\\", ".xml")` + `"$mission:env\\%1_territories.xml"` (:104-124)
- `expansionceplayerspawnpoints.c` — структура `cfgplayerspawnpoints.xml`: `<playerspawnpoints><fresh|hop|travel><spawn_params/><generator_params/><group_params/><generator_posbubbles><group name="..."><pos x y z/>…` — **группы posbubbles = именованные кластеры спавна (по сути города)**.
- `expansionceterritory.c` — структура `*_territories.xml`: `<territory-type><territory color><zone name smin smax dmin dmax x z r>`.

Эти файлы — ванильные данные CE миссии (`$mission:`), читаются парсером `CF_XML`
(Expansion) или напрямую `FileHandle`+парсер. `CfgSpawns`/`cfgGameplayProfile` как
скриптового API перечисления точек **нет** (grep по ванили пуст, кроме упоминания
`storage/spawnpoints.bin` в доке `ExportSpawnData`).

---

## Открытые вопросы / что осталось проверить

1. **Структура `CfgWorlds <world> Names` построчно** (реальные имена детей, есть ли
   у `position` высота, точные значения `type` для каждой карты) — данные не в
   скриптах; проверить в игре `ConfigGetChildrenCount("CfgWorlds chernarusplus Names")`
   или дампом конфига.
2. **`QueryFlags` — реально ли битовые флаги** в нативе (скриптовый enum идёт 0..4
   подряд, но код использует `|`). Проверить, что `STATIC|ORIGIN_DISTANCE` отдаёт
   статику всей карты на живом сервере.
3. **`SceneGetEntitiesInBox` на всю карту** — насколько затратно/долго; делать разово
   на старте + кэшировать. Возможно ограничение по количеству возвращаемых сущностей.
4. **Радиусы POI** — эвристика Expansion (Capital 1000 / City 500 / Village 200) не
   является ванилью; для точных границ города нужна кластеризация статики вокруг
   точки `Names`.
5. **`GetWaterSourceObjectType` на клиенте vs сервере** — доступность предикатов
   (wells — статика, должна быть видна на сервере).

## Рекомендуемый подход к построению POI-реестра (для конфига спавна/кочевания)

1. **Именованные локации**: читать `CfgWorlds <world> Names` через `ConfigGet*`
   на старте сервера → список `{name, type, position[x,z]}`. Радиус — по типу
   (эвристика Expansion) или кластеризацией.
2. **Здания/категории**: разово `SceneGetEntitiesInBox` на всю карту (коробка
   `0..worldSize`), собрать `{class, position}`, классифицировать по префиксу
   `GetType()` (`Land_Mil_*`, `Land_House_*`, `Land_...Hospital/Firestation/Police...`)
   + `IsInherited` для известных типов; сериализовать в JSON/XML в `$mission:`/`$profile:`.
3. **Колодцы**: из того же скана отфильтровать `IsWell()` / `GetWaterSourceObjectType()==WELL`.
4. **Пруды**: грид-скан `SurfaceGetType` + `SurfaceInfo.GetLiquidType() & (FRESHWATER|STILLWATER)`;
   редкий шаг, кэш на диске.
5. **Спавн «по городам»**: точки города = центр из `Names` (+ смещение внутрь по
   навмешу `SampleNavmeshPosition`), либо напрямую `$mission:cfgplayerspawnpoints.xml`
   (posbubbles-группы уже и есть кластеры по городам).

---

## Уточнения из офлайн-генерации (v3.149, фактические данные)

Проверено на реальных файлах `$mission:dayzOffline.chernarusplus` (в `/home/devalio/dayz/server/`).

1. **`CfgWorlds <world> Names`** — 306 локаций (не 316): `Village` 59, `ViewPoint` 57,
   `Local` 49, `Hill` 37, `LocalOffice` 31, `Marine` 18, `RailroadStation` 17, `City` 16,
   `Camp` 11, `Ruin` 9, `Capital` 2. Поселения (спавн-цели) = Capital+City+Village+Camp = 88.
   Данные лежат в `DayZ Projects/DZ/worlds/chernarusplus/world/config.cpp` (читаемы офлайн).

2. **`mapgrouppos.xml`** — 11680 зданий (класс + абсолютная позиция `x y z` + угол `a`),
   270 уникальных классов. **`mapgroupproto.xml`** — 435 групп (по классу): `<usage>`
   (категория) + `<container><point pos="x y z">` (относительные точки лута). 422 класса
   имеют точки лута.

3. **`<usage>` — НЕ пригоден как строгая классификация зданий.** Это весовые тэги
   лута, а не тип постройки: `Land_Mil_Guardhouse1` → `Police`, `Land_Mil_Barracks5_Basement`
   → `Medic`, `Bonfire` → `Firefighter`, `Land_Wreck_Volha_Police` (полицейская машина) →
   `Police`. Классификацию делаем **точными именами классов + чистыми префиксами**:
   - WATER: `Land_Misc_Well_Pump_Blue/Yellow` (79 колодцев: 59 синих + 20 жёлтых — НЕ 118).
   - POLICE: `Land_City_PoliceStation` + `Land_Village_PoliceStation` (=20).
   - FIRE: `Land_City_FireStation` + `Land_Mil_FireStation` (=7).
   - MEDICAL: `Land_City_Hospital`, `Land_Village_HealthCare`, `Land_Medical_Tent_*` (=37).
   - MILITARY: префиксы `Land_Mil_`, `Land_Tisy_`, `Land_Airfield_`, `Land_Prison_` + `Land_Guardhouse`.
   - INDUSTRIAL (промзоны): `Land_Factory_`, `Land_CementWorks_`, `Land_Quarry_`, `Land_Mine_`,
     `Land_CoalPlant_`, `Land_Smokestack_`, `Land_Sawmill_`, `Land_Rail_Station_`,
     `Land_Rail_Warehouse_`, `Land_Repair_Center`, `Land_Power_*`, `Land_Pier_`.
   - FUEL: `Land_FuelStation_Build` (=19).
   - RESIDENTIAL: `Land_House_`, `Land_HouseBlock_`, `Land_Tenement_`, `Land_Camp_House_`.
   - остальное → GENERIC (сараи `Land_Shed_*`, гаражи, амбары, гражданские `Land_Wreck_*` — роуминг).

4. **Военные wrecks (останки хаммера/вертолётов/БМП) — ДИНАМИЧЕСКИЕ события, НЕ статика.**
   `StaticObj_Wreck_HMMWV_DE`, `Wreck_UH1Y/Mi8/Mi8_Crashed`, `StaticObj_Wreck_BMP1/2_DE`,
   `BRDM_DE`, `Ural_DE`, `T72_Chassis_DE` — отсутствуют в `mapgrouppos.xml` (0 шт.). Они
   спавнятся CE-событиями (`cfgeventgroups.xml`: группы `Abandoned_*`/`Ambush_*`/`Block_*`/
   `Supply_*`/`Traffic_*`) в точках `cfgeventspawns.xml` (`<event name><pos x z a>`). В статике
   есть только `Land_Wreck_C130J_Cargo` (1, упавший самолёт). Для статичного `world_poi.json`
   доступен только C130J; динамические точки событий — отдельная тема (не в MVP).

5. **Водопои = только колодцы** (`Land_Misc_Well_Pump_*`). `Land_Water_Station` (76) — декоративная
   вышка, НЕ источник воды. Пруды/озёра — поверхность террейна (грид-скан), не в MVP.

6. **Цветовые варианты** (`Land_House_1W09_Yellow/Brown`) — отдельные классы в обоих XML; в
   `mapgroupproto.xml` присутствуют как отдельные группы с теми же точками. При рантайм-поиске
   интерьер-точек для варианта может отсутствовать группа → нужен фолбэк «срезать суффикс цвета».
