# Research: посадка в транспорт (vehicle boarding) для ИИ-бота

Источник: ванильный DayZ (DayZ-Script-Diff) + Expansion AI (FSM-стейты `vehicles/`).

## API

- `Transport` (`3_game/vehicles/transport.c`, `class Transport : EntityAI`):
  - `int CrewSize()` — число мест.
  - `Human CrewMember(int posIdx)` — экипаж на месте; `null` = свободно.
  - `void CrewEntryWS(int posIdx, out vector pos, out vector dir)` — точка входа (дверь) в world-space.
  - `int GetSeatAnimationType(int posIdx)`, `int GetAnimInstance()` — тип сидячей анимации / тип транспорта.
  - `CrewGetIn(Human, int)` — прямой телепорт в кресло (без анимации, сервер); аналог «мгновенно».
- `Human.StartCommand_Vehicle(Transport, int seatIdx, int seatAnimType)` → `HumanCommandVehicle`
  (натив, `3_game/human.c:1486`); `HumanCommandVehicle.SetVehicleType(int)`.
- `PlayerBase.IsInTransport()` = `COMMANDID_VEHICLE` или `GetParent().IsInherited(Transport)`
  (`4_world/entities/dayzplayerimplement.c:486`); `GetParent()` → `Transport`.

## Ванильный вход игрока

`ActionGetInTransport.Start` (`actiongetintransport.c:82`): `crew_index = CrewPositionIndex(component)`
(от двери) → `seat = GetSeatAnimationType(crew_index)` → `StartCommand_Vehicle(trans, crew_index, seat)`
→ `vehCommand.SetVehicleType(trans.GetAnimInstance())`. Гейт `ActionCondition` требует
`CanReachSeatFromDoors(...) <= 1.0` — игрок должен быть у двери.

## Эталон Expansion

FSM-цепочка (`Classes/FSM/states/vehicles/`):
1. `eAIState_FindVehicle` — выбор транспорта.
2. `eAIState_GoToVehicle` — `m_Transport.CrewEntryWS(m_Seat, pos, dir)` → `OverrideTargetPosition(pos)`
   (идёт к точке входа).
3. `eAIState_GetInVehicle` — ждёт idle (`Expansion_IsAnimationIdle`), затем
   `Notify_Transport(transport, seat)` → в `CommandHandler` (`eAIBase.c:7407`):
   `seat_anim = transport.GetSeatAnimationType(seat)` → `StartCommand_Vehicle(transport, seat, seat_anim)`
   + `SetVehicleType(transport.GetAnimInstance())`.

## Маппинг на botorama

- Примитив пешки `dmAISurvivorBase.GetInVehicle(Transport, int seat)`:
  `GetSeatAnimationType(seat)` → `StartCommand_Vehicle(transport, seat, seatAnim)` →
  `SetVehicleType(transport.GetAnimInstance())`. Возвращает false, если команда не стартовала.
- Интент `dmBotIntent_GetInVehicle : dmBotIntent_MoveTo`: `m_Goal = CrewEntryWS(seat)`,
  `m_ReachDistance = DM_GETIN_REACH`, `OnReachedGoal` → `GetInVehicle` (иначе `Fail`) → `Finish`.
  Команда транспорта дальше владеет корпусом на уровне движка сама.
- Поиск машины с игроком (`/bot car sitdown`): итерируем `dmEntityRegistry.GetPlayers()`,
  берём живого НЕ-бота (`dmAISurvivorBase.Cast(p) == null`), у которого
  `Transport.Cast(p.GetParent()) != null` (сидит в транспорте); ищем свободное место
  (`CrewMember(s) == null`).

## Выход из машины (get-out) — позиция пассажира

Цель: понять, что делает ваниль/Expansion при выходе, почему у серверного ИИ позиция
на один кадр ломается в `(0,0,0)` и что вызвать до/после `GetOutVehicle()` в botorama.

### Ванильный `ActionGetOutTransport` (`4_world/.../actiongetouttransport.c`)

- `ActionCondition` (`:68-82`) требует `trans.CrewCanGetThrough(crewIndex)` **и**
  `trans.IsAreaAtDoorFree(crewIndex)` — дверь свободна.
- `OnStart` (`:161-186`): `ProcessGetOutTransportActionData` (скорость → jump-out порог),
  затем `vehCommand.GetOutVehicle()` (обычный) **или** `vehCommand.JumpOutVehicle()` (на ходу).
  **Позицию в скрипте ваниль НЕ выставляет** — размещение у двери делает натив
  `GetOutVehicle()`. Доп. действия: `LockInventory(LOCK_FROM_SCRIPT)`, `OnLeaveCar()`.
- `OnUpdate` (`:203-210`): экшен завершается когда `!GetCommand_Vehicle()` (команда кончилась).
- `OnEndServer` (`:238-255`): только урон за jump-out; позиции не трогает.

Вывод: точка выхода рассчитывается нативом, скрипт её не снапает.

### API `HumanCommandVehicle` (`3_game/human.c:689-734`)

| метод | назначение |
|---|---|
| `GetTransport()` (`:694`) | транспорт-родитель |
| `GetVehicleSeat()` (`:696`) | индекс текущего кресла |
| `GetOutVehicle()` (`:700`) | обычный выход (анимация + размещение у двери) |
| `JumpOutVehicle()` (`:703`) | выпрыгивание (с уроном) |
| `KnockedOutVehicle()` (`:701`) | бессознательное состояние в машине |
| `SwitchSeat(i, seat)` (`:704`) | пересадка |
| `IsGettingIn/Out()` (`:705-706`), `IsSwitchSeat()` (`:707`) | флаги переходной анимации |
| `KeepInVehicleSpaceAfterLeave(bool)` (`:710`) | **остаться ли в локальном пространстве транспорта после выхода** |
| `ProcessLeaveEvents()` (`:711`) | доставить отложенные leave-события (см. ниже) |
| `IsObjectIgnoredOnGettingOut(entity)` (`:713-733`) | скрипт-хелпер проверки «не блокирует ли дверь» |

`KeepInVehicleSpaceAfterLeave` — это про то, **в каком координатном пространстве** остаётся
пешка после detach: если `true`, пешка остаётся припарентенной к транспорту (её
`GetPosition()` продолжает быть в vehicle-local). Используется только в пути смерти:
`m_PullPlayerOutOfVehicleKeepsInLocalSpace` (`dayzplayerimplement.c:184`) →
`StartCommand_Death(..., keepInLocalSpace)` (`:638`) / `m_TransportCache.CrewGetOut` (`:681`).
Для обычного выхода дефолт не подтверждён по скриптам (натив), логично явно ставить `false`.

`ProcessLeaveEvents()` вызывается ванилью **только** в edge-case: выход под водой → сразу
swim (`dayzplayerimplement.c:2316-2330`), т.к. не все leave-события успели отработать.
Для наземного выхода не обязателен.

### Точка выхода

Отдельного `GetCrewExit` **нет**. Точка выхода = точка входа у двери:
- `CrewEntry(posIdx, out pos, out dir)` — model-space (`transport.c:126`),
- `CrewEntryWS(posIdx, out pos, out dir)` — **world-space** (`transport.c:129`),
- `CrewTransform/WS(posIdx, out mat[4])` (`:132/:135`), `CrewGetIn/Out` (`:138/:141`).

`CrewGetOut(posIdx)` (натив, «transfer of player from vehicle into world») используется
только в смерти/вытаскивании (`dayzplayerimplement.c:572`, `:681`), НЕ в обычном get-out.
Expansion ходит к `CrewEntryWS` и при входе (`eaistate_gotovehicle.c:52`).

### `GetPosition()` vs `GetWorldPosition()` (корень симптома `(0,0,0)`)

`3_game/entities/object.c:293-297`:
- `GetPosition()` — «Retrieve position» (у припарентенной сущности — **hierarchy-local**,
  для пассажира = vehicle-local);
- `GetWorldPosition()` — «Returns world position. **Takes proxy transformation into account**».

Пешка в `HumanCommandVehicle` припарентена к транспорту. В кадр detach'а (после
`GetOutVehicle()`, до перестройки world-transform) `GetPosition()` на один кадр читает
vehicle-local `(0,0,0)`, тогда как `GetWorldPosition()` уже/ещё корректен. На клиенте это
чинит сетевая reconciliation; у серверного ИИ (без клиента) кадр `(0,0,0)` протекает в
`ApplyMovement` (лог `posDelta=10949.4` = дистанция до `(0,0,0)`), а `GetPosition()` из
мозга в тот же кадр успевает прочитать корректно (фаза тика до detach).

### Expansion get-out (эталон)

`eaistate_getoutvehicle.c:5-37` (наследует `eAIState_GoToVehicle`):
- `OnEntry`: `if (vehCmd && !vehCmd.IsGettingIn())` → `vehCmd.GetOutVehicle();`
  `ExpansionVehicle.ReserveSeat(vehCmd.GetVehicleSeat(), null);`
  `unit.LookAtDirection("0 0 1");` // сброс снапа головы после выхода.
- `OnUpdate`: `CONTINUE` пока `IsGettingOut()` или `< 2.0 s`, затем `EXIT`.

**Позицию Expansion тоже не снапает** — полагается на натив, как ваниль. Дополнительно:
- `eAIBase.c:6795-6801`: пока getting in/out — `LookAtDirection("0 0 1")` + `AimAtDirection("0 0 1")`, прицел/голова заморожены.
- `eAIBase.c:10013-10020` (HeadingModel): при `IsGettingOut/In` → `NoHeading`.
- `ReserveSeat` — чисто FSM-букинг сидений (`expansionvehicle.c:3-18`), к позиции отношения не имеет.

### «Выход для ИИ» в ванили

Отдельного выхода нет — только пользовательский `ActionGetOutTransport`. Зомби **не**
выбивают игрока: `zombiebase.c:683` и `:724` просто **пропускают атаку** игрока в машине
(«do not attack players in vehicle - hotfix»). Выброс из машины бывает только при смерти:
`CrewGetOut` (`dayzplayerimplement.c:572`) + `DisableSimulation(false)` +
сетевая синка (`TriggerPullPlayerOutOfVehicleImpl` `:593-611`, `OnVariablesSynchronized` `:613-621`).
Для серверного ИИ сетевой синки нет → позицию после выхода обязан восстановить натив/скрипт.

## Рекомендация для botorama (`dmAISurvivorBase.GetOutVehicle()` + интент)

Сейчас примитив (`dmAISurvivorBase.c:1039-1054`) делает **только** `cmd.GetOutVehicle()`,
а интент (`dmBotIntent_GetInVehicle.c:79-101`) просто ждёт `m_CommandFinishWait`.

1. **До** `GetOutVehicle()`: заснапить точку выхода и запретить «зависание» в vehicle-space:
   ```c
   Transport t = cmd.GetTransport();
   t.CrewEntryWS(cmd.GetVehicleSeat(), m_ExitPos, m_ExitDir); // world-space, дверь
   cmd.KeepInVehicleSpaceAfterLeave(false);                   // натив; явно вернуть в world
   ```
2. **После** `GetOutVehicle()` (в интенте, пока `IsGettingOut()`): не читать `GetPosition()`,
   а по завершении (`!cmd || !cmd.IsGettingOut()`):
   ```c
   cmd.ProcessLeaveEvents();          // безопасно; обязателен только при выходе в воду
   SetPosition(m_ExitPos);            // после detach (не припарентена) — это world
   PlaceOnSurface();                  // на землю, если выход на склоне/бровке
   LookAtDirection("0 0 1");          // как Expansion — сброс снапа головы
   ```
   `SetPosition` применять **только после detach** (когда `GetParent()` уже не транспорт),
   иначе это запишет vehicle-local координату.
3. **Чтение позиции**: в per-frame логике (`ApplyMovement` `dmAISurvivorBase.c:838`, перцепция,
   стейты) на время «в машине / выходит» использовать `GetWorldPosition()` (учитывает
   parent/proxy) вместо `GetPosition()`, либо гейтить на `GetCommand_Vehicle() &&
   GetCommand_Vehicle().IsGettingOut()` — это убирает однокадровый `(0,0,0)` и ложный
   `posDelta`.

Открытые вопросы:
- Дефолт `KeepInVehicleSpaceAfterLeave` для обычного выхода в скриптах не виден (натив);
  проверить на тесте, меняет ли что-то явный `false` для серверного ИИ.
- Точный кадр detach'а на сервере (когда `GetParent()` становится `null`) — проверить
  пер-фрейм логом `GetParent() == null` vs `GetWorldPosition()`.

## Готчи / замечания

- `StartCommand_Vehicle` требует близость к двери (ванильный гейт `CanReachSeatFromDoors <= 1.0`),
  поэтому `DM_GETIN_REACH = 1.0` и маршрут к `CrewEntryWS` (не к позиции транспорта).
- `FindPathTo` снапает цель на navmesh — точка входа у двери обычно на navmesh, но у края дороги
  может снапаться на ~1м — поэтому достижимость 1.0, а не 0.05.
- Боты регистрируются в `dmEntityRegistry` как `PlayerBase` — при поиске «игрок в машине»
  обязательно исключать `dmAISurvivorBase` (иначе найдём машину, где сидит другой бот).
