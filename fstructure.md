# Структура каталогов botorama

## Принцип

Каталоги повторяют script-модули DayZ (порядок загрузки: game → world → mission):

| Слой | Модуль в config.cpp | Назначение |
|------|---------------------|------------|
| `3_Game` | `gameScriptModule` | Код, нужный и серверу, и клиенту (логирование) |
| `4_World` | `worldScriptModule` | Сущности/логика мира (боты) |
| `5_Mission` | `missionScriptModule` | Миссия (MissionServer/MissionGameplay, чат-команды) |

Внутри слоя — подкаталоги:

- `cons/` — константы и дефайны: только `static const ...` и файлы-документация (без исполняемого кода).
- `core/` — исполняемый код каркаса/продукта (классы, `modded class`, сущности, движок команд).
- `test/` — тестовые команды/сценарии (модули чат-команд), отдельно от «продукта».

Внутри `core/<слой>` код группируется по функциональным папкам:

- `Entities/Bot/` — сущности бота (`dmAI*`).
- `Entities/Bot/FSM/` — ядро FSM (`dmBotFSM/State/Transition/Condition`) и `Conditions/`.
- `Entities/Bot/States/` — конкретные состояния FSM.
- `Entities/Bot/Presets/` — пресеты FSM (фабрики).
- `Entities/Bot/Pathfinding/` — обёртка над navmesh-API (`dmBotPathfinder`).
- `Commands/` — движок чат-команд (`dmCommandManager`/`dmCommandModule`).
- `Logging/` — логирование (`dmBotLog`).
- `Config/` — чтение/запись JSON-конфигов (`dmJsonFile`, `dmJsonConfigBase`).
- `Profiling/` — профайлер (`dmBotProfiler`, scope-guard + CSV-дамп).

## Дерево

```
botorama/
├── Animations/                  # кастомный animation graph (.agr, текстовый)
│   ├── player_main.agr
│   ├── Locomotion.agr
│   ├── Actions.agr
│   └── Tests.agr
├── config.cpp                   # CfgPatches / CfgMods / CfgVehicles (defines, modules)
├── fstructure.md                # этот файл — принцип размещения
├── cons/                        # константы и дефайны
│   ├── 3_Game/
│   │   └── constants.c          # DM_BOTORAMA_VERSION (версия мода)
│   ├── 4_World/
│   │   ├── constants.c          # DM_* константы мира (модель, спавн, look)
│   │   └── defines.c            # DM_BOT_DEBUG / DM_BOT_TRACE (документация)
│   └── 5_Mission/
│       └── constants.c          # DM_CHAT_* (чат-команды)
└── core/                        # исполняемый код
    ├── 3_Game/
    │   ├── Logging/
    │   │   └── dmBotLog.c       # dmBotLog.Debug/Trace/LogVersion
    │   └── Config/              # JSON-конфиги (переиспользуемые, без привязки к ботам)
    │       ├── dmJsonFile.c     # generic reader/writer + версионирование
    │       └── dmJsonConfigBase.c # база для версионируемых конфиг-структур
    │   └── Profiling/           # профайлер (scope-guard + CSV-дамп)
    │       └── dmBotProfiler.c  # dmBotProfiler/dmBotSpan/dmProfEntry
    ├── 4_World/
    │   └── Entities/
    │       └── Bot/
    │           ├── dmAISurvivor.c      # контроллер (мозг бота)
    │           ├── dmAISurvivorBase.c  # пешка (PlayerBase + анимации)
    │           ├── dmTarget.c          # цель бота (ACQUIRE/DESTROY, память)
    │           ├── FSM/                # ядро FSM
    │           │   ├── dmBotFSM.c
    │           │   ├── dmBotState.c
    │           │   ├── dmBotTransition.c
    │           │   ├── dmBotCondition.c
    │           │   └── Conditions/     # условия (предикаты)
    │           ├── Intent/             # намерения (пул + арбитраж)
    │           │   ├── dmBotIntent.c
    │           │   ├── dmBotIntentPool.c
    │           │   └── dmBotIntent_*.c # HoldLook/LookAround/MoveTo/Glance/Turn
    │           ├── States/             # состояния (dmBotState_*)
    │           ├── Pathfinding/        # обёртка над navmesh-API (dmBotPathfinder)
    │           └── Presets/            # пресеты (dmBotPreset_*)
    └── 5_Mission/
        ├── MissionServer.c      # сервер: OnInit (регистрация команд), OnEvent, тикер
        ├── MissionGameplay.c    # клиент: OnInit (LogVersion)
        └── Commands/            # движок чат-команд
            ├── dmCommandManager.c   # register + delegate + утилиты
            └── dmCommandModule.c    # базовый модуль команды
test/                       # тестовые команды/сценарии (не «продукт»)
    └── 5_Mission/
        ├── dmCommandContext.c  # общее состояние + доменные хелперы
        ├── dmBotCommand.c      # "/bot ..." (spawn/intent/patrol/speed/status/setX)
        ├── dmFSMCommand.c      # "/fsm ..." (new/add/apply)
        ├── dmTestCommand.c     # "/test ..." (сценарии)
        ├── dmBotTest.c         # самопроверяемые тесты тела (base + runner + shock/stamina/brokenleg/death)
        └── dmProfCommand.c     # "/prof ..." (dump/clear/start/stop)
```

## Правила размещения

- Классы бота (`dmAI*`) → `core/4_World/Entities/Bot/`.
- Ядро FSM (`dmBotFSM/State/Transition/Condition`) → `core/4_World/Entities/Bot/FSM/`.
- Условия (`dmBotCondition_*`) → `core/4_World/Entities/Bot/FSM/Conditions/`.
- Намерения (`dmBotIntent`/`dmBotIntentPool`/`dmBotIntent_*`) → `core/4_World/Entities/Bot/Intent/`.
- Состояния (`dmBotState_*`) → `core/4_World/Entities/Bot/States/`.
- Пресеты (`dmBotPreset_*`) → `core/4_World/Entities/Bot/Presets/`.
- Pathfinding (`dmBotPathfinder`) → `core/4_World/Entities/Bot/Pathfinding/`.
- Логирование (`dmBotLog`) → `core/3_Game/Logging/` (нужно и серверу, и клиенту).
- Читатель JSON-конфигов (`dmJsonFile`/`dmJsonConfigBase`) → `core/3_Game/Config/` (переиспользуемый, не привязан к ботам).
- Профайлер (`dmBotProfiler`) → `core/3_Game/Profiling/` (нужен серверу; грузится до world/mission, откуда он инструментируется).
- Константы → `cons/<слой>/constants.c`.
- Версия мода (`DM_BOTORAMA_VERSION`) → `cons/3_Game/constants.c` (используется из 3_Game; модуль грузится первым).
- Дефайны логирования → `cons/4_World/defines.c`.
- Миссия (`MissionServer`/`MissionGameplay`) → `core/5_Mission/`.
- Движок чат-команд (`dmCommandManager`/`dmCommandModule`) → `core/5_Mission/Commands/`.
- Тестовые команды/сценарии (`dmBotCommand`/`dmFSMCommand`/`dmTestCommand`, `dmCommandContext`) → `test/5_Mission/`.
- Команда профайлера (`dmProfCommand`) → `test/5_Mission/`.
- Графы анимаций → `Animations/`.

При добавлении нового файла: кладём в подходящую функциональную папку своего слоя;
новую функциональную область — в новую папку (`Entities/X`, `Logging`, `Commands`, …),
а не в корень слоя.

## Логирование

- `dmBotLog.Debug/Trace` всегда печатают. Отсечение — `#ifdef` **на месте вызова**,
  по доменным дефайнам (`DM_BOT_DEBUG_SPAWN/BRAIN/FSM/PAWN`, `DM_BOT_TRACE_LOOK` —
  полный список в `cons/4_World/defines.c`):

  ```
  #ifdef DM_BOT_DEBUG_FSM
  dmBotLog.Debug("...");
  #endif
  ```

  Enfusion не оптимизирует пустой вызов (в отличие от C++), а дорогая часть — это
  конкатенация строк в аргументах, поэтому `#ifdef` вокруг самого вызова вырезает и
  вызов, и конкатенацию, когда дефайн выключен. Включаются только нужные домены —
  через `defines[]` в `config.cpp`.
- `dmBotLog.Error` не гейтится — ошибки видны всегда.
- `dmBotLog.LogVersion()` не гейтится — версию видно всегда (сервер + клиент).

## Профилирование

- Профайлер: `core/3_Game/Profiling/dmBotProfiler.c` — scope-guard `dmBotSpan`
  (деструктор пишет время в аккумулятор) + CSV-дамп. Точки замера гейтятся
  `#ifdef DM_BOT_PROFILE` (по умолчанию включён в `config.cpp`).
- Команды: `/prof start|stop` — вкл/выкл накопление (по умолчанию включено),
  `/prof clear` — сброс, `/prof dump` — CSV в `$profile:dmBotorama/profile/`.
- Чистый замер сценария:
  `/prof stop` → `/prof clear` → `/prof start` → `<сценарий>` → `/prof stop` → `/prof dump`.
- **Правило разработки**: любая функция, которая может повлиять на
  производительность, должна содержать вставку замера (`dmBotSpan` под
  `#ifdef DM_BOT_PROFILE`), чтобы её влияние учитывалось в профиле:

  ```
  void SomeHeavy(float pDt)
  {
      #ifdef DM_BOT_PROFILE
      dmBotSpan _span = dmBotProfiler.Start("SomeHeavy");
      #endif
      ...
  }
  ```

- Бейзлайн (100 ботов, 30 Гц, логирование выключено): мозг (`Tick`) ~1.8 мс/тик
  (~5.5% ядра), пешка (`CommandHandler`) ~58 μs/тик (~18% ядра, почти целиком
  ванильный `super.CommandHandler`). Внутри мозга ~56% — арбитраж намерений
  (`Intents` ~9 μs). Мозг уже дешевле ванильной симуляции пешки.
- Частота тика мозга: `DM_BOT_TICK_INTERVAL` (0.033 = 30 Гц), привязана к sim-rate
  пешки (`Bot.Update`/`CommandHandler` ≈ 1.0).
- Оговорка: `GetTickTime()` квантуется ~1 мс — средние по большому числу вызовов
  корректны, единичные значения < 1 мс — лишь оценка.

## Симуляция тела (честные пределы)

- Пешка (`dmAISurvivorBase`) — полноценный `PlayerBase`: все системы тела (здоровье,
  кровь, шок, стамина, температура, токсичность, переломы, болезни) считает
  ванильный DayZ. Мы их не переопределяем — мы лишь перестали их «перебивать».
- **Ванильный тик систем тела** (`PlayerBase.OnScheduledTick`) гейтится на
  `IsPlayerSelected()` (всегда `false` у AI-бота) и на `m_AllowModifierTick`
  (включается только в `OnSelectPlayer`). Поэтому модификаторы сами не тикают.
  Мы в `CommandHandler` делаем `SetModifiers(true)` и тикаем
  `GetModifiersManager().OnScheduledTick(dt)` **по пониженной частоте**
  (`DM_BOT_MODIFIER_TICK_INTERVAL` = 0.25 c, аккумулятор) — внутренние интервалы
  модификаторов ≥ 0.35 c, чаще не нужно.
- **Нокаут (мост команды)**: синк-джанктура `UnconsciousnessMdfr → SendSyncJuncture`
  до server-only бота не доходит, а ванильный блок, который стартует команду, гейтится
  на `m_ActionManager` (у `INSTANCETYPE_AI_SERVER` он `NULL`). Поэтому `m_ShouldBeUnconscious`
  ведём сами из `GetHealth("","Shock")` и **напрямую** зовём
  `StartCommand_Unconscious(0)` / `hcu.WakeUp(PRONE)` (граф анимаций поддерживает
  `CMD_Unconscious`). Пороги — `PlayerConstants.UNCONSCIOUS_THRESHOLD` / `CONSCIOUS_THRESHOLD`.
- **Перелом**: `SetBrokenLegs(-eBrokenLegs.BROKEN_LEGS)` (отрицательное = первичная
  активация) ставит состояние сразу; `ActivateModifier(MDF_BROKEN_LEGS)` (через наш
  тик) применяет инжури-анимацию (хромоту) и `BrokenLegWalkShock` (шок от бега со
  сломанной ногой, без шины → в итоге нокаут).
- Гейт честности: `dmAISurvivorBase.CanAct()` = `IsAlive() && !IsUnconscious() && !IsRestrained()`.
  - `CommandHandler` применяет `ApplyLookVars`/`ApplyBodyTurn`/`ApplyMovement`/`ApplyStance`
    только при `CanAct()`, иначе вызывает `ResetActuation()` (останов поворота/движения,
    голова в нейтраль).
  - Мозг (`dmAISurvivor.OnUpdate`): при смерти — `Despawn()` (бот+мозг удаляются из мира);
    при бессознательном/связанном состоянии — пропуск моторики (FSM/интенты/взгляд).
- **Кап скорости (стамина + перелом)**: ванильный лимит спринта
  `hic.LimitsDisableSprint` обходится нашим `OverrideMovementSpeed`, поэтому в
  `ApplyMovement` капаем `target` до `DM_SPEED_IDX_JOG` (=2), если
  `!(CanConsumeStamina(SPRINT) && CanSprint())` (`CanSprint()` уже включает
  `GetBrokenLegs()`). Джог оставляем разрешённым намеренно: ванильный
  `BrokenLegWalkShock` бьёт шоком именно на джоге/спринте со сломанной ногой,
  что в итоге вырубает бота.
- Стамина — ванильная (`StaminaHandler`): вес в инвентаре режет кап, спринт честно
  её расходует, при нуле — форс на джог, реген. Мы её не обходим.
- Отладка: `/bot status` (полный отчёт о теле+мозге) и
  `/bot sethealth|setblood|setshock|setstamina|setheatbuffer|settoxicity|setenergy|setwater <число>`
  (каждая принимает обязательный float, без дефолтов).
- Самопроверяемые сценарии (`/test bot shock|stamina|brokenleg|death`): печатают
  ожидаемый результат и сами сверяют состояние бота по таймеру (`dmBotTest.c`);
  `/test cancel` — прервать работающий тест и удалить его бота.
- Отложено — см. `TECHDEBT.md` (кровотечение, холод/жара, токсичность, утопление,
  ослепление; реакции мозга на состояние тела).

## План развития «человечивание бота» — статус

**Выполнено** (честная симуляция тела, см. «Симуляция тела» выше):
- Шок → нокаут: падает, лежит (не двигается даже под `MoveTo`), приходит в себя.
- Стамина: вес в инвентаре режет кап, спринт честно тратит, при нуле — джог.
- Перелом: хромота (инжури-анимация) + шок от бега → нокаут (без шины).
- Смерть: детект + удаление бота и мозга из мира.
- Единый кап скорости, ручной тик модификаторов по пониженной частоте.
- Инструменты: `/bot status`, `/bot set*`, самопроверяемые `/test bot shock|stamina|brokenleg|death`, `/test cancel`.

**Отложено / осталось** (см. `TECHDEBT.md`):
- Тесты тела со сложной индукцией: кровотечение, холод/жара, токсичность, утопление, ослепление.
- Реакции мозга на состояние тела (искать тепло/еду/воду, отдых, бой/бегство).
- Голод/жажда/болезни (статы уже тикают, но мозг пока не ест/не пьёт).
- Сенсорика (ослепление и т.п.).
