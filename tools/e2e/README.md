# Регрессионные E2E-сценарии

Каталог сценариев, которые `dayz-tester` прогоняет перед фиксацией. Формат —
протокол файлового моста (`docs/plans/e2e-automation.md`): каждый `.json` файл
здесь — это сценарий, который кладётся в `$profile:dmBotorama/e2e/in/<name>.json`.

## Как добавить сценарий

1. Файл `<name>.json` с полями `Name`, `Timeout`, `Steps[]`.
2. Шаг: `Op` (`ping` | `spawn` | `moveto` | `follow` | `patrol` | `speed` | `loadout`
   | `stance` | `look` | `say` | `wait` | `assert` | `snapshot` | `clearall` | `killall`)
   + параметры (`Who`, `Target`, `Pos`, `Points`, `Cond`, `Value`, `Tolerance`, `Timeout`, …).
3. Координаты спавна — из `data/map/world_poi.json`, **не у воды** (иначе `SnapToGroundExactly`
   даёт `Pos[1]` в минус).

## Обязательный минимум (гейт перед merge)

- `hello-world.json` — проверка канала и спавна (ping → spawn → snapshot → clearall).

## Имена

- `Name` сценария должен совпадать с именем файла (без `.json`).

## bool в result-JSON

`bool` сериализуется как `1`/`0` (не `true`/`false`); статус шага — `Ok`, итог — `Status`.
