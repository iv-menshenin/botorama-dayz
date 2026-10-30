# Рецепт: сетевой синк мува предметов для ИИ-бота (dm-префикс)

Статус: эмпирически подтверждён E2E-прогоном (клиентская картина). Источник —
прогон `wearprobe`/`dropprobe` (серверные проб-опы) + `slotdump`/`scanobj` (клиентский
мост). Заменяет/уточняет тезисы из `docs/research/loot.md` (раздел «ре-синк») и
`docs/research/drop-to-ground.md` §3.2/§4.

## Правило одной строкой

**Любой мув предмета ИИ-бота между локациями оборачивается `RemoteObjectTreeDelete` ДО +
`RemoteObjectTreeCreate` ПОСЛЕ.** Примитив мува: на землю — `LocalDropEntity`, в слот/карго/
руки — `LocalTakeToDst`.

```c
GetGame().RemoteObjectTreeDelete(item);   // ДО мува — ОБЯЗАТЕЛЬНО для синка клиенту
<мув>                                     // LocalDropEntity (на землю) / LocalTakeToDst (в слот/карго)
GetGame().RemoteObjectTreeCreate(item);   // ПОСЛЕ мува
```

## Надеть / подобрать (GROUND → ATTACHMENT / CARGO / HANDS)

```c
InventoryLocation src = new InventoryLocation();
if (!item.GetInventory().GetCurrentInventoryLocation(src))   // src.GetType() == GROUND
    return false;

InventoryLocation dst = new InventoryLocation();
dst.SetAttachment(pawn, item, slotId);     // или dst.SetHands(pawn, item) / FindFreeLocationFor(CARGO, dst)

GetGame().RemoteObjectTreeDelete(item);
bool ok = pawn.LocalTakeToDst(src, dst);   // LOCAL-режим → LocationSyncMoveEntity
GetGame().RemoteObjectTreeCreate(item);
return ok;
```

## Снять / дропнуть (ATTACHMENT / CARGO → GROUND)

```c
GetGame().RemoteObjectTreeDelete(item);
bool ok = LocalDropEntity(item);           // НЕ LocalTakeToDst — для земли правилен LocalDropEntity
GetGame().RemoteObjectTreeCreate(item);
return ok;
```

## Направление мува → примитив

| Направление | Примитив | Почему |
|---|---|---|
| → GROUND | `LocalDropEntity(item)` | `SetGroundPosByOwner` + `TakeToDst(LOCAL)`; `LocalTakeToDst` на землю НЕ освобождает слот |
| → ATTACHMENT/CARGO/HANDS | `pawn.LocalTakeToDst(src, dst)` | синхронный `LocationSyncMoveEntity` |

## Ключевые выводы эксперимента (клиентская картина)

| Вариант | wear (надеть) | drop (снять) |
|---|---|---|
| голый `LocalTakeToDst` / `LocalDropEntity` (без обёртки) | ❌ призрак на земле | ❌ призрак надетого |
| `SetSynchDirty()` вместо обёртки | ❌ | ❌ |
| `create_only` (без delete) | ❌ | — |
| `deferred_create` (delete нет, create отложен) | ❌ | ❌ |
| **`tree`** (`delete` + мув + `create` синхронно) | ✅ | ✅ |
| `deferred_delete_create` (delete+мув, create отложен) | ✅ | — |
| `TakeToDst(SERVER)` | ❌ тихий no-op | ❌ |

1. **`RemoteObjectTreeDelete(item)` ДО мува обязателен.** Без него клиент держит старую
   картину (предмет «зависает» на земле при надевании / остаётся «надетым» при снятии).
2. **`RemoteObjectTreeCreate` можно отложить `CallLater`** (`deferred_delete_create` тоже
   работает) — но `delete` должен быть синхронным, до мува.
3. **`TakeToDst(SERVER)` для `AI_SERVER` — тихий no-op** (не `#return`): мув уходит в
   `SendServerMove`, который серверным ИИ не применяется. Не использовать.
4. **`SetSynchDirty()` не заменяет `delete`** — сам по себе клиентской картины не чинит.

## Исключение — HANDS (из рук)

`LocalTakeToDst` из HANDS уходит в deferred `HandEvent` (см. `docs/research/body-drop.md`) —
обёртка `RemoteObjectTreeCreate` срабатывает раньше фактического перемещения. Из рук —
**clone+delete** (`DropItemToGround`: `CreateObjectEx(ECE_OBJECT_SWAP)` + `TransferItemProperties` +
`TransferInventory` + `ObjectDelete`), как Expansion `Expansion_CloneItemToLocation`.

## Где это применяется в моде

- `dmLoot.TakeToHands` / `TakeIntoCargo` / `TakeToAttachmentSlot` / `TakeIntoDestination` — обёртка + `LocalTakeToDst`.
- `dmAISurvivorBase.DropItemToGroundLocal` — обёртка + `LocalDropEntity` (не-руки → земля).
- `dmAISurvivorBase.DropItemToGround` — clone+delete (только HANDS).

## Источники

- Прогон e2e: `wearprobe` (7 вариантов), `dropprobe` (3 варианта) в `src/test/4_World/dmE2EBridge.c`;
  клиентские `slotdump`/`scanobj` в `src/test/4_World/dmE2EClientBridge.c`.
- Ваниль: `RemoteObjectTreeDelete/Create` (`game.c:707/709`); `LocalTakeToDst` (`man.c:842` →
  `LocationSyncMoveEntity` `inventory.c:567`); `LocalDropEntity` (`man.c:140`); `TakeToDst(SERVER)`
  → `SendServerMove` (`inventory.c:1061-1064`).
- Expansion: `eAI_TakeItemToLocation` (`eAIBase.c:10605`) — тот же паттерн `delete`+`LocalTakeToDst`+`create`.
- Кросс-ссылки: `docs/research/loot.md` («краш 1/2», ре-синк), `docs/research/body-drop.md` (HANDS),
  `docs/research/drop-to-ground.md` (CARGO→GROUND).
