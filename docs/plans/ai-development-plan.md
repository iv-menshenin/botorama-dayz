# План развития ИИ ботов (дорожная карта)

Упорядоченный список атомарных задач. Правило: **зависимости раньше** (если X невозможен
без Y — Y впереди); внутри уровня без зависимостей — по сложности. Статус отмечается
чекбоксом; выполненные задачи фиксируются коммитом.

Легенда статусов: `[ ]` не начато · `[~]` в работе · `[x]` готово.

## Порядок

### Группа 0 — фундамент (движение + состояние)
1. `[x]` **MoveTo: recovery при застревании** — шаг назад/вбок вместо мгновенного `Fail()`.
   Deps: нет. Файлы: `Intent/dmBotIntent_MoveTo.c`, `cons/4_World/constants.c`. Критерий:
   бот у стены не зависает, а отходит и перепрокладывает. — сделано: фаза recovery
   (назад/вбок + пере-прокладка, до `DM_MOVE_MAX_RECOVER` попыток).
2. `[x]` **`HasNoAmmo()` настоящий** — инспекция магазина в руках/оружии. Deps: нет.
   Файлы: `dmAISurvivor.c`. Критерий: `/bot status`/условия различают «нет патронов».
   — сделано (в рамках огнестрела): `IsChamberEmpty/FiredOut` + `Magazine.GetAmmoCount()`.

### Группа 1 — зрение (perception)
3. `[x]` **Детект игроков и заражённых** — серверный скан (box/cone + FOV + LOS), отдаёт
   список угроз. Deps: нет (research: `docs/research/perception.md`). Файлы: новый
   `Perception/` (или в `dmAISurvivor`), константы. Критерий: дебаг-команда печатает
   увиденных зомби/игроков.
4. `[x]` **Память целей** — подключить `dmTarget` (пополнение, lastPosition, приоритет,
   забывание). Deps: 3. Файлы: `dmTarget.c`, `dmAISurvivor.c`.

### Группа 2 — эскорт (видимый прогресс)
5. `[x]` **Follow-движение** — точка сбоку игрока, пересчёт при движении. Deps: 1.
   Файлы: `dmAISurvivor.c` (+интент или режим). Критерий: бот идёт бок о бок, не сзади.
   — сделано без T1 (recovery) для happy path; recovery — отдельная доводка.
6. `[x]` **Состояние `Escort/Follow`** — вести игрока + периодический осмотр головой.
   Deps: 5. Файлы: `States/dmBotState_Follow.c`, `States/dmBotState_Idle.c`,
   `Intent/dmBotIntent_LookAround.c`, `FSM/Conditions/dmBotCondition_FollowFar.c`,
   `Presets/dmBotPreset_Escort.c`, `/bot follow [stop]`.
   Итоговая схема: `Follow(PREEMPTIVE)` / `Idle(INTERRUPTIBLE)`; `idle→follow`
   через `Require(FollowFar)` (цель > `GetThresholdDistance`: игрок 5м / прочее 1м);
   якорь «плечо ±1м для игрока/бота, 1м не доходя для предмета»; скорость по дистанции
   (sprint > 15м / jog > 10м / walk); выход Follow — цель стоит 3с; scan-интент
   (голова 5–35°/корпус 15–120°); `DM_LOOK_TURN_SPEED=3` (плавнее).

### Группа 3 — продвинутый pathfinding
7. `[x]` **Vault/climb** — перепрыгивание забора / влезание на ящик. Deps: 1.
   Research: `docs/research/navigation.md`. — сделано: фильтр `JUMP|CLIMB`, примитив
   `dmAISurvivorBase.TryVaultClimb()` (DoClimbTest → JumpOrClimb), в MoveTo при
   застревании + фаза vaulting.
8. `[x]` **Двери** — открыть и пройти. Deps: 1. — сделано: фильтр `DISABLED`+cost,
   `dmAISurvivor.TryOpenDoorOnPath()` (рейкаст → Building.OpenDoor), проактивный
   троттл-рейкаст в MoveTo.
9. `[x]` **Лестницы** — подъём/спуск. Deps: 1. (самое сложное в навигации). —
   сделано: фильтр `LADDER`+cost, `dmBotLadderCache` (парсинг memory LOD,
   `Expansion_GetLaddersCount`-аналог), `dmBotIntent_UseLadder` (EXCLUSIVE:
   подход→прицепка→подъём→отцепка), хук в MoveTo при застревании.

### Группа 4 — бой
10. `[x]` **Мили против заражённых** — реальная атака. Deps: 3, 4. Research: `combat.md`.
11. `[x]` **Огнестрельный бой** — прицел/стрельба/перезарядка + точность (dmAiming). Deps: 3, 4, 2. Research: `combat.md`.
    — сделано: `dmAISurvivorBase` (RaiseWeapon/SetAimTarget/GetWeaponAimDirection/dmBot_Fire/
    `modded WeaponFire`/`dmBotWeaponManager`+ReloadWeaponAI), `dmBotState_Shooting`, `HasNoAmmo`;
    режимы HIP/ADS (`dmBotAimMode`); точность — `dmAiming` (спред/точка прицеливания) + readiness
    (`IsReadyToShoot`/`IsRaising`) + `dmBotIntent_Aim` + выбор режима по дистанции/грейсу
    (см. `docs/plans/aiming-design.md`).
12. `[x]` **Состояние `Fighting` + реакция на угрозу**. Deps: 10, 11.
    — сделано: мили-Fighting (тонкий координатор + Approach/Evasion/HitTo/HoldLook), реактивная
    угроза (`GetHostileTarget`/`RegisterDamageThreat`, порог `DM_ATTACK_THREAT_THRESHOLD`).

### Группа 5 — лут
13. `[x]` **Перцепция предметов**. Deps: 3. Research: `loot.md` + `perception.md`.
    — сделано: `dmLoot.ScanNearbyItems` (SceneGetEntitiesInBox + ItemBase-фильтр).
14. `[x]` **Оценка полезности** (score). Deps: нет.
    — сделано: `dmLoot.GetCategory` + `dmWishlist.CalcDesired` + `dmRequirements` (индексы) + `dmNeeds`.
15. `[x]` **Pickup/drop примитивы**. Deps: нет (инвентарь освоен).
    — сделано: `dmBotIntent_PickUp` (наследник MoveTo) + `dmAISurvivorBase.DropItem`.
16. `[x]` **Состояние `Looting`** (у нас — `Exploration`). Deps: 13, 14, 15, 1.
    — сделано: `dmBotState_Exploration` (блуждание + оппортунистический подбор + выброс при переполнении)
    + `dmExplorer` (здания visited/забывание). См. `docs/plans/looting-and-exploration.md`.

### Группа 6 — интеграция
17. `[ ]` **Реактивный эскорт** — эскорт → угроза → бой → возврат. Deps: 6, 12.

## Первая веха (тестируемая)

Эскорт без боя: **задачи 5–6** (follow + состояние + `/bot follow`). Бот следует за
игроком бок о бок и периодически осматривается. Зрение/бой — следующие вехи.

## Воркфлоу выполнения (на каждую задачу)

0. **Prep** (оркестратор): research сигнатур (или делегировать `dayz-research`), читает
   текущее состояние, готовит task-spec (файлы + сигнатуры + запреты + чеклист + критерий).
1. **Delegate**: `Task` → субагент `dayz-dev` (преамбула: прочитать `docs/codeguide.md`,
   вызвать скилл `dayz-ai-bot`, чеклист).
2. **Review**: оркестратор читает diff, прогоняет чеклист codeguide, сверяет сигнатуры с ванилью.
3. **Classify**: синтаксис/движок → `docs/codeguide.md`; механика бота → скилл `dayz-ai-bot`;
   новые сигнатуры API → `docs/research/*.md`.
4. **Rework**: продолжить ту же сессию субагента (`task_id`) с перечнем ошибок.
5. **Record**: обновить статус здесь, bump `DM_BOTORAMA_VERSION`, git commit.
