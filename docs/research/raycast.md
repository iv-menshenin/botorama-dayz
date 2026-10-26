# Raycast (DayZPhysics) — справочник и готчи

Единый источник истины по райкастам. Сюда стягивается всё, что раньше было размазано
по `navigation.md` (деревья/препятствия), `combat.md` (hitscan), `perception.md` (LOS),
`vehicles.md` (сигнатуры), `client-e2e.md` (клиент vs сервер) и `codeguide.md`
(`CollisionFlags`/baked-static). Если райкаст ведёт себя неожиданно — сверяться здесь.

## API (нативные, `3_game/global/dayzphysics.c`)

- `proto static bool RaycastRVProxy(notnull RaycastRVParams in, out notnull array<ref RaycastRVResult> results, array<Object> excluded = null);`
- `proto static bool RaycastRV(vector begPos, vector endPos, out vector contactPos, out vector contactDir, out int contactComponent, set<Object> results = NULL, Object with = NULL, Object ignore = NULL, bool sorted = false, bool ground_only = false, int iType = ObjIntersectView, float radius = 0.0, CollisionFlags flags = CollisionFlags.NEARESTCONTACT);`
- `proto static bool RayCastBullet(vector begPos, vector endPos, PhxInteractionLayers layerMask, Object ignoreObj, out Object hitObject, out vector hitPosition, out vector hitNormal, out float hitFraction);` — «пулевой» по маске физических слоёв.

### `RaycastRVParams` (нативный класс, `new` БЕЗ `ref`)
Поля: `begPos`, `endPos`, `ignore`, `with`, `radius`, `flags`, `type`, `sorted`, `groundOnly`.
Конструктор `RaycastRVParams(beg, end, ignore = null, radius = 0.0)` ставит дефолты
`flags = CollisionFlags.NEARESTCONTACT`, `type = ObjIntersectView`, `sorted = false`.
Замечание codeguide: `RaycastRVParams(a,b)` (2 арг) ломал компиляцию — всегда 3-арг `(a,b,null)`.

### `RaycastRVResult`
`Object obj`, `Object parent`, `vector pos`, `vector dir`, `int hierLevel`, `int component`,
`SurfaceInfo surface`, `bool entry`, `bool exit`.

## `ObjIntersect*` — типы пересечения (поле `type`, int)

| Константа | Знач. | Геометрия | Что ловит / НЕ ловит |
|---|---|---|---|
| `ObjIntersectFire` | 0 | fire | то, что бьют пули. Стены/камни/цели — бьёт. Эталон ванильного hitscan (`combat.md:641`). |
| `ObjIntersectView` | 1 | view | то, что ВИДИТ игрок (листва/LOD/стекло). Дефолт. **НЕ бьёт бейкед-стены зданий** (см. готчу ниже). |
| `ObjIntersectGeom` | 2 | geom | физическая коллизия (что блокирует движение). Бьёт стены/камни. |
| `ObjIntersectIFire` | 3 | indirect fire | непрямой огонь (взрыв). Тоже бьёт стены. |
| `ObjIntersectNone` | 4 | — | без геометрии. |

Источник: `3_game/constants.c:31` (`enum ObjIntersect { Fire, View, Geom, IFire, None }`) и
`3_game/global/dayzphysics.c:67-71`.

## `CollisionFlags` — флаги (поле `flags`, ПОСЛЕДОВАТЕЛЬНЫЙ enum, НЕ битмаска)

| Константа | Знач. | Смысл |
|---|---|---|
| `FIRSTCONTACT` | 0 | первый контакт вдоль луча |
| `NEARESTCONTACT` | 1 | ближайший контакт (дефолт) |
| `ONLYSTATIC` | 2 | только статика |
| `ONLYDYNAMIC` | 3 | только динамика |
| `ONLYWATER` | 4 | только вода |
| `ALLOBJECTS` | 5 | «first contact для КАЖДОГО объекта» (статика+динамика) |

Комбинировать `|` **нельзя** (последовательные значения, даст чужой член). Эмпирически
(2026-10) для стены здания флаги НЕ влияют — важен только `type` (`Geom`/`Fire`/`IFire`
с `NEARESTCONTACT` уже бьют стену). Не путать с `PhxInteractionLayers` (ниже) — тот НАТИВНАЯ
битмаска, `|` легален.

## `PhxInteractionLayers` — маска для `RayCastBullet` (нативная битмаска)

`ITEM_LARGE | BUILDING | FENCE | DOOR | VEHICLE | …` — `|` легален (в отличие от
`QueryFlags`/`CollisionFlags`). Примеры: `navigation.md:1163`, `camping-doors.md:67-69`.

## Готчи

1. **`ObjIntersectView` НЕ бьёт бейкед-стены зданий.** Луч «глаз → голова цели» сквозь
   стену дома возвращает **0 хитов** (проходит насквозь) → LOS = «видно» → бот стрелял
   сквозь стену. Эмпирика (E2E, `raycast-wall-matrix.json`): `view` → 0 hits, `geom`/`fire`/
   `ifire` → 1 hit (`obj=Land_House_2M1_Dam2`, `parent=null`). В открытом поле `view` и
   `geom` дают **одинаково 0 hits** (нет препятствия) — т.е. `geom` не даёт ложных «не вижу».
   **Для LOS и препятствий — `ObjIntersectGeom` (или `Fire`/`IFire`), НЕ `View`.**
   Починено в `dmVision.HasLOS`/`dmBotIntent_Flank.HasClearLine` (v3.331).

2. **`ObjIntersectView` даёт ложные хиты по листве/LOD.** Для детекта ствола дерева/
   препятствия на земле — `ObjIntersectGeom`/`ObjIntersectIFire` (физическая геометрия),
   а не `View` (листва даёт ложное «препятствие»). `navigation.md:491-509`.

3. **baked-static хит ≠ «нет хита».** Для бейкед-геометрии (монолиты `Static_rock_monolith`,
   палатки, остовы, стены домов) `RaycastRVResult.obj` — **НЕ-null объект с ПУСТЫМ
   `GetType()`**, `parent == null`, `surface == null`, `hier == 0`. Отличать «хит статики»
   от «нет хита» — по `obj != null`, а НЕ по непустому имени типа. `codeguide.md:342-348`.

4. **`ObjIntersectFire` не возвращал `Static_rock_monolith`** (минный камень без
   fire-геометрии), а `Geom` — бил. `decisions.md` (детект сот). Для надёжного статик-препятствия
   предпочитать `Geom`.

## Рекомендации «какой тип для чего»

| Задача | Тип |
|---|---|
| LOS «вижу/достреливаю до цели» (perception, стрельба, фланг) | `ObjIntersectGeom` (или `Fire`/`IFire`) |
| Препятствие на пути (MoveTo, объезд, дерево-ствол) | `ObjIntersectGeom` |
| Пулевой hitscan / «пробьёт ли пуля» | `ObjIntersectFire` (эталон ваниль `Hitscan`) |
| Детект двери/поверхности (OpenDoor) | `ObjIntersectView` (нужна view-поверхность двери) |

## Известные места с `ObjIntersectView`, которые стоит пересмотреть (на дату)

- `dmCoverFinder.c:186`, `dmCampCoverFinder.c:400` — LOS укрытия: тот же баг «сквозь стену»
  (укрытие «видит» сквозь стену), отдельная задача.
- `dmAISurvivor.c:1003` (OpenDoor), `dmBotIntent_RetrieveWeapon.c:128`, `dmBotIntent_MoveTo.c:1206` —
  осознанно `View` (дверь/предмет/препятствие), не боевой LOS.
