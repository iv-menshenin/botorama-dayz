Создание модели бота, базовые движения (спавн, синхронизация, поворот головы, поворот тела)
================================================================================

> Scope: только Stage 1 (спавн/синхронизация/голова/тело). FSM, интенты, цели и
> движение к точке — в `Reference/fsm-implementation-plan.md`.

## Архитектура
- Проект: botorama (префикс dm). Client-server мод DayZ.
- Паттерн: контроллер `dmAISurvivor` (обычный class, не Managed → поля через `ref`) + пешка `dmAISurvivorBase : PlayerBase`.
- Глобальный список ботов — статические поля `dmAISurvivor.s_All` / `s_ByPawn` (массив `array<ref dmAISurvivor>`, мап `map<PlayerBase, ref dmAISurvivor>`).
- Пешка создаётся `GetGame().CreatePlayer(null, "dmAI_SurvivorM_Denis", pos, 0, "NONE")` → `INSTANCETYPE_AI_SERVER`. Модель наследуется из конфига (`dmAI_SurvivorM_Denis : SurvivorM_Denis`).
- `ref` обязателен для наших не-Managed классов в контейнерах/полях; у Managed (PlayerBase/EntityAI) — нет.

## Синхронизация сервер→клиент
- `RegisterNetSyncVariableFloat("m_X", min, max, precision)` в конструкторе.
- На сервере при изменении: `SetSynchDirty()`.
- На клиенте: `override void OnVariablesSynchronized()` под `#ifndef SERVER`.
- `SetSynchDirty()` БЕЗ изменения значения — не работает (нужно менять поле).

## Поворот головы (работает)
- Голова крутится через граф-переменные. Ванильные `Look`/`LookDirX`/`LookDirY` перетираются нативным "look at" (для ИИ ставит в 0) — их НЕЛЬЗЯ использовать.
- Решение: кастомные переменные `dmAI_Look`/`dmAI_LookDirX`/`dmAI_LookDirY`, добавленные в СВОЙ граф.
- `AnimSetFloat` (переменные) синхронизируется на клиент; `AnimCallCommand` (команда) — срабатывает на сервере и триггерит ПЕРЕХОД состояния, а на клиент уезжает уже итог (состояние), так что команду использовать можно (см. секцию про Expansion).
- Pitch из `VectorToAngles()[1]` в диапазоне [0,360) — нормализовать: `if (pitch > 180) pitch -= 360;`.

## Кастомный animation graph
- `.agr` — ТЕКСТОВЫЙ формат, игра читает его напрямую (бинарные только `.abt`/`.asi`). Редактор не нужен.
- Скопированы `player_main.agr` + `locomotion/actions/tests.agr` в `botorama/Animations/`.
- Правило: НЕ ПЕРЕИМЕНОВЫВАТЬ ванильные переменные/команды (нативный код обращается к ним по ХЕШУ имени → краш "doesn't have variable NNN").
- Вместо переименования: КЕПИТЬ ванильные (для хеша) + ДОБАВИТЬ кастомные + переименовать только ИСПОЛЬЗОВАНИЯ в подграфах.
- Подключение: `config.cpp` → `class enfAnimSys : enfAnimSys { graphName = "botorama\Animations\player_main.agr"; }` + `requiredAddons = {"DZ_Characters","DZ_Anims_Anm_Player","DZ_Anims_Cfg"}`.
- В `$Files` подграфы указываются своим путём (`botorama/Animations/Locomotion.agr`), остальные — ванильным.

## Важные факты про сервер/клиент (хуки)
- Для AI_REMOTE (клиент) ВСЁ, что относится к анимации, бежит ТОЛЬКО на сервере: `CommandHandler`, `HeadingModel`, `AimingModel`, `HumanCommandScript`. Клиент лишь проигрывает синхронизированную анимацию.
- Единственный клиентский хук — `OnVariablesSynchronized` (сеть). `AnimSet*` оттуда НЕ работают ("usable only from CommandHandler").
- Поэтому всё анимационное управление делаем на сервере в `CommandHandler`, а на клиент уезжает результат (переменные).

## Поворот тела (переступание)
- `SetOrientation()` — мгновенный поворот, "скользит" без шагов (это `HeadingModel::RotateOrient` — ванильный "turn slide").
- Правильное переступание = состояние "Turn" в графе, ВХОД по команде (`IsCommand(CMD_Turn)`) + переменная `TurnAmount` для бленда.
- `TurnAmount` в графе играет ДВЕ роли: бленд-ноды `*TurnNBlend` ЧИТАЮТ его (направление/величина поворота), а `AnimNodeVarUpdate`-нода `TurnVar` ПИШЕТ в него (фактический поворот из рут-моушена — нативный код читает его и крутит тело).
- Поэтому НЕЛЬЗЯ просто переименовать `TurnAmount` везде: `TurnVar` должен по-прежнему писать в ванильный `TurnAmount` (чтобы нативный код крутил), а на кастомную переменную (`dmAI_TurnAmount`) переводят только бленд-ноды `*TurnNBlend`.
- Вход в состояние "Turn" делаем по КАСТОМНОЙ команде (`IsCommand(dmAI_Turn)` / `IsCommand(dmAI_StopTurn)`), чтобы не конфликтовать с ванильным `CMD_Turn`, который ставит нативный movement.

## Ошибки (как НЕ делать / что НЕ работает)
- Переименовывать ванильные граф-переменные — краш по хешу.
- `OverrideAimChangeX/Y` (HumanInputController) — не двигает голову ИИ (это клиентский путь мыши).
- `HeadingModel`/`AimingModel` для головы/тела — сервер-онли, голову не крутят.
- `AnimSet*` из `OnVariablesSynchronized` — игнорируется.
- `AnimCallCommand` (граф-команда) на сервере — работает ТОЛЬКО если переход в графе тоже по этой команде (`IsCommand(...)`); просто дёрнуть ванильный `CMD_Turn` с пустым переходом/неверным параметром — эффекта нет.
- `HumanCommandScript` — исполняется только на сервере и подменяет MOVE-команду (ломает локомоцию).

## Что известно про Expansion (референсный механизм переступания)
- Их `eAICommandMove` (кастомный movement-команд) в `PreAnimUpdate` при `speed == 0` (идл) и `|turnDifference| > 1°`:
  - `m_Table.CallTurn(m_Unit)` = `AnimCallCommand(CMD_eAI_Turn)` (КАСТОМНАЯ команда) — вход в "Turn".
  - `m_Table.SetTurnAmount(m_Unit, m_TurnDifference / 90.0)` = `AnimSetFloat(eAI_TurnAmount, ...)` — бленд.
  - НИКАКОГО `SetOrientation` в этот момент — тело крутит сам рут-моушен анимации "Turn".
- Когда `|turnDifference| < 1°` или прошло 2с: `CallStopTurn` = `AnimCallCommand(CMD_eAI_StopTurn)`.
- `SetOrientation(плавно)` (`Anim_SetFilteredHeading`) Expansion использует ТОЛЬКО когда движется (speed > 0), а не в "Turn".
- Значение: `m_TurnDifference / 90.0` (градусы → -2..2).
- Схема: КАСТОМНЫЕ `CMD_eAI_Turn`/`CMD_eAI_StopTurn`/`eAI_TurnAmount` (команда входит в состояние, переменная крутит бленд), ванильный `TurnAmount` остаётся как выход `TurnVar` для нативного кода.

## Текущее состояние (v2.3 — ПОДТВЕРЖДЕНО)
- Спавн, синхронизация, поворот головы (`dmAI_Look*`) — работают.
- Переступание ногами — РАБОТАЕТ и выглядит реалистично: бот поворачивается, переступая ногами, именно когда это нужно. Схема Expansion дала результат.
- Механизм: в `ApplyBodyTurn` (после `super.CommandHandler()`) при идле и `|dBody| > 1°` — `AnimSetFloat(dmAI_TurnAmount, dBody/90)` + `AnimCallCommand(dmAI_Turn)`, при завершении — `AnimCallCommand(dmAI_StopTurn)`; `SetOrientation` убран.
- Граф: переход "Idles"↔"TurnMaster" по `IsCommand(dmAI_Turn)`/`IsCommand(dmAI_StopTurn)`; `TurnVar` пишет ванильный `TurnAmount`; бленды `*TurnNBlend` читают `dmAI_TurnAmount`.
- Валидация по логам: `Botorama initialized: 2.3`; `BindLookVars() Look=6 LookDirX=10 LookDirY=8 TurnAmount=4 CmdTurn=63 CmdStopTurn=65 instType=3`; ошибок графа нет.

## Ключевые файлы
- `botorama/core/4_World/dmAISurvivorBase.c` — пешка; `BindLookVars`, `ApplyLookVars`, `ApplyBodyTurn`, `CommandHandler`, sync-переменные.
- `botorama/core/4_World/dmAISurvivor.c` — контроллер; `Spawn`, `LookAtPoint`, `UpdateLook`, `SetTargetBodyYaw`.
- `botorama/Animations/` — кастомный граф (`.agr`) с `dmAI_Look*` + `dmAI_TurnAmount` + `dmAI_Turn`/`dmAI_StopTurn`.
- `botorama/config.cpp` — `CfgVehicles` c `enfAnimSys` `graphName`.
- `botorama/cons/4_World/constants.c` — `DM_BOTORAMA_VERSION`, константы look/turn.
- `botorama/core/5_Mission/MissionServer.c` / `MissionGameplay.c` — чат-команда `/bot spawn test` + тикер + лог версии.
