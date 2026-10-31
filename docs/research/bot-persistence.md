# Bot persistence — бинарное сохранение/восстановление физического состояния пешки

**Цель**: выяснить, как персистить физическое состояние CE-спавненной пешки botorama
(`dmAISurvivorBase : PlayerBase`, созданной `GetGame().CreateObject(..., position)` БЕЗ
Identity) в бинарный файл и восстановить после рестарта. Мозг (`dmBotPreset_Nomad`) не
персистим.

**Ключевой вывод (архитектура)**: вызов `PlayerBase.OnStoreSave(ctx)` / `OnStoreLoad(ctx,
version)` с `FileSerializer` покрывает ТОЛЬКО script-side цепочку (статы, модификаторы,
агенты, симптомы, кровотечения, желудок, сломанные ноги, area-флаг, лестница, стрелы,
temperature/frozen). Он НЕ покрывает:
- **позицию/ориентацию** (никогда не пишутся в OnStoreSave),
- **здоровье** (не входит в script-цепочку, сохраняется нативно движком при штатном сейве),
- **инвентарь/экипировку** (items — отдельные EntityAI; `Inventory.OnStoreSave` — пустой
  engine-hook без script-тела).

Их нужно писать вручную. Эталон — devaliada (`OneShot/PlayerBase.c` + `OneShot/ItemBase.c`):
state через `OnStoreSave`, health через `GetHealth/SetHealth`, инвентарь — рекурсивно через
item-level `OSD_StoreTo`.

---

## 1. `FileSerializer` / `ParamsWriteContext` / примитивы

**Вывод**: `FileSerializer` — это `Serializer`, а `ParamsWriteContext`/`ParamsReadContext` —
это просто `typedef Serializer`. Поэтому `FileSerializer` напрямую подставляется туда, где
ожидается `ParamsWriteContext`/`ParamsReadContext` (как в devaliada: `OnStoreSave(file)`).

- `class FileSerializer: Serializer` — `1_core/proto/serializer.c:101`.
- `proto native bool Open(string path, FileMode mode = FileMode.READ);` — `serializer.c:108`.
  **`Open` возвращает bool** (false при неудаче — как в примерах TestSave/TestLoad).
- `proto native bool IsOpen();` — `serializer.c:109`.
- `proto native void Close();` — `serializer.c:110`.
- `Serializer`:
  - `proto bool Write(void value_out);` — `serializer.c:53`.
  - `proto bool Read(void value_in);` — `serializer.c:54`.
  - `proto native bool CanWrite(); / CanRead();` — `serializer.c:56-57`.
- `typedef Serializer ParamsReadContext;` и `typedef Serializer ParamsWriteContext;` —
  `3_game/gameplay.c:15-16`.
- `enum FileMode { READ, WRITE, APPEND }` — `1_core/proto/ensystem.c:382-386`.

**Примитивы (через generic `Write(void)`/`Read(void)`, native)** — подтверждены в ванильном
`OnStoreSave`/`OnStoreLoad`:
- `ctx.Write(int)` / `ctx.Read(int)` — `ctx.Write(m_PersistentFlags)` (`playerbase.c:7057`).
- `ctx.Write(string)` / `ctx.Read(string)` — devaliada `ctx.Write(sectionCode)`.
- `ctx.Write(float)` / `ctx.Read(float)` — `ctx.Write(m_EM.GetEnergy())` (`entityai.c:2927`).
- `ctx.Write(bool)` / `ctx.Read(bool)` — `ctx.Write(true)` (`playerbase.c:7067`).
- **`ctx.Write(vector)` / `ctx.Read(vector)` — поддерживается напрямую**: подтверждено
  `ctx.Write(ladder.GetLogoutPosition())` (`playerbase.c:7068`) и `ctx.Read(position)`
  (`playerbase.c:7174`). `vector` — сериализуемый примитив (см. док serializer.c: "primitive
  types: int, float, string, bool, vector").
- Контейнеры и классы (deep serialization): `ctx.Write(m_SaveQueue)` (`array<int>`,
  `statemanager.c:643`), `ctx.Write(m_MapMarkerArray)` (`array<ref MapMarker>`,
  `inventoryitem.c:1168`), `ctx.Write(m_PersistentFlags)`.

## 2. OnStoreSave/OnStoreLoad на CE-пешке без Identity

**Вывод**: script-цепочка сериализации **НЕ зависит от `GetIdentity()`/`m_Identity`/CE-профиля**.
Все менеджеры (PlayerStats, ModifiersManager, AgentPool, BleedingManagerServer,
SymptomManager, PlayerStomach, ArrowManager) создаются в **конструкторе** `PlayerBase.Init()`
под гейтом `g_Game.IsServer()` — БЕЗ проверки identity. На сервере они гарантированно
не-null у CE-спавненной пешки.

Цепочка `PlayerBase.OnStoreSave` → `super.OnStoreSave`:
- `PlayerBase.OnStoreSave` — `4_world/entities/manbase/playerbase.c:7032`:
  - `super.OnStoreSave(ctx)` (безусловно) → `ManBase` НЕ переопределяет → `ItemBase.OnStoreSave`
    (`4_world/entities/itembase.c:3221`) → `InventoryItem.OnStoreSave`
    (`4_world/entities/core/inherited/inventoryitem.c:1165`) → `EntityAI.OnStoreSave`
    (`3_game/entities/entityai.c:2925`): energy-manager + `SaveVariables` (temperature/frozen).
  - `GetHumanInventory().OnStoreSave(ctx)` (FSM of hands) — `humaninventorywithfsm.c:159`
    (тело фактически пустое: только `super` + закомментированный `m_FSM.OnStoreSave`).
  - `OnStoreSaveLifespan(ctx)` — `playerbase.c:7222` (lifespan state, bloodType, bloody hands).
  - Внутри `if (GetDayZGame().IsServer() && GetDayZGame().IsMultiplayer())` (`playerbase.c:7046`):
    `GetPlayerStats().SaveStats` → `m_ModifiersManager.OnStoreSave` → `m_AgentPool.OnStoreSave`
    → `GetSymptomManager().OnStoreSave` → `GetBleedingManagerServer().OnStoreSave` →
    `m_PlayerStomach.OnStoreSave` → `ctx.Write(GetBrokenLegs())` → `SaveAreaPersistenceFlag` →
    ladder position → `ArrowManagerPlayer.Save`.

`OnStoreLoad` — `playerbase.c:7085`, симметрично. **Проверка identity в цепочке — только одна
потенциальная**:
- `ItemBase.OnStoreLoad` (`itembase.c:3120`) использует `GetHierarchyRootPlayer()` (для quickbar
  bind) — у PlayerBase это сам бот, не-null. НЕ identity.
- `PlayerStatBase.OnStoreLoad` (`playerstats/playerstatbase.c:174`) пишет `m_Value`; единственный
  `GetIdentity()` там (строка 111) — для RPC, НЕ для store.

**Devaliada как прямое доказательство**: их `modded PlayerBase.OnStoreSave/OnStoreLoad`
(`OneShot/PlayerBase.c:9/23`) вызывают `super.OnStoreSave(ctx)` / `super.OnStoreLoad(ctx,
version)` **безусловно**, а собственную identity-зависимую часть гейтят
`if (GetIdentity() && ...)`. То есть ванильная цепочка безопасна при `GetIdentity()==null`.

**Готча** (важно): в `OnStoreLoad` (`playerbase.c:7133`) вызов
`if (!GetBleedingManagerServer().OnStoreLoad(ctx, version))` идёт **БЕЗ null-проверки**
(в отличие от `OnStoreSave`, где есть `if (GetBleedingManagerServer())`). `GetBleedingManagerServer()`
определён в `playerbase.c:2773` и не-null на сервере (создаётся в `Init`). На CE-пешке на
dedicated-сервере он не-null → не сломается. Но `[нужно подтвердить]`: на нашем сервере
`GetBleedingManagerServer()` у CE-спавненного `dmAISurvivorBase` не null (ожидается — да,
создаётся в конструкторе под `g_Game.IsServer()`).

## 3. `g_Game.SaveVersion()`

- `proto native int SaveVersion();` — `3_game/global/game.c:434`. Доступен и на сервере, и на
  клиенте (глобал `g_Game`).
- Текущее значение: `static int GAME_STORAGE_VERSION = 142;` (`3_game/global/game.c:5`),
  выставляется через `StorageVersion(GAME_STORAGE_VERSION)` в конструкторе `DayZGame`
  (`game.c:50`). → `g_Game.SaveVersion()` возвращает **142**.
- **Писать ПЕРВЫМ и передавать в `OnStoreLoad`** — так делает devaliada (`OSD_WritePlayerState`
  пишет `g_Game.SaveVersion()`, `OSD_ReadPlayerState` читает и передаёт в `OnStoreLoad`).
  Все optional-секции в `OnStoreLoad` гейтятся `version >= 116/125/131/134/140` — при 142 все
  активны. `PlayerStats.SaveStats`/`LoadStats` сами берут `g_Game.SaveVersion()` внутри.
- PCO-совместимость: `PCOHandlerStats.GetPCO(version)` (`playerstatspco.c:29`) при отсутствии
  точного version берёт ближайший меньший (всегда найдёт `PlayerStatsPCO_current`, версия >100)
  → `LoadStats` не упадёт.

## 4. `$mission:` префикс пути

- **Валиден для FileIO**: `OpenFile` документирует префиксы `'$profile','$saves','$mission'`
  (`ensystem.c:401`). `MakeDirectory` — `ensystem.c:525`. `FileExist` — `ensystem.c:392`.
- `$mission:` **пишется** на сервере: `OpenFile("$mission:Autotest_%1.xml", FileMode.WRITE)`
  (`3_game/autotest/autotestfixture.c:17`). Используется для конфигов
  (`$mission:cfggameplay.json`, `$mission:cfgundergroundtriggers.json`, `$mission:storage_#`).
- Резолвится в папку текущей миссии (на dedicated-сервере `mpmissions/<mission>/` — на диске).
- **Готча (критично)**: `DeleteFile` работает ТОЛЬКО на `"$profile:"` и `"$saves:"`
  (`ensystem.c:527`); `CopyFile` — destName тоже только `$profile:`/`$saves:`
  (`ensystem.c:530`). → **файл под `$mission:` НЕЛЬЗЯ удалить через `DeleteFile`** при смерти
  бота (вопрос 7). Для персистент-каталога с удалением-по-смерти предпочтительнее
  `$profile:dmBotorama/settlement` (консистентно с уже принятым e2e-каналом
  `$profile:dmBotorama/e2e/`, см. decisions.md). `[нужно подтвердить]`: на нашем сервере
  `FileSerializer.Open("$profile:dmBotorama/settlement/<id>", FileMode.WRITE)` + `MakeDirectory`
  работают под обычным runtime-пользователем (ожидается — да, e2e уже пишет в `$profile:`).

## 5. Порядок инициализации / CallLater после CreateObject

**Вывод**: `OnStoreLoad` можно звать сразу после `CreateObject` + `SetPosition` в том же кадре —
все зависимые менеджеры создаются в конструкторе `PlayerBase.Init()` (`playerbase.c:359`, менеджеры
`playerbase.c:414-437`) под `g_Game.IsServer()`. `CreateObject` синхронно конструирует сущность
(`PlayerBase()` → `Init()`), поэтому после возврата `CreateObject` менеджеры уже существуют.

- Devaliada-задержка (`fade` + `CallLater 2500ms`) — для ВОССТАНОВЛЕНИЯ РЕАЛЬНОГО игрока при
  логине (респаун, UI, fade); для CE-бота не требуется.
- Ванильный тайминг косвенно подтверждает: `AfterStoreLoad()` документирован как "Called when
  entity is being loaded from DB or Storage (after all children loaded)" (`entityai.c:1391`) —
  т.е. вызывается сразу после `OnStoreLoad`, а не отложенно.
- **После `OnStoreLoad` нужно явно вызвать `AfterStoreLoad()`** — так делает devaliada
  (`OSD_LoadAdditional` → `OnStoreLoad` → `AfterStoreLoad()`, `OneShot/ItemBase.c`), и
  `PlayerBase.AfterStoreLoad` (`playerbase.c:7196`) триггерит post-load init (damage-system
  reinit, `GetHumanInventory().OnAfterStoreLoad`, `GetPlayerStats().OnAfterStoreLoad`, emote).
- `[нужно подтвердить]` эмпирически: на CE-пешке без Identity вызов `OnStoreLoad` в том же кадре
  после `CreateObject`+`SetPosition` не даёт `Script error`/null-deref в
  `GetBleedingManagerServer().OnStoreLoad` (`playerbase.c:7133`) и `GetPlayerStats().LoadStats`.
  Ручка: после восстановления бот должен встать в сохранённую позицию с сохранённым здоровьём
  и инвентарём (probe через `/bot dump` / `botdump`).

## 6. Позиция/ориентация не входят в OnStoreSave

**Подтверждено**: в теле `PlayerBase.OnStoreSave` (`playerbase.c:7032-7075`) позиция/ориентация
**не пишутся**. Единственная позиция — `ladder.GetLogoutPosition()` для лестницы (спец-кейс).
`OnStoreLoad` читает позицию только для этого лестничного кейса (`playerbase.c:7170-7180`,
`SetPosition(position)`). Базовая позиция/ориентация сущности персистится движком нативно (вне
`OnStoreSave`). → сохранять вручную в кастомном заголовке: `GetPosition()`/`GetWorldPosition()`
+ `GetOrientation()`, восстанавливать `SetPosition()`/`SetOrientation()` (бот уже так делает в
`Spawn()` — `dmAISurvivor.c:251-252`).

## 7. Детект смерти и удаление файла

**Подтверждено**: `dmAISurvivor.OnUpdate` (`src/core/4_World/Entities/Bot/dmAISurvivor.c:466`)
каждый кадр делает `if (!m_Pawn.IsAlive()) { OnDeath(); return; }` (`dmAISurvivor.c:477`).
`IsAlive()` — штатный native-предикат (`entityai.c:511`, `playerbase.c:2330`). `OnDeath()`
(`dmAISurvivor.c:288`) снимает мозг, оставляет труп. → `!m_Pawn.IsAlive()` — корректный триггер
для удаления персистент-файла.

**Готча**: файл можно удалить только если он под `$profile:`/`$saves:` (см. §4). Если каталог
будет `$mission:` — `DeleteFile` не сработает; тогда потребуется перезапись пустым/маркером или
переход на `$profile:`.

## 8. Прочие готчи

- **Гейт `IsServer() && IsMultiplayer()`** внутри `OnStoreSave`/`OnStoreLoad`
  (`playerbase.c:7046`/`7109`): на нашем dedicated-сервере оба true → полная цепочка. На
  singleplayer/offline секции (stats/modifiers/agents/…) будут ПРОПУЩЕНЫ — формат файла
  должен быть симметричен (пишем и читаем в одинаковом окружении).
- **Формат симметричен**: читать ровно то и в том же порядке, что писали (док `entityai.c:2903`
  "format and order of reading must be the same as writing"). Любой рассинхрон → `Read` false →
  `OnStoreLoad` вернёт false.
- **`FileSerializer` требует `Close()`** (или refcount-destructor). `Open` возвращает bool —
  проверять.
- **Инвентарь/экипировка — вручную** (не через OnStoreSave). Полный эталон рекурсивной
  сериализации — `OneShot/ItemBase.c`: `OSD_StoreCargoTo`/`OSD_StoreAttachmentsTo`/`OSD_LoadCargoFrom`/
  `OSD_LoadAttachmentsFrom`, создание предметов через `GetGame().CreateObject(className, Vector(0,0,0))`
  для cargo, `inventory.CreateAttachmentEx(className, slotID)` для attachment,
  `player.GetHumanInventory().CreateInHands(className)` для рук, `TakeEntityToCargoEx(InventoryMode.SERVER,
  item, idx, row, col)`, `GetInventory().SetFlipCargo(flip)`. Здоровье предмета — `GetHealth01()`/
  `SetHealth01("","",value)` (`itembase.c:2318/2556`), quantity — `GetQuantity()`/`SetQuantity()`.
- **`g_Game.SaveVersion()` пишется первым в каждом уровне** (и в заголовке бота, и внутри
  item-level `OnStoreSave` — devaliada пишет `g_Game.SaveVersion()` перед каждым `OnStoreSave`).

## Открытые вопросы / гипотезы

- `[нужно подтвердить]` `GetBleedingManagerServer()` != null у CE-спавненного `dmAISurvivorBase`
  на нашем dedicated-сервере (иначе `OnStoreLoad` упадёт на `playerbase.c:7133`).
- `[нужно подтвердить]` `FileSerializer.Open` + `MakeDirectory` под `$profile:dmBotorama/settlement`
  работают (запись/чтение/удаление) под runtime-пользователем сервера.
- `[нужно проверить]` вызов `OnStoreLoad`+`AfterStoreLoad` сразу после `CreateObject`+`SetPosition`
  (тот же кадр) восстанавливает здоровье/статы/инвентарь без `Script error` — проб: спавн бота →
  сохранить → убить/рестарт → восстановить → `botdump` показал совпадающие здоровье/позицию/инвентарь.
- `[нужно проверить]` выживает ли CE-пешка после восстановления в позиции, занятой другим
  объектом/землёй (нужен ли `SnapToGroundExactly`/сдвиг, если точка стала недостижимой).

## Существенные развилки / решения (для docs/decisions.md)

1. **Каталог персистента: `$profile:` vs `$mission:`** — выбрал бы `$profile:` (удаление по смерти
   через `DeleteFile` работает только на `$profile:`/`$saves:`, консистентно с e2e-каналом).
2. **Скоуп: OnStoreSave покрывает только "state", не здоровье/позицию/инвентарь** — решить, что
   здоровье и инвентарь сериализуем вручную (эталон devaliada `ItemBase.OSD_*`), а не пытаемся
   реиспользовать engine-native save (недоступен через FileSerializer).
3. **`AfterStoreLoad()` вызывать явно после `OnStoreLoad`** (иначе post-load init — damage reinit,
   stats/human-inventory/emote after-load — не выполнится).
