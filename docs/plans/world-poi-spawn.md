# Мир: POI-реестр, спавн «по городам» и кочевничество

> Статус: proposal (код не писан). Research: `docs/research/world-poi.md`.

## Цель и движущие силы

Мод «почти готов» в части отдельного бота, но чтобы **населить мир**, нужна продуманная,
**конфигурируемая** система спавна. В основе — **конфигурация точек интереса (POI)** мира:
населённые пункты и маркеры внутри них (водопой, полицейский участок, пожарная станция,
больница, военные, жилые, промышленные). Дальше «заспавнить по одному боту в каждом городе
и смотреть, что получится»: бот **кочует** от POI к POI, собирая то, что ему нужно.

Две движущие силы (как в луте — см. `docs/plans/looting-and-exploration.md`):

- **эстетика/население** — мир заполнен ботами, которые ведут себя как игроки (ходят по
  городам, заходят в здания, идут к колодцу);
- **механика** — бот реально нуждается и едет/идёт туда, где нужда закрывается
  (жажда → водопой, голод → жилой дом/магазин, нет оружия → полиция/военная база).

Главный архитектурный принцип (уже принятый в моде): **домены чистые, связи на слое выше**.
POI-реестр — чистый провайдер (отвечает на запросы, никого не пишет); выбор «куда идти» —
в координаторе/состоянии; спавн — отдельный менеджер.

## Ключевые факты из research (что даёт ваниль)

| Что нужно | Ваниль | Как |
|---|---|---|
| Именованные населённые пункты (имя+тип+позиция) | **ДА** | конфиг `CfgWorlds <world> Names` через `g_Game.ConfigGet*` |
| Категория здания (полиция/пожарка/больница/…) | **НЕТ** | рантайм-скан статики + классификация по классу (`GetType()`/`IsInherited`) |
| Все здания карты | **ДА** | `DayZPlayerUtils.SceneGetEntitiesInBox` на коробку `0..worldSize` |
| Колодцы (водопой) | **ДА** | скан + `IsWell()` / `GetWaterSourceObjectType()==WELL` |
| Пруды/озёра | **НЕТ** | грид-скан террейна `SurfaceGetType` + `GetLiquidType()` (позже) |
| «Спавн по городам» (CE) | **НЕТ** | читать mission XML либо строить из `Names` |

Вывод: **реестр строится разово на старте сервера** из (1) `CfgWorlds Names` (поселения) +
(2) скана статики (здания→категории, колодцы), сериализуется в JSON-кэш; конфиг задаёт
правила классификации, радиусы и политику спавна.

## Модель данных

```c
enum dmWorldPOIType {
    SETTLEMENT,   // населённый пункт (из CfgWorlds Names)
    WATER,        // водопой: колодец (IsWell) / пруд (грид-скан, позже)
    POLICE, FIRE, MEDICAL, MILITARY,
    RESIDENTIAL, INDUSTRIAL, COMMERCIAL, FUEL,
    GENERIC
};

class dmWorldPOI {
    vector m_Position;            // X/Z из источника; Y снапается при использовании
    dmWorldPOIType m_Type;
    string m_Name;                // имя (поселение) или класс (здание)
    float m_Radius;               // эвристика по типу/классу
    Building m_Building;          // если POI привязан к сущности здания (иначе null)
    dmWorldPOI m_Settlement;      // родительский населённый пункт (или null)
};

class dmWorldPOIRegistry {
    ref array<ref dmWorldPOI> m_POIs;
    ref array<ref dmWorldPOI> m_Settlements;   // подмножество (type == SETTLEMENT)
    void Build();                              // разово: Names + скан + классификация
    void Clear();
    int Count();  int SettlementCount();
    dmWorldPOI GetNearestPOI(dmWorldPOIType t, vector pos, float maxDist); // null ок
    dmWorldPOI GetNearestSettlement(vector pos);
    dmWorldPOI GetSettlementOf(vector pos);    // ближайший поселение в радиусе
    dmWorldPOI GetRandomPOI(dmWorldPOIType t);
    void GetPOIsOfType(dmWorldPOIType t, out array<ref dmWorldPOI> out);
};
```

Радиусы поселений — эвристика по типу `Names` (референс Expansion: Capital 1000 / City 500 /
Village 200 / Suburb 200 / Camp 100), переопределяется конфигом.

## Источники POI (порядок построения)

1. **Поселения** — `CfgWorlds <world> Names`: для каждого ребёнка `type` + `name` +
   `position` (float[2] X/Z). Y дозалить `SampleNavmeshPosition`/`SurfaceY` при спавне/ходьбе.
2. **Здания** — разовый `SceneGetEntitiesInBox(min=(0,-1000,0), max=(worldSize,1000,worldSize),
   QueryFlags.STATIC|ORIGIN_DISTANCE)` → фильтр `IsBuilding()` → классификация по префиксу
   класса `GetType()` (`Land_Mil_*`→MILITARY, `Land_House_*`→RESIDENTIAL, …) и `IsInherited`
   для известных типов; правила — из конфига. Каждое здание-категория привязывается к
   ближайшему поселению в радиусе.
3. **Колодцы** — из того же скана `IsWell()` → WATER.
4. **Пруды** — грид-скан террейна (шаг ~50–100 м, `SurfaceGetType`+`GetLiquidType() &
   (FRESHWATER|STILLWATER)`), редко/опционально, кэш на диске. **Отложено** (не в MVP).

## Конфиг (JSON, `$profile:dmBotorama/`)

Паттерн — как у loadout (`dmLoadoutApplier.Load` → `dmJsonFile<T>`). Два файла:

- **`poi.json`** (`dmPoiConfig`): правила классификации `prefix → type`, радиусы поселений,
  ручные POI (`HandPOIs`), флаги (`ScanBuildings`, `IncludePonds`).
- **`spawn.json`** (`dmSpawnConfig`): `Enabled`, `BotsPerSettlement`, `MaxBots`,
  `SpawnLoadout`, `RespawnDelay`, опциональные per-city overrides (`Settlements[]`).

При отсутствии файла — `Defaults()` + `Save()` (как у loadout). `JsonSerializer` не применяет
инициализаторы полей (готча `codeguide.md`), поэтому все поля пишем явно в `Defaults()`.

## Спавн-менеджер (`dmBotSpawnManager`, серверный, тикается из `MissionServer.OnUpdate`)

- `OnInit`: `dmWorldPOIRegistry.Build()` (разово) + загрузка `spawn.json`.
- Тик (редкий, ~1с): если `Enabled` и `Count() < MaxBots` — доспавнить недостающих:
  по `BotsPerSettlement` в каждом поселении (спавн-точка = центр + смещение, `SnapToGround`
  / `SampleNavmeshPosition`), применить loadout, поставить пресет `dmBotPreset_Nomad`.
- Смерть бота → через `RespawnDelay` заспавнить замену (счётчик «мёртвых на поселение»).
- Не плодить дублей: вести `map<string, int>` живых ботов на поселение.

Спавн-точка: **строго на землю** (`SnapToGroundExactly`/`SurfaceY`), НЕ `SnapToGroundRelative`
(прибавляет `pos[1]` → бот в воздухе; конвенция из AGENTS.md).

## Кочевничество (FSM)

Новый пресет `dmBotPreset_Nomad` = `Idle` + **`Travel`** + `Exploration` + `Fighting` +
`Shooting`. Состояния не диктуют переходы — guard'ы на рёбрах (см. `docs/fsm-design-guide.md`).

- **`dmBotState_Travel`** (INTERRUPTIBLE): на входе выбирает целевой POI по нуждам и идёт
  туда (`dmBotIntent_MoveTo`, переиспользуем длинный pathfinding). По прибытии — EXIT.
  `CanEnter()` = есть выбранная цель.
- **Выбор цели** (по «человеческой модели» — простая приоритизация нужд, без замкнутых формул):
  жажда ниже порога → ближайший `WATER`; голод → ближайший `RESIDENTIAL`/`COMMERCIAL`;
  нет оружия → `MILITARY`/`POLICE`; иначе — случайное соседнее поселение (кочёвка) или
  случайный POI текущего поселения. Пороги — константы, `dmNeeds` уже даёт жажду/голод/оружие.
- **Переходы**: `Exploration → Travel` по `Require(NeedPending)` (нужда/нет непосещённых
  зданий); `Travel → Exploration` при достижении цели (бот залутает место через существующий
  `Exploration`). Бой/стрельба вытесняют (PREEMPTIVE) как и сейчас.
- Логирование — DEBUG-домен `DM_BOT_DEBUG_SPAWN` (+`DM_BOT_DEBUG_FSM` для переходов),
  включить в `defines[]` (`config.cpp`).

## Команды отладки (test/5_Mission)

- `/poi` — список поселений (имя/тип/позиция) и счётчики POI по типам.
- `/poi goto {type}` — телепорт игрока к ближайшему POI типа (проверка реестра).
- `/spawncity [n]` — заспавнить по `n` (по умолчанию 1) боту в каждом поселении.
- `/spawncity clear` — убрать городских ботов.

## Фазы (атомарные задачи, зависимости раньше)

| # | Задача | Deps | Критерий |
|---|---|---|---|
| 1 | `dmWorldPOIType` + `dmWorldPOI` + `dmWorldPOIRegistry` (сборка поселений из `CfgWorlds Names` + query-API) | research | `/poi` печатает поселения с именами/типами |
| 2 | Скан статики + классификация зданий → POI-категории; колодцы → WATER; привязка к поселению | 1 | `/poi` печатает счётчики по типам; водопои видны |
| 3 | `poi.json` + `spawn.json` (`dmPoiConfig`/`dmSpawnConfig` через `dmJsonFile`) | 1 | файлы создаются с дефолтами; правила переопределяются |
| 4 | `dmBotSpawnManager` (спавн по городам + поддержание населения) | 1, 3 | `/spawncity` заспавнивает по боту на город, мёртвые респавнятся |
| 5 | `dmBotState_Travel` + выбор цели по нуждам + `dmBotPreset_Nomad` | 1, Exploration | жаждущий бот идёт к колодцу, потом лутает и кочует |
| 6 | Команды `/poi`, `/spawncity` (+`/poi goto`) | 1, 4 | команды работают из чата |

MVP-веха (тестируемая): **задачи 1→4** дают «по одному боту в каждом городе»; **задача 5**
включает кочёвку. Пруды (грид-скан) и per-city override-конфиг — за пределами MVP.

## Открытые вопросы (research, проверить на живом сервере)

1. Точная структура `CfgWorlds <world> Names` (реальные имена детей, есть ли высота у `position`).
2. `QueryFlags` — реально ли битовые флаги (`STATIC|ORIGIN_DISTANCE` на всю карту).
3. Стоимость `SceneGetEntitiesInBox` на всю карту → разовый скан + кэш на диске.
4. Доступность `IsWell()`/`GetWaterSourceObjectType` на сервере.
5. Реальные префиксы классов зданий под категории (полиция/пожарка/больница) — засев
   списка конфига уточнить на карте Черноруссии.
