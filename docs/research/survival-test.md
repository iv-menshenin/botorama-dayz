# survival-test — точные имена классов + API для E2E-тестов живучести

Цель: закрыть неизвестные API и собрать **точные** `ClassName` предметов DayZ, чтобы
сценарии живучести (еда/вода/открытие консервы/перевязка/лекарства/наполнение бутылки)
были детерминированными. Все имена проверены по ванильному config, а не угаданы.

Источники:
- Скрипты: `/home/devalio/dayz/Work/DayZ Projects/scripts/` (НЕ `DayZ-Script-Diff` — это
  устаревший путь из старых заметок).
- Ванильный config (CfgVehicles/CfgWeapons/CfgMagazines/CfgAmmo): распакован в
  `/home/devalio/dayz/Work/DayZ Projects/DZ/` (PBO-конфиги по каталогам).
- Мод: `/home/devalio/dayz/Work/botorama/` (тесты-эталоны `src/test/4_World/`).

Ключевой факт: **имя скрипт-класса = имя CfgVehicles-класса** (совпадает). Проверено
перекрёстно: `TacticalBaconCan` есть и в `scripts/.../tacticalbaconcan.c:1`, и в
`DZ/gear/food/config.cpp:2161`. Поэтому ниже имена скрипт-классов — это и есть `ClassName`.

---

## 1. Еда (консервы, `Edible_Base`)

### 1.1 Имена классов (закрытая → открытая)

| Консерва (закрытая) | Открытая (`_Opened`) | energy | water | itemSize | Скрипт-файл (закрытая) |
|---|---|---|---|---|---|
| `TacticalBaconCan` | `TacticalBaconCan_Opened` | 300 | 25 | {2,2} | `edible_base/tacticalbaconcan.c:1` |
| `SpaghettiCan` | `SpaghettiCan_Opened` | 175 | 38 | {2,2} | `edible_base/spaghettican.c:1` |
| `SardinesCan` | `SardinesCan_Opened` | 250 | 25 | {2,1} | `edible_base/sardinescan.c:1` |
| `PeachesCan` | `PeachesCan_Opened` | 150 | 50 | {2,2} | `edible_base/peachescan.c:1` |
| `BakedBeansCan` | `BakedBeansCan_Opened` | — | — | {2,2} | `edible_base/bakedbeanscan.c:1` |
| `TunaCan` | `TunaCan_Opened` | 150 | 20 | {2,1} | `edible_base/tunacan.c:1` |

Config-источник (energy/water/itemSize): `DZ/gear/food/config.cpp` (строки классов —
`TacticalBaconCan:2161`, `SpaghettiCan:2397`, `SardinesCan:2631`, `PeachesCan:1929`).
`energy`/`water` — в блоке `Nutrition` каждой записи (напр. `TacticalBaconCan_Opened`
`energy=300; water=25;` — `config.cpp:2360-2361`).

> **Замечание по имени:** в задаче упоминалась «`CanSardines`» — правильное имя
> **`SardinesCan`** (не `CanSardines`).

### 1.2 Механика «закрытая консерва → Opened» (подтверждено)

Закрытая консерва переопределяет `Edible_Base`:

```c
class TacticalBaconCan : Edible_Base {
    override void Open() { ReplaceEdibleWithNew("TacticalBaconCan_Opened"); }
    override bool IsOpen() { return false; }   // закрыта
}
```
(`scripts/4_world/entities/itembase/edible_base/tacticalbaconcan.c:1-17`).

Цепочка `ReplaceEdibleWithNew` (`edible_base.c:680-690`):
1. `PlayerBase player = PlayerBase.Cast(GetHierarchyRootPlayer());` — **нужен игрок/бот**,
   иначе `Error("ReplaceEdibleWithNew - cannot use edible without player")`.
2. `ReplaceEdibleWithNewLambda : TurnItemIntoItemLambda : ReplaceItemWithNewLambda`
   (`edible_base.c:1108`; `miscgameplayfunctions.c:1`).
3. `player.ServerReplaceItemInHandsWithNew(lambda)` (`3_game/entities/man.c:340`) →
   `ReplaceItemInHandsWithNewImpl(InventoryMode.SERVER, lambda)` (`man.c:335-337`).

**Ключевое для теста:** замена происходит в **руках** (`ReplaceItemInHandsWithNew`).
Детект «бот реально вскрыл» = тип предмета в руках сменился на `*_Opened`
(`GetHumanInventory().GetEntityInHands().GetType().IndexOf("_Opened") != -1`). Именно так
делает мод: `dmBotIntent_EatDrink.PhaseOpen`
(`src/core/4_World/Entities/Bot/Intent/dmBotIntent_EatDrink.c:99-104`), а `BeginEat` зовёт
`b.Open()` при `!b.IsOpen()` (`dmBotIntent_EatDrink.c:74-88`).

Открытая консерва (`*_Opened`) добавляет действия еды: `ActionForceFeedCan` + `ActionEatCan`
(`gear/food/tacticalbaconcan_opened.c:3-11`).

---

## 2. Вода / бутылка

### 2.1 Классы и объём

| Класс | varQuantityMax (мл) | varLiquidTypeInit | itemSize | Скрипт-файл |
|---|---|---|---|---|
| `WaterBottle` | 1000 | 512 (`LIQUID_WATER`) | {1,3} | `edible_base/bottle_base/waterbottle.c:1` |
| `Canteen` | 1000 | 512 (`LIQUID_WATER`) | {2,2} | `edible_base/bottle_base/canteen.c:1` |
| `FilteringBottle` | 750 | 512 | {1,3} | `edible_base/bottle_base/filteringbottle.c:1` |

Config: `DZ/gear/drinks/config.cpp` (`Canteen:26`, `WaterBottle:188`; `varQuantityMax=1000`,
`varLiquidTypeInit=512`). Оба `: Bottle_Base : Edible_Base : ItemBase`.

**Объём воды = `GetQuantity()` (мл)**, максимум = `GetQuantityMax()`, нормализация =
`GetQuantityNormalized()` (`itembase.c:3440`). Пустая = `GetQuantity() == 0` (или
`GetLiquidType() == LIQUID_NONE`). Полная = `GetQuantity() >= GetQuantityMax()`
(`IsFullQuantity()`, `itembase.c:3515`).

Свежая `WaterBottle`/`Canteen` по config спавнится с `varQuantityInit=1000` (полная) и
`varLiquidTypeInit=512` (вода) — но мод для детерминизма выставляет явно (см. 2.3).

### 2.2 Есть ли ванильная механика НАПОЛНЕНИЯ (подтверждено: есть, два слоя)

**Слой 1 — action игрока** `ActionFillBottleBase` (continuous):
- `4_world/classes/useractionscomponent/actions/continuous/actionfillbottlebase.c:19`.
- Требует `CCTWaterSurfaceEx`/`CCTNone` (водоём или источник), `m_AllowedLiquidMask =
  LIQUID_GROUP_DRINKWATER | LIQUID_GASOLINE` (без снега), команда
  `CMD_ACTIONFB_FILLBOTTLEPOND`/`...FILLBOTTLEWELL`.
- Ядро заполнения — `CAContinuousFill` → `Liquid.FillContainerEnviro(...)`
  (`cacontinuousfill.c:105`, серверная ветка `g_Game.IsServer()`).
- **Client-driven** (как все action'ы): для `INSTANCETYPE_AI_SERVER` напрямую не запустить
  (см. `docs/research/medical.md` C2).

**Слой 2 — статический API (серверный, для ИИ)** `class Liquid` (`4_world/static/liquid.c`):

```c
static void FillContainer(ItemBase container, int liquid_type, float amount);      // liquid.c:219
static void FillContainerEnviro(ItemBase container, int liquid_type, float amount, // liquid.c:240
                                bool inject_agents = true);                        // env-источник + агенты
static bool CanFillContainer(ItemBase container, int liquid_type, bool ignore_fullness_check = false); // :290
static void Transfer(ItemBase source, ItemBase target, float quantity = -1);       // :87 (из бутылки в бутылку)
```

`FillContainer` делает `SetLiquidType(liquid_type)` + `AddQuantity(amount)` (`liquid.c:234-235`)
и проверяет `CanFillContainer` (маска `liquidContainerType`, не полная, тип не смешан).
`FillContainerEnviro` дополнительно задаёт температуру и впрыскивает агенты (холера из
пруда — `AGT_WATER_POND`, `liquid.c:264-284`). Оба **статик-скриптовые, серверные** — годятся
для ИИ без action.

### 2.3 Как мод уже наполняет бутылку (эталон, без `Liquid`)

`dmBotTest_Drink` (`src/test/4_World/dmBotEatDrinkTest.c:52-56`):
```c
ItemBase drink = ItemBase.Cast(pawn.GetInventory().CreateInInventory("WaterBottle"));
drink.SetLiquidType(LIQUID_WATER);
drink.SetQuantityMax();
```
`SetLiquidType(int value, bool allow_client=false)` (`itembase.c:3888`) — серверный
(`IsServerCheck`). `SetQuantityMax()` (`itembase.c:3418`). Минимальный детерминированный
набор = `SetLiquidType(LIQUID_WATER)` + `SetQuantityMax()` (или `SetQuantity(...)`).

Пустая бутылка: `SetQuantity(0)` (или `SetQuantityToMinimum()`, min=0).

---

## 3. Оружие + калибры + патроны

### 3.1 Цепочки «оружие → магазин → пуля → пачка»

| Оружие | Класс оружия | База (скрипт) | Калибр | Пуля (`CfgAmmo`) | Пачка (`CfgMagazines`) | initSpeed | Отъёмный магазин |
|---|---|---|---|---|---|---|---|
| Mosin | `Mosin9130` | `BoltActionRifle_InnerMagazine_Base` | 7.62x54R | `Bullet_762x54` | `Ammo_762x54` (20 шт) | 785 | **нет** (внутр. 5, `magazines[]={}`) |
| CZ527 | `CZ527` | `BoltActionRifle_ExternalMagazine_Base` | 7.62x39 | `Bullet_762x39` | `Ammo_762x39` (20 шт) | 640 | `Mag_CZ527_5rnd` |
| R12 | `R12` | `Shotgun_Base` (трубчатый 7) | 12ga | `Bullet_12GaugePellets` | `Ammo_12gaPellets` (10 шт) | 340 | **нет** (внутр. 7) |
| R12 (пуля) | `R12` | — | 12ga | `Bullet_12GaugeSlug` | `Ammo_12gaSlug` (10 шт) | 380 | — |
| M4A1 | `M4A1` | `Rifle_Base` | 5.56x45 | `Bullet_556x45` | `Ammo_556x45` (20 шт) | 850 | `Mag_STANAG_30Rnd` |
| (пистолет) | — | — | 9x19 | `Bullet_9x19` | `Ammo_9x19` (25 шт) | 350 | `Mag_Glock_15Rnd` / `Mag_MP5_30Rnd` |
| (пистолет) | — | — | .45 ACP | `Bullet_45ACP` | `Ammo_45ACP` (25 шт) | 260 | `Mag_FNX45_15Rnd` / `Mag_1911_7Rnd` |
| (винтовка) | `B95` | `DoubleBarrel_Base` | .308 Win | `Bullet_308Win` | `Ammo_308Win` (20 шт) | 770 | нет (2 ствола) |

### 3.2 Источники (file:line)

**Пули (`CfgAmmo`)**: `DZ/weapons/projectiles/config.cpp`:
- `Bullet_762x54` (:4038, `spawnPileType="Ammo_762x54"`, `initSpeed=785` :4043-4046);
- `Bullet_762x39` (:4161, `Ammo_762x39`, 640);
- `Bullet_12GaugePellets` (:3413, `Ammo_12gaPellets`, 340) — **внимание: `12Gauge`, не `12ga`**;
- `Bullet_12GaugeSlug` (:3462, `Ammo_12gaSlug`, 380);
- `Bullet_556x45` (:3922, `Ammo_556x45`, 850);
- `Bullet_9x19` (:4323, `Ammo_9x19`, 350);
- `Bullet_45ACP` (:4223, `Ammo_45ACP`, 260);
- `Bullet_308Win` (:4100, `Ammo_308Win`, 770).

**Пачки (`CfgMagazines`)**: `DZ/weapons/ammunition/config.cpp` — все `: Ammunition_Base`,
`ammo=` поле = имя пули, `count=` = число патронов в стеке (см. таблицу). Скрипт-классы пачек —
`4_world/entities/itembase/magazine/ammunitionpiles.c` (`Ammo_762x54 : Ammunition_Base` :33 и т.д.).

**Оружие (`CfgWeapons`)**: `DZ/weapons/firearms/mosin9130/config.cpp` (`Mosin9130_Base:28`,
`chamberableFrom[]={"Ammo_762x54","Ammo_762x54Tracer","Mag_CLIP762x54_5Rnd"}`, `magazines[]={}`,
`chamberSize=5`); `firearms/cz527/config.cpp` (`CZ527_Base:31`, `magazines[]={"Mag_CZ527_5rnd"}`,
`chamberableFrom[]={"Ammo_762x39","Ammo_762x39Tracer"}`); `shotguns/remington_r12/config.cpp`
(`R12_Base:27`, `chamberableFrom[]={"Ammo_12gaPellets","Ammo_12gaSlug","Ammo_12gaRubberSlug",
"Ammo_12gaBeanbag"}`, `chamberSize=7`, `magazines[]={}`).

**Отъёмные магазины** (`CfgMagazines`): `4_world/entities/itembase/magazine/magazines.c`
(`Mag_CZ527_5rnd` :27, `Mag_STANAG_30Rnd` :28, `Mag_Glock_15Rnd` :6, `Mag_MP5_30Rnd` :10,
`Mag_FNX45_15Rnd` :1, `Mag_1911_7Rnd` :4).

### 3.3 Ключевая готча: имя пули ≠ имя пачки

`CfgMagazines <пачка> ammo` = имя **пули** из `CfgAmmo` (для конвертации см.
`AmmoTypesAPI.MagazineTypeToAmmoType`, `ammotypes.c:14`, и `docs/research/loot.md`
«Рейтинг оружия»). Пример расхождения: пачка `Ammo_12gaPellets` → пуля
**`Bullet_12GaugePellets`** (не `Bullet_12gaPellets`).

---

## 4. Медицина

Все имена подтверждены config'ом `DZ/gear/medical/config.cpp` и скриптами (см.
`docs/research/medical.md` B):

| Предмет | ClassName | Скрипт-класс | config строка |
|---|---|---|---|
| Бинт | `BandageDressing` | `BandageDressing : Inventory_Base` | :30 |
| Тряпка (бинт 0.5) | `Rag` | `Rag : ItemBase` | — (`rag.c:1`) |
| Шина | `Splint` | `Splint : Inventory_Base` | :2616 |
| Шина (надетая) | `Splint_Applied` | `Splint_Applied : Clothing` | :2515 |
| Обезбол | `PainkillerTablets` | `: Edible_Base` | :602 |
| Уголь | `CharcoalTablets` | `: Edible_Base` | :495 |
| Тетрациклин | `TetracyclineAntibiotics` | `: Edible_Base` | :1047 |
| Мультивитамины | `VitaminBottle` | `: Edible_Base` | :709 |
| Морфин | `Morphine` | `: Inventory_Base` | :1225 |
| Эпинефрин | `Epinephrine` | `: Inventory_Base` | :1160 |
| Физраствор | `SalineBag` | `: Inventory_Base` | :1741 |
| Пакет крови (пустой) | `BloodBagEmpty` | `: Inventory_Base` | :2062 |

> `MultivitaminPills` не существует — мультивитамины это `VitaminBottle` (бутылка пилюль,
> `quantity` = число пилюль, действие `ActionEatPillFromBottle`).

---

## 5. Хлам для заполнения карго

| Предмет | ClassName | itemSize | count |
|---|---|---|---|
| Пачка 7.62x54 | `Ammo_762x54` | {1,1} | 20 |
| Пачка 7.62x39 | `Ammo_762x39` | {1,1} | 20 |
| Пачка 5.56x45 | `Ammo_556x45` | {1,1} | 20 |
| Пачка 9x19 | `Ammo_9x19` | {1,1} | 25 |
| Пачка .45 ACP | `Ammo_45ACP` | {1,1} | 25 |

Все россыпные пачки — **1×1 ячейка** (наследуют `itemSize[]={1,1}` из
`Ammunition_Base`, `DZ/weapons/ammunition/config.cpp:21`; отдельные пачки itemSize не
переопределяют). Исключения (не для карго-мусора): болты `Ammo_HuntingBolt` {4,1} (:1450),
`Ammo_RPG7_HE` {2,1} (:1950), 40мм `Ammo_40mm_*` {2,1} (:2227).

---

## 6. API (сигнатуры + источники)

### 6.1 Energy/Water (`PlayerStat<float>`)

```c
PlayerStat<float> GetStatEnergy();  // playerbase.c:7861
PlayerStat<float> GetStatWater();   // playerbase.c:7843
```
`PlayerStat<T>` (`4_world/classes/playerstats/playerstatbase.c:32`):
- `void Set(T value, string system = "")` — клампит в [min,max], синкает (`:86`).
- `void Add(T value, string system = "")` (`:145`), `float Get()` (`:152`),
  `float GetMax()` / `GetMin()` / `GetNormalized()` (`:160/165/170`), `SetByFloat`.

**Max-значения** (`3_game/playerconstants.c:44/50`):
- `SL_ENERGY_MAX = 5000` → Energy ∈ [0, 5000].
- `SL_WATER_MAX = 5000` → Water ∈ [0, 5000].
Регистрация статов: `playerstatspco.c:297-298` (`new PlayerStat<float>(0, SL_ENERGY_MAX, ...)`).

Эталон использования в моде: `pawn.GetStatEnergy().Set(0.0)` / `pawn.GetStatWater().Set(0.0)`
(`src/test/4_World/dmBotEatDrinkTest.c:16/58`), команда `/bot setstat`
(`src/test/5_Mission/dmBotCommand.c:1138/1148`).

### 6.2 Health/Blood/Shock (`SetHealth`)

```c
proto native float GetHealth(string zoneName, string healthType);        // object.c:990 (абсолютное)
proto native float GetHealth01(string zoneName, string healthType);      // object.c:997 (0..1)
proto native void  SetHealth(string zoneName, string healthType, float); // object.c:1011
proto native void  AddHealth(string zoneName, string healthType, float); // object.c:1023
```

**Max-значения** (`DZ/characters/data/config.cpp`, `SurvivorBase` → `DamageSystem.GlobalHealth`):
- `Health` `hitpoints=100` (:385-388) → полное = `SetHealth("", "Health", 100.0)`.
- `Blood` `hitpoints=5000` (:414-417) → полное = `SetHealth("", "Blood", 5000.0)`.
- `Shock` `hitpoints=100` (:418-421) → полное = `SetHealth("", "Shock", 100.0)`.

**Shock инвертирован**: `<= UNCONSCIOUS_THRESHOLD (25)` = нокаут (см. `docs/research/body.md`).
`GetHealth("","Blood")` возвращает **абсолютное** значение (5000 = полное) — подтверждено
`boneregen.c:5,19` (`MINIMAL_BLOOD_TO_REGENRATE=5000`, сравнение с `GetHealth(...,"Blood") >= 5000`).

> ⚠️ Расхождение в моде: комментарий `dmAISurvivor.c:1213` «`GetHealth("","Blood") // 0..1,
> порог смерти 0.5`» противоречит ванили (абсолютная шкала 5000). См. открытые вопросы.

Эталоны использования в моде: `pawn.SetHealth("", "Health", 30.0)`
(`src/test/4_World/dmBotMedicalTest.c:122`), `SetHealth("", "Blood"/"Shock", value)`
(`src/test/5_Mission/dmBotCommand.c:1086/1096`), `GetHealth01()` (`dmE2EBridge.c:2574`).

### 6.3 Форс кровотечения

```c
bool AttemptAddBleedingSourceBySelection(string selection_name);  // bleedingsourcesmanagerbase.c:211
BleedingSourcesManagerServer GetBleedingManagerServer();           // playerbase.c:2773
bool IsBleeding();                                                 // playerbase.c:1035 (= m_BleedingBits != 0)
int  GetBleedingBits();                                            // playerbase.c:1057
```
`AttemptAddBleedingSourceBySelection` лоуэркейсит имя, ищет бит по `m_BleedingSourceZone`
и зовёт `AddBleedingSource(bit)` (если `CanAddBleedingSource`) — возвращает `bool`
(`bleedingsourcesmanagerbase.c:211-227`). `"Pelvis"` — валидная зона (используется в моде).

Достаточно ли для `IsBleeding()==true`: **по коду да** — `AddBleedingSource` ставит бит в
`m_BleedingBits`, а `IsBleeding()` читает `m_BleedingBits != 0` напрямую (без модификатор-тика).
Эталон в моде: `pawn.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("Pelvis")`
(`src/test/4_World/dmBotMedicalTest.c:16/123`, `dmE2EBridge.c:1092`).

### 6.4 Форс инфекции (грипп `MDF_INFLUENZA`)

Минимальный набор (эталон `src/test/4_World/dmBotMedicalTest.c:194-195`):
```c
pawn.InsertAgent(eAgents.INFLUENZA, 600.0);
pawn.GetModifiersManager().ActivateModifier(eModifiers.MDF_INFLUENZA);
```
Сигнатуры:
- `void InsertAgent(int agent, float count = 1)` (`3_game/entities/entityai.c:715`).
- `float GetSingleAgentCount(int agent)` (`entityai.c` / используется `poisoning.c:32`).
- `void ActivateModifier(int id, bool triggerEvent = EActivationType.TRIGGER_EVENT_ON_ACTIVATION)`
  (`modifiersmanager.c:219`); `bool IsModifierActive(eModifiers)` (`modifiersmanager.c:199`).
- `eAgents.INFLUENZA = 2` (`3_game/enums/eagents.c:6`).

**Почему нужны ОБА вызова** (важно для детерминизма):
- `ActivateModifier` → `ActivateRequest` → `m_ShouldBeActive=true`; реальная активация
  (`Activate()` → `m_IsActive=true`) — в следующем `Tick()`: `if (!m_IsActive &&
  m_ShouldBeActive) Activate();` (`modifierbase.c:93-94`). `Tick` дёргается из
  `OnScheduledTick` (`modifiersmanager.c:204`), который у ИИ-бота мод тикает вручную
  (`dmAISurvivorBase.TickBodySystems`).
- **Без `InsertAgent`** модификатор активируется, но тут же **деактивируется** на следующем
  тике: `InfluenzaMdfr.DeactivateCondition` = `GetSingleAgentCount(INFLUENZA) <= 400`
  (`influenza.c:51-53`), а при 0 агентов это true. Пороги: `AGENT_THRESHOLD_ACTIVATE=600`,
  `DEACTIVATE=400` (`influenza.c:3-4`).
- То же для сальмонеллы: `InsertAgent(eAgents.SALMONELLA, 60.0)` +
  `ActivateModifier(MDF_SALMONELLA)` (`dmBotMedicalTest.c:158-159`).

Проверка: `GetModifiersManager().IsModifierActive(eModifiers.MDF_INFLUENZA)`.

### 6.5 Наполнение бутылки (повтор из §2.2)

Есть **статический серверный API**, не требующий action:
```c
Liquid.FillContainer(bottle, LIQUID_WATER, 1000.0);          // liquid.c:219 — SetLiquidType + AddQuantity
Liquid.FillContainerEnviro(bottle, LIQUID_FRESHWATER, 1000.0); // liquid.c:240 — + температура + агенты
Liquid.Transfer(source_bottle, target_bottle);               // liquid.c:87 — перелив
```
либо прямой путь (как мод): `bottle.SetLiquidType(LIQUID_WATER); bottle.SetQuantityMax();`
(`dmBotEatDrinkTest.c:54-55`). `SetLiquidType` — серверный (`itembase.c:3888`).

Константы жидкостей (`3_game/constants.c:532-560`): `LIQUID_NONE=0`, `LIQUID_WATER=512`,
`LIQUID_FRESHWATER=524288`, `LIQUID_RIVERWATER=1024`, `LIQUID_STILLWATER=1048576`,
`LIQUID_CLEANWATER=4194304`, `LIQUID_SALTWATER=262144`, `LIQUID_GROUP_DRINKWATER` (= OR всех
питьевых).

---

## 7. Открытые вопросы / `[нужно проверить]`

1. **[нужно подтвердить]** `ReplaceEdibleWithNew` (→ `ServerReplaceItemInHandsWithNew`,
   `InventoryMode.SERVER`) реально заменяет консерву в руках у `INSTANCETYPE_AI_SERVER`-бота.
   Существующий тест `dmBotTest_EatCan` это подразумевает, но прямого лог-подтверждения
   «рука == `PeachesCan_Opened`» нет. **Прогнать**: дать боту закрытую `PeachesCan`, обнулить
   энергию, в `botdump`/логе зафиксировать тип предмета в руках до/после — должно смениться
   с `PeachesCan` на `PeachesCan_Opened`.
2. **[нужно проверить]** `Liquid.FillContainer(bottle, LIQUID_WATER, amount)` на сервере
   корректно наполняет пустую бутылку у ИИ (без action). Мод использует `SetLiquidType` +
   `SetQuantityMax()`. **Прогнать**: пустая `WaterBottle` → `Liquid.FillContainer(b, LIQUID_WATER,
   1000)` → проверить `GetQuantity()>0 && GetLiquidType()==LIQUID_WATER`. Сравнить с
   `SetLiquidType`+`SetQuantityMax()` (должны быть эквивалентны для воды без агентов).
3. **[нужно подтвердить]** `AttemptAddBleedingSourceBySelection("Pelvis")` делает
   `IsBleeding()==true` **немедленно** (без модификатор-тика). По коду `IsBleeding()` читает
   `m_BleedingBits` напрямую, но подтвердить на живом сервере: после вызова сразу
   `IsBleeding()==true && GetBleedingBits()!=0`.
4. **[нужно подтвердить]** точная шкала `GetHealth("","Blood")` у ИИ-бота: абсолютная
   (5000 = полное) по ванили, НО комментарий `dmAISurvivor.c:1213` утверждает «0..1». Если
   абсолютная — формула `bloodInv = (blood-0.5)/0.5` в `GetDangerDistance` считает неверно
   (это потенциальный баг мода, вне scope, но влияет на порог «пойти лечиться»). **Прогнать**:
   `/bot status` у здорового бота — если `Blood=5000` (не `1.0`), комментарий/формула ошибочны.
5. **[нужно проверить]** что после `InsertAgent(INFLUENZA, 600) + ActivateModifier(MDF_INFLUENZA)`
   нужен явный `GetModifiersManager().OnScheduledTick(dt)` перед тем как
   `IsModifierActive(MDF_INFLUENZA)` станет `true` (активация ленивая, в `Tick`). Мод тикает
   менеджер в `TickBodySystems` с интервалом `DM_BOT_MODIFIER_TICK_INTERVAL` — подтвердить, что
   проверка `IsModifierActive` в тест-сценарии не сработает раньше первого тика.
6. **[нужно подтвердить]** что `SetLiquidType(LIQUID_WATER)` у свежесозданной `WaterBottle`
   не перетирается config'ом (`varLiquidTypeInit=512` уже = вода) — т.е. для «пустой» бутылки
   достаточно `SetQuantity(0)`, а `SetLiquidType(0)` (LIQUID_NONE) не обязателен. Проверить
   поведение `GetLiquidType()` у свежей vs обнулённой бутылки.

---

## 8. Существенные развилки / решения (для `docs/decisions.md`)

- **Путь заполнения бутылки у ИИ**: прямой статик `Liquid.FillContainer`/`FillContainerEnviro`
  (или `SetLiquidType`+`SetQuantityMax`) vs ванильный `ActionFillBottleBase` (client-driven,
  для ИИ не запускается). Решил зафиксировать оба серверных пути и пометить action как
  недоступный — выбор влияет на то, как тест «наполнил бутылку» пишет сценарий (детерминизм).
- **Форс инфекции — нужны ОБА вызова** (`InsertAgent` + `ActivateModifier`), иначе модификатор
  сам деактивируется по `DeactivateCondition` (порог 400). Это нетривиально (работает только
  «в связке») — стоит зафиксировать, чтобы тест не падал на «грипп не активен».
- **Шкала `GetHealth("","Blood")`** — ваниль абсолютная (5000), а комментарий в моде говорит
  «0..1». Развилка, влияющая на пороги лечения/опасности; требует эмпирической проверки (п.4).
