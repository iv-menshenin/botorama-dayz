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
  `StartCommand_Climb`. Примитив в пешке: `bool TryVaultClimb()` =
  `HumanCommandClimb.DoClimbTest(this, res, 0)` → если `m_bIsClimb||m_bIsClimbOver` →
  свой `GetClimbTypeLocal(m_fClimbHeight)` (`<1.1`→0, `1.1..1.7`→1, `1.7..2.75`→2, иначе -1)
  → **напрямую `StartCommand_Climb(res, climbType)`** (натив `Human`). `GetClimbType`
  приватный в `DayZPlayerImplementJumpClimb` — поэтому локальная копия в пешке.
  Сделано и проверено (бот перелезает забор).

**Готча (обожглись): `JumpOrClimb()` НЕНАДЁЖЕН для серверного ИИ — звать напрямую
`StartCommand_Climb(res, climbType)`.** `DayZPlayerImplementJumpClimb.JumpOrClimb()`
(`dayzplayerimplementjumpclimb.c:20`) внутри делает СВОЙ тест `DoPerformClimbTest` (другой
натив, не `DoClimbTest` — результат может разойтись с нашим), затем гейт
`m_Player.CanClimb(climbType, res)` (стамина/переломы/`hibcfg.m_bJumpAllowed`/холограммы),
а при неудаче **фолбэчится в `Jump()`** (бесполезный подскок `StartCommand_Fall`, не
перелезает). Симптомы: `DoClimbTest` вернул `isClimbOver=true`, но бот НЕ лезет и ретраит
каждые `DM_VAULT_GRACE`+`DM_MOVE_STUCK_TIME` (~4с) — потому что клаймб так и не стартовал,
`IsClimbing()` всё время false. Правильный путь (как `Expansion_Climb` в Expansion
`DayZPlayerImplementJumpClimb.c:15`): `DoClimbTest` → свой `GetClimbType(m_fClimbHeight)`
(`<1.1`→0, `1.1..1.7`→1, `1.7..2.75`→2, иначе -1) → `StartCommand_Climb(res, climbType)`
напрямую (натив `Human`), БЕЗ `Jump()`-фолбэка. Для ИИ гейт `CanClimb` (особенно
`CanConsumeStamina`) можно смягчить/переопределить — ИИ-стамина не регенерит как у игрока,
поэтому ванильный `CanClimb` срезает повторные vault'ы.
- **Детект ребра**: в `dmBotIntent_MoveTo` при `m_NoProgressTime` / физической блокировке
  (`DayZPhysics.RaycastRV` вперёд) → звать `TryVaultClimb()`; «влез» = `GetCommand_Climb()`
  достиг `STATE_ONTOP` (или `!IsClimbing()` после старта) → продолжить следование.
  Держать счётчик попыток, иначе зациклится на непреодолимом препятствии.
- Место: логика детекта/решения — в `MoveTo` (он уже следит за застреванием), сама команда —
  примитив пешки. Отдельный интент не нужен (как и для дверей).

**Готча (обожглись): прогресс-монитор застревания сбрасывается при пересоздании интента и
пересчёте пути — vault не срабатывает у `FollowTo`.** `MoveTo.OnStart` обнуляет
`m_BestDist/m_NoProgressTime`, а `RePath` ставит `m_PathIdx=0` → бот проходит waypoint 0→1
(сброс прогресса). `FollowTo` пересчитывал путь каждую секунду (`DM_FOLLOW_PATH_INTERVAL`),
поэтому `m_NoProgressTime` никогда не набирал `DM_MOVE_STUCK_TIME=3с` → `TryVaultClimb` не
вызывался, бот вечно осциллировал у забора (симптом: скачки `moveAngle ≈ ±180°` в логе).
Фикс: (1) `useFollow` по рекенси `m_LastContact` (`DM_FOLLOW_VISIBLE_RECENT=5с`), а не по
мигающему `m_HasLOS` (FOV-конус зависит от поворота головы) + гистерезис
`DM_FOLLOW_SWITCH_DWELL=1с`; (2) `FollowTo` пересчитывает путь только при сдвиге якоря
> `DM_FOLLOW_REPATH_DIST=2м` (`m_LastPathGoal`/`m_PathGoalValid`). После этого прогресс-монитор
копится → застревание детектится → vault перелезает забор.

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
- **Готча (обожглись): `AIWorld.RaycastNavMesh` ловит РЁБРА navmesh (границы полигонов),
  НЕ плоскую поверхность.** Дока `aiworld.c:110` — `@returns true - if ray hits navmesh edge`.
  Вертикальный луч вниз (`point+1.8 → point-0.5`) на ровной земле не пересекает ни одного
  X/Z-ребра → `NOHIT`. Поэтому для ground-probe («есть ли проходимая поверхность рядом с
  точкой», fall-safety) использовать **`SampleNavmeshPosition(point, radius, filter, out sampled)`**
  (ближайшая точка navmesh в радиусе), а `RaycastNavMesh` — только для детекта «сегмент
  пересекает границу» (vault/climb/block, как Expansion `IsBlocked`). Реализовано в
  `dmBotIntent_MoveTo.IsPointOnNavMesh` (радиус `DM_MOVE_GROUND_PROBE_RADIUS=0.5`).

---

## Вертикальная навигация (лестницы/крыши/прыжки)

### Резюме (как это работает в Expansion)

Expansion решает проблему «нативный `FindPath` рутит 2D и не учитывает высоту» двумя
независимыми механизмами, плюс третий как страховку:

1. **Лестницы** — НЕ через navmesh, а **логикой поверх пути**. Когда бот уже в здании с
   лестницей или цель в здании с лестницей, `OverrideTargetPosition` подменяет цель пути
   на **entry-point лестницы** (низ для подъёма, верх для спуска), а прицепка к лестнице
   делается в `CommandHandler` (`StartCommand_Ladder`) после проверки
   `eAI_IsInLadderRadius` + `eAI_IsCloseToLadderEntryPoint` + `eAI_CanReachLadderEntryPoint`.
   Лестница выбирается «process of elimination» (пул лестниц здания), взвешиванием
   `2D-дистанция × разница Y`.
2. **«Leap of faith» (прыжок вниз с крыши/объекта)** — когда нативный `FindPath` не нашёл
   пути (бот на объекте/крыше без navmesh-связи с землёй), `ExpansionPathPoint.FindPathFrom`
   строит **обратный путь** (`FindPath(target → bot)`) и проверяет, безопасно ли спрыгнуть:
   `climbHeight < 2.5` && `fallHeight < HEALTH_HEIGHT_LOW` + физический raycast. Если да —
   разворачивает обратный путь в прямой и «сходит с края».
3. **Защита от смертельного падения** — `CheckFallHeight` + `eAI_IsDangerousAltitude` +
   `eAI_IsFallSafe` не дают боту шагнуть с опасной высоты (отодвигают точку пути от края
   на 0.55 м либо помечают цель недостижимой).

Плюс детект «сегмент требует vault/climb» (`UpdatePathSegmentState`) через block-filter
raycast + `IsVaultClimb`/`IsElevated` → флаг `m_IsJumpClimb`.

**Ключевое отличие от botorama**: у нас `dmBotPathfinder.FindPath` — это голая обёртка
над `AIWorld.FindPath` без `ExpansionPathHandler` и без «leap of faith»; наш
`MoveTo.TryStartLadder` выбирает лестницу по чистой 2D-дистанции (без взвешивания по Y),
а прыжка вниз и fall-защиты нет вообще. Значит, всё это надо добавить самим (см. таблицу
маппинга в конце).

---

### 1. Лестничная маршрутизация

**Точка входа** — `eAIBase.OverrideTargetPosition` (`Entities/AI/eAIBase.c:4742`):

```c
void OverrideTargetPosition(vector pPosition, bool isFinal = true, float maxDistance = 1.0, bool allowJumpClimb = true)
```

- `eAIBase.c:4752` — если `m_eAI_Ladder && eAI_CheckShouldUseBuildingWithLadder(pPosition)`,
  то `pPosition = m_eAI_LadderEntryPoint` (`eAIBase.c:4780`) — **цель пути подменяется на
  точку входа лестницы** (низ для подъёма, верх для спуска), пока бот не на лестнице.
  На лестнице (`m_eAI_IsOnLadder`) — return без изменения (`eAIBase.c:4754-4777`).
- `eAIBase.c:4785` — `else if (eAI_CheckShouldClimbLadderToReachPosition(pPosition))` →
  выбор лестницы (см. ниже).

**Гейт «нужна ли лестница»** — `eAI_CheckShouldClimbLadderToReachPosition(vector targetPos)`
(`eAIBase.c:4995-5047`):

- `m_eAI_IsOnLadder` → true (`:4997`).
- FSM-бой + острая угроза → false (`:5000-5001`).
- Нет `m_eAI_BuildingWithLadder`: если `|targetPos[1] - position[1]| <= 1.5` → false
  (цель на том же этаже — лестница не нужна) (`:5005-5008`).
- Есть `m_eAI_BuildingWithLadder`, но путь ещё не упёрся в него
  (`!m_PathFinding.m_IsUnreachable && !IsPointInCircle(GetEnd(), 0.55, position)`) → false
  (`:5010-5013`) — ждём, пока бот реально дойдёт до здания.
- **Детект здания под ногами**: `IEntity floor = PhysicsGetFloorEntity();` каст в
  `BuildingBase` с `Expansion_GetLaddersCount() > 0` → `m_eAI_BuildingWithLadder = building`,
  `m_eAI_LadderLoops = 0` (`:5015-5029`); `m_eAI_FloorIsBuildingWithLadder` = стоит ли на
  полу этого здания (`:5035-5038`).
- `eAI_CheckShouldUseBuildingWithLadder(targetPos)` (`:5040`), при
  `m_PathFinding.m_IsTargetUnreachable` → `m_eAI_PreferLadder = true` (`:5043-5044`).
- Возврат `m_eAI_PreferLadder` (`:5046`).

**Радиусная проверка** — `eAI_CheckShouldUseBuildingWithLadder(vector targetPos)`
(`eAIBase.c:5111-5163`):

- `center = building.GetPosition()`, `radius = ExpansionStatic.GetBoundingRadius(building)`
  (`:5116-5117`).
- `m_eAI_TargetIsInBuildingWithLadderRadius = IsPointInCircle(center, radius, targetPos)`
  (`:5124-5127`).
- Если ни цель, ни бот НЕ в радиусе здания → сброс всего ladder-состояния
  (`m_eAI_BuildingWithLadder/m_eAI_Ladder/m_eAI_PreferLadder/m_eAI_LadderLoops = 0`) и
  return false (`:5130-5143`).
- `m_eAI_LadderLoops == 10` → сдаёмся (сброс, return false) (`:5145-5160`) — защита от
  бесконечного цикла.

**Выбор ближайшей лестницы + entry-point/направление** (`eAIBase.c:4792-4896`):

- **Пул («process of elimination»)** — `map<int, ref ExpansionLadder> ladders` на здание
  (кэш в `m_eAI_Ladders`), `eAIBase.c:4792-4808`. Если пул пуст — копируем все
  `m_Expansion_Ladders` здания в пул и `m_eAI_LadderLoops++` (`:4799-4808`). Это НЕ
  оптимальный путь, но исключает вечное застревание: использованные/недостижимые лестницы
  удаляются из пула (см. прицепку ниже), при опустошении — добавляются обратно.
- **Взвешивание**: `distSqBtm = Distance2DSq(modelPos, btm) * AbsFloat(modelPos[1] - btm[1])`,
  `distSqTop = Distance2DSq(modelPos, top) * AbsFloat(modelPos[1] - top[1])` (`:4830-4831`)
  — 2D-дистанция × разница Y (учёт высоты).
- `distancesSqBtm.Sort()` / `distancesSqTop.Sort()` (`:4850-4851`); ближайший низ vs верх
  (`:4853-4854`): `closestBtm < closestTop` → entry = `m_Con[0]` (низ), `dirPoint = m_ConDir[0]`,
  `climbDir = 1` (вверх) (`:4861-4868`); иначе entry = `m_Con[1]` (верх), `climbDir = -1`
  (вниз) (`:4869-4876`).
- `m_eAI_PreferLadder = false`, если цель ниже самой нижней лестницы или не в радиусе
  здания (`:4842-4846`).
- Гейт воды: `if (GetWaterDepth(m_eAI_LadderEntryPoint) < 1.0 || eAI_IsInLadderRadius(entryPoint))`
  (`:4878`) — лестницу не берём, если entry под водой и мы не в радиусе.
- Запись: `m_eAI_Ladder / m_eAI_LadderEntryPoint / m_eAI_LadderDirPoint /
  m_eAI_LadderClimbDirection` (`:4882-4885`), `pPosition = m_eAI_LadderEntryPoint` (`:4887`).

**Прицепка** — `CommandHandler` (`eAIBase.c:7413-7443`):

- Условие: `m_eAI_Ladder && m_eAI_BuildingWithLadder && m_eAI_CommandTime > 1.0` (`:7413`).
- `eAI_IsInLadderRadius(m_eAI_LadderEntryPoint) && IsPointInCircle(GetEnd(), 0.55, playerPosition)
  && !eAI_IsChangingStance()` (`:7415`).
- `eAI_IsCloseToLadderEntryPoint() && eAI_CanReachLadderEntryPoint()` (`:7419`) →
  `m_eAI_IsOnLadder = true; m_eAI_LadderTime = 0; SetClimbingLadderType(m_eAI_Ladder.m_Type);
  eAI_ResetRaised(); StartCommand_Ladder(m_eAI_BuildingWithLadder, m_eAI_Ladder.m_Index);`
  (`:7423-7427`).
- Если лестниц в здании > 1 — удалить текущую из пула (`:7437-7438`); если недостижима —
  `m_eAI_Ladder = null` (`:7440-7441`).

**На лестнице / отцепка** (`eAIBase.c:7498-7511`):

- `m_eAI_IsOnLadder`: `HumanCommandLadder hcl = GetCommand_Ladder();` null → сброс состояния
  (`:7503-7506`); `hcl.CanExit() && m_eAI_LadderTime > 2.0` → `hcl.Exit()` (`:7508-7510`).

**Разворот при застревании** — `eAICommandMove` (`Classes/Commands/eAICommandMove.c:824-830`):
`if (m_eAI_IsOnLadder && m_eAI_BlockedTime > 2.0) { m_eAI_LadderClimbDirection *= -1; ... }`.

**Поля** (`eAIBase.c:138-163`): `m_eAI_IsOnLadder` (`:139`), `m_eAI_LadderTime` (`:140`),
`m_eAI_LadderClimbDirection` (`:141`, 1=вверх/-1=вниз), `m_eAI_Ladder` (`:142`, `ExpansionLadder`),
`m_eAI_LadderEntryPoint` (`:143`), `m_eAI_LadderDirPoint` (`:144`), `m_eAI_BuildingWithLadder`
(`:147`), `m_eAI_FloorIsBuildingWithLadder` (`:148`), `m_eAI_TargetIsInBuildingWithLadderRadius`
(`:149`), `m_eAI_Ladders` (`:152`, `ref eAILadders = new eAILadders`), `m_eAI_LadderLoops`
(`:155`), `m_eAI_LastClimbedBuildingWithLadder` (`:158`), `m_eAI_LastClimbedLadder` (`:161`),
`m_eAI_PreferLadder` (`:163`).
Тип `eAILadders` = `typedef map<BuildingBase, ref map<int, ref ExpansionLadder>>` (`eAIBase.c:15`).

**`ExpansionLadder`** (`Entities/Buildings/BuildingBase.c:1-44`): `m_Name`, `m_Index`,
`m_Type`, `m_Con[2]`, `m_ConDir[2]`; `InsertVertex` сортирует по Y (низ = `[0]`, верх = `[1]`).
Парсинг — `BuildingBase.Expansion_GetLaddersCount()` (`BuildingBase.c:297-434`): memory LOD
(`LOD.NAME_MEMORY`) + geometry LOD (свойство `laddertype`), выборки `ladder*` → парсинг
индекса из имени, вершины `ladderN_con` / `ladderN_con_dir`; кэш в static
`s_Expansion_BuildingsWithLadders` (`map<string, map<int, ExpansionLadder>>` по типу здания).

> **Урок (ботorama v3.135)**: `_con_dir` — НЕ опционально. Первая версия `dmBotLadderCache`
> парсила только `ladderN_con` (низ/верх), игнорируя `_con_dir` — нативный
> `HumanCommandLadder.Exit()` выпускал бота НЕ с той стороны лестницы (входил с одной
> стороны, наверху сходил в пустоту) → падение с крыши. Направление входа/выхода — это
> отдельные данные (`_con_dir`), не выводимые из точек входа. Также: дистанция прицепки к
> входу должна быть 3D (иначе бот цепляется к лестнице, находясь этажом выше).

**Проверки близости/достижимости** (`eAIBase.c`):

- `eAI_IsInLadderRadius(vector entryPoint)` (`:5233-5250`) — `IsPointInCircle(entryPoint,
  UAMaxDistances.LADDERS, begPos)`; `UAMaxDistances.LADDERS = 1.3` (ваниль
  `4_world/classes/useractionscomponent/actions/actionconstants.c:115`).
- `eAI_IsCloseToLadderEntryPoint(float maxDist = 2.282542)` (`:5211-5231`) — если entry выше,
  `begPos[1] += 1.1`, затем `DistanceSq < maxDist*maxDist`.
- `eAI_CanReachLadderEntryPoint()` (`:5165-5209`) — `begPos[1] += 1.1`, `DayZPhysics.
  SphereCastBullet(begPos, entryPoint, 0.1, mask, this, ...)`, затем
  `Math.IsPointInRotatedRectangle(min, max, 0.6, contactPos)`, где min/max = entry ±
  `perpend*0.35`/`dir*0.45`/`dir*0.15`.

### 2. «Leap of faith» / прыжок вниз с крыши/объекта

`Classes/PathFinding/ExpansionPathPoint.c`, метод `FindPathFrom(vector startPos, ExpansionPathHandler
pathFinding, inout array<vector> path, out int pathGlueIdx = -1)` (`:133-365`).

**Триггер** (`:193`): путь не найден ИЛИ путь из 2 точек, где `path[1]` рядом с ботом, но не
с целью:

```c
if ((!found || (path.Count() == 2 && !Math.IsPointInCircle(Position, 1.0, path[1]) && Math.IsPointInCircle(pathFinding.m_Unit.GetPosition(), 0.55, path[1]))) && !pathFinding.m_Unit.m_eAI_Ladder)
```

т.е. «бот стоит на объекте/крыше, navmesh-связи с землёй нет» (лестница отключена).

**Алгоритм**:

1. `endPos = startPos` (`:212`) — конец обратного пути = текущая позиция бота.
2. `dir = Direction(endPos, Position)`; если `dir.LengthSq() > 100.0` (дальше 10 м) —
   `targetPos = endPos + dir.Normalized() * 10.0`, иначе `targetPos = Position` (`:217-227`).
3. **Обратный поиск**: `m_AIWorld.FindPath(targetPos, endPos, filter, tempPath)` (`:229`) —
   ищется путь ОТ цели К боту (с земли на крышу), затем разворачивается.
4. Проверка: `tempPath.Count() > 2 || DistanceSq(tempEnd, endPos) > 0.0001` (`:241`).
   - Продлеваем `tempEnd` на 0.5 от `endPos` (по горизонтали) — «сойти с края»
     (`:255-257`); `checkPos[1] = max(max(endPos[1], checkPos[1]), unitY) + 0.5` (`:258-259`).
   - `surfaceEndPosition = GetSurfaceRoadPosition(endPos)`, `surfacePosition =
     GetSurfaceRoadPosition(tempEnd)` (`:261-262`).
   - `climbHeight = surfacePosition[1] - surfaceEndPosition[1]`; `fallHeight =
     surfaceEndPosition[1] - surfacePosition[1]` (`:276-277`).
   - **Условие безопасности** (`:278`):
     `IsPointInCircle(tempEnd, 10.0, endPos) && climbHeight < 2.5 && fallHeight <
     DayZPlayerImplementFallDamage.HEALTH_HEIGHT_LOW && !IsBlockedPhysically(endPos + "0 0.5 0", checkPos)`.
   - Второй raycast (`:280`): `isSwimming || !IsBlockedPhysically(checkPos, surfacePosition + "0 0.5 0")`
     + проверка воды `GetWaterDepth(surfacePosition) <= 1.5` (`:283`).
5. Если безопасно — `path.Clear()` и разворот: вставляем `tempPath` с конца в начало
   (`:285-293`), `m_TempCount = 0`, `found = true`, `m_Time = -15.0` (длинная пауза до
   пересчёта) (`:298-301`).

`IsBlockedGeom` (`:425-444`) — физический raycast `DayZPhysics.RayCastBullet` по маске
`BUILDING|DOOR|FENCE|ITEM_LARGE|VEHICLE|ROADWAY|TERRAIN` (замечание: НЕ `RaycastRV` —
ложные срабатывания у пирсов).

### 3. Защита от падения

`Classes/PathFinding/ExpansionPathHandler.c`:

- `UpdateNext()` вызывает `CheckFallHeight()` ТОЛЬКО когда путь почти завершён:
  `if (m_Count == 1 + m_PointIdx && !CheckFallHeight()) return;` (`:943`) — иначе бот
  застревает на верхних этажах (`Land_Barn_Metal_Big`/`Land_Mil_GuardTower`).
- `bool CheckFallHeight()` (`:1014-1048`):
  - если `m_Unit.eAI_IsDangerousAltitude()` (`:1016`):
    - `checkDirection = Direction(unitPos, m_Points[1+m_PointIdx])`, `checkDirection[1]=0`,
      `len = Length()` (`:1019-1021`).
    - если `(!m_eAI_Ladder || (!m_eAI_IsOnLadder && !eAI_IsCloseToLadderEntryPoint())) &&
      !eAI_IsFallSafe(checkDirection.Normalized()*(len+2.0), true, HEALTH_HEIGHT_LOW, true, 1339)`
      (`:1022`):
      - отодвинуть следующую точку на 0.55 м от края (`m_PathSegmentDirection`), `UpdatePoint`,
        return true (`:1025-1031`);
      - иначе `m_IsUnreachable = true; m_IsTargetUnreachable = true; return false`
        (`:1041-1043`).

`Entities/AI/eAIBase.c`:

- `bool eAI_IsDangerousAltitude()` (`:11454-11474`): `fallHeight = position[1] - m_eAI_SurfaceY`;
  `< HEALTH_HEIGHT_LOW` → false; при swimming вычитает `waterDepth`.
- `bool eAI_IsFallSafe(vector checkDirection, bool checkBlocking = true, float heightThresh = 0,
  bool checkHealth = true, int dbgIndex = 1337)` (`:11319-11452`):
  - `heightThresh == 0` → `HEALTH_HEIGHT_LOW` (`:11321-11322`).
  - **Блокировка (стена/перила)**: `RaycastRV(checkPosition + "0 0.76 0", position + "0 0.76 0",
    ..., ObjIntersectGeom, 0.26)` → если упёрлись → **safe** (`:11335-11341`). Оффсет 0.76/радиус
    0.26 подобран так, чтобы цеплять перила (`Land_Pier_Crane2_Base`, `Land_Factory_Small`).
  - Поверхность: `checkPosition[1] = ExpansionStatic.GetSurfaceRoadY3D(checkX, checkY+1.5, checkZ,
    RoadSurfaceDetection.UNDER)` (`:11358`); `fallHeight = position[1] - checkPosition[1]` (`:11360`).
  - Вода: `waterDepth = GetWaterDepth(checkPosition)`, `fallHeight -= waterDepth` (`:11417-11423`);
    `waterDepth > 1.5 && SurfaceIsWater` → safe если swimming включён (`:11425-11426`).
  - Итог: `fallHeight <= heightThresh || (checkHealth && GetHealth01() - Math.InverseLerp(
    heightThresh, HEALTH_HEIGHT_HIGH, fallHeight) >= 0.90)` → safe (`:11427-11428`).

### 4. Детект vault/climb на сегменте

`Classes/PathFinding/ExpansionPathHandler.c`:

- `void UpdatePathSegmentState()` (`:1050-1115`):
  - `start = m_Points[m_PointIdx]`, `end = m_Next0.GetPosition()` (`:1052-1053`),
    `m_PathSegmentDirection = Direction(start, end)` (`:1055`).
  - **Block-filter raycast** (`:1058`): `if ((AI_HANDLEVAULTING || AI_HANDLEDOORS) &&
    IsBlocked(start, end, m_BlockFilter))` → `m_IsBlocked = true` (`:1060`); затем
    `IsBlocked(start, end, m_PathFilter)` (обычный фильтр — true на vault/climb, но НЕ на
    открытые двери) && `IsVaultClimb(start, end)` → `m_IsJumpClimb = true`,
    `m_SuppressRecalculate = true` (`:1063-1074`).
  - **Физическая блокировка/подъём** (`:1077-1091`): `else if (AI_HANDLEVAULTING)`:
    `IsBlockedPhysically(start + "0 0.49 0", end + "0 0.49 0", ...) || IsElevated(start)` →
    `m_IsBlockedPhysically = true`, `m_IsJumpClimb = true`.
- `bool IsVaultClimb(vector start, vector end)` (`:1117-1127`): `distSq = DistanceSq(start, end)`;
  `return distSq > 0.25 && distSq < 100.0` (0.5..10 м).
- `bool IsElevated(vector start)` (`:1129-1135`): `start[1] - m_Unit.GetPosition()[1] > 0.5`.
- `m_IsJumpClimb` затем потребляется `eAIBase.HandleVaulting(eAICommandMove hcm, float pDt)`
  (`eAIBase.c:10946`) — `DoClimbTest` → `JumpOrClimb` (см. секцию Vault/climb выше);
  вход в `CommandHandler` (`eAIBase.c:7568-7571`).

### 5. Фильтры

Полный текст уже в секции «Итоговый набор PGFilter». Ключевое для вертикальной навигации
(`Classes/PathFinding/expansionpathfilters.c`):

- include: `UNREACHABLE|DISABLED|WALK|DOOR|INSIDE|LADDER` (`:58`), `SPECIAL` при
  `AI_HANDLEVAULTING` (`:62-64`); exclude: `CRAWL|CROUCH|SWIM_SEA|SWIM` (`:59`).
- Cost (`SetFilterCost`, `:131-176`): `LADDER 1.0` (`:133`), `FENCE_WALL 5.0` (vault, `:136`),
  `JUMP 10.0` (climb, `:137`), `DOOR_CLOSED 4.0` (`:143`), `DOOR_OPENED 10000.0` (`:144`).
  `NoJumpClimb`-вариант: `FENCE_WALL 1000.0`/`JUMP 1000.0` (`:158-159`).
- `m_BlockFilter` (`:103`): include = `m_IncludeFlags & ~(DOOR|DISABLED)`, exclude =
  `m_ExcludeFlags | DOOR | DISABLED`.

### 6. Сигнатуры/флаги (сводка)

**Ваниль**:

- `DayZPlayerImplementFallDamage.HEALTH_HEIGHT_LOW = 5`, `HEALTH_HEIGHT_HIGH = 14`
  (`4_world/entities/dayzplayerimplementfalldamage.c:26-27`).
- `UAMaxDistances.LADDERS = 1.3` (`4_world/classes/useractionscomponent/actions/actionconstants.c:115`).
- `PGPolyFlags.LADDER/JUMP_OVER/JUMP_DOWN/CLIMB/SPECIAL/JUMP` (`3_game/ai/aiworld.c:25`),
  `PGAreaType.LADDER/FENCE_WALL/JUMP` (см. «Итоговый набор PGFilter»).
- `PhysicsGetFloorEntity()` (ваниль, используется в `eAI_CheckShouldClimbLadderToReachPosition`).

**Expansion**:

- `eAIBase.OverrideTargetPosition(vector, bool, float, bool)` — `eAIBase.c:4742`.
- `eAIBase.eAI_CheckShouldClimbLadderToReachPosition(vector)` — `eAIBase.c:4995`.
- `eAIBase.eAI_CheckShouldUseBuildingWithLadder(vector)` — `eAIBase.c:5111`.
- `eAIBase.eAI_IsExcludedBuildingWithLadder(Object)` — `eAIBase.c:5049` (хардкод-исключения: краны, геоплант).
- `eAIBase.eAI_CanReachLadderEntryPoint()` — `eAIBase.c:5165`.
- `eAIBase.eAI_IsCloseToLadderEntryPoint(float maxDist=2.282542)` — `eAIBase.c:5211`.
- `eAIBase.eAI_IsInLadderRadius(vector)` — `eAIBase.c:5233`.
- `eAIBase.eAI_IsFallSafe(vector, bool, float, bool, int)` — `eAIBase.c:11319`.
- `eAIBase.eAI_IsDangerousAltitude()` — `eAIBase.c:11454`.
- `eAIBase.HandleVaulting(eAICommandMove, float)` — `eAIBase.c:10946`.
- `eAIBase.eAI_CanClimbOn(IEntity, SHumanCommandClimbResult)` — `eAIBase.c:11235` (гейты клаймба: деревья/кусты/люди/`m_eAI_PreventClimb`/открытые ворота).
- `ExpansionPathPoint.FindPathFrom(vector, ExpansionPathHandler, inout array<vector>, out int)` — `ExpansionPathPoint.c:133`; leap of faith `:191-344`.
- `ExpansionPathPoint.IsBlockedGeom(vector, vector, eAIBase, ...)` — `ExpansionPathPoint.c:425`.
- `ExpansionPathHandler.CheckFallHeight()` — `ExpansionPathHandler.c:1014`.
- `ExpansionPathHandler.UpdatePathSegmentState()` — `ExpansionPathHandler.c:1050`.
- `ExpansionPathHandler.IsVaultClimb(vector, vector)` — `ExpansionPathHandler.c:1117`.
- `ExpansionPathHandler.IsElevated(vector)` — `ExpansionPathHandler.c:1129`.
- `ExpansionPathHandler.IsBlocked(vector, vector, PGFilter, ...)` — `ExpansionPathHandler.c:262`.
- `ExpansionPathHandler.IsBlockedPhysically(vector, vector, ...)` — `ExpansionPathHandler.c:275`.
- `ExpansionPathHandler.UpdateNext(bool)` — `ExpansionPathHandler.c:918` (вызов `CheckFallHeight` на `:943`).
- `BuildingBase.Expansion_GetLaddersCount()` — `BuildingBase.c:297`; `ExpansionLadder` — `BuildingBase.c:1`.
- `ExpansionStatic.GetBoundingRadius(Object)` / `GetSurfaceRoadY3D` / `GetSurfaceRoadPosition` — helper'ы, используемые для радиусов/поверхности.

### Что нужно портировать → в какую нашу сущность

| Механизм (Expansion) | Наша сущность | Что сделать |
|---|---|---|
| Пул лестниц здания + взвешивание `2D-дистанция × ΔY` + entry-point низ/верх по направлению (`OverrideTargetPosition`/`eAI_CheckShouldClimbLadderToReachPosition`) | `dmBotLadderCache` + `MoveTo.TryStartLadder` | Хранить на лестницу `m_Con[2]` (низ/верх) + `m_ConDir[2]`; выбор лестницы заменить на `Distance2DSq × AbsFloat(ΔY)`, entry = низ при подъёме / верх при спуске; пул `map<Building, map<int, dmBotLadder>>` с удалением использованных (`process of elimination`) и капом циклов |
| Гейт «нужна ли лестница» (радиус здания, `PhysicsGetFloorEntity`, `m_eAI_PreferLadder` при unreachable) | `MoveTo` / `dmBotIntent_UseLadder` | Перед стартом лестничного интента: цель в радиусе здания с лестницей ИЛИ бот на полу здания с лестницей; `|ΔY| > 1.5` |
| Проверки близости/достижимости (`eAI_IsInLadderRadius`/`eAI_IsCloseToLadderEntryPoint`/`eAI_CanReachLadderEntryPoint`) | пешка `dmAISurvivorBase` (примитивы) | `IsPointInCircle(entry, 1.3, pos)`, `DistanceSq < 2.28²`, `SphereCastBullet` + `IsPointInRotatedRectangle` |
| Прицепка/отцепка (`StartCommand_Ladder` + `CanExit`/`Exit`, `SetClimbingLadderType`) | `dmBotIntent_UseLadder` | уже есть; добавить разворот `climbDirection *= -1` при `m_eAI_BlockedTime > 2.0` (в `MoveTo` при застревании на лестнице) |
| Leap of faith (`FindPathFrom` обратный путь) | `dmBotPathfinder` (или `MoveTo` при `Fail()`/unreachable) | При «нет пути»: обратный `FindPath(target → bot)` (лимит 10 м), проверка `climbHeight < 2.5 && fallHeight < HEALTH_HEIGHT_LOW` + физический raycast, разворот пути + `m_Time = -15` |
| Fall-защита (`CheckFallHeight`/`eAI_IsDangerousAltitude`/`eAI_IsFallSafe`) | пешка (примитив) + `MoveTo` | `m_eAI_SurfaceY`-аналог (поверхность под ногами), `eAI_IsFallSafe`: raycast перила + `GetSurfaceRoadY3D(UNDER)` + `fallHeight <= threshold \|\| здоровье-эвристика`; в `MoveTo` перед шагом на конечную подцель — отодвинуть точку от края или `Fail()` |
| Детект vault/climb на сегменте (`UpdatePathSegmentState`) | `MoveTo` (прогресс-монитор/блокировка) | raycast block-filter + `IsVaultClimb` (0.5..10 м) + `IsElevated` (>0.5) → флаг `m_IsJumpClimb` → `TryVaultClimb()` |
| Константы | `cons/4_World/constants.c` | `DM_BOT_FALL_HEIGHT_LOW=5`, `DM_BOT_FALL_HEIGHT_HIGH=14`, `DM_BOT_CLIMB_LEAP_MAX=2.5`, `DM_BOT_LADDER_RADIUS=1.3`, `DM_BOT_LADDER_CLOSE=2.28`, vault/climb дистанция 0.5..10, cap лестничных циклов 10 |

**Ключевые решения для botorama**: (1) лестницы — чисто «поверх пути», entry-point
подставляется в цель `FindPath`/`SetMove`, а НЕ рассчитывается на то, что `FindPath` сам
проведёт по лестнице; (2) прыжок вниз — это **обратный** `FindPath` + проверка высоты, а не
флаг в нативном пути; (3) fall-защита — проверка только на последнем сегменте пути (чтобы не
застревать на верхних этажах).

### 7. Детект «внутри здания / под крышей» (для гипотезы «крыша vs внутренний этаж»)

**Задача**: игрок залез по ВНЕШНЕЙ лестнице на крышу; бот вместо лестницы зашёл ВНУТРЬ и
поднялся на 2-й этаж по внутренней. Нужно различать «цель на крыше» (открытое небо) и
«цель на внутреннем этаже» (перекрытие сверху), чтобы выбрать внешнюю лестницу, а не
внутренний маршрут.

**Ванильный кандидат-хак: `Man.IsSoundInsideBuilding()`**:

- `proto native bool IsSoundInsideBuilding();` — `3_game/entities/man.c:37` (без комментария
  в исходнике; ДУБЛЬ в `3_game/entities/dayzanimal.c:195`). Наследуется всей цепочкой
  `Man → Human → DayZPlayer → DayZPlayerImplement → ManBase → PlayerBase`
  (`manbase.c:1`, `playerbase.c:49`), а также `DayZAnimal`/`DayZCreatureAI`
  (`dayzanimal.c:191/195`). Т.е. `dmAISurvivorBase : PlayerBase` имеет её бесплатно.
- Семантика — **звуковой реверб/акустическое окружение** («внутри интерьера здания»), НЕ
  геометрический raycast. Подтверждается использованиями:
  - `environment.c:313-316` — `bool IsInsideBuilding()` ванили = `m_Player.IsSoundInsideBuilding()`
    (дождь/тепло-комфорт: «под крышей, но не обязательно внутри реверба»).
  - `dayzplayerimplement.c:3446` — `builder.AddVariable("interior", IsSoundInsideBuilding())`
    (payload аудио-события).
  - `dayzplayerimplement.c:3834` и `dayzanimal.c:384` — сравнение своего `IsSoundInsideBuilding()`
    с `GetPlayer().IsSoundInsideBuilding()` для детекта смены акустической среды.
  - `spookyareamisc.c:12/62` — условие спауна/деспауна «жуткой» звуковой зоны.
- **Вывод**: это сигнал «я внутри звукового объёма здания» → на открытой крыше его НЕТ
  (нет потолка-резонатора), на внутреннем этаже — есть. Значит, это дешёвый и прямой
  признак «внутри vs на крыше». Ограничения: (а) натив эвристический (зависит от расстановки
  звуковых объёмов в геометрии здания — крыша может частично давать реверб у парапета);
  (б) не даёт НАПРАВЛЕНИЯ/ТОЧКИ — только булев флаг позиции ПЕШКИ, поэтому для произвольной
  целевой точки на крыше его напрямую НЕ применить (нельзя «IsSoundInsideBuilding(targetPos)»);
  (в) вычисляется на аудио-потоке, возможна задержка смены значения. Рекомендация — как
  вторичный гейт для позиции САМОГО бота, а не цели.

**Ванильный паттерн «под крышей» (эталон для raycast) — `environment.c CheckUnderRoof()`**:

- `4_world/classes/environment/environment.c:391-411`:
  ```c
  protected void CheckUnderRoof()
  {
      if (IsChildOfType({Car})) { m_IsUnderRoof = false; m_IsUnderRoofBuilding = false; return; }
      float hitFraction; vector hitPosition, hitNormal;
      vector from = m_Player.GetPosition();
      vector to = from + "0 25 0";
      Object hitObject;
      PhxInteractionLayers collisionLayerMask = PhxInteractionLayers.ITEM_LARGE|PhxInteractionLayers.BUILDING|PhxInteractionLayers.VEHICLE;
      m_IsUnderRoof = DayZPhysics.RayCastBullet(from, to, collisionLayerMask, null, hitObject, hitPosition, hitNormal, hitFraction);
      m_IsUnderRoofBuilding = hitObject && hitObject.IsInherited(House);
  }
  ```
  - `m_IsUnderRoof` = ЛЮБОЙ хит вверх на 25 м (включая навесы/тент/дерево/ИТЕМ_LARGE);
    `m_IsUnderRoofBuilding` = хит является `House` (здание). Именно `m_IsUnderRoofBuilding`
    («под крышей ЗДАНИЯ») — сигнал «внутри интерьера/под потолком», нужный для нашего кейса.
  - Периодичность `GameConstants.ENVIRO_TICK_ROOF_RC_CHECK = 10` с (`3_game/constants.c:725`),
    значение **кэшируется** (обновляется в `CheckUnderRoof` по таймеру `m_RoofCheckTimer`,
    `environment.c:214-217`). Для произвольной точки/мгновенного решения НЕ пригоден — но сам
    raycast-паттерн — эталон для собственного пробника.
  - Готча кэша: `IsUnderRoof()`/`IsUnderRoofBuilding()` (`:305/:343`) возвращают СТАРОЕ
    значение между тиками (до 10 с). Для FollowTo-решения бот должен делать СВОЙ raycast вверх,
    а не читать `Environment`.

**`DayZPhysics.RayCastBullet` vs `RaycastRV`/`RaycastRVProxy`** (`3_game/global/dayzphysics.c`):

- `proto static bool RayCastBullet(vector begPos, vector endPos, PhxInteractionLayers layerMask, Object ignoreObj, out Object hitObject, out vector hitPosition, out vector hitNormal, out float hitFraction)` (`:211`) —
  **«пулевой» raycast по маске физических слоёв** (`PhxInteractionLayers`, `:1-43`: `ITEM_LARGE`,
  `BUILDING`, `VEHICLE`, `DOOR`, `TERRAIN`, `WATERLAYER`, `AI`, ...). Один ближайший хит:
  `hitObject` + `hitPosition` + `hitNormal` + `hitFraction`. Без радиуса, без surface-info,
  без component-index. Это ТО, что использует `CheckUnderRoof` (и `JumpClimb` для тестов).
- `proto static bool RaycastRV(vector begPos, vector endPos, out vector contactPos, out vector contactDir, out int contactComponent, set<Object> results = NULL, Object with = NULL, Object ignore = NULL, bool sorted = false, bool ground_only = false, int iType = ObjIntersectView, float radius = 0.0, CollisionFlags flags = CollisionFlags.NEARESTCONTACT)` (`:199`) —
  raycast с **радиусом** (`radius`, «толстый» луч), `iType` (`ObjIntersectFire/View/Geom/...`),
  `CollisionFlags` (`NEARESTCONTACT`), выдаёт `contactComponent` (индекс компонента — важно для
  `GetDoorIndex(result.component)`), `contactDir`. botorama УЖЕ использует её
  (`dmBotIntent_MoveTo.c:972/993` с `ObjIntersectGeom`, «лёгкий» геом-детект препятствия).
- `proto static bool RaycastRVProxy(notnull RaycastRVParams in, out notnull array<ref RaycastRVResult> results, array<Object> excluded = null)` (`:208`) —
  тот же физический движок, но **структурный** API: `RaycastRVParams` (`:49-92`: `begPos/endPos/
  ignore/with/radius/flags/type/sorted/groundOnly`) → `array<RaycastRVResult>` (`:98-113`:
  `obj/parent/pos/dir/hierLevel/component/surface/entry/exit`) + `excluded`. Поддержка **прокси-иерархии**
  (`hierLevel > 0` → объект = прокси). Основной raycast в botorama
  (`dmVision.c:431`, `MoveTo.c:722/826/1044/1179`, `Flank.c:270`, `EvadeAim.c:388`,
  `dmAISurvivor.c:825`).
- **Итог для детекта крыши**: для «есть ли потолок над точкой» правильнее `RayCastBullet`
  (лёгкий, маска `ITEM_LARGE|BUILDING|VEHICLE`, луч вверх `from → from + "0 25 0"`) + проверка
  `hitObject.IsInherited(House)` — ровно как `CheckUnderRoof`. НЕ `RaycastRVProxy` (даёт лишний
  оверхед прокси/поверхности, и маска-слои в нём не задаются — только `with`/`ignore`).

### 8. Navmesh крыши и поверхность запросов `AIWorld`

**Поверхность нативных запросов подтверждена** (`3_game/ai/aiworld.c` — весь файл 123 строки):

- `FindPath(vector from, vector to, PGFilter, out TVectorArray waypoints)` (`:98`) — путь с учётом
  фильтра, waypoints включают старт/конец.
- `RaycastNavMesh(vector from, vector to, PGFilter, out vector hitPos, out vector hitNormal)` (`:110`) —
  дока `:108` «returns true — if ray hits **navmesh edge**» (ловит РЁБРА, не плоскую поверхность;
  уже известно, см. готчу в «Сводка сигнатур»).
- `SampleNavmeshPosition(vector position, float maxDistance, PGFilter, out vector sampledPosition)` (`:122`) —
  ближайшая точка navmesh в радиусе `maxDistance`.
- **Больше ничего нет**: нет запроса флагов полигона, нет запроса «в каком полигоне/острове точка»,
  нет прямого «есть ли navmesh-связь между двумя этажами». `PGFilter` — только
  `GetIncludeFlags/GetExcludeFlags/GetExlusiveFlags/SetFlags/SetCost` (`:59-68`).

**Флаги полигона** (`PGPolyFlags`, `:1-26`): `WALK, DISABLED, DOOR, INSIDE, SWIM, SWIM_SEA,
LADDER, JUMP_OVER, JUMP_DOWN, CLIMB, CRAWL, CROUCH, UNREACHABLE, ALL, JUMP, SPECIAL`.
**Флага «крыша/ROOF» НЕТ** — крыша, если она walkable, — это обычные `WALK`-полигоны
(потенциально `INSIDE` не ставится, т.к. это открытое небо). Различение «крыша vs внутренний
этаж» на уровне флагов полигона **невозможно** — только по `INSIDE` (внутри здания) против
`WALK`-на-открытом.

**Связь лестницы с крышей в navmesh** — через `PGPolyFlags.LADDER` (`:13`): полигоны-«трапы»
лестниц маркируются `LADDER` и служат точками перехода между navmesh-«островами» (этажи/крыша).
A* проходит по лестнице ТОЛЬКО если `LADDER` включён в include фильтра (уже в
`ExpansionPathFilters` и в нашей рекомендации). Прямых подтверждений «крыша = отдельный
navmesh-остров» в скриптах нет (нативна генерация navmesh в движке, не в скриптах) — это
остаётся гипотезой, проверяемой эмпирически (см. §9).

**`ExpansionNavMesh`/`ExpansionNavMeshPolygon`** (`DayZExpansion/AI/.../Classes/NavMesh/*.c`) —
**НЕ ИИ-navmesh**, а самописная полигональная сетка для ТРАНСПОРТА (корабль/лодка), генерится из
geometry LOD (`Generate()`, `ExpansionNavMesh.c:27`, по 4-вершинным селекшенам). К навигации бота
по зданиям/крышам отношения не имеет. Флаги там читаются из конфига `NavMesh pN flags`
(`ExpansionNavMeshPolygon.c:28-30`), это НЕ `PGPolyFlags`.

### 9. Маршрутизация Expansion к крыше/этажу выше (проверено в исходниках)

Проверено по `eAIBase.c` (линии совпадают с §1 выше). **Расхождений с уже записанным нет**,
но подчёркиваю для текущего бага:

- Expansion **НЕ различает «крыша vs внутренний этаж»** ни через «under roof», ни через
  `IsSoundInsideBuilding`, ни через флаги navmesh. Единственный признак вертикали —
  **`|ΔY| > 1.5`** (`eAIBase.c:5007`) + **радиус здания с лестницей**
  (`eAI_CheckShouldUseBuildingWithLadder`, `:5111`, через `IsPointInCircle(center, radius, targetPos)`).
- Здание под ногами определяется `PhysicsGetFloorEntity()` → каст в `BuildingBase` с
  `Expansion_GetLaddersCount() > 0` (`:5015-5029`), НЕ по нахождению цели.
- Выбор лестницы — «process of elimination» по взвешиванию `Distance2DSq × |ΔY|` (`:4830-4831`),
  entry-point = низ (`m_Con[0]`) при подъёме / верх (`m_Con[1]`) при спуске. Пул лестниц
  здания (`m_eAI_Ladders`) с удалением использованных и капом циклов (`m_eAI_LadderLoops`).
- **Кейс «цель на крыше, крыша вне navmesh»** обрабатывается НЕ лестничной логикой, а
  **leap-of-faith** (`ExpansionPathPoint.FindPathFrom`, обратный `FindPath` + проверка высоты,
  §2) — то есть «прыгнуть вниз», а НЕ «залезть на крышу». Специального «залезть на крышу по
  внешней лестнице» механизма НЕТ — Expansion просто ведёт к entry-point ближайшей лестницы
  здания, в чей радиус попадает цель. Если внешняя лестница не выбирается — значит у Expansion
  та же уязвимость, что и у нас (внутренняя лестница ближе по `2D × ΔY`).
- Единственное употребление «under roof» в Expansion AI — `Expansion_IsUnderRoofBuilding()`
  (`environment.c:37`) в `eaistate_takeitem_base.c:115` — гейт «присесть, чтобы взять предмет»
  (не ложиться под крышей). К навигации/выбору лестницы НЕ относится.

**Вывод**: чтобы починить «крыша → внешняя лестница», одной лестничной логики Expansion мало —
нужен ДОПОЛНИТЕЛЬНЫЙ признак «цель на крыше, а не внутри» (raycast вверх + `House` из §7) и,
при нём, предпочтение лестницы, чей верхний конец ближе к ЦЕЛИ на крыше / чей entry находится
СНАРУЖИ здания (вне интерьера). У Expansion этого признака нет — это наш инкремент.

### 10. Гипотеза бага и эмпирические диагностики

**Гипотеза** (две независимые, могут действовать вместе):

1. **Крыша вне navmesh / отдельный остров** → `dmBotPathfinder.SamplePosition(roofPos, radius)`
   возвращает БЛИЖАЙШУЮ точку navmesh, которой оказывается внутренний этаж ПОД крышей (3D-ближайшая
   точка = пол под потолком), т.е. цель «прижимается вниз» на 2-й этаж ещё ДО `FindPath`.
2. **A* выбирает внутренний маршрут** — даже если крыша на navmesh, путь через внешнюю лестницу
   (13.7 м) длиннее/дороже, чем через вход внутрь (2.7 м) + внутреннюю лестницу, и `FindPath`
   отдаёт внутренний маршрут.

**Диагностики (порядок важен)**:

1. **Raycast вверх от цели** (`RayCastBullet`, маска `ITEM_LARGE|BUILDING|VEHICLE`, `from=roofTarget`,
   `to=roofTarget + "0 25 0"`): если есть хит и `hitObject.IsInherited(House)` — цель ВНУТРИ
   (потолок над ней), лестница не нужна/внутренний маршрут легитимен; если хита нет — цель на
   ОТКРЫТОЙ крыше → нужен внешний маршрут. Это мгновенный и дешёвый фильтр.
2. **`SampleNavmeshPosition(roofTarget, radius, filter, out sampled)`** и логировать
   `sampled[1] - roofTarget[1]`: большое отрицательное ΔY (>1 этаж) подтверждает гипотезу 1
   («прижало вниз»). Пробовать несколько радиусов (`DM_MOVE_GROUND_PROBE_RADIUS=0.5` и больше,
   например 2..5 м).
3. **`FindPath(botPos, roofTarget)`** в лоб: если путь вернулся, но последний waypoint ниже
   крыши или ведёт внутрь (координаты двери/внутренней лестницы) — гипотеза 2 (A* обходит
   лестницу). Логировать весь массив waypoint'ов + длины сегментов.
4. **`FindPath` до entry-point внешней лестницы** (низ `m_Con[0]`) и сравнить с путём до цели:
   если путь до entry существует, но путь до крыши идёт внутрь — маршрутизация, а не
   достижимость.
5. **`RaycastNavMesh(botPos, roofTarget, filter, ...)`** на сегменте «вход в здание»: подтвердит,
   что переход «снаружи → внутри» пересекает navmesh-ребро (дверь/клаймб), т.е. что крыша и
   интерьер — разные острова, связанные только через дверь/лестницу.
6. **`IsSoundInsideBuilding()` бота** как санкция: если после подъёма по ВНУТРЕННЕЙ лестнице бот
   даёт `true`, а при выходе на крышу ожидаем `false` — сигнал валиден на сервере AI_SERVER
   (проверить, что натив вообще работает для INSTANCETYPE_AI_SERVER, а не только для
   контролируемого игрока).

**Рекомендуемый детект «внутри здания / под крышей» для botorama**:

- Для **точки** (цель/waypoint): собственный raycast вверх `RayCastBullet(from, from+"0 25 0",
  ITEM_LARGE|BUILDING|VEHICLE, null, hit, hitPos, hitNormal, hitFrac)` + `hit && hit.IsInherited(House)`
  → `IsPointUnderRoofBuilding(pos)`. Это мгновенный эквивалент `CheckUnderRoof`, но без 10-секундного
  кэша (ванильный `Environment.m_IsUnderRoofBuilding` читать НЕЛЬЗЯ — он кэшированный и только для
  игрока).
- Для **позиции самого бота** (дешёвый вторичный гейт): `IsSoundInsideBuilding()` — `true` значит
  «внутри интерьера», `false` — «на открытом (крыша/улица)». Использовать как подтверждение, а не
  как единственный источник истины (эвристика + возможная задержка).
- В `MoveTo`/`FollowTo`: перед выбором лестницы, если `|ΔY(цель, бот)| > 1.5` И
  `!IsPointUnderRoofBuilding(цель)` → цель НА КРЫШЕ → выбирать лестницу с верхним концом,
  ближайшим к цели на крыше, и/или entry СНАРУЖИ интерьера; не прижимать цель `SamplePosition`
  вниз, пока не проверен raycast вверх.

---

# Газ-зоны и опасные места

Статус: баги botorama — бота телепортирует из газ-зоны («Персонаж перемещён из опасной
зоны»), бот наступает в костёр. Цель — понять, как ваниль детектит газ-зону и урон от
огня, и как ИИ избегать опасных мест (или освободить ИИ от телепорта). Ведёт
`dayz-research`.

## Цель

1. Газ-зона: какой ванильный класс детектит нахождение в зоне и телепортирует наружу;
   есть ли API проверить «позиция в зоне»; можно ли пометить ИИ-бота, чтобы его не
   телепортировали, или правильнее избегать зон в pathfinding.
2. Костёр/огонь: как ваниль задаёт урон (класс/радиус/урон), как определить опасную
   близость для избегания.

## Газ-зоны (contaminated areas)

### Ванильная архитектура (что и где)

- `EffectArea : House` — база зон (`4_world/classes/contaminatedarea/effectarea.c:48`).
- `ContaminatedArea_Base : EffectArea` (`contaminatedarea.c:1`), наследники
  `ContaminatedArea_Static` / `ContaminatedArea_Dynamic` (в том же файле /
  `contaminatedarea_dynamic.c:22`).
- Каждая зона держит триггер `m_Trigger` (тип из конфига), создаваемый в
  `EffectArea.CreateTrigger(pos, radius)` (`effectarea.c:482`): `SetCollisionCylinderTwoWay(radius, ...)`
  (`effectarea.c:497`). Триггер = вертикальный цилиндр (радиус `m_Radius`, высота
  `m_PositiveHeight`/`m_NegativeHeight`).
- Классы триггеров: `EffectTrigger : CylinderTrigger` (`effecttrigger.c:3`) →
  `ContaminatedTrigger` / `ContaminatedTrigger_Dynamic` / `ContaminatedTrigger_Local`
  (`contaminatedtrigger.c:2/92/176`).
- Загрузка статических зон: `EffectAreaLoader.CreateZones()` (`contaminatedarealoader.c:6`),
  файл `cfgeffectarea.json` (миссия, фолбэк `dz/worlds/<world>/ce/cfgeffectarea.json`).
  Данные — `JsonDataContaminatedAreas` (`EffectAreaLoader.GetData()`, `:109`): массив
  `Areas[]` (Pos/Radius/PosHeight/NegHeight/…) + `SafePositions` (куда телепортировать).
- Динамические зоны (артобстрел) — `ContaminatedArea_Dynamic`, радиус 120
  (`contaminatedarea_dynamic.c:129`), спавнят `Grenade_ChemGas` (`:37-38`).

### Как ваниль решает «в зоне или нет»

`EffectTrigger.CanAddObjectAsInsider(Object)` (`effecttrigger.c:90-107`) — гейт входа в триггер:

```c
#ifdef SERVER
DayZCreatureAI creature = DayZCreatureAI.Cast(object);
if (creature)
    return !creature.ResistContaminatedEffect();
else {
    PlayerBase player = PlayerBase.Cast(object);
    return player != null;
}
#else
PlayerBase player = PlayerBase.Cast(object);
return (player && player.IsControlledPlayer());
#endif
```

**Важно для botorama**: `dmAISurvivorBase : PlayerBase` (НЕ `DayZCreatureAI` —
`DayZCreatureAI extends DayZCreature`, `3_game/entities/dayzanimal.c:191`). Поэтому бот
попадает в ветку `PlayerBase` → возвращает `true` → бот становится инсайдером зоны и
проходит ПОЛНЫЙ путь игрока (модификатор + телепорт), а НЕ усечённый путь существа.

Цепочка входа (сервер):
1. `EffectTrigger.OnEnterServerEvent` (`effecttrigger.c:117`) → `m_Manager.OnPlayerEnter(player, this)`.
2. `TriggerEffectManager.OnPlayerEnter` (`triggereffectmanager.c:41`) при первом входе в
   тип триггера → `trigger.GetEffectArea().OnPlayerEnterServer(player, trigger)`.
3. `ContaminatedArea_Base.OnPlayerEnterServer` (`contaminatedarea.c:3`) → `super` (даёт
   `IncreaseEffectAreaCount()`) + `player.IncreaseContaminatedAreaCount()`.
4. `PlayerBase.IncreaseContaminatedAreaCount()` (`playerbase.c:807`) при 0→1 →
   `OnContaminatedAreaEnterServer()` (`playerbase.c:915`) →
   `GetModifiersManager().ActivateModifier(eModifiers.MDF_AREAEXPOSURE)`.
5. `AreaExposureMdfr.OnActivate` (`areaexposure.c:39`) → телепорт-чек + химический яд.

### Телепорт из зоны

`MiscGameplayFunctions.TeleportCheck(PlayerBase, safe_positions)` (`miscgameplayfunctions.c:1690`):

```c
if (player.GetSimulationTimeStamp() < 20 && !player.IsPersistentFlag(PersistentFlag.AREA_PRESENCE))
{
    vector player_pos = player.GetPosition();
    vector closest_safe_pos = GetClosestSafePos(player_pos, safe_positions);
    if (player_pos != closest_safe_pos) {
        closest_safe_pos[1] = g_Game.SurfaceY(...);
        player.SetPosition(closest_safe_pos);            // :1704
        g_Game.RPCSingleParam(player, ERPCs.RPC_WARNING_TELEPORT, ...); // :1706
        ...
    }
    player.SetPersistentFlag(PersistentFlag.AREA_PRESENCE, false); // :1713
}
```

- Срабатывает **только** когда `GetSimulationTimeStamp() < 20` (сущность «недавно
  заспавнилась») И нет флага `AREA_PRESENCE` (не была в зоне на дисконнекте).
- `RPC_WARNING_TELEPORT` → клиент открывает `MENU_WARNING_TELEPORT` (=41,
  `3_game/constants.c:210`) — сообщение «Персонаж перемещён из опасной зоны»
  (`playerbase.c:5533-5538`, `5_mission/mission/missionbase.c:307`).
- Точка выноса — `GetClosestSafePos` по `data.SafePositions` (`miscgameplayfunctions.c:1717`).

> **Открытый вопрос**: для `INSTANCETYPE_AI_SERVER`-пешки продвигается ли
> `GetSimulationTimeStamp()`? Если STS у бота не растёт (или сбрасывается при каждом
> спавне) — условие `< 20` будет истинным и бот будет телепортироваться при КАЖДОМ входе
> в зону. Нужно проверить эмпирически (лог STS в `OnContaminatedAreaEnterServer`).

### Урон в газ-зоне (две ветки)

- **`DayZCreatureAI` (зомби/животные)** — прямой урон: `ContaminatedTrigger.OnStayServerEvent`
  (`contaminatedtrigger.c:74-82`) → `creature.DecreaseHealth("", "", GameConstants.AI_CONTAMINATION_DMG_PER_SEC * m_TimeAccuStay / creature.m_EffectTriggerCount)`;
  `AI_CONTAMINATION_DMG_PER_SEC = 3` (`3_game/constants.c:1070`); тик раз в
  `DAMAGE_TICK_RATE = 10 c` (`contaminatedtrigger.c:4`). Счётчики входа/выхода —
  `DayZCreatureAI.Increase/DecreaseEffectTriggerCount()` (`dayzanimal.c:222/227`).
- **`PlayerBase` (наш бот)** — не прямой `DecreaseHealth`, а химический яд через
  `AreaExposureMdfr` (`areaexposure.c`): `TransmitAgents` (`AGT_AIRBOURNE_CHEMICAL` →
  `eAgents.CHEMICAL_POISON`, `:141-145`), кашель, кровотечения. Тик модификатора у
  AI-бота — вручную (`GetModifiersManager().OnScheduledTick`, см. skill).

### Есть ли API «позиция в зоне»?

- **Нативного `IsInGasZone(pos)`/`GetGasZoneAt(pos)` в ванили НЕТ.**
- Статические зоны можно прочитать из JSON: `EffectAreaLoader.GetData()`
  (`contaminatedarealoader.c:109`) → `Areas[].Data.Pos/Radius/PosHeight/NegHeight`. Минусы:
  `m_Path` приватный, `GetData()` перечитывает файл; динамические зоны в JSON не входят.
- Живого реестра зон в ванили нет — каждая зона сама владеет своим `m_Trigger`.
- `TriggerEffectManager` (`triggereffectmanager.c:3`) хранит только «какой игрок в каком
  типе триггера» (`m_PlayerInsiderMap`), а НЕ список зон по позициям.

### Способы решить для botorama

1. **Заглушить у ИИ целиком (освободить от телепорта и газа)**: override
   `OnContaminatedAreaEnterServer()` в `dmAISurvivorBase` как no-op (НЕ звать
   `ActivateModifier(MDF_AREAEXPOSURE)`). Метод `protected` на `PlayerBase`
   (`playerbase.c:915`), переопределяется. Убирает И телепорт И химический яд. Минус:
   бот вообще не чувствует газ (нужно решать, допустимо ли это геймдизайнерски).
2. **`ResistContaminatedEffect()` — НЕ подходит**: метод есть только на `DayZCreatureAI`
   (`dayzanimal.c:394`); бот — `PlayerBase`, эта ветка (`effecttrigger.c:93-97`) для него
   не выполняется.
3. **Избегание в pathfinding** — правильный вариант, эталон Expansion (см. ниже).

### Expansion AI избегание (эталон для порта)

Файлы: `DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Classes/contaminatedarea/*`.

- `modded class EffectArea` (`effectarea.c`): статик `s_Expansion_DangerousAreas`
  (`Expansion_EffectAreas`) — глобальный список всех опасных зон; опасные типы
  `{ContaminatedArea_Base, GeyserArea, HotSpringArea, VolcanicArea}` (`:3`); зона
  добавляется в `InitZoneServer` (`:8-18`) и вычищается в `EEDelete`/`~EffectArea`
  (`Expansion_RemoveDangerousArea`, `:76`).
- Точечные проверки на зоне: `Expansion_IsPointInside(vector, tolerance, heightTolerance)`
  (`effectarea.c:173`), `Expansion_IsOverlapping` (`:117`), `Expansion_Contains` (`:142`).
- `eAI_IsDangerousToAI(eAIBase)` (`effectarea.c:184`, override `ContaminatedArea_Base`
  в `contaminatedarea.c:3`): `ai.m_eAI_ProtectionLevels[DEF_CHEMICAL] < 6.0 && !faction.IsInvincible()`.
- Кластеры пересекающихся зон → один цилиндр: `ExpansionEffectAreaMergedCluster`
  (`expansioneffectareacluster.c:280`). Константы: `MAX_AVOIDANCE_DISTANCE=30`,
  `MIN_AVOIDANCE_DISTANCE=3`, `IS_INSIDE_MARGIN=3` (`:282-284`);
  `GetAvoidanceDistance(radius) = max(min(radius*0.2, 30), 3)` (`:892`).
- Двигать конечную точку пути за цилиндр: `FindClosestPointOutsideCluster(start, inout end, avoidanceDirection, ignoreHeight, out closestArea)`
  (`:792`), обёртка `FindClosestPointOutsideAnyCluster` (`:910`); `FindClosestPointOutsideAnyArea` (`:224`).
- Реактивный вход: `eAIBase.Expansion_OnDangerousAreaEnterServer(area, trigger)`
  (`eAIBase.c:4654`) выбирает сторону обхода (left/right/random через
  `FindClosestPointOutsideCluster`); в `OverrideTargetPosition` (`eAIBase.c:4906-4959`)
  при `m_eAI_IsInDangerByArea` подменяет цель пути на точку за пределами кластера
  (`m_eAI_IsInDangerByArea` ставится в `eAI_CheckIsInDangerByArea`, `eAIBase.c:5269`).
- Готовый «no-go» триггер: `ExpansionAINoGoArea`/`ExpansionAINoGoAreaTrigger`
  (`expansionainogoarea.c:1/55`) — `CylinderTrigger`, инсайдеры только `eAIBase`,
  зовёт `ai.Expansion_OnDangerousAreaEnterServer`. Паттерн «пометь зону недоступной для ИИ».

**Маппинг на botorama**: у нас `dmBotPathfinder` (обёртка `AIWorld.FindPath`) без
кластеров. Минимальный порт: (а) реестр статических зон из `EffectAreaLoader.GetData()`
(плюс динамические — если доступны), (б) `IsPointInside(point)` по цилиндру, (в) при
`MoveTo` — если сегмент/цель пересекает зону, сдвинуть подцель по
`FindClosestPointOutsideRadius`-аналогу (ExpansionMath) или, проще, `Fail()`/пере-выбор
точки. Это дороже, чем вариант 1 (заглушка), поэтому выбор — по приоритету задачи.

## Костёр / огонь (опасные места)

### Ванильный урон от костра

`FireplaceBase : ItemBase` (`4_world/entities/itembase/fireplacebase.c:21`). Урон по площади
делает `m_AreaDamage` (`AreaDamageManager`), создаётся в `CreateAreaDamage()` (`:2331`):

- `AreaDamageLoopedDeferred` — триггер-**бокс**, `SetExtents("-0.30 0 -0.30", "0.30 0.75 0.30")`
  (`:2339`) — ~0.6×0.6×0.75 м вокруг центра костра (локально, от `GetPosition()` костра).
- `SetLoopInterval(0.5)` (`:2340`), `SetDeferDuration(0.5)` (`:2341`),
  `SetHitZones({Head,Torso,LeftHand,LeftLeg,LeftFoot,RightHand,RightLeg,RightFoot})` (`:2342`),
  `SetAmmoName("FireDamage")` (`:2343`), `Spawn()`.
- Создаётся в `StartHeating()` (`:1793`) и по `OnVariablesSynchronized` (`:511-519`),
  уничтожается в `DestroyAreaDamage()` (`:2347`) при остановке горения / `OnItemLocationChanged`.
- Навмеш: `StartFire` → `SetAffectPathgraph(false, true)` + `UpdatePathgraphRegionByObject`
  (`:1763-1768`) — горящий костёр меняет pathgraph-регион (влияет на `FindPath`).
- Тепло (НЕ урон, комфорт/прогрев): `PARAM_FULL_HEAT_RADIUS = 2.0`, `PARAM_HEAT_RADIUS = 4.0`
  (`:55-56`) — радиусы `UniversalTemperatureSource`. Не путать с уроном.

### AreaDamage API (как урон применяется)

- `AreaDamageManager` (`4_world/classes/areadamage/areadamagenew/areadamagemanager.c:8`):
  `SetExtents/GetExtents/GetWorldExtents` (`:233-254`), `SetAmmoName` (`:256`),
  `SetLoopInterval` (`:331`), `SetDeferDuration` (`:336`), `SetHitZones` (`:341`),
  `GetParentObject` (`:278`), `GetPosition` (`:292`), `Spawn/Destroy`.
- `AreaDamageComponent.ShouldDamage(object)` (`damagecomponents/areadamagecomponent.c:46`):
  `object.IsAlive() && object.IsAnyInherited(m_DamageableTypes)`; дефолт
  `m_DamageableTypes = {DayZPlayer}` (`:24`) → бот (PlayerBase) под ударом.
- Применение: `object.ProcessDirectDamage(m_DamageType, parent, hitzone, m_AmmoName, modelpos, damageCoef)`
  (`:67`); `damageCoef = deltaTime` (`AreaDamageLoopedDeferred.CalculateDamageScale`,
  `areadamageloopeddeferred.c:12`; базовый — `areadamagemanager.c:224`).
- Триггер — `AreaDamageTriggerBase : Trigger` (`areadamagetriggerbase.c:13`), серверный
  бокс-триггер, инсайдеры только на сервере (`:224-248`).

### Другие «горящие» опасные места

- `Misc_TirePile_Burning_DE : BuildingSuper` (`misc_tirepile_burning.c:1`): `SetExtents("-2.0 0 -2.0", "2.0 2.0 2.0")`
  (`:97`) — бокс **4×4×2 м** (заметно больше костра), `FireDamage`, loop 0.5, defer 0.5 (`:98-101`).
- `BarrelHoles_ColorBase : FireplaceBase` (`barrelholes_colorbase.c:1`) — бочка с дырками,
  свой `CreateAreaDamage()` (`:59`), температура `PARAM_OUTDOOR_FIRE_TEMPERATURE`.

### Как определять опасную близость (для избегания)

- Урон по костру — только в боксе `±0.30 м` горизонтально от центра; «наступить в костёр»
  = `Distance2D(botPos, fireplace.GetPosition())` меньше ~0.5 м (с запасом на корпус бота).
  Достаточно: `fireplace.IsBurning()` (`fireplacebase.c:1623`, публичный) + дистанция до центра.
- Более мягкий признак (не только урон, но и «горячо») — `GetTemperature()` / `IsOven()`
  (`:1654`). Для полного избегания горящего костра брать радиус ≥ 0.5 м (или `PARAM_FULL_HEAT_RADIUS=2.0`,
  если хотим избегать и теплового дискомфорта).

### Обнаружение костра вокруг точки маршрута

- Ванильного «реестра костров по позиции» НЕТ. Варианты для botorama:
  1. Box-запрос вокруг waypoint'а (как `dmVision`): `SceneGetEntitiesInBox`/
     `PhysicsGetEntitiesInBox` → `FireplaceBase.Cast` / `IsInherited(FireplaceBase)` →
     `IsBurning()`.
  2. Ограниченный перечень классов: `Fireplace`/`FireplaceIndoor`/`BarrelHoles_ColorBase`
     (`FireplaceBase`-наследники) + `Misc_TirePile_Burning_DE`.
- Практичнее совместить с общим «опасным фильтром» в `MoveTo`: перед шагом на подцель —
  бокс-запрос по радиусу `DM_BOT_DANGER_AVOID_RADIUS` и сдвиг подцели (как у газ-зон).

## Открытые вопросы

1. **STS у AI_SERVER-бота**: продвигается ли `GetSimulationTimeStamp()` — определяет,
   телепортируется ли бот из зоны всегда или только при спавне внутри зоны. Проверить
   логом в `OnContaminatedAreaEnterServer`.
2. **Динамические газ-зоны**: как зарегистрировать их для pathfinding-избегания
   (в JSON их нет; в Expansion они приходят через `InitZoneServer` мода `EffectArea`).
3. **Костёр в инвентаре/в руках**: `CreateAreaDamage` создаётся только на ground
   (`OnItemLocationChanged` → `DestroyAreaDamage` при уходе с ground, `fireplacebase.c:361-369`),
   т.е. «горящий костёр в руках» урона по площади не даёт — проверить.
4. **`FireDamage` ammo**: точные цифры урона — в CfgAmmo вне скриптов (в репо не найден),
   нужен `data/` для точного DPS.
