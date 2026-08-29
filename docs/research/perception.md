# Research: зрение и слух ботов (perception)

Статус: **не реализовано**. Задача T3 плана (`docs/plans/ai-development-plan.md`) —
детект игроков и заражённых. Ведёт субагент `dayz-research`.

## Цель

Бот «видит» заражённых (зомби) и игроков в радиусе/FOV/при прямой видимости;
результат → цели (`dmTarget`) для боя/реакции. Позже — и предметы (лут) и звуки (слух).

## Известные стартовые точки (проверены в `DayZ Projects/scripts`)

- Игроки: `GetGame().GetPlayers(array<Man> players)` → `PlayerBase`.
- Пространственные запросы (`4_world/classes/dayzplayerutils.c`):
  - `static proto native void PhysicsGetEntitiesInBox(vector min, vector max, notnull out array<EntityAI> entList);`
  - `SceneGetEntitiesInBox(minPos, maxPos, out array<EntityAI>, QueryFlags.STATIC|QueryFlags.DYNAMIC)` (env, universaltemperaturesource).
  - `GetEntitiesInCone(playerPos, headingDirection, coneAngle, maxDist, coneHeightMin, coneHeightMax, out vicinityObjects)` (actiontargets.c:287) — конус по направлению взгляда.
- Классы существ: `ZombieBase` (extends `DayZInfected`), `ZombieMaleBase : ZombieBase`,
  `AnimalBase` (extends `DayZAnimal`).
- LOS: `DayZPhysics.RaycastRV(beg, end, out pos, out dir, out comp, null, null, ignore, false, false, ObjIntersectView)`.
- FOV-проверка: угол между направлением корпуса/головы и вектором на цель (по `AngleDiff`).

## Открытые вопросы (заполнить research-субагенту)

1. Точный способ перечислить заражённых рядом: box/cone против чего (радиус? карта
   типов `CfgVehicles`?). Что дешевле — `SceneGetEntitiesInBox` или `PhysicsGetEntitiesInBox`?
2. Сигнатура `GetEntitiesInCone` (входы/выходы) — есть ли `out array<EntityAI>`.
3. Как фильтровать по типу: `GetType()`, `IsInherited(ZombieBase)`, `IsInherited(PlayerBase)`?
4. Оптимальный период скана (троттлинг) и радиус/FOV константы.
5. Есть ли встроенный в ваниллу способ «видит ли A объект B» (LOS-хелпер), или делать
   свой raycast.

## Источники

- `DayZ Projects/scripts/4_world/classes/dayzplayerutils.c`
- `DayZ Projects/scripts/4_world/classes/useractionscomponent/actiontargets.c`
- `DayZ Projects/scripts/4_world/entities/creatures/infected/zombiebase.c`
- `DayZ-Expansion-Scripts/.../eAIBase.c` (их перцепция — референс, но Expansion-специфична)
