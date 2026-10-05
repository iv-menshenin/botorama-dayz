# Animation — enfAnimSys / graph / skeleton (research)

Цель: понять, как DayZ резолвит скелет/воркспейс для `enfAnimSys`, как связаны
`.agr/.ast/.asy/.asi`, правильно ли botorama подключает кастомный граф, что означают
клиентские ошибки `skeletons.anim.xml` / `player_main.asi 3`, и что вызывает
«микро-тремор» бота.

Источники:
- Ванильные данные игры: `/home/devalio/dayz/Work/DayZ Projects/DZ/...` (полный дамп PBO:
  `config.cpp`, `.agr/.ast/.aw/.asy/.asi`, `skeletons.anim.xml`).
- Скрипт-дифф: `/home/devalio/dayz/Work/DayZ-Script-Diff/scripts`.
- Expansion: `/home/devalio/dayz/Work/DayZ-Expansion-Scripts`.
- Клиентский лог: `/home/devalio/dayz/Work/botorama/clientLog/DayZ_x64_2026-09-03_08-52-08.RPT`.
- Серверный лог: `/home/devalio/dayz/Work/botorama/serverLog/script_2026-09-03_08-51-56.log`.
- Мод: `/home/devalio/dayz/Work/botorama/config.cpp`, `Animations/*.agr`,
  `core/4_World/Entities/Bot/dmAISurvivorBase.c`.

---

## enfAnimSys / graphName / skeleton

### Поля конфиг-класса `enfAnimSys` (подтверждено, ваниль)

Ванильный игрок (`DZ/characters/data/config.cpp:153-160`, класс объявлен в
`SurvivorBase` — строка 122 `class SurvivorBase: Man`):

```cpp
class enfAnimSys
{
    meshObject="dz\characters\bodies\player_testing.xob";
    graphName="dz\anims\workspaces\player\player_main\player_main.agr";
    defaultInstance="dz\anims\workspaces\player\player_main\player_main.asi";
    skeletonName="player_testing.xob";
    startNode="MasterControl";
};
```

Полный набор полей (5 штук), подтверждён и на животных
(`DZ/animals/sus_scrofa/config.cpp:278-285`, там класс записан как `enfanimsys`
в нижнем регистре, но поля те же):

| Поле | Значение (игрок) | Смысл |
|------|------------------|-------|
| `meshObject` | `dz\characters\bodies\player_testing.xob` | скелетный меш `.xob` |
| `graphName` | `dz\anims\workspaces\player\player_main\player_main.agr` | анимационный граф |
| `defaultInstance` | `dz\anims\workspaces\player\player_main\player_main.asi` | дефолтный AnimSetInstance (без оружия) |
| `skeletonName` | `player_testing.xob` | имя скелета → ключ в `skeletons.anim.xml` |
| `startNode` | `MasterControl` | стартовый узел графа |

Рядом лежит **легаси-заглушка** `class AnimSystem { meshObject="INVALID MESH OBJET NAME"; animGraph="INVALID ANIMGRAPH NAME"; }` (`config.cpp:161-165`) — не используется.

**Важно:** полей `m_dof`, `m_fDistance`, `skeletonBones`, `CfgModels`, `CfgSkeletons`
в `enfAnimSys` НЕТ. `m_fDistance`/`m_dof` встречаются только в камерах
(`4_world/entities/manbase/dayzplayer/dayzplayercamera3rdperson.c` и др.) — это
камерные поля, к анимации отношения не имеют. Искать «настройки скелета» в
`enfAnimSys` не нужно — там их нет.

### Как `skeletonName` резолвится в `skeletons.anim.xml`

- `skeletonName="player_testing.xob"` — это имя скелета; оно должно совпадать с
  `<skeleton name="player_testing.xob">` внутри файла скелетов.
- Файл скелетов лежит в отдельном аддоне **`DZ_Anims_Cfg`** с префиксом `DZ\anims\cfg\`
  (файл `DZ/anims_cfg.txt`: `prefix=DZ\anims\cfg\`), путь `DZ/anims/cfg/skeletons.anim.xml`.
  Запись игрока — строка 110: `<skeleton name="player_testing.xob">` (всего в файле
  17 скелетов: игрок, заражённые, все животные).
- Само имя скелета (`player_testing.xob`) совпадает с `meshObject`/`defaultInstance` —
  это один и тот же `.xob` скелет.

### Откуда берётся путь `@Botorama/Anims/cfg/skeletons.anim.xml`

Это **фиксированный пробный путь движка**, а не путь из конфига. В клиентском логе:

```
8:52:14.130 ANIMATION (E): Can't load Z:\...\@Botorama/Anims/cfg/skeletons.anim.xml
8:52:14.130 ANIMATION (E): Can't load sakhal/Anims/cfg/skeletons.anim.xml
```

Движок на старте для **каждого загруженного модуля** (мира + каждого мода) пробует
загрузить `<корень-модуля>/Anims/cfg/skeletons.anim.xml`, чтобы модуль мог добавить
свои скелеты. В дампе игры существует ровно один `skeletons.anim.xml` —
`DZ/anims/cfg/skeletons.anim.xml`; ни `sakhal/Anims/cfg/`, ни `@Botorama/Anims/cfg/`
не существуют. Обе строки — **штатные нефатальные пробы** (движок молча падает на
загруженный `DZ\anims\cfg\skeletons.anim.xml`). Это НЕ признак неверной настройки и
не ломает скелет: `skeletonName` наследуется ванильный и успешно резолвится.

---

## .agr / .ast / .asy / .asi

Все файлы в дампе — текстовые (в реальном PBO `.asi`/`.anm` бинарные; здесь исходный
формат воркбенча). Структура:

| Расширение | Заголовок | Что это |
|-----------|-----------|---------|
| `.agr` | `$AnimGraph 7 {` | анимационный **граф**: команды `$Commands`, переменные `$Vars`, бленд-узлы/транзишены, подграфы `$Files` |
| `.ast` | `$animsettemplate {` | **AnimSetTemplate** — списки групп/имен анимаций (скелетные имена, не пути) |
| `.aw` | `$animWorkspace {` | **воркспейс** (только для редактора): ссылка `#animSetTemplate` + перечень всех `.asi` |
| `.asy` | `$synctable {` | **AnimSyncTable** — маркеры шагов (`RFootDown`, `LFootUp`…) для синхронизации походки |
| `.asi` | `$animsetinstance {` | **AnimSetInstance** — маппинг имени анимации на путь `.anm` (`DZ/anims/anm/player/...`) |
| `.anm` | бинарный | сами скелетные анимации (внутри `.asi` ссылаются на `DZ/anims/anm/...`) |

`.abt` (бленд-три) в дампе player_main НЕ встречается — бленды записаны прямо в `.agr`
(узлы `AnimNodeBlendT`/`...BlendT2`, `#BlendSpace`-подобные `...Var`-узлы).

### Связь цепочки (граф → шаблон → инстанс)

`enfAnimSys.graphName` → `.agr` (`$AnimGraph`):
- `#AnimSetTemplate "{GUID}...\player_main.ast"` — шаблон набора анимаций;
- `#AnimSyncTable "{GUID}...\Player_SyncTable.asy"` — таблица синхронизации шагов;
- `$Files { ... }` — подграфы (Locomotion/Combat/Weapons/Gestures/Vehicles/Actions/Master/Tests).
`enfAnimSys.defaultInstance` → `.asi` (`$animsetinstance`), который внутри ссылается на
тот же `#template "{GUID}...player_main.ast"`. `.aw` на рантайме не используется (только
воркбенч), поэтому мод не обязан его паковать.

### Как резолвятся относительные пути (GUID-префиксы)

Пути в `.agr`/`.asi`/`.ast` имеют вид `{GUID}относительный\путь`. GUID — это «владелец»
(привязка к PBO-префиксу, зарегистрированному при сборке). BOTORAMA переиспользует
**ванильные** GUID и пути:

```
#AnimSetTemplate "{F0C651DE8E24A5DE}DZ/anims/workspaces/player/player_main/player_main.ast"   (botorama player_main.agr:2)
#AnimSyncTable  "{8F89313B7FBB0B66}DZ/anims/workspaces/player/Player_SyncTable.asy"           (botorama player_main.agr:3)
```

Совпадают с ванильными `player_main.agr:2-3` и `player_main.aw:2`. Поэтому подграфы
`DZ/anims/workspaces/player/player_main/{Combat,Weapons,Gestures,Vehicles,Master}.agr`
(botorama `player_main.agr:551-555`) резолвятся в ванильный аддон `DZ_Anims_Workspaces`
(префикс `DZ\anims\workspaces\`) — в логе НЕТ ошибок их загрузки. Свои файлы botorama
ссылает без GUID (`botorama/Animations/{Locomotion,Actions,Tests}.agr`) — резолвятся
относительно собственного префикса мода `botorama\`.

---

## Как правильно подключить кастомный граф (рекомендация)

### Что делает Expansion (референс)

`DayZExpansion/Animations/AI/config.cpp:50-56`:

```cpp
class eAI_SurvivorM_Mirek: SurvivorM_Mirek
{
    scope=0;
    class enfAnimSys: enfAnimSys
    {
        graphName="DayZExpansion\Animations\AI\player_main.agr";
    };
};
```

`requiredAddons` (строки 8-13): `DZ_Anims_Anm_Player`, `DZ_Anims_Cfg`, `DZ_Characters`.
Больше НИЧЕГО: ни `skeletonName`, ни `defaultInstance`, ни `meshObject`, ни отдельного
`skeletons.anim.xml` в репо Expansion нет. Сами `.agr` в репо отсутствуют (скачиваются
отдельным PBO), поэтому содержимое графа Expansion сравнить нельзя — но паттерн
**идентичен botorama**: только `graphName`, остальное наследуется от `SurvivorBase`.

### Вывод по botorama

Текущая связка в `botorama/config.cpp:64-87` — **корректная** и полностью повторяет
Expansion:

```cpp
class dmAI_SurvivorM_Mirek : SurvivorM_Mirek
{
    scope = 2;
    class enfAnimSys : enfAnimSys
    {
        graphName = "botorama\Animations\player_main.agr";
    };
};
```

`class enfAnimSys : enfAnimSys` наследует от родительского (из `SurvivorBase`) поля
`meshObject`, `defaultInstance`, `skeletonName`, `startNode`, переопределяя только
`graphName`. Это правильный механизм: скелет остаётся ванильным
(`skeletonName="player_testing.xob"` → `DZ/anims/cfg/skeletons.anim.xml`), воркспейсы —
ванильные (через GUID), меняется только граф.

**Что можно поправить (опционально, не критично):**
1. `requiredAddons` — оставить как есть (совпадает с Expansion). Явно дописывать
   `DZ_Anims_Workspaces` НЕ обязательно (тянется транзитивно через `DZ_Data` →
   `DZ_Characters`), но можно для наглядности.
2. **НЕ** надо копировать `skeletons.anim.xml` в `botorama/Anims/cfg/` — ошибка в логе
   нефатальна, и добавление своего файла только создаст второй источник истины для
   скелета. Ничего не менять.
3. `defaultInstance` трогать не нужно — он ванильный и валиден, т.к. кастомный граф
   использует тот же ванильный `.ast`.
4. `scope = 2` vs `scope = 0`: у Expansion `scope=0` (скрытые классы, спавн только из
   кода). У botorama `scope=2` — классы видимы в редакторе/спавн-листах; если не нужно,
   можно опустить до 0, но на анимацию это не влияет.

---

## Коды ошибок клиента

### `RESOURCES (E): dz/anims/workspaces/player/player_main/player_main.asi   3`

Это **НЕ код ошибки**. Строка стоит в секции завершения лога (clientLog:525-526):

```
8:54:19.199 RESOURCES (E): ==== Resource leaks ====
8:54:19.199 RESOURCES (E): dz/anims/workspaces/player/player_main/player_main.asi   3
```

Цифра `3` — **счётчик утёкших ссылок** ресурса на момент выгрузки (сколько раз ресурс
был загружен и не отпущен). `player_main.asi` — ванильный дефолтный инстанс
(`defaultInstance`), загружается нормально, на выгрузке остаются 3 висячие ссылки.
Безобидно (диагностика шатдауна), к тремору отношения не имеет.

### `ANIMATION (E): Can't load .../Anims/cfg/skeletons.anim.xml` + `Failed to open file`

Нефатальная проба движка (см. выше). Появляется и для ванильного мира `sakhal`, т.е.
это штатное поведение при каждом запуске. НЕ признак поломки скелета/воркспейса.

### Чего в логе НЕТ (важно)

Нет ошибок вида «failed to load graph / .ast / .asy / bone not found / failed to bind
variable» — т.е. кастомный граф, ванильные подграфы, шаблон, таблица синхронизации и
все `dmAI_*` переменные/команды загрузились и связались успешно.

---

## Вывод по микро-тремору

### Скелет/воркспейс/граф — НЕ виноваты

1. **Бленд локомоции не тронут.** `botorama/Animations/Locomotion.agr` (9044 строки) и
   `Actions.agr` (7648 строк) имеют **ровно ту же длину, что и ванильные**, и
   `diff` показывает только переименования входных переменных/команд:
   `Look`→`dmAI_Look`, `Raised`→`dmAI_Raised`, `TurnAmount`→`dmAI_TurnAmount`,
   `CMD_Turn`→`dmAI_Turn`, `CMD_StopTurn`→`dmAI_StopTurn`, `AimX/Y/AimIKX`→`dmAI_*`
   (386 отличающихся строк в Locomotion, все — рейнеймы). Ключевые узлы
   `WalkRunSprintBlendT` (строка 6365), `WalkBlendSTM` (6237), `SprintCyclic` (4276),
   `RunSprintBlendF` (3924) на тех же строках и с теми же формулами
   `clamp((MovementSpeed-2)*(1-Injured), 0.0, 1.0)`. Т.е. смешение walk/jog/sprint —
   **байт-в-байт ванильное**.
2. **Скелет резолвится корректно** (ошибка `skeletons.anim.xml` — доброкачественная проба).
3. Граф загружается без ошибок (в логе нет «failed to load» для графа/`.ast`/`.asy`).

### Наиболее вероятная причина — джиттер ВХОДНЫХ переменных, а не граф

Серверный лог `[MOV]` (включён `DM_BOT_DEBUG_PERFRAME_MOVING_LOG`) показывает, что при
ровной позиции и скорости **угол движения осциллирует**:

- спринт (08:53:09, `desired=2 actual=2`): `angle` прыгает
  `2.17 → 1.71 → 1.71 → 2.33 → 0.64 → 1.13 → 0.11 → -0.03 → -1.50` (джиттер ±1–2°/кадр);
- бег (08:53:16, `desired=1`): `angle` `0 → 0.006 → 0.009 → 0.014 → 0.018 → 0.053` (нарастающий микро-джиттер).

`angle` здесь — это `m_DesiredMoveAngle`, который в
`ApplyMovement` (`dmAISurvivorBase.c:823`) подаётся через
`hic.OverrideMovementAngle(HumanInputControllerOverrideType.ONE_FRAME, m_DesiredMoveAngle)`
и в итоге становится анимационной переменной `MovementDirection`. Бленды локомоции
(`WalkBlendSTM`, `MovementLowVar`, `MovementRasVar`) читают `MovementDirection`
каждый кадр — осцилляция ±1–2° даёт видимое «подрагивание» туловища при движении/спринте.

Вторичный источник — **слайд-доворот `SetOrientation`** (`ApplyBodyTurn`,
`dmAISurvivorBase.c:762-766`): при движении тело доворачивается к `m_TargetBodyYaw`
ступенькой `DM_MOVE_TURN_RATE * pDt * DM_MOVE_TURN_SPEED`; в логе `bodyYaw` у цели
осциллирует (`... -88.5 → -88.5 → -90`), давая микро-довороты. Но это вторично:
`posDelta` в логе гладкий, т.е. **серверная позиция/скорость действительно ровные**
(совпадает с постановкой задачи), а «трясётся» именно направление (угол) и ориентация.

### Итоговая оценка

- Вероятность, что тремор вызван анимационной системой (сломанный скелет/воркспейс/бленд) — **низкая**: все компоненты загружены, бленд идентичен ванили.
- Вероятность, что тремор вызван джиттером входного сигнала (`MovementDirection` через `OverrideMovementAngle` + слайд-доворот `SetOrientation`) — **высокая**.

### Что проверить/исправить (в мод, не в `.agr`)

1. Найти источник джиттера `m_DesiredMoveAngle` в навигационном слое
   (`dmAISurvivor.c`: `SetMove(angle, speed)` вызывается из brain/FSM с пересчётом угла
   каждый кадр; угол к `m_TargetBodyYaw`/путёвой точке осциллирует). Это вопрос
   **navigation**, а не animation — свести в `docs/research/navigation.md`.
2. Добавить гистерезис/сглаживание/квантование угла: например, игнорировать изменения
   `|Δangle| < порог` (несколько градусов) и/или фильтровать `m_DesiredMoveAngle`
   (leaky integrator/скругление), чтобы `MovementDirection` не дёргался от кадра к кадру.
3. Для слайд-доворота — добавить мёртвую зону по `dBody` (уже есть порог `> 1.0`, но
   у цели сходится с осцилляцией) и/или сгладить шаг `SetOrientation`.

### Открытые вопросы

- Точная формула/источник `m_DesiredMoveAngle` в brain (какой FSM-интент и как считает
  угол) — не исследована (вне домена animation).
- Клиентская интерполяция net-synced `dmAI_Look*`/`dmAI_TurnAmount`: насколько клиент
  сам сглаживает эти переменные при репликации — не подтверждено (нативно это
  `RegisterNetSyncVariableFloat`, поведение интерполяции не задокументировано в
  скрипт-диффе).
- Содержимое `.agr` Expansion (`DayZExpansion\Animations\AI\player_main.agr`) недоступно
  (бинарный PBO вне репо) — прямое сравнение с botorama невозможно; сравнивали только
  `config.cpp`.

---

# Foot-step доворот корпуса: root motion vs визуал, и почему осциллирует ~25°

Цель: разобрать, почему foot-step-доворот (`dmAI_Turn`/`dmAI_TurnAmount`) у ИИ-бота
botorama осциллирует — тело поворачивается ~22° и отскакивает обратно, не доходит до
цели (`Turn: bodyYaw 0 → 21.92 → 0`, `state=1` зависает), из-за чего `HumanCommandMelee2`
делает «fast exit» без Hit-события.

Связанное уже известно: `combat.md` («Ванильный HeadingModel во время MELEE2 — крутит
корпус? ДА») и `decisions.md` («melee: кручение при ударе — глушить HeadingModel для
MELEE2»). Здесь — именно idle-ветка `ApplyBodyTurn` (foot-step), а не slide-turn.

## 1. Root motion или визуал — в графе ЕСТЬ root-motion-путь

### Структура foot-step-узла (подтверждено, file:line)

Кастомный граф botorama (`src/core/Animations/Locomotion.agr`) **идентичен ванильному**
кроме рейнеймов (см. выше «Бленд локомоции не тронут»); ваниль =
`DayZ Projects/DZ/anims/workspaces/player/player_main/locomotion.agr`.

- Бленд выбора позы `ErcTurnNBlend` (botorama `Locomotion.agr:7788-7800`): читает
  **`dmAI_TurnAmount`** (наш кастом), входы `-2.0/-1.0/0.0/1.0/2.0` →
  `ErcTurnL180/L90/Idle/R90/R180SyncAnim`. Ваниль на том же месте читает ванильный
  `TurnAmount` (ваниль `locomotion.agr:7788`).
- Сами turn-анимации — `AnimNodeSourceSync` с тегом `"Turn"` и **root-motion-декларацией**:
  ```
  "ErcTurnR180SyncAnim" "" "Locomotion.TurnR180" "noloop" "Turn"
  $Predictions { $Prediction { "PredictionTurn" "EntityPosition" "" 1.0 7 } }
  ```
  (botorama `Locomotion.agr:7801-7822`; то же для R90/L90/L180). `PredictionTurn` +
  `EntityPosition` — это и есть root motion: анимация вращает сущность (как `JumpPrediction`/
  `ClimbPrediction` для прыжков/клаймбов). Mode **7** у erect/crouch, mode **3** у prone
  (ваниль `locomotion.agr:5219-5354` — prone mode 3; `:7514-8210` — erect/crouch/prone mode 7).
- Нода `TurnVar` = `AnimNodeVarUpdate` (botorama `Locomotion.agr:8538`):
  ```
  "TurnVar" "" "TurnStanceSTM" 1 "TurnAmount" 7
  ```
  ПИШЕТ в **ванильный `TurnAmount`** (НЕ в `dmAI_TurnAmount`), mode **7** — совпадает с
  mode `PredictionTurn`. Т.е. `TurnVar` извлекает угол вращения из turn-анимации и пишет его
  в ванильный `TurnAmount`, который **нативный код читает и крутит корпус**.
- IdleStateMachine (botorama `Locomotion.agr:8030-8058`): состояния `Idles`/`TurnMaster`;
  переходы `Idles→TurnMaster` по `IsCommand(dmAI_Turn)` (`:8047`), `TurnMaster→Idles` по
  `GetLowerTime() > 0.85 || IsCommand(dmAI_StopTurn)` (`:8050`), `""→TurnMaster` по
  `IsCommand(dmAI_Turn)` (`:8053`). Ваниль — те же переходы с `CMD_Turn`/`CMD_StopTurn`
  (ваниль `locomotion.agr:8039-8053`).
- Команды/переменные объявлены в `player_main.agr`: `CMD_Turn`/`CMD_StopTurn` **оставлены**
  (`:68,70`), `dmAI_Turn`/`dmAI_StopTurn` добавлены (`:69,71`); `TurnAmount` оставлен (`:82`),
  `dmAI_TurnAmount` добавлен (`:83`).

### Вывод (1)

Root-motion-путь в графе **есть**: `PredictionTurn`+`EntityPosition` → `TurnVar` →
ванильный `TurnAmount` → натив крутит тело. Foot-step-поворот — НЕ «чисто визуальный» в
ванили. **НО** применяет ли натив этот root motion для server-side AI (`INSTANCETYPE_AI_SERVER`,
не имеющего native-инпута `CMD_Turn`) — не подтверждено (см. ниже «причина отскока»).

## 2. Почему отскок ~22° (точная причина — гипотезы, ранжированы)

Факт из лога: `bodyYaw` (это `GetOrientation()[0]`, т.е. физическая ориентация) мечется
`0 → 21.92 → 0`, `state=1` не сбрасывается между сэмплами (т.е. цикл «включил поворот →
через 2 с стоп → сразу снова включил», лог троттлится 2 с и ловит `state=1`).

Ключевой факт: `ApplyBodyTurn` idle-ветка (`dmAISurvivorBase.c:1430-1453`) ставит
`dmAI_TurnAmount` **один раз** на входе в turn и не «дожимает»; стоп по `m_TurnTime > 2.0`
ставит `dmAI_TurnAmount = 0` → бленд уходит в `Turn0` (нейтраль) → визуал (и/или
root-motion) откатывается к исходному яу.

Гипотезы (по убыванию вероятности):

- **(а) Root motion НЕ коммитится в `GetOrientation()` для server-AI — turn чисто визуальный.**
  Нативный `HumanCommandMove` (`3_game/human.c:433-504`) **не имеет** ни одного
  turn/`TurnAmount`-метода, доступного скрипту; `CMD_Turn`/`TurnAmount` в ванильных скриптах
  не встречаются вовсе (grep по `scripts/` — 0 вхождений). Т.е. foot-step-turn у ванильного
  игрока запускается **нативным инпутом** (`HumanInputController` при A/D стоя на месте),
  который сам ставит `CMD_Turn` + `TurnAmount` и сам применяет root motion. Наш `dmAI_Turn`
  (кастомная граф-команда) триггерит только **граф**-переход в `TurnMaster`, но НЕ нативный
  turn-режим → `TurnVar` пишет `TurnAmount`, но натив его не читает (нет turn-режима) →
  корпус не крутится. Видимая «22°» — частичное смещение от самой turn-анимации (или transient
  root-motion в один кадр), которое при выходе из turn откатывается. **[нужно подтвердить]**
  эмпирически: заспавнить бота, вызвать foot-step-turn на большой угол и залогировать
  `GetOrientation()` ПОКАДРОВО (не троттля) — если корпус не растёт монотонно к цели, а
  прыгает туда-сюда ±десятки градусов → root motion не применяется.
- **(б) Turn-анимация `noloop` — один шаг, выход по `GetLowerTime() > 0.85`, нет «дожима».**
  `ErcTurnR180SyncAnim` — `"noloop"`. Одна нога-ступенька вращает ~несколько десятков градусов;
  после неё `TurnMaster→Idles` (85% времени) сбрасывает в нейтраль. `ApplyBodyTurn` не
  перезапускает turn до `m_TurnTime > 2.0` → за 2 с происходит 1 шаг, потом откат. Это
  объясняет периодичность и малую амплитуду, но САМ откат на 0 объясняет только (а)/(в).
- **(в) Сброс `dmAI_TurnAmount` в 0 при стопе откатывает бленд в нейтраль.** `ResetActuation`/
  `ResetMotorActuation` (`dmAISurvivorBase.c:1309-1348`) зовутся только при `!CanAct()`/
  `inVehicle` — в мили-бою НЕ вызываются (бот живой, не в машине). `ApplyMovement`/`ApplyLookVars`
  turn-переменные не трогают. Так что сброс — только в самом `ApplyBodyTurn` при стопе
  (`:1447-1451`) и при переключении в slide-turn (`:1406-1413`). Т.е. гипотеза «сброс каждый
  кадр» НЕ подтверждается кодом; сброс — только на стопе (раз в 2 с).

## 3. Ванильный эталон (как игрок поворачивается стоя)

- Скриптовый `DayZPlayerImplement.HeadingModel` (`DayZ Projects/scripts/4_world/entities/
  dayzplayerimplement.c:1619-1700`) для обычного стоячего MOVE **не крутит корпус** шагово:
  он в конце падает в `DayZPlayerImplementHeading.RotateOrient` (`:1699`), который
  (`dayzplayerimplementheading.c:64-120`) — это **слайд-поворот** `m_fOrientationAngle` →
  `m_fHeadingAngle` и срабатывает только при разнице > `CONST_ROTLIMIT = π·0.95 ≈ 171°`.
  Т.е. у ванили «тело стоит на месте, но разворачивается» делается **двумя путями**:
  (1) нативный foot-step-turn (инпут `CMD_Turn`+`TurnAmount`, нативно, не в скриптах) для
  обычных доворотов; (2) `RotateOrient` (слайд) для разворота ~180°.
- Для MELEE2 ваниль падает в тот же `RotateOrient` (см. `combat.md` «HeadingModel во время
  MELEE2») — то есть во время удара корпус крутится **слайдом**, а не шагами.

## 4. Что чинить (ранжировано)

1. **[рекомендовано] Для мили-фейсинга (idle) использовать slide-turn, а не foot-step.**
   Тот же паттерн, что Expansion использует для MELEE2: `HeadingModel` override ставит
   `SetOrientation(Vector(heading, 0, 0))` (снап/слайд) — `eAIBase.c:10093-10102`; плюс
   `eAIMeleeCombat.Update` держит `m_AI.SetOrientation(m_AI.GetOrientation())` (`eAIMeleeCombat.c:217`),
   чтобы натив мили не крутил. Для botorama: в idle-ветке `ApplyBodyTurn` доворачивать
   `SetOrientation(bodyYaw + step)` с rate-limit (как уже делает slide-ветка `:1415-1421`),
   убрав foot-step для стоячего мили. Это «человеческий» замкнутый контур (меряем `dBody` →
   доворачиваем) и не зависит от нативного turn-режима. **[нужно проверить]** что slide-доворот
   стоя НЕ даёт «скольжения»/дёрганья и что `HeadingModel`-лок (`MOVE`+`MELEE2`) не перетирает
   `SetOrientation` (сейчас лок возвращает текущую ориентацию — должен быть совместим).
2. **[если foot-step принципиально нужен]** Сначала эмпирически подтвердить/опровергнуть (а):
   логировать `GetOrientation()` покадрово во время `dmAI_Turn`. Если root motion НЕ коммитится
   — foot-step для AI нерабочий без нативного turn-инпута, и его надо заменять на slide (п.1).
   Если коммитится, но откатывается — виноват (в): «дожимать» turn непрерывно (перезапускать
   `dmAI_Turn`/не сбрасывать `dmAI_TurnAmount` в 0 до достижения `|dBody| < 1°`), а не раз в 2 с.
3. **[минор]** Отличие от Expansion: Expansion `SetTurnAmount(diff/90.0)` НЕ клампит в ±2 и НЕ
   сбрасывает amount при стопе (`eAICommandMove.c:1967/1977` — только `CallStopTurn`); botorama
   клампит `Clamp(dBody/90, -2, 2)` и сбрасывает `dmAI_TurnAmount=0` при стопе. Для dBody≈177°
   это `1.975` vs `2.0` — на выбор R180 не влияет, но сброс в 0 на стопе — кандидат на откат (в).

## Источники (file:line)

- Мод: `src/core/4_World/Entities/Bot/dmAISurvivorBase.c:374-376` (бинды `dmAI_TurnAmount`/
  `dmAI_Turn`/`dmAI_StopTurn`), `:1354-1366` (HeadingModel override: MOVE+MELEE2 → текущая
  ориентация, `return true`), `:1382-1454` (`ApplyBodyTurn`: slide `:1400-1423`, idle foot-step
  `:1427-1453`), `:1309-1348` (`ResetActuation`/`ResetMotorActuation` — НЕ зовутся в мили-бою).
- Мод `src/core/Animations/player_main.agr:68-71` (команды), `:82-83` (`TurnAmount`/
  `dmAI_TurnAmount`); `Locomotion.agr:7788-7822` (ErcTurnNBlend + R180/R90 sync-anim с
  `PredictionTurn`/`EntityPosition` mode 7), `:8030-8058` (IdleStateMachine TurnMaster),
  `:8492-8542` (TurnStanceSTM + `TurnVar`→`TurnAmount` mode 7).
- Ваниль `DZ/anims/workspaces/player/player_main/locomotion.agr:7788` (ErcTurnNBlend читает
  `TurnAmount`), `:8039-8053` (TurnMaster с `CMD_Turn`/`CMD_StopTurn`), `:8538` (`TurnVar`),
  `:5219-5354`/`:7514-8210` (PredictionTurn mode 3/7).
- Ваниль `scripts/3_game/human.c:433-504` (`HumanCommandMove` — НЕТ turn-методов).
- Ваниль `scripts/4_world/entities/dayzplayerimplement.c:1619-1700` (HeadingModel→RotateOrient),
  `dayzplayerimplementheading.c:64-120` (`RotateOrient`, `CONST_ROTLIMIT=π·0.95`).
- Expansion `AI/.../Classes/Commands/eAICommandMove.c:1957-1983` (turn-state = копия нашей
  idle-ветки), `:1966-1967` (`CallTurn`+`SetTurnAmount(diff/90)`), `:2028-2043`
  (`Anim_SetFilteredHeading`→`SetOrientation`); `Core/.../ExpansionHumanST.c:88-89,110-112`
  (`CMD_eAI_Turn`/`eAI_TurnAmount`); `AI/.../eAIBase.c:10093-10102` (HeadingModel MELEE2 →
  `SetOrientation(heading)`), `:10135` (fallback `NoHeading`); `AI/.../eAIMeleeCombat.c:217`
  (`SetOrientation(GetOrientation())`).
