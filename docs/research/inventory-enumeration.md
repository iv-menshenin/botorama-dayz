# Research: рекурсивное перечисление инвентаря ИИ-бота (ранжирование/репак)

Статус: исследование API. Ведёт субагент `dayz-research`.
Дополняет `docs/research/loot.md` (инвентарные примитивы, готчи ре-синка),
`docs/research/entityai.md`, скилл `dayz-ai-bot` (разделы «Инвентарь / Loadout», «Лут»).

## Цель

Дать однозначную модель «собрать все дропаемые карго-предметы бота (главное + контейнеры +
вложенные), исключая аттачи» и ответить на критичный вопрос: **есть ли у пешки собственное
карго**. Плюс блок классификации/распаковки `AmmoBox_*`.

## Модель (зафиксирована в задаче)

```
ОБХОД(сущность):
  для каждого СЛОТА сущности:
    вещь = слот
    если у вещи есть КАРГО → собрать предметы карго (это ДРОПАЕМОЕ)
    если у вещи есть СЛОТЫ → ОБХОД(вещь)   // вложенные слоты (аттачи — НЕ дропаем)
```

Ключевое различие: **КАРГО = дропаемое** (для репака), **СЛОТЫ = аттачи** (магазин в оружии,
GPS/мачете в слоте рюкзака) — НЕ дропаем. Ниже всё подтверждается по ванили.

---

## 1. API слотов сущности

База: `DayZ Projects/scripts/3_game/systems/inventory/inventory.c` (класс `GameInventory`).

| Сигнатура | Что делает | file:line |
|---|---|---|
| `proto native int GetAttachmentSlotsCount()` | число **сконфигурированных** слотов-аттачей (включая пустые) | `inventory.c:184` |
| `proto native int GetAttachmentSlotId(int index)` | id слота по индексу (index ∈ [0, Count)) | `inventory.c:180` |
| `proto native EntityAI FindAttachment(int slot)` | сущность в слоте **по id слота**, `null` если пусто | `inventory.c:224` |
| `proto native EntityAI FindAttachmentByName(string slotName)` | то же по имени слота | `inventory.c:228` |
| `proto native int AttachmentCount()` | число **реально приаттаченных** сущностей | `inventory.c:205` |
| `proto native EntityAI GetAttachmentFromIndex(int index)` | аттач по индексу (index ≠ slot!) | `inventory.c:220` |
| `bool HasAttachmentSlot(int slotId)` | скриптовая проверка «есть ли слот» (цикл по `GetAttachmentSlotsCount`) | `inventory.c:190` |
| `EntityAI FindAttachmentBySlotName(string)` (на `EntityAI`) | обёртка `FindAttachment(GetSlotIdFromString(...))` | `entityai.c:1847` |
| `bool CanDisplayCargo()` (на `EntityAI`) | `GetInventory().GetCargo() != null` | `entityai.c:1786` |

**Где работает.** `GetInventory()` есть у любой `EntityAI` (`entityai.c` → `proto native
GameInventory GetInventory()`, строка 1834). Слоты есть у пешки (одежда: Head/Shoulder/Melee/
Headgear/.../Feet/Splint_Right), у одежды (свои слоты), у рюкзака (`Backpack_1` — инструмент,
см. `loot.md`), у оружия (оптика/магазин/приклад/штык). Иерархия слотов произвольной глубины —
рекурсия обязательна.

**Две равноценные идиомы перечисления слотов** (подтверждены ванилью):

- **По id слота** (ваниль `EntityAI.SetInvisibleRecursive`, `entityai.c:4021-4050`):
  ```c
  int n = inventory.GetAttachmentSlotsCount();
  for (int i = 0; i < n; ++i)
      attachmentsArray.Insert(inventory.GetAttachmentSlotId(i));
  foreach (int slot : attachmentsArray)
      item = parentInventory.FindAttachment(slot);   // null = пустой слот
  ```
- **По индексу аттача** (Expansion `ExpansionItemSpawnHelper.Clone`,
  `ExpansionItemSpawnHelper.c:645-651`):
  ```c
  for (int i = 0; i < src.GetInventory().AttachmentCount(); i++)
      cSrc = src.GetInventory().GetAttachmentFromIndex(i);
  ```

---

## 2. API карго сущности

| Сигнатура | Что делает | file:line |
|---|---|---|
| `proto native CargoBase GetCargo()` | карго сущности или `null`, если «не карго-контейнер» | `inventory.c:138` |
| `proto native CargoBase GetCargoFromIndex(int index)` | карго по индексу (много-карговые объекты) | `inventory.c:139` |
| `proto native int GetItemCount()` | число предметов в карго | `cargo.c:28` |
| `proto native EntityAI GetItem(int index)` | предмет карго по индексу | `cargo.c:32` |
| `proto native int GetWidth()` / `GetHeight()` | размеры сетки карго | `cargo.c:37/41` |
| `proto bool GetItemSize(int index, out int w, out int h)` | размеры предмета в карго | `cargo.c:51` |
| `proto native EntityAI GetCargoOwner()` | владелец карго | `cargo.c:12` |
| `bool HasAnyCargo()` (на `EntityAI`) | `GetCargo()` не-null && `GetItemCount()>0` | `entityai.c:677` |

**У каких сущностей есть карго, у каких нет.** Наличие карго задаётся конфигом
`itemsCargoSize[]` в `CfgVehicles` (или proxy-cargo у построек). Есть: одежда/рюкзак/жилет/
коробки (`AmmoBox`/`Bear_ColorBase` — `DZ/gear/containers/config.cpp`), протектор-кейсы.
Нет: оружие, магазины, пачки, мелкие предметы, и — ключевое — **сама пешка** (см. п.4).
Программно «есть ли карго» = `entity.GetInventory().GetCargo() != null` (ровно так делает
ваниль: `entityai.c:1786`, `HasAnyCargo` `entityai.c:677-683`).

---

## 3. Отличие КАРГО от СЛОТА (аттача)

| Сигнатура | file:line |
|---|---|
| `enum InventoryLocationType { UNKNOWN, GROUND, ATTACHMENT, CARGO, HANDS, PROXYCARGO, VEHICLE, TEMP }` | `inventorylocation.c:3-12` |
| `proto native int GetType()` (на `InventoryLocation`) | `inventorylocation.c:43` |
| `proto native bool GetCurrentInventoryLocation(out notnull InventoryLocation loc)` (на `GameInventory`) | `inventory.c:268` |
| `bool IsInCargo()` (на `GameInventory`) — `loc.GetType()==CARGO` | `inventory.c:397` |
| `bool IsAttachment()` (на `GameInventory`) — `loc.GetType()==ATTACHMENT` | `inventory.c:410` |

**Однозначный способ отличить:**
1. `item.GetInventory().GetCurrentInventoryLocation(loc)` → `loc.GetType()`.
2. `== InventoryLocationType.CARGO` (или `PROXYCARGO`) → дропаемое карго.
3. `== InventoryLocationType.ATTACHMENT` → аттач (НЕ дропаем).
4. `== InventoryLocationType.HANDS` → предмет в руках (отдельная ветка).

**И «ручной» способ — без `GetCurrentInventoryLocation`:** `FindAttachment(slot)` возвращает
ТОЛЬКО слот-вещи, а `GetCargo().GetItem(i)` — ТОЛЬКО карго-вещи. Два этих перечислителя
не пересекаются, поэтому при ручной рекурсии (п.5, вариант B) различие даётся структурно, без
чтения локации. Локация (`GetCurrentInventoryLocation`) нужна только если перечисляешь всё
плоским `EnumerateInventory` и потом фильтруешь.

---

## 4. «Главное карго» пешки — НЕ СУЩЕСТВУЕТ

**Ответ однозначный: у `PlayerBase`/`SurvivorBase` (и `dmAISurvivorBase`) НЕТ собственного
карго. `pawn.GetInventory().GetCargo()` возвращает `null`.**

Доказательство (конфиг пешки):

- `DayZ Projects/DZ/characters/data/config.cpp:122` — `class SurvivorBase: Man`. В нём есть
  `attachments[]` (Head/Shoulder/Melee/Headgear/Mask/Eyewear/Hands/LeftHand/Gloves/Armband/Vest/
  Body/Back/Hips/Legs/Feet/Splint_Right, строки ~133-151) и **НЕТ** `itemsCargoSize[]`.
- `DayZ Projects/DZ/characters/data/config.cpp:119` — `class Man: AllVehicles {}` (пусто;
  конкретика Man — в C++ конфиге движка, но в скриптах он тоже без `itemsCargoSize`).
- По всему `DZ/` и `scripts/` ни у одного класса-пешки нет `itemsCargoSize` (поиск по
  `SurvivorBase`/`SurvivorM_*`/`SurvivorF_*`).

Следствие: **loose-предметы всегда живут в карго какого-то контейнера** (одежда/рюкзак/жилет/
протектор-кейс), либо в руках (`InventoryLocationType.HANDS`), либо приаттачены. «Голого
инвентаря пешки» (свободной сетки вне контейнеров) не существует.

**Импликация для мода:** в `dmRequirements.ComputeUsedRatio` (dmRequirements.c) первый вызов
`AccumulateCargo(inv.GetCargo(), total, used)` — «главный cargo» — всегда no-op для пешки
(`GetCargo()==null`, `AccumulateCargo` корректно игнорирует null). Занятая доля
(`m_UsedRatio`) целиком складывается из карго надетых контейнеров (второй цикл по слотам).
Это не баг (null обрабатывается), но комментарий «Главный cargo (сам инвентарь)» вводит в
заблуждение — его можно убрать/уточнить. `[нужно подтвердить]` — см. п.6.1.

---

## 5. Точный алгоритм «собрать дропаемые карго-предметы»

### Вариант A — нативный `EnumerateInventory` + фильтр (рекомендую для простоты)

| Сигнатура | file:line |
|---|---|
| `proto native bool EnumerateInventory(InventoryTraversalType tt, out array<EntityAI> items)` | `inventory.c:127` |
| `enum InventoryTraversalType { PREORDER, INORDER, POSTORDER, LEVELORDER }` | `gameplay.c:5-10` |

`EnumerateInventory` **нативно** обходит всё дерево инвентаря (аттачи + их вложенное карго,
рекурсивно), НЕ включая саму корневую сущность. Канонические ванильные использования:
`PlayerBase.RemoveAllItems` (PREORDER, `playerbase.c:1590`), `ClearInventory` (INORDER,
`playerbase.c:6658`), `DropAllItems` (LEVELORDER, `playerbase.c:6683`). Expansion для
клонирования — тот же приём (`miscgameplayfunctions.c:131/349`, `ExpansionItemSpawnHelper`).

```c
void CollectDropableCargo(PlayerBase pawn, out array<EntityAI> out)
{
    array<EntityAI> items = new array<EntityAI>();
    pawn.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);
    for (int i = 0; i < items.Count(); i++)
    {
        InventoryLocation loc = new InventoryLocation();
        if (items[i].GetInventory().GetCurrentInventoryLocation(loc) &&
            loc.GetType() == InventoryLocationType.CARGO)      // или PROXYCARGO
        {
            out.Insert(items[i]);   // дропаемое карго
        }
    }
    // предмет в руках — отдельно (EnumerateInventory его НЕ возвращает, см. п.5-прим.)
    // ItemBase hands = pawn.GetHumanInventory().GetEntityInHands();
}
```

### Вариант B — ручная рекурсия (точное соответствие модели задачи)

```c
// Собрать дропаемые карго-предметы (включая вложенные), аттачи НЕ входят.
void CollectDropable(EntityAI ent, out array<EntityAI> out)
{
    GameInventory inv = ent.GetInventory();
    if (!inv) return;

    // 1) КАРГО сущности = дропаемое. Рекурсивно спускаемся в карго-контейнеры.
    CargoBase cargo = inv.GetCargo();
    if (cargo)
    {
        for (int i = 0; i < cargo.GetItemCount(); i++)
        {
            EntityAI item = cargo.GetItem(i);
            out.Insert(item);          // это дропаемое (лежит в карго)
            CollectDropable(item, out); // а если item сам контейнер — его карго тоже дропаемое
        }
    }

    // 2) СЛОТЫ = аттачи (магазин/оптика/GPS/мачете) — САМ аттач НЕ дропаем,
    //    но спускаемся внутрь него (у аттача может быть своё карго/слоты).
    for (int i = 0; i < inv.GetAttachmentSlotsCount(); i++)
    {
        EntityAI att = inv.FindAttachment(inv.GetAttachmentSlotId(i));
        if (att)
            CollectDropable(att, out);
    }
}

// Корень: CollectDropable(pawn, out); + отдельно руки (см. ниже).
```

**Примечание про HANDS.** `EnumerateInventory` НЕ возвращает предмет в руках: ваниль
`ClearInventory` (playerbase.c:6658) перечисляет инвентарь и **отдельно** обрабатывает
`GetItemInHands()` через `LocalDestroyEntityInHands()` (playerbase.c:6677). Ручная рекурсия
по слотам тоже не заденет руки (руки — это не слот). Если «дропаемое» должно включать оружие
в руках — добавить `pawn.GetHumanInventory().GetEntityInHands()` (`man.c:81`, native;
`HumanInventory.GetEntityInHands` — `humaninventory.c:16`). `[нужно проверить]` — см. п.6.2.

**Итог по модели:** `КАТЕГОРИЯ=КАРГО` и `СЛОТ=АТТАЧ` разделяются структурно (вариант B) либо
по `GetType()` (вариант A). Обе дают одинаковый результат.

---

## 6. AmmoBox (классификация «аммо»)

### 6.1 Что такое AmmoBox по классам

- **Запечатанные коробки** `AmmoBox_556x45_20Rnd` … `AmmoBox_9x39_20Rnd` — все
  `class AmmoBox_* : Box_Base {}` (`4_world/entities/itembase/ammoboxes.c`).
- `class Box_Base : InventoryItemSuper` (`4_world/entities/core/inherited/inventoryitem.c:1002`),
  регистрирует `AddAction(ActionUnpackBox)` (`inventoryitem.c:1007`).
- **Отдельно** есть переиспользуемый пустой контейнер `class AmmoBox: Container_Base`
  (`DZ/gear/containers/config.cpp:1003`) с `itemsCargoSize[]={5,4}` — это НАСТОЯЩИЙ контейнер
  (есть карго), не путать с запечатанными `AmmoBox_*_20Rnd` (те `Box_Base`, карго нет).

### 6.2 Как `dmLoot.GetCategory` классифицирует AmmoBox сейчас

`dmLoot.GetCategory` (dmLoot.c:57-90): `Edible_Base`→FOOD, `IsWeapon()`→WEAPON,
`IsMelee`→MELEE, `IsMagazine()`→MAGAZINE, `IsAmmoPile()`→AMMO, `IsClothing()`→CLOTHING,
рем-киты→REPAIR, `BandageDressing`→MEDICAL, иначе→OTHER.

`AmmoBox_*_20Rnd` (`Box_Base`) **не подходит ни под одну ветку** → попадает в **OTHER**.
Это пробел классификации: коробку патронов логично считать AMMO (или отдельной под-категорией),
потому что `IsMagazine()`/`IsAmmoPile()` для неё `false` (она не `Magazine` и не
`Ammunition_Base`).

### 6.3 Калибр/тип патронов внутри AmmoBox

Запечатанная коробка хранит «что внутри» **в конфиге**, поле `Resources`:

```cpp
// DZ/weapons/ammunition/config.cpp:2358 (AmmoBox_556x45_20Rnd)
class AmmoBox_556x45_20Rnd: Box_Base {
    ...
    class Resources {
        class Ammo_556x45 {   // <-- имя россыпной пачки (CfgMagazines)
            value=20;         // <-- сколько патронов
            variable="quantity";
        };
    };
};
```

Т.е. **имя калибра = имя ребёнка `Resources`, а не префикс имени класса.** Прочитать программно:

```c
// конфиг-нативы на g_Game:
//   proto native int ConfigGetChildrenCount(string path)         — game.c:610
//   proto bool ConfigGetChildName(string path, int idx, out string) — game.c:593
//   proto native int ConfigGetInt(string path)                   — game.c:538
string type = item.GetType();                      // "AmmoBox_762x54_20Rnd"
string resPath = "CfgVehicles " + type + " Resources";
string pileName;
if (g_Game.ConfigIsExisting(resPath) &&
    g_Game.ConfigGetChildrenCount(resPath) > 0 &&
    g_Game.ConfigGetChildName(resPath, 0, pileName))
{
    int count = g_Game.ConfigGetInt(resPath + " " + pileName + " value");
    string bulletType;
    if (AmmoTypesAPI.MagazineTypeToAmmoType(pileName, bulletType))  // ammotypes.c:14
    {
        // pileName = "Ammo_762x54" (CfgMagazines), bulletType = "Bullet_762x54" (CfgAmmo),
        // count = 20
    }
}
```

`AmmoTypesAPI.MagazineTypeToAmmoType` — `3_game/global/ammotypes.c:14` (конвертация
`CfgMagazines → CfgAmmo`; уже задокументирована в `loot.md`). Префикс имени класса
(`AmmoBox_762x54_...` → `"762x54"`) — запасной, хрупкий вариант; конфиг — канонический.

### 6.4 Как «распаковать» коробку (ванильный путь)

Ванильный action — **`ActionUnpackBox`** (`4_world/classes/useractionscomponent/actions/
continuous/actionunpackbox.c`), НЕ «ActionOpenAmmoBox». Ключевой метод
`OnFinishProgressServer` (actionunpackbox.c:32-64):

1. Читает `CfgVehicles <type> Resources`, берёт ПЕРВОГО ребёнка с `value > 0`.
2. `itemType = resources.Get(0)`, `itemCount = ConfigGetInt(... value)`.
3. `player.ServerReplaceItemInHandsWithNew(new UnboxLambda(box, itemType, player, itemCount))`.
4. `UnboxLambda.CopyOldPropertiesToNew` (actionunpackbox.c:76-94): если `itemType` есть в
   `CfgMagazines` → `Magazine pile = new_item; pile.ServerSetAmmoCount(m_ItemCount);`; иначе
   `ItemBase.SetQuantity(count)`. Плюс спавнит бумагу (`CreateObjectEx("Paper", ...)`).

`ServerReplaceItemInHandsWithNew(ReplaceItemWithNewLambdaBase)` — `3_game/entities/man.c:340`.
`Magazine.ServerSetAmmoCount(int)` / `ServerStoreCartridge` — нативы `Magazine` (эталон
клонирования — `ExpansionItemSpawnHelper.Clone`, ExpansionItemSpawnHelper.c:725-738).

**Для бота (нет action-manager) — ручная распаковка** (гипотеза, см. п.6.1):
```c
// 1) прочитать Resources → pileName + count (см. 6.3)
// 2) создать пачку: CreateObjectEx(pileName, groundPos, ECE_OBJECT_SWAP)  (или в карго)
// 3) Magazine pile; pile.ServerSetAmmoCount(count);
// 4) удалить коробку: ObjectDelete(box) / LocalDestroyEntity
```
`[нужно проверить]` — достаточен ли этот путь (без `ServerReplaceItemInHandsWithNew`, который
работает только когда коробка в руках), и нужен ли спавн «бумаги» для валидности мира.

---

## 7. Открытые вопросы / `[нужно проверить]`

1. **[нужно подтвердить]** `pawn.GetInventory().GetCargo() == null` в рантайме. Конфиг говорит
   «нет `itemsCargoSize`» — но теоретически C++-класс `Man` мог бы дать дефолтное карго.
   Probe: `botdump` бота → лог `GetCargo()` пешки (ожидаем `null`).
2. ~~[нужно проверить]~~ **ПОДТВЕРЖДЕНО (E2E, v3.374): `EnumerateInventory(LEVELORDER)`
   ВОЗВРАЩАЕТ предмет в руках** (`raw[2]=Mosin9130 loc=HANDS`). Итог: руки НЕ «косвенно НЕТ»,
   а валидная запись с `loc=HANDS` (в отличие от карго/вложенных аттачей — те `null`).
3. **[нужно проверить]** ручная распаковка `AmmoBox_*` (создать `Ammo_*` пачку +
   `ServerSetAmmoCount` + удалить коробку) вместо `ActionUnpackBox`/`ServerReplaceItemInHandsWithNew`
   — работает ли для ИИ-бота (коробка в карго/на полу, не в руках). Probe: спавн `AmmoBox_762x54_20Rnd`
   в карго бота → ручная распаковка → `botdump`: пачка `Ammo_762x54` с `GetAmmoCount()==20`, коробки нет.
4. **[нужно проверить]** классификация `AmmoBox_*` → AMMO: `IsInherited(Box_Base)` ловит и
   `NailBox` (тоже `Box_Base`, `consumables.c`). Нужен Resources-дискриминатор (ребёнок
   `Resources` — это `CfgMagazines`-пачка → аммо, иначе — нет). Probe: `spawnobj` NailBox и
   AmmoBox → лог `Resources` первого ребёнка у каждого.
5. **[нужно проверить]** `GetCurrentInventoryLocation().GetType()` на предмете в ПРОКСИ-карго
   (постройка) — вернёт ли `PROXYCARGO`, и надо ли его включать в «дропаемое» для бота.
   Для текущего скоупа (контейнеры в инвентаре) PROXYCARGO не встречается — вопрос теоретический.

---

## Существенные развилки и принятые решения

- **Развилка:** перечислять всё плоским `EnumerateInventory` + фильтр по локации (A) или
  ручной рекурсией по слотам/карго (B). **Решение (по E2E-эмпирике `enumdump`, v3.374): B.**
  Причина — **управляемость и понятность**, НЕ поломка A: `EnumerateInventory` работает (на
  нём строится ваниль/моды), но семантика нативного обхода нетривиальна (`LEVELORDER` в пробе
  давал `null`-записи для вложенного — тонкость обхода). Явная рекурсия B — структурная модель
  «карго vs слот» без чтения локации и без сюрпризов нативного traversal. Если нужна краткость
  A — разобраться с её семантикой отдельно (INORDER vs LEVELORDER, null-записи).
- **Развилка:** считать ли `GetCategory(AmmoBox_*)` пробелом (OTHER) и чинить через `Box_Base`
  или через Resources-дискриминатор. **Решение:** калибр/тип всегда брать из конфига `Resources`
  (не из префикса имени класса); классификацию поднять как AMMO через «`IsInherited(Box_Base)` +
  Resources-ребёнок есть в `CfgMagazines`». Почему: имя класса — не контракт, конфиг — контракт
  (`ActionUnpackBox` сам читает `Resources`). Действует пока: не появится коробка с несколькими
  типами патронов (`resources.Get(0)` в ванили тоже берёт только первый).

## Источники (file:line)

- Ваниль `DayZ Projects/scripts/`:
  - `3_game/systems/inventory/inventory.c` — `GetAttachmentSlotsCount:184`, `GetAttachmentSlotId:180`,
    `FindAttachment:224`, `FindAttachmentByName:228`, `AttachmentCount:205`,
    `GetAttachmentFromIndex:220`, `HasAttachmentSlot:190`, `GetCargo:138`, `GetCargoFromIndex:139`,
    `EnumerateInventory:127`, `GetCurrentInventoryLocation:268`, `IsInCargo:397`, `IsAttachment:410`,
    `HasEntityInInventory:118`.
  - `3_game/systems/inventory/cargo.c` — `GetCargoOwner:12`, `GetItemCount:28`, `GetItem:32`,
    `GetWidth:37`, `GetHeight:41`, `GetItemSize:51`, `FindEntityInCargo:56`.
  - `3_game/systems/inventory/inventorylocation.c` — `enum InventoryLocationType:3-12`, `GetType:43`,
    `GetParent:52`, `GetSlot:71`, `GetIdx:77`, `GetRow:83`, `GetCol:89`.
  - `3_game/gameplay.c` — `enum InventoryTraversalType:5-10`.
  - `3_game/entities/entityai.c` — `HasAnyCargo:677`, `GetInventory:1834`, `FindAttachmentBySlotName:1847`,
    `CanDisplayCargo:1786`, `SetInvisibleRecursive:4021-4050` (рекурсия по слотам), `IsEmpty:661`.
  - `3_game/entities/man.c` — `GetHumanInventory:79`, `GetEntityInHands:81`,
    `ServerReplaceItemInHandsWithNew:340`.
  - `3_game/global/game.c` — `ConfigGetInt:538`, `ConfigGetChildName:593`, `ConfigGetChildrenCount:610`,
    `ConfigIsExisting:611`.
  - `4_world/entities/manbase/playerbase.c` — `RemoveAllItems:1590`, `ClearInventory:6658`,
    `DropAllItems:6683`.
  - `4_world/entities/core/inherited/inventoryitem.c` — `Box_Base:1002`, `AddAction(ActionUnpackBox):1007`.
  - `4_world/entities/itembase/ammoboxes.c` — все `AmmoBox_* : Box_Base`.
  - `4_world/classes/useractionscomponent/actions/continuous/actionunpackbox.c` — `OnFinishProgressServer:32-64`,
    `UnboxLambda.CopyOldPropertiesToNew:76-94`.
  - `4_world/static/miscgameplayfunctions.c` — `EnumerateInventory(LEVELORDER)` обход:131/349.
  - `3_game/global/ammotypes.c` — `MagazineTypeToAmmoType:14` (см. также `loot.md`).
- Ваниль `DayZ Projects/DZ/`:
  - `characters/data/config.cpp:122` — `SurvivorBase: Man` без `itemsCargoSize` (ответ «нет главного карго»).
  - `weapons/ammunition/config.cpp:2358+` — `AmmoBox_*_20Rnd` с `class Resources { class Ammo_* { value=.. } }`.
  - `gear/containers/config.cpp:1003` — `class AmmoBox: Container_Base` (`itemsCargoSize[]={5,4}`).
- Expansion: `DayZExpansion/Core/Scripts/4_World/DayZExpansion_Core/Static/ExpansionItemSpawnHelper.c:642-677`
  (рекурсивный обход аттачей+карго+рук при клонировании).
- Мод botorama: `src/core/4_World/Entities/Bot/Loot/dmRequirements.c` (`ComputeUsedRatio`,
  `AccumulateCargo`), `src/core/4_World/Entities/Bot/Loot/dmLoot.c` (`GetCategory:57-90`).
