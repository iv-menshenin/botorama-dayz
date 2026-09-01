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

## Готчи / замечания

- `StartCommand_Vehicle` требует близость к двери (ванильный гейт `CanReachSeatFromDoors <= 1.0`),
  поэтому `DM_GETIN_REACH = 1.0` и маршрут к `CrewEntryWS` (не к позиции транспорта).
- `FindPathTo` снапает цель на navmesh — точка входа у двери обычно на navmesh, но у края дороги
  может снапаться на ~1м — поэтому достижимость 1.0, а не 0.05.
- Боты регистрируются в `dmEntityRegistry` как `PlayerBase` — при поиске «игрок в машине»
  обязательно исключать `dmAISurvivorBase` (иначе найдём машину, где сидит другой бот).
