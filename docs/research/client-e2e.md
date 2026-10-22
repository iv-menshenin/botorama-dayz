# Research: клиентская часть E2E-моста (client-e2e)

Цель: собрать точные сигнатуры/поведение API, чтобы сделать **клиентского близнеца**
серверного `dmE2EBridge` (`src/test/4_World/dmE2EBridge.c`). Серверный мост пишет в
`$profile:dmBotorama/e2e/` (in/out/done); клиент должен давать **клиентскую картину**:
синхронизированное состояние бота (пустая рука после дропа, «можно ли связать», упавший
предмет), клиентские райкасты, контейнеры, анимации.

Контекст: клиент-наблюдатель запускается тем же модом через Proton
(`tools/run-client.sh`, appid 221100, `-connect=127.0.0.1:2302 -name=e2e-observer`),
сервер и клиент на ОДНОЙ Linux-машине (общий FS). Бот на клиенте —
`INSTANCETYPE_AI_REMOTE` (удалённая реплика `dmAISurvivorBase : PlayerBase`).

---

## 1. Клиентский хук миссии

### MissionGameplay — это клиентская миссия в MP

`MissionGameplay` инстанцируется **на клиенте**, `MissionServer` — на сервере. Это видно
по содержимому: `MissionGameplay` работает с UI (`m_UIManager`, `m_Hud`, `m_Chat`),
`GetGame().GetPlayer()` и `OnMissionStart` (`g_Game.SetConnecting(false)`).

- `class MissionGameplay extends MissionBase` — `5_mission/mission/missiongameplay.c:1`.
- `override void OnInit()` — `missiongameplay.c:102` (мод уже оверрайдит его в
  `src/core/5_Mission/MissionGameplay.c:5` — лог версии).
- `override void OnMissionStart()` — `missiongameplay.c:186` (после загрузки, `SetMissionState(MISSION_STATE_GAME)`).
- `override void OnMissionFinish()` — `missiongameplay.c:257`.
- `override void OnUpdate(float timeslice)` — `missiongameplay.c:278` (per-frame на клиенте).

### Как отличить клиент от сервера

На уровне миссии/гейма (не сущности):

- `GetGame().IsClient()` — `3_game/global/game.c:1121` (`proto native bool`).
- `GetGame().IsServer()` — `game.c:1122`.
- `GetGame().IsMultiplayer()` — `game.c:1120`.
- `GetGame().IsDedicatedServer()` — `game.c:1127`.

На уровне сущности — `GetInstanceType()` (`enum DayZPlayerInstanceType`,
`3_game/dayzplayer.c:1070-1079`):

```c
INSTANCETYPE_SERVER,   // server instance
INSTANCETYPE_CLIENT,   // client instance && controlled
INSTANCETYPE_REMOTE,   // client instance && other (human) player
INSTANCETYPE_AI_SERVER,
INSTANCETYPE_AI_REMOTE,
INSTANCETYPE_AI_SINGLEPLAYER,
```

Для клиентского моста: **клиентский контролируемый игрок = `INSTANCETYPE_CLIENT`**,
**бот = `INSTANCETYPE_AI_REMOTE`**, чужие люди (если есть) = `INSTANCETYPE_REMOTE`.

### Конвенция `#ifdef` в модовом коде

Мод НЕ использует `#ifdef CLIENT` (в ванили DayZ `CLIENT` **не** является встроенным
define — стандарт `#ifdef SERVER` / `#ifndef SERVER`). Проверено: `grep -rn "#ifdef CLIENT"
src/` → 0 совпадений. Примеры в моде:
- `dmAISurvivorBase.OnVariablesSynchronized()` — обёрнуто `#ifndef SERVER`
  (`src/core/4_World/Entities/Bot/dmAISurvivorBase.c:905`).
- `TickTalking()` — `#ifdef SERVER` (`dmAISurvivorBase.c:1111`).

### Правильный per-tick хук на клиенте

**НЕ `MissionGameplay.OnUpdate`**, а **саморасписающийся CallLater** — тот же паттерн,
что у серверного `dmE2EBridge.E2ETick()` (`dmE2EBridge.c:186-239`):
`GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(..., interval, false)` +
пере-расписание в конце тика. Причина (из комментария в `dmE2EBridge.c:136-139`):
в 1.30 **два modded-блока `MissionServer`, оба оверрайдящие `OnUpdate`, тихо роняют
один из оверрайдов**. Тот же риск у `MissionGameplay.OnUpdate` (если другой загруженный
мод — CF/COT — тоже оверрайдит его). CallLater — изолированный канал, не зависящий от
порядка override. Плюс это даёт контролируемую каденцию (серверный мост: 1 тик = 1 c).

## 2. Файловый I/O на клиенте

### Что резолвит `$profile:` на клиенте

Сервер использует `-profiles=./profiles-cherno` → `$profile:` =
`/mnt/deep-space/Steam/steamapps/common/DayZServer/profiles-cherno`
(`docs/ai-testing-guide.md:151`, `tools/run-scenario.sh:15`).

Клиент (`tools/run-client.sh`) **НЕ передаёт `-profiles=`**, поэтому `$profile:` резолвится
в дефолтный профильный каталог клиента DayZ внутри Wine-префикса compatdata 221100:
`C:\users\steamuser\Documents\DayZ\` → Linux
`/home/devalio/.local/share/Steam/steamapps/compatdata/221100/pfx/drive_c/users/steamuser/Documents/DayZ/`.
Свежий префикс (`tools/_run_observer_fresh_pfx.sh`, `STEAM_COMPAT_DATA_PATH=/tmp/opencode/dayz-obs-pfx`)
→ то же самое под `/tmp/opencode/dayz-obs-pfx/pfx/drive_c/users/steamuser/Documents/DayZ/`.

`[нужно проверить]` — точный путь: DayZ-клиент может класть профиль не в корень
`Documents/DayZ/`, а в подкаталог по имени профиля (напр. `Documents/DayZ/e2e-observer/`)
или по-другому. Эмпирический вопрос: **после запуска `tools/run-client.sh` и одного
клиентского `MakeDirectory("$profile:dmBotorama/e2e-client/")` — где именно на Linux
появился каталог (find по `/home/devalio/.local/share/Steam/steamapps/compatdata/221100/pfx`
и `/tmp/opencode/dayz-obs-pfx` на `dmBotorama`)?** Точно ли `$profile:` совпадает с тем
каталогом, где клиент пишет `.RPT`-лог (тот лежит рядом с профилем)?

### Работают ли файловые API на клиенте

`MakeDirectory` / `CopyFile` / `DeleteFile` / `FileExist` / `OpenFile` /
`FindFile`/`FindNextFile`/`CloseFindFile` — это **движковые** функции файловой системы
(`enSystem.c`), не сервер-гейтнутые. Подтверждено живыми на 1.30
(`docs/research/1.30-breaking-changes.md §4`: `FindFile` ensystem.c:520-522,
`MakeDirectory` :525, `FileExist` :397, `JsonFileLoader` tools/jsonfileloader.c:1,
`JsonSerializer` gameplay.c:49). Мод уже использует их на сервере:
`JsonFileLoader<dmE2EResult>.SaveFile`, `CopyFile`, `DeleteFile`, `EnsureDir` →
`MakeDirectory` (`dmE2EBridge.c:3140-3185`). Те же вызовы должны работать на клиенте.

Готчи:
- **Proton/Wine-маппинг**: изнутри клиента `Z:\` = корень Linux `/`. Поэтому абсолютный
  путь вида `Z:\mnt\deep-space\Steam\steamapps\common\DayZServer\...` попадает в общий с
  сервером каталог (это уже использует `run-client.sh:36` для `-mod=`).
- **`$profile:`-префикс** мод обрабатывает сам (не движок): хелперы `EnsureDirectory`
  (`dmJsonFile.c:108-131`), `EnsureDir` (`dmE2EBridge.c:3142-3165`),
  `dmRoadGapIO.c:19-36` и др. сплитят путь по `/`, ловят первый компонент `"$profile:"`
  и строят цепочку `MakeDirectory`. Если клиентский мост пишет в `$profile:`, этот же
  хелпер обязан быть и на клиенте (он в `cons/3_Game` — грузится в обе стороны).

### Абсолютный путь vs свой `$profile:` каталог

Оба варианта технически возможны (Z: = `/` даёт общий FS):

1. **Свой `$profile:dmBotorama/e2e-client/`** (рекомендую). Клиент пишет в собственный
   профиль, тестер читает через compatdata-путь (`.../pfx/drive_c/.../Documents/DayZ/dmBotorama/e2e-client/`).
   Минус — тестеру нужно знать точный Linux-путь (см. `[нужно проверить]` выше).
2. **Абсолютный общий путь** (`Z:\mnt\deep-space\...\DayZServer\profiles-cherno\dmBotorama\e2e-client\`).
   Прямо читается тестером по серверному пути, но жёстко завязывает клиент на layout
   хоста и требует правки `EnsureDirectory` (он понимает только `$profile:`-префикс,
   но умеет строить цепочку и из абсолютного пути — там `startFrom=0`).

`[нужно проверить]` — эмпирически: **может ли клиент под Proton реально писать в
абсолютный `Z:\mnt\deep-space\...` путь (права/буферы Wine), и не ломается ли
`MakeDirectory` на бэкслеш-стиле `Z:\`?** (модные хелперы ожидают `/`-разделитель;
напр. `path.Substring(...).Split("/", ...)`). Проще и надёжнее держать клиентский канал
на `$profile:` с `/`-разделителями.

## 3. Доступ к состоянию бота с клиента (КЛЮЧЕВОЕ)

### Что net-sync'ится на AI_REMOTE и читаемо клиентом

| Состояние | Механизм | Сигнатура (файл:строка) |
|---|---|---|
| нокаут | `RegisterNetSyncVariableBool("m_IsUnconscious")` | `playerbase.c:575`; `IsUnconscious()` `playerbase.c:3655` |
| связан | `RegisterNetSyncVariableBool("m_IsRestrained")` | `playerbase.c:576`; `IsRestrained()` `playerbase.c:2040` |
| предмет в руках | реплицируемое инвентарное дерево | `GetItemInHands()` `playerbase.c:6437` = `ItemBase.Cast(GetEntityInHands())` (`humaninventory.c:16`) |
| позиция/ориентация | transform-репликация | `GetPosition()`/`GetOrientation()` |
| поза/команда | реплицируемое анимационное состояние | `GetMovementState(state)` → `m_iStanceIdx` / `m_CommandTypeId` |
| головной взгляд/голос (кастомные) | `RegisterNetSyncVariableFloat/Int` в конструкторе `dmAISurvivorBase` | `dmAISurvivorBase.c:198-202`; читаются в `OnVariablesSynchronized()` под `#ifndef SERVER` (`:905`) |

`IsUnconscious()` = `m_MovementState.m_CommandTypeId == COMMANDID_UNCONSCIOUS || m_IsUnconscious`
(`playerbase.c:3655-3658`). На клиенте оба слагаемых заполняются: `m_IsUnconscious` — из
net-sync (серверный `UpdateUnconsciousBridge` делает `m_IsUnconscious = true; SetSynchDirty()`,
`dmAISurvivorBase.c:1198-1199`), `m_MovementState.m_CommandTypeId` — из реплицируемой
анимации (команда `COMMANDID_UNCONSCIOUS` сама по себе едет клиенту как поза).

### `GetActionManager()` у AI_REMOTE — null (и следствие для CanBeRestrained)

`m_ActionManager` создаётся ТОЛЬКО в `OnSelectPlayer()` (`playerbase.c:6014`) и только для
трёх типов:

```c
// playerbase.c:6083-6097
if (GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
    m_ActionManager = new ActionManagerServer(this);
else if (GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_CLIENT)
    m_ActionManager = new ActionManagerClient(this);
else if (GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SINGLEPLAYER)
    m_ActionManager = new ActionManagerServer(this);
```

`INSTANCETYPE_AI_REMOTE` сюда **не входит**, а `OnSelectPlayer()` у AI_REMOTE вообще не
вызывается → `m_ActionManager == NULL` (инициализируется NULL в `playerbase.c:460`).

**Следствие (КЛЮЧЕВАЯ развилка клиент/сервер):**

Ванильный `CanBeRestrained()` (`playerbase.c:1999-2011`) требует
`!GetActionManager() || GetActionManager().GetRunningAction() != null` → `false`:

```c
bool CanBeRestrained()
{
    if (IsInVehicle() || IsRaised() || IsSwimming() || IsClimbing() || IsClimbingLadder()
        || IsRestrained() || !GetWeaponManager() || GetWeaponManager().IsRunning()
        || !GetActionManager() || GetActionManager().GetRunningAction() != null || IsMapOpen())
        return false;
    ...
}
```

Модный `modded CanBeRestrained()` (`src/core/4_World/modded/modded_Restrain.c:14-24`)
обходит гейт ТОЛЬКО для `INSTANCETYPE_AI_SERVER`:

```c
override bool CanBeRestrained()
{
    if (GetInstanceType() != DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
        return super.CanBeRestrained();   // <-- клиент AI_REMOTE попадает СЮДА
    ...
}
```

Поэтому **на клиенте вызов `bot.CanBeRestrained()` у AI_REMOTE вернёт `false` всегда**
(из-за `!GetActionManager()`), даже если сервер (модный override для AI_SERVER) вернёт
`true`. Т.е. «можно ли связать» **глазами клиента НЕ проверяется через `CanBeRestrained()`** —
это сервер-авторитетное решение, а не клиентский наблюдаемый факт.

Клиентский мост должен проверять **наблюдаемые** предикаты, которые видит и живой игрок:
`IsRestrained()` (net-var), `IsUnconscious()` (net-var), `IsRaised()` (у бота — override в
`dmAISurvivorBase.c:501` читает СЕРВЕРНЫЙ `m_WeaponRaised`, который на клиенте НЕ синкается
через net-var → на клиенте `m_WeaponRaised` всегда false; см. `[нужно проверить]` ниже),
позу/предмет в руках. Если нужен именно серверный вердикт «можно связать» — его должен
**задамплить серверный мост** (добавить в `botdump`/`snapshot`), а клиент лишь сверяет
свои наблюдаемые поля.

`[нужно подтвердить]` — что `m_WeaponRaised` на клиенте действительно всегда `false`
(серверный `RaiseWeapon()` пишет его в `CommandHandler`, который у AI_REMOTE на клиенте
не бежит; net-var на него нет). Эмпирический вопрос: **на клиенте `bot.IsRaised()` у
AI_REMOTE после серверного `RaiseWeapon(true)` — false?** (если так, «поднятое оружие»
клиент может судить только косвенно по `m_iStanceIdx`/анимации, а не по `IsRaised()`).

### Мозга `dmAISurvivor` на клиенте НЕТ

`dmAISurvivor` (мозг) — обычный `class`, создаётся ТОЛЬКО на сервере (`dmE2EBridge.RunSpawn`:
`new dmAISurvivor()` → `bot.Spawn(...)`), не является игровой сущностью → не реплицируется.
Клиентский мост **не может** звать `dmAISurvivor.Find(pawn)` (вернёт null на клиенте) и не
видит FSM/интенты/мозговые флаги. Вся клиентская работа — **только по пешке**
`dmAISurvivorBase` (реплицируемый `PlayerBase`).

### Как клиенту найти конкретного бота / упавший предмет

- **Реестр**: `dmEntityRegistry.GetPlayers()` работает и на клиенте — `modded PlayerBase`
  регистрирует каждую сущность в конструкторе (`src/reg/4_World/modded_PlayerBase.c:3-11`),
  модуль `reg/4_World` входит в клиентский worldScriptModule (`src/reg/config.cpp`).
  Фильтр (из `docs/decisions.md`): бот = `dmAISurvivorBase.Cast(p) != null`,
  человек = `== null`.
- **Имена ботов НЕ реплицируются**: у AI нет `PlayerIdentity`/character id
  (`modded DayZPlayerImplement.GetHive()` возвращает null для AI_SERVER — нет id → нет имени).
  Поэтому клиент **не может найти бота по имени**. Находить по:
  (a) ближайший `dmAISurvivorBase` к известной позиции спавна из job'а;
  (b) перечислить всех ботов реестра и коррелировать по позиции/порядку с серверным дампом.
  `[нужно проверить]` — стабилен ли порядок `dmEntityRegistry.GetPlayers()` на клиенте,
  чтобы сопоставить «бот N на клиенте» с «бот N на сервере» по индексу/позиции.
- **Упавший на землю предмет**: `GetGame().GetObjectsAtPosition(pos, radius, out array<Object> objects, out array<CargoBase> proxyCargos)` (`3_game/global/game.c:922`) или 3D-вариант `GetObjectsAtPosition3D` (`game.c:929`); либо бокс-скан `DayZPlayerUtils.SceneGetEntitiesInBox(min, max, out array<EntityAI>, flags)` (`4_world/entities/dayzplayerutils.c:75`, флаги `QueryFlags.DYNAMIC/STATIC`).
  Предикат «лежит на земле»: `item.GetHierarchyRootPlayer() == null` (`3_game/entities/entityai.c:877`). «В руках»: `bot.GetItemInHands() == item` или `item.GetHierarchyRootPlayer() == bot`.

## 4. Клиентские проверки

### Клиентский raycast (LOS)

Тот же API, что на сервере, доступен на клиенте (класс `DayZPhysics` в
`3_game/global/dayzphysics.c` — модуль `3_game`, грузится в обе стороны):

- `DayZPhysics.RaycastRV(begPos, endPos, out contactPos, out contactDir, out contactComponent, set<Object> results=NULL, Object with=NULL, Object ignore=NULL, bool sorted=false, bool ground_only=false, int iType=ObjIntersectView, float radius=0.0, CollisionFlags flags=CollisionFlags.NEARESTCONTACT)` — `dayzphysics.c:199`.
- `DayZPhysics.RaycastRVProxy(RaycastRVParams in, out array<ref RaycastRVResult> results, array<Object> excluded=null)` — `dayzphysics.c:208` (его использует серверный мост `dmE2EBridge.RunRaycast`).

Клиентское использование в ванили: `5_mission/gui/cameratools/cameratoolsmenu.c:538,557`,
`4_world/classes/weapondebug.c:326` — райкаст из камеры/глаз.

**Отличие от сервера**: геометрия. Клиент стреляет по **view/LOD-геометрии**
(`ObjIntersectView` — дефолт; то, что видит игрок), сервер — по **физической/симуляционной**
(`ObjIntersectGeom` для препятствий, см. `docs/decisions.md` про детект сот/деревьев).
Листва/LOD на клиенте может давать ложные хиты, физика на сервере — нет; и наоборот.
Поэтому «видит ли игрок бота» (клиент) и «есть ли физическое препятствие» (сервер) — разные
вопросы. `[нужно проверить]` эмпирически: **один и тот же raycast A→B — сходится ли
клиентский (view) и серверный (geom/fire) результат по числу/порядку хитов на характерных
препятствиях (забор, перила лестницы, куст, стена)?**

### Контейнер/инвентарь предмета с клиента

Инвентарь — реплицируемое дерево, читаемо на клиенте: `item.GetInventory()`,
`GetCargo()`, `EnumerateInventory(InventoryTraversalType.INORDER, items)`,
`GetHierarchyRootPlayer()`. Проверка «на земле» vs «в руках» — через `GetHierarchyRootPlayer()`
(см. §3). Наблюдаемая картина у клиента может расходиться с сервером на кадр-два из-за
задержки репликации дропа (серверный `DropItem` ре-синкает предмет через
`RemoteObjectTreeDelete/Create` с `CallLater 34 ms` — `dmAISurvivorBase.c:2183-2201`).

### Анимация/поза бота с клиента

`GetMovementState(state)` → `HumanMovementState` заполняется каждый кадр и на клиенте из
реплицируемого анимационного состояния:
- `m_iStanceIdx` — поза (ERECT/CROUCH/PRONE/RAISED*; нормализовать `- STANCEIDX_RAISED` при
  `>= STANCEIDX_RAISED`, как в `dmAISurvivorBase.ApplyStance`, `dmAISurvivorBase.c:1564-1567`).
- `m_CommandTypeId` — текущая команда (`DayZPlayerConstants.COMMANDID_MOVE/MELEE2/UNCONSCIOUS/DEATH/VEHICLE/LADDER/...`).
- `GetCurrentCommandID()` — альтернатива.

Это основной источник «что делает бот» глазами клиента (идёт/бьёт/без сознания/в машине).

---

## Минимальный рецепт клиентского моста

1. **Хук**: `modded class MissionGameplay { override void OnInit() { super.OnInit(); <singleton>.Get().Start(); } }`
   (точечно дополнить существующий `src/core/5_Mission/MissionGameplay.c`), а per-tick —
   **CallLater** (`CALL_CATEGORY_SYSTEM`), как `dmE2EBridge.E2ETick` (`dmE2EBridge.c:186-239`):
   самопере-расписание + гейт на маркер-файл `$profile:dmBotorama/e2e-client/enabled`.
2. **Канал**: читать `$profile:dmBotorama/e2e-client/in/job.json`, писать
   `$profile:dmBotorama/e2e-client/out/<id>.client.json` через `JsonFileLoader` +
   `EnsureDirectory` (копия хелпера `dmJsonFile.EnsureDirectory`). Тестер читает через
   compatdata-путь (см. §2). Если хочется общий канал — тот же `$profile:` сервера через
   `Z:\`-абсолют, но надёжнее собственный каталог.
3. **Дамп состояния бота с клиента** (по реестру, без мозга):
   ```
   dmEntityRegistry.GetPlayers() → фильтр dmAISurvivorBase.Cast(p) != null →
   для каждого бота:
     pos = p.GetPosition(); yaw = p.GetOrientation()[0];
     alive = p.IsAlive(); unconscious = p.IsUnconscious(); restrained = p.IsRestrained();
     hands = p.GetItemInHands(); handsType = hands ? hands.GetType() : "";
     GetMovementState(ms); stance = ms.m_iStanceIdx (нормализовать); cmd = ms.m_CommandTypeId;
   ```
   НЕ звать `CanBeRestrained()`/`GetActionManager()` (null на AI_REMOTE) и НЕ звать
   `dmAISurvivor.Find(...)` (мозга нет). Имя бота — по ближайшей позиции к spawn-точке из job'а.
4. **Клиентские пробы**: raycast (`DayZPhysics.RaycastRVProxy`, view-геометрия), scanbox
   (`DayZPlayerUtils.SceneGetEntitiesInBox` или `GetGame().GetObjectsAtPosition`), предмет
   на земле (`GetHierarchyRootPlayer() == null`), поза (`m_iStanceIdx`/`m_CommandTypeId`).

---

## Список `[нужно проверить]`

1. **Точный Linux-путь `$profile:` клиента** (Proton compatdata 221100 / свежий
   `/tmp/opencode/dayz-obs-pfx`): где физически появляется `dmBotorama/e2e-client/` после
   клиентского `MakeDirectory("$profile:dmBotorama/e2e-client/")`? Совпадает ли с каталогом
   клиентского `.RPT`-лога?
2. **Пишет ли клиент под Proton в абсолютный `Z:\mnt\deep-space\...`** путь (права/буферы
   Wine), и не ломается ли `MakeDirectory` на `Z:\`-префиксе (хелперы ждут `/`-разделитель).
3. **Клиентский `IsRaised()` у AI_REMOTE** — всегда `false`? (серверный `m_WeaponRaised`
   не синкается net-var'ом; `CommandHandler` у AI_REMOTE на клиенте не бежит).
4. **Расхождение raycast клиент (view) vs сервер (geom/fire)** на характерных препятствиях
   (забор, перила лестницы, куст, стена): число/порядок хитов.
5. **Порядок `dmEntityRegistry.GetPlayers()` на клиенте** — стабилен ли, чтобы сопоставить
   «бот N» клиента с «бот N» сервера (по индексу/позиции) без имён.
6. **Задержка репликации дропа** — сколько кадров/тиков клиент продолжает видеть предмет в
   руках после серверного `DropItem` (сервер ре-синкает через `CallLater 34ms`); нужно ли
   клиентскому wait'у дебаунс перед проверкой «рука пуста».
7. **`$profile:`-логин**: клиентский `.RPT`/файловый вывод доступен ли тестеру напрямую
   (через compatdata-путь), или надёжнее весь результат — в JSON-файле клиентского канала.

## Существенные развилки и принятые решения (для docs/decisions.md)

1. **Клиентский per-tick — CallLater, а не `MissionGameplay.OnUpdate`** (по образцу
   серверного моста): избегает 1.30-готчи «два modded-оверрайда OnUpdate тихо роняют один».
2. **«Можно ли связать» на клиенте — НЕ `CanBeRestrained()`** (у AI_REMOTE `GetActionManager()==null`
   → всегда false; модный override обходит гейт только для AI_SERVER). Клиент проверяет
   наблюдаемые net-вары (`IsRestrained`/`IsUnconscious`/поза/руки), серверный вердикт —
   отдельным дампом серверного моста.
3. **Клиентский мост — только по пешке** (`dmAISurvivorBase`), без мозга `dmAISurvivor`
   (на клиенте его нет) и без имён ботов (не реплицируются) — идентификация по позиции/реестру.
4. **Канал клиента — свой `$profile:dmBotorama/e2e-client/`**, а не общий абсолютный путь
   (общий Z:-путь возможен, но завязка на layout хоста и `\`-разделители хрупче).
