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
   Research: `docs/research/navigation.md`. — сделано и проверено в игре (бот перелезает
   забор): фильтр `JUMP|CLIMB`, примитив `dmAISurvivorBase.TryVaultClimb()` —
   `DoClimbTest` → **напрямую `StartCommand_Climb(res, climbType)`** (НЕ `JumpOrClimb()`:
   тот делает свой `DoPerformClimbTest`-ретест + `Jump()`-фолбэк и ломает ИИ), в MoveTo при
   застревании + фаза vaulting. Чтобы детектор застревания реально срабатывал — фиксы
   Follow: `useFollow` по рекенси `m_LastContact` (не по мигающему `m_HasLOS`) + пересчёт
   пути `FollowTo` только при сдвиге якоря (`DM_FOLLOW_REPATH_DIST`) — иначе прогресс-монитор
   сбрасывался каждую секунду и vault не запускался.
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
    — сделано: `dmAISurvivorBase` (RaiseWeapon/SetAim/GetWeaponAimDirection/dmBot_Fire/
    `modded WeaponFire`/`dmBotWeaponManager`+ReloadWeaponAI), `dmBotState_Shooting`, `HasNoAmmo`;
    режимы HIP/ADS (`dmBotAimMode`); точность/прицел — `dmAiming` (SetTarget/Enable/OnUpdate →
    SetAim: спред/точка прицеливания/дистанция/скорость) + readiness (`IsReadyToShoot`/`IsRaising`)
    + `dmBotIntent_Aim` + выбор режима по дистанции/грейсу (см. `docs/plans/aiming-design.md`).
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

### Группа 7 — мир: POI-реестр, спавн «по городам», кочевничество
> Реализовано v3.149–v3.157. Архитектура: `cons` (enum/константы) ← `reg` (хук House +
> живой реестр зданий + интерьер-карта) ← `core` (бот) ← `map` (конфиг локаций + спавн).
> Данные карты генерируются офлайн (см. `docs/research/world-poi.md` и `map/Configs/`).

18. `[x]` **Рефактор: JSON-инфра в `reg`** — `core/3_Game/Config/{dmJsonFile,dmJsonConfigBase}`
    → `reg/3_Game/Config`; `reg/3_Game` в `config.cpp`. (v3.149)
19. `[x]` **Генерация данных карты** — офлайн-скрипт из `CfgWorlds Names` + `mapgrouppos.xml` +
    `mapgroupproto.xml` → `world_poi.json` (306 локаций + 868 ключевых POI: колодцы/полиция/
    пожарка/больница/военка/промзоны/заправки + C130J) и `buildings_interior.json` (422 класса
    с точками лута). (v3.150)
20. `[x]` **`dmWorldPOIType` + конфиг-структуры** — enum в `cons/3_Game`; `dmBuildingInteriorConfig`
    (`reg/3_Game/Config`), `dmWorldPoiConfig` + `dmSpawnConfig` (`map/3_Game/Config`); map-модуль
    в `config.cpp`. (v3.150–v3.152)
21. `[x]` **`reg/4_World`: хук + живой реестр + интерьер-карта** — `modded House` (CallLater),
    `dmLiveBuildingRegistry` (GetNearest по типу), `dmBuildingInteriorMap` (точки лута → мир).
    (v3.151)
22. `[x]` **`map/4_World`: `dmWorldPOIRegistry`** — поселения Capital/City/Village/Camp + query.
    (v3.153)
23. `[x]` **`map/5_Mission`: `dmBotSpawnManager`** — синглтон+тикер, спавн по квоте/override,
    кап MaxBots, респавн, loadout + пресет Nomad; тик из `MissionServer.OnUpdate`. (v3.154)
24. `[x]` **Кочевничество (`Travel`)** — `dmBotState_Travel` (PREEMPTIVE, жажда→ближайший колодец
    через живой реестр, EXIT по прибытии/угрозе) + `dmBotCondition_Thirsty` + `dmBotPreset_Nomad`.
    (v3.155)
25. `[x]` **Команды `/poi`, `/spawncity`** — сводка реестра + ручной спавн. (v3.156)

План/детали: `docs/plans/world-poi-spawn.md`. MVP-веха достигнута: по боту на поселение + кочёвка
«жажда → колодец». Осталось на будущее: голод/оружие → POI, «поселение исчерпано → следующее»,
динамические военные wrecks (события CE), пруды.

---

## Статус боевого блока (v3.36–v3.44)

Группа 4 (бой) доведена до стабильного состояния: закрыты краш
`GetChamberedCartridgeMagazineTypeName` (мультимузл), `Fire` velocity/unit, спам
`pending event already posted`, ванильный `DropBullet`-шум, NULL-краши
(`ForgetStaleTargets`/`Shooting.OnUpdate`) и десинк FSM `DoubleBarrel_Base` (B95
не перезаряжался — фикс `PopCartridgeFromChamber` + `RandomizeFSMState`). Детали —
`docs/research/combat.md` и `docs/techdebt.md` (секция H). Свежие баги тестов — в
`docs/techdebt.md`, секция I (по `bugreport.txt`).

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
