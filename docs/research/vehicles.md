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

## Двери автомобиля

Дверь машины = **анимационная фаза** на `CarScript` (`4_world/entities/vehicles/carscript.c`).
Отдельного «открыть/закрыть дверь» натива нет — дверь гоняется через `SetAnimationPhase`
(натив `Entity`), а имя анимации получается по месту (`seat`) через цепочку:

```c
CarScript car = CarScript.Cast(transport);
string sel  = car.GetDoorSelectionNameFromSeatPos(seat);  // имя селекшена двери
string slot = car.GetDoorInvSlotNameFromSeatPos(seat);    // имя инвентарного слота
if (car.GetCarDoorsState(slot) == CarDoorState.DOORS_MISSING) // двери нет/снята
    return;
string anim = car.GetAnimSourceFromSelection(sel);         // имя анимации
car.SetAnimationPhase(anim, 1.0);                          // 1.0 = открыта, 0.0 = закрыта
```

- `CarDoorState` — `enum { DOORS_MISSING, DOORS_OPEN, DOORS_CLOSED }`.
- Текущая фаза — `car.GetAnimationPhase(anim)` (0..1, интерполируется).
- Ванильный порог «открыта» — `GetAnimationPhase(anim) > 0.5`
  (см. `CarScript.TranslateAnimationPhaseToCarDoorState`).
- Снап на землю — `GetGame().SurfaceY(x, z)` (натив `Game`, `game.c:1152`): возвращает
  высоту поверхности по горизонтальным координатам:
  `pos[1] = GetGame().SurfaceY(pos[0], pos[2]);` (ванильный идиом, напр. `actionrepacktent.c:21`).

## Готчи / замечания

- `StartCommand_Vehicle` требует близость к двери (ванильный гейт `CanReachSeatFromDoors <= 1.0`),
  поэтому `DM_GETIN_REACH = 1.0` и маршрут к `CrewEntryWS` (не к позиции транспорта).
- `FindPathTo` снапает цель на navmesh — точка входа у двери обычно на navmesh, но у края дороги
  может снапаться на ~1м — поэтому достижимость 1.0, а не 0.05.
- Боты регистрируются в `dmEntityRegistry` как `PlayerBase` — при поиске «игрок в машине»
  обязательно исключать `dmAISurvivorBase` (иначе найдём машину, где сидит другой бот).

## Вождение: газ/передачи/RPM/руль

Цель: подтвердить сигнатуры и поведение API `Car`/`CarScript` для «бот водит автомобиль»
(газ, передачи АКПП/МКПП, RPM, руль, толчок импульсом, заводится ли без водителя).

Источники: `scripts/3_game/vehicles/car.c`, `transport.c`, `4_world/entities/vehicles/carscript.c`,
`3_game/dayzplayer.c`, `1_core/proto/enphysics.c`; эталон — Expansion
`DayZExpansion_AI/.../Entities/CarScript.c` (AI-вождение) и `CarScript_Towing.c`.

### Ключевой хук: `Transport.OnInput(float dt)`

`OnInput` объявлен на `Transport` (`3_game/vehicles/transport.c:206`) как no-op virtual,
с комментарием «Called after every input simulation step. ... It is highly recommended to
store state of custom inputs elsewhere and call Setters here.» Именно в override'е `OnInput`
Expansion гоняет AI-машину (ниже). Это **главная точка входа для скриптового вождения** —
не `OnUpdate`, не `EOnPostSimulate`.

### 1. Передачи: АКПП vs МКПП

Сигнатуры (`3_game/vehicles/car.c`):

- `proto native void ShiftUp();` (`:264`)
- `proto native void ShiftTo(int gear);` (`:267`) — «Shifts the future gear to selected gear»
- `proto native void ShiftDown();` (`:270`)
- `proto native CarGearboxType GearboxGetType();` (`:273`) — `MANUAL|AUTOMATIC` (`:34-38`)
- `proto native CarAutomaticGearboxMode GearboxGetMode();` (`:276`) — «useful when car has automatic gearbox», `P|R|N|D` (`:68-74`)
- `proto native int GetCurrentGear();` (`:252`) — «current gear, -1 if there is no engine»
- `proto native int GetGear();` (`:255`) — «future gear, -1 if there is no engine»
- `proto native int GetNeutralGear();` (`:258`), `proto native int GetGearCount();` (`:261`)
- `CarGear` enum (`:43-63`): `REVERSE, NEUTRAL, FIRST..SIXTEENTH`.

**Как ванильный игрок переключает передачу:** НЕ через `ShiftUp/ShiftDown/ShiftTo` (в ванильном
скрипте эти методы **нигде не вызываются** — grep даёт только объявления в `car.c`/`boat.c`).
Передача гоняется нативно через `HumanCommandVehicle`:

- `4_world/entities/dayzplayerimplement.c:2384-2394` — в `COMMANDID_VEHICLE`:
  `if (hcv.WasGearChange())` → `AddCommandModifier_Action(CMD_ACTIONMOD_SHIFTGEAR, GearChangeActionCallback)`.
- `4_world/entities/dayzplayerimplementvehicle.c:1-25` — `GearChangeActionCallback` только
  дергает `hcv.SetClutchState(true)` при старте и `false` на `OnFinish`. Само переключение
  делает движок по вводу (`UACarShiftGearUp/Down`, `5_mission/.../dayzspectator.c:14-16`).

**Работает ли `ShiftTo` на автомате:** `ShiftTo(int gear)` принимает произвольный int; на МКПП
передаётся `CarGear`, на АКПП — `CarAutomaticGearboxMode` (это отдельные enum'ы, но оба int).
Прямое доказательство из Expansion towing (`CarScript_Towing.c:65-73`):

```c
if (GearboxGetType() == CarGearboxType.MANUAL)
    ShiftTo(CarGear.NEUTRAL);
else
    ShiftTo(CarAutomaticGearboxMode.D);   // + закомменченный вариант ShiftTo(CarAutomaticGearboxMode.N)
```

Expansion AI (`DayZExpansion_AI/.../CarScript.c:88-111`) ездит на **MANUAL** значениях `CarGear`:
`gear = CarGear.FIRST` (или `REVERSE` при развороте), затем `ShiftTo(gear)`. Т.е. для МКПП
`ShiftTo(CarGear.FIRST/REVERSE/NEUTRAL)` — рабочий путь, подтверждён рабочим модом.

**Как скрипт различает АКПП/МКПП** (ваниль, `carscript.c:2085-2090`, `UpdateLightsServer`):

```c
gear = GetGear();
if (GearboxGetType() == CarGearboxType.AUTOMATIC)
    gear = GearboxGetMode();
```

Аналогично HUD (`5_mission/gui/vehicles/carhud.c:203-236`): MANUAL → `GetGear()`, AUTOMATIC →
`GearboxGetMode()`. Т.е. на АКПП индикатор/логика смотрят **`GearboxGetMode()`**, а `GetGear()`
к актуальной передаче на АКПП не применяется (там «режим» P/R/N/D, сам выбор передачи внутри
режима делает нативная симуляция АКПП).

**Sedan_02:** тип коробки задаётся **конфигом транспорта, а не скриптом** (`type="GEARBOX_MANUAL"`
в `config.cpp` транспортного pbo). В ванильном DayZ-Script-Diff конфиг отсутствует (только скрипты),
в Expansion конфиге Sedan_02 стоит `type="GEARBOX_MANUAL"` (`Vehicles/Ground/Sedan_02/config.cpp:287`).
Но Expansion `Sedan_02` — свой (ВАЗ-2101), не ванильный `Sedan_02`. Поэтому тип ванильного
Sedan_02 — `[нужно проверить]` (см. гипотезы).

### 2. RPM — можно ли задать программно

- Только геттеры, сеттера RPM **нет**: `EngineGetRPMMin/Idle/Max/Redline/RPM` — `proto native`
  (`3_game/vehicles/car.c:222-234`). `EngineGetRPM()` = «engine's rpm value» (нативное значение
  симуляции).
- `OnSound(CarSoundCtrl ctrl, float oldValue)` (`car.c:415-427`) — вызывается «every sound
  simulation step», возврат = «new value of the specified sound controller», в доке:
  «The higher the return value is the more muted sound is» (см. также `carscript.c:1519-1523`).
  `CarSoundCtrl` (`car.c:1-13`): `ENGINE, RPM, SPEED, DOORS, PLAYER`. **`OnSound` управляет только
  звуковыми контроллерами (громкость/питч), а не физической симуляцией.** Override
  `carscript.c:1524-1537` показывает паттерн: на `CarSoundCtrl.ENGINE` вернуть `0.0`, если не
  заведён (заглушить звук), иначе `oldValue`. Подменить **звук оборотов** можно, подменить
  **симуляционное RPM** — нет.
- Тахометр читает нативное RPM напрямую, без скрипт-переменной (`carhud.c:104`):
  `rpm_value = m_CurrentVehicle.EngineGetRPM() / m_CurrentVehicle.EngineGetRPMMax();`
  и `m_VehicleRPMPointer.SetRotation(0, 0, rpm_value * 270 - 130, true)` (`:108`).
- **Синхронизация RPM/передачи/скорости на клиент** — НЕ через `RegisterNetSyncVariable`.
  В конструкторе `CarScript` регистрируются только `m_HeadlightsOn`, `m_BrakesArePressed`,
  `m_ForceUpdateLights`, crash-sounds, `m_CarHornState`, `m_CarEngineSoundState`
  (`carscript.c:339-345`). RPM/скорость/передача/руль реплицируются **нативной симуляцией** через
  `NetworkMoveStrategy.PHYSICS` (`3_game/entities/pawn.c:138-148`: «Sends over a fixed buffer of
  moves and re-simulates the physics steps on correction as a static scene»). Клиент ведёт свою
  копию симуляции машины и читает `EngineGetRPM()/GetGear()/GearboxGetMode()` локально
  (HUD это и делает). Факт, что машины используют PHYSICS, виден в `actionstartengine.c:51`
  и `carscript.c:3077-3086` (`IsServerOrOwner`).

**Вывод по «поднять обороты»:** единственный способ поднять настоящее `EngineGetRPM()` на
заведённом моторе — `SetThrottle(value)` (`car.c:198`, «Sets the future throttle value», `<0,1>`),
плюс отпустить тормоз/ручник и (на МКПП) держать сцепление/нейтраль. `OnSound(RPM)` override
влияет только на звук, а не на тахометр/симуляцию. На «заведённом, но толкаемом импульсом» моторе
RPM поднимает именно симуляция (нагрузка колёс/газ), отдельного «задать RPM» нет.

### 3. Руль / поворот колёс

- `proto native float GetSteering();` (`car.c:189`), `proto native void SetSteering(float value, bool unused0 = false);` (`car.c:192`) — диапазон `<-1,1>` (док у deprecated `CarController.SetSteering`, `car.c:466-474`).
- **`unused0` = флаг `analog`** (из deprecated `CarController.SetSteering(float in, bool analog = false)`: «analog indicates if the input value was taken from analog controller»). Для AI-вождения передавать дефолт (`false`).
- **Кто вызывает `SetSteering` у ванильного игрока:** никто в скрипте (grep — только объявления).
  Руль, как и газ/передачи, гонится нативно через `HumanCommandVehicle` (ввод игрока). Скриптовый
  API `SetSteering` существует именно для AI/автопилота.
- **Эталон AI:** Expansion `CarScript.c:97-107`: `steering = Math3D.AngleFromPosition(position, GetDirection(), wayPoint) / Math.PI;` → `SetSteering(steering);` (вместе с `SetThrottle`/`SetBrake`/`ShiftTo`).
- **Репликация поворота колёс:** руль — часть нативной симуляции, реплицируется PHYSICS move
  стратегией вместе с трансформом машины (см. п.2). `SetSteering(value)` на сервере/авторитете
  обновляет симуляцию → клиенты видят поворот передних колёс без доп. вызовов.
  `[нужно подтвердить]` эмпирически (см. гипотезы), т.к. в скрипте нет явного «синка руля».

### 4. `dBodyApplyImpulseAt`

Объявлена в `1_core/proto/enphysics.c:164`:

```c
/**
\brief Applies impuls on a pos position in world coordinates
*/
proto void dBodyApplyImpulseAt(notnull IEntity body, vector impulse, vector pos);
```

- Применима к любому `IEntity`-физике, включая `CarScript`/`Transport` (Car — физика).
  Ванильное доказательство — толчок машины, `4_world/.../actionpushcar.c:52`:
  `dBodyApplyImpulseAt(car, impulse, car.ModelToWorld(car.GetEnginePos()));` (точка = позиция
  двигателя в world-space). Это **серверный** continuous-action (`ApplyForce` выполняется на
  сервере; `OnStartServer`/`OnEndServer` управляют тормозами — `actionpushcar.c:85-112`).
- **Единицы импульса:** в `actionpushcar.c:38-48` импульс предварительно делится на массу
  (`impulse = direction * force * invBodyMass`), т.е. ваниль трактует его как **дельту скорости**
  (m/s), а не сырой импульс. Для «толчка» — считать `impulse = dir * desiredDeltaV` (по аналогии),
  а не сырой импульс силы.
- Перед толчком нужно отключить авто-торможение и тормоза:
  `SetBrake(0)`, `SetHandbrake(0)`, `SetBrakesActivateWithoutDriver(false)` (`actionpushcar.c:93-95`),
  после — `SetBrakesActivateWithoutDriver(true)` (`:111`).
- Если тело было «уснуло»/выгружено физикой — активировать: `dBodyActive(this, ActiveState.ACTIVE)`
  + `DisableSimulation(false)` (Expansion towing, `CarScript_Towing.c:52-57`). `[нужно проверить]`
  для сценария «машина стоит» — обязателен ли этот пробужающий вызов перед импульсом.
- `dBodyApplyImpulseAt` — `proto` (не `native`), но вызывается ванилью на сервере; работает на
  сервере (расширение: `zombiefightlogic.c:365` `dBodyApplyImpulseAt(cs, impulse, hitPosWS)`).

### 5. Двигатель и водитель

- `EngineStart()/EngineStop()/EngineIsOn()` — `proto native` (`car.c:237-243`).
- **Скриптовый гейт старта не требует водителя:** `OnBeforeEngineStart` (`carscript.c:1811-1816`)
  возвращает `CheckOperationalRequirements() == OK`; `CheckOperationalRequirements`
  (`carscript.c:1849-1888`) проверяет только `RUINED/NO_FUEL/NO_BATTERY/NO_IGNITER` — водителя нет.
- **Требование водителя — в action-гейте игрока**, не в нативе: `ActionStartEngine.ActionCondition`
  (`actionstartengine.c:26-37`) требует `vehicle.CrewDriver() == player`. Плюс ванильный action
  **пропускает** `EngineStart()` для серверного экземпляра (`actionstartengine.c:51-63`): при
  PHYSICS-стратегии, если `GetInstanceType() == INSTANCETYPE_SERVER` (серверный бот) → `return`
  без `EngineStart()`. Для серверного ИИ **надо вызывать `EngineStart()` напрямую**, а не через
  action (что и делает Expansion: `CarScript.c:71-75`).
- **Правила «двигатель глохнет без водителя» — скриптовые, пост-факт:** `OnDriverExit`
  (`carscript.c:1153-1161`) — если `GetGear() != GetNeutralGear()` → `EngineStop()`;
  `MarkCrewMemberUnconscious/Dead` (`carscript.c:1756-1776`) — `EngineStop()` при смерти/отключке
  водителя. Т.е. движок **умеет** работать без водителя в кресле, но ваниль сама глушит его при
  выходе/смерти водителя (если не нейтраль).
- **Машина без водителя авто-тормозит:** `SetBrakesActivateWithoutDriver(bool activate = true)`
  (`car.c:219`) — «Sets if brakes should activate without a driver present». По умолчанию `true`.
  Для езды/толчка без водителя — `SetBrakesActivateWithoutDriver(false)`.
- `carscript.c:1249-1259` (`OnUpdate`): если водитель есть, но `!driver.IsControllingVehicle()`
  (без сознания) — `SetBrake(0.5)`.
- **Требует ли нативная симуляция газа/передач водителя:** Expansion AI гейтит всё вождение на
  наличие AI-водителя в кресле 0 (`CarScript.c:62-66`: `if (!Class.CastTo(driver, CrewMember(VEHICLESEAT_DRIVER))) return;`).
  Значит практически «газ/передачи» применяют при водителе. Без водителя достоверно работает
  только **толчок импульсом** (ванильный `ActionPushCar`). `[нужно проверить]`, заведётся ли
  `EngineStart()` + `SetThrottle()` вообще без `CrewMember(0)`.

### 6. Скорость машины

- `proto native float GetSpeedometer();` (`car.c:109`) — «current speed in km/h», **знаковая**
  (вперёд +, назад −). `GetSpeedometerAbsolute()` (`car.c:112-115`) = `Math.AbsFloat(GetSpeedometer())`.
- `proto native vector GetVelocity(notnull IEntity ent)` (`1_core/proto/enphysics.c:132`) —
  физический вектор скорости (m/s). Пересчёт: `GetVelocity(this).Length() * 3.6` = км/ч
  (используется в crash-дебаге `carscript.c:1378`).
- Ваниль для «едет ли машина» использует **`GetSpeedometerAbsolute()`**: `IsMoving()` =
  `GetSpeedometerAbsolute() > 3.5` (`carscript.c:2631-2634`); пороги дверей — `car.c:127-129`.
- Для **толчка импульсом** обе дают одно и то же по модулю (машина едет вдоль направления);
  `GetSpeedometerAbsolute()` — уже км/ч и не требует `*3.6`, удобнее для порогов. `GetVelocity`
  нужен, если важна полная 3D-скорость (напр. после столкновения/полёта) или направление.
  Знаковая `GetSpeedometer()` — для «едет назад» (реверс).

## Выводы для разработки

- **Газ:** `Car.SetThrottle(0..1)` + `SetBrake(0..1)` + `SetHandbrake(0..1)` в override
  `Transport.OnInput(float dt)` на `CarScript` (паттерн Expansion AI `CarScript.c:58-114`).
  Подтверждено (Expansion рабочий мод + сигнатуры `car.c`).
- **Передачи:** `GearboxGetType()` → если MANUAL: `ShiftTo(CarGear.FIRST/REVERSE/NEUTRAL)`; если
  AUTOMATIC: `ShiftTo(CarAutomaticGearboxMode.D/R/N)`. `GetGear()` (manual) / `GearboxGetMode()`
  (auto) — для чтения. Подтверждено сигнатурами + `CarScript_Towing.c:65-73`.
- **RPM:** задать нельзя; только читать `EngineGetRPM()` и поднимать косвенно `SetThrottle()`.
  `OnSound(RPM)` — только звук. Подтверждено (`car.c:222-234`, `carhud.c:104`).
- **Руль:** `SetSteering(value)` (`<-1,1>`, второй параметр `unused0`/`analog` = `false`) в том же
  `OnInput`. Репликация колёс — через PHYSICS move-стратегию (гипотеза, `[нужно подтвердить]`).
- **Толчок:** `SetBrake(0)` + `SetHandbrake(0)` + `SetBrakesActivateWithoutDriver(false)` →
  `dBodyApplyImpulseAt(car, dir*deltaV, car.ModelToWorld(car.GetEnginePos()))` (единицы = дельта
  скорости, по образцу `actionpushcar.c:38-52`). Подтверждено.
- **Запуск:** `EngineStart()` напрямую (не через ванильный `ActionStartEngine`, который режет
  серверных ботов — `actionstartengine.c:54-57`). Подтверждено.

### Гипотезы для эмпирической проверки (`[нужно проверить]` / `[нужно подтвердить]`)

1. `[нужно проверить]` **Тип коробки Sedan_02** — `GearboxGetType()` на заспавненном `Sedan_02`
   возвращает `MANUAL` или `AUTOMATIC`? (конфиг `type="GEARBOX_MANUAL"` ванили не в script-diff;
   Expansion-аналог — MANUAL). Probe: `botdump`/лог `GearboxGetType()`.
2. `[нужно проверить]` **`ShiftTo` на АКПП** — реально ли `ShiftTo(CarAutomaticGearboxMode.D)` меняет
   `GearboxGetMode()` и заставляет машину ехать (не игнорируется ли нативом, как «будущая передача»
   для автомата)? Probe: на АКПП-машине `ShiftTo(D)` + `SetThrottle` → `botdump` `GearboxGetMode()`/`GetSpeedometerAbsolute()`.
3. `[нужно подтвердить]` **Репликация руля на клиент** — достаточно ли серверного `SetSteering(v)`,
   чтобы клиент визуально видел поворот передних колёс (без доп. вызова)? Probe: сервер крутит
   руль → наблюдатель видит колёса.
4. `[нужно проверить]` **Двигатель без водителя** — `EngineStart()` + `SetThrottle(1)` на машине с
   `CrewMember(0)==null` (после `SetBrakesActivateWithoutDriver(false)`): поднимается ли
   `EngineGetRPM()`, едет ли машина? Или нативная симуляция газа требует водителя в кресле?
5. `[нужно проверить]` **Пробуждение физики перед импульсом** — нужен ли `dBodyActive(this, ACTIVE)`
   + `DisableSimulation(false)` перед `dBodyApplyImpulseAt` на стоящей/уснувшей машине, или импульс
   будит тело сам? Probe: толчок стоящей машины → `botdump` скорости.
6. `[нужно подтвердить]` **`OnInput` на сервере для серверного ИИ** — вызывается ли `Transport.OnInput`
   на сервере для машины с серверным водителем-ботом (не только на клиенте-овнере)? От этого
   зависит, где крутить `SetThrottle`/`SetSteering`.

## Вождение: дорожный pathfinding

Цель: понять, как построить маршрут ПО ДОРОГАМ (для автомобиля), а не по пешеходному navmesh,
и выбрать минимальный жизнеспособный подход для botorama.

Источники: ваниль `/home/devalio/dayz/Work/DayZ-Script-Diff/scripts`, эталон —
`DayZ-Expansion-Scripts/.../Classes/Roads/` (`eAIRoadNetwork` и др.) + AI-вождение
`DayZExpansion_AI/.../Entities/CarScript.c`.

### 1. Как DayZ детектит «дорогу» — три кандидата

**а) navmesh `PGAreaType.ROADWAY`** — это НЕ детектор, а только cost-подсказка navmesh-поиска.
- `enum PGAreaType` (`3_game/ai/aiworld.c:28`), `ROADWAY` = строка 42, `ROADWAY_BUILDING` = 44.
- `PGFilter.SetCost(PGAreaType areaType, float cost)` — `proto native` (`aiworld.c:67`).
- **В ванили `SetCost(ROADWAY, ...)` нигде не вызывается** (grep по ванильным скриптам даёт
  только объявление enum + объявление `SetCost`). Ваниль вообще не использует ROADWAY для
  поиска. ROADWAY появляется только в `PGAreaType`/`PhxInteractionLayers` (физика, `dayzphysics.c:11`)
  и как area-тип навмеша.

**б) дорожные ОБЪЕКТЫ** — `eAIRoadNode.ObjectIsRoad(obj, geometry)` (`eAIRoadNode.c:322-337`):
```c
static bool ObjectIsRoad(Object obj, LOD geometry)
{
	for ( int i = 0; i < geometry.GetPropertyCount(); ++i )
	{
		string name = geometry.GetPropertyName(i);
		string value = geometry.GetPropertyValue(i);
		name.ToLower();
		value.ToLower();
		if (name == "class")
		{
			return value == "road";
		}
	}
	return false;
}
```
Механизм: у дорожного объекта в LOD **"geometry"** есть named-property `class="road"`.
`GetLODByName("geometry")` — `3_game/entities/object.c:106` (возвращает LOD по имени);
`LOD.GetPropertyCount/GetPropertyName/GetPropertyValue` — `3_game/gameplay.c:241-243` (named
properties p3d). Это **рабочий на-лету детектор**: перебрать объекты рядом
(`Game.GetObjectsAtPosition`, `game.c:912`) → `GetLODByName("geometry")` → `ObjectIsRoad`.
Не требует prefetch-графа.

**в) `Game.SurfaceRoadY(x, z, rsd)`** — натив (`3_game/global/game.c:1153-1154`):
```c
proto native float SurfaceRoadY(float x, float z, RoadSurfaceDetection rsd = RoadSurfaceDetection.LEGACY);
proto native float SurfaceRoadY3D(float x, float y, float z, RoadSurfaceDetection rsd);
```
Возвращает Y «roadway»-поверхности (driving surface = terrain + дорожные объекты с roadway-LOD:
дороги/мосты/тротуары). `RoadSurfaceDetection` (`3_game/constants.c:40-50`) — вертикальные
направления: `UNDER`/`ABOVE`/`CLOSEST`/`LEGACY`. Более общий вариант —
`Game.GetSurface(SurfaceDetectionParameters, SurfaceDetectionResult)` (`game.c:1150`) с
`SurfaceDetectionType.Roadway` (`3_game/surfaceinfo.c:65-69`): возвращает не только высоту, но и
`SurfaceInfo` (тип поверхности) + объект + нормаль (`surfaceinfo.c:99-122`). Именно его ваниль
использует, чтобы узнать нормаль «дороги/камня/моста» под машиной (`transport.c:308-327`,
комментарий «trace roadway, incase the vehicle is on a rock, or bridge»).

Вывод по Q1: «дорога» опознаётся (б) по объекту (`class="road"` в geometry-LOD) и (в) по
поверхности (`GetSurface(Roadway)` → тип). (а) ROADWAY — не детектор, только вес.

### 2. Expansion `eAIRoadNetwork` — устройство графа

Файлы (`Classes/Roads/`): `eAIRoadNetwork.c` (819 строк), `eAIRoadNode.c` (338),
`eAIRoadSection.c` (147), `eAIRoadNodeSection.c` (23), `eAIRoadNodeBase.c` (4),
`eAIRoadNodeJoinMap.c` (17, включает `eAIRoadConnection`). Зависимости: `ExpansionPathNode`
(`Classes/PathFinding/PathNode.c`, 48) и `ExpansionPathHandler`.

**а) Как строятся узлы/секции.**
- `_Generate` (`eAIRoadNetwork.c:328-661`): `g_Game.GetObjectsAtPosition(position, radius,
  objects, proxyCargos)` (`:356`) → отсев камер/партиклов/кричеров/манов/транспорта/предметов
  (`:358-367`) → `obj.GetLODByName("geometry")` + `ObjectIsRoad` (`:368-369`) → `eAIRoadNode.Generate`.
- `eAIRoadNode.Generate` (`eAIRoadNode.c:100-211`): узлы из **memory-points** дорожного объекта.
  Пары точек = концы куска дороги: `ROAD_MEMORY_POINT_PAIRS = {"LB","PB","LE","PE","LD","LH","PD","PH"}`
  (`eAIRoadNode.c:3-9`). Для каждой пары — `MemoryPointExists` + `GetMemoryPointPos` →
  `ModelToWorld` → середина = connection (`:111-132`). Если memory-points нет — fallback на
  `obj.ClippingInfo(min_max)` (bounding box) и концы по min/max Z (`:134-171`).
- Соединение узлов (`eAIRoadNetwork.c:390-589`): (1) «Memory Points» — связать близкие
  connections соседних кусков (отсекая заблокированные через `aiWorld.RaycastNavMesh`,
  `:413`); (2) «Far» — доистязать изолированные узлы по радиусу; (3) чистка треугольников,
  «Fixing missing links», `Optimize()` (схлопывание почти коллинеарных узлов `eAIRoadNode.c:213-279`).
- Секции (`GenerateSections` `eAIRoadNetwork.c:192-278`): цепочка узлов с ровно 2 соседями =
  «отрезок дороги»; узлы-перекрёстки (≠2 соседей) = `eAIRoadNodeSection` (концы секций).

**б) Как ищется путь.** **НИКАК.** `FindPath` и `_FindPath` — пустые заглушки
(`eAIRoadNetwork.c:805-818`, весь код закомментирован). `eAIRoadNode.PathTo`
(`eAIRoadNode.c:55-71`) — только обход простой 2-соседней цепочки, не общий графовый поиск.
Dijkstra/A* **не реализованы**.

**в) Портировать целиком?** **Нет — и вот почему это критично:** вся сеть в Expansion — мёртвый/
экспериментальный код. `m_Network.Init()` закомментирован (`ExpansionWorld.c:30`), триггер
генерации `m_Network.NotifyGenerate(...)` — внутри `/* */` блока (`ExpansionWorld.c:435-457`),
`GetRoadNetwork()` (`:466-469`) нигде не вызывается. **Реальное ИИ-вождение Expansion идёт по
пешеходному navmesh**: `CarScript.OnInput` (`DayZExpansion_AI/.../Entities/CarScript.c:81`)
берёт следующий вейпоинт из `driver.m_PathFinding.GetNext(wayPoint)` — это `ExpansionPathHandler`
(navmesh `FindPath`), а не road graph. Т.е. эталонного рабочего road-graph'а у Expansion НЕТ —
есть только набросок построения графа без поиска пути.

### 3. `ObjectIsRoad` — точный механизм

См. Q1(б). Определение дороги = named-property `class="road"` в LOD `"geometry"` объекта.
`LOD` названия стандартные (`3_game/gameplay.c:203-210`): `NAME_GEOMETRY="geometry"`,
`NAME_ROADWAY="roadway"`. Т.е. есть два разных LOD'а: `"geometry"` (для детекта по свойству
`class="road"`) и `"roadway"` (driving surface, по которому идёт `GetSurface(Roadway)`).

**Можно ли детектить дорогу на лету, без prefetch-графа?** Да:
`GetObjectsAtPosition(pos, r, objects, null)` → `obj.GetLODByName("geometry")` →
`ObjectIsRoad`. Ограничения: `GetObjectsAtPosition` возвращает только **загруженные/стримящиеся**
объекты в радиусе (дороги — static, стримятся как часть сцены, радиус практический `[нужно проверить]`),
и надо отсеивать не-дороги (ваниль/Expansion отсеивают man/transport/item/camera и т.п.).
Альтернатива для «найти дорожные объекты в боксе» — `DayZPlayerUtils.SceneGetEntitiesInBox(min, max,
entList, QueryFlags.ONLY_ROADWAYS)` (`4_world/entities/dayzplayerutils.c:75`, `ONLY_ROADWAYS` =
`:11`). **Готча** (см. `decisions.md`): `QueryFlags` — последовательный enum, `ONLY_ROADWAYS=4`,
поэтому его НЕЛЬЗЯ комбинировать с `STATIC|DYNAMIC` через `|` — либо только roadways, либо
static/dynamic, двумя отдельными вызовами.

### 4. Надёжность navmesh `ROADWAY` на реальных дорогах

Expansion использует `ROADWAY` только как мягкий вес, причём **равный TERRAIN**: в
`expansionpathfilters.c:146` и `:151` — `SetCost(PGAreaType.ROADWAY, 4.0)` и
`SetCost(PGAreaType.TERRAIN, 4.0)` (одинаково!), `ROADWAY_BUILDING` = 1.0 (`:153`). Т.е. Expansion
даже **не предпочитает** дорогу в пешем поиске — ROADWAY тут лишь «не штрафовать», а не
«ехать по дороге». Это косвенное, но сильное подтверждение, что полагаться на ROADWAY-навмеш как
на «дорогу» нельзя. Плюс собственный факт botorama (`decisions.md:131`): ROADWAY-навмеш
фрагментирован, дорожный маршрут усечён → fallback на пеший. Вывод: надёжный дорожный путь —
**дорожный граф по объектам/поверхности**, а не navmesh-ROADWAY.

### 5. `SurfaceRoadY` — сигнатура и семантика

Сигнатуры — Q1(в). Возвращает Y дорожной поверхности в точке (x,z). `RoadSurfaceDetection.CLOSEST`
ищет поверхность **вертикально** ближайшую к точке (над/под), НЕ по горизонтали. Парного натива
«ближайшая точка на дороге (горизонталь)» **нет**. Expansion-хелпер `GetSurfaceRoadPosition`
(`ExpansionStatic.c:2706-2709`) делает просто `Vector(x, roadY, z)` — сохраняет x,z и снэпает
только Y, горизонтально на дорогу НЕ тянет.

Для «притянуть точку на дорогу» по горизонтали натива нет; варианты:
- перебрать ближайшие road-объекты (`GetObjectsAtPosition`/`SceneGetEntitiesInBox(ONLY_ROADWAYS)`)
  и проецировать на их centerline/memory-points;
- сетка сэмплов вокруг точки + `GetSurface(Roadway)` → выбрать сэмпл, чей `SurfaceInfo` — дорога.

Открытый вопрос семантики: что возвращает `SurfaceRoadY` ВНЕ дороги (нет roadway-объекта под
точкой)? Гипотеза — fallback на terrain (по аналогии с трапами/`undergroundstash.c:8`,
которым нужно ложиться и на траву), но это `[нужно проверить]` (см. гипотезы).

### 6. Практическая рекомендация для botorama (ранжированно)

Ключевой факт: эталонного рабочего road-pathfinder нигде нет (Expansion-граф — заглушка без
поиска; navmesh-ROADWAY фрагментирован). Значит «списать» не с чего — выбираем по усилию/эффекту.

1. **(г) Явная полилиния (точки излома дороги от пользователя) + снэп** — **рекомендуемый MVP.**
   Уже есть waypoint-вождение; подаём вершины полилинии как вейпоинты, каждую снэпаем на дорогу
   через `GetSurface(Roadway)`/`SurfaceRoadY` (Y-снэп) и доводкой по road-объектам при необходимости.
   Усилие минимально, детерминированно, тестируемо на ВПП. Минус: нужны авторские данные, нет
   динамического перестроения маршрута.
2. **(в) «снэп пешего пути на дорогу»** — дёшево, но слабо: `SurfaceRoadY` двигает только Y, путь
   не ложится на дорогу горизонтально и по-прежнему может срезать по полю. Годится только как
   компонент Y-снэпа внутри (г), не как самостоятельный pathfinder.
3. **(б) navmesh-ROADWAY с весами** — уже опробовано, фрагментировано; оставить только как мягкое
   предпочтение (cost ниже TERRAIN) поверх (г), но не как гарантию.
4. **(а) Дорожный граф по road-объектам (как Expansion)** — максимальная точность, но (i) Expansion
   не доделал даже поиск пути, (ii) объём ~1300 строк только построения графа + нужно самому писать
   Dijkstra/A*, (iii) зависит от memory-points конкретных road-моделей (см. гипотезу 3). Брать
   ТОЛЬКО если (г) окажется недостаточно (нужен произвольный A→B без авторских данных).

Итог: начать с **(г) + (в)** (полилиния + Y-снэп через `GetSurface(Roadway)`), а `ObjectIsRoad`
(`class="road"` в geometry-LOD) держать как готовый на-лету детектор для будущего авто-вывода
точек дороги (или для `SceneGetEntitiesInBox(ONLY_ROADWAYS)`).

### Гипотезы для эмпирической проверки

1. `[нужно проверить]` **`SurfaceRoadY` вне дороги**: что возвращает `SurfaceRoadY(x,z)` в точке на
   траве/в лесу (далеко от дороги) — высоту terrain (как `SurfaceY`) или что-то иное (0 / ближайшую
   дорогу)? Probe: лог `SurfaceRoadY` vs `SurfaceY` в 3-4 точках (на дороге, на траве у дороги, в лесу).
2. `[нужно проверить]` **Детект road-объектов на РЕАЛЬНОЙ дороге**: возвращает ли
   `GetObjectsAtPosition`/`SceneGetEntitiesInBox(ONLY_ROADWAYS)` road-объекты на обычной (не ВПП)
   дороге, и какой практический радиус (объекты стримятся)? Probe: `scanbox`/`spawnobj` около
   известной дороги + лог `GetLODByName("geometry")` property `class`.
3. `[нужно проверить]` **Memory-points дорог**: есть ли у ВАНИЛЬНЫХ road-объектов memory-points
   `LB/PB/LE/PE/LD/LH/PD/PH` (или это только у Expansion-моделей)? Без них node-building по
   образцу Expansion не взлетит. Probe: на ванильном road-объекте `MemoryPointExists` для этих имён
   + `GetMemoryPointPos`.
4. `[нужно подтвердить]` **`GetSurface(Roadway)` различает дорогу/траву**: возвращает ли
   `SurfaceDetectionResult.surface.GetName()/GetSurfaceType()` (напр. «concrete»/«asphalt» vs «grass»)
   на дороге vs на траве, чтобы по точке решать «это дорога»? Probe: `GetSurface` с
   `SurfaceDetectionType.Roadway` на/вне дороги, лог `surface.GetName()`.

### Существенные развилки и принятые решения (для журнала)

- **Развилка:** «по какому источнику строить дорожный путь — navmesh-ROADWAY / road-объекты /
  SurfaceRoadY / полилиния от пользователя». **Решение:** для MVP — полилиния + Y-снэп (г+в);
  navmesh-ROADWAY признан ненадёжным (фрагментация + Expansion не использует его для следования
  по дороге), road-graph по объектам — отложить (Expansion-реализация не закончена). Влияет на
  архитектуру drive-интента (подавать явные вейпоинты дороги, а не полагаться на FindPathTo).
- **Развилка:** «портировать ли `eAIRoadNetwork` как готовый road-pathfinder». **Решение:** НЕ
  портировать — в самом Expansion это мёртвый код (Init закомментирован, FindPath пустой, реальный
  водитель едет по navmesh). Экономия ~1300+ строк и несуществующего поиска пути.

## Вождение: датчик поверхности (эмпирика фазы 0)

Эмпирика на реальных точках (probe-оп `surfprobe`, E2E `surfprobe-roads` + `surfprobe-trail`,
v3.211): 9 точек × 29 сэмплов креста, сырые значения `SurfaceGetType`/`SurfaceY`/`SurfaceRoadY`/
`GetSurface(Roadway)`.

### Вывод: датчик дороги = `SurfaceGetType(x, z, out string type)`

Один нативный вызов возвращает и имя поверхности, и Y высоты. Достаточен и чист — остальные
кандидаты отпали (ниже). Сигнатура: `proto float SurfaceGetType(float x, float z, out string type)`
(`game.c:1156`).

### Таксономия имён поверхности (наблюдено, 9 точек)

| имя | класс | примечание |
|---|---|---|
| `concrete_ext` | PAVED (асфальт) | однополосная дорога, асфальт |
| `cp_concrete1` | PAVED (бетон) | бетонное покрытие/площадка |
| `dirt_ext` | DIRT | **и грунтовая дорога, и ТРОПИНКА** (см. ниже) |
| `cp_dirt` | DIRT | грунтовая дорога, обочина, край поля |
| `cp_grass`, `cp_grass_tall` | GRASS | поле/трава |
| `cp_broadleaf_dense1`, `cp_broadleaf_sparse1`, `cp_conifer_common2` | FOREST | лес (под деревьями) |
| `ceramic_tiles_roof_ext`, `wood_planks_ext` | STRUCTURE | крыша/настил (здание) |
| вода | WATER | не сэмплено (гипотеза: `cp_water`/liquid) |

### Разрешение прошлых гипотез

1. **`SurfaceRoadY` вне дороги** → **НЕ чистый сигнал**: следует и КРЫШАМ зданий (на точке
   (2804,12422) прыгает на +7…10 м над черепицей `ceramic_tiles_roof_ext`), а не только дорогам.
   `SurfaceRoadY − SurfaceY > 0` ≠ «дорога» (может быть крыша/настил).
2. **Детект road-объектов** → **`GetSurface(Roadway).object == null` на всех 232 сэмплах**,
   включая чистый асфальт; `surface` возвращает террейн (`cp_dirt*`, `cp_grass_ca*`), а не дорогу.
   Дороги в этой части карты — **покраска террейна, а не 3D-объекты** → `GetSurface(Roadway)`/
   `ObjectIsRoad` для детекта дорог НЕ годятся.
3. **Memory-points дорог** → неактуально (не работаем с road-объектами).
4. **`GetSurface(Roadway)` различает дорогу/траву** → **НЕТ**, возвращает террейн. Различает
   именно `SurfaceGetType` (на асфальте `concrete_ext`, на траве `cp_grass`).

### Ключевой факт: тропинка = `dirt_ext`, от грунтовки поверхностно неотличима

- Тропинка (1149.43, 12172.34): центр `dirt_ext`, уже на ±2 м → `cp_grass`/`cp_conifer_common2`.
  Ширина полосы `dirt_ext` ≈ 2–3 м.
- Грунтовка (2471,12942 / 2415,13019): тоже `dirt_ext`, но полоса шире (≈5–11 м) и с обочиной
  `cp_dirt`.
- Следствие: **по типу поверхности тропинку от грунтовки НЕ отличить.** Дискриминатор — **ширина
  + непрерывность + центр**, которые меряет ЗОНД (геометрия), а не датчик. Это же решает
  «бетонная дорога vs бетонная площадка/тротуар рядом» (пользовательское требование).

### Вывод для разработки

- Датчик: `SurfaceGetType` → материал (`PAVED/DIRT/GRASS/FOREST/STRUCTURE/WATER`) — таблица имён,
  один вызов на точку (дёшево, годится в горячий цикл зонда).
- `GetSurface(Roadway)` и `SurfaceRoadY` в горячем цикле НЕ использовать (шум/крыши/null-объект).
- «Дорога vs тропинка vs площадка» — решает геометрия зонда (ширина по краям + непрерывность
  следования центральной линии), не материал.
