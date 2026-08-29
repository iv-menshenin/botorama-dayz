# Research: зрение и слух ботов (perception)

Статус: **реализовано (T3)**. Задача T3 плана (`docs/plans/ai-development-plan.md`) —
детект игроков, заражённых и животных. Слух (звуки) и перцепция предметов (лут) —
следующие вехи (T13). Ведёт субагент `dayz-research`.

## Цель

Бот «видит» заражённых (зомби), животных и игроков в радиусе/FOV/при прямой
видимости; результат → цели (`dmTarget`) для боя/реакции. Позже — и предметы (лут)
и звуки (слух).

## Реализация (T3)

`core/4_World/Entities/Bot/Perception/dmVision.c` — поле `ref dmVision m_Vision` в
мозге `dmAISurvivor`; тикает из `OnUpdate` с троттлингом `DM_PERCEPTION_INTERVAL`
(0.3 c). Пайплайн `Scan()`: box-запрос → классификация → дистанция/FOV → LOS →
`dmTarget` (DESTROY). Переключение Scene/Physics — `ToggleQuery()` (команда
`/bot vision switch`).

## Проверенные сигнатуры (ваниль, `DayZ Projects/scripts`)

- `4_world/entities/dayzplayerutils.c`:
  - `static proto native void PhysicsGetEntitiesInBox(vector min, vector max, notnull out array<EntityAI> entList);`
  - `static proto native void SceneGetEntitiesInBox(vector min, vector max, notnull out array<EntityAI> entList, int flags = QueryFlags.DYNAMIC);`
- `Object.IsInherited(typename baseType)` → `bool` (напр. `entity.IsInherited(ZombieBase)`).
  Игрок — `PlayerBase.Cast(entity)` (null, если не игрок).
- Классы существ: `ZombieBase extends DayZInfected`
  (`4_world/entities/creatures/infected/zombiebase.c`), `AnimalBase extends DayZAnimal`
  (`4_world/entities/creatures/animals/animalbase.c`).
- Кости: `GetBoneIndexByName("Head")` → `int` (-1 = нет) — **НЕ на `EntityAI`**, а на
  `Human` (игроки) и `DayZCreature` (зомби/животные) → каст `Human.Cast` / `DayZCreature.Cast`
  перед вызовом. `GetBonePositionWS(int)` → `vector` и `GetBoneTransformWS(int, out vector
  transform[4])` — на `Object` (общий базовый, без каста). **forward = `transform[1]`**
  (костное пространство, индекс 1).
- LOS (`3_game/global/dayzphysics.c`):
  - `proto static bool RaycastRVProxy(notnull RaycastRVParams in, out notnull array<ref RaycastRVResult> results, array<Object> excluded = null);`
  - `class RaycastRVParams` (нативный, создаётся `new` БЕЗ `ref`): поля `vector begPos`,
    `vector endPos`, `Object ignore`, `Object with`, `float radius`, `CollisionFlags flags`,
    `int type`, `bool sorted`, `bool groundOnly`. Конструктор
    `RaycastRVParams(vBeg, vEnd, pIgnore = null, fRadius = 0.0)` ставит дефолты
    `flags = CollisionFlags.NEARESTCONTACT`, `type = ObjIntersectView`, `sorted = false`.
  - `class RaycastRVResult`: `Object obj`, `Object parent`, `vector pos`, `vector dir`,
    `int hierLevel`, `int component`, `SurfaceInfo surface`, `bool entry`, `bool exit`.
  - Массив результатов — `array<ref RaycastRVResult>` (эталон `actiontargets.c:217`).

## Решения открытых вопросов

1. **Как перечислить сущности рядом**: box-запрос, НЕ конус и НЕ `GetPlayers()` вручную.
   `SceneGetEntitiesInBox` (по сцене, ловит DYNAMIC) и `PhysicsGetEntitiesInBox` (по
   физике) — обе дешёвые; выбор вынесен в переключатель `m_UseScene` (по умолчанию
   Scene). Дешевизну конкретной сцены можно проверить `/bot vision switch` + дебаг-лог.
2. `GetEntitiesInCone` — **не использован** (box + FOV-фильтр достаточен и проще).
3. Фильтр по типу — `PlayerBase.Cast(e)` (игрок/бот), `e.IsInherited(ZombieBase)`,
   `e.IsInherited(AnimalBase)`; порядок player → zombie → animal.
4. Троттлинг — `DM_PERCEPTION_INTERVAL = 0.3` c; радиус `DM_PERCEPTION_RADIUS = 30.0` м;
   FOV `DM_PERCEPTION_FOV = 120.0`° (полуугол 60°); высота бокса `DM_PERCEPTION_HEIGHT = 2.0` м.
5. Встроенного «видит ли A объект B» нет — свой raycast через `RaycastRVProxy`
   (глаза→голова цели; видно, если ближайшее попадание — сама цель).

## Источники

- `DayZ Projects/scripts/4_world/classes/dayzplayerutils.c`
- `DayZ Projects/scripts/3_game/global/dayzphysics.c`
- `DayZ Projects/scripts/4_world/classes/useractionscomponent/actiontargets.c`
- `DayZ Projects/scripts/4_world/entities/creatures/infected/zombiebase.c`
- `DayZ Projects/scripts/4_world/entities/creatures/animals/animalbase.c`
- `DayZ-Expansion-Scripts/.../eAIBase.c` (их перцепция — референс, но Expansion-специфична)
