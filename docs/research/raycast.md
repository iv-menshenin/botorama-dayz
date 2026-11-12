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

---

## Лут-гейт: «не брать предмет сквозь стену И сквозь закрытое стекло»

### Цель

Гейт лута для бота: бот НЕ берёт предмет (и содержимое контейнера), если между его глазом
и предметом/контейнером есть препятствие по лучу — **стена здания ИЛИ стекло окна**. Без угла
зрения и без освещённости (осознанное упрощение: «только барьер по лучу»).

### Ответ коротко (рекомендация)

Одного типа `ObjIntersect*` НЕ существует: **стена ловится геометрией `Geom`/`Fire`, стекло —
только `View`**. Нужен **двухлучевой гейт** (ИЛИ по блокировке):

1. **`ObjIntersectGeom`** (луч глаз → предмет): ловит стены (включая бейкед-стены), камни,
   любую твёрдую коллизию. Блок = ближайший хит ≠ сам предмет/контейнер.
2. **`ObjIntersectView`** (луч глаз → предмет): ловит стекло окна (view-поверхность, которую
   `Geom`/`Fire` НЕ видят). Блок = ближайший хит ≠ сам предмет.

Взять можно только если **оба** луча не блокированы (т.е. ближайший хит каждого — сама цель,
либо хитов нет). Это ровно ванильная модель `IsObjectObstructed` (см. ниже), только ваниль
берёт для стены `Fire`, а не `Geom`.

### Как стекло представлено в DayZ: это view-геометрия, НЕ fire и НЕ geom

- Стекло — это **поверхность view-слоя** модели здания (материал `Hit_Glass` / `Hit_Glass_Thin`,
  `3_game/impacteffects.c:55-56`). Оно входит в видимый меш здания, но в «огневую» геометрию
  (firegeo) окно — это **дыра** (стекла нет), а в физической коллизии (geom) стекла тоже нет
  (персонаж проходит сквозь стекло, разбивая его).
- **`ObjIntersectFire` НЕ ловит стекло** — Expansion явно это фиксирует: в `eAIBase` LOS-райкаст
  после `ObjIntersectView`, попавшего в здание, **перепроверяет** `ObjIntersectFire` «in case
  view is blocked by a window/other see-through object»; комментарий: AI «увидит» цель за стеной
  у окна «due to the wall's firegeo and use of ObjIntersectFire»
  (`DayZ-Expansion-Scripts/.../eAIBase.c:8223-8228`). Т.е. fire-луч проходит сквозь окно = стекло
  fire-лучом не ловится.
- **`ObjIntersectGeom` стекло тоже не ловит** — геометрия `Geom` = физическая коллизия; стекло
  не коллизит. `[нужно проверить]` (эмпирически не прогнано, но следует из модели firegeo/geom;
  прямой тест: райкаст `Geom` изнутри дома наружу сквозь закрытое окно → 0 хитов).

### Ванильный эталон: `MiscGameplayFunctions.IsObjectObstructed(FilterEx)`

Именно это «могу ли я взаимодействовать с объектом через препятствие» делает ваниль
(`4_world/static/miscgameplayfunctions.c:1531-1571`):

1. **Proxy-луч** (`RaycastRVProxy`, дефолт `type=View`, `flags=ALLOBJECTS`) — для proxy-объектов
   (`hierLevel > 0`), фильтр `parent.CanProxyObstruct()` / `object.CanProxyObstructSelf()`
   (стекло/прозрачные части у палаток `TentBase.HasProxyParts()` и basebuilding).
2. **Обычный луч** (`RaycastRV`, `type = ObjIntersectFire`, `flags=ALLOBJECTS`) — для твёрдых
   препятствий, фильтр по `hit.CanObstruct()`.

`CanObstruct()` — виртуальный метод `Object` (`3_game/entities/object.c:781`):
- база: `IsPlainObject() && !IsScenery()` (обычный объект, не декорация);
- `Building.CanObstruct() == true` (`3_game/entities/building.c:246`);
- `ItemBase.CanObstruct() == !player || !IsPlayerInside(player,"")` (`4_world/entities/itembase.c:973`).

Этот же механизм вызывает клиентский лут-визинити `VicinityItemManager`
(`5_mission/gui/inventorynew/vicinityitemmanager.c:306` — `type=ObjIntersectView`, `radius=0.1`,
`flags=ALLOBJECTS` для «предмет — первый по лучу»; далее `:347-359` бокс-скан `View+Fire` +
`FilterObstructingObjects` + `IsObjectObstructed`). Т.е. ваниль **объединяет View и Fire** ровно
потому, что ни один тип не покрывает и стену, и стекло.

Замечание: ваниль берёт `Fire` (не `Geom`) для стены. Для лут-гейта бота `Geom` надёжнее —
`Fire` не возвращал `Static_rock_monolith` (минный камень без fire-геометрии, готча 4 выше).
Стены зданий `Fire` ловит, но `Geom` покрывает более широкий класс статики и уже используется
в боте для «твёрдого препятствия» (`dmVision.HasLOS`, соты, объезд). Рекомендация — `Geom`.

### Expansion эталон LOS (двухпроходный)

`eAIBase` (Expansion, `.../eAIBase.c:8194-8232`):
1. `DayZPhysics.RaycastRV(beg, end, ..., ObjIntersectView, radius)` — radius 0.05 (для предмета
   0.15 под `EXPANSION_AI_ITEM_TARGET_REQUIRE_LOS`).
2. Если `results[0].IsBuilding()` — перепроверка `RaycastRV(contactPos - dir*0.1, end, ...,
   ObjIntersectFire, radius)` («view заблокирована окном/прозрачным» → fire скажет, есть ли
   реально непрозрачное препятствие).

Это инверсия лут-гейта (там нужно «видИт ли через стекло», у нас — «стекло блокирует»), но
подтверждает раскладку типов: View = стекло+стена, Fire = только непрозрачная стена.

### Проверенные сигнатуры

| Сигнатура | Источник |
|---|---|
| `enum ObjIntersect { Fire=0, View=1, Geom=2, IFire=3, None=4 }` | `3_game/constants.c:31-38` |
| `RaycastRVParams(beg,end,ignore=null,radius=0)`; дефолты `type=ObjIntersectView`, `flags=NEARESTCONTACT`, `sorted=false`, `groundOnly=false` | `3_game/global/dayzphysics.c:70-90` |
| `RaycastRVResult` поля `obj/parent/pos/dir/hierLevel/component/surface/entry/exit` | `3_game/global/dayzphysics.c:96-115` |
| `RaycastRV(..., iType=ObjIntersectView, radius=0.0, flags=NEARESTCONTACT)` | `3_game/global/dayzphysics.c:199` |
| `RaycastRVProxy(notnull RaycastRVParams, out array<ref RaycastRVResult>, excluded=null)` | `3_game/global/dayzphysics.c:204` |
| `Object.CanObstruct()/CanProxyObstruct()/CanProxyObstructSelf()` | `3_game/entities/object.c:781-795` |
| `Building.CanObstruct()==true` | `3_game/entities/building.c:246` |
| `IsObjectObstructedFilterEx`: proxy-View + Fire + `CanObstruct()` | `4_world/static/miscgameplayfunctions.c:1531-1571` |
| hitscan `ObjIntersect.Fire` + `ALLOBJECTS` + ручной поиск ближайшего («Proxy не гарантирует порядок») | `4_world/entities/firearms/weapon_base.c:1802-1815` |

### Параметры луча для гейта (флаги/radius/groundOnly)

- **`groundOnly = false`** (дефолт) — обязателен: нам нужны и стены, и стекло (groundOnly
  игнорирует объекты и ловит только землю). Не менять.
- **`radius`** — ваниль для «предмет — первый по лучу» использует `0.1` (`vicinityitemmanager.c:306`);
  Expansion LOS — `0.05`. Тонкий луч (0.0) может промахнуться мимо мелкого предмета/тонкого
  стекла → ложное «не заблокировано». Рекомендация `0.05–0.1` для обоих лучей.
  `[нужно проверить]` точное значение: «промахивается ли `Geom`-луч radius=0 мимо консервной
  банки» и «ловит ли `View`-луч radius=0 стекло» на тест-сервере.
- **`flags`** — порядок результата `RaycastRVProxy` **не гарантирован** (ваниль
  `weapon_base.c:1815` явно «Proxy doesn't guarantee any sensible order» и ищет ближайший по
  `DistanceSq`). Не полагаться на `hits[0]` при `ALLOBJECTS`. Надёжно:
  - либо `flags = NEARESTCONTACT` (дефолт) и брать единственный ближайший хит;
  - либо `ALLOBJECTS` + явно найти минимум `DistanceSq(pos, beg)` (как `weapon_base.c`).
  `[нужно подтвердить]` примечание из `decisions.md` («sorted=true → дальний первым, sorted=false →
  ближайший») противоречит комментарию `dmVision.ProbeRay` (`sorted=true → ближайший первым`,
  `dmVision.c:509-521`) — порядок `sorted` НЕ полагаться вовсе: считать ближайший хит самому.

### Отличить «хит стекла» от «хит стены»/«нет хита» (если нужно для лога/ветвления)

- «Нет хита» — `RaycastRVProxy` вернул `false` ИЛИ `hits.Count()==0`. Но осторожно: для
  бейкед-статики `obj` НЕ-null с пустым `GetType()` (готча 3 выше) — это хит, а не «нет хита».
- «Хит стекла» vs «хит стены»: `hit.surface` — `SurfaceInfo`-handle, `GetName()` возвращает имя
  поверхности (`SurfaceInfo` — нативный, `3_game/surfaceinfo.c:8`, метод `GetName()` — см.
  `weaponliftdiag.c:262`). Для стекла ожидается `Hit_Glass` / `Hit_Glass_Thin`
  (`3_game/impacteffects.c:55-56`). Для бейкед-статики `surface == null`.
  `[нужно проверить]`: заполняется ли `RaycastRVResult.surface` при `ObjIntersectView`-хите
  стекла (для `ObjIntersectGeom`/бейкед-хитов он null — подтверждено в codeguide.md:458).
- Для лут-гейта различать стекло/стену **не обязательно** — оба блокируют. Ветвление по
  поверхности нужно только ради диагностики («почему не взял»).

### Открытые вопросы / гипотезы для эмпирической проверки

1. `[нужно проверить]` `ObjIntersectGeom`-луч изнутри дома наружу сквозь **закрытое** окно →
   ожидается 0 хитов (стекло не geom). Если Geom всё же бьёт стекло — двухлучевой гейт
   вырождается в один `Geom` (но тогда View-луч лишний; проверить и на открытом окне).
2. `[нужно проверить]` `ObjIntersectView`-луч сквозь **бейкед-стену** конкретного дома с окном:
   где проходит (сквозь стену = 0 хитов, сквозь стекло = 1 хит). Именно эта асимметрия
   (View бьёт стекло, но не бейкед-стену) — основание двухлучевого подхода; подтвердить на
   реальном доме с окном на тест-сервере.
3. `[нужно проверить]` `radius` для `View`-луча: ловит ли `radius=0` стекло/мелкий предмет,
   или нужен `0.1` (ваниль) чтобы не давать ложный «не заблокировано».
4. `[нужно подтвердить]` порядок `hits[]` от `RaycastRVProxy` при `ALLOBJECTS` vs `NEARESTCONTACT`
   и влияние `sorted` (противоречие `decisions.md` vs `dmVision.c`) — определить канонический
   способ взять ближайший хит для гейта.
5. `[нужно проверить]` контейнеры: луч гейта должен идти к **контейнеру** (world-объекту), а не
   к предмету внутри карго (иначе `Geom` упрётся в стенку контейнера → ложный блок). Точка
   «глаз → контейнер» и трактовка «хит контейнера = достижимо» — отдельно уточнить в реализации.

### Развилки (для журнала `decisions.md`)

- **Один тип vs два луча**: выбрано два луча (`Geom` + `View`), т.к. ни один тип не покрывает
  и бейкед-стену, и стекло (стена = `Geom`/`Fire`, стекло = только `View`).
- **`Geom` vs `Fire` для стены**: выбрано `Geom` — покрывает бейкед-статику (`Static_rock_monolith`),
  которую `Fire` пропускает (готча 4), и совпадает с уже используемым в боте «твёрдым препятствием».
- **Гейт без угла зрения/освещённости**: осознанно только барьер по лучу (упрощение задачи),
  FOV/свет не участвуют.
