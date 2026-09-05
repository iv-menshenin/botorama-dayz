# Research: лут / инвентарь бота

Статус: не реализовано. Задачи T13–T16 плана. Ведёт субагент `dayz-research`.

## Цель

Боты собирают полезное, выбрасывают ненужное: перцепция предметов → оценка полезности →
взять/выбросить → состояние `Looting`.

## Известные стартовые точки

- Инвентарь: прямое создание и перемещение — см. `docs/research/entityai.md` и скилл
  `dayz-ai-bot` (раздел «Инвентарь / Loadout»).
- Взять предмет С ЗЕМЛИ (мир → инвентарь) — `TakeEntityToInventory(SERVER, CARGO, item)`
  (у предмета на земле локация уже есть — в отличие от свежесозданного).
- Оценка ёмкости/веса: `StaminaHandler` (вес режет кап стамины), `ItemBase.GetSingleInventoryItemWeightEx()`.

## Открытые вопросы (research)

1. Перцепция предметов в мире рядом с ботом: box/cone запросы (см. `perception.md`),
   фильтр по `ItemBase`/категории.
2. Оценка полезности: по чему ранжировать (категория, ценность CE, нужда бота — патроны
   к его калибру, еда/вода, медицина). Дизайн скоринга.
3. Pickup/drop: взять предмет с земли и выбросить (`CreateInInventory`? `TakeEntityTo*`?
   удаление/выброс на землю).
4. Взаимодействие с весом/ёмкостью рюкзака (переполнение → что выбросить).

Ответы на эти вопросы даёт Expansion (DayZ AI) — проверенные сигнатуры ниже в секциях
«Сигнатуры» и «Резюме». Главный вывод: **предмет = цель** (`eAIItemTargetInformation`),
утилита = `q / distance`, категоризация через ванильные `Object`/`ItemBase` методы,
фильтры «не брать» — через `eAI_ThreatOverride` (чёрный список), а не через удаление цели.

---

## Сигнатуры (Expansion DayZ AI — референс)

База: `/home/devalio/dayz/Work/DayZ-Expansion-Scripts/DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/`.

### 1. Скорер полезности: `eAIItemTargetInformation`

Файл `Classes/Targets/eAIItemTargetInformation.c` (наследует `eAIEntityTargetInformation`).

```c
override float CalculateThreat(eAIBase ai = null, eAITargetInformationState state = null)  // строка 51
override vector GetPosition(eAIBase ai = null, bool actual = false, eAITargetInformationState state = null)  // 46
override bool IsItem() { return true; }     // 19
override bool IsInanimate() { return true; } // 14
override bool ShouldRemove(eAIBase ai = null) { return GetThreat(ai) <= 0.1; }  // 289
```

**Фильтры → `0` (или `0.1` для «пометить и забыть»)** (строки 53–105):
- `IsDamageDestroyed()` || `IsSetForDeletion()` → 0 (53–54).
- Предмет уже у игрока (`GetHierarchyRootPlayer()` жив/в сознании) → 0 (56–58).
- `ai.eAI_GetThreatOverride(m_Item)` или `ai.IsRestrained()` → 0.1 (62–63).
- Недостижим (`pathFinding.m_IsUnreachable`/`m_IsTargetUnreachable`) или предмет
  obstructed/далеко (>4 м) — ставит `ai.eAI_ThreatOverride(m_Item, true)` и возвращает 0.1 (88–105).

**Утилита `q`** (строки 234–260):
| Категория | `q` | строка |
|---|---|---|
| `IsWeapon()` | 1000000 | 237 |
| `IsAmmoPile()` | 1000 | 241 |
| `Expansion_IsMeleeWeapon()` \|\| `IsMagazine()` | 10000 | 245 |
| бинт и `eAI_ShouldBandage()` (срочно) | 900000900 | 249 |

Возврат: `return q / distance;` (260), где `distance` — квадрат расстояния, зажат минимумом 1.0 (257–258).

**Немонотонные ветки** (не `q/distance`, а `ExpansionMath.PowerConversion(0.0, 227.968982, distance, 1.0, 0.0, 3.0)` — угроза 0.4 на 60 м, строки 231/254/267/276/280):
- предмет без LOS и не был моим (`m_Item.m_Expansion_PreviousOwner != ai`) — низкий приоритет реакции (227–232);
- `WeaponCleaningKit` (если нет ремкомплекта), `Edible_Base`/`IsFood()` (если `eAI_ShouldProcureFood()`),
  труп (`IsCorpse()` + есть melee → снятие шкуры) — 262–283.

**Доп. фильтры в `CalculateThreat`** (строки 108–225): бинтов уже ≥3 → 0; нет патронов под
ствол-цель (`eAI_HasAmmoForFirearm`) → 0; ствол в руках «лучше» (`eAI_WeaponSelection`) → 0;
магазин без оружия под него / уже есть N таких (`eAI_HasMagazineType*`) → 0; оружие-цель без
патронов и уже есть `m_eAI_Firearms`/`Handguns`/`Launchers` → 0.

### 2. Категории: `enum eAILootingBehavior`

Файл `3_Game/DayZExpansion_AI/Enums/AIenums.c:15–46` (битмаска):

```c
enum eAILootingBehavior
{
	NONE = 0,
	WEAPONS_FIREARMS = 1, WEAPONS_LAUNCHERS = 2, WEAPONS_MELEE = 4,
	WEAPONS = 7,                       // FIREARMS|LAUNCHERS|MELEE
	BANDAGES = 8,
	CLOTHING_ARMBAND = 16,
	CLOTHING_BACK_LARGE = 32, CLOTHING_BACK_MEDIUM = 64, CLOTHING_BACK_SMALL = 128,
	CLOTHING_BACK = 224,
	CLOTHING_BODY = 256, CLOTHING_EYEWEAR = 512, CLOTHING_FEET = 1024,
	CLOTHING_GLOVES = 2048, CLOTHING_HEADGEAR = 4096, CLOTHING_HIPS = 8192,
	CLOTHING_LEGS = 16384, CLOTHING_MASK = 32768, CLOTHING_MELEE = 65536,
	CLOTHING_SHOULDER = 131072, CLOTHING_VEST = 262144,
	CLOTHING_SIMILAR = 1048576,        // тот же base-тип, напр. TShirt_ColorBase
	CLOTHING_IDENTICAL = 2097152,      // тот же тип
	CLOTHING = 524272,                 // без SIMILAR/IDENTICAL
	FOOD = 4194304,
	UPGRADE = 524288,
	DEFAULT = 7,
	ALL = 5242879                      // WEAPONS|BANDAGES|CLOTHING|FOOD|UPGRADE
};
```

### 3. Состояние взятия предмета: `eAIState_TakeItemToInventory`

Файл `Classes/FSM/states/eaistate_takeitemtoinventory.c` (наследует `eAIState_TakeItem_Base`):

```c
override int OnUpdateEx(float DeltaTime, int SimulationPrecision)  // 3
{
	if (m_Item && !m_Item.Expansion_GetRootPlayerAliveExcluding(null))
	{
		if (unit.eAI_GetThreatOverride(m_Item)) return EXIT;
		if (!unit.eAI_TakeItemToInventory(m_Item)) { unit.eAI_Unbug(...); return EXIT; }
	}
	return CONTINUE;
}

int Guard()  // 20
```

**Guard-цепочка** (все → `eAITransition.FAIL`): `IsFighting` (26), `IsRestrained` (28),
`IsUnconscious` (29), `IsSwimming` (30), `eAI_IsChangingStance` (32), `IsRaised` (35),
`GetWeaponManager().IsRunning()` (37), `GetActionManager().GetRunningAction()` (38);
нет цели / не `ItemBase` / предмет у игрока / `IsSetForDeletion` / threat override (43);
live explosive или (`!IsClothing()` и `!CanPutInCargo(unit)`) (50);
`GetDistanceSq(true) > 4.0` или obstructed (53); `GetThreat() <= 0.1` (56).
Оружие и melee НЕ берутся этим состоянием (→ `TakeItemToHands`), строки 59–84;
труп → threat override (86–90); нет свободного слота → override (92–96).
Успех: `m_Item = targetItem; return eAITransition.SUCCESS;` (100–102).

База `Classes/FSM/states/eaistate_takeitem_base.c`: `OnUpdate` (32) ждёт опускания оружия
(`IsRaised` → `RaiseWeapon(false)`, 50–55) и смены стойки; `ChangeStance()` (105) — crouch/prone
перед подбором; таймауты 10с → `eAI_Unbug` + `EXIT`.

### 4. Лут-решение и утилиты в `eAIBase`

Файл `Entities/AI/eAIBase.c`.

```c
void eAI_ThreatOverride(EntityAI entity, bool state)  // 8612 — чёрный список «не брать»
bool eAI_GetThreatOverride(EntityAI entity)           // 8625
void eAI_PurgeThreatOverride()                        // 8633 — чистка null/дальних (>1000 м)

bool eAI_FindFreeInventoryLocationFor(ItemBase item, FindInventoryLocationType flags = 0, out InventoryLocation il_dst = null)  // 10363
bool eAI_TakeItemToInventory(ItemBase item, bool useAction = true)      // 10390 (StartActionObject(eAIActionTakeItem, item) либо Impl)
bool eAI_TakeItemToInventoryImpl(ItemBase item, FindInventoryLocationType flags = 0, bool threatOverrideOnFailure = true)  // 10476

bool eAI_CanLootWeapon(ItemBase weapon, eAIFaction faction)      // 10412
bool eAI_CanLootMeleeWeapon(ItemBase weapon, eAIFaction faction) // 10433
bool eAI_ShouldPickupBandage(ItemBase item)                      // 10444 (bleeding && 0 || bandages < 3)
bool eAI_ShouldProcureFood()                                     // 10468 (FOOD && m_eAI_Food.Count() < m_eAI_MaxFoodCount)
bool eAI_ShouldBandage()                                         // 1287 (не в опасности, недавно не били)
bool eAI_WeaponSelection(ItemBase currentItem, ItemBase compareItem)  // 3534 (сравнение DPS/health/attachments)
bool eAI_HasAmmoForFirearm(Weapon_Base gun, out Magazine mag, bool checkMagsInInventory = true) // 6619
bool eAI_HasMagazineType(string type)                            // 6474
bool eAI_HasMagazineTypeWithAmmo(string type)                    // 6488
bool eAI_HasWeaponForMagazine(Magazine mag)                      // 6502
int  eAI_GetMagazineTypeWithAmmoCount(string type)               // 6559
```

**Кэши инвентаря** (объявления строки 265–270; наполняются `eAI_AddItem` 6130–6257,
чистятся `eAI_RemoveItem` 6301+):
```c
ref array<Weapon_Base> m_eAI_Firearms = {};   // 265 (не launcher, не Pistol_Base)
ref array<Weapon_Base> m_eAI_Handguns = {};   // 266 (IsKindOf("Pistol_Base"))
ref array<Weapon_Base> m_eAI_Launchers = {};  // 267 (ShootsExplosiveAmmo)
ref array<ItemBase>    m_eAI_MeleeWeapons = {}; // 268
ref array<ItemBase>    m_eAI_Bandages = {};   // 269
ref array<ItemBase>    m_eAI_RepairKits = {}; // 270
int m_eAI_LootingBehavior = eAILootingBehavior.DEFAULT;  // 303
```

**История целей-предметов** (анти-осцилляция между двумя предметами):
```c
ref eAITarget m_eAI_ItemTargetHistory[3];  // 85
// логика в eAI_PrioritizeTargets (4017–4032): если новый предмет == history[1]
// и history[0] == history[2] → не переключаемся; иначе сдвиг кольца history.
```

**Блок категоризации цели-предмета** (в цикле по `m_eAI_PotentialTargetEntities`, строки
3214–3409): оружие (3232), melee (3270), магазин (3301), бинт (3319), ремкомплект (3324),
одежда (3329, проверка `Expansion_GetInventorySlots()` + `eAI_ClothingSelection`), еда (3390,
`IsCorpse()`+melee → skinning; `IsFood()`; отсев ROTTEN), всё прочее → `continue` (3404–3407).

---

## Ванильные методы категоризации (подтверждены)

База: `/home/devalio/dayz/Work/DayZ Projects/scripts/`.

| Метод | Где объявлен | Строка |
|---|---|---|
| `bool IsWeapon()` | `3_game/entities/object.c` (`Object`) | 636 (override `weapon.c:6`) |
| `bool IsMeleeWeapon()` | `3_game/entities/object.c` | 642 (override `inventoryitem.c:81` читает `isMeleeWeapon`) |
| `bool IsMagazine()` | `3_game/entities/object.c` | 571 (override `itembase/magazine/magazine.c:172`) |
| `bool IsAmmoPile()` | `3_game/entities/object.c` | 577 (override `magazine/ammunitionpiles.c:27`) |
| `bool IsClothing()` | `3_game/entities/object.c` | 589 (override `clothing_base.c:6`) |
| `bool IsContainer()` | `3_game/entities/object.c` | 565 |
| `bool IsBuilding()` | `3_game/entities/object.c` | 648 |
| `bool ShootsExplosiveAmmo()` | `3_game/entities/object.c` | 670 |
| `bool IsFood()` | `3_game/entities/object.c` | 719 (`IsFruit()||IsMeat()||IsCorpse()||IsMushroom()`) |
| `bool IsCorpse()` | `3_game/entities/object.c` | 734 (override `edible_base.c:372`) |
| `proto native bool IsDamageDestroyed()` | `3_game/entities/object.c` | 977 |
| `bool IsSetForDeletion()` | `3_game/entities/entityai.c` | 807 |
| `proto native Man GetHierarchyRootPlayer()` | `3_game/entities/entityai.c` | 877 |
| `bool CanPutInCargo(EntityAI parent)` | `3_game/entities/entityai.c` | 1579 (override `itembase.c:4162`) |
| `bool IsTakeable()` | `3_game/entities/entityai.c` | 1829 (override `itembase.c:4400`) |
| `bool HasQuantity()` | `3_game/entities/entityai.c` | 2237 |
| `override float GetSingleInventoryItemWeightEx()` | `4_world/entities/itembase.c` | 3519 |
| `proto native bool IsInherited(typename)` | `1_core/proto/enconvert.c` (и `enscript.c:23`) | 549 |

**`IsDrink()` — НЕ ванильный метод `ItemBase`/`EntityAI`.** Найден только на `ActionBase`
(`4_world/classes/useractionscomponent/actionbase.c:294`) — это классификация *действия*, не
предмета. Напитки на уровне предмета различают по `ItemBase.GetLiquidType() != 0`
(`itembase.c:913`) либо по наследованию (`IsInherited(SodaCan_ColorBase)`). Expansion в луте
использует только `IsFood()`/`Edible_Base` и «питьё» отдельно не классифицирует.

**Взятие с земли (ваниль):** `bool TakeEntityToInventory(InventoryMode mode, FindInventoryLocationType flags, notnull EntityAI item)` — `3_game/systems/inventory/inventory.c:1012`.

**Готча (обожглись): `TakeEntityToInventory(SERVER)` двигает предмет на сервере, но у ИИ-бота
клиент НЕ видит перемещение** — вещь остаётся на полу, модель голая (лог «Приаттачил» = true, а
предмет на месте). Штатный `SendServerMove` не синкает серверного ИИ как игрока. Фикс (эталон
`eAIBase.eAI_TakeItemToLocation`, `eAIBase.c:10605`): ручной ре-синк сетевой репрезентации:
```c
GetGame().RemoteObjectTreeDelete(item);   // клиент убирает со старой позиции
bool ok = LocalTakeToDst(src, dst);        // локальный перенос (Man.c:842, без SendServerMove)
GetGame().RemoteObjectTreeCreate(item);   // клиент рисует в новой позиции
```
`RemoteObjectTreeDelete/Create` — `3_game/global/game.c:706/708`; `Man.LocalTakeToDst` —
`3_game/entities/man.c:842`.

**Слот-зависимое размещение** (портировано в `dmLoot.FindDestination`): одежда → свободный слот
из `item.ConfigGetTextArray("inventorySlot", slots)` (`Object.c:894`) через
`InventorySlots.GetSlotIdFromString`/`HasAttachmentSlot`/`FindAttachment`; мили →
`InventorySlots.SHOULDER`/`MELEE` затем `HANDS`; оружие → `HANDS`; остальное → `CARGO`
(`FindFreeLocationFor(item, CARGO, loc)`). `InventoryLocation.SetAttachment/SetHands` —
`inventorylocation.c:130/171`.

**Готча (краш 1, 17:14:12): `FindDestination` возвращал `true` в ветке «свободный слот», но НЕ
заполнял `out InventoryLocation dst`.** Вызывающий `TakeToAttachmentSlot` звал
`LocalTakeToDst(src, dst)` с дефолтным (неинициализированным) `dst` → `PlayerBase.TakeToDstImpl`
(`playerbase.c:9281`) → `CheckAndExecuteStackSplitToInventoryLocation(dst, dst.GetItem())`
(`playerbase.c:9188`) → `dst.GetItem()` = null → NULL-ptr (`#return`). Правило: **каждая
return-true ветка метода, возвращающего назначение через `out`, обязана его заполнить** (здесь —
`dst.SetAttachment(pawn, item, slot)` перед `return true`; ветка «занятый слот» и так заполняет
через `FindFreeLocationFor(..., dst)`).

**Готча (краш 2, 17:15:02): сброс на землю тоже НЕЛЬЗЯ делать `DropEntity(InventoryMode.SERVER, …)`.**
SERVER-режим откладывает манипуляцию в CommandHandler
(`HumanInventory.Update` → `HandleInventoryManipulation` → `DayZPlayerInventory.HandleTakeToDst`),
где у серверного ИИ падает NULL-ptr (variable `player` при `GetHumanInventory`) — и сброс не
происходит (слот остаётся занят, `FindDestination` дальше даёт «нет места»). Аналог эталона выше
для направления «на землю» — LOCAL-режим с тем же ручным ре-синком:
```c
GetGame().RemoteObjectTreeDelete(item);
bool ok = LocalDropEntity(item);   // Man.c:132 → GetHumanInventory().DropEntity(LOCAL) → TakeToDst(LOCAL)
GetGame().RemoteObjectTreeCreate(item);
```
`LocalDropEntity`/`ServerDropEntity`/`PredictiveDropEntity`/`JunctureDropEntity` — `man.c:113-139`,
объявлены на `EntityAI` (`entityai.c:2043-2056`, дефолт `false`); `Inventory.DropEntity` (gameinventory,
`inventory.c:1264`) внутри сводится к `TakeToDst(mode, src, SetGroundPosByOwner(...))`. Для предмета
НЕ в руках `HumanInventory.DropEntity` (`humaninventory.c:139`) идёт по ветке `default` → `super.DropEntity`
(немедленно, без `HandEvent`).

---

## Резюме: как Expansion делает лут

1. **Предмет = цель.** `eAIItemTargetInformation` оборачивает `ItemBase`, `CalculateThreat`
   возвращает «угрозу» (утилиту) цели. Приоритизация целей — общая (`eAI_PrioritizeTargets`),
   поэтому лут ранжируется тем же механизмом, что и враги/шумы.
2. **Утилита = `q / distance`** (для оружия/магазина/ammo-pile/меле/срочного бинта), либо
   `PowerConversion` по дистанции для «фоновых» нужд (еда/ремкомплект/труп, когда нужда есть).
   `q` — константа категории (1000000 / 10000 / 1000 / 900000900).
3. **Фильтры «не брать»** выносятся в `eAI_ThreatOverride` (чёрный список): недостижимый,
   obstructed, не подходит, уже есть N таких, недавно выброшен, не влезает → `ThreatOverride(item, true)`,
   чтобы не пересчитывать каждый кадр. Периодический `eAI_PurgeThreatOverride`.
4. **Кэши инвентаря** (`m_eAI_Firearms/Handguns/Launchers/MeleeWeapons/Bandages/RepairKits/Magazines/Food`)
   поддерживаются событиями `eAI_AddItem`/`eAI_RemoveItem` — фильтры «уже есть 3 бинта»/«нет
   патронов под ствол»/«нет оружия под магазин» смотрят в кэши, а не в живой инвентарь.
5. **История целей-предметов** (`m_eAI_ItemTargetHistory[3]`) гасит осцилляцию бота между двумя
   равно-приоритетными предметами.
6. **Взятие** — отдельное FSM-состояние (`TakeItemToInventory`) с guard-цепочкой (не в бою, не
   restrained/без сознания/в воде, оружие опущено, в пределах 4 м, не obstructed, threat > 0.1,
   есть свободный слот) и `unit.eAI_TakeItemToInventory(item)` (поиск слота + `TakeEntityTo*`).
   Оружие/меле берутся отдельным состоянием `TakeItemToHands`.
7. **Что разрешено** управляется битмаской `m_eAI_LootingBehavior` (задаётся из конфига фракции).

---

## Что портировать → в какую сущность botorama

(сопоставление с планом `docs/plans/looting-and-exploration.md`: `dmLoot` / `dmWishlist` /
`dmRequirements` / `dmNeeds` / интенты.)

| Expansion (референс) | botorama | Замечание |
|---|---|---|
| `eAIItemTargetInformation.CalculateThreat` (q/distance + фильтры) | `dmWishlist.CalcDesired(item) → 0..1` | Expansion сливает «желание» и «утилиту/дистанцию» в одно число. В botorama желание — чистый скорер (`dmWishlist`), а дистанция/путь уходит в приоритет интента `PickUp`. |
| `enum eAILootingBehavior` (битмаска) | реестр желаний `dmWishlist.SetDesired(itemClass, necessity)` / конфиг бота | Битмаску категорий → «что вообще можно брать» = желания + `GetCategory` из `dmLoot`. |
| Фильтры «не брать» `eAI_ThreatOverride` / `eAI_PurgeThreatOverride` | `dmWishlist.Ignore(item)` / `Unignore` (с таймаутом авто-Unignore) | Переносить именно «чёрный список с авто-протухом», а не удаление предмета из перцепции. |
| `m_eAI_ItemTargetHistory[3]` (анти-осцилляция) | история целей в интент-арбитраже / `dmWishlist` | Кольцевой буфер последних 3 предметных целей. |
| Кэши `m_eAI_Firearms/Handguns/Launchers/Melee/Bandages/RepairKits/Magazines/Food` | `dmRequirements` (анализатор инвентаря, тик ~5с) | `dmRequirements.Update()` строит тот же индекс необходимости; фильтры «уже есть N» читают его. |
| `eAI_ShouldBandage` / `eAI_ShouldProcureFood` / `eAI_ShouldPickupBandage` | `dmNeeds` (инвентарь → желания) | Пороги: бинтов <3, еды < `MaxFoodCount` (≈5% свободного cargo), голод/кровотечение. |
| `eAI_WeaponSelection` / `eAI_ClothingSelection` / `Expansion_CompareDPS` | fallback-ветки `dmWishlist.CalcDesired` | Сравнение DPS/health/attachments для апгрейда оружия; слот+качество для одежды. |
| `eAIState_TakeItemToInventory.Guard` | guard `PickUp`-интента / `Require(...)` рёбер FSM | Те же гейты: не в бою/restrained/без сознания/воде, дистанция ≤4 м, не obstructed, `CanPutInCargo` или одежда, threat > порог. |
| `eAI_FindFreeInventoryLocationFor` + `eAI_TakeItemToInventoryImpl` | `PickUp.OnUpdate` → ванильный `TakeEntityToInventory(SERVER, CARGO/ATTACHMENT, item)` | Поиск свободного слота перед взятием; одежда → слот attachment, прочее → cargo. |
| «оружие/меле → TakeItemToHands» | отдельный интент / ветка `PickUp` для оружия в руки | Оружие не в cargo, а в руки (см. combat.md/loadout). |

**Ключевая упрощающая идея для botorama:** не делать полноценный `CalculateThreat` с
`q/distance` в перцепции. Разделить: `dmWishlist.CalcDesired` (чистое желание по категории/калибру/
качеству) + приоритет интента по дистанции. Фильтры «не брать» — `Ignore` с таймаутом, как
`eAI_ThreatOverride`. Кэши и пороги — в `dmRequirements`/`dmNeeds`.

## Источники

- `docs/research/entityai.md`, `docs/research/perception.md`
- `DayZ Projects/scripts/3_game/systems/inventory/inventory.c` (`TakeEntityToInventory:1012`)
- `DayZ Projects/scripts/3_game/entities/object.c`, `entityai.c`, `inventoryitem.c`; `4_world/entities/itembase.c`
- Expansion AI: `Classes/Targets/eAIItemTargetInformation.c`, `Classes/FSM/states/eaistate_takeitemtoinventory.c` (+`_base.c`), `3_Game/DayZExpansion_AI/Enums/AIenums.c`, `Entities/AI/eAIBase.c`
- План портирования: `docs/plans/looting-and-exploration.md`

---

## Chamber-loading (break-action, e.g. B95)

Статус: подтверждено по ванили. Задача — зарядить патронники break-action двустволки B95 из
россыпной пачки `Ammo_308Win` серверным `WeaponManager` (для `dmAISurvivorBase.ReloadWeaponAI`).

База ванили: `/home/devalio/dayz/Work/DayZ-Script-Diff/scripts/`.
Референс ИИ: `DayZ-Expansion-Scripts/.../DayZExpansion_AI/` (`eAIWeaponManager.c`, `eAIBase.c`).

### Иерархия классов

`B95 : B95_base : DoubleBarrel_Base : Rifle_Base : Weapon_Base : Weapon`

- `4_world/entities/firearms/rifle/b95.c` — `B95_base : DoubleBarrel_Base` (ничего не переопределяет).
- `4_world/entities/firearms/doublebarrel_base.c` — `DoubleBarrel_Base : Rifle_Base` (ключевой файл).
- У B95 **нет внутреннего магазина** и **нет отъёмного магазина**: `SpawnAmmo` идёт веткой
  `FillChamber` (`weapon_base.c:758-760`), т.к. `HasInternalMagazine(-1)=false` и
  `GetMagazineTypeCount(0)=0`. Патроны — только в 2 ствола (chambers).

### 1. LoadBullet vs LoadMultiBullet

`DoubleBarrel_Base.SetActions()` (`doublebarrel_base.c:312-317`) регистрирует **только**
`FirearmActionLoadMultiBulletQuick` и `FirearmActionLoadMultiBullet` — и **НЕ** добавляет
`FirearmActionLoadBullet`. Вывод: для break-action ваниль использует **непрерывную** зарядку.

Разница на уровне `WeaponManager` (`weaponmanager.c`):

| Вызов | `StartAction` | `m_WantContinue` в `StartPendingAction` |
|---|---|---|
| `LoadBullet(mag, ctrl=null)` (422) | `AT_WPN_LOAD_BULLET` | **false** (835) — ровно 1 патрон |
| `LoadMultiBullet(mag, ctrl=null)` (427) | `AT_WPN_LOAD_MULTI_BULLETS_START` | **не трогает** (841) — остаётся true, грузит до упора |
| `LoadMultiBulletStop()` (432) | — | `if (m_InProgress) m_WantContinue = false;` (434) |

Оба постит `WeaponEventLoad1Bullet` (835 / 841). Цикл продолжается по трём гардам
(`weaponchambering.c:846`, `guards.c`):
1. `WeaponGuardHasAmmoInLoopedState` — в пачке ещё есть патроны (`m_srcMagazine.GetAmmoCount() > 0`, `guards.c:527`);
2. `WeaponGuardChamberMultiHasRoomBulltet` — `GetTotalMaxCartridgeCount(i) - GetTotalCartridgeCount(i) >= 1` для любого ствола (`guards.c:478`);
3. `WeaponGuardWeaponManagerWantContinue` — `m_WantContinue` (`guards.c:606`).

Для B95 (2 ствола, каждый = 1 chamber) `LoadMultiBullet` сам заряжает **оба ствола** и
останавливается, когда оба полны или пачка опустела. `LoadBullet` зарядил бы ровно 1.

### 2. CanLoadBullet / CanLoadMultipleBullet

Оба требуют (`weaponmanager.c:200-229` / `231-284`):
- оружие в руках: `m_player.GetHumanInventory().GetEntityInHands() != wpn → false`;
- `!mag.IsDamageDestroyed()`, `!wpn.IsDamageDestroyed()`;
- `!wpn.IsJammed()`, `!m_player.IsItemsToDelete()`;
- `reservationCheck` → нет `HasInventoryReservation(wpn)`/`(mag)`.

Разница:
- `CanLoadBullet` (200): `for i < GetMuzzleCount(): if (wpn.CanChamberBullet(i, mag)) return true` — true если **хоть один** ствол примет патрон.
- `CanLoadMultipleBullet` (231): true только если можно зарядить **2+** патрона подряд (два пустых/выбитых ствола, или ствол + место во внутреннем магазине). Для B95 с **одним** пустым стволом → **false**.

`DoubleBarrel_Base.CanChamberBullet` (`doublebarrel_base.c:299-310`):
```c
if (CanChamberFromMag(muzzleIndex, mag))
    return IsChamberEmpty(muzzleIndex) || IsChamberFiredOut(muzzleIndex);
return false;
```

**Рекомендация:** проверять `wm.CanLoadBullet(wpn, mag)` (не `CanLoadMultipleBullet`) и затем
вызывать `wm.LoadMultiBullet(mag)`. Так делает Expansion (`eAIBase.ReloadWeaponAI:8955` →
`LoadMultiBullet`; `FirearmActionLoadMultiBulletRadial.ActionCondition` тоже через `CanLoadBullet`,
`firearmactionloadmultibullet.c:206`).

### 3. Поиск россыпной пачки под оружие

- `mag.IsAmmoPile()` — loose ammo stack (`ammunitionpiles.c:27`; `Ammo_308Win : Ammunition_Base`,
  `ammunitionpiles.c:66`).
- `wpn.CanChamberFromMag(i, mag)` — **нативный** и главный способ проверки «патроны этой пачки
  заряжаемы в ствол i» (`weapon.c:255`). Читает конфиг `chamberableFrom`. Именно его использует
  `WeaponManager.SetSutableMagazines` для пачек (`weaponmanager.c:1298`) и Expansion
  `CanLoadBullet_NoHandsCheck_NoChamberCheck` (`eAIWeaponManager.c:352-376`, цикл по muzzles).
- Альтернатива по типу: `AmmoTypesAPI.MagazineTypeToAmmoType(mag.GetType(), out ammoType)`
  (`ammotypes.c:14`) + сравнение с `wpn.GetRandomChamberableAmmoTypeName(i)` (`weapon.c:143`).
  `GetChamberAmmoTypeName(i)` (`weapon.c:150`) возвращает тип **заряженного** патрона в стволе
  (используется только при `!IsChamberEmpty`, `weapon_base.c:191-194`) — для **пустого** ствола не годится.
- `mag.GetAmmoCount()` (`magazine.c:68`, native) > 0 — пачка не пустая; `GetAmmoMax()` (158) — ёмкость стека.

Эталон выбора пачки — `eAI_GetMagazineToReload` (`eAIBase.c:1583`): перебор `m_eAI_Magazines`,
приоритет отъёмного магазина (attach/swap), иначе пачка через `CanLoadBullet_NoHandsCheck_NoChamberCheck`;
для пачек берёт минимальный по `GetAmmoCount()`. В `FindReloadMagazine` бота (текущий
`dmAISurvivorBase.c:1425`) пачки сейчас отфильтрованы (`mag.IsAmmoPile() → continue`) — для
chamber-loading нужен отдельный проход, который НЕ отбрасывает `IsAmmoPile()` и проверяет
`CanChamberFromMag`/`CanLoadBullet`.

### 4. Завершение действия и число стволов

- Запуск асинхронный: `StartAction` ставит `m_InProgress=true`, `m_readyToStart=true`;
  реальный ивент постится в `StartPendingAction` из `Update` (`weaponmanager.c:943-948`).
- Завершение: `Update` ждёт `m_canEnd && m_WeaponInHand.IsIdle()` → `OnWeaponActionEnd()` →
  `m_InProgress=false` (`weaponmanager.c:953-958`, `995`). Опрос для AI: `wm.IsRunning()` (872).
- **Один `LoadMultiBullet(mag)` заряжает оба ствола**; два вызова `LoadBullet` не нужны.
  `LoadMultiBulletStop()` нужен только чтобы прервать раньше (напр. бой) — цикл сам
  останавливается по гардам «нет места / пачка пуста».
- Проверка результата: `wpn.GetMuzzleCount()` (2), `IsChamberEmpty(i)` / `IsChamberFiredOut(i)`
  (`weapon.c:74/79`), либо `GetTotalCartridgeCount(i)` == `GetTotalMaxCartridgeCount(i)`.
- Таймаут-страховка: Expansion держит 12с (`eaistate_weapon_reloading_reloading.c:36`).

### 5. SpawnAmmo("Ammo_308Win", CHAMBER)

`SpawnAmmo` (`weapon_base.c:748`) → `HasInternalMagazine(-1)` false → `GetMagazineTypeCount(0)==0`
→ `FillChamber(magazineType, flags)` (759). `FillChamber` (`weapon_base.c:929`): с `CHAMBER`
`amountToChamber = GetMuzzleCount()` (=2), по каждому стволу `FillSpecificChamber(m)` (965-966),
который заполняет только `IsChamberEmpty` (988). Итог: **оба ствола заряжены** → состояние
`DoubleBarrelLoadedLoaded` (L_L), `doublebarrel_base.c:25-35`.

### Рекомендация для `ReloadWeaponAI`

После unjam/eject и перед attach/swap добавить ветку:
1. `if (wm.CanLoadBullet(weapon, mag))` где `mag` — непустая `IsAmmoPile()` из инвентаря
   (найдена по `CanChamberFromMag`/`CanLoadBullet`).
2. `wm.LoadMultiBullet(mag)` (одним вызовом, зарядит оба ствола).
3. В `dmBotWeaponManager.StartPendingAction` добавить case'ы `AT_WPN_LOAD_BULLET` (с
   `m_WantContinue=false`) и `AT_WPN_LOAD_MULTI_BULLETS_START` (пост `WeaponEventLoad1Bullet`),
   иначе ивент никогда не постится (сейчас switch имеет только ATTACH/SWAP/DETACH/UNJAM/EJECT,
   `dmBotWeaponManager.c:64-94`).
4. Опрос `wm.IsRunning()` до false, затем проверить `!IsChamberEmpty` обоих стволов.

### Подводные камни

1. **Серверный путь** (главное): ванильный `WeaponManager.StartAction` возвращает `false` на
   мультиплеерном сервере без `control_action` (`weaponmanager.c:787`). У бота уже есть
   `dmBotWeaponManager`, но **обязательно** добавить обработку load-ивентов (см. выше) — иначе
   `LoadBullet`/`LoadMultiBullet` ничего не сделают.
2. **Оружие в руках**: `CanLoadBullet`/`CanLoadMultipleBullet` требуют
   `GetHumanInventory().GetEntityInHands() == wpn`. `ReloadWeaponAI` читает именно из рук — ок.
   Expansion держит `_NoHandsCheck`-варианты на случай, если бот целится из другого предмета.
3. **Пачка временно уходит в левую руку**: `ChamberMultiBullet.OnEntry` двигает пачку в
   `InventorySlots.LEFTHAND` (`weaponchambering.c:875-876`); если левая рука занята — зарядка
   падает. У ИИ-бота левая рука обычно свободна, но учесть.
4. **Jammed**: `CanLoadBullet` = false при `IsJammed()` — сначала unjam (в `ReloadWeaponAI` уже есть).
5. **Reservation**: `InventoryReservation` (`weaponmanager.c:338`) резервирует оружие+пачку;
   если прошлая операция не сняла резерв, `StartAction` вернёт false. `dmBotWeaponManager.OnWeaponActionEnd`
   уже чистит резервы (`dmBotWeaponManager.c:113-144`).
6. **Пустая/1-патронная пачка**: `destroyOnEmpty` съедает пачку при 0; при 1 патроне зарядится
   1 ствол. `LoadMultiBullet` безопасен и в этом случае (останавливается по гарду «нет патронов»).
7. **`GetChamberAmmoTypeName` ≠ chamberable type**: он читает патрон, **уже** находящийся в
   стволе (не список «подходящих»). Для сопоставления пачки с оружием используй
   `CanChamberFromMag` / `GetRandomChamberableAmmoTypeName`, а не `GetChamberAmmoTypeName`.

### Сигнатуры (путь + строка)

| Сигнатура | Файл:строка |
|---|---|
| `bool CanLoadBullet(Weapon_Base, Magazine, bool reservationCheck=true)` | `weaponmanager.c:200` |
| `bool CanLoadMultipleBullet(Weapon_Base, Magazine, bool=true)` | `weaponmanager.c:231` |
| `bool LoadBullet(Magazine, ActionBase=null)` | `weaponmanager.c:422` |
| `bool LoadMultiBullet(Magazine, ActionBase=null)` | `weaponmanager.c:427` |
| `void LoadMultiBulletStop()` | `weaponmanager.c:432` |
| `bool StartAction(int, Magazine, InventoryLocation, ActionBase=null)` (server-return false на 787) | `weaponmanager.c:770` |
| `bool IsRunning()` / `bool WantContinue()` | `weaponmanager.c:872` / `1069` |
| `Magazine GetPreparedMagazine()` / `GetNextPreparedMagazine(out int)` | `weaponmanager.c:1074` / `1097` |
| `void SetSutableMagazines()` (CanChamberFromMag на 1298) | `weaponmanager.c:1264` |
| `bool CanChamberBullet(int, Magazine)` (base) | `weapon_base.c:324` |
| `bool SpawnAmmo(string, int flags=CHAMBER)` | `weapon_base.c:748` |
| `bool FillChamber(string, int)` / `FillSpecificChamber(int, float, string)` | `weapon_base.c:929` / `986` |
| `proto native bool CanChamberFromMag(int, Magazine)` | `weapon.c:255` |
| `proto native int GetMuzzleCount()` | `weapon.c:16` |
| `proto native bool IsChamberEmpty/FiredOut(int)` | `weapon.c:74/79` |
| `proto native bool HasInternalMagazine(int)` (`-1`=все стволы) | `weapon.c:106` |
| `proto native owned string GetChamberAmmoTypeName(int)` / `GetRandomChamberableAmmoTypeName(int)` | `weapon.c:150/143` |
| `static bool MagazineTypeToAmmoType(string, out string)` | `ammotypes.c:14` |
| `override bool IsAmmoPile()` | `ammunitionpiles.c:27` |
| `proto native int GetAmmoCount()` / `int GetAmmoMax()` | `magazine.c:68` / `158` |
| `override bool CanChamberBullet(int, Magazine)` (B95) | `doublebarrel_base.c:299` |
| `override void SetActions()` (только LoadMultiBullet*) | `doublebarrel_base.c:312` |
| `eAIWeaponManager.StartAction` (server-path эталон) | `eAIWeaponManager.c:17` |
| `eAIWeaponManager.CanLoadBullet_NoHandsCheck(_NoChamberCheck)` | `eAIWeaponManager.c:326/352` |
| `eAIBase.ReloadWeaponAI` (ветка chamber-load на 8955) | `eAIBase.c:8825` |
| `eAIBase.eAI_GetMagazineToReload` (выбор пачки) | `eAIBase.c:1583` |
