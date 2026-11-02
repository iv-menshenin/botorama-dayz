# Research: точки входа лестниц (ladderN_con vertices) и спуск с промежуточной платформы

Статус: баг `Land_Tower_TC1` — бот на промежуточной платформе (Y≈363.6) не может
спуститься, т.к. лестница (низ 351.76 → верх 379.945, ~28 м) проходит сквозь его
уровень, а кэш хранит только низ/верх.

## Цель

1. Сколько вершин реально содержит selection `ladderN_con` (2 или N — по точке на ярус).
2. Как хранить промежуточные точки в `dmBotLadder`.
3. Как Expansion парсит `_con` и что делает с >2 вершинами (`InsertVertex`).
4. Почему ванильный `GetLaddersCount()` возвращает 0 у серверного ИИ и как ваниль
   использует `_con` для attach на промежуточном ярусе.
5. Вывод для rework кэша + `near/far`/`level-gate` в `dmBotPathfinder`.

## Ключевой вывод (читать первым)

**Selection `ladderN_con` рассчитан на N точек входа** (по одной на каждый ярус/платформу),
а `ladderN_con_dir` — на N парных точек направления. Это доказывает ванильный клиентский
`ActionEnterLadder.ActionCondition`: он перебирает **все** вершины `_con`
(`for j < memSelection[i].GetVertexCount()`) и выбирает **ближайшую** к игроку в радиусе
`UAMaxDistances.LADDERS` (1.3 м), затем ближайшую `_con_dir`-вершину к выбранной точке.

**Обе реализации ИИ — и наша, и Expansion — срезают это до 2 вершин (мин/макс Y).**
Поэтому обе не умеют спускаться/подниматься с промежуточной платформы вышки, где у
`_con` >2 вершин. Это НЕ «недостаток только нашего кэша», а унаследованное упрощение
Expansion, которое для обычных лестниц (2 точки) работает, а для вышек — нет.

## Таблица: хранение точек входа

| Аспект | botorama (`dmBotLadderCache`) | Expansion (`BuildingBase`) | Ваниль |
|---|---|---|---|
| Источник | `dmBotLadderCache.c` | `BuildingBase.c` (modded) | `actionenterladder.c` (клиент) / `building.c` (натив) |
| Парсит `_con` | да, `CollectConVertices` (стр.165) | да, `Expansion_GetLaddersCount` (стр.297) | да, `ActionCondition` (стр.62-75) |
| Сколько вершин `_con` хранит | **2** (`m_Bottom`/`m_Top`, мин/макс Y) | **2** (`m_Con[2]`, через `InsertVertex`) | **N** (перебор всех, выбор ближайшей) |
| `_con_dir` | **2** (`m_BottomDir`/`m_TopDir`) | **2** (`m_ConDir[2]`) | **N** (ближайшая к выбранной `_con`) |
| Судьба промежуточных вершин | **выброшены** (мин/макс) | **выброшены** + баг при >2 (см. ниже) | **используются** (ближайшая по дистанции) |
| Парность `_con`↔`_con_dir` | по индексу (неявно) | по индексу (неявно) | **по близости** (dist от entry) |
| `GetLaddersCount()` | не используется | нативный «всегда 0» (комментарий стр.296) | натив, у AI «всегда 0» |

## Подтверждённые сигнатуры (файл + строка)

### Ваниль

- `Building` (`3_game/entities/building.c:12-14`):
  - `proto native int GetLaddersCount();`
  - `proto native vector GetLadderPosTop(int ladderIndex);`
  - `proto native vector GetLadderPosBottom(int ladderIndex);`
  — нативные, **только низ/верх** (промежуточных точек в нативном API нет в принципе).
- `Selection` (`3_game/gameplay.c:179-197`):
  - `GetVertexCount()` (стр.185) — число вершин **selection'а**.
  - `GetLODVertexIndex(int sel_vertex_index)` (стр.186) — маппинг selection-вершины →
    LOD-вершину; может вернуть **-1**.
  - `GetVertexPosition(LOD lod, int index)` (стр.188) — внутри делает
    `GetLODVertexIndex` и при `-1` возвращает `vector.Zero` + `Error("Vertex doesn't exist")`.
- `UAMaxDistances.LADDERS = 1.3` (`4_world/.../actionconstants.c:115`).
- `ActionEnterLadder.ActionCondition` (`4_world/.../interact/actionenterladder.c`):
  - стр.65: `for (j = 0; j < memSelection[i].GetVertexCount(); j++)` — перебор **всех** `_con`.
  - стр.68: `vector.DistanceSq(ladderEnterPointGlobal,pos) < UAMaxDistances.LADDERS^2` —
    выбор **ближайшей** точки входа к игроку.
  - стр.84-90: аналогично перебор `_con_dir`, выбор `minDistanceSq` к выбранной entry.
  - стр.122/137: `HumanCommandLadder.DebugGetLadderIndex(compName)` →
    `StartCommand_Ladder(b, ladderIndex)`.
  — это **клиентский** action (реагирует на cursor-компонент), для серверного ИИ
  используется тот же нативный `StartCommand_Ladder` напрямую.

### Expansion

- `ExpansionLadder` (`BuildingBase.c:1-41`): поля `m_Con[2]` (стр.10), `m_ConDir[2]`
  (стр.11), `m_ConCount`/`m_ConDirCount` (стр.7-8). **Фиксированные массивы на 2** —
  промежуточных точек не хранит.
- `InsertVertex(vector, inout vector target[2], inout int count)` (`BuildingBase.c:23-41`):
  ```c
  static void InsertVertex(vector vertex, inout vector target[2], inout int count)
  {
      if (count == 0) { target[count++] = vertex; }
      else
      {
          vector v0 = target[0];
          if (vertex[1] < v0[1]) { target[0] = vertex; target[1] = v0; }
          else { target[1] = vertex; }
      }
  }
  ```
  **Баг при >2 вершин**: `count` инкрементится только в первой ветке (остаётся 1), а в
  `else` сравнивает только с `target[0]` (текущим минимумом). При 3+ вершинах новый минимум
  перезаписывает `target[1]` прежним минимумом — **текущий максимум теряется**.
  Трассировка (Y = 10, 20, 5): `[10]` → `[10,20]` → `[5,10]` (20 потерян). Корректно
  работает только при ровно 2 вершинах.
- `Expansion_GetLaddersCount()` (`BuildingBase.c:297`): парсит `LOD.NAME_MEMORY`,
  `geometry.GetPropertyName("laddertype")`; `_con` → `InsertVertex(m_Con, m_ConCount)`
  (стр.397), `_con_dir` → `InsertVertex(m_ConDir, m_ConDirCount)` (стр.400).
  Кэш: `static map<string, map<int, ref ExpansionLadder>> s_Expansion_BuildingsWithLadders`.
- `OverrideTargetPosition` (`eAIBase.c:4742-4900`): выбирает только `m_Con[0]` (низ,
  `climbDir=1`) или `m_Con[1]` (верх, `climbDir=-1`) по `Distance2DSq * ΔY`; промежуточный
  уровень не учитывается. Т.е. Expansion **тоже не умеет** спускаться с промежуточной
  платформы вышки (если только вышка не разбита на отдельные лестницы-сегменты).

### botorama

- `dmBotLadder` (`dmBotLadderCache.c:16-24`): только `m_Index/m_Type/m_Bottom/m_Top/
  m_BottomDir/m_TopDir`.
- `CollectConVertices` (`dmBotLadderCache.c:165-190`): перебор
  `selection.GetVertexCount()`, оставляет только мин/макс Y → `m_Bottom`/`m_Top`.
  **Промежуточные вершины выброшены.**
- `CollectConDirVertices` (`dmBotLadderCache.c:194-219`): то же для `_con_dir`.
- `FindRouteRecursive` (`dmBotPathfinder.c:196-352`): level-gate стр.273
  `if (Math.Min(dBottom, dTop) > DM_FALL_DANGER_DROP) continue;`, near/far стр.281-289.
- `FindRouteOffNavmesh` (`dmBotPathfinder.c:358-479`): тот же gate стр.401, near/far стр.409-417.
- `dmBotIntent_UseLadder.UpdateEntry` (`dmBotIntent_UseLadder.c:38-44`): вход =
  `m_Bottom` (dir>0) / `m_Top` (dir<0) — **только крайние точки**, промежуточной нет.
- Константы: `DM_FALL_DANGER_DROP = 2.75` (`cons/4_World/constants.c:888`),
  `DM_NAV_GAP = 1.5` (стр.249), `DM_NAV_MAX_DEPTH = 6` (стр.253),
  `DM_PATH_SAMPLE_RADIUS = 2.0` (стр.242).

## Почему `GetLaddersCount()` возвращает 0 у серверного ИИ

Не подтверждено из исходников (метод `proto native` — реализация в движке, не в скриптах).
Зафиксировано как **наблюдение** в двух местах:
- наш комментарий `dmBotLadderCache.c:3` («Ванильный Building.GetLaddersCount() у
  серверного ИИ возвращает 0»);
- Expansion `BuildingBase.c:296` («//! Vanilla GetLaddersCount seems to always return 0?»).

Причина не видна статически — `[нужно подтвердить]`, но для rework несущественна: даже
рабочий `GetLadderPosTop/Bottom` даёт **только низ/верх**, промежуточных точек нативный API
не отдаёт, поэтому парсить `_con` всё равно придётся самим.

## Вывод для rework

### 1. Кэш (`dmBotLadder` / `CollectConVertices`)

- Заменить `m_Bottom/m_Top` на `ref array<vector> m_Vertices` (все вершины `_con`,
  **отсортированные по Y**), и `m_BottomDir/m_TopDir` на `ref array<vector> m_VertDirs`
  (парные направления).
- **Парность `_con`↔`_con_dir` — по близости, как ваниль**, а не по индексу: для каждого
  `_con`-входа выбрать ближайшую `_con_dir`-вершину. Порядок/число вершин у `_con` и
  `_con_dir` может не совпадать (ваниль ищет `minDistanceSq` от entry).
- Обойти готчу `Selection.GetVertexPosition` возвращает `vector.Zero` при
  `GetLODVertexIndex == -1` (см. `gameplay.c:188-197`): фильтровать
  `GetLODVertexIndex(j) > -1` перед чтением (как Expansion стр.380-383), иначе
  `vector.Zero` станет «низом» и сорвёт min/max. Сейчас наш код этого не делает.
- `m_Bottom`/`m_Top` для совместимости можно оставить как getter-свойства
  (`m_Vertices[0]` / `m_Vertices[last]`), чтобы не трогать `UseLadder` сразу.

### 2. Роутинг (`dmBotPathfinder.FindRouteRecursive` / `FindRouteOffNavmesh`)

- **Level-gate** (стр.273/401) заменить с `min(|bottomY−lastY|, |topY−lastY|) > 2.75 → skip`
  на «**лестница проходит сквозь уровень бота**»:
  ```
  bottomY < botY < topY   → лестница входит в уровень, НЕ skip
  ```
  плюс обычный случай «бот уже у края» (`min(|ΔY|) <= 2.75`).
- **near/far** (стр.281-289/409-417) заменить на: `near` = **вершина `m_Vertices` с
  ближайшим world-Y к botY** (это и есть точка входа на уровне бота), `far` = противоположный
  конец (`m_Vertices[0]` или `m_Vertices[last]`, смотря по направлению к цели `to`).
  Для промежуточной платформы `far` — это **ближайшая к цели `to`** крайняя точка, а не
  «другой конец всей лестницы», иначе бот полезет через все 28 м вверх вместо спуска.
- **`UseLadder`**: `UpdateEntry` (стр.38-44) должен брать переданную точку входа (не
  `m_Bottom/m_Top`), т.е. `dmBotRouteSegment` / `dmBotLadder` должен нести **конкретный
  индекс входа** (`m_EntryIndex`), а не только направление. Сейчас сегмент несёт лишь
  `m_Direction`, а entry вычисляется по `m_Bottom/m_Top` — этого не хватит для «вход на
  уровне бота».

### 3. Что НЕ менять

- `StartCommand_Ladder(building, index)` / `SetClimbingLadderType(type)` /
  `HumanCommandLadder.CanExit/Exit` — нативный жизненный цикл не зависит от числа
  entry-точек; меняется только **выбор точки входа**.

## Открытые вопросы

- `[нужно проверить]` Реальное число вершин `ladderN_con` у `Land_Tower_TC1` — это
  **корневой факт** для фикса. Способы проверки (конкретно):
  1. Временный debug-лог в `CollectConVertices`:
     `dmBotLog.Debug("", "[Ladder] selection=" + name + " vertexCount=" + selection.GetVertexCount());`
     и по каждой вершине её `GetVertexPosition` (Y). Прогнать на тест-сервере, grep RPT по
     `[Ladder]`.
  2. Либо e2e-проба через существующую ручку моста (если `botdump`/`scanbox` даёт дамп
     memory-LOD лестниц — уточнить у `dayz-dev`, есть ли probe для `GetVertexCount`).
  Если у `_con` ровно 2 вершины (351.76 / 379.945), значит вышка смоделирована как **одна
  непрерывная лестница без промежуточных `_con`-точек**, и тогда фикс кэша не поможет —
  нужен другой подход (см. ниже «Развилка B»).
- `[нужно подтвердить]` Причина «`GetLaddersCount()` == 0 у серверного ИИ» (натив, не видно
  в скриптах). Probe: временный лог `building.GetLaddersCount()` на сервере для здания с
  лестницей, grep RPT.

## Гипотезы для эмпирической проверки (формат: вопрос → probe)

1. `[нужно проверить]` **Содержит ли `ladderN_con` у `Land_Tower_TC1` вершину на уровне
   Y≈363.6 (промежуточная платформа)?** Probe: debug-лог `GetVertexCount()` + Y каждой
   вершины в `CollectConVertices` → grep RPT. Ожидаемо (по модели ванили) ≥3 вершины.
2. `[нужно проверить]` **Совпадает ли число вершин `_con` и `_con_dir` у вышки, и парные ли
   они по порядку индексов?** Probe: лог обоих selection'ов (`vertexCount` + позиции),
   сверить Y-порядок. Если порядок не парный — парность «по близости» обязательна.
3. `[нужно проверить]` **Может ли `StartCommand_Ladder` присоединить бота к промежуточной
   точке входа (не низ/верх)?** Probe: на тест-сервере поставить бота на платформу 363.6,
   вызвать `StartCommand_Ladder(building, index)` с entry на 363.6 — присоединился ли
   (`IsClimbingLadder()`)? Это проверяет, что натив не привязывает только к краям лестницы.
4. `[нужно проверить]` **Пролезает ли бот сквозь платформу по лестнице при подъёме/спуске
   (нет ли физической блокировки на ярусе)?** Probe: e2e-сценарий «спуск с платформы вышки»,
   наблюдать `UseLadder` (застревание/`CanExit`).

## Существенные развилки и принятые решения (для docs/decisions.md)

### Развилка A: хранить N вершин vs 2 (как Expansion)
Решение (рекомендация): хранить **все** вершины `_con` + парные `_con_dir`, выбирать вход
по ближайшему Y к боту.
Почему: ванильный `ActionEnterLadder` доказывает, что `_con` рассчитан на N точек (перебор
всех + ближайшая). Expansion-подход «2 точки» — упрощение, которое само ломается на вышках
(и его `InsertVertex` багнут при >2). Класс задачи один: «лестница проходит сквозь уровень
бота» → выбираем вход на уровне бота.

### Развилка B: если у вышки `_con` окажется ровно 2 вершины (не подтвердилась гипотеза N)
Решение (запасное): вышка смоделирована одной непрерывной лестницей; промежуточную «точку
входа» синтезировать самому как проекцию позиции бота на прямую лестницы
(`bottom + (top-bottom) * t`, `t = (botY−bottomY)/(topY−bottomY)`), а `_con_dir` — линейно
интерполировать. Entry = снэпнутая на лестницу точка, `StartCommand_Ladder` — нативный.
Почему: это закроет спуск без изменения модели; но требует проверить, что натив прицепит
бота к середине лестницы (гипотеза 3). Проверять только если гипотеза 1 даст «2 вершины».

### Развилка C: где чинить «вход на уровне бота» — в кэше или в pathfinder
Решение: парность/хранение всех вершин — в кэше (`dmBotLadderCache`); выбор `near` по
ближайшему Y и новый level-gate — в `dmBotPathfinder`; конкретный индекс входа — пробросить
через `dmBotRouteSegment` в `UseLadder`.
Почему: кэш отвечает за «что за лестница», pathfinder — за «какой конец/точка», интент — за
«прицепиться». Сохраняет текущее разделение слоёв, не размазывает одну логику по трём местам.
