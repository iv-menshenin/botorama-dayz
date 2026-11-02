# Research: история «реверсивного спуска» по лестнице (reverse `FindPath`)

Статус: исследование git-истории + сверка с ванилью/Expansion. Код мода не менялся.
Дата: 2026-11-02.

## Цель

Выяснить, БЫЛ ЛИ «реверсивный спуск» — обратный `FindPath(цель → бот)` для поиска
лестницы при спуске с off-navmesh позиции (крыша/платформа без навмеша) — и куда он
делся, если был. Заказчик помнит, что при борьбе с «односторонней лестницей» пробовали
искать путь ОТ точки снизу ДО бота, находить лестницу и делать верхний конец лестницы
«точкой входа» для спуска.

## Резюме (ответ на главный вопрос)

**Обратного `FindPath(цель → бот)` в лестничной маршрутизации НИКОГДА не было.**
Обратный `FindPath` появлялся в git-истории ровно дважды, и оба раза — НЕ в лестничном
роутинге:

1. **`f19fdaa` (13.09 07:10) — debug-проба `ProbeReverse`** — `FindPath(m_Goal, botPos, ...)`
   внутри `DebugPathProbes` под `#ifdef DM_BOT_DEBUG_PATHFINDER`. Это ДИАГНОСТИКА, не
   маршрутизация: логировала «есть ли путь с земли до бота», но ни на что не влияла.
   Именно это, скорее всего, запомнилось как «реверсивный спуск».
2. **`8ac9ba8` (01.11 22:42) — `TryLeapOfFaith` (продакшн)** — `pf.FindPath(m_Goal, botPos, ...)`
   для **leap-of-faith** (сойти с НЕВЫСОКОГО края, fall < `HEALTH_HEIGHT_LOW`), а НЕ для
   лестницы. Явный гейт: `botPos[1] - groundY > DM_FALL_JUMP_BLOCK_HEIGHT (3.0) → return false`
   («спуск только по лестнице»).

Память заказчика — контаминация трёх механизмов: (а) debug-пробы `ProbeReverse`,
(б) старой лестничной маршрутизации `TryStartLadder` (находила лестницу через
`PhysicsGetFloorEntity` под ботом, а НЕ обратным FindPath), (в) leap-of-faith (обратный
FindPath, но для прыжков вниз, а не для лестниц).

Лестничный спуск реально чинился **другим путём**: segmented `FindRoute` (прямой
рекурсивный `FindPath`) + `near`/`far`-логика выбора конца лестницы на уровне бота
(`161a4c8`) + reverse **направления подъёма** на лестнице (`e12776c`, `m_Direction = -m_Direction`).
Ни в одном из них нет обратного `FindPath`.

## Хронология коммитов (лестница, Sep 12–13 + Nov 1)

| commit | дата | что |
|---|---|---|
| `67755a4` | 01.09 | Phase 1: `TryStartLadder` — floor-entity детект + взвешенный выбор лестницы (`2D² × ΔY`) |
| `4d42ca0` | 30.08 | `dmBotIntent_UseLadder` (approach→attach→climb→detach) + хук в MoveTo |
| `6cb07f3` | 12.09 20:18 | **ladder move down stuck**: `SetMove(180, 3.0)` при спуске vs `SetMove(0, 2.0)` при подъёме |
| `7be3bcc` | 13.09 06:16 | гейт: пропускать лестницу, если цель на том же этаже + бот у входа (`DM_LADDER_FLOOR_GAP=1.5`, `DM_LADDER_ENTRY_REACH=2.5`) |
| `b6fee53` | 13.09 06:17 | таймаут подхода к лестнице |
| `13381d0` | 13.09 06:47 | research: under-roof + Expansion ladder-routing (→ `navigation.md` §7–10) |
| `f19fdaa` | 13.09 07:10 | **debug-пробы**: `ProbePlatform`/`ProbeReverse`/`ProbeHeight`/`ProbeLadders` |
| `6c6cc4a` | 13.09 08:00 | сторона выхода лестницы (`_con_dir`) |
| `f94b546` | 13.09 09:13 | **segmented `FindRoute`** (рекурсивный лестничный поиск через navmesh-гэпы); УДАЛИЛ `TryStartLadder` |
| `4d95f22` | 13.09 10:38 | выбор кратчайшего лестничного маршрута (2D длина + высота лестницы) |
| `e12776c` | 13.09 10:53 | stuck по no-progress + **reverse направления БЕЗ слезания** (`m_Direction=-m_Direction`) |
| `862765f` | 13.09 13:27 | фолбэк поиска здания лестницы ОТ БОТА (`FindLadderBuildingAt(from)`) + step-back fall-safe |
| `161a4c8` | 13.09 15:23 | **near/far + level gate**: лезть с того конца лестницы, что на уровне бота (`ladderDir=±1`) |
| `d271897` | 13.09 15:24 | подъём на спринте (скорость 3.0 вместо 2.0) |
| `8ac9ba8` | 01.11 22:42 | **leap-of-faith** — обратный `FindPath(goal→bot)` для спуска с оторванного navmesh-острова (малый перепад) |
| `1d65ab7` | 01.11 22:42 | docs: решения + техдолг leap-of-faith |

## Что именно делал `6cb07f3` «ladder move down stuck»

7 строк в `dmBotIntent_UseLadder.c`, в фазе подъёма/спуска (тогда ещё до реворка фаз):

```c
if ( m_Direction < 0 ) { bot.SetMove(180.0, 3.0); } else { bot.SetMove(0.0, 2.0); }
```

Корень бага: бот **стоял на месте** — направление движения не подавалось в нативный
`HumanCommandLadder` (вперёд=вверх, назад=вниз). Фикс подал движение: назад (`180°`) для
спуска, вперёд для подъёма. Это НЕ про маршрутизацию и НЕ про поиск лестницы — про
подачу движения. **Механизм пережил все рефакторинги** и сегодня живёт в
`dmBotIntent_UseLadder.c:175-182` как `bot.IntentMove(180.0, 3.0)` / `IntentMove(0.0, 3.0)`
(скорость подъёма поднята 2.0→3.0 в `d271897`).

## Как сегодня обрабатывается «бот стартует ВНЕ navmesh»

`dmBotPathfinder.FindRoute` → `FindRouteRecursive` (`dmBotPathfinder.c:196`):

1. `SamplePosition(to, DM_PATH_SAMPLE_RADIUS=2.0, sampledTo)` — снап ЦЕЛИ.
2. `m_AIWorld.FindPath(from, sampledTo, m_Filter, path)` — **`from` = `GetPosition()`
   бота БЕЗ снапа** (снапится нативом). Если бот вне навмеша (крыша/платформа без
   навмеша) — `FindPath` возвращает `false`/пусто → `return false` **сразу**.
3. Лестничный поиск (`FindLadderBuildingAt` + перебор лестниц + рекурсия) запускается
   ТОЛЬКО если шаг 2 дал путь, у которого последний waypoint на другой высоте
   (`|last[1]-sampledTo[1]| > DM_NAV_GAP=1.5`).

**Вывод**: кейс «бот на off-navmesh крыше» сегодня НЕ попадает в лестничную логику
вообще — `FindRoute` падает на шаге 2, и `RePath` уходит в фолбэки:
`TryLeapOfFaith` (только малый перепад, заблокирован выше 3 м) → `HasDropAhead`
(step-back) → `NO PATH` (аборт). Лестница при этом НЕ ищется — в отличие от старого
`TryStartLadder`, который искал лестницу здания под ботом через `PhysicsGetFloorEntity`
и мог стартовать подъём/спуск **даже без navmesh-пути от бота**.

Причина регресса: при переходе на `f94b546` старый floor-entity детект (`PhysicsGetFloorEntity`)
заменили на `FindLadderBuildingAt` (raycast вниз/вверх), но вызов `FindLadderBuildingAt(from)`
(`862765f`) стоит ПОСЛЕ успешного `FindPath(from, ...)` — то есть как фолбэк к
`FindLadderBuildingAt(sampledTo)`, а не как самостоятельный вход для off-navmesh бота.
Первый же `FindPath(from, sampledTo)` для off-navmesh `from` обрывает рекурсию раньше.

## Семантика `SampleNavmeshPosition` (ваниль)

Сигнатура (`3_game/ai/aiworld.c:122`):

```c
proto native bool SampleNavmeshPosition(vector position, float maxDistance, PGFilter pgFilter, out vector sampledPosition);
```

Дока (`:118-120`): «Finds closest point on navmesh **within maxDistance radius** using
path graph filter». `false` — «failed to find position on navmesh within given radius,
output param is left intact».

`FindPath` (`:98`): «waypoints array including starting and ending position», `true` —
если путь найден. `RaycastNavMesh` (`:110`) ловит **рёбра** навмеша, не плоскую поверхность.

**Радиус 2D vs 3D — НЕ подтверждён из скриптов** (это натив; дока говорит просто
«radius», Expansion везде использует `maxDistance=1.0` для снапа цели — см.
`ExpansionPathHandler.c:615,633` и `OverrideTargetPosition(..., 1.0, ...)` в
`eAIBase.c:4742`). Для точки на 13 м выше навмеша:

- гипотеза A (радиус = 3D-сфера): radius 2 м → `false` (ближайшая навмеш-точка дальше
  2 м); radius 20 м → `true`, `sampled[1] ≈ -13 м` (пол под крышей, «прижало вниз»).
- гипотеза B (радиус = XZ-горизонталь): radius 2 м → `true` (точка прямо над полигоном),
  `sampled` на земле под ботом.

Это различие критично для фикса: при A снап цели с крыши «проваливается» на этаж под
крышей ещё ДО `FindPath` (совпадает с уже зафиксированной гипотезой в
`navigation.md` §10.1), при B — крыша снапается сама, и проблема только в том, что
`from` (бот) вне навмеша. См. гипотезы ниже.

## Вывод: что реально нужно для фикса «спуск с off-navmesh платформы по лестнице»

1. **Вернуть «вход с бота» в лестничную маршрутизацию.** Сегодня `FindRouteRecursive`
   падает на `FindPath(from, sampledTo)` при off-navmesh `from` ДО того, как
   `FindLadderBuildingAt(from)` (`862765f`) мог бы сработать. Нужен отдельный вход:
   если `FindPath(from, ...)` пуст ИЛИ `from` вне навмеша (`SamplePosition(from, r)` =
   false / большой ΔY), то искать здание лестницы ПОД/НАД ботом
   (`FindLadderBuildingAt(from)`) и строить лестничный сегмент напрямую от ближайшего
   конца лестницы — аналог старого `TryStartLadder` (`PhysicsGetFloorEntity` → выбор
   entry на уровне бота), но в segmented-форме.
2. **Разделить «лестничный спуск» и «leap-of-faith».** `TryLeapOfFaith` уже корректно
   заблокирован выше 3 м; для высокой крыши нужен именно лестничный путь (шаг 1), а не
   прыжок. Это ровно то, что заказчик описывал как «реверсивный спуск» — но механика
   лестничного спуска должна быть прямой (найти лестницу под ботом → лезть вниз), а не
   обратным FindPath (обратный FindPath — правильный инструмент именно для
   leap-of-faith/малого перепада, где навмеш-остров оторван и надо найти точку схода).
3. **Не переизобретать обратный FindPath для лестницы.** Он не нужен: у лестницы есть
   дискретные концы (`m_Bottom`/`m_Top`), которые и так известны из `dmBotLadderCache`.
   Обратный FindPath дал бы «ближайшую точку земли», но не «какая лестница ведёт вниз».

## Гипотезы для эмпирической проверки (route → dayz-tester)

- `[нужно проверить]` **Семантика радиуса `SampleNavmeshPosition` (2D vs 3D).**
  Вопрос: для точки на ~13 м выше навмеша (крыша) `SampleNavmeshPosition(point, 2.0)`
  вернёт `false` или `true`? А при `radius=20.0`? Замер `sampled[1]-point[1]` покажет,
  «прижало вниз» или нет. Нужна probe-ручка `navmeshsample` (сейчас ЕЁ НЕТ — есть
  `spawnobj`/`raycast`/`scanbox`/`botdump`/`getpos`/`surfprobe`, но не
  `SampleNavmeshPosition`; gap → `dayz-dev` расширить `dmE2EBridge`).
- `[нужно подтвердить]` **`FindPath` снапит `from` сам или требует точку на навмеше?**
  Вопрос: `FindPath(offNavmeshBotPos, groundPoint, filter)` для бота на крыше без
  навмеша — вернёт `false` (пусто) или частичный путь от ближайшей достижимой точки?
  Это определяет, где именно падает `FindRouteRecursive` (комментарий в
  `TryLeapOfFaith.c:1586-1589` уже утверждает «FindPath сам снипает концы и при
  оторванном острове возвращает частичный путь» — надо подтвердить на реальной крыше).
- `[нужно проверить]` **Даёт ли `PhysicsGetFloorEntity` здание лестницы, когда бот на
  крыше?** Старый `TryStartLadder` полагался на это; после `f94b546` заменён на raycast.
  Вопрос: на off-navmesh крыше `PhysicsGetFloorEntity()` возвращает `Building` с
  лестницей (тогда это дешёвый вход для спуска) или `null`?

## Развилки / решения (для docs/decisions.md, предлагает субагент)

- **Выбор: лестничный спуск с off-navmesh позиции — прямой поиск лестницы под ботом, а
  НЕ обратный `FindPath(цель→бот)`.** Обратный FindPath закреплён только за
  leap-of-faith (малый перепад). Для лестницы концы дискретны и известны — обратный
  FindPath избыточен. (Действует пока: лестницы дискретны и кэшированы; если появится
  «платформа без лестницы, только край» — тогда leap-of-faith, уже есть.)
- **Регресс при `f94b546`:** замена `PhysicsGetFloorEntity` на raycast убрала возможность
  стартовать лестницу с off-navmesh бота (старый `TryStartLadder` умел, новый
  `FindRouteRecursive` — нет, т.к. `FindPath(from)` падает раньше). Это надо вернуть
  точечно (шаг 1 выше), а не откатывать segmented-роутинг целиком.
