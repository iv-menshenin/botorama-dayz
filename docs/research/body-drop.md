# Research: дроп предмета из рук ИИ-бота (клиент видит)

Статус: исследование API. Ведёт субагент `dayz-research`.
Дополняет `docs/research/body.md` (нокаут/смерть) и `docs/research/loot.md`
(готчи «краш 1/2», ре-синк `RemoteObjectTreeDelete/Create`).

## Цель

Понять, как КОРРЕКТНО дропнуть предмет из рук `dmAISurvivorBase`
(`INSTANCETYPE_AI_SERVER`) так, чтобы **клиент это увидел**: рука пустеет, предмет
падает на землю. Сейчас `DropItem` (`dmAISurvivorBase.c:2153`) двигает предмет только
на сервере (лог `DropItem result=true`, `GetItemInHands()==null`), а клиент продолжает
видеть оружие в руке.

---

## Корневая причина рассинхрона

Текущий `DropItem`:

```c
override bool DropItem(ItemBase item)   // dmAISurvivorBase.c:2153
{
    GetGame().RemoteObjectTreeDelete(item);   // (A)
    bool ok = LocalDropEntity(item);          // (B)
    GetGame().RemoteObjectTreeCreate(item);   // (C)
    ...
}
```

`LocalDropEntity(item)` для предмета **в руках** НЕ двигает его сразу — он **откладывает**
событие в hand-FSM:

- `Man.LocalDropEntity` → `DropEntityImpl(InventoryMode.LOCAL, …)` (`3_game/entities/man.c:140/150`)
  → `GetHumanInventory().DropEntity(LOCAL, owner, item)`.
- `HumanInventory.DropEntity` для `src.GetType()==HANDS` → `HandEvent(LOCAL, new HandEventDrop(...))`
  (`3_game/systems/inventory/humaninventory.c:139-152`, сам `HandEvent` на `:151`).
- `DayZPlayerInventory.HandEvent` (`4_world/systems/inventory/dayzplayerinventory.c:2605`) ставит
  `m_DeferredEvent = new DeferredHandEvent(mode, e)` + `ReserveInventory(this)` и возвращает `true`
  (`:2618-2621`). **Фактическое перемещение происходит позже**, в
  `HumanInventory.Update → HandleInventoryManipulation` (`humaninventory.c:593/595`), т.е. следующим
  тиком.

Поэтому в (C) предмет всё ещё в руке (деferred-дроп не обработан): `RemoteObjectTreeCreate`
пере-создаёт его на клиенте **на прежнем месте (в руке)**. Потом deferred-дроп двигает предмет
на землю только на сервере — для AI_SERVER нет `SendServerMove`, поэтому клиент так и не узнаёт,
что рука пуста.

Дополнительно: дроп из рук **всегда** идёт через hand-FSM (`HumanInventory.TakeToDst` →
`RedirectToHandEvent` → `HandEvent(MoveTo/Take/Drop)`, `humaninventory.c:243-251` и `:200-241`);
синхронного «вынуть из руки» пути в скриптовом API нет.

**Вывод:** `RemoteObjectTreeDelete/Create` работает для CARGO/ATTACHMENT→GROUND (там
`LocalTakeToDst` синхронен — эталон Expansion `eAI_TakeItemToLocation`, `eAIBase.c:10605`), но
**для HANDS→GROUND он ломается именно из-за deferred-тайминга**.

---

## 1. Как Expansion дропает из рук (клиент видит)

База: `DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/AI/eAIBase.c`.

`eAI_DropItemImpl` (`eAIBase.c:10237-10287`) — ключевая ветка:

```c
if (item != GetItemInHands())
{
    eAI_TakeItemToLocation(item, il_dst);          // :10256 — синхронный путь (не руки)
}
else
{
    EntityAI dst = Expansion_CloneItemToLocation(item, il_dst);  // :10268 — ПРЕДМЕТ В РУКАХ
    ...
}
```

`Expansion_CloneItemToLocation(src, location)` (`eAIBase.c:6000-6075`) — **клонирует + удаляет**:

```c
if (il_src.GetType() == InventoryLocationType.HANDS)
    if (GetWeaponManager().IsRunning())
        GetWeaponManager().OnWeaponActionEnd();    // :6011-6012 — не оставлять WM в running

EntityAI dst = ExpansionItemSpawnHelper.Clone(src, true, location, false);  // :6017
if (dst)
{
    ...
    g_Game.ObjectDelete(src);                      // :6042 — полный сетевой delete оригинала
    ...
}
return dst;                                        // :6074
```

`ExpansionItemSpawnHelper.Clone(src, recursively=true, location, keepUnlootable=false)`
(`DayZExpansion/Core/Scripts/4_World/DayZExpansion_Core/Static/ExpansionItemSpawnHelper.c:594-765`):

1. **Создать новую сущность** в `location`:
   - GROUND: `g_Game.CreateObjectEx(src.GetType(), location.GetPos(), ECE_OBJECT_SWAP)` (`:608`);
   - ATTACHMENT/HANDS: `GameInventory.LocationCreateEntity(location, type, ECE_IN_INVENTORY, RF_DEFAULT)`;
   - CARGO: `location.GetParent().GetInventory().CreateEntityInCargoEx(...)`.
2. **Рекурсивно склонировать** attachments + cargo (+ предмет в руках, если src — Man) (`:643-677`).
3. **Оружие**: скопировать патроны стволов/внутреннего магазина
   `PushCartridgeToChamber`/`PushCartridgeToInternalMagazine` (`:679-705`).
4. **`OnStoreSave(src)` → `OnStoreLoad(dst)`** — сериализуемое состояние (health/quantity/etc.) (`:708-710`).
5. **Магазины**: `ServerSetAmmoCount(0)` + `ServerStoreCartridge` по каждому патрону (`:725-738`).
6. **Health/damage-zones** (`:748-755`); `AfterStoreLoad()` + `SetSynchDirty()` + (weapon) `Synchronize()`
   (`:758-761`); `GetCallQueue(CALL_CATEGORY_SYSTEM).Call(dst.EEOnAfterLoad)` (`:762`).

**Что именно делает руку пустой на клиенте у Expansion:** `g_Game.ObjectDelete(src)` (`eAIBase.c:6042`) —
полный сетевой delete оригинального предмета (который в руке). Клиент удаляет его из ручной модели
(он был дочерним объектом игрока), а новая сущность (`CreateObjectEx`, обычный сетевой spawn) появляется
на земле. Нет ни `HandEvent`, ни `SendServerMove`, ни `RemoteObjectTreeDelete/Create` — поэтому нет
деferred-тайминга и рассинхрона.

`eAI_TakeItemToLocation` (`eAIBase.c:10605-10673`) — для НЕ-рук предметов:
`RemoteObjectTreeDelete(item)` → `LocalTakeToDst(il_src, il_dst)` → `RemoteObjectTreeCreate(item)`
(`:10634-10670`). Это тот самый синхронный путь, который у нас скопирован в `DropItem`, но он
**непригоден для HANDS**, т.к. `LocalTakeToDst` из HANDS всё равно уходит в deferred `HandEvent`.

`eAI_TakeItemToHandsImpl` (`eAIBase.c:10303-10361`): `il_dst.SetHands(this, item)` →
`Expansion_CloneItemToLocation(item, il_dst)` (`:10335`, под `#ifndef EXPANSIONMODAI_TAKETOLOCATION`)
либо `eAI_TakeItemToLocation` (`:10333`). После взятия в руки — `GetItemAccessor().HideItemInHands(true)`
затем `HideItemInHands(false)` («unbreaks hand anim state», `:10652-10653`).

**Нокаут у Expansion** (`eAIBase.c:10158-10186`): `OnUnconsciousStart()` зовёт
`eAI_DropItemInHandsImpl(false, false, false)` (`:10163`) **до** `super.OnUnconsciousStart()`; тот →
`eAI_DropItemInHandsImpl` (`:10210`) → `eAI_DropItemImpl` (ветка «в руках» = clone+delete).
Т.е. Expansion дропает в том же фрейме, что и `StartCommand_Unconscious` — **наш заказчик сознательно
хочет иначе (отложенно)**, см. п.3.

---

## 2. Физическое отцепление (падение с velocity)

Гипотеза заказчика: «отцепить предмет от руки физически, оставить в мире и дать упасть». Разобрано по
API — **физика НЕ решает проблему «рука пуста на клиенте»** (это отдельная сетевая проблема, п.1);
физика нужна только чтобы предмет КРАСИВО упал/кувырнулся **после** того, как он уже вынут из руки.

Физический API (ваниль):

| API | Файл:строка | Назначение |
|---|---|---|
| `proto native void DisableSimulation(bool)` | `3_game/entities/entity.c:3` | вкл/выкл симуляцию сущности |
| `proto native bool GetIsSimulationDisabled()` | `entity.c:6` | статус симуляции |
| `proto native void CreateDynamicPhysics(int interactionLayers)` | `3_game/entities/object.c:462` | создать динамическое физтело |
| `proto native void EnableDynamicCCD(bool)` | `object.c:463` | continuous collision detection |
| `proto native void SetDynamicPhysicsLifeTime(float)` | `object.c:464` | время жизни физтела |
| `proto native void SetPosition(vector)` | `object.c:300` | переместить |
| `proto native void PlaceOnSurface()` | `object.c:305` | поставить на поверхность |
| `proto native vector GetSpeed()/GetModelSpeed()` | `object.c:341/344` | скорость |
| `proto native void dBodyActive(ent, ActiveState)` | `1_core/proto/enphysics.c:64` | активировать физтело |
| `proto native void dBodyDynamic(ent, bool)` | `enphysics.c:65` | сделать тело динамическим |
| `proto native bool dBodyIsActive(ent)` | `enphysics.c:68` | активно ли |
| `proto native bool dBodyEnableGravity(ent, bool)` | `enphysics.c:69` | гравитация |
| `proto native vector GetVelocity(ent)` / `void SetVelocity(ent, vector)` | `enphysics.c` (GetVelocity/SetVelocity) | скорость |
| `proto void dBodyApplyImpulse(ent, vector)` | `enphysics.c:141` | импульс в origin |
| `proto void dBodyApplyImpulseAt(ent, vector impulse, vector pos)` | `enphysics.c:136` | импульс в точке |
| `proto native void dBodyEnableCCD(ent, float maxMotion, float sphereCastRadius)` | `enphysics.c` | CCD |

Как ваниль делает «предмет падает из рук» у НАСТОЯЩЕГО игрока:

- `PlayerBase.DropItem` (`4_world/entities/manbase/playerbase.c:6497-6512`):
  `CanDropEntity(item)` → `PredictiveDropEntity(item)` (клиентский предиктив — именно он
  активирует физтело и показывает падение) → затем `item.SetPosition(pos_spawn)` +
  `item.PlaceOnSurface()` (ручная подстановка на землю).
- `OnUnconsciousStart` (`playerbase.c:3502-3522`): дроп `DropItem` только в ветке
  `INSTANCETYPE_CLIENT` (`:3508-3522`); серверная ветка (`:3524`) дропа не делает — см. `body.md`.
- Смерть: `PlayerBase.DropAllItems` (`playerbase.c:6679-6695`) — `ServerDropEntity(item)` по каждому
  предмету (для AI_SERVER падает, известная готча loot.md «краш 2»).

Для **серверного** «падения с velocity» эталонный приём: после того как предмет оказался в мире
(через clone из п.1), поднять его на ~10-30 см над землёй (`SetPosition`) и дать
`dBodyApplyImpulse`/`SetVelocity` (или просто `CreateObjectEx` с `ECE_OBJECT_SWAP` — флаг уже
содержит `ECE_CREATEPHYSICS`, см. `centraleconomy.c:38`, поэтому предмет сам упадёт под гравитацией,
если его позиция чуть выше поверхности). Отдельно `dBodyActive(ActiveState.ACTIVE)` +
`dBodyDynamic(true)` нужны только если физтело было «заморожено» (например после `DisableSimulation`).

`ECE_OBJECT_SWAP = 787488` (`ECE_CREATEPHYSICS|ECE_UPDATEPATHGRAPH|ECE_KEEPHEIGHT|ECE_NOSURFACEALIGN`,
`3_game/ce/centraleconomy.c:38`); `ECE_PLACE_ON_SURFACE = 1060` (`:37`, включает `ECE_TRACE` —
снап к поверхности). `CreateObjectEx(type, pos, flags, iRotation=RF_DEFAULT)` — `game.c:702`.

---

## 3. Отложенный вызов (НЕ в том же фрейме, что `StartCommand_Unconscious`)

`class ScriptCallQueue` (`2_gamelib/tools.c:52-74`):

```c
proto void Call(func fn, void param1 = NULL, ... void param9 = NULL);                    // tools.c:57
proto void CallLater(func fn, int delay = 0, bool repeat = false, void param1 = NULL, ...); // tools.c:61
proto void Remove(func fn);                                                              // tools.c:65
proto int  GetRemainingTime(func fn);                                                    // tools.c:67
proto native void Clear();                                                               // tools.c:73
```

- `delay` — миллисекунды; `repeat=false` — один вызов.
- Получение: `GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM)` (`game.c:1525`,
  `ScriptCallQueue GetCallQueue(int call_category)`).
- Категории: `CALL_CATEGORY_SYSTEM` / `CALL_CATEGORY_GAMEPLAY` / `CALL_CATEGORY_GUI`
  (константы движка; `CALL_CATEGORY_SYSTEM` — для серверной логики).

Паттерны использования в Expansion (референс):

- `g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Call(eAI_AddItem, item);` — `eAIBase.c:6090`
  («add in next tick to make sure we're not instantly adding items created by ObjectCreate(Ex)»).
- `g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Call(dst.EEOnAfterLoad);` — `ExpansionItemSpawnHelper.c:762`.
- `g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(eAI_Recreate, 34, false);` — `eAIBase.c:7174`
  (`34` мс ≈ 2 тика — используется как «следующий тик+буфер»).
- `CallLater(eAI_SetHalt, timeTresh * 0.65, false, false)` — `eAIBase.c:11692` (передача аргументов).
- `CallLater(m_EmoteManager.ServerRequestEmoteCancel, delay)` — `eAIBase.c:10887`.
- Отмена: `GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Expansion_PlayEmote)` — упомянут в задаче.

**Рекомендация по интервалу:** следующий тик (`CallLater(fn, 0, false)`) уже не совпадает с
`StartCommand_Unconscious` (тот выполнился в текущем кадре). Для надёжности «unconscious уже
стартовал» использовать ~1 кадр (`0`) с проверкой `IsUnconscious()` в самой deferred-функции, либо
`34` мс (эталон Expansion) как буфер. `IsUnconscious()` =
`m_MovementState.m_CommandTypeId == COMMANDID_UNCONSCIOUS || m_IsUnconscious`
(`playerbase.c:3655-3658`); `GetCommand_Unconscious()` — native (`human.c:1515`),
`StartCommand_Unconscious(float)` — native (`human.c:1513`). Проверку можно сделать дешёвым guard'ом:
`if (!IsUnconscious() && !IsAlive()) return;` — но обязательность проверки зависит от сценария (для
`EEKilled` смерть уже наступила — `IsUnconscious()` может быть `false`, поэтому в death-ветке guard
другой либо отсутствует).

**Гарантия «команда стартовала»:** после `StartCommand_Unconscious(0)` синхронно становится
`m_MovementState.m_CommandTypeId == COMMANDID_UNCONSCIOUS` (и `GetCommand_Unconscious() != null`);
deferred-дроп с задержкой ≥1 кадра не пересечётся с инициализацией команды. Риск «сломать нативную команду» возникает не от `CallLater`
как такового, а от манипуляции с предметом в руке в тот же кадр, что и `StartCommand_Unconscious` —
именно это и убирается defer'ом.

---

## 4. Рецепт для `dayz-dev`

> **⚠ ОТМЕНЕНО (v3.403).** «Вариант B» (clone+delete) ниже — ЗАПРЕЩЁН (см. `docs/codeguide.md`
> «ЖЁСТКОЕ ПРАВИЛО: НИКОГДА clone+delete»). Рабочий рецепт — обёртка `RemoteObjectTreeDelete(item)` ДО
> + `LocalDropEntity(item)`/`LocalTakeToDst` + **отложенный** `RemoteObjectTreeCreate` (`CallLater`, после
> фактического `HandEventDrop`) — та же сущность, без клона. См. `docs/research/item-move-sync.md`
> (вариант `deferred_delete_create`). Текст ниже — историческая трасса.

### Вариант A — минимальный (правка текущего `DropItem`): deferred re-sync

Меняем `DropItem` так, чтобы `RemoteObjectTreeDelete/Create` выполнялись **после** фактического
(deferred) перемещения предмета на землю:

```c
override bool DropItem(ItemBase item)   // dmAISurvivorBase.c:2153
{
    if (!item) return false;

    bool ok = LocalDropEntity(item);    // ставит deferred HandEventDrop; предмет двигается след. тиком
    if (ok)
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ReSyncDroppedItem, 0, false, item);
    return ok;
}

private void ReSyncDroppedItem(ItemBase item)
{
    if (!item || item.IsSetForDeletion()) return;
    GetGame().RemoteObjectTreeDelete(item);
    GetGame().RemoteObjectTreeCreate(item);
}
```

- Плюс: минимальное изменение, предмет сохраняет идентичность/состояние (не клонируем).
- Минус: **`[нужно проверить]`** — подтверждает ли `RemoteObjectTreeDelete/Create`, вызванный после
  фактического дропа, что клиент отцепит руку. Логика: delete снимает предмет с клиента (в т.ч. из
  руки, т.к. он дочерний у игрока), create пере-создаёт его уже как world-объект (parent=null).
  Остаточный риск — точное время обработки `HandleInventoryManipulation` относительно `CallQueue.Tick`
  (если create сработает раньше фактического перемещения, рука снова не очистится) → при неудаче
  поднять задержку до `34` мс.

### Вариант B — проверенный (эталон Expansion): clone + delete

```c
// 1. Куда кладём
InventoryLocation il_dst = new InventoryLocation();
GameInventory.SetGroundPosByOwner(this, item, il_dst);   // inventory.c:1257

// 2. Стоп WM, чтобы не остаться в running (эталон eAIBase.c:6011-6012)
WeaponManager wm = GetWeaponManager();
if (wm && wm.IsRunning()) wm.OnWeaponActionEnd();

// 3. Новая сущность на земле (сетевой spawn → клиент видит)
EntityAI clone = EntityAI.Cast(g_Game.CreateObjectEx(item.GetType(), il_dst.GetPos(), ECE_OBJECT_SWAP));

// 4. Скопировать состояние
MiscGameplayFunctions.TransferItemProperties(item, clone); // health+variables+agents (miscgameplayfunctions.c:269)
// attachments/cargo: MiscGameplayFunctions.TransferInventory(item, clone, this); // :344 (переносит children)
// (для оружия дополнительно: chambers/internal-mag патроны и ServerSetAmmoCount/StoreCartridge —
//  полный набор см. ExpansionItemSpawnHelper.Clone, ExpansionItemSpawnHelper.c:679-738)

// 5. Удалить оригинал (снимает предмет с клиента И с руки на сервере)
g_Game.ObjectDelete(item);   // game.c:704
```

- Плюс: **гарантированно чистит руку** (полный `ObjectDelete`), без deferred-тайминга. Это ровно то,
  что делает Expansion (`eAIBase.c:6017 + :6042`).
- Минус: тяжелее (клонирование состояния; для оружия нужен полный набор копирования патронов/
  магазинов/attachments). Для нашего случая (из рук бота падает обычно оружие) потребуется перенести
  из `ExpansionItemSpawnHelper.Clone` шаги 3-6.

**Рекомендация:** начать с **Варианта A** (правка минимальна, `dayz-tester` быстро подтвердит
probe'ом `botdump`/`scanbox`). Если клиент по-прежнему видит оружие в руке — перейти к Варианту B
(эталон, подтверждённый производственным кодом Expansion).

### Отложенность (обязательно для обоих вариантов)

Сам вызов `DropItem` из `UpdateUnconsciousBridge` (`dmAISurvivorBase.c:1218`) сейчас происходит в том же
кадре, что и `StartCommand_Unconscious(0)` (`:1195`). Заказчик требует НЕ дропать в том же фрейме.
Минимальный фикс — обернуть дроп в `CallLater` на вызывающей стороне (не внутри `DropItem`):

```c
// в UpdateUnconsciousBridge, вместо прямого DropItem(...):
GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DropItemDeferred, 34, false, item);
// DropItemDeferred(ItemBase item) { DropItem(item); /* + вернуть оружие при пробуждении */ }
```

Причина интервала ~34 мс: эталон Expansion `CallLater(eAI_Recreate, 34, false)` (`eAIBase.c:7174`);
`0` — тоже допустимо (следующий тик), но 34 мс даёт буфер на инициализацию `HumanCommandUnconscious`.

### Смерть vs отключка

Отдельный путь НЕ нужен — оба идут через один `DropItem`:

- **Нокаут**: `UpdateUnconsciousBridge` (`:1181`) → deferred `DropItem`. Guard «не дропать в том же
  фрейме» — через `CallLater` (см. выше). `SetDroppedWeapon(inHands)` (`:1217`) сохраняем как есть.
- **Смерть**: `EEKilled` (`:1309`) дропает ДО `super.EEKilled` (чтобы предмет оказался на земле к
  моменту `InsertCorpse` в `super`). Здесь `IsAlive()` ещё `true` на момент `DropItem` → hand-FSM
  deferred-путь. После `super.EEKilled` ванильный `DayZPlayerImplement.EEKilled`
  (`dayzplayerimplement.c:786`) сам зовёт `SendDeathJuncture` + `super`; `DropAllItems` (который
  падал бы на `ServerDropEntity`) наш override не трогает, т.к. мы вынули предмет из руки заранее.
  Для смерти defer не обязателен (не `StartCommand_Unconscious`), но и не вредит — можно тем же
  `DropItem` напрямую.

---

## Открытые вопросы / `[нужно проверить]`

1. **[нужно проверить] Вариант A** — чистит ли клиент руку, если `RemoteObjectTreeDelete/Create`
   вызвать `CallLater`-ом **после** фактического deferred-дропа (задержка 0 мс, при неудаче 34 мс)?
   Probe: `spawnobj`/`botdump` бота с оружием в руках → дроп → наблюдатель клиентом видит пустую руку
   + предмет на земле (`scanbox` ловит упавший `Weapon_Base`).
2. **[нужно проверить]** точный порядок `HandleInventoryManipulation` (обработка `DeferredHandEvent`)
   vs `CallQueue(CALL_CATEGORY_SYSTEM).Tick` в кадре: достаточно ли `delay=0`, чтобы `RemoteObjectTreeCreate`
   сработал уже на «земле», или нужен `34` мс. Эмпирика: снять `GetItemInHands()`/`item.GetHierarchyParent()`
   в deferred-функции до/после.
3. **[нужно проверить]** в Варианте B: хватает ли `MiscGameplayFunctions.TransferItemProperties` +
   `TransferInventory` для сохранения боевого оружия (магазин+патроны+ствольные патроны), или нужно
   полное клонирование патронов как в `ExpansionItemSpawnHelper.Clone` (`:679-738`). Проверка: дроп
   заряженного M4/B95 → `botdump` упавшего оружия → `GetAmmoCount` магазина и `IsChamberEmpty` стволов.
4. **[нужно подтвердить]** `ObjectDelete(item)` на предмете в руках корректно чистит слот руки
   **на сервере** (не только на клиенте) — чтобы `InsertCorpse` при смерти не подхватил «призрачный»
   предмет. Сценарий: `EEKilled` → труп без оружия в инвентаре, оружие лежит отдельно на земле.

## Существенные развилки и принятые решения

- **Развилка:** чинить тайминг текущего `RemoteObjectTreeDelete/Create` (A) или переходить на
  clone+delete (B). **Решение:** предложить A как первую итерацию (минимальный дифф), B как
  эталонный fallback. Почему: B гарантированно чистит руку (полный `ObjectDelete`), но тянет за
  собой клонирование состояния оружия; A — одна строка `CallLater`, но требует эмпирической
  валидации. Выбор влияет на объём кода и на сохранность боевого лоадаута при дропе.
- **Развилка:** defer'ить дроп на вызывающей стороне (`UpdateUnconsciousBridge`/`EEKilled`) или
  внутри `DropItem`. **Решение:** на вызывающей стороне. Почему: `DropItem` — общий (используется и
  лутом, где defer не нужен/вреден); требование «не в том же фрейме» относится именно к нокауту
  (риск с нативным `HumanCommandUnconscious`).
- **Развилка:** нужен ли отдельный путь для смерти. **Решение:** нет — оба через `DropItem`.
  Почему: оба кейса уже идут через один override; отличие только в «надо ли defer», что решается на
  вызывающей стороне. Действует пока: не появится спец-требование к трупу (напр. «оружие должно
  остаться при трупе, а не падать отдельно»).

## Источники (файл:строка)

- Ваниль (`DayZ-Script-Diff/scripts/`): `3_game/entities/man.c:140/150/159`;
  `3_game/systems/inventory/humaninventory.c:139-162/200-251/593-612`;
  `4_world/systems/inventory/dayzplayerinventory.c:2605-2645/2452`;
  `4_world/entities/manbase/playerbase.c:6497-6512/3502-3553/6679-6695`;
  `4_world/entities/dayzplayerimplement.c:786/988`;
  `3_game/global/game.c:702/704/707/709/1525`;
  `3_game/ce/centraleconomy.c:36-38/24`;
  `3_game/systems/inventory/inventory.c:1257-1284`;
  `4_world/static/miscgameplayfunctions.c:269-286/344-384`;
  `2_gamelib/tools.c:52-74`;
  `1_core/proto/enphysics.c:64-69/136/141` (+ `GetVelocity`/`SetVelocity`);
  `3_game/entities/object.c:300/305/341-344/462-464`;
  `3_game/entities/entity.c:3/6`;
  `3_game/humanitems.c:109-121`.
- Expansion: `DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/AI/eAIBase.c:6000-6075/
  10210-10287/10605-10673/10303-10361/10158-10186/6090/7174`;
  `DayZExpansion/Core/Scripts/4_World/DayZExpansion_Core/Static/ExpansionItemSpawnHelper.c:594-765`.
- Мод botorama: `src/core/4_World/Entities/Bot/dmAISurvivorBase.c:1181-1255/1309-1337/2153-2170`.
- Кросс-ссылки: `docs/research/body.md` (нокаут, `m_IsUnconscious`), `docs/research/loot.md`
  (готчи «краш 1/2», ре-синк `RemoteObjectTreeDelete/Create`).
