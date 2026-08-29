# Research: спавн и механики EntityAI / PlayerBase

Статус: спавн/инвентарь уже освоены (loadout). Пополняется по мере задач.

## Цель

Собрать проверенные сигнатуры и поведение `EntityAI`/`PlayerBase`/`ItemBase`,
используемые ботами: спавн, инвентарь, здоровье/количество, сетевые деревья.

## Проверено (работает)

- Спавн пешки: `GetGame().CreateObject(model, pos)` → `PlayerBase.Cast(...)` →
  `INSTANCETYPE_AI_SERVER` + `EconomyProfile` (протухание трупа). `CreatePlayer(null,…)`
  профиля НЕ даёт.
- Спавн предмета в мир: `GetGame().CreateObject(cls, pos)` (возвращает `Object`, каст в `EntityAI`).
- Инвентарь (выдача экипировки, эталон `cfgplayerspawnhandler.c`):
  - `GameInventory.CreateAttachmentEx(typeName, slotId)` — слот.
  - `HumanInventory.CreateInHands(typeName)` — руки (`Man.GetHumanInventory()`).
  - `GameInventory.CreateInInventory(type)` — карго/вложенный предмет (сам находит).
  - `GameInventory.CreateEntityInCargoEx(typeName, idx, row, col, flip)` — карго (низкоуровневый).
  - `GameInventory.CreateAttachment(typeName)` — авто-слот.
  - `Weapon_Base.SpawnAmmo(magazineType, flags)` — магазин (внутр./внеш.).
- Перемещение: `TakeEntityToInventory/AsAttachmentEx` = **MOVE** (`TakeToDst`) — НЕ работает
  на свежем `CreateObject` (нет `InventoryLocation`). Добавление нового — `AddEntityToInventory(item)`
  (`FindFreeLocationFor` + `LocationAddEntity`).
- Здоровье/количество: `SetHealth01("", "Health", x)`; магазин — `ServerSetAmmoCount(int)`,
  `GetAmmoMax()`; стак — `SetQuantity(Lerp(GetQuantityMin(), GetQuantityMax(), frac))`,
  `HasQuantity()`.
- `InventorySlots.GetSlotIdFromString(name)` → slotId (или `INVALID`); `GetSlotName(id)`.
- Синк предмета: `SetSynchDirty()`; сетевые деревья `RemoteObjectTreeDelete/Create(Object)`.

## Открытые вопросы (research)

1. `GetInventory()` возвращает `GameInventory`; у Man — `HumanInventory`. Проверить
   нюансы для `INSTANCETYPE_AI_SERVER` (руки, reserved locations).
2. Как корректно «взять предмет с земли» (мир → инвентарь) существующим предметом:
   `TakeEntityToInventory(SERVER, CARGO, item)` после того как у предмета есть локация —
   работает ли для бота?
3. `AddEntityToInventory` — полный список флагов/ограничений (вложенные контейнеры).

## Источники

- `DayZ Projects/scripts/3_game/systems/inventory/inventory.c` (inventorylocation.c, humaninventory.c)
- `DayZ Projects/scripts/4_world/classes/playergearspawn/cfgplayerspawnhandler.c`
- `dayz-devaliada/src/mods/devaliada/4_World/ExpLootSpawner.c` (обход вложенного контейнера)
