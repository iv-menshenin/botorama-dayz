# Структура каталогов botorama

## Принцип

Каталоги повторяют script-модули DayZ (порядок загрузки: game → world → mission):

| Слой | Модуль в config.cpp | Назначение |
|------|---------------------|------------|
| `3_Game` | `gameScriptModule` | Код, нужный и серверу, и клиенту (логирование) |
| `4_World` | `worldScriptModule` | Сущности/логика мира (боты) |
| `5_Mission` | `missionScriptModule` | Миссия (MissionServer/MissionGameplay, чат-команды) |

Внутри слоя — два подкаталога:

- `cons/` — константы и дефайны: только `static const ...` и файлы-документация (без исполняемого кода).
- `core/` — исполняемый код (классы, `modded class`, сущности).

Внутри `core/<слой>` код группируется по функциональным папкам:

- `Entities/Bot/` — сущности бота (`dmAI*`).
- `Entities/Bot/FSM/` — ядро FSM (`dmBotFSM/State/Transition/Condition`) и `Conditions/`.
- `Entities/Bot/States/` — конкретные состояния FSM.
- `Entities/Bot/Presets/` — пресеты FSM (фабрики).
- `Logging/` — логирование (`dmBotLog`).
- `Config/` — чтение/запись JSON-конфигов (`dmJsonFile`, `dmJsonConfigBase`).

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
    │           └── Presets/            # пресеты (dmBotPreset_*)
    └── 5_Mission/
        ├── MissionServer.c      # сервер: OnInit, OnEvent, чат-команды, тикер
        └── MissionGameplay.c    # клиент: OnInit (LogVersion)
```

## Правила размещения

- Классы бота (`dmAI*`) → `core/4_World/Entities/Bot/`.
- Ядро FSM (`dmBotFSM/State/Transition/Condition`) → `core/4_World/Entities/Bot/FSM/`.
- Условия (`dmBotCondition_*`) → `core/4_World/Entities/Bot/FSM/Conditions/`.
- Намерения (`dmBotIntent`/`dmBotIntentPool`/`dmBotIntent_*`) → `core/4_World/Entities/Bot/Intent/`.
- Состояния (`dmBotState_*`) → `core/4_World/Entities/Bot/States/`.
- Пресеты (`dmBotPreset_*`) → `core/4_World/Entities/Bot/Presets/`.
- Логирование (`dmBotLog`) → `core/3_Game/Logging/` (нужно и серверу, и клиенту).
- Читатель JSON-конфигов (`dmJsonFile`/`dmJsonConfigBase`) → `core/3_Game/Config/` (переиспользуемый, не привязан к ботам).
- Константы → `cons/<слой>/constants.c`.
- Версия мода (`DM_BOTORAMA_VERSION`) → `cons/3_Game/constants.c` (используется из 3_Game; модуль грузится первым).
- Дефайны логирования → `cons/4_World/defines.c`.
- Миссия (`MissionServer`/`MissionGameplay`) → `core/5_Mission/`.
- Графы анимаций → `Animations/`.

При добавлении нового файла: кладём в подходящую функциональную папку своего слоя;
новую функциональную область — в новую папку (`Entities/X`, `Logging`, `Commands`, …),
а не в корень слоя.

## Логирование

- `dmBotLog.Debug/Trace` всегда печатают. Отсечение — `#ifdef` **на месте вызова**:

  ```
  #ifdef DM_BOT_DEBUG
  dmBotLog.Debug("...");
  #endif
  ```

  Enfusion не оптимизирует пустой вызов (в отличие от C++), а дорогая часть — это
  конкатенация строк в аргументах, поэтому `#ifdef` вокруг самого вызова вырезает и
  вызов, и конкатенацию, когда дефайн выключен.
- `dmBotLog.LogVersion()` не гейтится — версию видно всегда (сервер + клиент).
