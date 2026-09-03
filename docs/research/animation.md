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
