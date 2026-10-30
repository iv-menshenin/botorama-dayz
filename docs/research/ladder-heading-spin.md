# Research: спин корпуса после спуска с лестницы

Статус: диагностика нативного остатка heading после `HumanCommandLadder` (спуск).
Ведёт субагент `dayz-research`. Смежно: `navigation.md` (ladder-navigation, точки
`_con`/`_con_dir`, `StartCommand_Ladder`, `CanExit/Exit` — там уже задокументировано,
здесь НЕ дублируем).

## Симптом (уже установлено эмпирически, E2E)

Бот (`INSTANCETYPE_AI_SERVER`, без оружия) после **спуска** (`dir=-1`) равномерно
крутится. Сырой `m_fHeadingAngle` (`SDayZPlayerHeadingModel`, `3_game/dayzplayer.c:1084-1094`):
- Подъём `dir=1`: `heading == orientation` (чисто).
- Спуск `dir=-1`: `heading == orientation + 162.26°` (= 2.83228 рад). Сдвиг вносит сам
  нативный `HumanCommandLadder`, даже если кормление движения убрано (бот не лезет, сдвиг есть).
- После схода `m_fHeadingAngle` накручивается ~+162.28°/кадр (линейно); `m_fOrientationAngle`
  (тело) заморожено, либо догоняет через ванильный `RotateOrient` (прыжки ~50° при зазоре >171°).
- `162.26° ≈ 180° − 17.74°` — «незавершённая нативная фаза спуска».

## Q1. Как нативный `HumanCommandLadder` управляет heading/orientation

**Факты:**

- `HumanCommandLadder` — **целиком нативный** (`3_game/human.c:644-664`), из скрипта
  видны только `CanExit()` (`:650`), `Exit()` (`:653`), `DebugDrawLadder` (`:655`),
  `DebugGetLadderIndex` (`:658`), `GetLogoutPosition()` (`:661`). Как он двигает
  `m_fHeadingAngle`/`m_fOrientationAngle` на подъёме/спуске — **в скрипте НЕ видно**.
  Вся логика поворота на спуске — внутри C++ движка.

- Скриптовый `HeadingModel` на лестнице **ничего не трогает**:
  `4_world/entities/dayzplayerimplement.c:1629-1633` — `COMMANDID_LADDER` →
  `m_fLastHeadingDiff = 0; return false;` (не трогает ни heading, ни orientation).
  Expansion — то же самое (`eAIBase.c:10042-10045`, `COMMANDID_CLIMB`/`COMMANDID_LADDER` →
  `m_fLastHeadingDiff = 0; return false;`).

- **Признак направления спуска в скрипте есть** — это индекс движения `m_iMovement`,
  а не отдельный флаг dir. `ProcessHandDamage` (`4_world/entities/manbase/playerbase.c:1850-1854`)
  ловит «скольжение вниз» по `pState.m_iMovement == DayZPlayerConstants.MOVEMENTIDX_SLIDE`
  (= **-2**, единственный отрицательный индекс, `3_game/dayzplayer.c:656`). Подъём — положительные
  индексы (0/1/2). Т.е. **направление спуска = знак `m_iMovement`** (отрицательный = вниз).

- **Направление лезть вверх/вниз кормится через движение**, а не вычисляется нативом из
  геометрии: Expansion `eAICommandMove.c:1707-1720` — если `m_eAI_LadderClimbDirection < 0`
  (спуск) → `m_MovementDirection = -180.0` (движение НАЗАД относительно корпуса), иначе
  (подъём) → `m_MovementDirection = 0` (вперёд). Это подтверждает: нативный `HumanCommandLadder`
  читает направление движения из инпут-контроллера, как ванильный игрок (W=вверх, S=вниз).

- Геометрия лестницы (`ladder*_con` / `ladder*_con_dir`, `Building.GetLadderPosTop/Bottom`
  `3_game/entities/building.c:13-14`) используется **только для выбора точки входа/выхода**
  (`interact/actionenterladder.c:58,82`; наш `dmBotLadderCache.c`), а **НЕ для вычисления
  угла heading**. Скриптового `PI - ladderAngle` / `atan2` по геометрии лестницы НЕТ
  (grep по `atan2`/`PI` в ladder-контексте — пусто).

**Вывод Q1:** конкретный угол ~162.26° — нативный внутренний остаток фазы спуска
(`m_iMovement == MOVEMENTIDX_SLIDE`), в скрипте не вычисляется и не виден. Откуда именно
берётся — из C++ движка. Структурно это «heading развёрнут относительно orientation на ~180°
(спуск = лицом наружу/вниз) минус ~17.74° (наклон лестницы)».

## Q2. Что накручивает heading ~+162°/кадр после схода

**Факты:**

- **Ванильный переход после лестницы — это FALL.** В `DayZPlayerImplement.CommandHandler`
  обработка `pCurrentCommandFinished` (`4_world/entities/dayzplayerimplement.c:2368-2397`):
  при завершении команды `if (PhysicsIsFalling(true)) { StartCommand_Fall(0); ... }`
  (`:2384-2388`), иначе `StartCommand_Move()`. Т.е. **спуск с лестницы → сход с верхней/нижней
  точки → `COMMANDID_FALL` → затем `COMMANDID_MOVE`**. Скриптовый хук `OnCommandLadderFinish()`
  (`playerbase.c:3991-3999`) — это про скрытие предмета/инвентарь, не про угол.

- **Расширение прямо называет этот баг.** `eAIBase.c` HeadingModel, ветка
  `COMMANDID_MELEE/MELEE2/FALL` (`eAIBase.c:10093-10101`):
  ```c
  //! Needed to prevent AI from spinning in place after going down ladder
  case DayZPlayerConstants.COMMANDID_MELEE:
  case DayZPlayerConstants.COMMANDID_MELEE2:
  case DayZPlayerConstants.COMMANDID_FALL:
      ...
      pModel.m_fOrientationAngle = pModel.m_fHeadingAngle;
      SetOrientation(Vector(pModel.m_fHeadingAngle * Math.RAD2DEG, 0, 0));
      m_fLastHeadingDiff = 0;
      return false;
  ```
  Та же пометка «after going down ladder» — в ветке `COMMANDID_SWIM` при `m_iMovement <= 0`
  (`eAIBase.c:10087-10089`). Т.е. **спин после спуска — это зазор heading↔orientation, который
  переживает переход LADDER→FALL**, и Expansion его гасит снапом `orientation = heading`.

- **Почему ванильный `RotateOrient` НЕ догоняет сам.** `DayZPlayerImplementHeading.RotateOrient`
  (`4_world/entities/dayzplayerimplementheading.c:67-120`): `CONST_ROTLIMIT = Math.PI * 0.95`
  (`:64`) = **171°**. Он двигает `m_fOrientationAngle` к heading ТОЛЬКО когда зазор
  `|aDiff| > 171°` (`:92-115`), причём прыжком на `aDiff - CONST_ROTLIMIT` (`:101`/`:113`).
  Зазор 162.26° < 171° → `RotateOrient` возвращает `false` (тело **заморожено**). Когда
  heading накрутится так, что зазор перевалит 171°, тело прыгает на избыток (~50°) — ровно
  как в наблюдении. `m_fLastHeadingDiff` при этом **обнулился ещё на лестнице**
  (`dayzplayerimplement.c:1631`), поэтому «стuck-развёртки» (`:82-89`) не срабатывают —
  раскрутка зазора идёт от 0.

- **Что именно прибавляет ~162°/кадр к heading — в скрипте НЕ видно** (нативный остаток).
  Скриптовый `HeadingModel`/`RotateOrient` heading **не** прибавляют (только orientation).
  `m_fHeadingAngle` (aim) для AI пишется нативной командой (move/fall) каждый кадр из
  направления инпут-контроллера. Гипотеза: незавершённый нативный turn фазы спуска (или
  FALL-перехода) продолжает доворачивать aim, потому что мы выходим по `!IsClimbingLadder()`
  без явного `hcl.Exit()` — нативный командный конечный автомат не закрыт.

**Вывод Q2:** накрутка heading — нативный остаток (скрипт не прибавляет heading). Механика
замораживания тела — подтверждена (`RotateOrient` молчит при зазоре <171°). Точный источник
прибавки ~162°/кадр — [нужно подтвердить] эмпирически (см. пробу ниже).

## Q3. Как проходит Expansion и почему у них нет спина

**Факты (eAIBase.c / eAICommandMove.c):**

1. **Детект завершения — по `pCurrentCommandFinished`, а НЕ `!IsClimbingLadder()`.**
   `eAIBase.c:7316-7325`: в `CommandHandler` при `pCurrentCommandFinished && m_eAI_IsOnLadder`
   → сброс `m_eAI_IsOnLadder=false; m_eAI_Ladder=null; m_eAI_PreferLadder=false; m_eAI_LadderLoops=0;`.
   Плюс override `OnCommandLadderFinish()` (`eAIBase.c:4433-4437`) → `super` + `ForceRecalculate(true)`.
   Отдельно FALL-ветка (`:7387-7391`) тоже сбрасывает `m_eAI_Ladder`/`m_eAI_IsOnLadder`.

2. **Явный выход `hcl.Exit()`.** `eAIBase.c:7500-7510`:
   ```c
   if (m_eAI_IsOnLadder) {
       HumanCommandLadder hcl = GetCommand_Ladder();
       if (!hcl) { m_eAI_IsOnLadder = false; ... }
       else if (hcl.CanExit() && m_eAI_LadderTime > 2.0) { hcl.Exit(); }
   }
   ```
   (`m_eAI_LadderTime` копится только в `COMMANDID_LADDER`, `eAIBase.c:7333-7335`.)

3. **HeadingModel: полный снап в MOVE, снап-обратно в FALL/MELEE/SWIM.**
   - `COMMANDID_MOVE` (`eAIBase.c:10047-10065`): `angle = GetOrientation()[0]*DEG2RAD;
     heading = angle; orientation = angle; return true;` — heading ЖЁСТКО равен физической
     ориентации, нативный move-команде НЕ дают самостоятельно крутить aim.
   - `COMMANDID_FALL`/`MELEE`/`MELEE2`/`SWIM`-назад (`:10093-10101`): `orientation = heading`
     + `SetOrientation(...)` — гасит спин после лестницы.
   - `COMMANDID_LADDER`/`CLIMB` (`:10042-10045`): `return false` (как ваниль).

4. **Исторический hack-фикс.** В обоих местах (finish `:7322-7323`, FALL `:7389-7390`)
   закомментирован `//m_eAI_Recreate = true;` с пометкой «Hack fix for vanilla bug with spinning
   while falling (each time) after climbing ladder». Т.е. раньше Expansion просто ПЕРЕСОЗДАВАЛ
   AI-юзера, чтобы сбросить нативный остаток; теперь обходится снапом в HeadingModel.

**Отличие от нашего подхода:**
- Мы завершаем интент по `!IsClimbingLadder()` (`dmBotIntent_UseLadder.c:164-171`) ДО ветки
  `CanExit()→Exit()` (`:175-186`) — поэтому ветка `Exit()` **не срабатывает** (после смены
  типа команды `IsClimbingLadder()` уже false). Расширение сначала ждёт `CanExit()` и явно
  `hcl.Exit()`, а финиш ловит по `pCurrentCommandFinished`/`OnCommandLadderFinish`.
- Мы не снапаем heading в `COMMANDID_MOVE` и не гасим зазор в `COMMANDID_FALL` — наш override
  (`dmAISurvivorBase.c:638-676`) снапает только на лестнице + 2 с grace, дальше отдаёт в
  ванильный `super.HeadingModel` (для MOVE/FALL → `RotateOrient`, который при зазоре <171°
  молчит). Grace `DM_HEADING_GRACE_AFTER_LADDER = 2.0` (`cons/4_World/constants.c:319`) — не
  достаточен: после него зазор от нативного остатка всё ещё живёт.
- Наш `ApplyMovement` (`dmAISurvivorBase.c:1244`) НЕ гейтится `onLadder` — на лестнице мы
  продолжаем писать `OverrideMovementAngle/Speed`. Плюс текущий эксперимент
  (`dmBotIntent_UseLadder.c:173`) вообще убрал кормление движения на лестнице — т.е. мы
  НЕ задаём нативу направление вверх/вниз (Expansion задаёт вперёд/назад).

## Q4. Где правильное место фикса

**Порядок приоритетов (по «человеческой модели»: закрыть нативную фазу → гасить остаток):**

1. **Закрыть нативную фазу лестницы, как Expansion.** В `dmBotIntent_UseLadder`:
   (а) вернуть кормление направления на лестнице (`IntentMove(0, speed)` вверх /
   `IntentMove(180, speed)` вниз — эталон `eAICommandMove.c:1707-1720`);
   (б) переставить порядок: сначала `hcl.CanExit() && время_на_лестнице > 2.0 → hcl.Exit()`,
   а `!IsClimbingLadder() → Finish()` — только как fallback (или вообще ждать
   `OnCommandLadderFinish`). Сейчас порядок обратный → `Exit()` мёртв.
   (в) опционально: override `OnCommandLadderFinish()` (нет в нашем коде) как сигнал
   «лестница реально закончилась», а не `!IsClimbingLadder()`.

2. **Гасить остаток в HeadingModel (главный фикс спина), по образцу Expansion.**
   В `dmAISurvivorBase.HeadingModel` (сейчас `:638-676`):
   - для `COMMANDID_FALL` (и, вероятно, `COMMANDID_MELEE2` — уже есть в другом месте) —
     снап `orientation = heading` + `SetOrientation(...)`, `m_fLastHeadingDiff = 0`,
     `return false` (эталон `eAIBase.c:10093-10101`);
   - для `COMMANDID_MOVE` — решить, снапать ли `heading = orientation = GetOrientation()[0]`
     как Expansion (`eAIBase.c:10047-10065`), либо оставить ванильный `RotateOrient` (наш
     бот доворачивает корпус foot-step'ом `ApplyBodyTurn`, поэтому полный снап MOVE может
     конфликтовать — [нужно проверить]). Минимум: НЕ оставлять зазор >171° «на волю»
     `RotateOrient` с обнулённым `m_fLastHeadingDiff`.
   - альтернатива «как человек»: после схода (по `OnCommandLadderFinish`/`!IsClimbingLadder()`)
     однократно снапнуть `SetOrientation(heading)` (довернуть тело к тому, куда натив повернул
     aim), сняв зазор до 0 — тогда `RotateOrient` не будет прыгать.

3. **Не чинить сбросом через пересоздание** (`m_eAI_Recreate` — Expansion оставил это
   закомментированным как «hack»). Правильный путь — снап в HeadingModel + корректное
   завершение команды.

## Гипотезы для эмпирической проверки

- **[нужно подтвердить]** Какой `m_CommandTypeId` активен в момент накрутки heading ~+162°/кадр
  после схода — `COMMANDID_FALL` или уже `COMMANDID_MOVE`? Проб: лог `m_MovementState.m_CommandTypeId`
  + `m_iMovement` + `m_fHeadingAngle`/`m_fOrientationAngle` per-frame в окне 0..3 с после
  `!IsClimbingLadder()` (ручка `botdump`, DEBUG-домен `DM_BOT_DEBUG_LADDER` уже есть в
  `dmAISurvivorBase.c:1220-1232`). Вопрос: накрутка идёт в FALL или в MOVE?
- **[нужно проверить]** Снимает ли спин снап `SetOrientation(heading)` сразу после схода
  (однократно, по `OnCommandLadderFinish`)? Ожидание: зазор→0, `RotateOrient` больше не прыгает.
- **[нужно проверить]** Перестаёт ли бот спускаться корректно, если кормить `IntentMove(180, …)`
  на лестнице при `dir=-1` (эталон Expansion `eAICommandMove.c:1710-1716`)? Сейчас кормление
  убрано экспериментом (`dmBotIntent_UseLadder.c:173`).
- **[нужно подтвердить]** Фиксируется ли завершение лестницы, если переставить порядок
  `CanExit()→Exit()` ДО `!IsClimbingLadder()` (и добавить `время_на_лестнице > 2.0`)?
  Вопрос: приходит ли `CanExit()==true` вообще (бот доезжает до точки выхода)?
- **[нужно проверить]** Конфликтует ли полный снап heading в `COMMANDID_MOVE` (Expansion
  `eAIBase.c:10047-10065`) с нашим foot-step доворотом `ApplyBodyTurn` (двойное управление
  корпусом)? Если конфликтует — ограничиться снапом только в FALL.

## Развилки / решения (для журнала decisions.md)

1. **Где гасить остаток — снап в HeadingModel vs однократный `SetOrientation` после схода.**
   Склоняюсь к снапу в HeadingModel по образцу Expansion (закрывает класс задач «зазор
   heading↔orientation после нативных full-body команд», как уже сделано для MELEE2 в
   `decisions.md` «melee: заморозка тела на замахе»). Окончательно — после пробы [нужно проверить]
   «снап после схода».
2. **Детект завершения лестницы — `!IsClimbingLadder()` vs `OnCommandLadderFinish()`/**
   **`pCurrentCommandFinished`.** Expansion использует нативный сигнал финиша; наш
   `!IsClimbingLadder()` срабатывает раньше и не даёт `Exit()` отработать. Правильный путь —
   нативный сигнал (через `OnCommandLadderFinish()` или ожидание `CanExit()`+`Exit()`).
