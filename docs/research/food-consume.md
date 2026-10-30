# Research: завершение потребления еды/напитка у ИИ-бота (food-consume)

## Цель

Выяснить, как корректно завершить потребление еды/напитка у `INSTANCETYPE_AI_SERVER`-бота:
доеденная еда должна **исчезнуть**, допитая бутылка/фляга — **остаться пустой** (переиспользуема),
сода-банка — **удалиться**. Сейчас `dmBotIntent_EatDrink.PhaseEat` зовёт `pawn.Consume(...)`
по тикам и при `GetQuantity() <= 0` просто сбрасывает фазу, НЕ удаляя еду и не освобождая предмет —
бот находит ту же вещь с нулевым количеством и ест бесконечно.

## 1. Полная цепочка потребления (file:line)

```
PlayerBase.Consume(ItemBase source, float amount, EConsumeType consume_type)  // playerbase.c:9723
  → строит PlayerConsumeData {m_Type, m_Amount, m_Source, m_Agents=0}          // :9726-9730
  → Consume(PlayerConsumeData data)                                             // :7344
      - Edible_Base edibleItem = Edible_Base.Cast(data.m_Source);               // :7368
      - if (!edibleItem || !edibleItem.CanBeConsumed()) return false;           // :7369-7370
      - ITEM_SINGLE_TIME | ITEM_CONTINUOUS:                                     // :7372
        * IsLiquidContainer()? → m_PlayerStomach.AddToStomach(liquidClassName,...) :7386-7387
        * else                 → AddToStomach(data.m_Source.GetType(), ...)       :7395
        * edibleItem.Consume(data.m_Amount, this);                               // :7398

Edible_Base.Consume(float amount, PlayerBase consumer)   // edible_base.c:95
  → AddQuantity(-amount, false, false);                  // :97  ← destroy_config=false, destroy_forced=false
  → OnConsume(amount, consumer);                         // :98  (горячий урон: ProcessDirectDamage CUSTOM)
```

**Ключевой вывод**: `Edible_Base.Consume` уменьшает количество через
`AddQuantity(-amount, false, false)` — **оба флага уничтожения сброшены**, т.е. сам `Consume`
НИКОГДА не удаляет предмет. Удаление — НЕ внутри `Consume`, а в завершении **экшена** (см. §2).

`CanBeConsumed()` = `!GetIsFrozen() && IsOpen()` (itembase.c:4653) — **НЕ проверяет quantity**.
Поэтому пустая вещь (qty=0) по-прежнему «потребляема» через `Consume` — это и есть корень бага.

## 2. Как ваниль удаляет съеденное — `SetQuantity(0)` + `varQuantityDestroyOnMin`

Ванильный экшен еды/питья завершается в `OnEndServer` и вызывает `SetQuantity(0)`:

```
ActionConsume.OnEndServer(ActionData)     // actionconsume.c:45
  if (item && item.GetQuantity() <= 0.01)  // :51
      item.SetQuantity(0);                 // :53   (destroy_config = true по умолчанию)

ActionConsumeSingle.OnEndServer            // actionconsumesingle.c
  if (GetQuantity() <= 0) SetQuantity(0);  // аналог для однократного
```

`SetQuantity` (itembase.c:3340) — вот где происходит авто-удаление:

```
override bool SetQuantity(float value, bool destroy_config = true,
                          bool destroy_forced = false, bool allow_client = false,
                          bool clamp_to_stack_max = true)   // itembase.c:3340
{
    if (!IsServerCheck(allow_client)) return false;
    if (!HasQuantity()) return false;
    float min = GetQuantityMin(); ...
    if (value <= (min + 0.001)) value = min;                 // :3349
    if (value == min) {                                     // :3351
        if (destroy_config) {
            bool dstr = ConfigGetBool("varQuantityDestroyOnMin");  // :3358
            if (dstr) { m_VarQuantity = ...; this.Delete(); return true; }  // :3361-3363
        }
        else if (destroy_forced) { m_VarQuantity = ...; this.Delete(); return true; } // :3367-3370
        RemoveAllAgents();                                   // :3373  ← НЕ удаляет, только сбрасывает агентов
    }
    ...
}
bool AddQuantity(float value, bool destroy_config = true, bool destroy_forced = false)  // :3413
    → SetQuantity(GetQuantity() + value, destroy_config, destroy_forced);
```

**Механика**: `SetQuantity(0)` удаляет предмет ТОЛЬКО если в конфиге предмета
`varQuantityDestroyOnMin = true`. Иначе предмет остаётся с quantity=0 (пустой) и лишь
сбрасывает агентов (`RemoveAllAgents`). Это ЕДИНЫЙ источник правды «удалить vs оставить пустым».

### Разница `EConsumeType.ITEM_CONTINUOUS` vs `ITEM_SINGLE_TIME`

Для авто-удаления разницы НЕТ: оба пути в `Consume` (`playerbase.c:7372`) вызывают
`edibleItem.Consume` → `AddQuantity(-amount, false, false)` (без удаления). Отличие только:
- `ITEM_SINGLE_TIME` дополнительно зовёт `plugin.TransmitAgents(...)` (`:7377-7378`) — перенос
  агентов с предмета на потребителя за один раз (для таблеток/однократного).
- `ITEM_CONTINUOUS` этого не делает (агенты идут через `FilterAgents` при каждой порции).

Удаление в обоих случаях — только через финальный `SetQuantity(0)` в `OnEndServer` экшена.
Наш бот зовёт `Consume(..., ITEM_CONTINUOUS)` напрямую и обходит `OnEndServer` → поэтому предмет
никогда не удаляется.

## 3. Конфиг: кто удаляется, кто остаётся пустым

Иерархия конфига (CfgVehicles) — источник `varQuantityDestroyOnMin`:

```
Inventory_Base (DZ/data/config.cpp:3145): destroyOnEmpty=1; varQuantityDestroyOnMin=1;  // :3211
├── Edible_Base (DZ/data/config.cpp:3332): varQuantityInit=1000, Min=0, Max=1000  (без override → наследует =1)
│   ├── ЕДА (DZ/gear/food/config.cpp): НЕТ varQuantityDestroyOnMin → наследует =1 → УДАЛЯЕТСЯ
│   ├── SodaCan_ColorBase (DZ/gear/drinks/config.cpp:788): НЕТ override → наследует =1 → УДАЛЯЕТСЯ
│   └── Bottle_Base (DZ/data/config.cpp:3503)  (абстрактный, наследует =1)
│       ├── Canteen (drinks:26):        destroyOnEmpty=0; varQuantityDestroyOnMin=0  // :51-52
│       ├── WaterBottle (drinks:188):   ...=0                                          // :197-198
│       ├── GlassBottle (drinks:~334):  ...=0                                          // :339-340
│       ├── WaterPouch_ColorBase (~476):...=0                                          // :484-485
│       └── FilteringBottle (~648):     ...=0                                          // :653-654
```

`destroyOnEmpty` в скриптах не читается (это нативный/движковый признак), используется только
`varQuantityDestroyOnMin` в `SetQuantity`.

**Вывод по предметам:**
- **Еда** (Apple, мясо, консервы `BakedBeansCan`, ...): `varQuantityDestroyOnMin=1` → `SetQuantity(0)` удаляет.
- **Сода-банка** (`SodaCan_ColorBase` и наследники): наследует `=1` → удаляется. `SodaCan_Empty`
  (`sodacan_empty.c`, `Inventory_Base`, модель `SodaCan_Used.p3d`) — ОТДЕЛЬНЫЙ world-spawn мусор,
  НЕ результат допивания полной банки (никакой `ReplaceEdibleWithNew`/трансформации в scripts нет).
- **Бутылка/фляга** (`Canteen`/`WaterBottle`/`GlassBottle`/`WaterPouch`/`FilteringBottle`):
  явный `=0` → остаётся пустой, переиспользуема (у `Bottle_Base` есть `ActionFillBottleBase`,
  у `SodaCan_ColorBase` — нет).

## 4. API удаления для ИИ-бота (`AI_SERVER`)

```
Object.Delete()          // 3_game/entities/object.c:82
    → g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Call(g_Game.ObjectDelete, this);   // deferred, след. кадр

EntityAI.Delete()        // 3_game/entities/entityai.c:774
    → m_PendingDelete = true; super.Delete();   // помечает + планирует ObjectDelete

EntityAI.DeleteSafe()    // entityai.c:786
    → нет живого hierarchy-root игрока → Delete()
    → иначе (сервер+MP) → GetHierarchyRootPlayer().JunctureDeleteItem(this)   // синк-удаление
    → иначе             → GetHierarchyRootPlayer().AddItemToDelete(this)

g_Game.ObjectDelete(obj) // 3_game/global/game.c:704  (native, полное удаление)
g_Game.ObjectDeleteOnClient(obj)  // game.c:705
g_Game.RemoteObjectDelete / RemoteObjectTreeDelete(obj) // game.c:706-707 — НЕ удаляет на сервере,
    // только снимает сетевую репликацию («do not use if not sure what you do»)
```

**Для нашего случая (предмет в РУКАХ AI_SERVER-бота)**: правильный путь — тот же, что делает
ванильный `SetQuantity(0)` → `this.Delete()` = `EntityAI.Delete()` → deferred `ObjectDelete`.
Полное `ObjectDelete` чистит руку и на сервере, и на клиенте (подтверждено decisions.md
«body: дроп из рук — clone+delete», v3.302; тот же приём в `DropItemToGround`
`dmAISurvivorBase.c:2539`). `DeleteSafe()` для предмета в руках ЖИВОГО `PlayerBase` уходит в
`JunctureDeleteItem` — это тоже валидно, но для простого «доел» избыточно; `Delete()` (голый,
через `SetQuantity(0)`) — штатный ванильный путь.

Установившийся паттерн в моде: `dmBotIntent_MedicalAction.c:190/206` уже делает `m_Item.Delete()`
для таблеток/шин; `ApplyBandage` — `m_Item.AddQuantity(-1, true)` (destroy_config=true) для
предметов с quantity.

## 5. «Вернуть пустую бутылку в инвентарь» — мув из рук ломается

Ванильные мувы из РУКИ уходят в deferred HandEvent, что для `AI_SERVER` не годится:

```
HumanInventory.TakeToDst / TakeEntityToInventory   // 3_game/systems/inventory/humaninventory.c:240/253
  → if (GetManOwner().IsAlive() && RedirectToHandEvent(mode, src, dst)) return true;  // :247/261

RedirectToHandEvent   // humaninventory.c:200
  if (src.GetType() == HANDS)  → man.GetHumanInventory().HandEvent(mode, new HandEventMoveTo(...)); // :232
  if (dst.GetType() == HANDS)  → HandEvent(mode, new HandEventTake(...));                            // :243
```

`TakeEntityToInventory(mode, flags, item)` (inventory.c:1012) → `FindFreeLocationFor` → `TakeToDst`.
`Man.LocalTakeEntityToInventory(flags, item)` (man.c:386) = `TakeEntityToInventoryImpl(LOCAL, ...)`.
Всё равно для `src==HANDS` редиректится в `HandEvent` (deferred). Это тот же deferred-`HandEvent`,
который ломает дроп из рук у AI_SERVER (`body-drop.md` §1, `loot.md:336` «deferred inventory-команда
с null-player»).

**Вывод**: «вернуть бутылку из рук в карго» скриптовым мувом (`TakeIntoCargo`/`LocalTakeEntityToInventory`)
**не надёжно** из-за deferred HandEvent. Надёжный мув из рук — только clone+delete (Expansion
`Expansion_CloneItemToLocation`, eAIBase.c:6000; наш `DropItemToGround`, dmAISurvivorBase.c:2508),
но он меняет ссылку на сущность (ломает `Ignore`/re-collect).

**Рекомендация**: пустую бутылку **не перемещать вовсе** — она уже «в инвентаре» (в руках).
Достаточно оставить её в руках пустой; `FindEdible` обязан фильтровать `GetQuantity()==0`,
чтобы не пить её повторно. Свобода рук — отдельная забота (holster/TidyInventory), не этой задачи.

## 6. Рекомендация для `PhaseEat` (`dmBotIntent_EatDrink.c`)

1. **Фильтр в `dmLoot.FindEdible`** (dmLoot.c:102): пропускать `item.GetQuantity() <= 0`
   (аналог ванильного `CCINotRuinedAndEmpty.Can`, ccinotruinedandempty.c:3: `item.GetQuantity() > 0`).
   Это чинит «ест пустую вещь бесконечно».

2. **В `PhaseEat` при `GetQuantity() <= 0`** — зеркалить ванильный `ActionConsume.OnEndServer`,
   вызвать `m_Item.SetQuantity(0)` (destroy_config=true по умолчанию) ДО сброса `m_Item`:
   - еда и сода-банка (`varQuantityDestroyOnMin=1`) → авто `Delete()` (сетевое удаление, рука чистится);
   - бутылка/фляга (`varQuantityDestroyOnMin=0`) → остаётся пустой, только `RemoveAllAgents()`.

   Т.е. один вызов `m_Item.SetQuantity(0)` через конфиг делает ровно нужную развилку
   «удалить vs оставить пустым» — без ручного ветвления `IsDrink`/`Bottle_Base`.

3. Альтернатива (если хочется явно, без опоры на конфиг): ветвить вручную
   - `m_Item.IsInherited(Bottle_Base)` → оставить (не удалять);
   - иначе → `m_Item.Delete()` (паттерн уже есть в `MedicalAction`).
   Опора на `SetQuantity(0)` предпочтительнее: один механизм на класс задач, идентичен ванили.

4. **Порядок**: не сбрасывать `m_Item = null` до вызова `SetQuantity(0)/Delete()`, иначе потеряем
   ссылку. Псевдокод фикса:

   ```
   if (m_Item && m_Item.GetQuantity() <= 0.0) {
       StopAnim(pawn);
       m_Item.SetQuantity(0);   // еда/сода → Delete(); бутылка/фляга → остаётся пустой
       m_Item = null;
       m_Phase = 0;
       m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
   }
   ```

## 7. Гипотезы для эмпирической проверки

- **[нужно подтвердить] Конфиг-наследование `varQuantityDestroyOnMin`.** Подтвердить прогоном на
  сервере: дать боту яблоко (`Apple`) и соду (`SodaCan_Cola`), дождаться конца `PhaseEat` и
  проверить (`botdump`/`scanobj`), что предмет исчез из рук/мира (а не остался qty=0). Вопрос:
  действительно ли `Edible_Base`→`Inventory_Base` даёт `varQuantityDestroyOnMin=1` для еды и соды
  (в скриптах override отсутствует — вывод сделан по иерархии конфига, не по рантайму).
- **[нужно проверить] `m_Item.SetQuantity(0)` на AI_SERVER-боте сетево чист.** После `SetQuantity(0)`
  на доеденной еде в руках бота — видит ли клиентский наблюдатель (`scanobj`/`botdump` со стороны
  клиента) пустую руку И отсутствие предмета в мире, без «призрака»? (Ожидаемо да, т.к. путь —
  `EntityAI.Delete()` → `ObjectDelete`, тот же что подтверждён в decisions.md v3.302; но через
  `SetQuantity` путь не гоняли явно.)
- **[нужно проверить] Бутылка после `SetQuantity(0)` остаётся в руках с qty=0 и не репьётся.**
  Дать боту флягу с водой, дождаться «допил»; проверить: фляга в руках/инвентаре с `GetQuantity()==0`,
  `FindEdible` её больше не возвращает (после фикса фильтра), повторных `Consume` нет.

## 8. Существенные развилки / решения (для docs/decisions.md)

- **Развилка: «удалить доеденное» через `SetQuantity(0)` (конфиг) vs ручное ветвление
  `IsInherited(Bottle_Base)` + `Delete()`.** Решение: `SetQuantity(0)` — один механизм на класс
  задач (еда/сода/бутылка), идентичен ванильному `ActionConsume.OnEndServer`, не требует дублировать
  знание конфига в коде мода. Почему: ручное ветвление хрупко (каждый новый вид тары — новое условие),
  конфиг — единый источник правды. Действует пока: разница «удалить vs оставить» выражается
  `varQuantityDestroyOnMin`; если понадобится нестандартное поведение (напр. банка → `SodaCan_Empty`)
  — перейти на явное ветвление.
- **Развилка: «вернуть пустую бутылку в карго» мувом vs оставить в руках.** Решение: оставить в
  руках (не перемещать). Почему: мув из HANDS уходит в deferred `HandEvent` (ломается для AI_SERVER,
  body-drop.md/loot.md), а бутылка и так «в инвентаре»; надёжный мув из рук — только clone+delete
  (меняет ссылку, ломает Ignore/re-collect). Действует пока: не нужна гарантированная свобода рук
  после питья (тогда holster/TidyInventory-задача отдельно).
