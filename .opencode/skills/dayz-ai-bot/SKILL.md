---
name: dayz-ai-bot
description: Живой справочник по ИИ-ботам для DayZ (мод botorama, префикс dm). Использовать при работе с dmAISurvivor/dmAISurvivorBase, FSM/интентами/состояниями ботов, спавном/синхронизацией, движением/поворотом головы и тела, pathfinding, инвентарём/loadout, боем/лутом. Содержит архитектуру, механики и DayZ-готчи; синтаксис Enfusion — в docs/codeguide.md.
---

# DayZ AI Bot (botorama) — живой справочник

Справочник по механике ИИ-ботов DayZ (мод `botorama`, префикс `dm`).
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

## Сервер/клиент (что где бежит)

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
- **Бот быстрее игрока на walk/jog — не баг, а обход ванильных ограничителей**:
  `OverrideMovementSpeed` ставит индекс скорости напрямую и **пропускает цепочку
  модификаторов, которые режут игрока** (`LimitsDisableSprint`, штраф за поднятое оружие,
  прицеливание, стамину, режим бега). Бот бежит на полной конфиг-скорости; `ApplyMovement`
  воспроизводит только спринт-гейт `CanConsumeStamina(SPRINT) && CanSprint()`.
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
- **Вес перехода — спец-случай приоритета**: если среди подходящих рёбер есть хотя бы
  одно с `weight > 1.0` (приоритетное), в розыгрыше участвуют **только** такие рёбра —
  обычные (`weight <= 1.0`) исключаются. Т.е. `weight > 1.0` = детерминированный выбор
  без розыгрыша; `weight < 1.0` = «низкий приоритет» (участвует, только если нет
  приоритетных). Используется в эскорте: `follow→fight`/`idle→fight` = `2.0`, `idle→follow` = `0.5`.
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
- **Правила проектирования состояний — `docs/fsm-design-guide.md`** (обязательно к чтению
  при создании/ревью FSM-элементов). Главное: логика состояния максимально простая,
  без предиктивной экстраполяции; якорь — по вектору движения, не по facing; «догнать» и
  «держать строй» — разные интенты; скорость — просто; рандом бок/сдвиг — один раз на вход;
  фильтруй мёртвые цели. Детальная документация состояния эскорта — `docs/states/follow.md`.

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
  (точка/сущность), `Glance` (голова), `Turn` (тело), `Stance`, `LookAround`,
  `Flank` (обход дугой за LOS), `EvadeAim` (реверс-Фланг: укрытие по полукругу +
  страйф + threat агрессору; ставится парой с `HoldLook FULL` через `bot.EvadeAim`).
- **Интент НЕ создаёт и НЕ использует другие интенты** (`bot.AddFSMIntent(...)`
  внутри интента — анти-паттерн). Старый `Approach` держал внутренний
  `dmBotIntent_MoveTo` и пересоздавал его — ломалось на автодедлайне пула/арбитраже.
  Фикс: **наследовать** базовый интент (`Approach : MoveTo`, как `FollowTo`/`PickUp`)
  или пусть СОСТОЯНИЕ координирует несколько интентов (как `Fighting`).
- **Пересоздание интента сбрасывает его прогресс-монитор** (`MoveTo.OnStart`
  обнуляет `m_BestDist/m_NoProgressTime/m_RecoverCount/m_Vaulting`). Если состояние
  трешит интенты (создаёт/финишит каждый тик из-за мигающего условия), детектор
  застревания НИКОГДА не наберёт `DM_MOVE_STUCK_TIME` → нет vault/recover → бот
  вечно осциллирует у препятствия. Симптом: в логе ливень `X.OnStart`/`X.OnCancel`.
  Фикс: **гистерезис** решения состояния (менять интент только после удержания
  условия, напр. `DM_FOLLOW_SWITCH_DWELL`). Частая причина мигания — `m_HasLOS`
  гейтится FOV-конусом (`dmVision.c`) и зависит от направления головы, которую
  крутит `LookAround`.

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
- **Лестница (`dmBotIntent_UseLadder` + `dmBotLadderCache`)**: memory-LOD лестницы содержит ДВА
  набора вершин — `ladderN_con` (точки входа: низ/верх) **и** `ladderN_con_dir` (направление
  входа/выхода). Оба обязательны. **Готча (v3.135)**: без `_con_dir` нативный
  `HumanCommandLadder.Exit()` выпускает бота НЕ с той стороны лестницы → падение с крыши
  (входишь с одной стороны, наверху сходишь в пустоту). Дистанция прицепки к входу — ТОЛЬКО 3D
  (не занулять Y в `m_Entry - pos`), иначе бот цепляется к лестнице с другого этажа (был на 9 м
  выше нижней точки). Эталон — ваниль `ActionEnterLadder` (`actionenterladder.c:58,82`) и
  Expansion `ExpansionLadder.m_ConDir[2]` (сортировка по Y, низ=`[0]`/верх=`[1]`).

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
- **Перемещение в руки/слот ИИ-бота — только `LocalTakeToDst` + ручной ре-синк**, НЕ
  `ServerTakeEntityToHands`/`ServerTakeToDst`/`TakeToDst(SERVER)`: у серверного ИИ (нет
  клиента) SERVER-режим не кладёт предмет — руки остаются пустыми (`GetEntityInHands()`=null →
  `HasNoAmmo()`=true → Shooting-фликер/EXIT каждый кадр). Паттерн: `GetCurrentInventoryLocation(src)`
  → `dst.SetHands`/`SetAttachment` → `GetGame().RemoteObjectTreeDelete(item)` → `LocalTakeToDst(src,dst)`
  → `GetGame().RemoteObjectTreeCreate(item)`. Реализовано в статике `dmLoot.TakeToHands`/`TakeToAttachmentSlot`/
  `TakeIntoCargo`/`TakeIntoDestination` (первый аргумент `PlayerBase pawn`), НЕ на пешке (пешка держит только
  `override bool DropItem(ItemBase)` — ванильный контракт). **На ревью**: любой `ServerTakeEntityToHands`/
  `ServerTakeToDst`/`TakeToDst(SERVER)` у ИИ — красный флаг.
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
  `TargetSelection()` без райкаста: цель берётся из **явного** `pawn.GetMeleeAttackTarget()`
  (ставит `dmBotIntent_HitTo` через `RequestMeleeAttack`), НЕ из `GetHostileTarget()`;
  `GetReach()` — публичная обёртка protected `GetRange()`.
- **Реактивная модель угрозы**: бот дерётся только с тем, кто его ударил. Обнаруженные
  сущности низкоугрозны (`DM_TARGET_THREAT_PLAYER 0.1 / _ANIMAL 0.2 / _ZOMBIE 0.3`).
  Враждебность (`threat >= DM_ATTACK_THREAT_THRESHOLD = 0.5`) ставит только
  `RegisterDamageThreat(source, damage)` из `EEHitBy` (резолвит `GetHierarchyRootPlayer()`,
  threat 0.8/0.9 по HP-урону). `GetHostileTarget()` = ближайшая **живая** цель
  `threat >= 0.5`, без дистанции; мёртвые пропускаются.
- **Состояние `dmBotState_Fighting`** (PREEMPTIVE, без raised, бьём из ERECT) — тонкий
  координатор: `m_TargetEntity`/`m_Target` + 3 интента CRITICAL+PARALLEL (`Approach` —
  подойти/пересоздать MoveTo при дрейфе; `HitTo` — удар по кулдауну `m_MeleeCooldown`
  (на мозге) при `dist <= GetMeleeReach()` + `|angle| <= DM_MELEE_FACE_ANGLE` + `m_HasLOS`;
  `Evasion` — стрейф ±90° пока кулдаун >0) + `HoldLook` (FULL). Кулдаун тикается в
  `OnUpdate`. Пере-резолв цели на входе/при смерти/раз в `DM_FIGHT_RETARGET_INTERVAL`.
  `EXIT` когда `GetHostileTarget()` пусто.
- **Пресеты**: `dmBotPreset_Combat` (Idle + Fighting, вход по `HasHostile`);
  `dmBotPreset_Escort` — Fighting входит по `HasHostile` из Idle/Follow (Follow выходит
  в Fighting при `GetHostileTarget() != null`). `/bot combat` переключает боевой пресет.
- **Огнестрел (T11)** — `dmBotState_Shooting` (PREEMPTIVE: Raise→aim→fire по кулдауну→EXIT при пустом магазине) + примитивы пешки:
  - **подъём** — кастомная граф-переменная `dmAI_Raised` + `AnimSetBool` (вшита в граф), НЕ raised-стойка;
  - **прицел** — единый механизм `dmAiming` (интент/тест: `aiming.SetTarget(entity)` + `Enable()`,
    мозг тикает `OnUpdate` после интентов; `OnUpdate` кладёт направление/дистанцию/EMA-скорость
    цели в пешку через `SetAim(direction, targetPos, distance, targetVelocity)`) + кастомные
    `dmAI_AimX/AimY` + `SetADS`;
  - **выстрел** — `dmBot_Fire(mi)` → `ComputeShot` (aim + дроп без рейкаста + оружейный
    dispersion) → `Fire(mi, pos, dir, dir)` с явным `dir`, через `modded WeaponFire` (ванильный
    `TryFireWeapon` берёт `GetCameraPoint` — внутренний прицел, у ИИ не задан);
  - **перезарядка** — `dmBotWeaponManager : WeaponManager` (серверный `StartAction`/`OnWeaponActionEnd`) + `ReloadWeaponAI()`;
  - **звук** — `modded Weapon_Base.SyncEventToRemote` (шлёт `INPUT_UDT_WEAPON_REMOTE_EVENT` для `INSTANCETYPE_AI_SERVER`);
  - **`HasNoAmmo()`** — реальная инспекция магазина (`IsChamberEmpty/FiredOut` + `Magazine.GetAmmoCount()`).
- **Готовность оружия — ДВЕ разные проверки, не путать** (готча-рефлексия из flytime):
  - `IsReadyToShoot()` = **поднято** (`m_WeaponRaised && timer >= m_RaiseReadyDuration`) **И** ствол
    заряжен. Возвращает `false` МОЛЧА (без лога `[Weapon] IsReadyToShoot`) на ранней ветке «не поднято» —
    лог `[Weapon] IsReadyToShoot: ...` печатается ТОЛЬКО когда поднято, а ствол пуст/стрелян/заклинил.
    Т.е. отсутствие этого лога ≠ «кончились патроны», а «ещё поднимаю».
  - `IsWeaponReady()` = ствол заряжен **независимо от подъёма** (`!IsChamberFiredOut && !IsJammed && !IsChamberEmpty`).
    Разница `IsWeaponReady() && !IsReadyToShoot()` = «поднимаю»; `!IsWeaponReady()` = «ствол пуст/стрелян».
  - `ReloadWeaponAI()` возвращает `false` в ДВУХ случаях: «перезаряжать нечего» (ствол заряжен — бот ещё
    поднимает) И «реально кончились патроны». Поэтому зови его только под гейтом `!IsWeaponReady()`:
    тогда `false` = точно «патроны кончились». Не гейть перезарядку по счётчику фазы (`m_SubPhase > 0`) —
    тот сбрасывается при переходе фазы и оставит стреляный ствол без перезарядки (бесконечное ожидание).
- **Ванильные «конкуренты» прицела**: `HandleWeapons`/`HandleADS`/`HandleOptic` (все зовутся из `CommandHandler`, каждый может `ExitSights()→SetADS(false)`) + `AimingModel` — переопределить no-op'ом/false, иначе наш `SetADS` топчется и ствол трясётся/не поднимается. `HasPlayerSigns()` — заглушка `false`.

## Лут

- **`dmLoot` (статик)** — лутинг-движок: `GetCategory` (FOOD/WEAPON/MELEE/MAGAZINE/AMMO/CLOTHING/
  REPAIR/MEDICAL/OTHER), `ScanNearbyItems`, категорийное определение места и примитивы (см. Инвентарь).
  **Готча мили**: НЕ используй `IsMeleeWeapon()` (флаг `isMeleeWeapon`/`m_IsMeleeWeapon` даёт ложные
  срабатывания — `BomberJacket_Brown`, и рюкзаки уходили в MELEE вместо CLOTHING). Надёжная
  классификация — `dmLoot.IsMelee(item)` (статик): НЕ `IsWeapon()` + `inventorySlot`/`itemInfo`
  содержит Knife/Melee/Shoulder/Axe. `dmAISurvivor.EntityIsMelee` — делегат в `dmLoot.IsMelee`.
  Определение места **категорийно**, НЕ дженерик-скан слотов (готча: сканирование слотов по порядку
  кладёт штаны в карго занятой куртки, не дойдя до пустого LEGS):
  - `FindAttachmentSlot(pawn, item, out slotId)` — WEAPON→`SHOULDER`; MELEE→`MELEE`→`SHOULDER`;
    CLOTHING→слот из `ConfigGetTextArray("inventorySlot")`. **Готча**: для CLOTHING проверку
    «свободен ли слот» делай через `inv.HasAttachmentSlot(slotId)` + `!inv.FindAttachment(slotId)` —
    НЕ `CanAddAttachmentEx` (тот возвращает `false` для одежды/рюкзака → предмет уходит в карго или
    «нет места»). `CanAddAttachmentEx` оставлен только для WEAPON/MELEE (различает винтовку/пистолет,
    нож/топор — там он валиден).
  - `FindCargo(pawn, item, out dst)` — свободное карго ВСЕГО одетого инвентаря (не только рюкзак).
  - `FindDestination(pawn, item, out dst)` = `FindAttachmentSlot` → иначе `FindCargo`. **Каждая
    return-true ветка обязана заполнить `out`** (иначе `LocalTakeToDst` с пустым `dst` → NULL-ptr).
  - Порядок из спеки: оружие/мили без свободного слота → в карго (инвентарь).
- **`dmBotIntent_PickUp`** — поток решения: `OnReachedGoal` → `InventoryPickUp` (слот/карго);
  нет места → `Evacuate` (репак-дефрагментация) → иначе `Wishlist.Ignore`+`Fail`.
  **`Evacuate`** (в интенте, высокоуровнево): `Requirements.GetDiscardOrder()` (по возрастанию
  индекса необходимости) → дроп всех предметов на пол → коллект «новая вещь первой, затем order
  с конца» — всё ПАРАЛЛЕЛЬНЫМИ `Enqueue` (НЕ `Then`/`SuccessOnly`: фейл одного шага не прерывает).
  После репака — `IgnoreLeftovers` (выложенное, что не легло обратно — на полу) → игнор.
- **Сравнение одежды** (`dmAISurvivor.IsBetterClothing`, авторитетно в `CalcDesired`): берём только
  лучше надетого, строгий лексикографический порядок — cargo (`CargoCapacity` = ширина×высота карго)
  → `GetHeatIsolation()` → `GetHealth()` → `GetMaxHealth()`; все равны → НЕ брать; пустой слот → брать.
- **`dmInventoryFrame` — два РАЗНЫХ сигнала завершения** (не путать):
  - `IsAllDone()` — **«всё успех»**: каждый лист выполненной ветки вернул успех
    (листовой фрейм → `m_Success`).
  - `IsAllFinished()` — **«доигралось»**: выполненная ветка доигралась, НЕ важно успех или нет
    (листовой фрейм → `m_Done`). Пример: `dmBotState_Fighting` ждёт `IsAllFinished()` экипировки —
    «цепочка доделалась», чтобы на следующем тике пересобрать, даже если шаг провалился.
- **«Всё успех» ≠ «операция успешна»** (готча-рефлексия): у дерева одежды
  `InventoryChangeClothes` при неудаче новой вещи фейл-ветка «надеть старую обратно» доигрывается
  успешно → `IsAllDone()` вернёт `true`, хотя операция семантически провалилась (новая вещь не
  надета). Поэтому `dmBotIntent_PickUp` судит об исходе по **фактическому состоянию мира**, а не
  по флагам фрейма: успех = `item.GetHierarchyRootPlayer() != null` (вещь попала в иерархию игрока,
  а не осталась на полу), неудача = `GetInventoryFrames().IsEmpty() && GetHierarchyRootPlayer()==null`
  (очередь опустела, а вещь не легла). **Правило**: исход многошаговой фрейм-операции проверяй по
  фактическому состоянию мира (где лежит предмет), а не по `IsAllDone`/`IsAllFinished`.

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
- **Кость головы ≠ прицел.** `transform[1]` головы отклонён от реального ствола на ~12.5° по
  питчу (и ~5° по яу) — голова на сервере не следует стволу по питчу. Для «куда целится игрок»
  применяй поправку (эталон — Expansion `Expansion_GetAimDirection`, SERVER-ветка):
  `angles = headDir.VectorToAngles(); angles[0] = angles[0] + 5; angles[1] = angles[1] + 12.5;`
  (с wrap `>360 → -=360`); `dir = angles.AnglesToVector()`. Без поправки питч-ошибка ~13°
  ломает угловые проверки «целятся в меня» (реализовано как `dmVision.GetPlayerAimDir`).
- `HumanCommandMove.GetCurrentMovementSpeed()` НЕ годится как «иду ли я» (ненулевой во
  время переступания) — используй свой флаг из `SetMove`.
- Цели (паттерн Expansion): `eAITargetInformation` + `eAITargetInformationState`
  (последняя известная позиция, поиск, LOS, threat). У нас упрощённый `dmTarget`
  (entity/class + память lastPosition/LOS/lastContact + оценка threat/attractiveness/
  friendly) — `threat`/`friendly` уже питают бой (`GetHostileTarget`); attractiveness — к
  луту ещё не подключена.
- **Engine-driven граф-переменные**: `Raised`/`AimX`/`AimY`/`AimIKX` — проекция
  стойки/прицела, движок перезаписывает их каждый кадр → `AnimSetBool/Float` на них —
  no-op. Для ИИ нужны **свои** переменные (`dmAI_Raised`/`dmAI_AimX/AimY`) + вшивка в граф
  (добавить `#Var` в `player_main.agr`, заменить токен в `.agr`).
- **`STANCEIDX_RAISED` — НЕ маска/смещение**: enum последовательный (`ERECT=0, CROUCH=1,
  PRONE=2, RAISEDERECT=3, RAISEDCROUCH=4, RAISEDPRONE=5, RAISED=6`). Смещение «raised» = 3,
  а `STANCEIDX_RAISED` (=6) — sentinel. Поднимать оружие через стойку — только
  `STANCEIDX_RAISEDERECT` явно; комментарий ванили «ERECT+RAISED=RAISEDERECT» — устаревший.
- **Ванильные «конкуренты» прицела**: `DayZPlayerImplement.CommandHandler` зовёт ТРИ
  метода оружия подряд — `HandleADS()`, `HandleWeapons()`, `HandleOptic()` — и каждый может
  `ExitSights()→hcw.SetADS(false)`. Чтобы гнать `SetADS` самому, переопределить ВСЕ ТРИ
  no-op'ом + `AimingModel(...)→false`. (Мы гоняли ADS в `ApplyWeaponADS` после super.)

## Памятки (когда пишешь код)

- **Сравнивая два направления (прицел/взгляд vs «на цель») — оба вектора обязаны выходить из ОДНОЙ точки.**
  Ошибка (агр-детект «в меня целятся», v3.122): прицел игрока брался из кости головы
  (origin = голова), а «игрок→бот» считался от НОГ (`GetPosition()`). Разница высот ~1.5 м
  на близкой дистанции даёт питч-ошибку `atan(height/dist)` (25–60°) — она всегда превышала
  угловой полуразмер цели, и условие не срабатывало. Яу не страдал (ноги/голова — одна XZ),
  поэтому баг выглядел «только по вертикали». Правило: `dirToTarget = TargetBone - SourceBone`
  (origin = `GetBonePositionWS("Head")` или `pos + (0, DM_EYE_HEIGHT, 0)`, эталон — `LookAtPoint`),
  цель — кость центра масс (`Spine3`), а не `pos + фикс.высота`. Проверяй питч по логам:
  при точном наведении `pitch ≈ 0`, а не десятки градусов.
- **Интент/состояние движения** → помни про цепочку `SetMove(angle, CalcSpeed(target, m_ReachDeadline))`.
  Без дедлайна бот бежит только на preferred speed (jog); чтобы догонял/спринтовал —
  выставляй `m_ReachDeadline`. Ревизуя движение, пройди всю цепочку «кто зовёт SetMove →
  с каким speed → CalcSpeed → m_ReachDeadline» (это был пропущенный недочёт при ревью эскорта).
- **Новый FSM-стейт** → обязательно `GetKind()` (INTERRUPTIBLE, если должен вытесняться боем)
  и `CanEnter()` (условие входа); иначе зависнет или войдёт в нерелевантном контексте.
- **Состояние, владеющее интентом** → держит `ref` и пересоздаёт его, если пул сожрал
  (автодедлайн `DM_INTENT_MAX_AGE`).
- **Кулдаун-таймер, гейтящий создание интента, ОБЯЗАН вооружаться** (получать
  положительное значение) в момент создания интента. Ошибка: `m_LastLook` в
  `dmBotState_Follow` только декрементился, но никогда не ставился в `= deadline` →
  «look at target» спамил CRITICAL `HoldLook` каждый тик при смене дистанции.
  Пишешь `if (m_Timer == 0.0) { CreateIntent(); }` — сразу ставь `m_Timer = deadline;`.
- **Интенты кэшируют цель на момент создания** → когда состояние пере-резолвит НОВУЮ цель
  (пере-таргетинг внутри одного стейта, напр. бот убил зомби №1 → `ResolveTarget` взял
  №2), старые интенты продолжают бить/смотреть в СТАРУЮ цель (труп). Фикс: в `ResolveTarget`
  при смене сущности цели (`newEntity != m_TargetEntity`) `Finish()`+`null` ВСЕ интенты —
  тогда `Ensure*`/`CreateLook` пересоздадут их под новую цель. Симптом был «стоит спиной
  к живому зомби после убийства первого».
- **Триггер «идти/стоять» — по дистанции с мёртвой зоной**, а не по факту смещения игрока.
  Ошибка: follow определял «игрок двигается» по смещению позиции → поворот на месте
  триггерил подход. Правильно: фиксированная точка стояния + пере-выбор стороны только
  при `distToPlayer > мёртвая_зона` (или nudge по таймеру).
- **Подъём/прицел оружия у ИИ** → кастомные граф-переменные (`dmAI_Raised`/`dmAI_AimX/Y`),
  НЕ raised-стойка и НЕ ванильные `Raised`/`AimX/Y` (engine-driven, см. DayZ-готчи).
  Граф: добавить `#Var` в `player_main.agr` + заменить токен в `Locomotion.agr`/`Actions.agr`.
  Выстрел — явный `dir` в `Fire(mi,pos,dir,dir)`, не ванильный `TryFireWeapon` (`GetCameraPoint`).
- **Моторика/оружие в машине**: пока бот сидит в транспорте (`GetCommand_Vehicle() != null`,
  см. `dmAISurvivorBase.IsInVehicle()`), `CommandHandler` обязан НЕ звать `ApplyBodyTurn`/
  `ApplyMovement`/`ApplyStance` и оружейные `ApplyWeaponRaise`/`ApplyWeaponAim`/`ApplyWeaponADS`/
  `TryFireWeapon` — иначе Look-интент крутит корпус через `ApplyBodyTurn` (SetOrientation/
  foot-step) прямо в кресле, и бот «вращается в сторону». Голову (`ApplyLookVars`) НЕ
  блокировать. Мозг при этом может продолжать писать move/look интенты — пешка авторитетно
  игнорирует (`if (inVehicle) { ResetMotorActuation(); return; }` после `TickVehicle()`,
  плюс опустить оружие в `else`-ветке). `TickVehicle()` ранний-return покрывает только
  переходы get-in/get-out, а НЕ устоявшееся сидение — это и была причина бага.

- **Дверь машины** → анимационная фаза на `CarScript`, а не отдельный натив:
  `seat → GetDoorSelectionNameFromSeatPos(seat) → GetAnimSourceFromSelection(sel)` даёт имя
  анимации; есть ли дверь — `GetDoorInvSlotNameFromSeatPos(seat) + GetCarDoorsState(slot)`
  (`CarDoorState.DOORS_MISSING`). Открыть/закрыть — `car.SetAnimationPhase(anim, 1.0/0.0)`;
  ждать — ПОЛЛИНГ `car.GetAnimationPhase(anim)` по порогам (ваниль: открыта > 0.5), не
  фикс-таймер. Позицию в кадр detach'а выхода читать через `GetWorldPosition()` (а не
  `GetPosition()` — тот на один кадр отдаёт vehicle-local `(0,0,0)`), а после выхода
  снапнуть на землю `GetGame().SurfaceY(x, z)` (натив `Game`): `pos[1] = GetGame().SurfaceY(pos[0], pos[2])`.
- **Интент, владеющий посадкой/выходом, — FSM-интент → его отменяет `ClearFSMIntents()` при
  переходе FSM.** Если `Finish()` интента запускает выход (дверь→вылезть→дверь), а стейт
  в тот же тик вернул EXIT, FSM перейдёт и отменит интент ДО его `OnUpdate` → бот зависает
  в машине. Фикс: стейт держит `ref` на интент до `IsFinished()` (снять только когда выход
  завершился) → `CanExit()` возвращает `false`, FSM не переходит, интент доигрывает выход.
  Готча проявилась на машине БЕЗ двери (поллинг двери мгновенно true, но интент всё равно
  успевали отменить в тот же тик).

## Ключевые файлы

- `botorama/core/4_World/Entities/Bot/dmAISurvivor.c` — мозг (спавн, look, движение, FSM, интенты, патруль, цели).
- `botorama/core/4_World/Entities/Bot/dmAISurvivorBase.c` — пешка (CommandHandler, look/turn/move/stance, body systems).
- `botorama/core/4_World/Entities/Bot/FSM/*` — ядро FSM; `States/*`, `Intent/*`, `Presets/*`, `Pathfinding/*`, `Conditions/*`.
- `botorama/loadout/4_World/` — loadout (dmLoadoutConfig/dmLoadoutApplier).
- `botorama/Animations/` — кастомный граф; `config.cpp` — CfgVehicles + defines.
- `botorama/cons/*/constants.c` — константы; `cons/4_World/defines.c` — дефайны логирования.
