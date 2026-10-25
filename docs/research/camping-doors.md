# Research: внешние выходы здания (Kamping — бот караулит здание цели)

Цель: закрыть API-вопросы для состояния `dmBotState_Kamping` — бот находит ВНЕШНИЕ
выходы здания, в котором засела цель, встаёт напротив ближайшего и ждёт. Нужно:
(1) отличать внешнюю дверь от межкомнатной; (2) знать ось «наружу» для точки стояния;
(3) получить точку двери пригодной высоты; (4) детект «внутри здания» для условий
`PawnInBuilding`/`TargetInBuilding`.

Известное уже покрыто `docs/research/navigation.md` §двери и §7 (детект внутри здания).
Здесь — только новые сигнатуры/выводы и не-подтверждённые гипотезы.

---

## 1. Внешние vs внутренние двери

### Что есть в нативе (полный список door-API здания)

`class Building : EntityAI` (`3_game/entities/building.c`):

```
proto native int    GetDoorIndex(int componentIndex);  // building.c:17
proto native int    GetDoorCount();                    // building.c:20
proto native bool   IsDoorOpen(int index);             // building.c:23 (+ Opening:26, OpeningAjar:29, Opened:35, OpenedAjar:38, Closed:41)
proto native bool   IsDoorLocked(int index);           // building.c:44
proto native void   OpenDoor(int index);               // building.c:50
proto native void   CloseDoor(int index);              // building.c:53
proto native vector GetDoorSoundPos(int index);        // building.c:62
proto native float  GetDoorSoundDistance(int index);   // building.c:65
bool CanDoorBeOpened(int doorIndex, bool checkIfLocked = false); // building.c:130
proto native int    GetLaddersCount();                 // building.c:12
proto native vector GetLadderPosTop(int ladderIndex);  // building.c:13
proto native vector GetLadderPosBottom(int ladderIndex);// building.c:14
```

**Ничего из этого НЕ даёт признак «внешняя/внутренняя» и НЕ даёт ориентацию створки.**
Прочитан весь файл `building.c` — нативных методов `GetDoorPos`, `GetDoorNormal`,
`GetDoorAngle`, «дверь на оболочке», «exterior flag» НЕТ. `GetDoorCount()` возвращает
ВСЕ двери здания (межкомнатные входят — это видно по логике Expansion, который
открывает ЛЮБУЮ дверь, блокирующую путь, без различения внешняя/внутренняя).

### Как это видит Expansion (референс, подтверждает отсутствие exterior-признака)

`eAIBase.HandleBuildingDoors` (`AI/.../Entities/AI/eAIBase.c:11540-11750`):
- райкаст вперёд → `building.GetDoorIndex(result.component)` (`eAIBase.c:11566`);
- специальный случай `doorIndex == -1` (подземные входы `Expansion_IsUndergroundEntrance()`,
  не входят в `GetDoorCount`) — `eAIBase.c:11570`;
- «enterable» проверка `Expansion_IsEnterable()` (`AI/.../Entities/Buildings/BuildingBase.c:258`):
  `GetDoorCount()==0 → false` + хардкод-исключения `land_wreck_*`. НИКАКОГО понятия
  «внешняя дверь» нет — Expansion просто открывает/закрывает дверь на пути.

**Вывод**: нативного/Expansion-признака «внешний выход» не существует — детект придётся
строить геометрически (рейкаст) самим.

### Рекомендуемый детект «внешняя vs внутренняя» (геометрия)

Признак: у внешней двери с одной стороны — открытое небо/улица; у внутренней — с обеих
сторон стены того же здания. Детект через **горизонтальный «выходной» луч**:

```
для каждой двери i:
  D  = building.GetDoorSoundPos(i)                       // точка двери (XZ важно)
  C  = центр здания: (minMax[0]+minMax[1])*0.5 из GetCollisionBox (см. §2)
  out = (D - C); out[1] = 0.0; out = out.Normalized();   // ось «наружу» (фолбэк §2)
  p0  = D + out * 0.5;                                    // старт за створкой/косяком
  end = D + out * probe;                                  // probe ≈ 3..5 м
  hitSelf = DayZPhysics.RayCastBullet(p0, end,
              PhxInteractionLayers.BUILDING|PhxInteractionLayers.FENCE|
              PhxInteractionLayers.DOOR|PhxInteractionLayers.ITEM_LARGE|
              PhxInteractionLayers.VEHICLE,
              null, hitObj, hitPos, hitNormal, hitFrac)
           && hitObj == building;
  внутренняя = hitSelf;   // луч упёрся в своё же здание (стена комнаты/фасад за дверью)
  внешняя    = !hitSelf;  // луч вышел из футпринта, не зацепив своё здание
```

Логика: у внутренней двери «наружу» (от центра) луч проходит сквозь интерьер и
упирается во внешнюю стену СВОЕГО здания (`hitObj == building`). У внешней двери луч
покидает футпринт и своё здание НЕ задевает (может попасть в другое здание/забор/землю —
это не «своё», значит внешняя). Тест именно на `hitObj == building` (не `IsInherited(House)`)
— чтобы соседнее здание за окном не засчиталось как «внутренняя».

Альтернатива (кросс-чек, дешевле, но с ложными срабатываниями): луч ВВЕРХ из точки
`D + out*1.5` — если над ней открытое небо, а над самой `D` — `House` (§4), то дверь
внешняя. НО крыльцо/навес/балкон дают ложный «под крышей» снаружи → использовать только
как вторичное подтверждение, не как основной признак.

**Сигнатуры для детекта**: `RayCastBullet` — `dayzphysics.c:211`; маска слоёв —
`PhxInteractionLayers` (§4, `|` легален); `GetCollisionBox` — `object.c:376`.

---

## 2. Направление двери (ось «наружу»)

### Есть ли натив ориентации створки — НЕТ

В `building.c` нет ни вектора направления двери, ни угла открытия, ни нормали проёма.
Подтверждено полным чтением файла. Единственная позиционная информация о двери —
`GetDoorSoundPos(i)` (одна точка).

### Фолбэк «центр здания → точка двери» — подтверждаю как единственный рабочий

`out = normalize(GetDoorSoundPos(i) - center)` (с занулённым Y) — ось «наружу». Это
грубая, но достаточная ось: для большинства дверей она указывает от центра футпринта к
периметру (внешняя дверь — примерно верно; у внутренней двери — направление к ближайшей
внешней стене, что и нужно для «выходного» луча §1).

### Как получить «центр здания» в world-space

- **`proto native bool GetCollisionBox(out vector minMax[2]);`** — `3_game/entities/object.c:376`.
  `minMax[0]` = min, `minMax[1]` = max. **ПРОВЕРЕНО ЭМПИРИЧЕСКИ (v3.319): возвращает
  MODEL-SPACE (локальный) AABB, а НЕ world-space** — центр `(minMax[0]+minMax[1])*0.5`
  для здания дал `(2.16, −0.99)` ≈ локальную величину, а не world-координату (~13500,13043).
  НЕ использовать для world-центра; смешивание с world-space (`GetDoorSoundPos`) ломает
  направление `out = doorPos − center` (указывает на абсолютную позицию двери от 0,0).
- **`proto native vector GetWorldPosition();`** — `object.c:297` — **world-space позиция
  (origin/точка привязки модели, у зданий ≈ центр футпринта у основания)**. КАНОНИЧНЫЙ
  world-центр здания для оси «наружу».
- **`proto native vector GetPosition();`** — `object.c:293` — для top-level здания = world-space,
  фолбэк к `GetWorldPosition()`.
- **`proto native vector GetBoundingCenter();`** — `object.c:103` (нативный bounding-центр);
  никем не используется в vanilla/Expansion — семантика (world vs model) непроверяема,
  НЕ полагаться.

**Рекомендация**: центр здания = `GetWorldPosition()` (world-space, ≈ центр футпринта);
НЕ `GetCollisionBox` (model-space). Для точки стояния «напротив двери»:
`standPoint = D + out * standDist` (`standDist ≈ 1.0–1.5 м`), XZ на navmesh через
`SampleNavmeshPosition` (бот и так снэпит в `MoveTo`).

---

## 3. Точка двери для рейкастов / высота `GetDoorSoundPos`

`proto native vector GetDoorSoundPos(int index);` (`building.c:62`) — дока: «Position in
world space for where the door sounds are played from» — это точка ЭМИССИИ звука двери.
Подтверждено использованием: `containerlocked.c:90` ставит звук замка в
`GetDoorSoundPos(doorIdx)`; `eAIBase.c:11649` использует её как целевую точку двери.

**Высота НЕ задокументирована и НЕ выводится из скриптов.** Скорее всего это точка у
двери на уровне ~середины/ручки (звук створки), НЕ у пола и НЕ на высоте глаз — но это
[нужно проверить] эмпирически.

**Что это значит для LOS-рейкаста «бот → дверь»**: для рейкаста от глаз бота к двери
использовать `XZ` из `GetDoorSoundPos`, а `Y` — высоту глаз бота (или `groundY + DM_EYE_HEIGHT`),
а НЕ доверять Y из `GetDoorSoundPos` (он может быть у пола). Для «встать напротив двери»
высота не важна — точка стояния снэпится на navmesh.

Probe для `dayz-tester`: `botdump`/`raycast` — вывести `GetDoorSoundPos(i)` и сравнить
его `Y` с уровнем земли (`SurfaceY`) и высотой глаз, для двери на первом этаже типового дома.

---

## 4. Детект «внутри здания» (`PawnInBuilding` / `TargetInBuilding`)

### 4a. `IsSoundInsideBuilding()` на AI-боте

`proto native bool IsSoundInsideBuilding();` — `3_game/entities/man.c:37` (дубль
`3_game/entities/dayzanimal.c:195`). Наследуется `dmAISurvivorBase : PlayerBase : Man`.

**Семантика** — акустический реверб «внутри звукового объёма здания», НЕ геометрический
raycast. Подтверждено всеми употреблениями (см. `navigation.md` §7):
`environment.c:315`, `dayzplayerimplement.c:3446` (payload аудио), `dayzplayerimplement.c:3834` /
`dayzanimal.c:384` (сравнение акустической среды), `spookyareamisc.c:12/62`.

**Работает ли на `INSTANCETYPE_AI_SERVER`?** Прямого гейта `IsControlledPlayer()` в
употреблениях нет, и натив объявлен на `Man`/`DayZAnimal` (оба — сервер-авторитетные
сущности; `DayZAnimal` — серверное существо). НО все найденные вызовы в ванили —
клиентские/аудио (Environment, звуковая аттенюация, spooky-зона). Ботorama уже вызывает
её на пешке в отладке: `dmBotIntent_MoveTo.c:1112` (`pawn.IsSoundInsideBuilding().ToString()`
в `DebugElevatedGoal`, гейт `DM_BOT_DEBUG_PATHFINDER`). **Статус: работает без краша на
AI_SERVER, но даёт ли осмысленное значение (а не всегда false) — [нужно проверить]
эмпирически.** Из-за «аудио-потока» возможна задержка смены значения.

**Вывод**: `IsSoundInsideBuilding()` — дешёвый вторичный сигнал для позиции САМОГО бота
(`PawnInBuilding`), но не для произвольной точки цели. Основной детект — raycast (4b).

### 4b. `TargetInBuilding` — raycast вверх + `House`

Подтверждаю сигнатуру и состав маски (совпадает с `navigation.md` §7, эталон —
`CheckUnderRoof`, `4_world/classes/environment/environment.c:391-411`):

```
proto static bool RayCastBullet(vector begPos, vector endPos, PhxInteractionLayers layerMask,
    Object ignoreObj, out Object hitObject, out vector hitPosition, out vector hitNormal,
    out float hitFraction);                                  // dayzphysics.c:211
```

Паттерн «точка под крышей здания»:

```
bool IsPointUnderRoofBuilding(vector from)
{
    Object hitObject; vector hitPosition, hitNormal; float hitFraction;
    bool hit = DayZPhysics.RayCastBullet(from, from + "0 25 0",
        PhxInteractionLayers.ITEM_LARGE|PhxInteractionLayers.BUILDING|PhxInteractionLayers.VEHICLE,
        null, hitObject, hitPosition, hitNormal, hitFraction);
    return hit && hitObject.IsInherited(House);   // House : BuildingBase : Building (game/super/building.c:85)
}
```

`House : BuildingBase : Building` (`4_world/entities/game/super/building.c:85/1`),
`typedef House BuildingSuper` (`:92`). `IsInherited(House)` покрывает `House` и наследники.

### 4c. `PhxInteractionLayers` — битмаска или enum? можно ли `|`?

**Это нативный enum, который движок трактует как БИТМАСКУ — `|` на нём легален и
каноничен.** Объявление — `3_game/global/dayzphysics.c:1` (голый enum без явных значений:
`NOCOLLISION, DEFAULT, BUILDING, CHARACTER, VEHICLE, DYNAMICITEM, DYNAMICITEM_NOCHAR,
ROADWAY, VEHICLE_NOTERRAIN, CHARACTER_NO_GRAVITY, RAGDOLL_NO_CHARACTER, FIREGEOM, DOOR,
RAGDOLL, WATERLAYER, TERRAIN, GHOST, WORLDBOUNDS, FENCE, AI, AI_NO_COLLISION, AI_COMPLEX,
TINYCAPSULE, TRIGGER, TRIGGER_NOTERRAIN, ITEM_SMALL, ITEM_LARGE, CAMERA, TEMP`).

Ваниль комбинирует слои через `|` повсеместно:
- `environment.c:406` — `ITEM_LARGE|BUILDING|VEHICLE` (наш паттерн);
- `meleetargeting.c:74`, `dayzplayerimplementmeleecombat.c:671`, `weapon_base.c:75` —
  полные маски через `|`;
- `actiontargets.c:329` — `ROADWAY|TERRAIN|WATERLAYER`;
- «все слои» — сырой литерал `0xFFFFFFFFFF` (`plugindiagmenuclient.c:1357`,
  `plugintargettemperature.c:44`).

**Важное отличие от правила codeguide** («побитовые операторы не поддерживаются»,
`docs/codeguide.md:59`): то правило верно для **скриптовых** int и для **обычных
последовательных enum'ов** (`QueryFlags`/`CollisionFlags` — см. `docs/decisions.md`
«QueryFlags/CollisionFlags — последовательные enum'ы»), где `STATIC|DYNAMIC` даёт чужой
член. `PhxInteractionLayers` — исключение: это НАТИВНЫЙ битмасковый enum, `|` на нём
компилируется и корректен (движок трактует `layerMask` как битовую маску). Точные
числовые значения (степени двойки или `1<<index`) в скриптах не видны — это неважно,
т.к. копируем готовый ванильный паттерн `A|B|C`.

**Разница с существующим `RoofProbe` ботorama**: `dmBotIntent_MoveTo.c:1120` (`RoofProbe`,
гейт `DM_BOT_DEBUG_PATHFINDER`) делает рейкаст вверх через `RaycastRVProxy` +
`CollisionFlags.ALLOBJECTS` (тяжелее, и без маски слоёв). Для прод-детекта «под крышей
здания» предпочтительнее лёгкий `RayCastBullet` с маской `ITEM_LARGE|BUILDING|VEHICLE` +
`IsInherited(House)` (ровно `CheckUnderRoof`, без 10-секундного кэша `Environment`).

---

## Рекомендация для botorama (итог)

1. **Перечень внешних дверей** — примитив в пешке/мозге (или статик-хелпер):
   ```
   void GetExternalDoors(Building b, out array<int> extDoors):
     vector minMax[2]; if (!b.GetCollisionBox(minMax)) return;
     vector center = (minMax[0] + minMax[1]) * 0.5;
     for i in 0..b.GetDoorCount():
       vector D = b.GetDoorSoundPos(i);
       vector out = D - center; out[1] = 0; out.Normalize();
       // горизонтальный «выходной» луч (§1); hitObj == b → внутренняя
       if (!horizontalRayHitsSelf(b, D, out)) extDoors.Insert(i);
   ```
   Для «Kamping» — выбрать ближайшую по 2D-дистанции к боту внешнюю дверь
   (`GetDoorSoundPos(i)`, как уже делает `GetNearestDoorBySoundPos`, `building.c:73`),
   `standPoint = D + out * 1.0..1.5`, снэп на navmesh → `MoveTo`/`HoldLook` на дверь.

2. **Центр здания** — `GetCollisionBox`-центр (фолбэк `GetPosition`). Направление
   «наружу» — только «центр → точка двери» (натива ориентации нет).

3. **Точка двери** — `GetDoorSoundPos(i)`: `XZ` использовать как есть, `Y` подменять на
   высоту глаз при LOS-рейкастах. Высоту точки [нужно проверить].

4. **Детект «внутри»**:
   - `PawnInBuilding` — `pawn.IsSoundInsideBuilding()` (дешёво, [нужно проверить] на
     AI_SERVER); основной/надёжный — `IsPointUnderRoofBuilding(botPos)` (raycast вверх).
   - `TargetInBuilding` — `RayCastBullet(target, target+"0 25 0",
     ITEM_LARGE|BUILDING|VEHICLE, null, …) && hit.IsInherited(House)`.
   - `PhxInteractionLayers` комбинировать через `|` (легально, в отличие от
     `QueryFlags`/`CollisionFlags`).

5. **Эмпирические гипотезы** → `dayz-tester`:
   - [нужно проверить] `GetDoorSoundPos(i).Y` — на уровне пола / середины / ручки?
     Probe: `botdump`/`raycast` — вывести `GetDoorSoundPos(i)` и `SurfaceY` под ней.
   - [нужно проверить] Горизонтальный «выходной» луч §1 корректно классифицирует внешние
     vs межкомнатные двери на типовом доме (крыльцо/навес не даёт ложный `hitSelf`).
     Probe: `raycast` из точки двери по «центр→дверь» и обратно, сравнить `hitObj == building`.
   - [нужно проверить] `IsSoundInsideBuilding()` возвращает `true/false` (не всегда
     `false`) на `INSTANCETYPE_AI_SERVER`-боте внутри/снаружи здания. Probe: `botdump`
     пешку внутри и снаружи.
   - [нужно проверить] `GetCollisionBox` возвращает world-space AABB (центр корректен для
     повёрнутого здания). Probe: вывести `minMax` + `GetPosition` + `GetBoundingCenter`
     для повёрнутого дома.
   - [нужно подтвердить] `GetPosition()` здания = центр футпринта (а не угол привязки) для
     целевых типов зданий — влияет только на точность оси «наружу», не на корректность
     классификации (AABB-центр надёжнее).

---

## Существенные развилки / решения (для `docs/decisions.md`)

1. **Внешняя дверь ≠ нативный признак.** Решение: детект геометрией («выходной» луч
   `RayCastBullet` + `hitObj == building`), а не поиск нативного exterior-флага (его нет —
   подтверждено полным чтением `building.c` и логикой Expansion `HandleBuildingDoors`).
   Почему: `GetDoorCount()` отдаёт и межкомнатные двери, отличить их можно только по
   тому, что «за дверью» (открытое небо vs стена своего здания). Действует пока: не
   появится нативный exterior/orientation-признак двери.

2. **`PhxInteractionLayers` — исключение из запрета `|`.** Решение: для маски слоёв
   `RayCastBullet`/`SphereCastBullet` комбинировать через `|` (ванильный канон), НЕ через
   «два отдельных вызова» (как для `QueryFlags`/`CollisionFlags`). Почему: `PhxInteractionLayers`
   — нативный битмасковый enum (ваниль `|`-ит его повсеместно, `environment.c:406` и др.),
   а `QueryFlags`/`CollisionFlags` — последовательные enum'ы, где `|` даёт чужой член.
   Действует пока: движок держит `PhxInteractionLayers` битмасковым.

3. **Ось «наружу» = «центр здания → точка двери»** (натива ориентации нет). Центр — из
   `GetCollisionBox` (не `GetPosition`, чья семантика «центр vs угол привязки» под вопросом).
