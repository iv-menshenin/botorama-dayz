# FSM движка ботов — план реализации (ядро)

Детальный план ядра FSM для `botorama`. Высокоуровневые фазы и TODO — в
`Reference/stage-1.FSL.plan.md`; здесь — конкретный дизайн кода, согласованный в
обсуждении. Накопленный техдолг (мёртвый код, заглушки, отложенное) — в
`Reference/techdebt.md`.

## Зафиксированные решения

1. **Взвешенный случайный переход**: у каждого перехода `weight` (0..1); выбор —
   `r = Math.RandomFloat01() * Σweight`, идём по накопленной сумме. Не «первый в
   списке».
2. **Два уровня блокировки**:
   - `dmBotState.CanEnter()` — «состояние вообще актуально?» (ко всем входам).
   - `dmBotTransition.BlockWhen(cond)` / `Require(cond)` — «это ребро разрешено?».
3. **Условия — композируемые и stateless** (`dmBotCondition`), бот передаётся в
   `Evaluate(dmAISurvivor bot)`. Композиция `And` / `Or` / `Not`.
4. **Топология — только кодом** (состояния/переходы/веса в Enfusion-классах и
   пресетах). JSON (Phase 3) — только параметры.
5. `to` в guard не нужен (переход хранит `m_To`); `from` тоже не нужен (условие
   видит бота, бот знает своё состояние FSM).

## Классы

### `dmBotFSM` (контейнер)
- Поля: `m_Owner` (`dmAISurvivor`), `m_States`, `m_CurrentState`, `m_DefaultState`.
- `AddState`, `GetState(name)`, `GetCurrentState()`, `Start(name="")`, `Update(pDt)`.
- `Update`: `OnUpdate(current)`; при `EXIT` → `SelectTransition` → `OnExit`/`OnEntry`;
  раз в `s_PreemptInterval` — вытеснение (см. «Модель переходов»).

### `dmBotState` (база)
- Поля: `m_Name`, `m_FSM`, `m_Transitions`.
- `AddTransition(to, weight)` (флюент, возвращает переход), `GetTransitions()`.
- `GetOwner()` → бот; `GetState(name)` → по имени.
- Виртуальные: `OnEntry(from)`, `OnExit(to)`, `OnUpdate(pDt)` (CONTINUE/EXIT),
  `CanEnter()` (релевантность).

### `dmBotTransition` (данные ребра)
- Поля: `m_To`, `m_Weight`, `m_Block` (`dmBotCondition`), `m_Require`.
- `BlockWhen(cond)` / `Require(cond)` — флюент.
- `Guard(dmAISurvivor bot)`: `!m_Block.Evaluate(bot) && (!m_Require || m_Require.Evaluate(bot))`.

### `dmBotCondition` (предикат, stateless)
- `bool Evaluate(dmAISurvivor bot)` (override в листьях).
- `And(other)` / `Or(other)` / `Not()` — возвращают композиты.
- Статичные фабрики листьев — в отдельном классе `dmBotConditions` (не перегружаем
  базовый): `dmBotConditions.LowHealth()` и т.д.

## Розыгрыш (SelectTransition)

```
собрать переходы текущего состояния, где:
  to.CanEnter() == true          // релевантность состояния
  && t.Guard(bot) == true        // условия ребра (block/require)
  && t.GetWeight() > 0
total = Σ weight
если нет допустимых или total <= 0 -> fallback в default-состояние
r = RandomFloat01() * total
acc = 0; идём по переходам: acc += weight; если r < acc -> выбран
```

## Модель переходов (гибрид: кооперативная + вытеснение)

Переходы двух видов:

1. **Кооперативный (EXIT)**: состояние вернуло `EXIT` → принудительный взвешенный
   розыгрыш среди eligible-переходов (`CanEnter && Guard && weight > 0`). Нет
   eligible → fallback в default-состояние.
2. **Вытеснение (preempt)**: если текущее состояние `INTERRUPTIBLE`, то раз в
   `s_PreemptInterval` (по умолчанию `DM_FSM_PREEMPT_INTERVAL` = 0.25с; меняется
   статическим `dmBotFSM.SetPreemptInterval`) ищем переходы к `PREEMPTIVE`
   состояниям с открытым guard. Есть хоть один → взвешенный рандом **обязан**
   перейти (вытеснить текущее, без его `EXIT`).

`enum dmBotStateKind { NORMAL, INTERRUPTIBLE, PREEMPTIVE }` — взаимоисключающие
(enum запрещает быть одновременно прерываемым и вытесняющим → нет дрожания):
- `NORMAL` — атомарное (ни прерывается, ни вытесняет).
- `INTERRUPTIBLE` — может быть прервано (напр. Patrol).
- `PREEMPTIVE` — вытесняет INTERRUPTIBLE (напр. Fight в будущем).

Состояние задаёт вид переопределением `GetKind()` (по умолчанию `NORMAL`).

### Нюансы реализации (важно)
- `dmBotFSM.Start()` входит в дефолт-состояние **без проверки `CanEnter()`** — guard
  проверяется только на переходах, не на старте. Поэтому «данные» для стартового
  состояния (напр. точки патруля) должны быть готовы до `Start`/`/fsm apply`.
- Дефолт в билдере = **первое добавленное** состояние: порядок `/fsm add` задаёт,
  с чего бот стартует (`add patrol` → старт в патруле).
- При двух состояниях авто-переходы **детерминированы** (у каждого ровно один
  исходящий переход, вес 1.0): это цикл `A → B → A → …`, а не «один раз и стоп».
  Случайность появляется только при 3+ eligible-вариантах.

## Файлы

```
core/4_World/Entities/Bot/
├── dmTarget.c                     # цель бота (ACQUIRE/DESTROY, скелет)
├── FSM/
│   ├── dmBotFSM.c
│   ├── dmBotState.c
│   ├── dmBotTransition.c
│   ├── dmBotCondition.c            # база + And/Or/Not
│   └── Conditions/
│       ├── dmBotConditions.c       # статичные фабрики листьев
│       ├── dmBotCondition_LowHealth.c
│       ├── dmBotCondition_NoAmmo.c
│       └── dmBotCondition_HasPlayerSigns.c
├── Intent/                        # намерения (атомарные желания)
│   ├── dmBotIntent.c
│   ├── dmBotIntentPool.c
│   ├── dmBotIntent_HoldLook.c     # смотреть на точку/сущность
│   ├── dmBotIntent_LookAround.c   # периодически озираться
│   ├── dmBotIntent_Glance.c       # взгляд в сторону (голова)
│   ├── dmBotIntent_Turn.c         # разворот тела
│   └── dmBotIntent_MoveTo.c       # идти к точке
├── States/
│   ├── dmBotState_Idle.c          # реализовано
│   ├── dmBotState_Patrol.c        # реализовано
│   ├── dmBotState_Hunting.c       # скелет (пример охотника)
│   ├── dmBotState_Stealth.c       # скелет (пример охотника)
│   ├── dmBotState_Fighting.c      # скелет (пример охотника)
│   └── dmBotState_Surrender.c     # скелет (пример охотника)
└── Presets/
    └── dmBotPreset_Hunter.c       # пример
```

## Intent-пул (намерения)

Слой между FSM (что делать) и примитивами мозга (как). Атомарное «желание» бота:
FSM-состояние задаёт намерение, бот выполняет его по тикам. Намерения конкурентны —
могут выполняться одновременно; арбитраж решает, кто что контролирует.

### Каналы и тело
- Каналов два: **взгляд** (голова) и **движение** (ноги).
- **Тело — не канал, а производная**:
  - движется → тело смотрит по направлению движения;
  - стоит + взгляд `FULL`/`AUTO` → тело доворачивается к взгляду;
  - стоит + `NONE` → тело держится.
- Режим взгляда:
  ```c
  enum dmBotLookTurn { NONE, AUTO, FULL }
  // NONE — только голова (не глянет за спину)
  // AUTO — через плечо, если цель за пределами головы (текущее поведение)
  // FULL — полностью развернуться к цели
  ```
  Методы: `LookAtPoint(pt, dmBotLookTurn turn = AUTO)` /
  `LookAtDirection(v, h, dmBotLookTurn turn = AUTO)`.

### `dmBotIntent`
```c
enum dmBotIntentPriority    { IDLE, DESIRABLE, CRITICAL }
enum dmBotIntentConcurrency { PARALLEL, EXCLUSIVE }

class dmBotIntent
{
	dmBotIntentPriority    m_Priority;
	dmBotIntentConcurrency m_Concurrency;
	float m_Deadline = -1.0;   // -1 = без дедлайна
	bool  m_Finished;

	void OnStart(dmAISurvivor bot) {}
	void OnUpdate(dmAISurvivor bot, float pDt) {}   // пишет в канал мозга (LookAt/MoveTo)
	void OnCancel(dmAISurvivor bot) {}
}
```

### Три пула
Бот держит три `dmBotIntentPool`:
- `m_FSMIntents` — подсознание/автоматика; **`Clear()` на FSM-переходе**. FSM сама
  управляет присутствием (может и с таймером, но удаляет сама; есть проверка
  `Has(intent)`).
- `m_PersonalityIntents` — прерывания «спасти жизнь» (шум, угроза). Дедлайн;
  просроченные удаляются молча.
- `m_CommandIntents` — приказы группы; живут до отмены приказа.

### Приоритет и конкурентность (арбитраж)
- `EXCLUSIVE` — глушит всё остальное.
- `PARALLEL` — сосуществует (может смотреть в другую сторону).
- При двух `CRITICAL` — порядок пулов как тай-брейк: **личность → команда → FSM**.
- Реализовано: `EXCLUSIVE` глушит всё (один exclusive-победитель по приоритету и
  тай-брейку пулов), `PARALLEL` сосуществуют (по возрастанию приоритета, старший
  пишет в канал позже). **Отложено**: пер-канальное «`CRITICAL+PARALLEL` из личности
  переопределяет движение `CRITICAL+EXCLUSIVE`» (сейчас exclusive глушит безусловно).

## Пример охотника (idle → hunting/stealth → idle)

- `Hunting.CanEnter()` = `HasPlayerSigns() && !IsLowHealth()`.
- `Stealth.CanEnter()` = `IsLowHealth()`.
- `idle → surrender` с `Require(LowHealth().And(NoAmmo()))` — сдаться только при
  низком здоровье И без патронов.
- `hunting → fighting` с `BlockWhen(LowHealth().Or(NoAmmo()))` — не в бой, если
  низкое здоровье ИЛИ нет патронов.

Приоритет «low health → скрытность вместо охоты» выражается самими `CanEnter()`
(без отдельной логики приоритетов).

## Тест-команды намерений

Ручное тестирование исполнения намерений (без FSM) — чат-команды:

- `/bot intent lookAt` — смотреть в точку, куда смотрит игрок (raycast до пересечения).
- `/bot intent lookAtMe` — смотреть на игрока-отправителя (в лицо, `EntityAI`).
- `/bot intent goto` — идти в точку, куда смотрит игрок.
- `/bot intent clear` — очистить пул приказов.

Конфиг намерений:
- «посмотреть» (`lookAt`/`lookAtMe`) — `PARALLEL` + `CRITICAL`, дедлайн `DM_TEST_LOOK_DEADLINE` (30 с).
- «идти» (`goto`) — `PARALLEL` + `CRITICAL`, без дедлайна.

Всё добавляется в **command-пул** (`AddCommandIntent`); добавление НЕ очищает пул
(можно набирать последовательность); `clear` → `ClearCommandIntents()`.

Привязка бота к игроку: `/bot spawn test` кладёт бота в `MissionServer.s_TestBotByPlayer`
(по имени игрока), чтобы `intent`-команды знали, кем командует игрок.

Офсет взгляда: игрок → в лицо (`GetPosition + DM_EYE_HEIGHT`); точка/предмет → ровно
в точку (raycast-пересечение). Тип `m_Entity` пока `EntityAI`.

Raycast: `DayZPhysics.RaycastRV(глаз, глаз + MiscGameplayFunctions.GetHeadingVector(player) * DM_LOOK_RAYCAST_DISTANCE, ...)`
с `ignore = player`. (Направление взгляда игрока — ванильный
`MiscGameplayFunctions.GetHeadingVector`, НЕ Expansion `GetLookDirection`.)

## Состояния Idle / Patrol + цели

### Намерения ориентации
- `dmBotIntent_Glance` — «посмотреть»: взгляд в сторону (голова, `NONE`), дедлайн 15с.
- `dmBotIntent_Turn` — «повернуться»: безоговорочный разворот тела+головы на угол,
  FINISH при довороте (допуск 2°). Отдельный класс, не режим взгляда.

### Idle
- Случайный таймер 15–60с; по таймеру — кубик угла 15–120 (±); `<45` → кубик 0/1
  (0 = glance на 15с, 1 = turn); `≥45` → turn. Всё через интенты.

### Patrol
- `CanEnter()` = есть точки (`GetPatrolPoints().Count() > 0`).
- На входе фиксирует маршрут (копия), `MoveTo` к точке; «≤1м + 5с» → следующая;
  последняя → EXIT.

### Цели (память бота, скелет)
- `dmTarget`: `EntityAI m_Entity` (может быть null) + `string m_ClassEntity`
  (класс для поиска) + `float m_Priority` + `vector m_LastPosition` + тип
  (ACQUIRE/DESTROY).
- Бот владеет коллекцией целей (переживает переходы FSM). Цель вне видимости → бот
  ищет по `m_LastPosition`/`m_ClassEntity`, а не видит живую позицию
  (см. Expansion `eAITargetInformation`/`eAITargetInformationState`).

### Fallback «нет перехода»
- `dmBotFSM`: если `SelectTransition` не нашёл eligible-переход → форс-переход в
  default-состояние (Idle) — страховка от застревания.

### Команды (унифицировано)
- `/fsm new`, `/fsm add idle|patrol`, `/fsm apply` — билдер FSM
  (авто-соединение всех пар, guard = `CanEnter`).
- `/bot patrol add|clear` — точки патруля (точка, куда смотрит игрок).

## Движение к точке (реализовано)

> **Статус: реализовано** (`2.5`, коммиты по движению). Ниже — исходный план;
> что именно легло в код и что изменилось по ходу — в «Как сделано».

Исходная проблема: `MoveTo` был упрощён (`FacePoint(цель)` + `SetWalk(true)`),
тело жёстко связано с направлением движения.

### Цель
Тело (куда смотрит) и направление движения (куда идёт) — **независимы**.
Движение вперёд/назад/вбок согласуется с поворотом тела. Пример: патруль A→B,
выстрел со стороны A → тело разворачивается на A (посмотреть), но бот продолжает
**пятиться** к B (`OverrideMovementAngle(±180)`).

### Референс (Expansion / ванилла)
- `HumanInputController.OverrideMovementAngle(type, angle)` — угол направления
  движения **относительно корпуса**, `-180..180`: 0 = вперёд, ±90 = страйф,
  ±180 = назад. `OverrideMovementSpeed(type, 0..3)` — скорость. Тип `ONE_FRAME`
  (пересчитывается каждый кадр) или `ENABLED` (держится).
- `HumanCommandMove.GetCurrentMovementAngle()` / `GetCurrentInputAngle(out float)` —
  текущий/сырой угол движения (-180..180).
- Expansion `eAICommandMove` (`.../AI/Classes/Commands/eAICommandMove.c`):
  - движение — `OverrideMovementAngle(ONE_FRAME, m_MovementDirection)` +
    `OverrideMovementSpeed(ONE_FRAME, speed)`;
  - направление движения = `ExpansionMath.AngleDiff2(текущий yaw тела, целевой yaw)`
    — угол цели относительно корпуса (страйф/бэкпедал);
  - поворот корпуса при движении — `Anim_SetFilteredHeading` → `SetOrientation`
    (слайд), при идле — foot-step turn (`CallTurn` + `SetTurnAmount`).

### Как делаем
1. В `dmAISurvivor` развести «взгляд» и «движение»: `SetMoveTarget(vector)` считает
   угол цели относительно корпуса и зовёт `OverrideMovementAngle`/`OverrideMovementSpeed`;
   корпус крутит отдельный look/turn-интент (уже есть `dmBotIntent_Turn`,
   `HoldLook` с `FULL`).
2. `dmBotIntent_MoveTo.OnUpdate` пересчитывает направление движения каждый тик
   (цель может двигаться / тело поворачивается) и не трогает корпус.
3. Протестировать: `/bot intent goto` при развёрнутом корпусе → бот пятится/страфит,
   а не доворачивается к цели.

### Как сделано (по факту)
- `dmAISurvivor.SetMove(angle, speed)` (движение относительно корпуса) + `SetWalk`
  остался обёрткой; `UpdateLook` пускает `FULL`-поворот тела и на ходу.
- `dmAISurvivorBase.ApplyBodyTurn` — два режима: на ходу слайд-поворот
  `SetOrientation` (скорость `DM_MOVE_TURN_RATE`), на идле — переступание.
- `dmBotIntent_MoveTo` — пер-тик доворот + прогресс-монитор (`Fail()` + `Error`).
- `dmBotIntent.Fail()/IsFailed()`; `dmBotState_Patrol` стал intent-driven
  (реагирует на `IsFinished()/IsFailed()` интента, своего замера дистанции нет).

### Уроки (что обожгло)
- Детект «цель удаляется» (`dist > best + порог`) НЕ ловит «застрял в углу»
  (дистанция просто перестаёт убывать). Правильный монитор — «нет прогресса»:
  сброс таймера только при `dist < best - DM_MOVE_PROGRESS_EPS`.
- `Patrol` не должен сам мерить дистанцию (3D vs 2D у `MoveTo` расходятся из-за
  высоты) — единственный источник истины «достигнута точка» — состояние интента.

## Pathfinding (навигация по navmesh) — Phase 5

> **Статус: базовый обход реализован** (`2.6`, `dmBotPathfinder` + path-aware
> `MoveTo`). «TODO на будущее» ниже — не сделано.

Учимся находить путь: `MoveTo` сам решает, как лучше пройти к цели, обходя
препятствия по navmesh (а не упираясь в стены).

### Зафиксированные решения
1. **Базовый обход по navmesh** на первом шаге: только `AIWorld.FindPath` по
   `WALK|DOOR|INSIDE`. Двери/прыжки/лестницы/плавание — отложены (TODO ниже).
2. **Нет пути → сразу `Fail()`** (без fallback «по прямой»), с `dmBotLog.Error`.
3. **Pathfinding внутри `dmBotIntent_MoveTo`** (расширяем существующий интент),
   `Patrol` и `/bot intent goto` не меняются.

### Нативная API (ванилла `3_game/ai/aiworld.c`)
- `AIWorld` = `GetGame().GetWorld().GetAIWorld()`:
  - `FindPath(from, to, PGFilter, out TVectorArray waypoints)` — A* по navmesh,
    возвращает точки (вкл. старт и конец); `TVectorArray` = `array<vector>`.
  - `RaycastNavMesh(from, to, filter, out hitPos, out hitNormal)`.
  - `SampleNavmeshPosition(pos, maxDist, filter, out sampled)` — прижать точку к navmesh.
- `PGFilter` (`Managed`): `SetFlags(include, exclude, exclusive)`, `SetCost(PGAreaType, cost)`.
- `PGPolyFlags`: `WALK/DOOR/INSIDE/LADDER/SPECIAL/JUMP/CLIMB/CRAWL/CROUCH/SWIM/UNREACHABLE/ALL`.
- Реф: Expansion `ExpansionPathHandler` — «обёртка-монстр» над этими тремя вызовами
  (троттлинг пересчёта, двери, vault, лестницы, плавание, attachment-navmesh,
  fall-height, path glue). Мы берём только ядро.

### Файлы и изменения
1. **Новый `core/4_World/Entities/Bot/Pathfinding/dmBotPathfinder.c`** — обёртка:
   - `AIWorld m_AIWorld`; `PGFilter m_Filter` (include `WALK|DOOR|INSIDE`, exclude
     `CRAWL|CROUCH|SWIM|SWIM_SEA|SPECIAL|UNREACHABLE`); `PGFilter m_SampleFilter`
     (`ALL & ~(CRAWL|CROUCH)`). Стоимости дефолтные.
   - `bool FindPath(from, to, out ref array<vector> waypoints)`,
     `bool SamplePosition(pos, maxDist, out sampled)`.
2. **`dmAISurvivor.c`** — примитив `bool FindPathTo(vector target, out ref array<vector> path)`:
   сэмпл цели к navmesh (`DM_PATH_SAMPLE_RADIUS`), при неудаче `false`; иначе
   `FindPath(GetPosition(), sampled, path)`. Ленивый `ref dmBotPathfinder m_Pathfinder`.
3. **`Intent/dmBotIntent_MoveTo.c`** — расширяем:
   - `OnStart`: `bot.FindPathTo(m_Target)` → `m_Path`, `m_PathIdx = 0`; пути нет →
     `dmBotLog.Error` + `Fail()`.
   - Тик: подцель = `m_Path[m_PathIdx]`; пер-тик доворот к подцели + `SetMove`.
   - Достиг подцели (`DM_PATH_WAYPOINT_REACH`) → `m_PathIdx++`; последняя →
     `Finish()` (радиус `m_ReachDistance`).
   - Прогресс-монитор — по подцели; застрял → пересчёт пути 1 раз → не помогло →
     `Fail()`.
4. **`cons/4_World/constants.c`** — `DM_PATH_SAMPLE_RADIUS`, `DM_PATH_WAYPOINT_REACH`.
5. **`dmBotState_Patrol.c`** — без изменений.

### Гипотезы (что предполагаем и проверяем)
- **Г1**: `AIWorld.FindPath` доступен серверу и работает для `INSTANCETYPE_AI_SERVER`-пешки
  без отдельной регистрации в AI-системе (это просто запрос по координатам).
- **Г2**: `PGFilter`/`AIWorld` (`Managed`) безопасно держать как `autoptr`/поле
  (по образцу Expansion `ExpansionPathFilters`); точный тип владения уточнить при сборке.
- **Г3**: пер-тик доворот к подцели (тот же `moveAngle = AngleDiff(subYaw, bodyYaw)`)
  без string-pulling даёт приемлемый маршрут — зигзаги по углам полигонов терпимы.
- **Г4**: `FindPath` синхронный, но для нескольких ботов и «по событию» (старт + застревание)
  достаточно дёшев; вызывать каждый тик не нужно.
- **Г5**: проглотить цель к navmesh в радиусе `DM_PATH_SAMPLE_RADIUS` достаточно,
  чтобы точки патруля «у крыльца» не считались недостижимыми.

### Чего ожидаем от реализации (приёмка)
- `/bot intent goto` за дом → бот огибает дом по navmesh, не упирается в стену.
- `/bot patrol add` с точкой за углом → проходит все точки по пути, `EXIT` на последней.
- Точка вне navmesh (нет сэмпла) → `Fail()` + `[dmBot][error]`, патруль пропускает точку.
- Застревание на середине пути → пересчёт 1 раз → `Fail()` (не зависает).

### TODO на будущее (отложено сознательно)
- **Двери**: `PGPolyFlags.DOOR`/`DISABLED`, открытие/закрытие (`AI_HANDLEDOORS`),
  стоимость `DOOR_CLOSED`/`DOOR_OPENED` в `PGFilter.SetCost`.
- **Vault/climb**: `SPECIAL`/`JUMP`/`CLIMB` + команда движения на перепрыгивание/перелезание
  (`eAICommandMove` + `AI_HANDLEVAULTING`, `RaycastNavMesh` для детекта ребра).
- **Лестницы**: `PGPolyFlags.LADDER`, состояние подъёма/спуска (`m_eAI_IsOnLadder`),
  entry-point и `SamplePosition` к лестнице.
- **Плавание**: `SWIM`/`SWIM_SEA`, фильтры плавания, скорость/анимации в воде.
- **Attachment-navmesh** (поезда/подвижные объекты): `EXPANSION_AI_ATTACHMENT_PATH_FINDING`,
  transform-преобразование точек (`Multiply4`/`InvMultiply4`).
- **String-pulling / сглаживание пути**: отбрасывание лишних углов полигонов.
- **Троттлинг пересчёта** как у Expansion: интервалы + детект овершута (у нас — только
  по событию «застрял»).
- **Кост-тюнинг**: `PGFilter.SetCost` (дороги дешевле, вода/двери дороже).

## Стойка (осанка) и укрытие (Stealth) — реализовано `2.7`

### Модель
Стойка (`стоя`/`крадучись`/`лёжа`) — третий канал арбитража (как взгляд/движение).
Фоновое намерение «стоять» = пер-тик сброс `SetStance(STANCEIDX_ERECT)` в
`UpdateIntents`. Нет активного stance-интента → бот стоит; есть crouch/prone-интент
(приоритет выше) → перекрывает. Интент исчез (дедлайн/`ClearFSMIntents`) → следующий
арбитраж снова видит только «стоять» → бот встаёт сам.

- **`dmBotIntent_Stance`** (PARALLEL): поле `int m_Stance` (STANCEIDX_*).
- Мозг: `m_DesiredStance` + `SetStance(int)`; `ApplyStance()` после арбитража зовёт
  `GetCommand_Move().ForceStance(next)` только при смене. **erect↔prone — через crouch**
  (таймауты `DM_STANCE_TIMEOUT_CROUCH`/`DM_STANCE_TIMEOUT_PRONE`), иначе «ломается
  привязка к поверхности и хитбокс» (реф: Expansion `eAICommandMove`).
- Текущая стойка: `GetMovementState(state)` → `state.m_iStanceIdx` (нормализовать
  вычитанием `STANCEIDX_RAISED`, если raised).

### Состояние `dmBotState_Stealth`
- Атрибут: `vector m_CoverPosition` (укрытие).
- Фазы: entry → crouch (CRITICAL, deadline -1) + `MoveTo(укрытие)` → дошёл →
  `m_StanceIntent.m_Stance = PRONE` (лечь) → dwell `DM_STEALTH_PRONE_DWELL_TIME` → EXIT.
- `CanEnter()` = есть укрытие; `GetKind()` = INTERRUPTIBLE. Нет пути → лог + EXIT.

### Команды
- `/bot intent stance erect|crouch|prone` — держать стойку (command-пул, без дедлайна);
  сброс — `/bot intent clear`.
- `/fsm add stealth` — добавить состояние с укрытием = точка, куда смотрит игрок
  (`s_DraftStealthCover`).

### Уроки
- Стойка — дискретный канал, а не пер-тик непрерывный контроль: применяем `ForceStance`
  только при смене `m_DesiredStance` (иначе дергается). Сброс в «стоять» и есть «фоновое
  намерение встать».
- Один stance-интент на состояние проще двух: фаза меняет его `m_Stance`, а не добавляет
  второй интент (не полагаемся на порядок в пуле).

## Доворот корпуса на ходу (баг + план)

### Баг
Слайд-поворот (`SetOrientation` в `ApplyBodyTurn` после `super.CommandHandler()`)
**не крутит корпус на ходу**: пока идёт движение, ванильный movement-command
(heading model) сам управляет ориентацией и перетирает наш `SetOrientation` в
следующем кадре. Переступание на идле работает (root-motion анимации), а слайд-поворот
— нет. Симптом: бот шёл к точке **спиной** (корпус не доворачивался). У Expansion доворот
на ходу — это кастомный `eAICommandMove` (крутит корпус в физ-фазе `PrePhys_SetRotation`,
`Anim_SetFilteredHeading` изнутри `PreAnimUpdate`), а не простой `SetOrientation`.

### Стратегия (два шага)
1. **Быстрый фикс (сейчас) — stop-turn-walk** в `MoveTo`: если `|moveAngle| >
   DM_MOVE_FACE_THRESHOLD` (корпус сильно не совпадает с направлением движения) —
   стоп (`SetMove(0,0)`), идл-переступание доворачивает корпус, прогресс-монитор паузим;
   довернулись — пошли. Константа `DM_MOVE_FACE_THRESHOLD`.
2. **Правильный фикс (техдолг) — кастомный movement-command** по образцу `eAICommandMove`
   (упрощённый): `PreAnimUpdate` (look + `SetOrientation` в нужной фазе),
   `PrePhys_SetRotation`, `PostPhysUpdate`. Сюда уезжают `SetMove`/`ApplyStance`/
   `ApplyBodyTurn`/часть `UpdateLook`. Локализовано в слое движения.

### Шов (контроль техдолга)
Примитивы мозга (`SetMove`, `SetStance`, `ApplyStance`, `LookAtPoint→SetTargetBodyYaw`)
— единственный интерфейс для интентов. Кастомный команд меняет только их внутренности,
интенты/FSM не трогаем.

### Результат (`2.9`, незакоммичено)
- **Stop-turn-walk реализован** в `MoveTo`: `|moveAngle| > DM_MOVE_FACE_THRESHOLD` →
  стоп + идл-переступание доворачивает корпус, прогресс-монитор паузится. Бот больше не
  идёт к точке спиной.
- **Автодедлайн интентов**: `dmBotIntent.IsExpired()` — интент без дедлайна, проживший
  `DM_INTENT_MAX_AGE` (5 мин), удаляется. Ни один интент не живёт вечно.
- **`/bot intent`-приказы**: `goto`/`stance` получили дедлайн 1 мин
  (`DM_TEST_COMMAND_DEADLINE`); `crouch` — 5 мин, `lookAt`/`lookAtMe` — 30 с.
- Кастомный movement-command — в техдолг (см. `techdebt.md` D).

### Результат-2 (`2.11`, незакоммичено) — фикс «шарканья» stop-turn-walk
- **Баг**: `ApplyBodyTurn` выбирал слайд-поворот/переступание по
  `GetCurrentMovementSpeed() > 0.01`. Само переступание (locomotion `Turn`) даёт
  ненулевой `GetCurrentMovementSpeed()`, поэтому на следующем кадре ветка «движусь»
  **отменяла** переступание — тело болталось на ±7° и не доворачивалось (бот «шаркал»).
- **Фикс**: выбор режима — по **намерению** двигаться (`m_IsMoving` на пешке, ставит
  `SetMove` через `SetMoving(speed > 0)`), а не по фактической скорости.

## Кастомный movement-command (план перехода, поэтапно)

Цель — убрать костыли доворота корпуса, сделав наше управление ориентацией
авторитетным (по образцу Expansion `eAICommandMove`).

### Ключевая механика (главное открытие)
Ванильный move-command **сам крутит корпус** к направлению движения через хук
`HeadingModel(pDt, pModel)` → `DayZPlayerImplementHeading.RotateOrient`
(`dayzplayerimplement.c:1673`). Именно он перетирает наш `SetOrientation`.
Expansion в `eAIBase.HeadingModel` для `COMMANDID_MOVE` **отключает** его:
`m_fHeadingAngle = m_fOrientationAngle = GetOrientation()[0]*DEG2RAD; return true;`
после чего крутит корпус сам через `Anim_SetFilteredHeading` → `SetOrientation`.

### Переносимые механики
1. **`HeadingModel` override** — отключить ванильный поворот корпуса.
2. **`SetOrientation` (слайд)** — доворот корпуса (работает на ходу и в приседе).
3. **`OverrideMovementAngle/Speed(ONE_FRAME, …)`** — движение без «залипших» override'ов.
4. **`ForceStance`** — стойка (перенос в пешку).

### Границы классов
- **Мозг `dmAISurvivor` — только решения**: `CalcSpeed`, `FindPathTo`,
  `LookAtPoint` (цель взгляда + целевой yaw корпуса), `SetMove`, `SetStance`.
  НЕ трогает `GetInputController`/`GetCommand_Move`.
- **Пешка `dmAISurvivorBase` — только применение**: желаемое состояние
  (`m_DesiredMoveAngle/Speed/Stance/BodyYaw`, `m_IsMoving`), сеттеры, `HeadingModel`
  override, `CommandHandler` (look до super → super → `ApplyBodyTurn` слайд →
  `ApplyMovement` ONE_FRAME → `ApplyStance`).
- Интенты/FSM/pathfinding — не меняются (зовут примитивы мозга).

### Фазы
- **Фаза 1 (сделано, `7596ade`)**: `HeadingModel` override + `ApplyBodyTurn`:
  слайд `SetOrientation` — только на ходу; на идле — **переступание для всех стоек**
  (граф уже умеет: `TurnStanceSTM` выбирает стойку-специфичный поворот — шаг стоя,
  шаг в приседе, перекат лёжа). Слайд-поворот начинает работать на ходу.
- **Фаза 2 (сделано, `309fd3b`)**: консолидация — `SetMove`/`SetStance`/`ApplyMovement`/
  `ApplyStance` в пешку, `ONE_FRAME`; мозг чистые решения; убран stop-turn-walk.
- **Фаза 3 (сделано, `5043847`)**: сглаживание движения (рамп скорости + доворот-с-
  притормаживанием) и политика комфорта корпуса (`m_MoveYaw` + `ComputeBodyYaw`).
  Дальше — только уточнение «наименьшей дискомфортности» (непрерывный компромисс
  `w_head·|headAngle| + w_move·|moveAngle|`), когда появится «смотреть на врага» (Fight).

### Как прошлые ошибки становятся архитектурно невозможны
- «слайд перетирается» → `HeadingModel` отключён, `SetOrientation` авторитетен;
- «дребезг движусь/стою» → состояние движения наше (`m_IsMoving`/`m_DesiredSpeed`),
  `GetCurrentMovementSpeed()` не используется;
- «переступание не крутит в приседе/лёжа» → переступание на идле для ВСЕХ стоек (граф
  `TurnStanceSTM`), стойка влияет только на выбор анимации поворота внутри графа, а не
  на нашу логику.

## Этапы

- **Phase 1 (готово)**: ядро FSM (`dmBotFSM/State/Transition`), условия
  (`dmBotCondition` + композиты), взвешенный выбор, гибридная модель переходов
  (EXIT + вытеснение). Не прогонялось распределение весов (критерий Phase 1).
- **Phase 2 (готово)**: привязка к `dmAISurvivor` (`m_FSM`, `LoadFSM`,
  `OnUpdate → Update`), хардкод `SetLookTarget` убран; intent-слой
  (HoldLook/LookAround/Glance/Turn/MoveTo) + три пула + арбитраж; состояния
  Idle/Patrol; билдер `/fsm …` и тест-команды `/bot intent …` / `/bot patrol …`.
- **Phase 3**: веса/пороги из JSON (`fsm_<preset>.json` через `dmJsonFile`).
- **Phase 4 (частично)**: реальные действия состояний (статы, скан признаков) —
  осталось; **движение к точке** (развязка взгляда и движения) — готово.
- **Phase 5 (база готова, `ef66b60`)**: pathfinding — базовый обход по navmesh
  (см. «Pathfinding» выше). Осталось: recovery при зависании + TODO (двери/лестницы/…).
- **Phase 6 (готово, `bee16b7`)**: стойка (`dmBotIntent_Stance` + канал) и состояние
  `Stealth` (укрытие: crouch → move → prone → dwell), `/fsm add stealth`,
  `/bot intent stance`.

## Критерии приёмки ядра

- Переходы разнообразны (частоты ≈ веса, не всегда первый в списке).
- Блокировка через `CanEnter` / `BlockWhen` / `Require` работает.
- Новое состояние = новый класс; новый пресет = новая фабрика; ядро не трогается.

## Коммиты (что подчерпнуть)

- `5043847` "Custom-command Phase 3: speed ramp, turn-slow, comfort body-orientation policy" —
  сглаживание движения + политика ориентации корпуса. Уроки:
  - **Три независимых сглаживания** (скорость / угловая скорость / целевой угол корпуса)
    нельзя смешивать: рамп скорости (`m_ActualSpeed` → `m_DesiredSpeed`, `DM_MOVE_ACCEL_RATE`)
    и доворот-с-притормаживанием (`m_TurnSharp` → кэп скорости `DM_MOVE_TURN_SLOW_SPEED`) —
    это про **скорость**; политика комфорта (`ComputeBodyYaw`) — про **куда смотрит корпус**.
  - **Политика комфорта = одна функция** `ComputeBodyYaw(headTarget)`: `FULL` → корпус к взгляду
    (страйф/задом), `движусь` → корпус к `m_MoveYaw` (направление движения), `идл+AUTO` → через
    плечо, `идл+NONE` → корпус держится. `MoveTo` теперь задаёт тело через `SetMoveYaw`, а голову —
    `LookAtPoint(…, NONE)` — взгляд и тело окончательно развязаны.
  - **Рамп скорости — в пешке (применение)**, целевая скорость — в мозге (`CalcSpeed`): траит
    «насколько резво ускоряется» ляжет позже в рамп-параметр, не трогая интенты.
  - **Тест-сценарии как команда** (`/bot testcase patrol`) — быстрый способ гонять маршруты с
    поворотами без ручной настройки точек/FSM.
- `309fd3b` "Custom-command Phase 2: consolidate movement application into the pawn" —
  консолидация применения движения в пешке. Уроки:
  - **Мозг — решения, пешка — применение**: весь `OverrideMovement*`/`ForceStance`/`SetOrientation`
    переехал в `dmAISurvivorBase.CommandHandler`; мозг пишет только «чего хочу» (`SetMove`/
    `SetStance`/`SetTargetBodyYaw`). Шов — публичные примитивы мозга, интенты/FSM не тронуты.
  - **`ONE_FRAME` вместо `ENABLED`** для движения: override применяется на один `CommandHandler`
    и сам сбрасывается — «залипшее» состояние движения архитектурно невозможно.
  - **stop-turn-walk удалён**: после отключения `HeadingModel` бот доворачивается на ходу,
    стоп-поворот не нужен. `ForceStance` из `CommandHandler` (после super) — правильная фаза.
  - **Замечено (в техдолг)**: развороты на высокой скорости выглядят неестественно — будущая
    «политика комфорта» (Фаза 3) должна ограничивать угловую скорость/скорость в довороте.
- `7596ade` "Movement fixes + custom-command Phase 1: heading override, look direction" —
  доворот корпуса (Фаза 1 кастомного команд) + фикс направления взгляда. Уроки:
  - **Главное: ванильный move-command сам крутит корпус** через `HeadingModel` →
    `RotateOrient`, перетирая наш `SetOrientation`. Отключение — `override HeadingModel`:
    для `COMMANDID_MOVE` ставим `m_fHeadingAngle = m_fOrientationAngle = текущий угол`
    (рад), `return true`. После этого `SetOrientation` авторитетен.
  - **Переступание работает во ВСЕХ стойках**: граф `TurnStanceSTM` по `Stance` выбирает
    поворот (стоя — шаг, присед — шаг, лёжа — перекат), а `*TurnNBlend` уже читают
    `dmAI_TurnAmount`. Не нужен слайд для приседа/лёжи — на идле переступание, слайд
    только на ходу.
  - **Направление камеры на сервере** = кость головы: `GetBoneTransformWS("Head")` →
    `transform[1]` = forward (с питчем), позиция `GetBonePositionWS`. `GetHeadingVector`
    — это корпус (без питча), точка «выше/дальше» прицела.
  - **«Движется ли бот» — наше состояние** (`m_IsMoving` на пешке из `SetMove`), а не
    `GetCurrentMovementSpeed()` (он ненулевой во время переступания → отмена поворота).
- `e3ea7f3` "Speed, stop-turn-walk, intent auto-deadline" — скорость как вычисление
  мозга + доворот + жизненный цикл интентов. Уроки:
  - **Скорость изолирована в мозге** (`CalcSpeed(toPoint, deadline)`): интенты не хардкодят
    скорость, а зовут примитив — задел под характер (торопливость/страх/риск).
  - **`max(предпочтительная, по дедлайну)`**: без дедлайна бот идёт в своём темпе, при
    жёстком дедлайне ускоряется до минимально нужной (`required = dist/deadline`).
  - **Слайд-поворот на ходу не работает** (`SetOrientation` после `super.CommandHandler()`
    перетирается heading model) — быстрый фикс **stop-turn-walk**: стоп + идл-переступание
    доворачивает корпус (`DM_MOVE_FACE_THRESHOLD`), прогресс-монитор паузится.
  - **Ни один интент не живёт вечно**: `IsExpired()` удаляет бездедлайновый интент старше
    `DM_INTENT_MAX_AGE` (5 мин); `/bot intent`-приказы получили дедлайн 1 мин — иначе
    command-интенты копятся и перебивают FSM.
- `bee16b7` "Stance + Stealth: posture intent, stance channel, cover state" —
  стойка как интент + состояние укрытия. Уроки:
  - **Стойка — дискретный канал, не пер-тик контроль**: `ForceStance` зовём только при
    смене `m_DesiredStance` (иначе дёргается). Сброс в `ERECT` каждый арбитраж — это и
    есть «фоновое намерение встать»: убрали stance-интент → бот встаёт сам, без `OnCancel`.
  - **`erect↔prone` — только через crouch** (иначе «ломается привязка к поверхности и
    хитбокс»), таймауты `DM_STANCE_TIMEOUT_CROUCH/PRONE` (реф: Expansion `eAICommandMove`).
  - **Текущая стойка**: `GetMovementState(state)` → `state.m_iStanceIdx` (нормализовать
    вычитанием `STANCEIDX_RAISED`).
  - **Один stance-интент на состояние проще двух**: фаза меняет `m_Stance` интента
    (crouch → prone), а не добавляет второй интент (не полагаемся на порядок в пуле).
  - Состояние-владелец держит `ref` на свои интенты (`m_Move`, `m_StanceIntent`) и
    реагирует на `IsFinished()`/`IsFailed()`.
- `ef66b60` "Movement + pathfinding: decouple body/movement, progress monitor, navmesh paths" —
  развязка взгляда и движения + прогресс-монитор + pathfinding. Уроки:
  - **Движение относительно корпуса**: `OverrideMovementAngle(angle)` задаёт направление
    ДВИЖЕНИЯ относительно тела (-180..180: 0 вперёд, ±90 страйф, ±180 назад), а тело
    крутится отдельно (слайд-поворот `SetOrientation` на ходу, переступание на идле).
    Это и есть «пятится/страфит, глядя в другую сторону» — как Expansion `eAICommandMove`.
  - **Прогресс-монитор интента**: «цель удаляется» (рост дистанции) НЕ ловит «застрял
    в углу»; правильный детект — «нет прогресса» (дистанция не уменьшилась на
    `DM_MOVE_PROGRESS_EPS` за `DM_MOVE_STUCK_TIME`).
  - **Интент — источник истины «достигнута точка»**: `Patrol` не должен сам мерить
    дистанцию (3D vs 2D расходятся из-за высоты) — реагировать на `IsFinished()`/`IsFailed()`.
  - **`AIWorld.FindPath` + `PGFilter`**: путь — массив waypoints; `PGFilter` (new-объект)
    держать через `ref`, а `AIWorld` (движковый синглтон) — БЕЗ `ref` (`~AIWorld` private).
  - **Нет пути → `Fail()` сразу**, а не fallback «по прямой»; на «застрял» — пересчёт
    пути один раз, потом `Fail()`.
- `93f4907` "FSM: Idle/Patrol states, orientation intents, hybrid preemptive transitions" —
  состояния Idle/Patrol, интенты ориентации, гибридная модель переходов. Уроки:
  - **Кооперативная vs вытесняющая модель**: переход по `EXIT` (состояние «доделало»)
    + вытеснение (`INTERRUPTIBLE` прерывается `PREEMPTIVE` раз в интервал). Гибрид
    покрывает и «закончил патруль → idle», и «увидел врага → бой».
  - **Взаимоисключение через enum** (`dmBotStateKind`): запрет «прерываемое +
    вытесняющее» одновременно убивает дрожание переходов.
  - **`Start()` не проверяет `CanEnter()`**, дефолт = первое добавленное состояние →
    порядок `/fsm add` задаёт старт; данные стартового состояния готовь до apply.
  - **Два состояния → детерминированный цикл** (один исходящий переход), случайность
    только при 3+ вариантах.
  - **Состояние, которое никогда не `EXIT`, застревает в FSM навсегда** — Idle так
    и «не двигался» в тесте, пока не добавили `EXIT` по таймеру.
- `a4a3180` "Test commands: /bot intent lookAt|lookAtMe|goto|clear" — тест-команды
  намерений. Уроки:
  - `GetLookDirection()`/`GetAimDirection()` — это Expansion, НЕ ванилла (ошибка
    `Undefined function 'PlayerBase.GetLookDirection'`). Ванильный способ —
    `MiscGameplayFunctions.GetHeadingVector(PlayerBase)` + `DayZPhysics.RaycastRV`
    (см. `codeguide.md`).
  - Спавн сам ничего не делает: ни FSM, ни «залипания» на игроке — только привязка
    бота к игроку (`s_TestBotByPlayer`); поведение задаётся intent-командами.
