# E2E-автотесты: файловый мост для ИИ-агента

Статус: `[ ]` не начато · `[~]` в работе · `[x]` готово.

## Цель

Дать ИИ-агенту возможность управлять тестовым сервером и читать результат **без
игрока и без HTTP-сервиса** — через файловую систему. Агент кладёт JSON-сценарий,
мод исполняет его на сервере, агент читает JSON-результат и tail'ит RPT-лог.

Полный цикл (см. `docs/ai-testing-guide.md`): сборка → деплой → старт → сценарий →
результат → фиксация.

## Почему файлы, а не HTTP

- Файл — природная транзакция (запись во временный файл + `rename` = атомарность).
- Нет процесса-сервиса, нечего держать живым.
- Результат лежит рядом с логом сервера — агент читает одну папку профиля.

## Каталог

Под `$profile:dmBotorama/` (профиль сервера — `profiles-cherno/`):

```
$profile:dmBotorama/e2e/
  enabled                 # маркер: мост активен, только если файл существует
  in/                     # агент кладёт сценарии <job>.json
  done/                   # обработанные входы (история)
  out/                    # мост пишет <job>.result.json
```

Атомарность: агент пишет `in/<job>.json.tmp` → `rename` в `in/<job>.json`; мост
сканирует **только `*.json`** (не `.tmp`). Мост пишет результат в `out/<job>.json.tmp`
→ `rename`.

## Сценарий (вход) — `in/<job>.json`

```json
{
  "Name": "hello-001",
  "Timeout": 60,
  "Steps": [
    { "Op": "ping" },
    { "Op": "spawn", "Who": "A", "Pos": [12600.0, 0.0, 9500.0], "Yaw": 0.0 },
    { "Op": "snapshot" },
    { "Op": "clearall" }
  ]
}
```

Структуры (`test/4_World`, поля сериализуются по точному имени, без `m_`):

```c
class dmE2EJob
{
    string Name;                          // id сценария (= имя файла)
    float Timeout;                        // общий таймаут, секунды
    autoptr array<ref dmE2EStep> Steps;
}

class dmE2EStep
{
    string Op;        // ping | spawn | moveto | follow | patrol | speed | loadout |
                      // stance | look | say | wait | assert | snapshot | clearall | killall
                      // + пробы мира: spawnobj | raycast | scanbox | botdump | getpos | setpos | clearobj
    string Who;       // имя бота (кем управляем) / имя объекта (spawnobj/botdump)
    string Target;    // имя цели-бота (follow / distance)
    vector Pos;       // [x,y,z] мировая точка
    float Yaw;        // ориентация при спавне (градусы)
    autoptr array<vector> Points;   // точки патруля [[x,z],…]
    float Speed;      // предпочтительная скорость (1..3)
    string Loadout;   // имя loadout
    string Cond;      // условие wait/assert: state|reached|distance|alive|moving
    string Value;     // значение условия ("Follow", "true"/"false", …)
    float Tolerance;  // допуск для reached/distance (метры)
    float Timeout;    // таймаут шага (wait), секунды
    string ClassName; // класс CfgVehicles (spawnobj)
    vector From;      // точка A (raycast)
    vector To;        // точка B (raycast)
    vector Min;       // min-угол коробки (scanbox)
    vector Max;       // max-угол коробки (scanbox)
    string Obj;       // имя объекта (getpos/setpos/clearobj)
}
```

## Словарь команд

| Op | поля | что делает в моде |
|---|---|---|
| `ping` | — | отклик «жив» (проверка канала) |
| `spawn` | `Who`, `Pos`, `Yaw` | `new dmAISurvivor().Spawn(SnapToGroundExactly(Pos), Vector(Yaw,0,0))` + регистрация в `map<name, ref dmAISurvivor>` + «кик» физики |
| `moveto` | `Who`, `Pos` | `dmBotIntent_MoveTo` (m_Goal=Pos, CRITICAL, deadline) → `AddCommandIntent` |
| `follow` | `Who`, `Target` | `botWho.SetFollowTarget(botTarget.GetPawn())` + `SetFSM(dmBotPreset_Escort.Create(...))` |
| `patrol` | `Who`, `Points` | `AddPatrolPoint` ×N + FSM Patrol/Idle |
| `speed` | `Who`, `Speed` | `SetPreferredSpeed(Speed)` |
| `loadout` | `Who`, `Loadout` | `dmLoadoutApplier.Load/Apply` |
| `look` | `Who`, `Pos`/`Target` | `dmBotIntent_HoldLook` |
| `say` | `Who`, `Value` | `SpeakLine(int)` |
| `wait` | `Who`, `Cond`, `Value`/`Target`/`Pos`, `Tolerance`, `Timeout` | **отложенный**: тикается до условия или таймаута |
| `assert` | то же | мгновенная проверка → `Ok`/`Reason` |
| `snapshot` | — | снять состояние всех именованных ботов |
| `clearall` / `killall` | — | `dmAISurvivor.ClearAll()` / `KillAll()` |

Условия (`Cond`): `state` (имя FSM-состояния), `reached` (дистанция до `Pos` <
`Tolerance`), `distance` (дистанция до `Target` < `Tolerance`), `alive`/`moving`
(`Value` = `"true"/"false"`).

## Пробы мира (probe-операции) — для гипотез `dayz-research`

Отдельная поверхность: проверка окружения/физики, а не поведения ботов. Дамп идёт и в
`Steps[].Dump` (result-JSON), и в RPT (домен `DM_BOT_DEBUG_E2E`). Все — мгновенные.

| Op | поля | что делает | Reason |
|---|---|---|---|
| `spawnobj` | `Who` (имя), `ClassName`, `Pos`, `Yaw` | `GetGame().CreateObject(ClassName, SnapToGroundExactly(Pos), false)` + `SetOrientation`; регистрация в `m_Objects[Who]` | `spawned <Class>` |
| `raycast` | `From`, `To` | снап `From`/`To` на землю + `DM_E2E_EYE_HEIGHT` (1.8м); `DayZPhysics.RaycastRVProxy` с `CollisionFlags.ALLOBJECTS`; дамп всех хитов (obj/parent/pos/dist/component) | `<N> hits` / `0 hits (clear)` |
| `scanbox` | `Min`, `Max` | `SceneGetEntitiesInBox` **два вызова** (DYNAMIC + STATIC, т.к. `QueryFlags` — не битмаска); дамп `ent[D]`/`ent[S]` | `<N> entities` |
| `botdump` | `Who` (имя бота) | дамп тела/движения/мозга: pos, alive/unconscious/restrained/bleeding, health/blood/shock, stamina, vel, orient, fsm, fsmIntents | `dumped` |
| `getpos` | `Obj` | позиция + yaw объекта | — |
| `setpos` | `Obj`, `Pos`, `Yaw` | `SetPosition(SnapToGroundExactly)` + `SetOrientation` | — |
| `clearobj` | `Obj` (`"*"` — все) | удалить объект(ы) из мира и реестра | `cleared N` |

Примечания:
- `spawnobj`/`getpos`/`setpos`/`clearobj` работают с реестром `m_Objects`
  (`map<string, Object>`), боты — с `m_Named`. Реестры чистятся по завершении job.
- `raycast` с 0 хитов = чистый LOS (луч не встретил препятствие между A и B).
- `botdump` выгружает FSM-интенты; **командные интенты пока не выгружаются** (нет
  публичного геттера `m_CommandIntents` в `dmAISurvivor` — техдолг, добавить при нужде).

## Результат (выход) — `out/<job>.result.json`

```json
{
  "Name": "hello-001",
  "Status": "ok",          // ok | error | timeout
  "Error": "",
  "Steps": [
    { "Index": 0, "Op": "ping", "Ok": true, "Reason": "" },
    { "Index": 1, "Op": "spawn", "Ok": true, "Reason": "A spawned" }
  ],
  "Snapshot": [
    { "Name": "A", "Alive": true, "Pos": [12600.0, 47.2, 9500.0], "State": "Idle", "Moving": false }
  ]
}
```

```c
class dmE2EResult
{
    string Name;
    string Status;
    string Error;
    autoptr array<ref dmE2EStepResult> Steps;
    autoptr array<ref dmE2ESnapshot> Snapshot;
}
class dmE2EStepResult { int Index; string Op; bool Ok; string Reason; autoptr array<string> Dump; }
class dmE2ESnapshot  { string Name; bool Alive; vector Pos; string State; bool Moving; }
```

## Данные в лог (два канала)

1. **Структурный результат** — `out/*.result.json`: PASS/FAIL по шагам + снапшот.
   Это источник истины для решения агента.
2. **RPT-лог** (free-form, домен `DM_BOT_DEBUG_E2E`) — жизненный цикл моста для
   триажа: «enabled», «job X picked up», «step N: op=…», «job X → status=…», ошибки.
   Всегда через `#ifdef DM_BOT_DEBUG_E2E` на месте вызова (см. `docs/codeguide.md`,
   «Логирование»); `dmBotLog.Error` — без гейта.

## Исполнитель — `dmE2EBridge` (синглтон)

- Тикается из `test/5_Mission/MissionServer.c` `OnUpdate` (моддед-патч).
- Гейт: если нет файла `e2e/enabled` → сразу выход (нулевой оверхед).
- Сканирует `in/*.json` (`FindFile` + `FindNextFile`) раз в `DM_E2E_SCAN_INTERVAL`.
- Исполняет **один** сценарий за раз (очередь, детерминированность): пошаговый
  автомат `m_StepIndex` + таймер для `wait`/`assert`.
- `map<string, ref dmAISurvivor> m_Named` — именование ботов на время сценария;
  чистка после.
- По завершении: пишет `out/<job>.result.json`, переносит вход в `done/`.

## Размещение файлов (по конвенциям `test/`)

- `test/3_Game/constants.c` — `DM_E2E_DIR`/`DM_E2E_IN_DIR`/…, `DM_E2E_SCAN_INTERVAL`,
  имена `Op`/`Cond` (строки), `DM_BOT_DEBUG_E2E` (doc-упоминание).
- `test/4_World/dmE2EBridge.c` — структуры + синглтон + исполнитель шагов.
- `test/5_Mission/MissionServer.c` — хук `OnUpdate → dmE2EBridge.Get().Tick()`.

## Привязка к Enfusion (готчи)

- JSON-структуры: поля по точному имени, **имена полей ≠ имена типов** (`Pos`, а не
  `Position` — ок; запрещено `World`/`Class`/`Object`/`Entity`). Массивы — `autoptr`.
- `JsonSerializer` не применяет инициализаторы: `float Timeout = 0;` при отсутствии
  ключа читается как `0.0` — дефолты нормализовать после `Load()`.
- Чтение каталога — `FindFile(path + "/*.json", …, FindFileFlags.DIRECTORIES)`.
- Векторы: `vector Pos` ↔ `[x,y,z]`.
- `string.ToLower()` мутирует на месте и возвращает `int` — отдельный statement.

## Фазы

1. `[x]` **Hello world** — полный цикл без игрока: `ping` + `spawn` + `snapshot` +
   `clearall` (все — мгновенные, без отложенных `wait`). Проверено: агент кладёт
   `hello-001.json` → в `out/hello-001.result.json` `Status:"ok"` + снапшот бота `A`
   (`Alive=1`, `Pos=[…]`). Реализация — `src/test/4_World/dmE2EBridge.c`,
   `src/test/5_Mission/MissionServer.c` (хук OnUpdate), `DM_E2E_*` в `test/3_Game/constants.c`.
2. `[x]` **Пробы мира** — `spawnobj`/`raycast`/`scanbox`/`botdump`/`getpos`/`setpos`/
   `clearobj` (мгновенные, дамп в `Steps[].Dump` + RPT). Для проверки гипотез
   `dayz-research`. `botdump` пока без командных интентов (нет геттера `m_CommandIntents`).
3. `[ ]` **Движение + follow** — `moveto`, `follow`, `speed`, `patrol` + условия
   `state`/`reached`/`distance`/`alive`/`moving` (отложенный автомат).
4. `[ ]` **Агентская обвязка** — скрипт «сценарий → poll результата» + tail RPT.
5. `[ ]` Опционально: `watch` (таймсерия состояния), резолв локаций по
   `dmWorldPOIRegistry`.

## Готчи, вскрытые на hello-world (см. `docs/codeguide.md`)

- `dmJsonFile<T>` делает `dmJsonConfigBase.Cast(config)` (версионирование) → для
  структур БЕЗ наследования `dmJsonConfigBase` шаблон не компилируется
  (`Types ... are not related`). Для e2e-структур использовать **`JsonFileLoader<T>`
  напрямую** + локальный `EnsureDir` (копия `dmJsonFile.EnsureDirectory`).
- output-массивы (`Steps`/`Snapshot`) — `ref array<ref T>` + явный `new`, НЕ `autoptr`
  (иначе `Insert` — silent no-op и в результате `[]`).
- `bool` в JSON сериализуется как `1`/`0`.
