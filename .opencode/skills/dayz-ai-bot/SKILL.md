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
  preferred speed (jog) — спринта не будет, без единой ошибки. Follow управляет скоростью
  сам — `SetPreferredSpeed` по дистанции до цели (sprint > `DM_FOLLOW_SPRINT_DISTANCE` 15м,
  jog > `DM_FOLLOW_JOG_DISTANCE` 10м, walk иначе) с save в `OnEntry` / restore в `OnExit`,
  а не через `m_ReachDeadline`. См. «Памятки».
- **Якорь эскорта (Follow)**: для `PlayerBase` (игрок/бот) — плечо `±DM_FOLLOW_SIDE_DISTANCE`
  (1м) вбок от направления взгляда цели (знак — `m_SideSign`, рандом в `OnEntry`); для
  предмета — точка в `DM_FOLLOW_SIDE_DISTANCE` не доходя по линии бот→предмет.
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
  **Stealth**✅ (укрытие: crouch→move→prone→dwell), **Fighting**✅ (мили: подойти/HoldLook
  FULL/удар по кулдауну), **Hunting/Surrender** ⚠️ заглушки.
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

- **Реализовано (T3)**: `dmVision` (`core/4_World/Entities/Bot/Perception/dmVision.c`) —
  поле `ref dmVision m_Vision` в мозге, тикает из `OnUpdate` с троттлингом
  `DM_PERCEPTION_INTERVAL` (0.3 c).
- Пайплайн `Scan()`: box-запрос вокруг бота (`SceneGetEntitiesInBox` с `QueryFlags.DYNAMIC`
  или `PhysicsGetEntitiesInBox` — переключается `ToggleQuery()`/`m_UseScene`) →
  классификация (`PlayerBase.Cast` / `IsInherited(ZombieBase)` / `IsInherited(AnimalBase)`)
  → дистанция → FOV (полуугол `DM_PERCEPTION_FOV/2` от направления взгляда) → LOS.
- Направление взгляда — корпус+голова: head-bone `GetBoneTransformWS` → `transform[1]`
  (forward), кэш `m_HeadBone`; нет кости → `GetDirection()`. `lookDir[1]=0` + `Normalize()`.
- LOS — `DayZPhysics.RaycastRVProxy(RaycastRVParams(eye, end, pawn), hits)`: глаза
  (`botPos + Vector(0, DM_EYE_HEIGHT, 0)`) → голова цели (`GetBonePositionWS("Head")`,
  фолбэк — ноги+высота глаз); видно, если ближайшее попадание `hits[0].obj`/`.parent`
  — сама цель. **Готча**: `GetBoneIndexByName` нет на `EntityAI` — каст к `Human`
  (игрок) или `DayZCreature` (зомби/животное) перед вызовом (`Undefined function`).
- Результат — `dmTarget` = **память + оценка** (T4): `m_Type` (DESTROY/ACQUIRE), `m_Entity`,
  `m_ClassEntity` (лут, когда `m_Entity == null`), память `m_LastPosition`/`m_HasLOS`/
  `m_LastContact` (время = `GetGame().GetTickTime()`, float секунды монотонного серверного
  времени), оценка `m_Threat`/`m_Attractiveness` (0..1) и `m_Friendly` (пока = только
  цель эскорта `GetFollowTarget()`).
- `Scan()` делает merge-модель, а НЕ `ClearTargets()`: `BeginTargetScan()` сбрасывает
  `m_HasLOS`, `RememberTarget(entity, threat, attract, friendly, pos)` создаёт/обновляет
  цель (`m_HasLOS = true`, `m_LastContact = now`), затем `ForgetStaleTargets(DM_TARGET_FORGET_TIME = 300с)`
  выкидывает цели без контакта дольше таймаута. Цель, скрывшаяся за стеной/кустами,
  остаётся в `m_Targets` с последней известной позицией до таймаута. Оценка по виду —
  `DM_TARGET_THREAT_*` / `DM_TARGET_ATTRACT_*` в `cons/4_World/constants.c`.
- Команда `/bot vision [switch]` — печать видимых целей или переключение Scene/Physics.
- Слух и перцепция предметов (лут) — TODO. Research-детали — `docs/research/perception.md`.

## Бой

- **Мили реализовано (T10)**: `dmBotMeleeFightLogic_LightHeavy` подменяет `m_MeleeFightLogic`
  (ванильный `DayZPlayerMeleeFightLogic_LightHeavy.HandleFightLogic` дёргает
  `GetCommand_Move()` без null-проверки, а `CanFight()` у AI-бота всегда `true` → VM
  Exception вне MOVE-команды) и сам ведёт один light/heavy удар по запросу мозга
  (`RequestMeleeAttack`/`HasMeleeAttackRequest`/`ConsumeMeleeAttackRequest`) через
  `StartCommand_Melee2` из ERECT (без raised). Урон — `ProcessMeleeHitName` по имени
  компонента (`GetDefaultHitComponent()`); по зомби — ×2 (`DM_MELEE_DAMAGE_MULT_ZOMBIE`)
  циклом в `EvaluateHit`.
- **Магия-цель** `dmBotMeleeCombat : DayZPlayerImplementMeleeCombat` — override `Update()`
  (на сервере всегда `Reset` → `TargetSelection` → `SetFinisherType(-1)`) и
  `TargetSelection()` без райкаста: цель берётся из `bot.GetHostileTarget()`;
  `GetReach()` — публичная обёртка protected `GetRange()`.
- **Состояние `dmBotState_Fighting`** (PREEMPTIVE, без raised, бьём из ERECT): линейный
  флоу в `OnUpdate` — подойти (`dmBotIntent_MoveTo`, пересоздание при дрейфе цели >1м) →
  держать `dmBotIntent_HoldLook` (FULL — корпус к врагу) → удар по кулдауну
  (`DM_MELEE_COOLDOWN`) при `dist <= GetMeleeReach()`, `|angle| <= DM_MELEE_FACE_ANGLE`
  и `m_HasLOS`; `EXIT` когда цели нет или враг мёртв (`IsAlive()`).
- **Пресеты**: `dmBotPreset_Combat` (Idle + Fighting, вход из Idle по `ThreatInRange`);
  в `dmBotPreset_Escort` добавлен Fighting (реакция на угрозу срабатывает из Idle,
  Follow остаётся PREEMPTIVE). `/bot combat` переключает бота на боевой пресет.
- Огнестрел (прицел/стрельба/перезарядка) — TODO, research `docs/research/combat.md`.
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
  (entity/class + память lastPosition/LOS/lastContact + оценка threat/attractiveness/
  friendly) — к поведению (бой/лут) ещё не подключён.

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
