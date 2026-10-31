# water-sources — детект и наполнение водоисточников DayZ

**Цель**: для фичи «бот наполняет пустую бутылку водой» выяснить (1) как в ванили
определяется «источник воды» (объект), (2) какие классы реально отдают воду, (3) каким
серверным API наполнять бутылку у ИИ (`AI_SERVER`), (4) какие источники есть на карте
nasdara (эмпирика probe `waterdump`).

Домен: `water-sources`. Смежные заметки: `survival-test.md` (§2 «Вода/бутылка», §6.5
«Наполнение бутылки» — классы бутылок и API `Liquid.*`), `world-poi.md` (§3 «Водопои» —
колодцы-сущности vs пруды-поверхность). Здесь НЕ дублирую их таблицы (еда/медицина/объёмы
бутылок), только то, что относится к **детекту источника** и **наполнению**.

> **Примечание по проверке.** В этой сессии прямой доступ к
> `/home/devalio/dayz/Work/DayZ Projects/scripts/` запрещён (`external_directory`), поэтому
> file:line ванили ниже НЕ перечитаны заново — они взяты из (а) фактов, собранных
> оркестратором, и (б) кросс-проверки с уже выверенными `survival-test.md` / `world-poi.md`
> (обе писались с доступом к ванили). Строки, подтверждённые только оркестратором и не
> пересекающиеся с существующими заметками, помечены `[не перечитано в этой сессии]`.

---

## 1. Вывод-сводка

- **Детект «это источник воды» = виртуальные методы на `Object`/`Building`**, а НЕ
  `actioncomponent` и НЕ `poiType`. Канонический признак:
  `GetWaterSourceObjectType() != EWaterSourceObjectType.NONE` (−1).
- **`poiType` (`dmWorldPOIType.WATER`) НЕ годится как детект.** Это наш тэг из
  интерьер-карты (`dmBuildingInteriorMap`), а не ваниль; он покрывает только классические
  колодцы (`Land_Misc_Well_Pump_*`), но НЕ корыта (`Land_Misc_Through_Static` → `NONE`) и
  НЕ реальный «водовоз» nasdara — цистерну (`Land_Wreck_Truck01_Aban*_Cistern_DE` →
  `GENERIC`). Подробнее в §5.
- **Наполнение у ИИ — серверный статик** (`Liquid.FillContainer` или прямой
  `SetLiquidType(LIQUID_WATER)` + `SetQuantityMax()`), НЕ ванильный `ActionFillBottleBase`
  (client-driven, на `AI_SERVER` не запускается). Это уже зафиксировано в
  `survival-test.md` §2.2/§6.5; здесь добавляю только детект источника и команду анимации.

---

## 2. API детекта (методы + enum)

### 2.1 Виртуальные методы на `Object` (default-имплементация = «не источник»)

`3_game/entities/object.c`:

```c
EWaterSourceObjectType GetWaterSourceObjectType(); // :653 — default NONE
int  GetLiquidSourceType();                        // :658 — default LIQUID_NONE
bool IsWell();                                     // :665 — deprecated by GetWaterSourceObjectType
```

- `GetWaterSourceObjectType()` — «какой тип источника» (`NONE/WELL/THROUGH`).
- `GetLiquidSourceType()` — «какая жидкость течёт» (бит `LIQUID_*`).
- `IsWell()` — помечен DEPRECATED, возвращает `false` в базе; не полагаться.

`Building` (`3_game/entities/building.c`) наследует `EntityAI` → те же методы; классы
источников переопределяют их (см. §3).

### 2.2 Enum `EWaterSourceObjectType`

`3_game/enums/ewatersourceobjecttype.c`:

```c
enum EWaterSourceObjectType {
    NONE = -1,
    WELL = 0,
    THROUGH = 1
};
```

### 2.3 Сопутствующие предикаты (для фильтра скана)

`object.c` также даёт `IsBuilding()` (:648), `IsFuelStation()` (:676), `IsTree()`, `IsRock()`
и т.д. (полный список — `world-poi.md` §2). Для водоисточников релевантны только три метода
из §2.1; `IsFuelStation()`/`FuelStation` → `LIQUID_GASOLINE` (топливо, НЕ вода) — не путать.

---

## 3. Классы источников (ваниль)

| Класс | `GetWaterSourceObjectType` | `GetLiquidSourceType` | Где (скрипт) | Назначение |
|---|---|---|---|---|
| `Well` (база) | `WELL` | `LIQUID_CLEANWATER` | `4_world/entities/building/well.c` | базовый колодец; `GetLiquidThroughputCoef()==LIQUID_THROUGHPUT_WELL` |
| `Land_Misc_Well_Pump_Blue` / `_Yellow` | `WELL` | `LIQUID_CLEANWATER` | `.../building/residential/misc/land_misc_well_pump_*.c` | колодец-насос (классический) |
| `Land_Misc_Through_Static` | `THROUGH` | `LIQUID_FRESHWATER` | `.../building/industrial/misc/land_misc_through_static.c` | корыто (желоб) |
| `Land_WaterSpring_Sakhal` | (пустой `.c`) | — | `.../building/specific/land_waterspring_sakhal.c` | родник, только Sakhal; файл пуст — методы не переопределены |
| `Land_Underground_WaterReservoir` | **не переопределяет** | **не переопределяет** | `.../building/underground/water/land_underground_waterreservoir.c` | подземный резервуар; наполняется клапаном `ActionTurnValveUndergroundReservoir` + water-plane `Land_Underground_WaterReservoir_Water : BuildingBase`; водных методов НЕТ |
| `FuelStation` / `Land_FuelStation_Feed` | — | `LIQUID_GASOLINE` | `.../fuelstation/` | топливо, НЕ вода |

**Ключевое**: `GetLiquidSourceType()` важен, потому что `GetWaterSourceObjectType()` даёт
только «колодец vs корыто», а тип жидкости различает `CLEANWATER` (колодец) vs
`FRESHWATER` (корыто). Оба — питьевые (входят в `LIQUID_GROUP_DRINKWATER`).

---

## 4. API наполнения (для ИИ, серверный)

### 4.1 Статик `Liquid` (не требует action)

`4_world/static/liquid.c` (сигнатуры — `survival-test.md` §2.2, §6.5):

```c
static void FillContainer(ItemBase container, int liquid_type, float amount);            // :219
static void FillContainerEnviro(ItemBase container, int liquid_type, float amount,       // :240
                                bool inject_agents = true);                              // env-источник + агенты
static bool CanFillContainer(ItemBase container, int liquid_type,                        // :290
                             bool ignore_fullness_check = false);
static void Transfer(ItemBase source, ItemBase target, float quantity = -1);             // :87
```

`FillContainer` = `SetLiquidType(liquid_type)` + `AddQuantity(amount)` (`liquid.c:234-235`).
`FillContainerEnviro` дополнительно задаёт температуру и впрыскивает агенты (холера из
пруда — `AGT_WATER_POND`); для чистой воды из колодца он не нужен.

### 4.2 Прямой путь (эталон мода)

`src/test/4_World/dmBotEatDrinkTest.c:52-56` (аналог в `givebottle` —
`dmE2EBridge.c:1533-1534`, где вместо `SetQuantityMax()` стоит `SetQuantity(ml)`):

```c
ItemBase drink = ItemBase.Cast(pawn.GetInventory().CreateInInventory("WaterBottle"));
drink.SetLiquidType(LIQUID_WATER);   // itembase.c:3888 (серверный IsServerCheck)
drink.SetQuantityMax();              // itembase.c:3418
```

Минимальный детерминированный набор = `SetLiquidType(LIQUID_WATER)` + `SetQuantityMax()`.
Объём = `GetQuantity()` (мл, `itembase.c:3440`); пустая = `GetQuantity()==0` или
`GetLiquidType()==LIQUID_NONE`; полная = `IsFullQuantity()` (`itembase.c:3515`).

**Выбор жидкости по источнику**: колодец/цистерна → `LIQUID_CLEANWATER` (или
`LIQUID_WATER`); корыто → `LIQUID_FRESHWATER` (+ при желании агенты через
`FillContainerEnviro`).

### 4.3 Константы жидкостей

`3_game/constants.c:532-560`:

```
LIQUID_NONE=0, LIQUID_WATER=512, LIQUID_RIVERWATER=1024, LIQUID_SALTWATER=262144,
LIQUID_FRESHWATER=524288, LIQUID_STILLWATER=1048576, LIQUID_CLEANWATER=4194304,
LIQUID_GROUP_DRINKWATER (OR питьевых)
```

Значения сверены с маппингом `LiquidSourceTypeName` в `dmE2EBridge.c:2797-2814`.

### 4.4 Анимация наполнения (косметика, отдельно от эффекта)

Команда анимации берётся из `ActionFillBottleBase.GetActionCommandEx` —
`4_world/classes/useractionscomponent/actions/continuous/actionfillbottlebase.c:92-101`
`[не перечитано в этой сессии]`:

- колодец → `CMD_ACTIONFB_FILLBOTTLEWELL`
- пруд/водоём → `CMD_ACTIONFB_FILLBOTTLEPOND`

Игровой эффект наполнения — чисто скриптовый (`Liquid.*` / `SetLiquidType`+`SetQuantity`),
анимация — косметический слой; в моде она играется через
`pawn.AddCommandModifier_Action(cmd, dmBotActionAnimCB)` (паттерн
`dmBotIntent_EatDrink.StartAnim`, `dmBotIntent_EatDrink.c:223-231`, только там команды еды/
питья). Сам `ActionFillBottleBase` у `AI_SERVER`-бота не запускается (client-driven; см.
`medical.md` C2): его `ActionCondition` (`actionfillbottlebase.c:51-72`) читает
`target.GetObject().GetLiquidSourceType()` (если есть объект) или водную поверхность террейна
(`CCTWaterSurfaceEx` + `SurfaceGetType3D`).

---

## 5. Эмпирика nasdara (probe `waterdump`, v3.408)

Probe: `dmE2EBridge.RunWaterDump` (`dmE2EBridge.c:2688-2738`) — дампит
`GetType()` / `IsBuilding()` / `IsWell()` / `GetWaterSourceObjectType()` /
`GetLiquidSourceType()` / `poiType` целевого объекта (спавн через `spawnobj`, либо
ближайший по `ClassName` в радиусе). `poiType` = `dmWorldPOIType`
(`src/cons/3_Game/constants.c:78-91`: `NONE=0, WATER=1, MILITARY_WRECK=6, GENERIC=10`).

| Класс | `GetWaterSourceObjectType` | `GetLiquidSourceType` | `poiType` | Вердикт |
|---|---|---|---|---|
| `Land_Misc_Well_Pump_Blue` | WELL (0) | LIQUID_CLEANWATER (4194304) | WATER (1) | источник ✓ (`CreateObject` спавнит ок) |
| `Land_Misc_Through_Static` | THROUGH (1) | LIQUID_FRESHWATER (524288) | NONE (0) | источник ✓ |
| `StaticObj_Wreck_MTVR_Water[_DE]` | NONE (−1) | LIQUID_NONE (0) | MILITARY_WRECK (6) | декоративный wreck, НЕ источник ✗ |
| `Land_Underground_WaterReservoir` | NONE (−1) | LIQUID_NONE (0) | — | НЕ источник ✗ |
| `Land_Underground_WaterReservoir_Water` | NONE (−1) | LIQUID_NONE (0) | — | НЕ источник ✗ |
| `Land_Wreck_Truck01_Aban1/2_Cistern_DE` | WELL (0) | LIQUID_CLEANWATER (4194304) | GENERIC (10) | **водовоз** ✓ |

**Выводы по nasdara:**

1. **Реальный «водовоз» nasdara — цистерна `Land_Wreck_Truck01_Aban1/2_Cistern_DE`**
   (событие `StaticWaterTruck`; `db/events.xml`:
   `<child type="Land_Wreck_Truck01_Aban*_Cistern_DE"/>`). Интерфейс подтверждён через
   `spawnobj` → `waterdump`: `WELL` + `LIQUID_CLEANWATER` + `poiType=GENERIC`.
2. **Классических колодцев/корыт в CE-расстановке nasdara НЕТ** — только loot-таблицы
   `mapgroupproto.xml` (имена классов задекларированы, но как статики не спавнятся).
   Колодец `Land_Misc_Well_Pump_Blue` и корыто `Land_Misc_Through_Static` подтверждены
   только через `CreateObject` (spawnobj), не найдены в живом CE-скане.
3. **Декоративные «водные» имена — НЕ источники**: `Land_Water_Station` (вышка, 76 шт на
   Chernarus, см. `world-poi.md` §3 п.5), `Land_WaterTower_*`, `StaticObj_WaterReservoir`,
   `StaticObj_Wreck_MTVR_Water` — все `NONE/LIQUID_NONE`. Имя класса с «water» ≠ источник.
4. **Следствие для детекта**: полагаться на `poiType==WATER` нельзя — цистерна-водовоз
   (`GENERIC`) и корыто (`NONE`) выпадут. Детект = `GetWaterSourceObjectType() != NONE`.

---

## 6. Открытые вопросы / `[нужно проверить]`

1. **[нужно подтвердить]** Живой CE-спавн цистерны `Land_Wreck_Truck01_Aban*_Cistern_DE`
   на nasdara: интерфейс подтверждён только через `spawnobj`, сам объект в живом
   CE-скане (`scanbox`/`SceneGetEntitiesInBox`) в прогоне v3.408 не найден. Прогнать:
   скан статики всей карты + фильтр `GetType() == "Land_Wreck_Truck01_Aban1_Cistern_DE"`
   (и `_Aban2_`) → зафиксировать число/позиции реально заспавненных цистерн и что у них
   `GetWaterSourceObjectType()==WELL`.
2. **[нужно подтвердить]** `Liquid.FillContainer(bottle, LIQUID_WATER, 1000)` на сервере
   у ИИ-бота даёт тот же результат, что `SetLiquidType(LIQUID_WATER)+SetQuantityMax()`
   (для воды без агентов) — см. `survival-test.md` §7 п.2.
3. **[частично снято]** Корыта/цистерны — тоже `House`, регистрируются как колодец.
   Иерархия: `Land_Misc_Through_Static : BuildingSuper`, а `typedef House BuildingSuper`
   (`game/super/building.c:92`) → корыто это `House`, попадает в `dmLiveBuildingRegistry`
   через `modded class House` (E2E Atom A подтвердил для колодца; корыто — тот же typedef-путь).
   Готча: `IsBuilding()` у `Well`/цистерны = `false` (override в `well.c`), у корыта = `true`
   (не переопределён) — это НЕ признак иерархии, а косметика; детект источника идёт по
   `GetWaterSourceObjectType()!=NONE`, который покрывает WELL и THROUGH одинаково.
   Открытым остаётся только: `LIQUID_FRESHWATER` (корыто) не впрыскивает агенты без
   `FillContainerEnviro` (мы всё равно заливаем чистую `LIQUID_WATER`).
4. **[нужно подтвердить]** Анимация `CMD_ACTIONFB_FILLBOTTLEWELL` vs
   `CMD_ACTIONFB_FILLBOTTLEPOND` — какую из двух играть для цистерны-водовоза (по
   ванили колодец → `...WELL`; цистерна по типу `WELL` — вероятно тоже `...WELL`, но
   семантика «бочка на грузовике» может требовать `...POND`). Косметика, не блокирует фичу.

---

## 7. Существенные развилки / решения (для `docs/decisions.md`)

- **Детект источника = `GetWaterSourceObjectType()!=NONE` (+`GetLiquidSourceType` для
  типа жидкости), а НЕ `poiType` и НЕ `IsWell()`.** `poiType` — наш тэг интерьер-карты,
  не покрывает цистерну-водовоз (`GENERIC`) и корыто (`NONE`); `IsWell()` — deprecated и
  в базе всегда `false`. Выбор влияет на то, как интент «наполнить бутылку» ищет источник.
- **Наполнение у ИИ — серверный статик (прямой `SetLiquidType`+`SetQuantity` или
  `Liquid.FillContainer`), а НЕ `ActionFillBottleBase`.** Action client-driven и не
  запускается на `AI_SERVER`; анимация (`CMD_ACTIONFB_FILLBOTTLE*`) — отдельный
  косметический слой через `AddCommandModifier_Action`, эффект — чисто скриптовый.
- **«Водовоз» nasdara = цистерна `Land_Wreck_Truck01_Aban*_Cistern_DE`, а не классический
  колодец.** Классические `Land_Misc_Well_Pump_*` на nasdara отсутствуют (только
  loot-таблицы), поэтому тест «наполнил бутылку» на nasdara должен либо спавнить колодец/
  цистерну через `spawnobj`, либо подтвердить живой CE-спавн цистерны (п.1 открытых вопросов).
