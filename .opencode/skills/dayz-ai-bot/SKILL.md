---
name: dayz-ai-bot
description: Живой справочник по серверным ИИ-ботам для DayZ (мод botorama, префикс dm). Использовать при работе с dmAISurvivor/dmAISurvivorBase, FSM/интентами/состояниями ботов, спавном/синхронизацией, движением/поворотом головы и тела, pathfinding, инвентарём/loadout, боем/лутом. Содержит архитектуру, механики и DayZ-готчи; синтаксис Enfusion — в docs/codeguide.md.
---

# DayZ AI Bot (botorama) — живой справочник

Справочник по механике серверных ИИ-ботов DayZ (мод `botorama`, префикс `dm`).
Синтаксис/движок Enfusion — в `docs/codeguide.md`. Research-заметки по API — в
`docs/research/*.md`. Планы — в `docs/plans/`.

## Архитектура

- Проект: botorama (префикс `dm`), client-server мод DayZ.
- Паттерн: контроллер `dmAISurvivor` (обычный `class`, не Managed → поля через `ref`)
  + пешка `dmAISurvivorBase : PlayerBase`.
- Слои: **FSM** (что делать) → **интенты** (желания, конкурентные, арбитраж) →
  **примитивы мозга** (`dmAISurvivor.*`) → **применение в пешке** (`CommandHandler`).
  Мозг пишет «чего хочу», пешка — как это применить (`OverrideMovement*`/`ForceStance`/
  `SetOrientation`/граф-переменные).
- Глобальный список ботов — статические поля `dmAISurvivor.s_All` / `s_ByPawn`
  (`array<ref dmAISurvivor>`, `map<PlayerBase, ref dmAISurvivor>`).
- Пешка создаётся `GetGame().CreateObject(model, pos)` (CE-спавн, как у Expansion) →
  `INSTANCETYPE_AI_SERVER` + `EconomyProfile` (ванильное протухание трупа).
  `CreatePlayer(null, …)` профиля НЕ даёт — труп не протухает.
- Модель наследуется из конфига (`dmAI_SurvivorM_Denis : SurvivorM_Denis`).

## Синхронизация сервер→клиент

- `RegisterNetSyncVariableFloat("имя", min, max, precision)` в конструкторе.
- Сервер меняет поле + `SetSynchDirty()`.
- Клиент читает в `override void OnVariablesSynchronized()` под `#ifndef SERVER`.
- `SetSynchDirty()` БЕЗ реального изменения значения — не сработает (надо менять поле).

## Серверные хуки (что где бежит)

- **Только сервер**: `MissionServer` (`OnInit`/`OnEvent` чат-команды/`OnUpdate` →
  `dmAISurvivor.TickAll`), мозг `dmAISurvivor`, FSM + интенты, pathfinding
  (`AIWorld.FindPath` — navmesh серверная штука), создание пешки.
- **Только клиент**: `MissionGameplay.OnInit` (лог версии), `OnVariablesSynchronized`.
- **Пешка** — client-server сущность: для AI_REMOTE ВСЯ анимационная логика бежит
  ТОЛЬКО на сервере (`CommandHandler`, `HeadingModel`, `AimingModel`, `HumanCommandScript`).
  Клиент лишь проигрывает синхронизированную анимацию. `CommandHandler` НЕ обёрнут в
  `#ifdef SERVER` — на клиенте для AI_REMOTE просто не вызывается.
- `AnimSet*` из `OnVariablesSynchronized` НЕ работают («usable only from CommandHandler»).

## Поворот головы (работает)

- Голова крутится через граф-переменные. Ванильные `Look`/`LookDirX`/`LookDirY`
  перетираются нативным "look at" (для ИИ ставит в 0) — их НЕЛЬЗЯ использовать.
- Решение: кастомные `dmAI_Look`/`dmAI_LookDirX`/`dmAI_LookDirY` в СВОЁМ графе.
- `AnimSetFloat` (переменные) синхронизируется на клиент; `AnimCallCommand` (команда)
  срабатывает на сервере и триггерит ПЕРЕХОД состояния, на клиент уезжает итог.
- Pitch из `VectorToAngles()[1]` в [0,360) — нормализовать: `if (pitch > 180) pitch -= 360;`.
- В мозге: `LookAtPoint` (абс. яу по миру), `LookAtDirection` (относительно корпуса),
  `LookForward`, `LookAtYaw`; сглаживание в `UpdateLook` (`DM_LOOK_TURN_SPEED`).

## Кастомный animation graph

- `.agr` — ТЕКСТОВЫЙ формат (бинарные только `.abt`/`.asi`). Редактор не нужен.
- Скопированы `player_main.agr` + `locomotion/actions/tests.agr` в `botorama/Animations/`.
- **НЕ ПЕРЕИМЕНОВЫВАТЬ ванильные переменные/команды** (нативный код обращается по
  ХЕШУ имени → краш "doesn't have variable NNN"). Вместо этого: КЕПИТЬ ванильные +
  ДОБАВИТЬ кастомные + переименовать только ИСПОЛЬЗОВАНИЯ в подграфах.
- Подключение: `config.cpp` → `class enfAnimSys : enfAnimSys { graphName = "botorama\Animations\player_main.agr"; }`
  + `requiredAddons = {"DZ_Characters","DZ_Anims_Anm_Player","DZ_Anims_Cfg"}`.

## Поворот тела (переступание)

- `SetOrientation()` — мгновенный поворот, «скользит» (это `HeadingModel::RotateOrient`).
- Правильное переступание = состояние "Turn" в графе по КАСТОМНОЙ команде
  (`IsCommand(dmAI_Turn)`/`IsCommand(dmAI_StopTurn)`) + переменная `dmAI_TurnAmount`.
- `TurnAmount` играет ДВЕ роли: `TurnVar`-нода ПИШЕТ в ванильный `TurnAmount`
  (нативный код читает и крутит тело), бленд-ноды `*TurnNBlend` ЧИТАЮТ `dmAI_TurnAmount`.
- `ApplyBodyTurn` (после `super.CommandHandler()`): при идле и `|dBody| > 1°` —
  `AnimSetFloat(dmAI_TurnAmount, dBody/90)` + `AnimCallCommand(dmAI_Turn)`; при
  завершении (`|dBody| < 1°` или 2с) — `AnimCallCommand(dmAI_StopTurn)`. При движении —
  слайд-поворот `SetOrientation` (скорость `DM_MOVE_TURN_RATE`). Эталон — Expansion `eAICommandMove`.

## Движение

- `HumanInputController.OverrideMovementAngle(ONE_FRAME, angle)` — угол направления
  движения **относительно корпуса** (-180..180: 0 вперёд, ±90 страйф, ±180 назад);
  `OverrideMovementSpeed(ONE_FRAME, 0..3)`. `ONE_FRAME` сам сбрасывается.
- Мозг: `SetMove(angle, speed)` (движение отн. корпуса) + `SetMoveYaw` (направление
  корпуса) + `CalcSpeed` (скорость; дедлайн ускоряет). Пешка: `ApplyMovement` рампует
  скорость (`DM_MOVE_ACCEL_RATE`), кэп спринта (`CanConsumeStamina`/`CanSprint`),
  притормаживание на резком довороте (`DM_MOVE_TURN_SLOW_*`).
- **Готча «бот не бежит»**: скорость движения берётся из `CalcSpeed(toPoint, deadline)`,
  и при `deadline == 0` (дефолт `m_ReachDeadline` у `MoveTo`) он ВСЕГДА возвращает
  preferred speed (jog) — спринта не будет, без единой ошибки. Если бот должен догонять/
  спешить — выставляй `m_ReachDeadline` (напр. `dist / DM_FOLLOW_CATCHUP_SPEED`), иначе
  молча ползёт jog-ом. См. «Памятки».
- `override HeadingModel` для `COMMANDID_MOVE`: ставим `m_fHeadingAngle = m_fOrientationAngle`
  и `return true` — иначе ваниль сама крутит корпус и перетирает наш `SetOrientation`.
- «Движется ли бот» — **свой флаг** (`m_IsMoving` из `SetMove`), НЕ
  `GetCurrentMovementSpeed()` (он ненулевой во время переступания → отмена поворота).

## Стойка (осанка)

- `ForceStance(next)` зовём только при смене `m_DesiredStance`; erect↔prone — только
  через crouch (иначе ломается привязка/хитбокс). Таймауты `DM_STANCE_TIMEOUT_CROUCH/PRONE`.
- Текущая стойка: `GetMovementState(state)` → `state.m_iStanceIdx` (нормализовать
  вычитанием `STANCEIDX_RAISED`).
- Канал стойки в арбитраже: пер-тик сброс в `ERECT` — убрали stance-интент → бот встаёт сам.

## FSM

- `dmBotFSM` — гибрид переходов: кооперативный `EXIT` (состояние доделало → взвешенный
  случайный переход) + вытеснение (`INTERRUPTIBLE` прерывается `PREEMPTIVE` раз в
  `DM_FSM_PREEMPT_INTERVAL`). Fallback в default-состояние при пустом выборе.
- `dmBotState`: `EXIT`/`CONTINUE`, `OnEntry/OnExit/OnUpdate`, `CanEnter()` (блокирует
  вход), `GetKind()` (NORMAL/INTERRUPTIBLE/PREEMPTIVE), `AddTransition(to, weight)`
  с fluent `BlockWhen/Require(условие)`.
- `dmBotCondition` — stateless-предикат + композиты `And/Or/Not` (фабрики — `dmBotConditions`).
- **Состояния НЕ диктуют переходы** (главное правило при планировании): состояние лишь
  возвращает `EXIT` («моя работа кончилась») или живёт, а ВЫБОР перехода делает FSM по
  guard'ам на рёбрах (`Require`/`BlockWhen` + `CanEnter`). Реакция на внешний фактор
  (напр. «игрок отошёл > порога») — НЕ `if (...) return EXIT;` внутри `OnUpdate`, а
  вытесняющий переход: целевое состояние делаем `PREEMPTIVE`, текущее — `INTERRUPTIBLE`,
  а ребро — `Require(условие)`. FSM сам вытеснит (`SelectPreemptive` раз в
  `DM_FSM_PREEMPT_INTERVAL`). Повторять ошибку «состояние само решает, куда идти» — нельзя.
- Состояния сейчас: **Idle**✅ (глядит/поворачивается), **Patrol**✅ (intent-driven),
  **Stealth**✅ (укрытие: crouch→move→prone→dwell), **Hunting/Fighting/Surrender** ⚠️ заглушки.
- Состояние-владелец держит `ref` на свои интенты и реагирует на `IsFinished()/IsFailed()`;
  обязано ПЕРЕСОЗДАВАТЬ интенты, если их сожрал автодедлайн (`DM_INTENT_MAX_AGE`).

## Интенты

- `dmBotIntent`: `m_Priority` (IDLE/DESIRABLE/CRITICAL), `m_Concurrency`
  (PARALLEL/EXCLUSIVE), `m_Deadline` (-1 = без), `Finish()`/`Fail()`, `IsExpired()`
  (бездедлайновый живёт ≤ `DM_INTENT_MAX_AGE`).
- `dmBotIntentPool`: Insert/Remove/Clear(OnCancel)/Tick(удаляет finished/expired).
- Арбитраж (`UpdateIntents`): три канала — взгляд/движение/стойка; пер-тик сброс в
  «покой» (вперёд/стоять/стоя), победитель переустанавливает. EXCLUSIVE глушит всё,
  PARALLEL — по приоритету.
- **Интент, пишущий канал, обязан пере-записывать его КАЖДЫЙ тик** (как `HoldLook`),
  иначе арбитражный сброс канала вернёт его в «покой». Ошибка: `LookAround` был
  «реализован, но не использовался» — менял направление раз в `m_Interval`, но не
  пере-записывал взгляд между сменами → голова «дёргалась» и возвращалась в центр.
  Переиспользуя/ревьюя интент — проверь, что он держит свой канал потиково.
- Интенты: `MoveTo` (path-aware, прогресс-монитор, один пересчёт), `HoldLook`
  (точка/сущность), `Glance` (голова), `Turn` (тело), `Stance`, `LookAround`.

## Pathfinding

- `dmBotPathfinder` — обёртка над `AIWorld.FindPath` (A* по navmesh) + `SamplePosition`.
- `AIWorld m_AIWorld` — БЕЗ `ref` (`~AIWorld` private, движок владеет);
  `ref PGFilter m_Filter/m_SampleFilter` (создаём через `new`).
- Фильтр ходьбы: include `WALK|DOOR|INSIDE`, exclude `SWIM|SWIM_SEA|SPECIAL|UNREACHABLE`.
- `MoveTo`: `OnStart` → `bot.FindPathTo(target)`; нет пути → `Fail()`. Пер-тик доворот к
  подцели + прогресс-монитор (`DM_MOVE_PROGRESS_EPS`/`DM_MOVE_STUCK_TIME`) → пересчёт 1 раз → `Fail()`.
- **TODO**: recovery при зависании (шаг назад/вбок), двери (`PGPolyFlags.DOOR` + `SetCost`),
  vault/climb (`SPECIAL/JUMP/CLIMB` + `HumanCommandClimb`/`m_JumpClimb`), лестницы
  (`PGPolyFlags.LADDER` + `HumanCommandLadder`/`COMMANDID_LADDER`). Детали — `docs/research/navigation.md`.

## Инвентарь / Loadout

- **Выдача предметов боту — только прямое создание, НЕ `TakeEntityTo*`** (эталон
  `cfgplayerspawnhandler.c`): слот — `GetInventory().CreateAttachmentEx(cls, slotId)`,
  руки — `GetHumanInventory().CreateInHands(cls)`, карго/вложенный предмет —
  `GetInventory().CreateInInventory(cls)`, магазин на оружии — `wep.SpawnAmmo(cls)`.
- `TakeEntityToInventory`/`TakeEntityAsAttachmentEx` — это **перемещение** (`TakeToDst`);
  у свежего `CreateObject`-предмета ещё нет `InventoryLocation` →
  `GetCurrentInventoryLocation` вернёт `false`, предмет «не надевается» и остаётся на полу.
- **Баг вложенного контейнера**: `CreateEntityInCargoEx` не создаст предмет внутри
  контейнера, который сам лежит в карго другого контейнера (вернёт `null`). Обход —
  только как фолбэк (не основной путь): `CreateObject` + `AddEntityToInventory`, а если
  не вышло — «танец на полу» (`TakeToDst(SERVER, parentLoc, ground)` → `AddEntityToInventory`
  → вернуть назад, вокруг `RemoteObjectTreeDelete`/`RemoteObjectTreeCreate`). Эталон —
  `ExpLootSpawner.c` → `DMCreateInInventory`. Реализовано в `dmLoadoutApplier.CreateInContainerFallback`.
- Формат loadout — `botorama/loadouts.md`; применение — `loadout/4_World/`.

## Зрение / слух (восприятие)

- Пока НЕ реализовано (фундамент для боя/лута/реактивного эскорта). Research-детали —
  `docs/research/perception.md`. Стартовые точки: `GetGame().GetPlayers()`,
  `DayZPlayerUtils.SceneGetEntitiesInBox/PhysicsGetEntitiesInBox/GetEntitiesInCone`,
  классы `ZombieBase`/`ZombieMaleBase`/`AnimalBase`, LOS через `DayZPhysics.RaycastRV`.

## Бой

- **Мили-заглушка**: `dmBotMeleeFightLogic_LightHeavy` подменяет `m_MeleeFightLogic`
  (ванильный `DayZPlayerMeleeFightLogic_LightHeavy.HandleFightLogic` дёргает
  `GetCommand_Move()` без null-проверки, а `CanFight()` у AI-бота всегда `true` → VM
  Exception вне MOVE-команды). Наша заглушка просто возвращает `false` (бот не дерётся).
- Реальный мили/огнестрел (прицел/стрельба/перезарядка) — TODO, research `docs/research/combat.md`.
- `HasNoAmmo()` — заглушка `false` (TODO: инспекция магазина). `HasPlayerSigns()` — заглушка `false`.

## Лут

- TODO. Планируется: перцепция предметов → оценка полезности → pickup/drop → состояние
  `Looting`. Research — `docs/research/loot.md`.

## Системы тела (PlayerBase) у AI-бота

- Ванильный тик систем тела (`PlayerBase.OnScheduledTick`) гейтится
  `!IsPlayerSelected()`/`m_AllowModifierTick` — у AI-бота не тикает. Тикаем вручную в
  `CommandHandler`: `GetModifiersManager().OnScheduledTick(dt)` +
  `GetBleedingManagerServer().OnTick(dt)` по пониженной частоте (`DM_BOT_MODIFIER_TICK_INTERVAL`).
- Шок→нокаут: `SetHealth("", "Shock", value)` (инвертирован: `<= UNCONSCIOUS_THRESHOLD (25)`
  = нокаут); мост `StartCommand_Unconscious`/`hcu.WakeUp` (см. `UpdateUnconsciousBridge`).
- Стамина — ванильная (`StaminaHandler` тикает в `CommandHandler`), вес режет кап.
- `GetHealth01()` — нормированное здоровье 0..1.

## Порядок модулей и константы

- Модули грузятся `3_Game` → `4_World` → `5_Mission`; поздний видит константы раннего.
- Внутри модуля порядка файлов нет → зависимые `static const` клади в `cons/<модуль>/constants.c`
  (компилируется первым). Константа, нужная `core/4_World`, — в `cons/4_World`, а не в соседнем модуле.

## DayZ-готчи (чем уже обожглись)

- `GetLookDirection()`/`GetAimDirection()` — это **Expansion, НЕ ванилла** (ошибка
  `Undefined function`). Направление камеры (с питчем) на сервере — кость головы:
  `GetBoneTransformWS("Head")` → `transform[1]` = forward (в костном пространстве forward
  = индекс 1, а не `transform[2]` как в `GetTransform`); позиция глаз — `GetBonePositionWS("Head")`.
  НЕ `MiscGameplayFunctions.GetHeadingVector` — это горизонтальный вектор **корпуса**, без питча.
  Raycast: `DayZPhysics.RaycastRV(beg, end, out pos, out dir, out comp, null, null, ignoreObj, false, false, ObjIntersectView)`.
- `HumanCommandMove.GetCurrentMovementSpeed()` НЕ годится как «иду ли я» (ненулевой во
  время переступания) — используй свой флаг из `SetMove`.
- Цели (паттерн Expansion): `eAITargetInformation` + `eAITargetInformationState`
  (последняя известная позиция, поиск, LOS, threat). У нас упрощённый `dmTarget`
  (entity/class/priority/lastPosition) — скелет, к поведению не подключён.

## Памятки (когда пишешь код)

- **Интент/состояние движения** → помни про цепочку `SetMove(angle, CalcSpeed(target, m_ReachDeadline))`.
  Без дедлайна бот бежит только на preferred speed (jog); чтобы догонял/спринтовал —
  выставляй `m_ReachDeadline`. Ревизуя движение, пройди всю цепочку «кто зовёт SetMove →
  с каким speed → CalcSpeed → m_ReachDeadline» (это был пропущенный недочёт при ревью эскорта).
- **Новый FSM-стейт** → обязательно `GetKind()` (INTERRUPTIBLE, если должен вытесняться боем)
  и `CanEnter()` (условие входа); иначе зависнет или войдёт в нерелевантном контексте.
- **Состояние, владеющее интентом** → держит `ref` и пересоздаёт его, если пул сожрал
  (автодедлайн `DM_INTENT_MAX_AGE`).
- **Триггер «идти/стоять» — по дистанции с мёртвой зоной**, а не по факту смещения игрока.
  Ошибка: follow определял «игрок двигается» по смещению позиции → поворот на месте
  триггерил подход. Правильно: фиксированная точка стояния + пере-выбор стороны только
  при `distToPlayer > мёртвая_зона` (или nudge по таймеру).

## Ключевые файлы

- `botorama/core/4_World/Entities/Bot/dmAISurvivor.c` — мозг (спавн, look, движение, FSM, интенты, патруль, цели).
- `botorama/core/4_World/Entities/Bot/dmAISurvivorBase.c` — пешка (CommandHandler, look/turn/move/stance, body systems).
- `botorama/core/4_World/Entities/Bot/FSM/*` — ядро FSM; `States/*`, `Intent/*`, `Presets/*`, `Pathfinding/*`, `Conditions/*`.
- `botorama/loadout/4_World/` — loadout (dmLoadoutConfig/dmLoadoutApplier).
- `botorama/Animations/` — кастомный граф; `config.cpp` — CfgVehicles + defines.
- `botorama/cons/*/constants.c` — константы; `cons/4_World/defines.c` — дефайны логирования.
