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
