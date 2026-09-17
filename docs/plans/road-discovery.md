# Дорожный дискаверинг — дизайн зонда (итерация 1)

Итеративный план: сначала зонд-«ходок» (следует за дорогой, не ломается), потом
граф + перекрёстки + дедуп, потом JSON + PNG.

## Датчик: `dmRoadSensor` (материал поверхности)

Единственный источник — `GetGame().SurfaceGetType(x, z, out string type)` → имя поверхности.
Классификация по подстроке имени (регистронезависимо, порядок важен):

| приоритет | подстрока | класс |
|---|---|---|
| 1 | `roof` / `planks` / `tiles` | STRUCTURE |
| 2 | `water` / `pond` / `sea` | WATER |
| 3 | `concrete` | PAVED |
| 4 | `dirt` | DIRT |
| 5 | `broadleaf` / `conifer` | FOREST |
| 6 | `grass` | GRASS |
| 7 | иначе | UNKNOWN |

`IsDrivable = PAVED || DIRT`. (Эмпирика: асфальт=`concrete_ext`, бетон=`cp_concrete1`,
грунт=`dirt_ext`/`cp_dirt`, трава=`cp_grass*`, лес=`cp_broadleaf*`/`cp_conifer*`,
крыша=`ceramic_tiles_roof_ext`/`wood_planks_ext`.)

## Зонд-«ходок»: `dmRoadProbe` (итерация 1)

Дискретный, синхронный, с ограничением шагов. Следует за центральной линией дороги.

Состояние: `P` (точка центра, x/z), `D` (единичный вектор направления), `width`.

### FindDirection(P) — направление дороги в точке (двойной угол)
24 сэмпла по кругу радиуса `DIR_RADIUS` (4 м), шаг 15°. Для каждого drivable сэмпла
накапливаем `cos(2θ)`, `sin(2θ)`. Направление = `0.5·atan2(Σsin2θ, Σcos2θ)`.
(Двойной угол гасит встречное направление: дорога идёт в обе стороны.) Нет drivable →
`null`.

### ReCenter(P, D) — центр + ширина по краям
Скан перпендикуляром `N = (-D.z, D.x)` от P в обе стороны шагом `EDGE_STEP` (0.5 м) до
`MAX_HALF_WIDTH` (12 м). Края — где поверхность перестаёт быть drivable.
`center = P + N·(left−right)/2`, `width = left+right`.

### TurnFan(P, D) — доворот на повороте
Пробуем углы {15, 30, 45, 60, 75}° в обе стороны; первый drivable на дистанции `STEP`
возвращаем как новое направление. Нет → `null` (тупик).

### Walk(seed) — цикл
```
P = seed; D = FindDirection(P)         // нет направления -> nodir
записать P
for step in 1..MAX_STEPS:
  P_next = P + D·STEP
  if !drivable(P_next):
    D = TurnFan(P, D); если null -> deadend
    P_next = P + D·STEP; если !drivable -> deadend
  center,width = ReCenter(P_next, D)
  D = normalize(center − P)            // новый тангенс
  P = center
  записать P (x,z, y=SurfaceY, width, surfaceClass)
  если step > MIN_CLOSE_STEPS и dist(P,seed) < CLOSE_DIST -> loop (замкнулась)
return ok/maxsteps
```

### Результат
`dmRoadProbeResult`: полилиния точек (x, y, z) + ширина + класс поверхности на точку +
статус + число шагов.

## Константы (итерация 1)
- `DM_ROAD_STEP = 3.0` м; `DM_ROAD_DIR_RADIUS = 4.0` м; `DM_ROAD_EDGE_STEP = 1.0` м.
- `DM_ROAD_MAX_HALF_WIDTH = 12.0` м; `DM_ROAD_MAX_STEPS = 200`.
- `DM_ROAD_MIN_CLOSE_STEPS = 10`; `DM_ROAD_CLOSE_DIST = 6.0` м.
- TurnFan углы: {15, 30, 45, 60, 75}°.

## Запуск (итерация 1)
E2E-оп `roadwalk` в `test` (мост) вызывает `dmRoadProbe.Walk(step.Pos)` и дампит полилинию
в `r.Dump`. Точки-сиды — 8 точек пользователя (+ тропинка для проверки, что зонд её не
берёт/сразу тупик).

## Дальше (итерации 2+)
- Перекрёстки: веер лучей → развилка → новые зонды на ветки.
- Дедуп/замыкание: сетка пройденных ячеек.
- Граф: вершины (сиды/перекрёстки) + рёбра (полилинии), вес = длина/покрытие/уклон/ширина.
- Вывод JSON-графа + PNG (Python-скрипт по JSON).
