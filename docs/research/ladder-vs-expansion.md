# Research: лестничная маршрутизация — botorama vs Expansion

Статус: сравнение подходов к лестницам, фокус — «бот на off-navmesh платформе/крыше
должен спуститься вниз по лестнице». Дополняет `navigation.md` (там — полный разбор
механик Expansion: leap-of-faith, fall-safety, выбор лестницы); здесь — только СРАВНЕНИЕ
с нашим кодом и вывод «что перенять» для фикса `nopath`.

## Цель

Выяснить, почему botorama не спускается с off-navmesh платформы по лестнице (`nopath`),
чем отличается наш механизм от Expansion, и что конкретно перенять.

---

## 1. Таблица отличий

| Аспект | botorama | Expansion |
|---|---|---|
| **Детект здания с лестницей** | `FindLadderBuildingAt(pos)` — raycast ВНИЗ −50 м (фолбэк ВВЕРХ +25 м) из `RaycastRVProxy`, каст в `Building` (`dmBotPathfinder.c:348-371`) | `PhysicsGetFloorEntity()` (натив `Human`, `human.c:1400`) → каст в `BuildingBase` + `Expansion_GetLaddersCount() > 0` (`eAIBase.c:5015-5029`) |
| **Когда ищется лестница** | ТОЛЬКО после успешного `FindPath(from, sampledTo)` (`FindRouteRecursive`, `dmBotPathfinder.c:206`) → при off-navmesh `from` до лестничной логики НЕ доходит | Логика лестницы ортогональна pathfinding: гейт `eAI_CheckShouldClimbLadderToReachPosition` дёргается в `OverrideTargetPosition` каждый тик (`eAIBase.c:4742`) и опирается на **физический** `PhysicsGetFloorEntity`, а НЕ на успех `FindPath` |
| **Выбор лестницы** | `near/far` по высоте: `dBottom=|bottomY−lastY|`, `dTop=|topY−lastY|`, меньший → ближний конец (`dmBotPathfinder.c:259-282`). Стоимость = `RouteLength2D` (2D-длина + высота лестницы) (`:388-400`). Пул/посещения — только `visited` по `тип_индекс` внутри одной рекурсии | «process of elimination» + взвешивание `Distance2DSq × AbsFloat(ΔY)` отдельно для низа и верха (`eAIBase.c:4830-4831`), сортировка, `closestBtm < closestTop` → низ/верх (`:4850-4876`); пул `m_eAI_Ladders` с удалением использованных + кап `m_eAI_LadderLoops == 10` (`:4792-4808`, `:5145-5160`) |
| **entry-point для спуска** | `near` = верх лестницы при `dTop < dBottom`, `ladderDir = −1` (`dmBotPathfinder.c:277-282`) | `closestTop < closestBtm` → `entry = m_Con[1]` (верх), `climbDir = −1` (`eAIBase.c:4869-4876`) |
| **Детект «бот вне navmesh»** | явного НЕТ. `FindPath(from)` либо снипает `from`, либо возвращает false → `nopath` (`RePath`/`OnStart`, `MoveTo.c:295`, `:374`, `:410`) | косвенный: `m_IsTargetUnreachable` ставится в `UpdateNext` при `m_Count <= 3 && end≈бот && dist(end,target)≥2 м` (`ExpansionPathHandler.c:805-830`, комментарий прямо про «on the roof and need to go down a ladder e.g. Land_City_Stand_*») → `m_eAI_PreferLadder = true` (`eAIBase.c:5043-5044`) |
| **Прицепка к лестнице** | `dmBotIntent_UseLadder`: подход к `m_Entry` (`IntentMove` к entry-point, `DM_LADDER_ATTACH_DIST=2.0`), затем `SetClimbingLadderType` + `StartCommand_Ladder` напрямую (`UseLadder.c:97-129`) — navmesh НЕ нужен | то же по духу, но через per-tick физические проверки в `CommandHandler`: `eAI_IsInLadderRadius` (1.3 м) + `IsPointInCircle(GetEnd(), 0.55, pos)` + `eAI_IsCloseToLadderEntryPoint` (2.28 м) + `eAI_CanReachLadderEntryPoint` (`SphereCastBullet` + `IsPointInRotatedRectangle`) → `StartCommand_Ladder` (`eAIBase.c:7413-7427`) |
| **Исполнение подъёма/спуска** | активно кормим `IntentMove`: вверх `SetMove(0,3.0)`, вниз `SetMove(180,3.0)` (`UseLadder.c:175-182`) | НЕ задаёт движение на лестнице вообще (`//! Do nothing` при `m_eAI_IsOnLadder`, `eAIBase.c:7556-7559`); только `StartCommand_Ladder` → ждать `CanExit()` (`:7508-7510`) |
| **Отцепка** | `hcl.CanExit()` → `hcl.Exit()` (`UseLadder.c:184-194`) | `hcl.CanExit() && m_eAI_LadderTime > 2.0` → `hcl.Exit()` (`eAIBase.c:7508-7510`) |
| **Разворот при застревании на лестнице** | `m_StuckReversals`: нет верт. прогресса > `DM_LADDER_STUCK_TIME` → `m_Direction = −m_Direction`, повтор → `Exit()+Fail` (`UseLadder.c:199-222`) | `m_eAI_BlockedTime > 2.0` → `m_eAI_LadderClimbDirection *= −1` (`eAICommandMove.c:826-828`) |
| **Прыжок вниз (leap of faith)** | `TryLeapOfFaith` (`MoveTo.c:1552`): обратный `FindPath(goal→bot)`, НО гейт `botPos[1] − groundY > DM_FALL_JUMP_BLOCK_HEIGHT (3.0)` → return false («спуск только по лестнице») (`:1564-1567`) | `FindPathFrom` (`ExpansionPathPoint.c:133`), обратный `FindPath(target→bot)` + `climbHeight<2.5 && fallHeight<HEALTH_HEIGHT_LOW` (`:229`, `:278`) |

**Главное архитектурное отличие**: у нас лестница — **вторая фаза после navmesh-пути**
(`FindRouteRecursive` сначала должен найти `FindPath(from)`); у Expansion лестница —
**логика поверх пути, независимая от успеха `FindPath`**, детект здания идёт физическим
`PhysicsGetFloorEntity()` и per-tick физическими проверками близости.

---

## 2. Механизм Expansion для off-navmesh спуска (конкретно, метод + строки)

Сценарий: бот стоит на платформе/крыше, где navmesh нет (или остров оторван от земли),
цель внизу. Цепочка Expansion:

1. **Детект здания под ногами (физический, работает вне navmesh)** —
   `eAI_CheckShouldClimbLadderToReachPosition` (`eAIBase.c:4995`):
   ```c
   IEntity floor = PhysicsGetFloorEntity();          // eAIBase.c:5015
   BuildingBase building;
   if (Class.CastTo(building, floor) && building != m_eAI_BuildingWithLadder
       && building.Expansion_GetLaddersCount() > 0 && !eAI_IsExcludedBuildingWithLadder(building))
       m_eAI_BuildingWithLadder = building;           // eAIBase.c:5016-5029
   ```
   `PhysicsGetFloorEntity()` — натив `Human` (`human.c:1400`), возвращает сущность-пол под
   ногами пешки, независимо от navmesh.

2. **Детект «на крыше, путь не ведёт вниз» → цель недостижима** — `ExpansionPathHandler.UpdateNext`
   (`ExpansionPathHandler.c:805-830`):
   ```c
   //! @note normally we would check m_Count <= 2, but there are certain structures
   //! where this won't work if we are on the roof and need to go down a ladder
   //! e.g. Land_City_Stand_*
   if (!m_IsTargetUnreachable && m_Count <= 3 && m_Unit.m_eAI_CommandTime > 1.0
       && m_Unit.m_eAI_BuildingWithLadder) {
       vector end = GetEnd();
       if (Math.IsPointInCircle(end, 0.55, m_Current.Position)
           && vector.DistanceSq(end, GetTarget()) >= 4.0)
           m_IsTargetUnreachable = true;
   }
   ```
   (`GetEnd()` при пустом пути возвращает позицию юнита — `ExpansionPathHandler.c:1167-1173`.)

3. **Unreachable → предпочесть лестницу** — `eAI_CheckShouldClimbLadderToReachPosition`
   (`eAIBase.c:5043-5044`): `if (m_PathFinding.m_IsTargetUnreachable) m_eAI_PreferLadder = true;`

4. **Выбор лестницы + entry = верх (спуск)** — `OverrideTargetPosition` (`eAIBase.c:4792-4896`):
   взвешивание `Distance2DSq × AbsFloat(ΔY)` (`:4830-4831`), `closestTop < closestBtm` →
   `entry = ModelToWorld(m_Con[1])`, `climbDir = −1` (`:4869-4876`), затем
   `pPosition = m_eAI_LadderEntryPoint` (`:4887`), `SetTarget(pPosition)` (`:4996`).

5. **Прицепка — per-tick, физическая, без navmesh** — `CommandHandler` (`eAIBase.c:7413-7427`):
   `eAI_IsInLadderRadius(entry)` (1.3 м) + `IsPointInCircle(GetEnd(), 0.55, pos)` +
   `eAI_IsCloseToLadderEntryPoint()` (2.28 м) + `eAI_CanReachLadderEntryPoint()`
   (`SphereCastBullet` от `pos+1.1` к entry + `IsPointInRotatedRectangle`, `eAIBase.c:5165-5209`)
   → `SetClimbingLadderType` + `StartCommand_Ladder(building, index)`.

**Ключ**: бот уже СТОИТ рядом с верхом лестницы (он на платформе), поэтому прицепка
срабатывает по физическим проверкам близости, а НЕ по нахождению navmesh-пути до entry.
`FindPath` вообще не обязан вернуть путь — его роль сводится к тому, чтобы через
`m_IsTargetUnreachable` включить `m_eAI_PreferLadder`.

---

## 3. Что перенять для фикса «спуск с off-navmesh платформы»

Наш баг — двухсторонний гейт, между которыми зазор:
- `FindRouteRecursive` требует `FindPath(from)` успешным ДО лестничной логики
  (`dmBotPathfinder.c:206`); при off-navmesh `from` → return false.
- `TryLeapOfFaith` при `botPos[1]−groundY > 3.0` сознательно отдаёт спуск лестнице
  (`MoveTo.c:1564-1567`), но лестница не находит маршрут (п. выше) → итог `nopath`.

Минимальные порты (по возрастанию стоимости):

1. **Детект здания ДО гейта `FindPath(from)`.** В `FindRouteRecursive` перед/вместо
   `if (!FindPath(from, sampledTo)) return false` (`:206`) сделать раннюю ветку: если
   `from` off-navmesh (`SamplePosition(from, …)` вернул точку с большим ΔY, либо `FindPath`
   вернул false) И `botPos[1] − SurfaceY > DM_NAV_GAP` И `m_Goal[1] < botPos[1]` — сразу
   искать `FindLadderBuildingAt(from)` и строить лестничный сегмент с `entry = top`
   (`m_Direction = −1`), БЕЗ рекурсивного navmesh-пути до entry. Бот уже у верха лестницы,
   путь до entry не нужен.

2. **Заменить/дополнить raycast-детект здания на `PhysicsGetFloorEntity()`.** Наш
   `FindLadderBuildingAt` (`:348`) бьёт лучом вниз из `pos+1.0`; надёжнее физический пол под
   ногами (аналог `eAIBase.c:5015`). `dmAISurvivorBase : PlayerBase : … : Human` имеет
   `PhysicsGetFloorEntity()` бесплатно. Это устраняет зависимость от того, «попал ли луч в
   здание под платформой» (луч может промазать сквозь щели платформы/крыши).

3. **Перенять «process of elimination» + взвешивание `2D × ΔY`.** Наш `near/far` по чистой
   высоте (`dBottom`/`dTop`) не учитывает горизонтальную близость входа — на здании с
   несколькими лестницами может выбрать дальний вход, недостижимый с платформы. Порт:
   `distSq = Distance2DSq(modelPos, con) × AbsFloat(modelPos[1] − con[1])` для низа и верха,
   `closestBtm < closestTop` → низ (вверх) / верх (вниз), + пул с удалением использованных
   лестниц и капом циклов (`m_eAI_LadderLoops == 10`). Это и надёжнее, и не зацикливается.

4. **Детект «на крыше, путь не вниз» → флаг недостижимости.** Порт
   `ExpansionPathHandler.c:805-830`: если `FindPath` вернул путь `Count() ≤ 3`, конец рядом
   с ботом (0.55), а дистанция до цели ≥ 2 м, и есть здание с лестницей — не `return false`,
   а пометить «цель достижима только по лестнице» и идти в лестничную ветку. Это ровно тот
   сигнал, которого нам не хватает, чтобы лестница сработала при коротком/пустом navmesh-пути.

5. **(опционально) Прицепка по физическим проверкам.** Наш `UseLadder` уже прицепляется без
   navmesh (подход `IntentMove` + `StartCommand_Ladder`) — менять не требуется. Дополнительно
   можно перенять `eAI_IsCloseToLadderEntryPoint` (entry выше → `begPos[1] += 1.1`, 3D-дистанция
   < 2.28 м) и `eAI_CanReachLadderEntryPoint` (`SphereCastBullet` + прямоугольник), чтобы
   гарантировать, что вход реально достижим с платформы (у нас пока только дистанция до entry).

**Не перенимать**: активный `IntentMove` на подъёме/спуске — у Expansion его нет (`//! Do
nothing`), но у нас это уже проработано отдельно (см. `navigation.md` §«Climb speed»); к
данному багу отношения не имеет.

---

## 4. Готчи/подводные камни Expansion в этом сценарии

- **Лестничный спин после слезания** — в Expansion закомментирован хак
  `//m_eAI_Recreate = true;  //! Hack fix for vanilla bug with spinning while falling ...`
  (`eAIBase.c:7396`) — ванильный баг вращения при падении после лестницы. У нас та же
  проблема уже диагностирована (`DM_BOT_DEBUG_LADDER`, «бесконечное кручение после лестницы»,
  `MoveTo.c:780-795`) — общая первопричина, не связана с данным фиксом.
- **Зависание на лестнице при нескольких лестницах**: «allow changing direction while
  climbing» отключен (закомментирован) именно из-за проблем «вверх по одной, вниз по другой»
  (`eAIBase.c:4754-4776`) — Expansion ограничивает смену направления зданием с 1 лестницей.
- **Зацикливание пула** лестниц купируется `m_eAI_LadderLoops == 10` и удалением
  использованных лестниц (`eAIBase.c:5145-5160`, `:7437-7438`); без этого бот вечно выбирает
  недостижимый вход.
- **Хардкод-исключения зданий** (`eAI_IsExcludedBuildingWithLadder`, `eAIBase.c:5049-5110`):
  краны (`Land_Pier_Crane*`), геоплант (`Land_Geoplant_*`) — неправильный navmesh на верхушках
  или недостижимая лестница. Для боевых/реалистичных сцен может понадобиться свой список.
- **Прицепка должна быть 3D**: `eAI_IsCloseToLadderEntryPoint` при entry выше добавляет
  `+1.1` к Y бота (`eAIBase.c:5211-5231`) — иначе бот «цепляется к лестнице, находясь этажом
  выше» (уже отражено в `navigation.md`, урок v3.135).

---

## Гипотезы для эмпирической проверки

- [нужно подтвердить] Возвращает ли `AIWorld.FindPath` при off-navmesh `from` (бот на
  платформе без навмеша) `false` целиком, или снипает `from` к ближайшему navmesh (в т.ч.
  этажом ниже) и возвращает «усечённый» путь? От этого зависит, где ставить раннюю
  лестничную ветку — на `!FindPath` или на `path.Count() <= 3 && end≈бот`. Probe: `raycast`/
  `FindPath` из точки платформы (через `botdump` позиции бота) в точку земли.
- [нужно проверить] Достаточно ли `PhysicsGetFloorEntity()` на `INSTANCETYPE_AI_SERVER`-пешке
  для детекта платформы/крыши (а не только пола здания), или на крыше он возвращает сам
  `Building`, а на тонкой платформе — null? Probe: лог `PhysicsGetFloorEntity().GetType()` у
  бота на крыше vs на внутреннем этаже vs на столе.
- [нужно проверить] Промахивается ли наш `FindLadderBuildingAt` (луч вниз из `pos+1.0`)
  сквозь щели платформы/крыши, из-за чего здание не находится — сравнить с
  `PhysicsGetFloorEntity()` на той же позиции.
- [нужно проверить] Выбирает ли наш `near/far` (по чистой ΔY) дальний/недостижимый вход на
  здании с ≥2 лестницами, и лечится ли это взвешиванием `2D × ΔY` из Expansion (п.3).

---

## Существенные развилки и принятые решения (для `docs/decisions.md`)

- **Развилка**: чинить лестницу «поверх пути» (перенести детект здания/лестницы ДО гейта
  `FindPath(from)`) vs расширять `TryLeapOfFaith` (разрешить прыжок с крыши). Выбрано первое —
  прыжок с крыши >3 м опасен и уже сознательно отдан лестнице; лестница — корректный
  «человеческий» маршрут, и её блокировка искусственна (гейт `FindPath`). Влияет: `dmBotPathfinder.FindRouteRecursive`
  + `MoveTo.RePath`.
- **Развилка**: детект здания под ботом — raycast вниз (текущий) vs `PhysicsGetFloorEntity()`
  (Expansion). Склоняюсь к нативу как к более надёжному (не зависит от попадания луча), но
  эмпирически не подтверждён на крыше — решение отложено до probe-гипотезы.
- **Развилка**: взвешивание выбора лестницы — оставить `near/far` (проще) vs портировать
  `2D × ΔY` + пул elimination (надёжнее, анти-зацикливание). Для одно-лестничных зданий разницы
  нет; для много-лестничных нужен порт. Решение — портировать, если подтвердится гипотеза про
  недостижимый вход.
