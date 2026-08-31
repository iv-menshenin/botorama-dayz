# Research: навигация (vault / doors / ladders)

Статус: база navmesh готова (`dmBotPathfinder`, `FindPath`/`SamplePosition`).
Задачи T7/T8/T9 плана. Ведёт субагент `dayz-research`.

## Цель

Довести `MoveTo` до прохождения препятствий: перепрыгивание/влезание (vault/climb),
открывание дверей, подъём/спуск по лестницам.

## Ключевая развилка архитектуры (читать первым)

Expansion НЕ кладёт всю навигацию в `FindPath`. Он строит путь нативным
`AIWorld.FindPath` (обёрнутый в `ExpansionPathHandler`), но обнаружение/отработку
препятствий делает **в момент следования**, на каждый сегмент пути:

1. `ExpansionPathHandler.UpdatePathSegmentState()` (pathfinding/ExpansionPathHandler.c:1050)
   ставит флаги сегмента: `m_IsBlocked` (raycast через `m_BlockFilter` — детектит
   двери/клаймб), `m_IsBlockedPhysically` (физический raycast — детектит препятствие,
   которого нет в navmesh), `m_IsJumpClimb` (сегмент требует vault/climb).
2. `eAICommandMove.AvoidObstacles()` (Commands/eAICommandMove.c:645) — физический
   обход «в лоб» (влево/вправо/назад/разворот) + определение `m_BlockingObject`
   (здание с дверью / с лестницей).
3. `eAIBase.CommandHandler()` (Entities/AI/eAIBase.c:7313-7536) — каскад
   «лестница → дверь → vault/climb → движение» в каждом тике.

У botorama своего `ExpansionPathHandler` нет — есть тонкая обёртка `dmBotPathfinder`
(`FindPath`/`SamplePosition`) + следование по waypoint'ам в `dmBotIntent_MoveTo`.
Вывод для маппинга: детект препятствия придётся делать самим (raycast на сегменте
между текущим и следующим waypoint'ом), а НЕ рассчитывать на флаги waypoint'ов,
которых `AIWorld.FindPath` не возвращает.

---

## Двери (Expansion/ваниль)

### API / как открыть серверно

**Ваниль** (`3_game/entities/building.c`, класс `Building`):

- `proto native int    GetDoorIndex(int componentIndex);` — индекс двери по индексу
  компонента (view geometry).
- `proto native int    GetDoorCount();`
- `proto native bool   IsDoorOpen(int index);` / `IsDoorOpening` / `IsDoorOpened` /
  `IsDoorClosing` / `IsDoorClosed` — фаза/желаемая фаза анимации двери.
- `proto native bool   IsDoorLocked(int index);`
- `proto native void   OpenDoor(int index);` / `CloseDoor(int index);`
- `proto native vector GetDoorSoundPos(int index);`
- `CanDoorBeOpened(int doorIndex, bool checkIfLocked = false)` (building.c:130) —
  **готча**: при `checkIfLocked=true` возвращает `false` если дверь закрыта И заперта;
  при `false` (дефолт) — `false` если закрыта И НЕ заперта (инвертированная логика).
- `CanDoorBeClosed(int)` = `IsDoorOpen(index)`.
- `GetLockCompatibilityType(int doorIdx)` = `1 << EBuildingLockType.LOCKPICK` по умолчанию.

**Как открывает клиентский игрок** (`4_world/classes/useractionscomponent/actions/
interact/actionopendoors.c`): `ActionOpenDoors.OnStartServer` → `building.OpenDoor(doorIndex)`.
Это **клиентский** action — для серверного ИИ он не нужен.

**Как открывает Expansion ИИ** (`eAIBase.HandleBuildingDoors`, eAIBase.c:11497):

1. Raycast вперёд: `position = m_ExTransformPlayer[3] + (m_ExTransformPlayer[1]*1.1)`,
   `p1 = position + direction * 1.5 * clamp(speed,1.0,1.333) * fwdBwd` (fwdBwd=-1 при
   backpedal), `DayZPhysics.RaycastRVProxy(params, results, excluded)` с `radius=0.5`,
   `ObjIntersectView`.
2. `Class.CastTo(building, result.obj)` → `doorIndex = building.GetDoorIndex(result.component)`.
   (`doorIndex == -1` — спец-случаи: подземные входы `Expansion_IsUndergroundEntrance()`.)
3. `isDoorOpen = building.IsDoorOpen(doorIndex)`, `isEnterable = building.Expansion_IsEnterable()`.
   - дверь открыта + проходимо → НЕ открывать (если долго блокирует — `ForceRecalculate`).
   - дверь закрыта + `CanDoorBeOpened(doorIndex, true)` → `StartActionObject(eAIActionOpenDoorsDummy, building)` +
     **`building.OpenDoor(doorIndex)`** (натив, без action-менеджера), затем через 34 мс
     `m_PathFinding.ForceRecalculate(true)`.
   - заперта + `GetAllowDamage()` + lockpickable → создаёт `eAIDoorTargetInformation`
     (цель «взломать/расстрелять дверь»), НЕ открывает.
4. Кулдауны: `m_eAI_LastDoorInteractionTime` (этот ИИ, 1000 мс) +
   `building.m_eAI_LastDoorInteractionTime[doorIndex]` (это здание, `timeTresh+500` мс),
   где `timeTresh = building.eAI_GetDoorAnimationTime(doorIndex) * 1000` (из
   `CfgVehicles ... Doors animPeriod`).

**Итог для botorama**: чтобы «пройти сквозь дверь», боту нужны только
`building.OpenDoor(doorIndex)` + «подождать анимацию открытия» (`eAI_GetDoorAnimationTime`
или просто `IsDoorOpened`) + `ForceRecalculate`/продолжить путь. ActionManager/действия
НЕ нужны — `OpenDoor` это серверный натив здания. `StartActionObject(...Dummy...)` в
Expansion — только ради hand-анимации; как её вернуть серверному ИИ без ActionManager —
см. подраздел «Hand-анимация открывания двери» ниже.

### Hand-анимация открывания двери

#### API (StartActionObject / action-класс / анимация)

**Корневой механизм**: hand-жест «взяться за ручку/толкнуть» играется НЕ через
`Building.OpenDoor` (это анимация створки, серверный натив здания), а через нативную
анимационную команду `Human`:

- `Human.StartCommand_Action(int pActionID, typename pCallbackClass, int pStanceMask)` —
  полнокадровая команда действия (`human.c:1533`).
- `Human.AddCommandModifier_Action(int pActionID, typename pCallbackClass)` —
  **аддитивный** (upper-body/hand) модификатор поверх `COMMANDID_MOVE` (`human.c:1563`,
  комментарий «modifier/additive actions - played on COMMANDID_MOVE command»).
- Контроль: `GetCommand_Action()` (`human.c:1536`), `GetCommandModifier_Action()`
  (`human.c:1569`), принудительное удаление `DeleteCommandModifier_Action(cb)`
  (`human.c:1566`).
- `pActionID` для открывания двери = `DayZPlayerConstants.CMD_ACTIONMOD_OPENDOORFW = 506`
  (`3_game/dayzplayer.c:774`, секция «onetime», метка `// erc,cro`).

`pActionID` попадает в граф анимаций как значение команды `CMD_Action`: переход
`GetCommandI(CMD_Action) == 506` → состояние `OpenDoorErc` (стойка) / `OpenDoorCro`
(присед). Жест — **одноразовый** (ActionOnceSTM, noloop), завершается сам, модификатор
автоматически удаляется (для one-shot `DeleteCommandModifier_Action` обычно не нужен).

**`StartActionObject` — это НЕ ванильный API**, а helper на `eAIBase` (не на
`Human`/`DayZPlayerImplement`):

- `ActionBase StartActionObject(typename actionType, Object target, ItemBase mainItem = null)`
  (`eAIBase.c:9812`) → строит `ActionTarget(target, null, -1, vector.Zero, -1.0)` →
  `StartAction(actionType, actionTgt, mainItem)` (`eAIBase.c:9798`) →
  `m_eActionManager.PerformActionStart(action, target, mainItem)` (`eAIBase.c:9805`).
- Опирается на СОБСТВЕННЫЙ менеджер действий `eAIActionManager : ActionManagerBase`
  (`eAIActionManager.c:15`), которым eAIBase подменяет ванильный `m_ActionManager`
  (`eAIBase.c:735-736`: `m_eActionManager = new eAIActionManager(this); m_ActionManager = m_eActionManager;`).
- `PerformActionStart` (`eAIActionManager.c:252`) кладёт action в `m_PendingActionData`,
  а `Update()` (`eAIActionManager.c:82-98`) в следующем тике делает
  `m_CurrentActionData.m_Action.Start(...)`.
- **Слот/команду движения `StartActionObject` сам не занимает** — её занимает уже
  `AnimatedActionBase.Start()` → `CreateAndSetupActionCallback`, который зовёт
  `AddCommandModifier_Action` (аддитив) либо `StartCommand_Action` (полнокадр).
- Отменяется как обычное действие: `m_CurrentActionData.m_Action.Interrupt(...)` →
  `callback.Cancel()` (натив `HumanCommandActionCallback.Cancel`, `human.c:318`).

**Ванильная цепочка действия** (`actionopendoors.c` + `actionbase.c` + `animatedactionbase.c`):

- `ActionOpenDoors : ActionInteractBase` (`actionopendoors.c:1`);
  `ActionInteractBase : AnimatedActionBase` (`actioninteractbase.c:38`).
- В конструкторе `m_CommandUID = CMD_ACTIONMOD_OPENDOORFW` и
  `m_StanceMask = CROUCH|ERECT` (`actionopendoors.c:7-8`).
- `m_FullBody` остаётся `false` (дефолт из `ActionBase()`, `actionbase.c:92`) → в
  `AnimatedActionBase.CreateAndSetupActionCallback` (`animatedactionbase.c:324-342`)
  выбирается **аддитивная** ветка:
  `player.AddCommandModifier_Action(GetActionCommandEx(action_data), GetCallbackClassTypename())`
  (`animatedactionbase.c:335`), где `GetActionCommandEx` = `m_CommandUID` = 506.
- `OnStartServer` → `building.OpenDoor(doorIndex)` (`actionopendoors.c:53-69`) — это и есть
  функциональное открытие створки; hand-жест к нему отношения не имеет.
- Callback `ActionInteractBaseCB.EndActionComponent` шлёт `SetCommand(CMD_ACTIONINT_END)`
  (`actioninteractbase.c:31`) — завершение (для one-shot не критично).

**Разделение анимаций (сервер-авторитет vs клиент)**:

- створка = `Building.OpenDoor(int)` — натив здания, крутится на сервере, синхронизируется.
- жест = `AddCommandModifier_Action(506, cb)` — анимационная команда `Human`; граф
  (`CMD_Action == 506`) играет `OpenDoorErc/OpenDoorCro` на сервере, состояние синхронизируется
  клиентам (как и `AnimSetFloat`/`AnimCallCommand`, уже используемые ботом).
- Ни то, ни другое НЕ требует `ActionManager`/`EMoteManager`: команда — нативный примитив
  `DayZPlayerImplement`.

#### Как это делает Expansion (eAIActionOpenDoorsDummy + HandleBuildingDoors)

**Класс-заглушка** (`AI/.../Actions/UserActionsComponent/Actions/Interact/eaiactiondoorsdummy.c`):

```c
//! Only needed for hand animation
class eAIActionOpenDoorsDummy: ActionOpenDoors
{
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        return false;   // только ради hand-анимации: НЕ показывать в UI игрока
    }

    override void OnStartServer(ActionData action_data)
    {
        // пусто: дверь открывается отдельно вызовом building.OpenDoor()
    }
}
```

- База = `ActionOpenDoors`, поэтому `m_CommandUID = 506` и вся анимационная машинерия
  (`AddCommandModifier_Action(506, ActionInteractBaseCB)`) наследуются.
- `ActionCondition() → false` нужно только чтобы клиентское action-меню игрока не видело
  этот action; для серверного ИИ это НЕ проверяется (action стартуется напрямую через
  `PerformActionStart` → `Start()`, минуя `Can()`/`StartDeliveredAction`).
- `OnStartServer` пуст: само действие НЕ вызывает `OpenDoor`, иначе был бы двойной вызов
  (дверь открывает `HandleBuildingDoors` отдельно). `OnFinishServer` не переопределён.

**Порядок в `HandleBuildingDoors`** (`eAIBase.c:11497-11742`), ветка «закрыта и можно открыть»:

1. (`eAIBase.c:11688-11693`) `if (!isDoorOpen && !m_eAI_Halt) { eAI_SetHalt(true); CallLater(eAI_SetHalt, timeTresh*0.65, false, false); }`
   — кратковременная остановка, чтобы створка не толкнула ИИ.
2. (`eAIBase.c:11717-11724`) `StartActionObject(eAIActionOpenDoorsDummy, building);` **потом**
   `if (canInteract || building.IsDoorLocked(doorIndex)) building.OpenDoor(doorIndex);`
   — жест стартует, затем натив открывает створку (в одном тике).
3. (`eAIBase.c:11726-11729`) `CallLater(m_PathFinding.ForceRecalculate, 34, false, true)` —
   пересчёт пути через 34 мс.
4. Кулдауны (`eAIBase.c:11736-11737`): `building.m_eAI_LastDoorInteractionTime[doorIndex]`
   (per-дверь, `timeTresh + 500` мс) и `m_eAI_LastDoorInteractionTime` (этот ИИ, 1000 мс),
   где `timeTresh = building.eAI_GetDoorAnimationTime(doorIndex) * 1000`
   (`BuildingBase.c:206` — читает `CfgVehicles ... Doors ... animPeriod`).

#### Минимальный путь для botorama (что добавить, порядок вызовов, риски)

**Вывод**: полноценный dummy-action + свой ActionManager НЕ нужны. У бота
`dmAISurvivorBase : PlayerBase` (`INSTANCETYPE_AI_SERVER`) `m_ActionManager == NULL`
(`playerbase.c:460` выставляет NULL, а `ActionManagerServer` создаётся только для
`INSTANCETYPE_SERVER`/`AI_SINGLEPLAYER`, `playerbase.c:6081-6098` — `AI_SERVER` туда НЕ
входит). Воспроизводить `eAIActionManager` (наследование от `ActionManagerBase` + подмена
`m_ActionManager`) — избыточно ради одного жеста.

Вместо этого — дёрнуть нативный примитив напрямую. Что добавить:

1. Тривиальный callback-класс (натив требует typename `HumanCommandActionCallback`):
   ```c
   class dmBotActionAnimCB : HumanCommandActionCallback {}
   ```
   (пустой достаточно; при желании в деструкторе — `GetHuman()` →
   `PlayerBase.Cast(...).RequestHandAnimationStateRefresh()` для ре-синка рук, как это
   делает `EmoteCB` в `emotemanager.c:8-17`).

2. В пешке примитив (аналог эффекта `eAIActionOpenDoorsDummy` без action-менеджера):
   ```c
   void PlayDoorOpenGesture()
   {
       if (GetCommandModifier_Action() || GetCommand_Action())
           return;   // уже играется какое-то действие — не наслаивать
       AddCommandModifier_Action(DayZPlayerConstants.CMD_ACTIONMOD_OPENDOORFW, dmBotActionAnimCB);
   }
   ```

**Порядок вызовов в `dmBotIntent_OpenDoor`** (текущий код `dmBotIntent_OpenDoor.c:56` зовёт
только `m_Building.OpenDoor(m_DoorIdx)`):

```
m_Building.OpenDoor(m_DoorIdx);   // функциональное открытие (как сейчас)
bot.PlayDoorOpenGesture();        // hand-жест, в том же тике (или сразу после)
// ... существующее ожидание IsDoorOpened()/таймаут → Finish()
```

Жест — аддитивный (`CMD_Action == 506` → `OpenDoorErc/Cro`), **не блокирует движение**;
бот и так отходит назад (phase 0) и стоит в ожидании открытия (EXCLUSIVE MOVE), поэтому
дополнительный halt не обязателен. Одноразовая анимация завершается сама; `Finish()` по
`IsDoorOpened` уже дожидается створку, а не жест.

**Нужны ли `EMoteManager`/`ActionManager`/`AnimationState`?** Нет.
- `EMoteManager` у бота уже есть (`playerbase.c:434` создаётся безусловно), но для жеста
  не нужен — `AddCommandModifier_Action` самодостаточен.
- `ActionManager` не нужен и отсутствует (NULL на `AI_SERVER`).
- Явная инициализация `AnimationState` не нужна — команда `Human` сама ведёт граф.

**Риски**:

- **Граф**: подтверждено, что кастомный `botorama/Animations/Actions.agr` уже содержит
  состояния/переходы/источники `OpenDoorErc`/`OpenDoorCro` (ключ `GetCommandI(CMD_Action) == 506`,
  `Actions.agr:295/301/1167/1170/4968/4974`), а `player_main.agr:2` ссылается на ВАНИЛЬНЫЙ
  `.ast`-шаблон (`player_main.ast`), т.е. маппинг 506→анимация ванильный. Риск «анимации нет
  в графе» минимален, но требует визуальной проверки на тесте.
- **Руки/предмет в руках**: если в руках оружие/предмет, жест может выглядеть криво;
  `RequestHandAnimationStateRefresh()` в деструкторе callback (или `RefreshHandAnimationState()`)
  вернёт руки после жеста. Синк на клиент — через `SetSynchDirty()` (как `AnimSetFloat`).
- **Конфликт с другими командами**: аддитив подавится при full-body команде (raise оружия,
  melee, climb, ladder, unconscious). Гейт `if (GetCommandModifier_Action() || GetCommand_Action())`
  + «не играть при raised/climbing/melee» (аналог
  `ActionManagerBase.ActionPossibilityCheck`, `actionmanagerbase.c:244-253`) защищает от наслоения.
- **Не завершился**: one-shot анимация обычно завершается сама; на всякий случай — таймаут +
  `DeleteCommandModifier_Action(cb)` (force remove), если `GetCommandModifier_Action()` всё ещё
  жив спустя N секунд. Это дешевле, чем тащить весь action-жизненный цикл.

#### Сигнатуры (шпаргалка)

- `DayZPlayerConstants.CMD_ACTIONMOD_OPENDOORFW` = `506` (`dayzplayer.c:774`) — pActionID жеста.
- `Human.AddCommandModifier_Action(int pActionID, typename pCallbackClass)` → `HumanCommandActionCallback`
  (`human.c:1563`) — аддитивный жест поверх MOVE.
- `Human.StartCommand_Action(int pActionID, typename pCallbackClass, int pStanceMask)` (`human.c:1533`)
  — полнокадровый вариант (для `ActionOpenDoors` НЕ используется: `m_FullBody=false`).
- `Human.GetCommandModifier_Action()` (`human.c:1569`), `GetCommand_Action()` (`human.c:1536`),
  `DeleteCommandModifier_Action(cb)` (`human.c:1566`).
- `HumanCommandActionCallback.Cancel()` (`human.c:318`), `InternalCommand(int)` (`human.c:322`).
- `ActionOpenDoors.m_CommandUID = CMD_ACTIONMOD_OPENDOORFW` (`actionopendoors.c:7`);
  `OnStartServer → building.OpenDoor(doorIndex)` (`actionopendoors.c:53-69`).
- eAIBase: `StartActionObject(typename, Object, ItemBase)` (`eAIBase.c:9812`),
  `StartAction(typename, ActionTarget, ItemBase)` (`eAIBase.c:9798`),
  `m_eActionManager = new eAIActionManager(this)` (`eAIBase.c:735-736`).
- eAIActionManager: `PerformActionStart(ActionBase, ActionTarget, ItemBase, Param)`
  (`eAIActionManager.c:252`).
- `eAIActionOpenDoorsDummy : ActionOpenDoors` (`eaiactiondoorsdummy.c:2`) — `ActionCondition=false`,
  пустой `OnStartServer`.
- HandleBuildingDoors: `StartActionObject(...)` → `OpenDoor(...)` → `ForceRecalculate` через 34 мс
  (`eAIBase.c:11717-11729`); кулдауны `eAIBase.c:11736-11737`.
- botorama граф: `Actions.agr:295/301` (`GetCommandI(CMD_Action) == 506` → `OpenDoorErc/Cro`),
  `player_main.agr:2` (ванильный `player_main.ast`).

### Фильтр/стоимость

Из `ExpansionPathFilters` (pathfinding/expansionpathfilters.c:51-58):

- `PGPolyFlags.DISABLED` = **закрытая дверь**; `PGPolyFlags.DOOR` = **открытая дверь**;
  `PGPolyFlags.INSIDE` = внутри здания.
- include: `... | PGPolyFlags.DISABLED | PGPolyFlags.DOOR | PGPolyFlags.INSIDE | ...` —
  т.е. чтобы A* **прокладывал путь сквозь закрытые двери**, их нужно ВКЛЮЧИТЬ в include
  (`DISABLED`), иначе путь пойдёт вокруг.
- `SetCost(PGAreaType.DOOR_CLOSED, 4.0)` — дешёво → путь через закрытую дверь приоритетен
  (бот её откроет).
- `SetCost(PGAreaType.DOOR_OPENED, 10000.0)` — дорого → открытую дверь (физический объект)
  A* обходит, сам дверной проём всё равно используется.
- `AllowClosedDoors(false, timeout)` (ExpansionPathHandler.c:155) → фильтр без `DISABLED`
  (`m_PathFilter_NoClosedDoors`): путь **вокруг** закрытых дверей (нужно, когда дверь
  заперта/не открывается).

### Маппинг на botorama (рекомендация)

- **Фильтр**: расширить `dmBotPathfinder.m_Filter` — добавить `DISABLED` в include,
  `SetCost(DOOR_CLOSED, 4.0)` + `SetCost(DOOR_OPENED, 10000.0)`. Держать второй
  фильтр без `DISABLED` (аналог `m_PathFilter_NoClosedDoors`) для фолбэка «заперта».
- **Открытие**: примитив в пешке `dmAISurvivorBase` (а не интент): `TryOpenDoorOnPath()`
  — raycast вперёд (как `HandleBuildingDoors`) → `GetDoorIndex` → если закрыта →
  `building.OpenDoor(doorIndex)`. Логику «подождать открытия и продолжить» — в
  `dmBotIntent_MoveTo`: при `m_NoProgressTime` детектить blocking-building, открыть
  дверь, выставить короткий таймаут ожидания вместо немедленного `Fail()`.
- НЕ выносить в отдельный интент: открытие двери — короткая синхронная пауза внутри
  `MoveTo` (как в Expansion — `m_eAI_Halt` + `CallLater`), отдельный интент усложнит
  арбитраж без выгоды.

---

## Vault/climb

### API / команда / детект ребра

**Ваниль**:

- `HumanCommandClimb` (3_game/human.c:772):
  - `proto native int GetState();` — `ClimbStates` (human.c:762): `STATE_MOVE, STATE_TAKEOFF,
    STATE_ONTOP, STATE_FALLING, STATE_FINISH`. Vault: MOVE→TAKEOFF→ONTOP→FALLING→FINISH;
    climb: MOVE→TAKEOFF→ONTOP.
  - `proto native static bool DoClimbTest(Human, SHumanCommandClimbResult, int debugDrawLevel);`
  - `proto native static bool DoPerformClimbTest(Human, SHumanCommandClimbResult, int);` —
    тест перед реальным стартом команды.
- `SHumanCommandClimbResult` (human.c:741): `m_bIsClimb`, `m_bIsClimbOver`,
  `m_bFinishWithFall`, `m_bHasParent`, `m_fClimbHeight`, `m_ClimbGrabPoint(Normal)`,
  `m_ClimbStandPoint`, `m_ClimbOverStandPoint`, `m_GrabPointParent`, `m_ClimbStandPointParent`,
  `m_ClimbOverStandPointParent`.
- `DayZPlayerImplementJumpClimb` (4_world/entities/dayzplayerimplementjumpclimb.c):
  - `void JumpOrClimb()` — full-цикл: `DoPerformClimbTest` → `GetClimbType(height)` →
    `CanClimb(type,res)` → `Climb(res)` (=`StartCommand_Climb(res,type)`) либо `Jump()`.
  - `GetClimbType(height)`: `<1.1` → 0 (vault), `1.1..1.7` → 1 (vault), `1.7..2.75` → 2 (climb).
  - `WasSuccessful()`, `CheckAndFinishJump()`.
- `DayZPlayerImplement` (4_world/entities/dayzplayerimplement.c): `protected ref
  DayZPlayerImplementJumpClimb m_JumpClimb` (стр.98, создаётся в 182); `StartCommand_Climb`,
  `GetCommand_Climb` — нативы `Human` (human.c:1500/1502); `CanClimb(int, SHumanCommandClimbResult)`;
  `CanJump()`.
- `PlayerBase` (4_world/entities/manbase/playerbase.c):
  - `IsClimbing()` = `m_MovementState.m_CommandTypeId == COMMANDID_CLIMB` (стр.5373).
  - `override bool CanClimb(int climbType, SHumanCommandClimbResult climbRes)` (стр.4531) —
    гейты по стамине/переломам/травмам.
  - `ProcessJumpOrClimb` в `DayZPlayerImplement` (dayzplayerimplement.c:1750) — **клиентский**
    вход (`GetInputController().IsJumpClimb()`). У серверного ИИ НЕ сработает.

**Expansion**:

- `eAIBase.ProcessJumpOrClimb` override возвращает `false` (eAIBase.c:11074) — отключает
  input-driven прыжок.
- `eAIBase.HandleVaulting(eAICommandMove hcm, float pDt)` (eAIBase.c:10946) — детект:
  гейты (`m_PathFinding.m_IsJumpClimb` / `hcm.IsBlocked()` / `m_BlockingObject`),
  затем `HumanCommandClimb.DoClimbTest(this, m_ExClimbResult, 0)` (ванильный статик),
  фолбэк — `ExpansionClimb.DoClimbTest(this, m_ExClimbResult, true)` (свой, с
  `alwaysAllowClimb`).
- `ExpansionClimb` (Classes/ExpansionClimb.c:4) — самописный `DoClimbTest`/`ValidateHeight`/
  `CopyClimbResult` на `DayZPhysics.SphereCastBullet`/`RaycastRVProxy`; нужен только потому,
  что ванильный `DoClimbTest` не пускает ИИ через заборы, которые перепрыгивают зомби.
  Для botorama **не обязателен** — ванильного `DoClimbTest` достаточно на старте.
- Результат теста сохраняется в `ref SHumanCommandClimbResult m_ExClimbResult` —
  это **Expansion-добавленное поле** (Core/.../ManBase/DayZPlayerImplement.c:57), его в
  ванили НЕТ. У botorama своего эквивалента нет — либо добавить своё поле в пешку, либо
  пользоваться локальной переменной.
- Сам старт: `m_eAI_JumpClimb = true` (флаг в `eAI_OnMovementUpdate`, eAIBase.c:7571),
  затем в `CommandHandler` (eAIBase.c:7521): `if (m_eAI_JumpClimb) { m_eAI_JumpClimb=false;
  ... !m_JumpClimb.Expansion_Climb() ... }`, где `Expansion_Climb()` (Expansion modded
  `DayZPlayerImplementJumpClimb.c:15`) = `StartCommand_Climb(m_ExClimbResult, climbType)`
  + деплет стамины. Аналог в ванили — `m_JumpClimb.JumpOrClimb()`.
- Жизненный цикл команды: `GetCommand_Climb().GetState()` — при переходе в `STATE_ONTOP`,
  если `dist2DSq(позиция, следующий waypoint) < 4.0` → `m_PathFinding.UpdateNext(true)`
  (eAIBase.c:7358-7370). Т.е. «влез/перепрыгнул» = достиг `STATE_ONTOP` и следующая
  подцель рядом.

### Фильтр

Из `ExpansionPathFilters` (expansionpathfilters.c:58-76):

- `PGPolyFlags.SPECIAL` = `JUMP | CLIMB | CRAWL | CROUCH` (aiworld.c:25) — это **суммарный**
  флаг, именно его Expansion включает в include при `AI_HANDLEVAULTING`:
  `m_IncludeFlags |= PGPolyFlags.SPECIAL`.
- `PGPolyFlags.JUMP_OVER` / `JUMP_DOWN` / `CLIMB` — отдельные; `JUMP` = `JUMP_OVER|JUMP_DOWN`.
  Expansion кладёт `SPECIAL` целиком (не JUMP/CLIMB по отдельности).
- `SetCost(PGAreaType.FENCE_WALL, 5.0)` — **vault** (перепрыгивание забора/стены).
- `SetCost(PGAreaType.JUMP, 10.0)` — **climb** (влезание).
- `m_PathFilter_NoJumpClimb`: `FENCE_WALL`/`JUMP` = `1000.0` + `SPECIAL` в exclude — путь
  в обход клаймба (аналог `SetAllowJumpClimb(false, timeout)`, eAIBase/ExpansionPathHandler).

### Маппинг на botorama (рекомендация)

- **Фильтр**: в `dmBotPathfinder.m_Filter` добавить `SPECIAL` в include (или `JUMP_OVER|JUMP_DOWN|CLIMB`)
  + `SetCost(FENCE_WALL, 5.0)` + `SetCost(JUMP, 10.0)`. Держать `NoJumpClimb`-вариант для фолбэка.
- **Команда**: пешка `dmAISurvivorBase : PlayerBase` уже наследует `m_JumpClimb` и
  `StartCommand_Climb`. Примитив в пешке: `bool TryVaultClimb(out SHumanCommandClimbResult)` =
  `HumanCommandClimb.DoClimbTest(this, res, 0)` → если `m_bIsClimb||m_bIsClimbOver` →
  `m_JumpClimb.JumpOrClimb()` (или напрямую `StartCommand_Climb(res, GetClimbType(res.m_fClimbHeight))`
  после `CanClimb`). `GetClimbType` приватный в `DayZPlayerImplementJumpClimb` — проще звать
  `m_JumpClimb.JumpOrClimb()`, он сам всё делает (и `CanClimb`, и стамину).
- **Детект ребра**: в `dmBotIntent_MoveTo` при `m_NoProgressTime` / физической блокировке
  (`DayZPhysics.RaycastRV` вперёд) → звать `TryVaultClimb()`; «влез» = `GetCommand_Climb()`
  достиг `STATE_ONTOP` (или `!IsClimbing()` после старта) → продолжить следование.
  Держать счётчик попыток, иначе зациклится на непреодолимом препятствии.
- Место: логика детекта/решения — в `MoveTo` (он уже следит за застреванием), сама команда —
  примитив пешки. Отдельный интент не нужен (как и для дверей).

---

## Лестницы

### API / entry-exit / жизненный цикл

**Ваниль**:

- `HumanCommandLadder` (3_game/human.c:644): `proto native bool CanExit();` (на точке выхода),
  `proto native void Exit();`, `proto native vector GetLogoutPosition();`,
  `static DebugDrawLadder/DebugGetLadderIndex`.
- `Human` (human.c:1476/1478): `proto native HumanCommandLadder StartCommand_Ladder(Building pBuilding, int pLadderIndex);`
  и `GetCommand_Ladder();`.
- `DayZPlayerImplement` (dayzplayerimplement.c:452): `void SetClimbingLadderType(string value)`
  (тип лестницы, напр. "metal" — для анимации); `bool IsClimbingLadder()` =
  `m_MovementState.m_CommandTypeId == COMMANDID_LADDER` (стр.475).
- `Building` (building.c:12-14): `GetLaddersCount()`, `GetLadderPosTop(int)`, `GetLadderPosBottom(int)`.
- `ActionEnterLadder` (4_world/.../interact/actionenterladder.c): memory LOD выборки
  `ladder*_con` (точка входа) и `ladder*_con_dir` (направление), `HumanCommandLadder.DebugGetLadderIndex(compName)`
  → `SetClimbingLadderType(ladderType)` + `StartCommand_Ladder(b, ladderIndex)`. Это
  **клиентский** action — для ИИ заменяется тем же вызовом на сервере.

**Expansion** (лестницы свои, т.к. ванильный `GetLaddersCount` «всегда 0»):

- `ExpansionLadder` (AI/.../Buildings/BuildingBase.c:1): `m_Name`, `m_Index`, `m_Type`,
  `m_Con[2]` (низ/верх — точки входа, отсортированы по Y), `m_ConDir[2]`, `InsertVertex`.
- `BuildingBase.Expansion_GetLaddersCount()` (BuildingBase.c:297) — парсит memory LOD
  (`LOD.NAME_MEMORY`) + geometry LOD (`laddertype` property), выборки `ladder*_con` /
  `ladder*_con_dir`, кэш в `s_Expansion_BuildingsWithLadders`.
- Выбор лестницы в `eAIBase.OverrideTargetPosition` (eAIBase.c:4742-4900): если цель в
  радиусе здания с лестницей → выбрать ближайшую (`m_Con[0]` низ / `m_Con[1]` верх по
  высоте), `m_eAI_LadderClimbDirection` (1=вверх, -1=вниз), `pPosition = m_eAI_LadderEntryPoint`.
- Прицепка (`eAIBase.CommandHandler`, eAIBase.c:7413-7443):
  `eAI_IsInLadderRadius(entryPoint)` (радиус `UAMaxDistances.LADDERS`) +
  `eAI_IsCloseToLadderEntryPoint(2.28м)` + `eAI_CanReachLadderEntryPoint()`
  (`SphereCastBullet` от `pos+1.1` к entry, проверка `IsPointInRotatedRectangle`) →
  `SetClimbingLadderType(m_eAI_Ladder.m_Type)` + `StartCommand_Ladder(m_eAI_BuildingWithLadder, m_eAI_Ladder.m_Index)`.
- На лестнице (`m_eAI_IsOnLadder`, eAIBase.c:7498-7511): `HumanCommandLadder hcl = GetCommand_Ladder()`;
  `hcl.CanExit() && m_eAI_LadderTime > 2.0` → `hcl.Exit()`. Команда завершена
  (`pCurrentCommandFinished`) → сброс `m_eAI_IsOnLadder=false`, `m_eAI_Ladder=null`.
- Разворот при застревании: `m_eAI_LadderClimbDirection *= -1` при `m_eAI_BlockedTime > 2.0`
  (eAICommandMove.c:824-830).

### Фильтр

- `PGPolyFlags.LADDER` в include (`m_IncludeFlags = ... | LADDER`, expansionpathfilters.c:58).
- `SetCost(PGAreaType.LADDER, 1.0)` — дёшево (бот идёт к лестнице).
- Лестница НЕ влияет на путь как препятствие — это точка перехода между navmesh-этажами;
  A* должен уметь через неё пройти (include `LADDER`).

### Маппинг на botorama (рекомендация)

- **Фильтр**: добавить `LADDER` в include `dmBotPathfinder.m_Filter` + `SetCost(LADDER, 1.0)`.
- **Детект лестницы**: нужен аналог `ExpansionLadder` (парсинг memory LOD) — примитив в
  пешке/мозге `Expansion_GetLaddersCount`-подобный, т.к. ванильный `GetLaddersCount`
  не работает. Класть в `dmAISurvivorBase` (или в отдельный `dmBotLadderCache`).
- **Жизненный цикл**: в `dmBotIntent_MoveTo` при приближении к waypoint'у, который не
  лежит на текущем этаже (или при физической блокировке зданием с лестницей) — выбрать
  entry-point, `SetClimbingLadderType(type)` + `StartCommand_Ladder(building, index)`,
  подождать `GetCommand_Ladder()` → `CanExit()` → `Exit()` → продолжить.
- Сложнее дверей/клаймба: требует парсинга LOD + управления направлением. Рекомендую
  вынести в **отдельный интент `dmBotIntent_UseLadder`** (владеющий командой лестницы),
  а не впихивать в `MoveTo` — жизненный цикл длинный (подход → прицепка → подъём →
  отцепка) и несовместим с обычным «SetMove к waypoint'у».

---

## Итоговый набор PGFilter (include/exclude/cost)

Базовые флаги (ваниль, `3_game/ai/aiworld.c`):

- `PGPolyFlags`: `WALK, DISABLED, DOOR, INSIDE, SWIM, SWIM_SEA, LADDER, JUMP_OVER,
  JUMP_DOWN, CLIMB, CRAWL, CROUCH, UNREACHABLE, ALL, JUMP(=JUMP_OVER|JUMP_DOWN),
  SPECIAL(=JUMP|CLIMB|CRAWL|CROUCH)`.
- `PGAreaType`: `... DOOR_OPENED, DOOR_CLOSED, LADDER, CRAWL, CROUCH, FENCE_WALL, JUMP`.

Рекомендуемые фильтры для `dmBotPathfinder` (по образцу `ExpansionPathFilters`,
expansionpathfilters.c:51-176):

| Фильтр | include | exclude | SetCost |
|---|---|---|---|
| `m_Filter` (полный) | `UNREACHABLE\|DISABLED\|WALK\|DOOR\|INSIDE\|LADDER\|SPECIAL` | `CRAWL\|CROUCH\|SWIM\|SWIM_SEA` | `DOOR_CLOSED 4.0`, `DOOR_OPENED 10000.0`, `FENCE_WALL 5.0`, `JUMP 10.0`, `LADDER 1.0` |
| `m_Filter_NoJumpClimb` | без `SPECIAL` | `+SPECIAL` | `FENCE_WALL 1000.0`, `JUMP 1000.0` |
| `m_Filter_NoClosedDoors` | без `DISABLED` | `+DISABLED` | (как полный) |
| `m_SampleFilter` | `ALL & ~(CRAWL\|CROUCH)` | `CRAWL\|CROUCH` | — |

Примечания:

- **Замечание про `SPECIAL`**: include `SPECIAL` тянет за собой `CRAWL|CROUCH`, которые
  иначе в exclude. Это осознанный выбор Expansion (ползучие/сидячие полигоны не критичны,
  но без `SPECIAL` A* не пройдёт vault/climb). Альтернатива — включать только
  `JUMP_OVER|JUMP_DOWN|CLIMB` и оставить `CRAWL|CROUCH` в exclude.
- Блок-фильтр (детект «сегмент перекрыт дверью/клаймбом», `m_BlockFilter`,
  expansionpathfilters.c:103): те же флаги, что `m_Filter`, но БЕЗ `DOOR` и `DISABLED`
  (в include их нет, в exclude — есть). Используется в `AIWorld.RaycastNavMesh` для
  `IsBlocked(start, end, blockFilter)` (ExpansionPathHandler.c:262).

---

## Сводка сигнатур (шпаргалка для dayz-dev)

- Дверь открыть/закрыть: `Building.OpenDoor(int)`, `CloseDoor(int)`, `GetDoorIndex(int)`,
  `IsDoorOpen/IsDoorLocked(int)`, `CanDoorBeOpened(int, bool)`.
- Vault/climb: `HumanCommandClimb.DoClimbTest(Human, SHumanCommandClimbResult, int)` (static),
  `DayZPlayerImplementJumpClimb.JumpOrClimb()`, `StartCommand_Climb(SHumanCommandClimbResult, int)`,
  `GetCommand_Climb().GetState()` (`ClimbStates`), `PlayerBase.IsClimbing()`.
- Лестница: `StartCommand_Ladder(Building, int)`, `GetCommand_Ladder().CanExit()/Exit()`,
  `SetClimbingLadderType(string)`, `IsClimbingLadder()`, `Building.Expansion_GetLaddersCount()`
  (Expansion; ванильный `GetLaddersCount` не работает).
- Path: `AIWorld.FindPath/RaycastNavMesh/SampleNavmeshPosition`, `PGFilter.SetFlags/SetCost`.
