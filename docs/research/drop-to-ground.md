# Research: сброс предмета ИИ на пол БЕЗ clone+delete (локальный мув)

Статус: исследование API. Ведёт субагент `dayz-research`.
Дополняет `docs/research/body-drop.md` (дроп из рук — clone+delete) и
`docs/research/loot.md` (ре-синк `RemoteObjectTreeDelete/Create`, готчи «краш 1/2»).
Домен: **репак инвентаря** — «выложить предмет на пол → взять обратно», где предмет
должен ОСТАТЬСЯ тем же `EntityAI`, чтобы re-collect / `Ignore(item)` продолжали
работать по одной и той же ссылке.

## Цель

Установить, существует ли у Expansion/ванили «локальный мув» (сущность та же,
удаляется/создаётся только сетевое представление), точные сигнатуры, и чем он
отличается от clone+delete, который сейчас делает `dmAISurvivorBase.DropItemToGround`
(`dmAISurvivorBase.c:2470-2505`).

## Вывод одной строкой

**Да, локальный мув существует, он ванильный, сущность остаётся ТОЙ ЖЕ, карго
освобождается синхронно.** Это ровно то, что Expansion делает в
`eAI_TakeItemToLocation` (`eAIBase.c:10605-10673`), и ровно то, что уже портировано
в `dmLoot.c` (`TakeIntoCargo`/`TakeIntoDestination`/`TakeToHands`/`TakeToAttachmentSlot`,
`dmLoot.c:537/554/585/610`). Для репака (CARGO→GROUND→CARGO) clone+delete **не нужен и
вреден** — он меняет сущность и ломает `Ignore`/re-collect по ссылке.

---

## 1. Локальный мув у Expansion (file:line)

`eAI_TakeItemToLocation` (`eAIBase.c:10605-10673`) — **не-клон**-вариант, сущность та же:

```c
bool eAI_TakeItemToLocation(ItemBase item, InventoryLocation il_dst)
{
    InventoryLocation il_src = new InventoryLocation();
    item.GetInventory().GetCurrentInventoryLocation(il_src);          // :10609
    if (il_src.CompareLocationOnly(il_dst) && il_src.GetFlip() == il_dst.GetFlip())
        return false;                                                  // :10612
    ...
    if (g_Game.IsMultiplayer())
        g_Game.RemoteObjectTreeDelete(item);                           // :10633
    bool result = LocalTakeToDst(il_src, il_dst);                      // :10637  <-- САМ МУВ, тот же item
    ...
    if (g_Game.IsMultiplayer())
        g_Game.RemoteObjectTreeCreate(item);                           // :10669
    return result;
}
```

Ключевое доказательство «сущность та же»: **после** `LocalTakeToDst` Expansion
продолжает использовать тот же `item` — `ExpansionItemBaseModule.SetLootable(item, false)`
и `item.SetLifetimeMax(120)` (`eAIBase.c:10659-10661`). Никакого `ObjectDelete`/клона нет.

`Expansion_CloneItemToLocation` (`eAIBase.c:6000-6075`) — **клон**-вариант (для HANDS):
`ExpansionItemSpawnHelper.Clone(src, ...)` (`:6017`) + `g_Game.ObjectDelete(src)` (`:6042`).
Куда какой применяется — `eAI_DropItemImpl` (`eAIBase.c:10237-10287`):

```c
if (item != GetItemInHands())
    eAI_TakeItemToLocation(item, il_dst);          // :10256 — НЕ руки → локальный мув
else
    EntityAI dst = Expansion_CloneItemToLocation(item, il_dst);  // :10268 — руки → clone+delete
```

**Почему для HANDS клонируют:** `LocalTakeToDst` из HANDS уходит в deferred `HandEvent`
(см. body-drop.md) → re-sync `RemoteObjectTreeCreate` срабатывает раньше фактического
перемещения. Для CARGO/ATTACHMENT→GROUND `LocalTakeToDst` синхронен, поэтому клон не нужен.

**Точка сброса на пол у Expansion:** в `eAI_TakeItemToLocation` `il_dst` приходит готовым
(его строит вызывающий через `GameInventory.SetGroundPosByOwner` — тот же вызов, что в
`dmLoot`/`DropItemToGround`). Аналога `SetGroundPosByTransform` Expansion НЕ использует;
они пользуются дефолтным `SetGroundPosByOwner` (авто-точка у владельца).

---

## 2. Точные сигнатуры (ваниль, file:line)

| Сигнатура | Файл:строка | Примечание |
|---|---|---|
| `static bool GameInventory.SetGroundPosByOwner(EntityAI owner, notnull EntityAI item, out InventoryLocation ground)` | `3_game/systems/inventory/inventory.c:1257` | `PrepareDropEntityPos(owner,item,m4,false,2)` + `ground.SetGround(item,m4)` |
| `static bool GameInventory.SetGroundPosByTransform(EntityAI owner, notnull EntityAI item, out InventoryLocation ground, vector transform[4])` | `inventory.c:1286` | `PrepareDropEntityPos(owner,item,transform,true,2)` + `ground.SetGround(item,transform)` |
| `static proto native bool GameInventory.PrepareDropEntityPos(EntityAI owner, notnull EntityAI item, out vector mat[4], bool useValuesInMatrix = false, int conflictCheckDepth = -1)` | `inventory.c:615` | `useValuesInMatrix=true` → берёт матрицу как есть (снап при конфликте); `false` → генерит у владельца |
| `proto native void InventoryLocation.SetGround(EntityAI e, vector mat[4])` | `3_game/systems/inventory/inventorylocation.c:115` | dst = GROUND, parent=null, матрица = mat |
| `bool EntityAI.LocalTakeToDst(notnull InventoryLocation src, notnull InventoryLocation dst)` | `3_game/entities/entityai.c:2055` | `return GetInventory().TakeToDst(InventoryMode.LOCAL, src, dst);` |
| `override bool Man.LocalTakeToDst(notnull InventoryLocation src, notnull InventoryLocation dst)` | `3_game/entities/man.c:858` | → `TakeToDstImpl(LOCAL,...)` → `GetHumanInventory().TakeToDst(LOCAL,...)` + `UpdateInventoryMenu()` |
| `bool GameInventory.TakeToDst(InventoryMode mode, notnull InventoryLocation src, notnull InventoryLocation dst)` | `inventory.c:1056` | `case LOCAL:` → `LocationSyncMoveEntity(src,dst)` (`:1066-1068`) |
| `static proto native bool GameInventory.LocationSyncMoveEntity(notnull InventoryLocation src_loc, notnull InventoryLocation dst_loc)` | `inventory.c:567` | **«synchronously removes item from current inventory location and adds it to destination»** |
| `bool HumanInventory.TakeToDst(InventoryMode mode, notnull InventoryLocation src, notnull InventoryLocation dst)` | `3_game/systems/inventory/humaninventory.c:243-251` | `RedirectToHandEvent` если src/dst==HANDS, иначе `super.TakeToDst` |
| `bool HumanInventory.RedirectToHandEvent(mode, src, dst)` | `humaninventory.c:200-241` | true только при src==HANDS (→`HandEventMoveTo`) или dst==HANDS (→`HandEventTake`) |
| `bool HumanInventory.DropEntity(mode, owner, item)` | `humaninventory.c:139-162` | HANDS → `HandEvent(LOCAL, HandEventDrop)`; `default:` → `super.DropEntity` (синхронно) |
| `override bool Man.LocalDropEntity(notnull EntityAI item)` | `man.c:140` | → `DropEntityImpl(InventoryMode.LOCAL, this, item)` |
| `proto native void Game.RemoteObjectTreeDelete(Object obj)` | `3_game/global/game.c:707` | «deletes only remote object tree (unregisters from network)» — **НЕ удаляет EntityAI** |
| `proto native void Game.RemoteObjectTreeCreate(Object obj)` | `game.c:709` | «postponed registration of network object tree» — **НЕ создаёт EntityAI** |
| `proto native void Game.ObjectDelete(Object obj)` | `game.c:704` | полный сетевой delete (это и есть clone+delete-путь) |
| `proto native Object Game.CreateObjectEx(string type, vector pos, int iFlags, int iRotation = RF_DEFAULT)` | `game.c:702` | создание НОВОЙ сущности (clone+delete) |
| `const int GameConstants.INVENTORY_ENTITY_DROP_OVERLAP_DEPTH = 2` | `3_game/constants.c:1057` | глубина поиска свободной точки дропа |
| `static void MiscGameplayFunctions.TransferItemProperties(source, target, transfer_agents=true, transfer_variables=true, transfer_health=true, exclude_quantity=false)` | `4_world/static/miscgameplayfunctions.c:269` | только для clone+delete |
| `static TransferInventoryResult MiscGameplayFunctions.TransferInventory(source, target, player)` | `miscgameplayfunctions.c:344` | только для clone+delete |

### Разница `LocalTakeToDst` vs `TakeToDst`

- `TakeToDst(InventoryMode, src, dst)` — общий (GameInventory): `SERVER` → `LocationSyncMoveEntity` +
  `InventoryInputUserData.SendServerMove` (`inventory.c:1061-1064`); `LOCAL` → только `LocationSyncMoveEntity`
  (`:1066-1068`). **Для AI_SERVER `SERVER`-режим опасен** (см. loot.md «краш 2»: `SendServerMove`/deferred
  ломается у серверного ИИ), поэтому используют **LOCAL**.
- `LocalTakeToDst(src, dst)` — обёртка = `TakeToDst(InventoryMode.LOCAL, src, dst)`. Два перегруза:
  - на `EntityAI` (`entityai.c:2055`): `GetInventory().TakeToDst(LOCAL,...)` — **напрямую** в GameInventory,
    минуя HumanInventory. Для не-рук предмета это самый «чистый» путь.
  - на `Man` (`man.c:858`): идёт через `HumanInventory.TakeToDst` → `RedirectToHandEvent` (делегирует
    в `HandEvent` **только** если src/dst==HANDS; иначе `super.TakeToDst` → тот же `LocationSyncMoveEntity`).

**Вывод для репака:** для CARGO/ATTACHMENT→GROUND оба перегруза синхронны и не трогают hand-FSM.
Различие появляется только при HANDS — тогда `Man.LocalTakeToDst` уйдёт в deferred `HandEventMoveTo`
(готча body-drop.md), а `EntityAI.LocalTakeToDst` (вызов на **предмете**) всё равно двинет через
GameInventory, но «рука» слотом рук останется занятой на сервере — поэтому из HANDS так делать нельзя.

---

## 3. Критично подтверждено

### 3.1 Сущность та же и валидна после delete→move→create? — ДА (чтение кода)

- `RemoteObjectTreeDelete`/`Create` — «deletes only remote object tree (unregisters from network)» /
  «postponed registration of network object tree» (`game.c:707/709`). Ванильный комментарий в
  `replaceitemwithnewlambdabase.c:117`: «@NOTE: this does not delete the object on server, only it's
  network representation». `EntityAI`-объект на сервере НЕ удаляется.
- `LocalTakeToDst` → `LocationSyncMoveEntity` — «synchronously removes item from current inventory
  location and adds it to destination» (`inventory.c:567`). Ни create, ни delete EntityAI.
- Доказательство из ванили: `DropEquipAndDestroyRootLambda.CopyOldPropertiesToNew`
  (`miscgameplayfunctions.c:130-156`) — каждый child переносится на пол ровно так:
  `m_Player.LocalTakeToDst(child_src, child_dst)` + `g_Game.RemoteObjectTreeCreate(child)`
  (`:152/:154`) — child-сущность **не пересоздаётся**.
- Доказательство из Expansion: `eAI_TakeItemToLocation` повторно использует `item` после мува
  (`SetLootable`/`SetLifetimeMax`, `eAIBase.c:10659-10661`).

Итог: ссылка на `item` остаётся валидной, `IsSetForDeletion()==false`, `GetHierarchyParent()==null`
(на земле) — **в следующем фрейме можно взять обратно по той же ссылке**.

### 3.2 Карго освобождается синхронно? — ДА, но обёртка `RemoteObjectTreeDelete/Create` ОБЯЗАТЕЛЬНА (УТОЧНЕНО E2E v3.398)

> **ПОСТ-МОРТЕМ v3.391→3.392:** связка `RemoteObjectTreeDelete(item)` → `LocalTakeToDst(src, GROUND)` →
> `RemoteObjectTreeCreate(item)` НЕ освобождала слот карго (20 дропов `ok=true`, но `FindDestination`
> видел «нет места»). **Но виноват был `LocalTakeToDst` НА ЗЕМЛЮ, а не обёртка**: для направления «на
> землю» правильный примитив — `LocalDropEntity` (`SetGroundPosByOwner` + `TakeToDst(LOCAL)`), а не
> `LocalTakeToDst`.

> **УТОЧНЕНИЕ v3.397→3.398 (прогон `wearprobe`/`dropprobe` + клиентский `slotdump`/`scanobj`):**
> обёртка `RemoteObjectTreeDelete`/`Create` **обязательна для синка клиенту**. Голый `LocalDropEntity`
> без обёртки слот на сервере освобождает, но клиент НЕ видит изменения (для ATTACHMENT→GROUND предмет
> остаётся «надетым» на клиенте). Рабочий рецепт — обёртка + правильный примитив мува:
> `RemoteObjectTreeDelete(item)` → `LocalDropEntity(item)` → `RemoteObjectTreeCreate(item)`.
> Полный рецепт и таблица вариантов — `docs/research/item-move-sync.md`.

### 3.3 `SetGroundPosByTransform` vs `SetGroundPosByOwner`

Оба static на `GameInventory`, оба сводятся к `PrepareDropEntityPos(...)` + `ground.SetGround(item, mat)`.
Различие — только 4-й аргумент `useValuesInMatrix`:

- `SetGroundPosByOwner` (`:1257`) → `PrepareDropEntityPos(owner,item,m4, false, 2)` — матрица
  **генерится** «рядом с владельцем» (направление/расстояние из конфига дропа), `m4[3]` не важен.
- `SetGroundPosByTransform` (`:1286`) → `PrepareDropEntityPos(owner,item,transform, true, 2)` —
  матрица **берётся как есть**. Требуемый transform: `Math3D.MatrixIdentity4(t); t[3] = pos;`
  (identity-поворот + позиция). Снап при конфликте («попало внутрь машины → сдвинуть к краю»)
  сохраняется — `PrepareDropEntityPos` с `conflictCheckDepth=2` игнорирует/подправляет матрицу при
  пересечении (`inventory.c:607-615`).

Эквивалентны по результату; `SetGroundPosByTransform` даёт явный контроль над точкой. Для репака
(предмет вернётся в карго) конкретная точка на полу несущественна — можно `SetGroundPosByOwner`
(проще, уже используется модом).

---

## 4. Рецепт для репака (ИСПРАВЛЕН дважды — финальный: обёртка + LocalDropEntity)

> **Финальный рецепт (v3.398):** обёртка `RemoteObjectTreeDelete/Create` обязательна (иначе клиент не
> видит дроп), а примитив на землю — `LocalDropEntity` (не `LocalTakeToDst`, тот не освобождает слот).
> См. `docs/research/item-move-sync.md`.

```c
// «выложить на пол» (CARGO/ATTACHMENT → GROUND), сущность та же:
GetGame().RemoteObjectTreeDelete(item);
bool ok = LocalDropEntity(item);    // SetGroundPosByOwner + TakeToDst(LOCAL), внутри обёртки
GetGame().RemoteObjectTreeCreate(item);

// «взять обратно» — тот же item, та же ссылка (dmLoot.TakeIntoCargo):
GetGame().RemoteObjectTreeDelete(item);
ok = pawn.LocalTakeToDst(src, dst);   // GROUND → CARGO
GetGame().RemoteObjectTreeCreate(item);
```

Ограничения/условия применимости:
1. **Только для не-рук предметов.** Если `src.GetType()==HANDS`, `LocalTakeToDst` (через
   `Man`) уйдёт в deferred `HandEvent` — репак по рукам НЕ так (для рук — clone+delete, либо
   сначала переложить из руки в карго штатным путём, затем репакнуть из карго).
2. `SetGroundPosByOwner`/`ByTransform` возвращают `bool` (валидная точка дропа?) — при `false`
   падать не нужно.
3. На offline/SP (`!IsMultiplayer()`) Expansion пропускает `RemoteObjectTreeDelete/Create` — но
   вызывать их в SP безопасно (no-op по сути); держать гейт `if (g_Game.IsMultiplayer())` по эталону
   `eAIBase.c:10633/10669`.

---

## 5. `[нужно проверить]` / `[нужно подтвердить]` — эмпирика

1. **[ПОДТВЕРЖДЕНО] сущность та же в следующем фрейме.** E2E фаза 1+2: `LocalDropEntity` оставляет
   сущность той же (`IsSetForDeletion()==false`, `GetHierarchyRootPlayer()==null` на земле), re-collect
   по той же ссылке работает.
2. **[ПОДТВЕРЖДЕНО, с поправкой] синхронное освобождение слота.** Обёртка `RemoteObjectTreeDelete/Create`
   НЕ освобождала слот (вернулся 1 из 20 патронов, консерва не влезла); ванильный `LocalDropEntity`
   (без обёртки) освобождает слот немедленно — все патроны вернулись, консерва легла. См. §3.2.
3. **[ПОДТВЕРЖДЕНО] клиент видит дроп.** E2E фаза 2 (клиент-наблюдатель): клиентский `scanobj` видит
   выложенные предметы на земле как `onGround=true`, картина совпадает с серверным `scanbox` — `LocalDropEntity`
   синкает дроп клиенту БЕЗ `RemoteObjectTreeDelete/Create`. (Прежняя гипотеза «клиент синкается только
   через RemoteObjectTreeDelete/Create» — неверна для ванильного дропа.)
4. **[нужно проверить] физика дропа.** `LocationSyncMoveEntity` в GROUND ставит предмет на матрицу
   без `CreateDynamicPhysics`/импульса — предмет «телепортируется» на землю, а не падает с velocity.
   Для репака (предмет тут же возвращается) это неважно; если заказчику нужен видимый «падение» —
   отдельная физика (см. body-drop.md §2). Probe: `scanbox` ловит предмет сразу в точке дропа
   (без фазы падения).
5. **[нужно подтвердить]** `Man.LocalTakeToDst` вызывает `UpdateInventoryMenu()` (`man.c:862`) —
   безопасен ли он для `INSTANCETYPE_AI_SERVER` (не трогает ли клиентское UI/не падает). Эмпирика:
   отсутствие ошибок/`#return` в serverLog при серии мувов; альтернатива — звать
   `item.LocalTakeToDst` (EntityAI-перегруз, минует HumanInventory+UpdateInventoryMenu).

---

## Существенные развилки и принятые решения

- **Развилка:** репак через clone+delete (`DropItemToGround`) vs локальный мув
  (`RemoteObjectTreeDelete` + `LocalTakeToDst` + `RemoteObjectTreeCreate`).
  **Решение:** локальный мув. Почему: clone+delete меняет `EntityAI` — `Ignore(item)`/re-collect
  закешированы по ссылке на удалённый оригинал и перестают работать (уже задокументировано в
  `decisions.md` «body: дроп из рук — clone+delete», следствие «клон не игнорится»). Локальный мув
  — ванильный, синхронный, сущность та же; он уже портирован в `dmLoot` и используется в проде.
  Выбор влияет на корректность re-collect/ignore в цикле репака.
- **Развилка:** звать локальный мув через `player.LocalTakeToDst` (Man) или `item.LocalTakeToDst`
  (EntityAI). **Решение:** для не-рук оба эквивалентны (доходят до `LocationSyncMoveEntity`); эталон
  (Expansion `eAIBase.c:10637`, ваниль `miscgameplayfunctions.c:152`, мод `dmLoot`) зовёт на Man.
  Почему: единообразие с существующим `dmLoot`; различие критично только для HANDS (Man → deferred),
  где репак не должен работать. Действует пока: репакуются только не-руки предметы.

## Источники (файл:строка)

- Ваниль: `3_game/systems/inventory/inventory.c:567/615/1056-1068/1257/1286`;
  `3_game/systems/inventory/inventorylocation.c:115`;
  `3_game/systems/inventory/humaninventory.c:139-162/200-241/243-251`;
  `3_game/systems/inventory/replaceitemwithnewlambdabase.c:117/240-280`;
  `3_game/entities/entityai.c:2055`; `3_game/entities/man.c:140/858-866`;
  `3_game/global/game.c:702/704/707/709`; `3_game/constants.c:1057`;
  `4_world/static/miscgameplayfunctions.c:130-156/269/344`.
- Expansion: `eAIBase.c:6000-6075/10237-10287/10605-10673` (локальный мув — `:10605`);
  `ExpansionItemSpawnHelper.c:594-765` (clone).
- Мод botorama: `dmAISurvivorBase.c:2470-2505` (clone+delete, НЕ для репака);
  `dmLoot.c:537/554/585/610` (локальный мув, уже в проде).
- Кросс-ссылки: `docs/research/loot.md` (готчи краш 1/2, ре-синк),
  `docs/research/body-drop.md` (HANDS deferred, clone+delete), `docs/decisions.md`
  («body: дроп из рук — clone+delete»).
