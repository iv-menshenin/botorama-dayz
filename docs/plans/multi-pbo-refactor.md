# Мульти-PBO рефакторинг + src/ + сборка/подпись ИИ-агентом

Статус: `[ ]` не начато · `[~]` в работе · `[x]` готово.

## Цель

Вместо одного `botorama.pbo` — по одному PBO из каждого верхнеуровневого модуля
(`cons`, `reg`, `core`, `map`, `loadout`, `test`). Каждый модуль получает свой
`config.cpp` (с префиксом и `requiredAddons`-цепочкой). Код модов отделяется от
прочего (доки/данные) папкой `src/`. Работа ведётся в отдельной ветке.

После рефакторинга — сборка/подпись всех PBO средствами ИИ-агента и проверка
работоспособности на сервере (`/mnt/deep-space/Steam/steamapps/common/DayZServer/@Botorama/Addons`,
ключ `/home/devalio/dayz/Work/Keys`).

## Ключевые факты (проверено)

- Текущий `botorama.pbo` содержит **исходники `.c`** (не байткод) + `config.bin` +
  `Animations/*.agr` + `voices/*.ogg`. Движок компилирует `.c` на старте →
  **Workbench для компиляции скриптов не нужен**.
- Сборка = `AddonBuilder.exe` (pack + `config.cpp`→`config.bin` + префикс) +
  `DSSignFile.exe` (подпись). Оба Windows-`.exe` под Steam «DayZ Tools» (Wine/Proton).
- Подпись обязательна (`verifySignatures=2`). Ключ: `devalio.biprivatekey` +
  `devalio.bikey` в `/home/devalio/dayz/Work/Keys`; серверный `keys/devalio.bikey` уже стоит.

## Решения (зафиксированы)

- Префиксы PBO: `dm_cons`, `dm_reg`, `dm_core`, `dm_map`, `dm_loadout`, `dm_test`.
- Данные (не-код) — вне `src/`: `map/Configs/*.json` → `data/map/`; корень держит
  `docs/`, `names.json`, `loadouts.md` и пр.
- `defines[]` (debug-домены) дублируются в CfgMods каждого PBO с гейтед-кодом
  (`core`, `map`, `loadout`, `test`).

## Граф зависимостей (выведен из перекрёстных ссылок)

`cons → reg → core → loadout → map → test`

| PBO | CfgPatches class | requiredAddons |
|---|---|---|
| cons | `dmBotorama_Cons` | (нет dm-зависимостей) |
| reg | `dmBotorama_Reg` | `dmBotorama_Cons` |
| core | `dmBotorama_Core` | `dmBotorama_Cons`, `dmBotorama_Reg`, `DZ_Characters`, `DZ_Anims_Anm_Player`, `DZ_Anims_Cfg`, `DZ_Sounds_Effects` |
| loadout | `dmBotorama_Loadout` | `dmBotorama_Cons`, `dmBotorama_Reg`, `dmBotorama_Core` |
| map | `dmBotorama_Map` | `dmBotorama_Cons`, `dmBotorama_Reg`, `dmBotorama_Core`, `dmBotorama_Loadout` |
| test | `dmBotorama_Test` | все пять выше |

Обоснование: `reg` использует `DM_*` (cons); `core` использует `dmJsonFile`/
`dmEntityRegistry` (reg); `loadout` использует `dmJsonFile`+`dmLoot` (reg+core);
`map` использует `dmJsonFile`+`dmAISurvivor`+`dmLoadoutApplier` (reg+core+loadout);
`test` — всё.

## Целевое дерево

```
botorama/
├── src/
│   ├── cons/    config.cpp + 3_Game 4_World 5_Mission
│   ├── reg/     config.cpp + 3_Game 4_World
│   ├── core/    config.cpp (+CfgVehicles/CfgSoundShaders/Sets) + 3_Game 4_World 5_Mission + Animations/ + voices/
│   ├── map/     config.cpp + 3_Game 4_World 5_Mission
│   ├── loadout/ config.cpp + 4_World
│   └── test/    config.cpp + 3_Game 4_World 5_Mission
├── data/map/                   # world_poi.json, buildings_interior.json, spawn.json
├── docs/  AGENTS.md  names.json  loadouts.md  …
```

## Разбиение config.cpp

Каждый `src/<модуль>/config.cpp`:
- `CfgPatches` (класс + `requiredAddons` из таблицы).
- `CfgMods` (свой класс) + `defs.files[]` **только своих слоёв**:
  - cons: `dm_cons/3_Game`, `dm_cons/4_World`, `dm_cons/5_Mission`;
  - reg: `dm_reg/3_Game`, `dm_reg/4_World`;
  - core: `dm_core/3_Game`, `dm_core/4_World`, `dm_core/5_Mission`;
  - loadout: `dm_loadout/4_World`;
  - map: `dm_map/3_Game`, `dm_map/4_World`, `dm_map/5_Mission`;
  - test: `dm_test/3_Game`, `dm_test/4_World`, `dm_test/5_Mission`.
- `defines[]` — продублировать в CfgMods core/map/loadout/test.

`core/config.cpp` дополнительно несёт `CfgVehicles` (31 класс `dmAI_Survivor*`),
`CfgSoundShaders`, `CfgSoundSets`.

## Правки путей-литералов (префикс `botorama` → `dm_core`)

- `graphName = "botorama\Animations\player_main.agr"` → `"dm_core\Animations\player_main.agr"` (31 шт., в `core/config.cpp`).
- `samples[] = "botorama\voices\..."` → `"dm_core\voices\..."` (97 шт., в `core/config.cpp`).
- `src/core/Animations/player_main.agr`: `botorama/Animations/{Locomotion,Actions,Tests}.agr` → `dm_core/...` (3 строки).

## Фаза 2 — сборка/подпись/деплой/проверка

### tools/build.sh (ИИ-агент)
Для каждого модуля:
1. `AddonBuilder.exe -prefix dm_<модуль>` + exclude (`.git`, `docs`, `*.md`, `*.json`, `data/`, `Presets/`) → `<модуль>.pbo`;
2. `DSSignFile.exe devalio.biprivatekey` → `<модуль>.pbo.devalio.bisign`;
3. копия 6×`.pbo` + 6×`.bisign` в `@Botorama/Addons/`.

**Риск**: headless-запуск `AddonBuilder`/`DSSignFile` под имеющимся Wine/Proton
(ключ создан под `Z:`-маппингом → Wine уже использовался). Первый шаг — зафиксировать
точную CLI-инвокацию.

### Проверка
- Старт `DayZServer` с `-mod=@Botorama;…`; ожидание `dmBotLog.LogVersion` в RPT;
  отсутствие ошибок линковки скриптов.
- `/bot`/`/test` работают как до рефакторинга (поведение не менялось — только раскладка).

## Критерий приёмки

6 подписанных PBO загружаются сервером (`verifySignatures=2`), `LogVersion` в логе,
поведение мода не изменилось.

## Порядок задач

1. `[x]` Ветка `feature/multi-pbo-refactor` (от master).
2. `[x]` План-документ (этот файл).
3. `[ ]` Перемещение модулей в `src/`, ассетов в `src/core/`, map JSON в `data/map/`.
4. `[ ]` Разбиение `config.cpp` на 6 модульных + правки путей.
5. `[ ]` bump `DM_BOTORAMA_VERSION` (`src/cons/3_Game/constants.c`).
6. `[ ]` `tools/build.sh` + сборка/подпись/деплой.
7. `[ ]` Верификация на сервере (лог + команды).
